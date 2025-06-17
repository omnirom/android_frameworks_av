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

#ifndef ANDROID_SERVERS_CAMERA_SHAREDSESSIONCONFIGREADER_H_
#define ANDROID_SERVERS_CAMERA_SHAREDSESSIONCONFIGREADER_H_

#include "SharedSessionConfigUtils.h"

#include <string>
#include "tinyxml2.h"
#include <vector>
#include <unordered_map>

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;
namespace android {

class SharedSessionConfigReader {
public:

    // Struct for shared session configurations.
    struct SharedSessionConfig {
        // TODO: add documentation for each field.
        int64_t surfaceType;
        int64_t width;
        int64_t height;
        std::string physicalCameraId;
        int64_t streamUseCase;
        int64_t timestampBase;
        int64_t mirrorMode;
        bool useReadoutTimestamp;
        int64_t format;
        int64_t usage;
        int64_t dataSpace;
    };

    // Reads shared session config files and stores parsed results in mColorSpace and
    // mCameraIdToSharedSessionConfigs.
    ErrorCode parseSharedSessionConfig(const char* sharedSessionConfigFilePath);

    // Reads shared session config files and stores parsed results in mColorSpace and
    // mCameraIdToSharedSessionConfigs.
    ErrorCode parseSharedSessionConfigFromXMLDocument(const XMLDocument& xmlDoc);

    // Return color space of a camera device.
    ErrorCode getColorSpace(int32_t* colorSpace);

    // Return all available shared configs for a cameraId.
    ErrorCode getAvailableSharedSessionConfigs(
            const char* cameraId, std::vector<SharedSessionConfig>* availableConfigurations);

private:

    // shared color space of devices
    int32_t mColorSpace;

    // stores parsed configs, mapped from cameraId to available session configs.
    std::unordered_map<std::string, std::vector<SharedSessionConfig>>
            mCameraIdToSharedSessionConfigs;

    // processes xml and populates mCameraIdToColorSpace and mCameraIdToSharedSessionConfigs. Called
    // by initialize().
    ErrorCode readConfig(const XMLElement* rootElem);
};

}  // namespace android

#endif  // ANDROID_SERVERS_CAMERA_SHAREDSESSIONCONFIGREADER_H_
