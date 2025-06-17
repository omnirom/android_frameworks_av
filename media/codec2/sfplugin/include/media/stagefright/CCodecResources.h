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

#ifndef CCODEC_RESOURCES_H_
#define CCODEC_RESOURCES_H_

#include <string>
#include <vector>

#include <C2Component.h>
#include <codec2/hidl/client.h>

#include <media/stagefright/foundation/Mutexed.h>
#include <media/stagefright/ResourceInfo.h>

namespace android {

class CCodecResources {
public:
    CCodecResources(const std::string& storeName);

    /// Gets the globally available resources from the
    /// default store.
    static std::vector<GlobalResourceInfo> GetGloballyAvailableResources();

    /// Queries the regurired resources for the given codec component.
    status_t queryRequiredResources(
            const std::shared_ptr<Codec2Client::Component>& comp);

    /// Gets the required resources.
    std::vector<InstanceResourceInfo> getRequiredResources();

    /// Updates the required resources.
    status_t updateRequiredResources(const C2ResourcesNeededTuning* systemResourcesInfo);

private:
    const std::string mStoreName;
    Mutexed<std::vector<InstanceResourceInfo>> mResources;
};

} // namespace android

#endif  // CCODEC_RESOURCES_H_
