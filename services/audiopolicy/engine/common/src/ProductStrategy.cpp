/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "APM::AudioPolicyEngine/ProductStrategy"
//#define LOG_NDEBUG 0

#include "ProductStrategy.h"

#include <android/media/audio/common/AudioHalProductStrategy.h>
#include <media/AudioProductStrategy.h>
#include <media/TypeConverter.h>
#include <utils/String8.h>
#include <cstdint>
#include <string>

#include <log/log.h>

#include <android_media_audiopolicy.h>

namespace audio_flags = android::media::audiopolicy;

namespace android {

using media::audio::common::AudioHalProductStrategy;

/*
 * Note on the id: either is provided (legacy strategies have hard coded id, aidl have
 * own id, enforced to be started from VENDOR_STRATEGY_ID_START.
 * REROUTING & PATCH system strategies are added.
 * To prevent from collision, generate from VENDOR_STRATEGY_ID_START when id is not provided.
 */
ProductStrategy::ProductStrategy(const std::string &name, int id, int zoneId) :
    mName(name),
    mId((static_cast<product_strategy_t>(id) != PRODUCT_STRATEGY_NONE) ?
            static_cast<product_strategy_t>(id) :
            static_cast<product_strategy_t>(AudioHalProductStrategy::VENDOR_STRATEGY_ID_START +
                    HandleGenerator<uint32_t>::getNextHandle())),
    mZoneId(zoneId) {}

void ProductStrategy::addAttributes(const VolumeGroupAttributes &volumeGroupAttributes)
{
    mAttributesVector.push_back(volumeGroupAttributes);
}

std::vector<android::VolumeGroupAttributes> ProductStrategy::listVolumeGroupAttributes() const
{
    std::vector<android::VolumeGroupAttributes> androidAa;
    for (const auto &attr : mAttributesVector) {
        androidAa.push_back({attr.getGroupId(), attr.getStreamType(), attr.getAttributes()});
    }
    return androidAa;
}

AttributesVector ProductStrategy::getAudioAttributes() const
{
    AttributesVector attrVector;
    for (const auto &attrGroup : mAttributesVector) {
        attrVector.push_back(attrGroup.getAttributes());
    }
    if (not attrVector.empty()) {
        return attrVector;
    }
    return { AUDIO_ATTRIBUTES_INITIALIZER };
}

std::pair<int, VolumeGroupAttributes> ProductStrategy::getScoredVolumeGroupAttributesForAttributes(
        const audio_attributes_t attributes, int zoneId) const
{
    int bestScore = AudioProductStrategy::NO_MATCH;
    VolumeGroupAttributes bestVolumeGroupAttributesOrDefault = {};
    for (const auto &aag : mAttributesVector) {
        int score = AudioProductStrategy::attributesMatchesScore(aag.getAttributes(), attributes,
                mZoneId, zoneId);
        if (AudioProductStrategy::isMatchingScore(score)) {
            return std::pair<int, VolumeGroupAttributes>(AudioProductStrategy::MATCH_EQUALS, aag);
        }
        if (score > bestScore) {
            bestVolumeGroupAttributesOrDefault = aag;
            bestScore = score;
        }
    }
    return std::pair<int, VolumeGroupAttributes>(bestScore, bestVolumeGroupAttributesOrDefault);
}

int ProductStrategy::matchesScore(const audio_attributes_t attr) const
{
    int strategyScore = AudioProductStrategy::NO_MATCH;
    for (const auto &attrGroup : mAttributesVector) {
        int score = AudioProductStrategy::attributesMatchesScore(attrGroup.getAttributes(), attr);
        if (AudioProductStrategy::isMatchingScore(score)) {
            return score;
        }
        strategyScore = std::max(score, strategyScore);
    }
    return strategyScore;
}

int ProductStrategy::matchesScore(const audio_attributes_t attr, int zoneId) const
{
    int strategyScore = AudioProductStrategy::NO_MATCH;
    for (const auto &attrGroup : mAttributesVector) {
        int score = AudioProductStrategy::attributesMatchesScore(attrGroup.getAttributes(), attr,
                mZoneId, zoneId);
        if (AudioProductStrategy::isMatchingScore(score)) {
            return score;
        }
        strategyScore = std::max(score, strategyScore);
    }
    return strategyScore;
}

audio_attributes_t ProductStrategy::getAttributesForStreamType(audio_stream_type_t streamType) const
{
    const auto iter = std::find_if(begin(mAttributesVector), end(mAttributesVector),
                                   [&streamType](const auto &supportedAttr) {
        return supportedAttr.getStreamType() == streamType; });
    return iter != end(mAttributesVector) ? iter->getAttributes() : AUDIO_ATTRIBUTES_INITIALIZER;
}

bool ProductStrategy::isDefault() const
{
    return std::find_if(begin(mAttributesVector), end(mAttributesVector), [](const auto &attr) {
        return attr.getAttributes() == defaultAttr; }) != end(mAttributesVector);
}

StreamTypeVector ProductStrategy::getSupportedStreams() const
{
    StreamTypeVector streams;
    for (const auto &supportedAttr : mAttributesVector) {
        if (std::find(begin(streams), end(streams), supportedAttr.getStreamType())
                == end(streams) && supportedAttr.getStreamType() != AUDIO_STREAM_DEFAULT) {
            streams.push_back(supportedAttr.getStreamType());
        }
    }
    return streams;
}

bool ProductStrategy::supportStreamType(const audio_stream_type_t &streamType) const
{
    return std::find_if(begin(mAttributesVector), end(mAttributesVector),
                        [&streamType](const auto &supportedAttr) {
        return supportedAttr.getStreamType() == streamType; }) != end(mAttributesVector);
}

volume_group_t ProductStrategy::getVolumeGroupForStreamType(audio_stream_type_t stream) const
{
    for (const auto &supportedAttr : mAttributesVector) {
        if (supportedAttr.getStreamType() == stream) {
            return supportedAttr.getGroupId();
        }
    }
    return VOLUME_GROUP_NONE;
}

volume_group_t ProductStrategy::getDefaultVolumeGroup() const
{
    const auto &iter = std::find_if(begin(mAttributesVector), end(mAttributesVector),
                                    [](const auto &attr) {
        return attr.getAttributes() == defaultAttr;
    });
    return iter != end(mAttributesVector) ? iter->getGroupId() : VOLUME_GROUP_NONE;
}

void ProductStrategy::dump(String8 *dst, int spaces) const
{
    dst->appendFormat("\n%*s-%s (id: %d)", spaces, "", mName.c_str(), mId);
    if (audio_flags::multi_zone_audio()) {
        dst->appendFormat("(ZoneId: %d)", mZoneId);
    }
    std::string deviceLiteral = deviceTypesToString(mApplicableDevices);
    dst->appendFormat("\n%*sSelected Device: {%s, @:%s}\n", spaces + 2, "",
                       deviceLiteral.c_str(), mDeviceAddress.c_str());

    for (const auto &attr : mAttributesVector) {
        dst->appendFormat("%*sGroup: %d stream: %s\n", spaces + 3, "", attr.getGroupId(),
                          android::toString(attr.getStreamType()).c_str());
        dst->appendFormat("%*s Attributes: ", spaces + 3, "");
        std::string attStr = attr.getAttributes() == defaultAttr ?
                "{ Any }" : android::toString(attr.getAttributes());
        dst->appendFormat("%s\n", attStr.c_str());
    }
}

product_strategy_t ProductStrategyMap::getProductStrategyForAttributes(
        const audio_attributes_t &attributes, bool fallbackOnDefault) const
{
    product_strategy_t bestStrategyOrDefault = PRODUCT_STRATEGY_NONE;
    int matchScore = AudioProductStrategy::NO_MATCH;
    for (const auto &iter : *this) {
        int score = iter.second->matchesScore(attributes);
        if (AudioProductStrategy::isMatchingScore(score)) {
            return iter.second->getId();
        }
        if (score > matchScore) {
            bestStrategyOrDefault = iter.second->getId();
            matchScore = score;
        }
    }
    return (!AudioProductStrategy::isDefaultMatchingScore(matchScore) || fallbackOnDefault) ?
           bestStrategyOrDefault : PRODUCT_STRATEGY_NONE;
}

product_strategy_t ProductStrategyMap::getProductStrategyForAttributes(
        const audio_attributes_t &attributes, int zoneId, bool fallbackOnDefault) const
{
    product_strategy_t bestStrategyOrDefault = PRODUCT_STRATEGY_NONE;
    int matchScore = AudioProductStrategy::NO_MATCH;
    for (const auto &iter : *this) {
        int score = iter.second->matchesScore(attributes, zoneId);
        if (AudioProductStrategy::isMatchingScore(score)) {
            return iter.second->getId();
        }
        if (score > matchScore) {
            bestStrategyOrDefault = iter.second->getId();
            matchScore = score;
        }
    }
    return (!AudioProductStrategy::isDefaultMatchingScore(matchScore) || fallbackOnDefault) ?
           bestStrategyOrDefault : PRODUCT_STRATEGY_NONE;
}

audio_attributes_t ProductStrategyMap::getAttributesForStreamType(audio_stream_type_t stream) const
{
    for (const auto &iter : *this) {
        const auto strategy = iter.second;
        if (strategy->supportStreamType(stream)) {
            return strategy->getAttributesForStreamType(stream);
        }
    }
    ALOGV("%s: No product strategy for stream %s, using default", __FUNCTION__,
          toString(stream).c_str());
    return {};
}

audio_attributes_t ProductStrategyMap::getAttributesForStreamType(audio_stream_type_t stream,
        int zoneId) const
{
    audio_attributes_t defaultAttributesForStream = {};
    for (const auto &iter : *this) {
        const auto strategy = iter.second;
        if (strategy->supportStreamType(stream)) {
            if (zoneId == strategy->getZoneId()) {
                return strategy->getAttributesForStreamType(stream);
            }
            if (strategy->getZoneId() == AudioProductStrategy::DEFAULT_ZONE_ID) {
                defaultAttributesForStream = strategy->getAttributesForStreamType(stream);
            }
        }
    }
    ALOGV("%s: No product strategy for stream %s, using default", __FUNCTION__,
            toString(stream).c_str());
    return defaultAttributesForStream;
}

product_strategy_t ProductStrategyMap::getDefault() const
{
    if (mDefaultStrategy != PRODUCT_STRATEGY_NONE) {
        return mDefaultStrategy;
    }
    for (const auto &iter : *this) {
        if (iter.second->isDefault()) {
            ALOGV("%s: using default %s", __FUNCTION__, iter.second->getName().c_str());
            return iter.second->getId();
        }
    }
    ALOGE("%s: No default product strategy defined", __FUNCTION__);
    return PRODUCT_STRATEGY_NONE;
}

product_strategy_t ProductStrategyMap::getDefault(int zoneId) const
{
    if (mDefaultStrategy != PRODUCT_STRATEGY_NONE
            && zoneId == AudioProductStrategy::DEFAULT_ZONE_ID) {
        return mDefaultStrategy;
    }
    for (const auto &iter : *this) {
        if (zoneId == iter.second->getZoneId() && iter.second->isDefault()) {
            ALOGV("%s: using default %s", __FUNCTION__, iter.second->getName().c_str());
            return iter.second->getId();
        }
    }
    ALOGE_IF(mDefaultStrategy == PRODUCT_STRATEGY_NONE,
             "%s: No default product strategy defined", __FUNCTION__);
    return mDefaultStrategy;
}

audio_attributes_t ProductStrategyMap::getAttributesForProductStrategy(
        product_strategy_t strategy) const
{
    if (find(strategy) == end()) {
        ALOGE("Invalid %d strategy requested", strategy);
        return AUDIO_ATTRIBUTES_INITIALIZER;
    }
    return at(strategy)->getAudioAttributes()[0];
}

product_strategy_t ProductStrategyMap::getProductStrategyForStream(audio_stream_type_t stream) const
{
    for (const auto &iter : *this) {
        if (iter.second->supportStreamType(stream)) {
            return iter.second->getId();
        }
    }
    ALOGV("%s: No product strategy for stream %d, using default", __FUNCTION__, stream);
    return getDefault();
}

product_strategy_t ProductStrategyMap::getProductStrategyForStream(audio_stream_type_t stream,
        int zoneId) const
{
    product_strategy_t defaultStrategyForStream = mDefaultStrategy;
    for (const auto &iter : *this) {
        const auto &strategy = iter.second;
        if (strategy->supportStreamType(stream)) {
            if (strategy->getZoneId() == zoneId) {
                return strategy->getId();
            } else if (strategy->getZoneId() == AudioProductStrategy::DEFAULT_ZONE_ID) {
                defaultStrategyForStream = strategy->getId();
            }
        }
    }
    ALOGV("%s: No product strategy for stream %d, using default", __FUNCTION__, stream);
    return defaultStrategyForStream;
}


DeviceTypeSet ProductStrategyMap::getDeviceTypesForProductStrategy(
        product_strategy_t strategy) const
{
    if (find(strategy) == end()) {
        ALOGE("Invalid %d strategy requested, returning device for default strategy", strategy);
        product_strategy_t defaultStrategy = getDefault();
        if (defaultStrategy == PRODUCT_STRATEGY_NONE) {
            return {AUDIO_DEVICE_NONE};
        }
        return at(getDefault())->getDeviceTypes();
    }
    return at(strategy)->getDeviceTypes();
}

std::string ProductStrategyMap::getDeviceAddressForProductStrategy(product_strategy_t psId) const
{
    if (find(psId) == end()) {
        ALOGE("Invalid %d strategy requested, returning device for default strategy", psId);
        product_strategy_t defaultStrategy = getDefault();
        if (defaultStrategy == PRODUCT_STRATEGY_NONE) {
            return {};
        }
        return at(getDefault())->getDeviceAddress();
    }
    return at(psId)->getDeviceAddress();
}

VolumeGroupAttributes ProductStrategyMap::getVolumeGroupAttributesForAttributes(
        const audio_attributes_t &attr, bool fallbackOnDefault) const
{
    int matchScore = AudioProductStrategy::NO_MATCH;
    VolumeGroupAttributes bestVolumeGroupAttributes = {};
    for (const auto &iter : *this) {
        for (const auto &volGroupAttr : iter.second->getVolumeGroupAttributes()) {
            int score = volGroupAttr.matchesScore(attr);
            if (AudioProductStrategy::isMatchingScore(score)) {
                return volGroupAttr;
            }
            if (score > matchScore) {
                matchScore = score;
                bestVolumeGroupAttributes = volGroupAttr;
            }
        }
    }
    return (AudioProductStrategy::isDefaultMatchingScore(matchScore)  || fallbackOnDefault) ?
           bestVolumeGroupAttributes : VolumeGroupAttributes();
}

VolumeGroupAttributes ProductStrategyMap::getVolumeGroupAttributesForAttributes(
        const audio_attributes_t &attr, int zoneId, bool fallbackOnDefault) const
{
    int matchScore = AudioProductStrategy::NO_MATCH;
    VolumeGroupAttributes bestVolumeGroupAttributes = {};
    for (const auto &iter : *this) {
        std::pair<int, VolumeGroupAttributes> scoredAag =
                iter.second->getScoredVolumeGroupAttributesForAttributes(attr, zoneId);
        int score = scoredAag.first;
        if (AudioProductStrategy::isMatchingScore(score)) {
            return scoredAag.second;
        }
        if (score > matchScore) {
            matchScore = score;
            bestVolumeGroupAttributes = scoredAag.second;
        }
    }
    return (!AudioProductStrategy::isDefaultMatchingScore(matchScore) || fallbackOnDefault) ?
            bestVolumeGroupAttributes : VolumeGroupAttributes();
}

audio_stream_type_t ProductStrategyMap::getStreamTypeForAttributes(
        const audio_attributes_t &attr) const
{
    audio_stream_type_t streamType = getVolumeGroupAttributesForAttributes(
            attr, /* fallbackOnDefault= */ true).getStreamType();
    return streamType != AUDIO_STREAM_DEFAULT ? streamType : AUDIO_STREAM_MUSIC;
}

volume_group_t ProductStrategyMap::getVolumeGroupForAttributes(
        const audio_attributes_t &attr, bool fallbackOnDefault) const
{
    return getVolumeGroupAttributesForAttributes(attr, fallbackOnDefault).getGroupId();
}

volume_group_t ProductStrategyMap::getVolumeGroupForStreamType(
        audio_stream_type_t stream, bool fallbackOnDefault) const
{
    for (const auto &iter : *this) {
        volume_group_t group = iter.second->getVolumeGroupForStreamType(stream);
        if (group != VOLUME_GROUP_NONE) {
            return group;
        }
    }
    ALOGW("%s: no volume group for %s, using default", __func__, toString(stream).c_str());
    return fallbackOnDefault ? mDefaultVolumeGroup : VOLUME_GROUP_NONE;
}

volume_group_t ProductStrategyMap::getDefaultVolumeGroup() const
{
    product_strategy_t defaultStrategy = getDefault();
    if (defaultStrategy == PRODUCT_STRATEGY_NONE) {
        return VOLUME_GROUP_NONE;
    }
    return at(defaultStrategy)->getDefaultVolumeGroup();
}

audio_stream_type_t ProductStrategyMap::getStreamTypeForAttributes(
        const audio_attributes_t &attr, int zoneId) const
{
    audio_stream_type_t streamType = getVolumeGroupAttributesForAttributes(
            attr, zoneId, /* fallbackOnDefault= */ true).getStreamType();
    return streamType != AUDIO_STREAM_DEFAULT ? streamType : AUDIO_STREAM_MUSIC;
}

volume_group_t ProductStrategyMap::getVolumeGroupForAttributes(
        const audio_attributes_t &attr, int zoneId, bool fallbackOnDefault) const
{
    return getVolumeGroupAttributesForAttributes(attr, zoneId, fallbackOnDefault).getGroupId();
}

volume_group_t ProductStrategyMap::getVolumeGroupForStreamType(
        audio_stream_type_t stream, int zoneId, bool fallbackOnDefault) const
{
    volume_group_t defaultVolumeGroupForStream = mDefaultVolumeGroup;
    for (const auto &iter : *this) {
        const auto &strategy = iter.second;
        volume_group_t group = strategy->getVolumeGroupForStreamType(stream);
        if (group != VOLUME_GROUP_NONE) {
            if (strategy->getZoneId() == zoneId) {
                return group;
            } else if (strategy->getZoneId() == AudioProductStrategy::DEFAULT_ZONE_ID) {
                defaultVolumeGroupForStream = group;
            }
        }
    }
    ALOGW("%s: no volume group for %s, using default", __func__, toString(stream).c_str());
    return fallbackOnDefault ? defaultVolumeGroupForStream : VOLUME_GROUP_NONE;
}

volume_group_t ProductStrategyMap::getDefaultVolumeGroup(int zoneId) const
{
    product_strategy_t defaultStrategy = getDefault(zoneId);
    if (defaultStrategy == PRODUCT_STRATEGY_NONE) {
        return VOLUME_GROUP_NONE;
    }
    return at(defaultStrategy)->getDefaultVolumeGroup();
}

void ProductStrategyMap::initialize()
{
    mDefaultStrategy = PRODUCT_STRATEGY_NONE;
    mDefaultStrategy = getDefault();
    if (audio_flags::multi_zone_audio()) {
        mDefaultVolumeGroup = getDefaultVolumeGroup(AudioProductStrategy::DEFAULT_ZONE_ID);
    } else {
        mDefaultVolumeGroup = getDefaultVolumeGroup();
    }
    ALOG_ASSERT(mDefaultStrategy != PRODUCT_STRATEGY_NONE, "No default product strategy found");
    ALOG_ASSERT(mDefaultVolumeGroup != VOLUME_GROUP_NONE, "No default volume group found");
}

void ProductStrategyMap::dump(String8 *dst, int spaces) const
{
    dst->appendFormat("%*sProduct Strategies dump:", spaces, "");
    for (const auto &iter : *this) {
        iter.second->dump(dst, spaces + 2);
    }
}

void dumpProductStrategyDevicesRoleMap(
        const ProductStrategyDevicesRoleMap& productStrategyDeviceRoleMap,
        String8 *dst,
        int spaces) {
    dst->appendFormat("\n%*sDevice role per product strategy dump:", spaces, "");
    for (const auto& [strategyRolePair, devices] : productStrategyDeviceRoleMap) {
        dst->appendFormat("\n%*sStrategy(%u) Device Role(%u) Devices(%s)", spaces + 2, "",
                strategyRolePair.first, strategyRolePair.second,
                dumpAudioDeviceTypeAddrVector(devices, true /*includeSensitiveInfo*/).c_str());
    }
    dst->appendFormat("\n");
}
}
