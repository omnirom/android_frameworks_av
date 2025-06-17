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

#pragma once

#include <memory>
#include <vector>

#include <C2Component.h>

#include <apex/ApexCodecs.h>
#include <apex/ApexCodecsParam.h>

namespace android::apexcodecs {

class ApexComponentIntf {
public:
    virtual ~ApexComponentIntf() = default;
    virtual ApexCodec_Status start() = 0;
    virtual ApexCodec_Status flush() = 0;
    virtual ApexCodec_Status reset() = 0;
    virtual ApexCodec_Configurable *getConfigurable() = 0;
    virtual ApexCodec_Status process(
            const ApexCodec_Buffer *input,
            ApexCodec_Buffer *output,
            size_t *consumed,
            size_t *produced) = 0;
};

class ApexComponentStoreIntf {
public:
    virtual ~ApexComponentStoreIntf() = default;
    virtual std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() const = 0;
    virtual std::unique_ptr<ApexComponentIntf> createComponent(const char *name) = 0;
};

}  // namespace android

__BEGIN_DECLS

void *GetApexComponentStore();

__END_DECLS