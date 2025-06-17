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

#define LOG_TAG "SharedSessionConfigUtils"

#include "SharedSessionConfigUtils.h"

#include <inttypes.h>
#include <sstream>
#include <utils/Log.h>

namespace android {

const char* SharedSessionConfigUtils::toString(ErrorCode errorCode) {
    switch (errorCode) {
        case ErrorCode::STATUS_OK:
            return "STATUS_OK";
        case ErrorCode::ERROR_READ_CONFIG_FILE:
            return "ERROR_READ_CONFIG_FILE";
        case ErrorCode::ERROR_CONFIG_FILE_FORMAT:
            return "ERROR_CONFIG_FILE_FORMAT";
        case ErrorCode::ERROR_CONFIG_READER_UNINITIALIZED:
            return "ERROR_CONFIG_READER_UNINITIALIZED";
        case ErrorCode::ERROR_BAD_PARAMETER:
            return "ERROR_BAD_PARAMETER";
        default:
            ALOGE("%s: Called toString on an unknown ErrorCode. This should never happen",
                  __FUNCTION__);
            return "";
    }
}

ErrorCode SharedSessionConfigUtils::getColorSpaceFromStr(const char* colorSpaceStr,
                                                         /* out */ int32_t* colorSpace) {
    if (colorSpaceStr == nullptr || !strcmp(colorSpaceStr, "")) {
        *colorSpace = ANDROID_REQUEST_AVAILABLE_COLOR_SPACE_PROFILES_MAP_UNSPECIFIED;
        return ErrorCode::STATUS_OK;
    }

    int32_t colorSpaceInt = (int32_t) std::strtol(colorSpaceStr, nullptr, 0);
    if (VALID_COLOR_SPACES.find(colorSpaceInt) == VALID_COLOR_SPACES.end()) {
        ALOGE("%s: colorSpace %" PRId32 " is invalid: ", __FUNCTION__, colorSpaceInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_COLOR_SPACES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *colorSpace = colorSpaceInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getSurfaceTypeFromXml(const XMLElement* surfaceTypeXml,
                                                          /* out */ int64_t* surfaceType) {
    if (surfaceTypeXml == nullptr || surfaceTypeXml->GetText() == nullptr
            || !strcmp(surfaceTypeXml->GetText(), "")) {
        ALOGE("%s: surface type field must be populated", __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    int64_t surfaceTypeInt = std::strtol(surfaceTypeXml->GetText(), nullptr, 0);
    if (VALID_SURFACE_TYPES.find(surfaceTypeInt) == VALID_SURFACE_TYPES.end()) {
        ALOGE("%s: surfaceType %" PRId64 " is invalid: ", __FUNCTION__, surfaceTypeInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_SURFACE_TYPES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *surfaceType = surfaceTypeInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getWidthFromXml(const XMLElement* widthXml,
                                                    /* out */ int64_t* width) {
    if (widthXml == nullptr || widthXml->GetText() == nullptr
            || !strcmp(widthXml->GetText(), "")) {
        ALOGE("%s: width field must be populated", __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    const char* widthStr = widthXml->GetText();
    *width = std::strtol(widthStr, nullptr, 0);
    if (*width <= 0) {
        ALOGE("%s: width value is invalid", __FUNCTION__);
    }

    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getHeightFromXml(const XMLElement* heightXml,
                                                     /* out */ int64_t* height) {
    if (heightXml == nullptr || heightXml->GetText() == nullptr
            || !strcmp(heightXml->GetText(), "")) {
        ALOGE("%s: height field must be populated", __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    const char* heightStr = heightXml->GetText();
    *height = std::strtol(heightStr, nullptr, 0);
    if (*height <= 0) {
        ALOGE("%s: height value is invalid", __FUNCTION__);
    }

    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getPhysicalCameraIdFromXml(
        const XMLElement* physicalCameraIdXml, /* out */ std::string* physicalCameraId) {
    *physicalCameraId =
            (physicalCameraIdXml == nullptr || physicalCameraIdXml->GetText() == nullptr)
                    ? "": physicalCameraIdXml->GetText();
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getStreamUseCaseFromXml(const XMLElement* streamUseCaseXml,
                                                            /* out */ int64_t* streamUseCase) {
    if (streamUseCaseXml == nullptr || streamUseCaseXml->GetText() == nullptr
            || !strcmp(streamUseCaseXml->GetText(), "")) {
        *streamUseCase = ANDROID_SCALER_AVAILABLE_STREAM_USE_CASES_DEFAULT;
        return ErrorCode::STATUS_OK;
    }

    int64_t streamUseCaseInt = std::strtol(streamUseCaseXml->GetText(), nullptr, 0);
    if (VALID_STREAM_USE_CASES.find(streamUseCaseInt) == VALID_STREAM_USE_CASES.end()) {
        ALOGE("%s: streamUseCase %" PRId64 " is invalid: ", __FUNCTION__, streamUseCaseInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_STREAM_USE_CASES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *streamUseCase = streamUseCaseInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getTimestampBaseFromXml(const XMLElement* timestampBaseXml,
                                                            /* out */ int64_t* timestampBase) {
    if (timestampBaseXml == nullptr || timestampBaseXml->GetText() == nullptr
            || !strcmp(timestampBaseXml->GetText(), "")) {
        *timestampBase = OutputConfiguration::TIMESTAMP_BASE_DEFAULT;
        return ErrorCode::STATUS_OK;
    }

    int64_t timestampBaseInt = std::strtol(timestampBaseXml->GetText(), nullptr, 0);
    if (VALID_TIMESTAMP_BASES.find(timestampBaseInt) == VALID_TIMESTAMP_BASES.end()) {
        ALOGE("%s: timestampBase %" PRId64 " is invalid: ", __FUNCTION__, timestampBaseInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_TIMESTAMP_BASES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *timestampBase = timestampBaseInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getMirrorModeFromXml(const XMLElement* mirrorModeXml,
                                                         /* out */ int64_t* mirrorMode) {
    if (mirrorModeXml == nullptr || mirrorModeXml->GetText() == nullptr
            || !strcmp(mirrorModeXml->GetText(), "")) {
        *mirrorMode = OutputConfiguration::MIRROR_MODE_AUTO;
        return ErrorCode::STATUS_OK;
    }

    int64_t mirrorModeInt = std::strtol(mirrorModeXml->GetText(), nullptr, 0);
    if (VALID_MIRROR_MODES.find(mirrorModeInt) == VALID_MIRROR_MODES.end()) {
        ALOGE("%s: mirrorMode %" PRId64 " is invalid: ", __FUNCTION__, mirrorModeInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_MIRROR_MODES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *mirrorMode = mirrorModeInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getUseReadoutTimestampFromXml(
        const XMLElement* useReadoutTimestampXml, /* out */ bool* useReadoutTimestamp) {
    if (useReadoutTimestampXml != nullptr && useReadoutTimestampXml->GetText() != nullptr
            && strcmp(useReadoutTimestampXml->GetText(), "")) {
        const char* useReadoutTimestampStr = useReadoutTimestampXml->GetText();
        if (!strcmp(useReadoutTimestampStr, "1")) {
            *useReadoutTimestamp = true;
            return ErrorCode::STATUS_OK;
        } else if (strcmp(useReadoutTimestampStr, "0")) {
            ALOGE("%s: useReadoutTimestamp string %s is invalid: ", __FUNCTION__,
                  useReadoutTimestampStr);
            ALOGE("%s: Expected one of: {0, 1}", __FUNCTION__);
            return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
        }
    }

    *useReadoutTimestamp = false;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getFormatFromXml(const XMLElement* formatXml,
                                                     /* out */ int64_t* format,
                                                     int64_t surfaceType) {
    if (surfaceType != OutputConfiguration::SURFACE_TYPE_IMAGE_READER) {
        // if surface type is not image reader, format must default to impl defined enum.
        *format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
        return ErrorCode::STATUS_OK;
    }

    if (formatXml == nullptr || formatXml->GetText() == nullptr
            || !strcmp(formatXml->GetText(), "")) {
        ALOGE("%s: format field must be populated", __FUNCTION__);
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    int64_t formatInt = std::strtol(formatXml->GetText(), nullptr, 0);
    if (VALID_FORMATS.find(formatInt) == VALID_FORMATS.end()) {
        ALOGE("%s: format %" PRId64 " is invalid: ", __FUNCTION__, formatInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_FORMATS).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *format = formatInt;
    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getUsageFromXml(const XMLElement* usageXml,
                                                    /* out */ int64_t* usage,
                                                    int64_t surfaceType) {
    if (surfaceType == OutputConfiguration::SURFACE_TYPE_SURFACE_TEXTURE) {
        // if surface type is SURFACE_TYPE_SURFACE_TEXTURE, usage must default to
        // AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE.
        *usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        return ErrorCode::STATUS_OK;
    }

    if (surfaceType == OutputConfiguration::SURFACE_TYPE_SURFACE_VIEW) {
        // if surface type is SURFACE_TYPE_SURFACE_VIEW, usage must default to
        // AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY.
        *usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;
        return ErrorCode::STATUS_OK;
    }

    if (surfaceType == OutputConfiguration::SURFACE_TYPE_MEDIA_RECORDER
            || surfaceType == OutputConfiguration::SURFACE_TYPE_MEDIA_CODEC) {
        // if surface type is SURFACE_TYPE_MEDIA_RECORDER or SURFACE_TYPE_MEDIA_CODEC, usage must
        // default to AHARDWAREBUFFER_USAGE_VIDEO_ENCODE
        *usage = AHARDWAREBUFFER_USAGE_VIDEO_ENCODE;
        return ErrorCode::STATUS_OK;
    }

    if (usageXml == nullptr || usageXml->GetText() == nullptr
            || !strcmp(usageXml->GetText(), "")) {
        *usage = AHARDWAREBUFFER_USAGE_CPU_READ_NEVER;
        return ErrorCode::STATUS_OK;
    }

    const char* usageStr = usageXml->GetText();
    std::vector<std::string> usageFlags = splitString(usageStr, '|');

    for (std::string usageFlagStr : usageFlags) {
        int64_t usageFlag = std::strtol(usageFlagStr.c_str(), nullptr, 0);
        if (VALID_USAGES.find(usageFlag) == VALID_USAGES.end()) {
            ALOGE("%s: usage %" PRId64 " is invalid: ", __FUNCTION__, usageFlag);
            ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_USAGES).c_str());
            return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
        }

        *usage |= usageFlag;
    }

    return ErrorCode::STATUS_OK;
}

ErrorCode SharedSessionConfigUtils::getDataSpaceFromXml(const XMLElement* dataSpaceXml,
                                                        /* out */ int64_t* dataSpace) {
    if (dataSpaceXml == nullptr || dataSpaceXml->GetText() == nullptr
            || !strcmp(dataSpaceXml->GetText(), "")) {
        *dataSpace = HAL_DATASPACE_UNKNOWN;
        return ErrorCode::STATUS_OK;
    }

    int64_t dataSpaceInt = std::strtol(dataSpaceXml->GetText(), nullptr, 0);
    if (VALID_DATA_SPACES.find(dataSpaceInt) == VALID_DATA_SPACES.end()) {
        ALOGE("%s: dataSpace %" PRId64 " is invalid: ", __FUNCTION__, dataSpaceInt);
        ALOGE("%s: Expected one of: %s", __FUNCTION__, setToString(VALID_DATA_SPACES).c_str());
        return ErrorCode::ERROR_CONFIG_FILE_FORMAT;
    }

    *dataSpace = dataSpaceInt;
    return ErrorCode::STATUS_OK;
}

std::vector<std::string> SharedSessionConfigUtils::splitString(std::string inputString,
                                                               char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(inputString);
    std::string token;

    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string SharedSessionConfigUtils::setToString(const std::set<int64_t>& s) {
    std::ostringstream oss;
    oss << "{";

    for (auto it = s.begin(); it != s.end();) {
        oss << *it;

        if (++it != s.end()) {
            oss << ", ";
        }
    }

    oss << "}";
    return oss.str();
}

}  // namespace android
