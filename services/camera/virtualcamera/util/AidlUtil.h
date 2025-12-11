/*
 * Copyright 2023 The Android Open Source Project
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

#ifndef ANDROID_COMPANION_VIRTUALCAMERA_AIDLUTIL_H
#define ANDROID_COMPANION_VIRTUALCAMERA_AIDLUTIL_H

#include "aidl/android/companion/virtualcamera/VirtualCameraMetadata.h"
#include "aidl/android/hardware/camera/device/CameraMetadata.h"
#include "system/camera_metadata.h"

namespace android {
namespace companion {
namespace virtualcamera {

/**
 * Converts a HAL Device CameraMetadata to a VirtualCameraMetadata.
 */
status_t convertDeviceToVirtualCameraMetadata(
    const aidl::android::hardware::camera::device::CameraMetadata&
        srcCameraMetadata,
    aidl::android::companion::virtualcamera::VirtualCameraMetadata&
        destVirtualCameraMetadata);

/**
 * Converts a VirtualCameraMetadata to a HAL Device CameraMetadata.
 */
status_t convertVirtualToDeviceCameraMetadata(
    const aidl::android::companion::virtualcamera::VirtualCameraMetadata&
        srcVirtualCameraMetadata,
    aidl::android::hardware::camera::device::CameraMetadata& destCameraMetadata);

}  // namespace virtualcamera
}  // namespace companion
}  // namespace android

#endif  // ANDROID_COMPANION_VIRTUALCAMERA_AIDLUTIL_H
