/*
 * Copyright (C) 2025 The Android Open Source Project
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

#define LOG_TAG "VirtualCameraCaptureResultConsumer"

#include "VirtualCameraCaptureResultConsumer.h"

#include "util/AidlUtil.h"
#include "utils/Log.h"

namespace android {
namespace companion {
namespace virtualcamera {

using ::aidl::android::companion::virtualcamera::VirtualCameraMetadata;

VirtualCameraCaptureResultConsumer::~VirtualCameraCaptureResultConsumer() {
  std::lock_guard<std::mutex> lock(mLock);
  if (mLastMetadata != nullptr) {
    free_camera_metadata(mLastMetadata);
    mLastMetadata = nullptr;
  }
}

ndk::ScopedAStatus VirtualCameraCaptureResultConsumer::acceptCaptureResult(
    int64_t timestamp, const VirtualCameraMetadata& captureResultMetadata) {
  ::aidl::android::hardware::camera::device::CameraMetadata deviceCameraMetadata;
  status_t ret = convertVirtualToDeviceCameraMetadata(captureResultMetadata,
                                                      deviceCameraMetadata);
  if (ret != OK) {
    ALOGE("Failed to convert virtual to device CaptureResult!");
    // Return OK to the client, we just log the error and drop the metadata.
    return ndk::ScopedAStatus::ok();
  }

  const camera_metadata_t* rawMetadata =
      reinterpret_cast<const camera_metadata_t*>(
          deviceCameraMetadata.metadata.data());
  camera_metadata_t* newMetadata = clone_camera_metadata(rawMetadata);

  std::lock_guard<std::mutex> lock(mLock);
  if (mLastMetadata) {
    free_camera_metadata(mLastMetadata);
  }
  mLastTimestamp = timestamp;
  mLastMetadata = newMetadata;

  return ndk::ScopedAStatus::ok();
}

const camera_metadata_t*
VirtualCameraCaptureResultConsumer::getCaptureResultMetadataForTimestamp(
    int64_t timestamp) {
  std::lock_guard<std::mutex> lock(mLock);
  if (mLastTimestamp == timestamp && mLastMetadata != nullptr) {
    return clone_camera_metadata(mLastMetadata);
  }
  return nullptr;
}

}  // namespace virtualcamera
}  // namespace companion
}  // namespace android