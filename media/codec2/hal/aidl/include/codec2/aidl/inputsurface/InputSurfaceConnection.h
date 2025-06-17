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

#pragma once

#include <aidl/android/hardware/media/c2/BnInputSink.h>
#include <aidl/android/hardware/media/c2/BnInputSurfaceConnection.h>
#include <media/NdkImage.h>
#include <utils/RefBase.h>

#include <C2.h>

#include <memory>

namespace aidl::android::hardware::media::c2::implementation {
class InputSurfaceSource;
}

namespace aidl::android::hardware::media::c2::utils {

struct InputSurfaceConnection : public BnInputSurfaceConnection {
    InputSurfaceConnection(
            const std::shared_ptr<IInputSink>& sink,
            ::android::sp<c2::implementation::InputSurfaceSource> const &source);
    c2_status_t status() const;

    // Methods from IInputSurfaceConnection follow.
    ::ndk::ScopedAStatus disconnect() override;
    ::ndk::ScopedAStatus signalEndOfStream() override;

    // implementation specific interface.

    // Submit a buffer to the connected component.
    c2_status_t submitBuffer(
            int32_t bufferId,
            const AImage *buffer = nullptr,
            int64_t timestamp = 0,
            int fenceFd = -1);

    // Submit eos to the connected component.
    c2_status_t submitEos(int32_t bufferId);

    // notify dataspace being changed to the component.
    void dispatchDataSpaceChanged(
            int32_t dataSpace, int32_t aspects, int32_t pixelFormat);

protected:
    virtual ~InputSurfaceConnection() override;

private:
    std::weak_ptr<IInputSink> mSink;
    ::android::sp<c2::implementation::InputSurfaceSource> mSource;
};

}  // namespace aidl::android::hardware::media::c2::utils
