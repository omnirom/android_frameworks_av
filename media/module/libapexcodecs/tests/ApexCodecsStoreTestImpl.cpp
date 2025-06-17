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

#include <android-base/no_destructor.h>
#include <apex/ApexCodecsImpl.h>

namespace android::apexcodecs::test {

// This is a test implementation of ApexComponentStoreIntf.
// It may contain different set of components than the APEX for testing purpose.
class ApexComponentStoreImpl : public ApexComponentStoreIntf {
public:
    ApexComponentStoreImpl() = default;

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() const override {
        return {};
    }
    virtual std::unique_ptr<ApexComponentIntf> createComponent(const char *name [[maybe_unused]]) {
        return nullptr;
    }
};

}  // namespace android::apexcodecs::test

extern "C" void *GetApexComponentStore() {
    using ::android::apexcodecs::test::ApexComponentStoreImpl;
    static ::android::base::NoDestructor<ApexComponentStoreImpl> sStore;
    return sStore.get();
}