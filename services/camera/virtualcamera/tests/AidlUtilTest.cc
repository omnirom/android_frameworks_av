/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "aidl/android/companion/virtualcamera/VirtualCameraMetadata.h"
#include "aidl/android/hardware/camera/device/CameraMetadata.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/AidlUtil.h"
#include "util/MetadataUtil.h"

namespace android {
namespace companion {
namespace virtualcamera {
namespace {

constexpr float kFocalLength = 2.1f;

using ::aidl::android::companion::virtualcamera::VirtualCameraMetadata;
using ::aidl::android::hardware::camera::device::CameraMetadata;
using ::testing::Eq;

class AidlUtilTest : public ::testing::Test {
 public:
  void SetUp() override {
  }
};

TEST_F(AidlUtilTest, DeviceToVirtualAndBackSuccessful) {
  CameraMetadata deviceMetadata;
  CameraMetadata recreatedDeviceMetadata;
  VirtualCameraMetadata virtualCameraMetadata;

  deviceMetadata = *(MetadataBuilder()
                         .setFlashAvailable(true)
                         .setFocalLength(kFocalLength)
                         .build());

  EXPECT_THAT(convertDeviceToVirtualCameraMetadata(deviceMetadata,
                                                   virtualCameraMetadata),
              OK);
  EXPECT_THAT(convertVirtualToDeviceCameraMetadata(virtualCameraMetadata,
                                                   recreatedDeviceMetadata),
              OK);

  EXPECT_THAT(deviceMetadata, Eq(recreatedDeviceMetadata));
}

}  // namespace
}  // namespace virtualcamera
}  // namespace companion
}  // namespace android
