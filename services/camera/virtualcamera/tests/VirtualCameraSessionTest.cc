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

#include <cstdint>
#include <memory>

#include "VirtualCameraDevice.h"
#include "VirtualCameraSession.h"
#include "aidl/android/companion/virtualcamera/BnVirtualCameraCallback.h"
#include "aidl/android/companion/virtualcamera/SupportedStreamConfiguration.h"
#include "aidl/android/hardware/camera/common/Status.h"
#include "aidl/android/hardware/camera/device/BnCameraDeviceCallback.h"
#include "aidl/android/hardware/camera/device/StreamConfiguration.h"
#include "aidl/android/hardware/graphics/common/PixelFormat.h"
#include "android/binder_auto_utils.h"
#include "android/binder_interface_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/MetadataBuilder.h"

namespace android {
namespace companion {
namespace virtualcamera {
namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr int kStreamId = 0;
constexpr int kCameraId = 42;

using ::aidl::android::companion::virtualcamera::BnVirtualCameraCallback;
using ::aidl::android::companion::virtualcamera::Format;
using ::aidl::android::companion::virtualcamera::SupportedStreamConfiguration;
using ::aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::device::BnCameraDeviceCallback;
using ::aidl::android::hardware::camera::device::BufferRequest;
using ::aidl::android::hardware::camera::device::BufferRequestStatus;
using ::aidl::android::hardware::camera::device::CaptureRequest;
using ::aidl::android::hardware::camera::device::CaptureResult;
using ::aidl::android::hardware::camera::device::HalStream;
using ::aidl::android::hardware::camera::device::NotifyMsg;
using ::aidl::android::hardware::camera::device::Stream;
using ::aidl::android::hardware::camera::device::StreamBuffer;
using ::aidl::android::hardware::camera::device::StreamBufferRet;
using ::aidl::android::hardware::camera::device::StreamConfiguration;
using ::aidl::android::hardware::graphics::common::PixelFormat;
using ::aidl::android::view::Surface;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SizeIs;

Stream createStream(int streamId, int width, int height, PixelFormat format) {
  Stream s;
  s.id = streamId;
  s.width = width;
  s.height = height;
  s.format = format;
  return s;
}

class MockCameraDeviceCallback : public BnCameraDeviceCallback {
 public:
  MOCK_METHOD(ndk::ScopedAStatus, notify, (const std::vector<NotifyMsg>&),
              (override));
  MOCK_METHOD(ndk::ScopedAStatus, processCaptureResult,
              (const std::vector<CaptureResult>&), (override));
  MOCK_METHOD(ndk::ScopedAStatus, requestStreamBuffers,
              (const std::vector<BufferRequest>&, std::vector<StreamBufferRet>*,
               BufferRequestStatus*),
              (override));
  MOCK_METHOD(ndk::ScopedAStatus, returnStreamBuffers,
              (const std::vector<StreamBuffer>&), (override));
};

class MockVirtualCameraCallback : public BnVirtualCameraCallback {
 public:
  MOCK_METHOD(ndk::ScopedAStatus, onStreamConfigured,
              (int, const Surface&, int32_t, int32_t, Format), (override));
  MOCK_METHOD(ndk::ScopedAStatus, onProcessCaptureRequest, (int, int),
              (override));
  MOCK_METHOD(ndk::ScopedAStatus, onStreamClosed, (int), (override));
};

class VirtualCameraSessionTest : public ::testing::Test {
 public:
  void SetUp() override {
    mMockCameraDeviceCallback =
        ndk::SharedRefBase::make<MockCameraDeviceCallback>();
    mMockVirtualCameraClientCallback =
        ndk::SharedRefBase::make<MockVirtualCameraCallback>();
    mVirtualCameraDevice = ndk::SharedRefBase::make<VirtualCameraDevice>(
        kCameraId,
        std::vector<SupportedStreamConfiguration>{
            SupportedStreamConfiguration{.width = kWidth,
                                         .height = kHeight,
                                         .pixelFormat = Format::YUV_420_888}},
        mMockVirtualCameraClientCallback);
    mVirtualCameraSession = ndk::SharedRefBase::make<VirtualCameraSession>(
        mVirtualCameraDevice, mMockCameraDeviceCallback,
        mMockVirtualCameraClientCallback);

    // Explicitly defining default actions below to prevent gmock from
    // default-constructing ndk::ScopedAStatus, because default-constructed
    // status wraps nullptr AStatus and causes crash when attempting to print
    // it in gtest report.
    ON_CALL(*mMockCameraDeviceCallback, notify)
        .WillByDefault(ndk::ScopedAStatus::ok);
    ON_CALL(*mMockCameraDeviceCallback, processCaptureResult)
        .WillByDefault(ndk::ScopedAStatus::ok);
    ON_CALL(*mMockCameraDeviceCallback, requestStreamBuffers)
        .WillByDefault(ndk::ScopedAStatus::ok);
    ON_CALL(*mMockCameraDeviceCallback, returnStreamBuffers)
        .WillByDefault(ndk::ScopedAStatus::ok);

    ON_CALL(*mMockVirtualCameraClientCallback, onStreamConfigured)
        .WillByDefault(ndk::ScopedAStatus::ok);
    ON_CALL(*mMockVirtualCameraClientCallback, onProcessCaptureRequest)
        .WillByDefault(ndk::ScopedAStatus::ok);
    ON_CALL(*mMockVirtualCameraClientCallback, onStreamClosed)
        .WillByDefault(ndk::ScopedAStatus::ok);
  }

 protected:
  std::shared_ptr<MockCameraDeviceCallback> mMockCameraDeviceCallback;
  std::shared_ptr<MockVirtualCameraCallback> mMockVirtualCameraClientCallback;
  std::shared_ptr<VirtualCameraDevice> mVirtualCameraDevice;
  std::shared_ptr<VirtualCameraSession> mVirtualCameraSession;
};

TEST_F(VirtualCameraSessionTest, ConfigureTriggersClientConfigureCallback) {
  PixelFormat format = PixelFormat::YCBCR_420_888;
  StreamConfiguration streamConfiguration;
  streamConfiguration.streams = {
      createStream(kStreamId, kWidth, kHeight, format)};
  std::vector<HalStream> halStreams;
  EXPECT_CALL(
      *mMockVirtualCameraClientCallback,
      onStreamConfigured(kStreamId, _, kWidth, kHeight, Format::YUV_420_888));

  ASSERT_TRUE(
      mVirtualCameraSession->configureStreams(streamConfiguration, &halStreams)
          .isOk());

  EXPECT_THAT(halStreams, SizeIs(streamConfiguration.streams.size()));
  EXPECT_THAT(mVirtualCameraSession->getStreamIds(), ElementsAre(0));
}

TEST_F(VirtualCameraSessionTest, SecondConfigureDropsUnreferencedStreams) {
  PixelFormat format = PixelFormat::YCBCR_420_888;
  StreamConfiguration streamConfiguration;
  std::vector<HalStream> halStreams;

  streamConfiguration.streams = {createStream(0, kWidth, kHeight, format),
                                 createStream(1, kWidth, kHeight, format),
                                 createStream(2, kWidth, kHeight, format)};
  ASSERT_TRUE(
      mVirtualCameraSession->configureStreams(streamConfiguration, &halStreams)
          .isOk());

  EXPECT_THAT(mVirtualCameraSession->getStreamIds(), ElementsAre(0, 1, 2));

  streamConfiguration.streams = {createStream(0, kWidth, kHeight, format),
                                 createStream(2, kWidth, kHeight, format),
                                 createStream(3, kWidth, kHeight, format)};
  ASSERT_TRUE(
      mVirtualCameraSession->configureStreams(streamConfiguration, &halStreams)
          .isOk());

  EXPECT_THAT(mVirtualCameraSession->getStreamIds(), ElementsAre(0, 2, 3));
}

TEST_F(VirtualCameraSessionTest, CloseTriggersClientTerminateCallback) {
  EXPECT_CALL(*mMockVirtualCameraClientCallback, onStreamClosed(kStreamId))
      .WillOnce(Return(ndk::ScopedAStatus::ok()));

  ASSERT_TRUE(mVirtualCameraSession->close().isOk());
}

TEST_F(VirtualCameraSessionTest, FlushBeforeConfigure) {
  // Flush request coming before the configure request finished
  // (so potentially the thread is not yet running) should be
  // gracefully handled.

  EXPECT_TRUE(mVirtualCameraSession->flush().isOk());
}

TEST_F(VirtualCameraSessionTest, onProcessCaptureRequestTriggersClientCallback) {
  StreamConfiguration streamConfiguration;
  streamConfiguration.streams = {
      createStream(kStreamId, kWidth, kHeight, PixelFormat::YCBCR_420_888)};
  std::vector<CaptureRequest> requests(1);
  requests[0].frameNumber = 42;
  requests[0].settings = *(
      MetadataBuilder().setControlAfMode(ANDROID_CONTROL_AF_MODE_AUTO).build());

  std::vector<HalStream> halStreams;
  ASSERT_TRUE(
      mVirtualCameraSession->configureStreams(streamConfiguration, &halStreams)
          .isOk());

  EXPECT_CALL(*mMockVirtualCameraClientCallback,
              onProcessCaptureRequest(kStreamId, requests[0].frameNumber))
      .WillOnce(Return(ndk::ScopedAStatus::ok()));
  int32_t aidlReturn = 0;
  ASSERT_TRUE(mVirtualCameraSession
                  ->processCaptureRequest(requests, /*in_cachesToRemove=*/{},
                                          &aidlReturn)
                  .isOk());
  EXPECT_THAT(aidlReturn, Eq(requests.size()));
}

TEST_F(VirtualCameraSessionTest, configureAfterCameraRelease) {
  StreamConfiguration streamConfiguration;
  streamConfiguration.streams = {
      createStream(kStreamId, kWidth, kHeight, PixelFormat::YCBCR_420_888)};
  std::vector<HalStream> halStreams;

  // Release virtual camera.
  mVirtualCameraDevice.reset();

  // Expect configuration attempt returns CAMERA_DISCONNECTED service specific code.
  EXPECT_THAT(
      mVirtualCameraSession->configureStreams(streamConfiguration, &halStreams)
          .getServiceSpecificError(),
      Eq(static_cast<int32_t>(Status::CAMERA_DISCONNECTED)));
}

}  // namespace
}  // namespace virtualcamera
}  // namespace companion
}  // namespace android
