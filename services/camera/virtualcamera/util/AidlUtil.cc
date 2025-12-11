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

#include "AidlUtil.h"

#include <binder/Parcel.h>

#include "aidl/android/hardware/camera/device/CameraMetadata.h"
#include "camera/CameraMetadata.h"
#include "system/camera_metadata.h"
#include "utils/Errors.h"

namespace android {
namespace companion {
namespace virtualcamera {

using ::aidl::android::companion::virtualcamera::VirtualCameraMetadata;
using DeviceCameraMetadata =
    ::aidl::android::hardware::camera::device::CameraMetadata;
using AndroidCameraMetadata = ::android::CameraMetadata;

/**
 * The bytes from a Device CameraMetadata (used by the HAL) are just a
 * representation of the camera_metadata_t. The CameraMetadataNative used in the
 * Java client layer is a representation of the Android framework
 * CameraMetadata, which when parceled includes also the size and alignment of
 * the structure. So when using the byte[] of the VirtualCameraMetadata, it
 * represents the bytes of the CameMetadataNative parcel (Android framework
 * CameraMetadata). Hence the intermediary Android framework CameraMetadata used
 * to convert the bytes of a Device CameraMetadata object to the bytes of a
 * CameraMetadataNative object.
 */

status_t convertDeviceToVirtualCameraMetadata(
    const DeviceCameraMetadata& srcCameraMetadata,
    VirtualCameraMetadata& destVirtualCameraMetadata) {
  auto metadata = reinterpret_cast<const camera_metadata_t*>(
      srcCameraMetadata.metadata.data());
  AndroidCameraMetadata androidCameraMetadata;
  androidCameraMetadata = metadata;

  android::Parcel parcel;
  status_t ret = androidCameraMetadata.writeToParcel(&parcel);

  if (ret == OK) {
    auto parcelBytes = parcel.data();
    destVirtualCameraMetadata.metadata.assign(parcelBytes,
                                              parcelBytes + parcel.dataSize());
  }
  return ret;
}

status_t convertVirtualToDeviceCameraMetadata(
    const VirtualCameraMetadata& srcVirtualCameraMetadata,
    DeviceCameraMetadata& destCameraMetadata) {
  android::Parcel parcel;
  status_t ret = parcel.write(srcVirtualCameraMetadata.metadata.data(),
                              srcVirtualCameraMetadata.metadata.size());
  if (ret != OK) {
    return ret;
  }

  parcel.setDataPosition(0);
  AndroidCameraMetadata androidCameraMetadata;
  ret = androidCameraMetadata.readFromParcel(&parcel);
  if (ret != OK) {
    return ret;
  }

  const camera_metadata_t* metadata = androidCameraMetadata.getAndLock();
  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(metadata);
  destCameraMetadata.metadata.assign(
      data_ptr, data_ptr + get_camera_metadata_size(metadata));
  androidCameraMetadata.unlock(metadata);

  return ret;
}

}  // namespace virtualcamera
}  // namespace companion
}  // namespace android
