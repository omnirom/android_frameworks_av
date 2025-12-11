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

#define LOG_TAG "AudioProductStrategy"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <media/AudioProductStrategy.h>
#include <media/VolumeGroupAttributes.h>
#include <media/PolicyAidlConversion.h>

#include <android_media_audiopolicy.h>

namespace audio_flags = android::media::audiopolicy;

namespace android {

status_t AudioProductStrategy::readFromParcel(const Parcel* parcel) {
    media::AudioProductStrategy aidl;
    RETURN_STATUS_IF_ERROR(aidl.readFromParcel(parcel));
    *this = VALUE_OR_RETURN_STATUS(aidl2legacy_AudioProductStrategy(aidl));
    return OK;
}

status_t AudioProductStrategy::writeToParcel(Parcel* parcel) const {
    media::AudioProductStrategy aidl = VALUE_OR_RETURN_STATUS(
            legacy2aidl_AudioProductStrategy(*this));
    return aidl.writeToParcel(parcel);
}

ConversionResult<media::AudioProductStrategy>
legacy2aidl_AudioProductStrategy(const AudioProductStrategy& legacy) {
    media::AudioProductStrategy aidl;
    aidl.name = legacy.getName();
    aidl.audioAttributes = VALUE_OR_RETURN(
            convertContainer<std::vector<media::AudioAttributesEx>>(
                    legacy.getVolumeGroupAttributes(),
                    legacy2aidl_VolumeGroupAttributes_AudioAttributesEx));
    aidl.id = VALUE_OR_RETURN(legacy2aidl_product_strategy_t_int32_t(legacy.getId()));
    aidl.zoneId = legacy.getZoneId();
    return aidl;
}

ConversionResult<AudioProductStrategy>
aidl2legacy_AudioProductStrategy(const media::AudioProductStrategy& aidl) {
    return AudioProductStrategy(
            aidl.name,
            VALUE_OR_RETURN(
                    convertContainer<std::vector<VolumeGroupAttributes>>(
                            aidl.audioAttributes,
                            aidl2legacy_AudioAttributesEx_VolumeGroupAttributes)),
            VALUE_OR_RETURN(aidl2legacy_int32_t_product_strategy_t(aidl.id)),
            aidl.zoneId);
}

// Keep in sync with android/media/audiopolicy/AudioProductStrategy#attributeMatches
int AudioProductStrategy::attributesMatchesScore(audio_attributes_t refAttributes,
        audio_attributes_t clientAttributes, int refZoneId, int clientZoneId)
{
    if (clientZoneId != refZoneId && refZoneId != DEFAULT_ZONE_ID) {
        // Default zone shall match for all zoneId requested to ensure a fallback
        return NO_MATCH;
    }
    int score = MATCH_ON_DEFAULT_SCORE;
    if (refZoneId == clientZoneId) {
        score |= MATCH_ON_ZONE_ID_SCORE;
    }
    int scoreOnAttributes = attributesMatchesScore(refAttributes, clientAttributes);
    if (scoreOnAttributes == NO_MATCH) {
        return scoreOnAttributes;
    }
    return score | scoreOnAttributes;
}

// Keep in sync with android/media/audiopolicy/AudioProductStrategy#attributeMatches
int AudioProductStrategy::attributesMatchesScore(audio_attributes_t refAttributes,
                                                 audio_attributes_t clientAttributes)
{
    refAttributes.flags = static_cast<audio_flags_mask_t>(
            refAttributes.flags & AUDIO_FLAGS_AFFECT_STRATEGY_SELECTION);
    clientAttributes.flags = static_cast<audio_flags_mask_t>(
            clientAttributes.flags & AUDIO_FLAGS_AFFECT_STRATEGY_SELECTION);
    if (refAttributes == clientAttributes) {
        return MATCH_ATTRIBUTES_EQUALS;
    }
    if (refAttributes == AUDIO_ATTRIBUTES_INITIALIZER) {
        // The default product strategy is the strategy that holds default attributes by convention.
        // All attributes that fail to match will follow the default strategy for routing.
        // Choosing the default must be done as a fallback,so return a default (zero) score to
        // allow identify the fallback.
        return MATCH_ON_DEFAULT_SCORE;
    }
    int score = MATCH_ON_DEFAULT_SCORE;
    if (refAttributes.usage == AUDIO_USAGE_UNKNOWN) {
        score |= MATCH_ON_DEFAULT_SCORE;
    } else if (clientAttributes.usage == refAttributes.usage) {
        score |= MATCH_ON_USAGE_SCORE;
    } else {
        return NO_MATCH;
    }
    if (refAttributes.content_type == AUDIO_CONTENT_TYPE_UNKNOWN) {
        score |= MATCH_ON_DEFAULT_SCORE;
    } else if (clientAttributes.content_type == refAttributes.content_type) {
        score |= MATCH_ON_CONTENT_TYPE_SCORE;
    } else {
        return NO_MATCH;
    }
    if (strlen(refAttributes.tags) == 0) {
        score |= MATCH_ON_DEFAULT_SCORE;
    } else if (std::strcmp(clientAttributes.tags, refAttributes.tags) == 0) {
        score |= MATCH_ON_TAGS_SCORE;
    } else {
        return NO_MATCH;
    }
    if (refAttributes.flags == AUDIO_FLAG_NONE) {
        score |= MATCH_ON_DEFAULT_SCORE;
    } else if ((clientAttributes.flags != AUDIO_FLAG_NONE)
            && ((clientAttributes.flags & refAttributes.flags) == refAttributes.flags)) {
        score |= MATCH_ON_FLAGS_SCORE;
    } else {
        return NO_MATCH;
    }
    return score;
}

 bool AudioProductStrategy::isDefaultMatchingScore(int score)
 {
     if (audio_flags::multi_zone_audio()) {
         return score == MATCH_ON_DEFAULT_SCORE || score == MATCH_ON_ZONE_ID_SCORE;
     }
     return score == MATCH_ON_DEFAULT_SCORE;
 }
 bool AudioProductStrategy::isMatchingScore(int score)
 {
     if (audio_flags::multi_zone_audio()) {
         return score == MATCH_EQUALS;
     }
     return score == MATCH_ATTRIBUTES_EQUALS;
 }

} // namespace android
