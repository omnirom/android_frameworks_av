/*
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "AudioHwDevice"
//#define LOG_NDEBUG 0

#include <system/audio.h>
#include <utils/Log.h>

#include <audio_utils/spdif/SPDIFEncoder.h>

#include "AudioHwDevice.h"
#include "AudioStreamOut.h"
#include "SpdifStreamOut.h"

namespace android {

using media::audio::common::AudioMMapPolicyInfo;
using media::audio::common::AudioMMapPolicyType;

// ----------------------------------------------------------------------------

status_t AudioHwDevice::openOutputStream(
        AudioStreamOut **ppStreamOut,
        audio_io_handle_t handle,
        audio_devices_t deviceType,
        audio_output_flags_t flags,
        struct audio_config *config,
        const char *address)
{

    struct audio_config originalConfig = *config;
    auto outputStream = new AudioStreamOut(this, flags);

    // Try to open the HAL first using the current format.
    ALOGV("openOutputStream(), try "
            " sampleRate %d, Format %#x, "
            "channelMask %#x",
            config->sample_rate,
            config->format,
            config->channel_mask);
    status_t status = outputStream->open(handle, deviceType, config, address);

    if (status != NO_ERROR) {
        delete outputStream;
        outputStream = nullptr;

        // FIXME Look at any modification to the config.
        // The HAL might modify the config to suggest a wrapped format.
        // Log this so we can see what the HALs are doing.
        ALOGI("openOutputStream(), HAL returned"
            " sampleRate %d, Format %#x, "
            "channelMask %#x, status %d",
            config->sample_rate,
            config->format,
            config->channel_mask,
            status);

        // If the data is encoded then try again using wrapped PCM.
        const bool wrapperNeeded = !audio_has_proportional_frames(originalConfig.format)
                && ((flags & AUDIO_OUTPUT_FLAG_DIRECT) != 0)
                && ((flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0);

        if (wrapperNeeded) {
            if (SPDIFEncoder::isFormatSupported(originalConfig.format)) {
                outputStream = new SpdifStreamOut(this, flags, originalConfig.format);
                status = outputStream->open(handle, deviceType, &originalConfig, address);
                if (status != NO_ERROR) {
                    ALOGE("ERROR - openOutputStream(), SPDIF open returned %d",
                        status);
                    delete outputStream;
                    outputStream = nullptr;
                }
            } else {
                ALOGE("ERROR - openOutputStream(), SPDIFEncoder does not support format 0x%08x",
                    originalConfig.format);
            }
        }
    }

    *ppStreamOut = outputStream;
    return status;
}

bool AudioHwDevice::supportsAudioPatches() const {
    bool result;
    return mHwDevice->supportsAudioPatches(&result) == OK ? result : false;
}

status_t AudioHwDevice::getAudioPort(struct audio_port_v7 *port) const {
    return mHwDevice->getAudioPort(port);
}

status_t AudioHwDevice::getMmapPolicyInfos(
            AudioMMapPolicyType policyType, std::vector<AudioMMapPolicyInfo> *policyInfos) const {
    return mHwDevice->getMmapPolicyInfos(policyType, policyInfos);
}

int32_t AudioHwDevice::getAAudioMixerBurstCount() const {
    return mHwDevice->getAAudioMixerBurstCount();
}

int32_t AudioHwDevice::getAAudioHardwareBurstMinUsec() const {
    return mHwDevice->getAAudioHardwareBurstMinUsec();
}

status_t AudioHwDevice::getAudioMixPort(const struct audio_port_v7 *devicePort,
                                        struct audio_port_v7 *mixPort) const {
    return mHwDevice->getAudioMixPort(devicePort, mixPort);
}


}; // namespace android
