//
// Copyright 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "SharedSessionConfigReader"

#include "SharedSessionConfigReader.h"

#include <fstream>
#include <utils/Log.h>

using tinyxml2::XML_SUCCESS;
using tinyxml2::XMLDocument;
namespace android {

ErrorCode SharedSessionConfigReader::parseSharedSessionConfig(
        const char* sharedSessionConfigFilePath) {
    if (!mCameraIdToSharedSessionConfigs.empty()) {
        ALOGV("mCameraIdToSharedSessionConfigs already initialized.");
        return ErrorCode::STATUS_OK;
    }

    XMLDocument xmlDoc;

    // load and parse the configuration file
    xmlDoc.LoadFile(sharedSessionConfigFilePath);
    if (xmlDoc.ErrorID() != XML_SUCCESS) {
        ALOGE("%s: Failed to load/parse the configuration file: %s, with error: %s", __FUNCTION__,
              sharedSessionConfigFilePath, xmlDoc.ErrorStr());
        return ErrorCode::ERROR_READ_CONFIG_FILE;
    }

    ErrorCode status = parseSharedSessionConfigFromXMLDocument(xmlDoc);
    if (status != ErrorCode::STATUS_OK) {
        ALOGE("%s: Error while parsing XML elements of file at: %s", __FUNCTION__,
              sharedSessionConfigFilePath);
        return status;
    }

    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigReader::parseSharedSessionConfigFromXMLDocument(
        const XMLDocument& xmlDoc) {
    const XMLElement* rootElem = xmlDoc.RootElement();
    if (strcmp(rootElem->Name(), "SharedCameraSessionConfigurations")) {
        ALOGE("%s: Expected root element to be 'SharedCameraSessionConfigurations'. Instead got %s",
              __FUNCTION__, rootElem->Name());
        return ErrorCode::ERROR_READ_CONFIG_FILE;
    }

    ErrorCode status;
    const char* colorSpaceStr = rootElem->Attribute("colorSpace");
    status = SharedSessionConfigUtils::getColorSpaceFromStr(colorSpaceStr, &mColorSpace);
    if (status != ErrorCode::STATUS_OK) {
        ALOGE("%s: getColorSpaceFromStr has returned an error: %s", __FUNCTION__,
              SharedSessionConfigUtils::toString(status));
        return status;
    }

    std::unordered_map<std::string, std::vector<SharedSessionConfig>>
            cameraIdToSharedSessionConfigs;

    for (const XMLElement* sharedConfigElem =
                 rootElem->FirstChildElement("SharedCameraSessionConfiguration");
            sharedConfigElem != nullptr;
            sharedConfigElem =
                 sharedConfigElem->NextSiblingElement("SharedCameraSessionConfiguration")) {

        const char* cameraId = sharedConfigElem->Attribute("cameraId");
        if (cameraId == nullptr || !strcmp(cameraId, "")) {
            ALOGE("%s: cameraId attribute is empty", __FUNCTION__);
            return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
        }

        for (const XMLElement* outputConfigElem =
                     sharedConfigElem->FirstChildElement("OutputConfiguration");
                outputConfigElem != nullptr;
                outputConfigElem = outputConfigElem->NextSiblingElement("OutputConfiguration")) {
            int64_t surfaceType;
            const XMLElement* surfaceTypeXml = outputConfigElem->FirstChildElement("surfaceType");
            status = SharedSessionConfigUtils::getSurfaceTypeFromXml(surfaceTypeXml, &surfaceType);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getSurfaceTypeFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t width;
            const XMLElement* widthXml = outputConfigElem->FirstChildElement("width");
            status = SharedSessionConfigUtils::getWidthFromXml(widthXml, &width);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getWidthFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t height;
            const XMLElement* heightXml = outputConfigElem->FirstChildElement("height");
            status = SharedSessionConfigUtils::getHeightFromXml(heightXml, &height);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getHeightFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            std::string physicalCameraId;
            const XMLElement* physicalCameraIdXml =
                    outputConfigElem->FirstChildElement("physicalCameraId");
            status = SharedSessionConfigUtils::getPhysicalCameraIdFromXml(physicalCameraIdXml,
                                                                          &physicalCameraId);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getPhysicalCameraIdFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t streamUseCase;
            const XMLElement* streamUseCaseXml =
                    outputConfigElem->FirstChildElement("streamUseCase");
            status = SharedSessionConfigUtils::getStreamUseCaseFromXml(streamUseCaseXml,
                                                                       &streamUseCase);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getStreamUseCaseFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t timestampBase;
            const XMLElement* timestampBaseXml =
                    outputConfigElem->FirstChildElement("timestampBase");
            status = SharedSessionConfigUtils::getTimestampBaseFromXml(timestampBaseXml,
                                                                       &timestampBase);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getTimestampBaseFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t mirrorMode;
            const XMLElement* mirrorModeXml = outputConfigElem->FirstChildElement("mirrorMode");
            status = SharedSessionConfigUtils::getMirrorModeFromXml(mirrorModeXml, &mirrorMode);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getMirrorModeFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            bool useReadoutTimestamp;
            const XMLElement* useReadoutTimestampXml =
                    outputConfigElem->FirstChildElement("useReadoutTimestamp");
            status = SharedSessionConfigUtils::getUseReadoutTimestampFromXml(useReadoutTimestampXml,
                                                                             &useReadoutTimestamp);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getUseReadoutTimestampFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t format;
            const XMLElement* formatXml = outputConfigElem->FirstChildElement("format");
            status = SharedSessionConfigUtils::getFormatFromXml(formatXml, &format, surfaceType);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getFormatFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t usage;
            const XMLElement* usageXml = outputConfigElem->FirstChildElement("usage");
            status = SharedSessionConfigUtils::getUsageFromXml(usageXml, &usage, surfaceType);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getUsageFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            int64_t dataSpace;
            const XMLElement* dataSpaceXml = outputConfigElem->FirstChildElement("dataSpace");
            status = SharedSessionConfigUtils::getDataSpaceFromXml(dataSpaceXml, &dataSpace);
            if (status != ErrorCode::STATUS_OK) {
                ALOGE("%s: getUsageFromXml has returned an error: %s", __FUNCTION__,
                      SharedSessionConfigUtils::toString(status));
                return status;
            }

            cameraIdToSharedSessionConfigs[cameraId].push_back(
                    SharedSessionConfig{surfaceType, width, height, physicalCameraId, streamUseCase,
                                        timestampBase, mirrorMode, useReadoutTimestamp, format,
                                        usage, dataSpace});
        }
    }

    if (cameraIdToSharedSessionConfigs.empty()) {
        ALOGE("%s: No elements with tag 'SharedCameraSessionConfiguration' in file", __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    mCameraIdToSharedSessionConfigs = cameraIdToSharedSessionConfigs;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigReader::getColorSpace(/* out */ int32_t* colorSpace) {
    *colorSpace = mColorSpace;
    return ErrorCode::STATUS_OK;
}

// Returns the cameraConfig parameters.
ErrorCode SharedSessionConfigReader::getAvailableSharedSessionConfigs(
        const char* cameraId, /* out */ std::vector<SharedSessionConfig>* availableConfigurations) {
    if (mCameraIdToSharedSessionConfigs.empty()) {
        ALOGE("%s: mCameraIdToSharedSessionConfigs is empty. Call initialize() first.",
              __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_READER_UNINITIALIZED;
    }

    if (!mCameraIdToSharedSessionConfigs.contains(cameraId)) {
        ALOGE("%s: cameraId: %s not found in mCameraIdToSharedSessionConfigs.", __FUNCTION__,
              cameraId);
        return ErrorCode::ERROR_BAD_PARAMETER;
    }

    *availableConfigurations = mCameraIdToSharedSessionConfigs[cameraId];
    return ErrorCode::STATUS_OK;
}

}  // namespace android
