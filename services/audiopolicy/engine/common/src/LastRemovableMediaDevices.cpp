/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "APM::AudioPolicyEngine/LastRemovableMediaDevices"
//#define LOG_NDEBUG 0

#include "LastRemovableMediaDevices.h"
#include <log/log.h>

namespace android {

void LastRemovableMediaDevices::setRemovableMediaDevices(sp<DeviceDescriptor> desc,
                                                         audio_policy_dev_state_t state)
{
    if (desc == nullptr) {
        return;
    } else {
        if ((state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) &&
                (getDeviceOutGroup(desc->type()) != GROUP_NONE)) {
            setRemovableMediaDevices(desc, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE);
            mMediaDevices.insert(mMediaDevices.begin(), {desc, getDeviceOutGroup(desc->type())});
        } else if (state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE) {
            for (auto iter = mMediaDevices.begin(); iter != mMediaDevices.end(); ++iter) {
                if ((iter->desc)->equals(desc)) {
                    mMediaDevices.erase(iter);
                    break;
                }
            }
        }
    }
}

std::vector<audio_devices_t> LastRemovableMediaDevices::getLastRemovableMediaDevices(
        device_out_group_t group, std::vector<audio_devices_t> excludedDevices) const
{
    std::vector<audio_devices_t> ret;
    for (auto iter = mMediaDevices.begin(); iter != mMediaDevices.end(); ++iter) {
        audio_devices_t type = (iter->desc)->type();
        if ((group == GROUP_NONE || group == getDeviceOutGroup(type))
                && std::find(excludedDevices.begin(), excludedDevices.end(), type) ==
                                       excludedDevices.end()) {
            ret.push_back(type);
        }
    }
    return ret;
}

sp<DeviceDescriptor> LastRemovableMediaDevices::getLastRemovableMediaDevice(
        const DeviceVector& excludedDevices, device_out_group_t group) const {
    for (auto iter = mMediaDevices.begin(); iter != mMediaDevices.end(); ++iter) {
        if ((group == GROUP_NONE || group == getDeviceOutGroup((iter->desc)->type())) &&
                !excludedDevices.contains(iter->desc)) {
            return iter->desc;
        }
    }
    return nullptr;
}

device_out_group_t LastRemovableMediaDevices::getDeviceOutGroup(audio_devices_t device) const
{
    switch (device) {
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
    case AUDIO_DEVICE_OUT_LINE:
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
    case AUDIO_DEVICE_OUT_USB_HEADSET:
    case AUDIO_DEVICE_OUT_USB_ACCESSORY:
    case AUDIO_DEVICE_OUT_USB_DEVICE:
    case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
    case AUDIO_DEVICE_OUT_AUX_DIGITAL:
        return GROUP_WIRED;
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
    // TODO (b/122931261): remove when preferred device for strategy media will be used instead of
    //  AUDIO_POLICY_FORCE_NO_BT_A2DP.
    case AUDIO_DEVICE_OUT_HEARING_AID:
    case AUDIO_DEVICE_OUT_BLE_HEADSET:
    case AUDIO_DEVICE_OUT_BLE_SPEAKER:
    case AUDIO_DEVICE_OUT_BLE_BROADCAST:
        return GROUP_BT_A2DP;
    default:
        return GROUP_NONE;
    }
}

} // namespace android
