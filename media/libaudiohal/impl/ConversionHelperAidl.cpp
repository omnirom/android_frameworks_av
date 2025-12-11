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

#define LOG_TAG "ConversionHelperAidl"

#include <memory>

#include <media/AidlConversionUtil.h>
#include <utils/Log.h>

#include "ConversionHelperAidl.h"

using aidl::android::aidl_utils::statusTFromBinderStatus;
using aidl::android::hardware::audio::core::VendorParameter;
using aidl::android::media::audio::IHalAdapterVendorExtension;

namespace android {

status_t reparseVendorParameters(const std::string& vendorParameters, String8* values) {
    // Re-parse the vendor-provided string to ensure that it is correct.
    AudioParameter reparse(String8(vendorParameters.c_str()));
    if (reparse.size() != 0) {
        if (values->length() > 0) {
            values->append(";");
        }
        values->append(reparse.toString().c_str());
    }
    return OK;
}

status_t fillVendorParameterIds(std::shared_ptr<IHalAdapterVendorExtension> vendorExt,
                                IHalAdapterVendorExtension::ParameterScope scope,
                                const AudioParameter& parameterKeys,
                                std::vector<std::string>& vendorParametersIds) {
    if (parameterKeys.size() == 0) return OK;
    const String8 rawKeys = parameterKeys.keysToString();
    assert(vendorParametersIds.size() == 0);
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(vendorExt->parseVendorParameterIds(
            scope, std::string(rawKeys.c_str()), &vendorParametersIds)));
    return OK;
}

status_t fillKeyValuePairsFromVendorParameters(
        std::shared_ptr<IHalAdapterVendorExtension> vendorExt,
        IHalAdapterVendorExtension::ParameterScope scope,
        const std::vector<VendorParameter>& vendorParameters, String8* values) {
    if (vendorParameters.empty()) {
        return OK;
    }
    std::string keyValuePairs;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(
            vendorExt->processVendorParameters(scope, vendorParameters, &keyValuePairs)));
    RETURN_STATUS_IF_ERROR(reparseVendorParameters(keyValuePairs, values));
    return OK;
}

status_t fillVendorParameters(std::shared_ptr<IHalAdapterVendorExtension> vendorExt,
                              IHalAdapterVendorExtension::ParameterScope scope,
                              const AudioParameter& parameters,
                              std::vector<VendorParameter>& syncParameters,
                              std::vector<VendorParameter>& asyncParameters) {
    if (parameters.size() == 0) return OK;

    assert(syncParameters.empty() && asyncParameters.empty());

    const String8 rawKeysAndValues = parameters.toString();
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(vendorExt->parseVendorParameters(
            scope, std::string(rawKeysAndValues.c_str()), &syncParameters, &asyncParameters)));
    return OK;
}

} // namespace android
