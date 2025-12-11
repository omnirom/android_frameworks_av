/*
 * Copyright 2025 The Android Open Source Project
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
#define LOG_TAG "Codec2-InputSink"
#include <android-base/logging.h>

#include <C2.h>

#include <codec2/aidl/Component.h>
#include <codec2/aidl/InputSink.h>


namespace aidl::android::hardware::media::c2::utils {

InputSink::InputSink(const std::shared_ptr<Component>& component)
    : mComponent(component) {}

InputSink::~InputSink() {
}

::ndk::ScopedAStatus InputSink::queue(
        const ::aidl::android::hardware::media::c2::WorkBundle& in_workBundle) {
    auto comp = mComponent.lock();
    if (!comp) {
        ALOGE("Component not alive for queueing works");
        return ::ndk::ScopedAStatus::fromServiceSpecificError(C2_CORRUPTED);
    }
    return comp->queue(in_workBundle);
}

} // namespace aidl::android::hardware::media::c2::utils
