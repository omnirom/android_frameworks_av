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

#include <media/AudioProductStrategy.h>
#include <media/TypeConverter.h>
#include <utils/String8.h>
#include <cstdint>
#include <string>

#include <log/log.h>


namespace android {

ProductStrategy::ProductStrategy(const std::string &name) :
    mName(name),
    mId(static_cast<product_strategy_t>(HandleGenerator<uint32_t>::getNextHandle()))
{
}

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

int ProductStrategy::matchesScore(const audio_attributes_t attr) const
{
    int strategyScore = AudioProductStrategy::NO_MATCH;
    for (const auto &attrGroup : mAttributesVector) {
        int score = AudioProductStrategy::attributesMatchesScore(attrGroup.getAttributes(), attr);
        if (score == AudioProductStrategy::MATCH_EQUALS) {
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
    dst->appendFormat("\n%*s-%s (id: %d)\n", spaces, "", mName.c_str(), mId);
    std::string deviceLiteral = deviceTypesToString(mApplicableDevices);
    dst->appendFormat("%*sSelected Device: {%s, @:%s}\n", spaces + 2, "",
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
    product_strategy_t bestStrategyOrdefault = PRODUCT_STRATEGY_NONE;
    int matchScore = AudioProductStrategy::NO_MATCH;
    for (const auto &iter : *this) {
        int score = iter.second->matchesScore(attributes);
        if (score == AudioProductStrategy::MATCH_EQUALS) {
            return iter.second->getId();
        }
        if (score > matchScore) {
            bestStrategyOrdefault = iter.second->getId();
            matchScore = score;
        }
    }
    return (matchScore != AudioProductStrategy::MATCH_ON_DEFAULT_SCORE || fallbackOnDefault) ?
            bestStrategyOrdefault : PRODUCT_STRATEGY_NONE;
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
            if (score == AudioProductStrategy::MATCH_EQUALS) {
                return volGroupAttr;
            }
            if (score > matchScore) {
                matchScore = score;
                bestVolumeGroupAttributes = volGroupAttr;
            }
        }
    }
    return (matchScore != AudioProductStrategy::MATCH_ON_DEFAULT_SCORE || fallbackOnDefault) ?
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
    return fallbackOnDefault ? getDefaultVolumeGroup() : VOLUME_GROUP_NONE;
}

volume_group_t ProductStrategyMap::getDefaultVolumeGroup() const
{
    product_strategy_t defaultStrategy = getDefault();
    if (defaultStrategy == PRODUCT_STRATEGY_NONE) {
        return VOLUME_GROUP_NONE;
    }
    return at(defaultStrategy)->getDefaultVolumeGroup();
}

void ProductStrategyMap::initialize()
{
    mDefaultStrategy = getDefault();
    ALOG_ASSERT(mDefaultStrategy != PRODUCT_STRATEGY_NONE, "No default product strategy found");
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
