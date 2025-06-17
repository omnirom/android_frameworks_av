/*
 * Copyright 2024 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "Codec2-InputSurface"
#include <android-base/logging.h>

#include <codec2/aidl/inputsurface/InputSurfaceConnection.h>
#include <codec2/aidl/inputsurface/InputSurfaceSource.h>

namespace aidl::android::hardware::media::c2::utils {

InputSurfaceConnection::InputSurfaceConnection(
        const std::shared_ptr<IInputSink>& sink,
        ::android::sp<c2::implementation::InputSurfaceSource> const &source)
        : mSink{sink}, mSource{source} {
}

InputSurfaceConnection::~InputSurfaceConnection() {
}

c2_status_t InputSurfaceConnection::status() const {
    // TODO;
    return C2_OK;
}

::ndk::ScopedAStatus InputSurfaceConnection::disconnect() {
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus InputSurfaceConnection::signalEndOfStream() {
    return ::ndk::ScopedAStatus::ok();
}

c2_status_t InputSurfaceConnection::submitBuffer(
        int32_t bufferId, const AImage *buffer, int64_t timestamp, int fenceFd) {
    (void)bufferId;
    (void)buffer;
    (void)timestamp;
    (void)fenceFd;
    return C2_OK;
}

c2_status_t InputSurfaceConnection::submitEos(int32_t bufferId) {
    (void)bufferId;
    return C2_OK;
}

void InputSurfaceConnection::dispatchDataSpaceChanged(
            int32_t dataSpace, int32_t aspects, int32_t pixelFormat) {
    (void)dataSpace;
    (void)aspects;
    (void)pixelFormat;
}

}  // namespace aidl::android::hardware::media::c2::utils
