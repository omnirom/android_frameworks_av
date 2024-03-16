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
#include <fuzzbinder/libbinder_ndk_driver.h>
#include <fuzzer/FuzzedDataProvider.h>

#include <C2PlatformSupport.h>

#include <android/binder_interface_utils.h>
#include <codec2/aidl/ComponentStore.h>

using aidl::android::hardware::media::c2::utils::ComponentStore;
using android::fuzzService;
using ndk::SharedRefBase;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::shared_ptr<C2ComponentStore> store = ::android::GetCodec2PlatformComponentStore();
    std::shared_ptr<ComponentStore> binder = SharedRefBase::make<ComponentStore>(store);

    signal(SIGPIPE, SIG_IGN);
    fuzzService(binder->asBinder().get(), FuzzedDataProvider(data, size));

    return 0;
}
