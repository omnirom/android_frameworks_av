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
#include "VirtualCameraCaptureResult.h"

#include <cstdint>
#include <memory>

#include "VirtualCameraCaptureRequest.h"
#include "aidl/android/hardware/camera/device/CameraMetadata.h"
#include "system/camera_metadata.h"
#include "util/MetadataUtil.h"

namespace android {
namespace companion {
namespace virtualcamera {

using ::aidl::android::hardware::camera::device::CameraMetadata;
namespace {
// See REQUEST_PIPELINE_DEPTH in CaptureResult.java.
// This roughly corresponds to frame latency, we set to
// documented minimum of 2.
static constexpr uint8_t kPipelineDepth = 2;

}  // namespace

std::unique_ptr<CameraMetadata> createCaptureResultMetadata(
    const std::chrono::nanoseconds timestamp,
    const RequestSettings& requestSettings,
    const Resolution reportedSensorSize,
    const camera_metadata_t* customMetadata) {
  // All of the keys used in the response needs to be referenced in
  // availableResultKeys in CameraCharacteristics (see initCameraCharacteristics
  // in VirtualCameraDevice.cc).
  MetadataBuilder builder;
  builder
      .setAberrationCorrectionMode(ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF)
      .setControlAeAvailableAntibandingModes(
          {ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF})
      .setControlAeAntibandingMode(ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF)
      .setControlAeExposureCompensation(0)
      .setControlAeLockAvailable(false)
      .setControlAeLock(ANDROID_CONTROL_AE_LOCK_OFF)
      .setControlAeMode(ANDROID_CONTROL_AE_MODE_ON)
      .setControlAePrecaptureTrigger(
          // Limited devices are expected to have precapture ae enabled and
          // respond to cancellation request. Since we don't actuall support
          // AE at all, let's just respect the cancellation expectation in
          // case it's requested
          requestSettings.aePrecaptureTrigger ==
                  ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL
              ? ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL
              : ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE)
      .setControlAeState(ANDROID_CONTROL_AE_STATE_INACTIVE)
      .setControlAfMode(ANDROID_CONTROL_AF_MODE_OFF)
      .setControlAfTrigger(ANDROID_CONTROL_AF_TRIGGER_IDLE)
      .setControlAfState(ANDROID_CONTROL_AF_STATE_INACTIVE)
      .setControlAwbMode(ANDROID_CONTROL_AWB_MODE_AUTO)
      .setControlAwbLock(ANDROID_CONTROL_AWB_LOCK_OFF)
      .setControlAwbState(ANDROID_CONTROL_AWB_STATE_INACTIVE)
      .setControlCaptureIntent(requestSettings.captureIntent)
      .setControlEffectMode(ANDROID_CONTROL_EFFECT_MODE_OFF)
      .setControlMode(ANDROID_CONTROL_MODE_AUTO)
      .setControlSceneMode(ANDROID_CONTROL_SCENE_MODE_DISABLED)
      .setControlVideoStabilizationMode(
          ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF)
      .setCropRegion(0, 0, reportedSensorSize.width, reportedSensorSize.height)
      .setFaceDetectMode(ANDROID_STATISTICS_FACE_DETECT_MODE_OFF)
      .setFlashState(ANDROID_FLASH_STATE_UNAVAILABLE)
      .setFlashMode(ANDROID_FLASH_MODE_OFF)
      .setFocalLength(VirtualCameraDevice::kFocalLength)
      .setJpegQuality(requestSettings.jpegQuality)
      .setJpegOrientation(requestSettings.jpegOrientation)
      .setJpegThumbnailSize(requestSettings.thumbnailResolution.width,
                            requestSettings.thumbnailResolution.height)
      .setJpegThumbnailQuality(requestSettings.thumbnailJpegQuality)
      .setLensOpticalStabilizationMode(
          ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF)
      .setNoiseReductionMode(ANDROID_NOISE_REDUCTION_MODE_OFF)
      .setPipelineDepth(kPipelineDepth)
      .setSensorTimestamp(timestamp)
      .setStatisticsHotPixelMapMode(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF)
      .setStatisticsLensShadingMapMode(
          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF)
      .setStatisticsSceneFlicker(ANDROID_STATISTICS_SCENE_FLICKER_NONE)
      .setCustomMetadata(customMetadata);

  if (requestSettings.fpsRange.has_value()) {
    builder.setControlAeTargetFpsRange(requestSettings.fpsRange.value());
  }

  if (requestSettings.gpsCoordinates.has_value()) {
    const GpsCoordinates& coordinates = requestSettings.gpsCoordinates.value();
    builder.setJpegGpsCoordinates(coordinates);
  }

  std::unique_ptr<CameraMetadata> metadata = builder.build();

  if (metadata == nullptr) {
    ALOGE("%s: Failed to build capture result metadata", __func__);
    return std::make_unique<CameraMetadata>();
  }
  return metadata;
}

}  // namespace virtualcamera
}  // namespace companion
}  // namespace android