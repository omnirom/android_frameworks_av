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

#define LOG_TAG "C2SoftIamfDec"
#include <log/log.h>

namespace android {

namespace {

constexpr char COMPONENT_NAME[] = "c2.android.iamf.decoder";

}  // namespace

class C2SoftIamfDec::IntfImpl : public SimpleInterface<void>::BaseParams {
  public:
    explicit IntfImpl(const std::shared_ptr<C2ReflectorHelper>& helper)
        : SimpleInterface<void>::BaseParams(helper, COMPONENT_NAME, C2Component::KIND_DECODER,
                                            C2Component::DOMAIN_AUDIO,
                                            // Replace with IAMF mimetype when available
                                            "audio/iamf") {
        // Configure (e.g. noPrivateBuffers(), etc.)
        // Add parameters.
    }
}

C2SoftIamfDec::C2SoftIamfDec(const char* name, c2_node_id_t id,
                             const std::shared_ptr<IntfImpl>& intfImpl)
    : SimpleC2Component(std::make_shared<SimpleInterface<IntfImpl>>(name, id, intfImpl)),
      mIntf(intfImpl) {
}

C2SoftIamfDec::~C2SoftIamfDec() {
    onRelease();
}

c2_status_t C2SoftIamfDec::onInit() {
    return C2_BAD_STATE;
}

c2_status_t C2SoftIamfDec::onStop() {
    return C2_NO_INIT;
}

void C2SoftIamfDec::onReset() {
    return;
}

void C2SoftIamfDec::onRelease() {
    return;
}

c2_status_t C2SoftIamfDec::onFlush_sm() {
    return C2_NO_INIT;
}

void C2SoftIamfDec::process(const std::unique_ptr<C2Work>& work,
                            const std::shared_ptr<C2BlockPool>& pool) {
    return;
}

c2_status_t C2SoftIamfDec::drain(uint32_t drainMode, const std::shared_ptr<C2BlockPool>& pool) {
    return C2_NO_INIT;
}

}  // namespace android
