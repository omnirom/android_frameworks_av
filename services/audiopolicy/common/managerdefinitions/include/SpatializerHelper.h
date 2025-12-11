/*
 * Copyright (C) 2024 The Android Open Source Project
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

#pragma once

#include "media/AudioContainers.h"
#include <cutils/properties.h>
#include <system/audio_effects/effect_spatializer.h>

namespace android {

class SpatializerHelper {

private:

    static const std::map<audio_devices_t, spatialization_mode_t> sDeviceModeMap;

    static bool isModeCompatibleWithDevices(spatialization_mode_t mode,
              DeviceTypeSet devices);
public:
    /**
     * @brief Check if the stereo spatialization feature turned on if:
     *        - com_android_media_audio_stereo_spatialization flag is on
     *          AND
     *          - sysprop "ro.audio.stereo_spatialization_enabled" is true
     *            OR
     *          - com_android_media_audio_stereo_spatialization_binaural_transaural flag is on
     *              AND
                    - sysprop "ro.audio.stereo_spatialization_binaural_enabled" is true
     *                   AND
     *                   - devices is empty or all devices are compatible with binaural
     *                OR
     *              - sysprop "ro.audio.stereo_spatialization_transaural_enabled" is true
     *                   AND
     *                   - devices is empty or all devices are compatible with transaural
     * @param  devices set of device types to check for compatibility with enabled
     *                 spatialization modes
     * @return true if the stereo spatialization feature is enabled
     * @return false if the stereo spatialization feature is not enabled
     */
    static bool isStereoSpatializationFeatureEnabled(DeviceTypeSet devices);
};

} // namespace android
