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

#include <C2Config.h>
#include <media/stagefright/CCodecResources.h>

namespace android {

// Construct the name based on the component store name
// and the id of the resource.
// resource name = "componentStoreName-id"
static inline std::string getResourceName(const std::string& componentStoreName, uint32_t id) {
    return componentStoreName + "-" + std::to_string(id);
}

static
c2_status_t queryGlobalResources(const std::shared_ptr<Codec2Client>& client,
                                 std::vector<GlobalResourceInfo>& systemAvailableResources) {
    std::vector<std::unique_ptr<C2Param>> heapParams;
    c2_status_t c2err = client->query(
            {},
            {C2ResourcesCapacityTuning::PARAM_TYPE, C2ResourcesExcludedTuning::PARAM_TYPE},
            C2_MAY_BLOCK,
            &heapParams);

    if (c2err == C2_OK || heapParams.size() == 2u) {
        // Construct Globally available resources now.
        // Get the total capacity first.
        std::string storeName = client->getServiceName();
        const C2ResourcesCapacityTuning* systemCapacity =
                C2ResourcesCapacityTuning::From(heapParams[0].get());
        if (systemCapacity && *systemCapacity) {
            for (size_t i = 0; i < systemCapacity->flexCount(); ++i) {
                const C2SystemResourceStruct& resource =
                    systemCapacity->m.values[i];
                std::string name = getResourceName(storeName, resource.id);
                uint64_t capacity = (resource.kind == CONST) ? resource.amount : 0;
                systemAvailableResources.push_back({name, capacity, capacity});
            }
        } else {
            ALOGW("Failed to get C2ResourcesCapacityTuning");
        }

        // Get the excluded resource info.
        // The available resource should exclude this, if there are any.
        const C2ResourcesExcludedTuning* systemExcluded =
                C2ResourcesExcludedTuning::From( heapParams[1].get());
        if (systemExcluded && *systemExcluded) {
            for (size_t i = 0; i < systemExcluded->flexCount(); ++i) {
                const C2SystemResourceStruct& resource =
                    systemExcluded->m.values[i];
                std::string name = getResourceName(storeName, resource.id);
                uint64_t excluded = (resource.kind == CONST) ? resource.amount : 0;
                auto found = std::find_if(systemAvailableResources.begin(),
                                          systemAvailableResources.end(),
                                          [name](const GlobalResourceInfo& item) {
                                              return item.mName == name; });

                if (found != systemAvailableResources.end()) {
                    // Take off excluded resources from available resources.
                    if (found->mAvailable >= excluded) {
                        found->mAvailable -= excluded;
                    } else {
                        ALOGW("Excluded resources(%jd) can't be more than Available resources(%jd)",
                              excluded, found->mAvailable);
                        found->mAvailable = 0;
                    }
                } else {
                    ALOGW("Failed to find the resource [%s]", name.c_str());
                }
            }
        } else {
            ALOGW("Failed to get C2ResourcesExcludedTuning");
        }

    } else {
        ALOGW("Failed to query component store for system resources: %d", c2err);
    }

    return c2err;
}

/**
 * A utility function that converts C2ResourcesNeededTuning into
 * a vector of InstanceResourceInfo.
 *
 * Right now, this function is at its simplest form looking into
 * mapping constant and per frame resource kinds,
 * but we need to extend this to address:
 *  - Construct the name for each resources
 *    (using the resource id and component store)
 *  - Devise a unified way of presenting per frame, per input/output block
 *    resource requirements.
 */
static status_t getSystemResource(const C2ResourcesNeededTuning* systemResourcesInfo,
                                  const std::string& storeName,
                                  std::vector<InstanceResourceInfo>& resources) {
    resources.clear();
    if (systemResourcesInfo && *systemResourcesInfo) {
        for (size_t i = 0; i < systemResourcesInfo->flexCount(); ++i) {
            const C2SystemResourceStruct& resource =
                systemResourcesInfo->m.values[i];
            uint64_t staticCount = 0;
            uint64_t perFrameCount = 0;
            std::string name = getResourceName(storeName, resource.id);

            switch (resource.kind) {
            case CONST:
                staticCount = resource.amount;
                break;
            case PER_FRAME:
                perFrameCount = resource.amount;
                break;
            case PER_INPUT_BLOCK:
            case PER_OUTPUT_BLOCK:
                // TODO: Find a way to pass this info through InstanceResourceInfo
                // For now, we are using this as per frame count.
                perFrameCount = resource.amount;
                break;
            }
            resources.push_back({name, staticCount, perFrameCount});
        }

        return OK;
    }

    return UNKNOWN_ERROR;
}

//static
std::vector<GlobalResourceInfo> CCodecResources::GetGloballyAvailableResources() {
    // Try creating client from "default" service:
    std::shared_ptr<Codec2Client> client = Codec2Client::CreateFromService("default");
    if (client) {
        // Query the system resource capacity from the component store.
        std::vector<GlobalResourceInfo> systemAvailableResources;
        c2_status_t status = queryGlobalResources(client, systemAvailableResources);
        if (status == C2_OK) {
            return systemAvailableResources;
        }
    } else {
        ALOGW("Failed to create client from default component store!");
    }

    return std::vector<GlobalResourceInfo>{};
}

CCodecResources::CCodecResources(const std::string& storeName) : mStoreName(storeName) {}

status_t CCodecResources::queryRequiredResources(
        const std::shared_ptr<Codec2Client::Component>& comp) {
    // Query required/needed system resources for the current configuration.
    std::vector<std::unique_ptr<C2Param>> heapParams;
    c2_status_t c2err = comp->query(
            {},
            {C2ResourcesNeededTuning::PARAM_TYPE},
            C2_MAY_BLOCK,
            &heapParams);
    if (c2err != C2_OK || heapParams.size() != 1u) {
        ALOGE("Failed to query component interface for required system resources: %d", c2err);
        return UNKNOWN_ERROR;
    }

    // Construct Required System Resources.
    Mutexed<std::vector<InstanceResourceInfo>>::Locked resourcesLocked(mResources);
    std::vector<InstanceResourceInfo>& resources = *resourcesLocked;
    return getSystemResource(C2ResourcesNeededTuning::From(heapParams[0].get()),
                             mStoreName,
                             resources);
}

std::vector<InstanceResourceInfo> CCodecResources::getRequiredResources() {
    Mutexed<std::vector<InstanceResourceInfo>>::Locked resourcesLocked(mResources);
    return *resourcesLocked;
}

status_t CCodecResources::updateRequiredResources(
        const C2ResourcesNeededTuning* systemResourcesInfo) {
    // Update the required resources from the given systemResourcesInfo.
    Mutexed<std::vector<InstanceResourceInfo>>::Locked resourcesLocked(mResources);
    std::vector<InstanceResourceInfo>& resources = *resourcesLocked;
    return getSystemResource(systemResourcesInfo, mStoreName, resources);
}

} // namespace android
