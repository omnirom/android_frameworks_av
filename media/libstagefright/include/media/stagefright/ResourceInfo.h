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

#ifndef RESOURCE_INFO_H_
#define RESOURCE_INFO_H_

#include <string>

namespace android {
/**
 * Abstraction for the Global Codec resources.
 * This encapsulates all the available codec resources on the device.
 */
struct GlobalResourceInfo {
    /**
     * Name of the Resource type.
     */
    std::string mName;
    /**
     * Total count/capacity of resources of this type.
     */
    uint64_t mCapacity;
    /**
     * Available count of this resource type.
     */
    uint64_t mAvailable;

    GlobalResourceInfo(const std::string& name, uint64_t capacity, uint64_t available) :
            mName(name),
            mCapacity(capacity),
            mAvailable(available) {}

    GlobalResourceInfo(const GlobalResourceInfo& info) :
            mName(info.mName),
            mCapacity(info.mCapacity),
            mAvailable(info.mAvailable) {}
};

/**
 * Abstraction for the resources associated with a codec instance.
 * This encapsulates the required codec resources for a configured codec instance.
 */
struct InstanceResourceInfo {
    /**
     * Name of the Resource type.
     */
    std::string mName;
    /**
     * Required resource count of this type.
     */
    uint64_t mStaticCount;
    /**
     * Per frame resource requirement of this resource type.
     */
    uint64_t mPerFrameCount;

    InstanceResourceInfo(const std::string& name, uint64_t staticCount, uint64_t perFrameCount) :
            mName(name),
            mStaticCount(staticCount),
            mPerFrameCount(perFrameCount) {}

    InstanceResourceInfo(const InstanceResourceInfo& info) :
            mName(info.mName),
            mStaticCount(info.mStaticCount),
            mPerFrameCount(info.mPerFrameCount) {}
};

}  // namespace android

#endif // RESOURCE_INFO_H_
