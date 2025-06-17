/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecListWriter"
#include <utils/Log.h>

#include <media/stagefright/foundation/AMessage.h>
#include "media/stagefright/foundation/AString.h"
#include <media/stagefright/MediaCodecListWriter.h>
#include <media/MediaCodecInfo.h>

#include <string>

namespace android {

void MediaCodecListWriter::addGlobalSetting(
        const char* key, const char* value) {
    mGlobalSettings.emplace_back(key, value);
}

std::unique_ptr<MediaCodecInfoWriter>
        MediaCodecListWriter::addMediaCodecInfo() {
    sp<MediaCodecInfo> info = new MediaCodecInfo();
    mCodecInfos.push_back(info);
    return std::unique_ptr<MediaCodecInfoWriter>(
            new MediaCodecInfoWriter(info.get()));
}

std::unique_ptr<MediaCodecInfoWriter>
        MediaCodecListWriter::findMediaCodecInfo(const char *name) {
    for (const sp<MediaCodecInfo> &info : mCodecInfos) {
        if (!strcmp(info->getCodecName(), name)) {
            return std::unique_ptr<MediaCodecInfoWriter>(new MediaCodecInfoWriter(info.get()));
        }
    }
    return nullptr;
}

void MediaCodecListWriter::writeGlobalSettings(
        const sp<AMessage> &globalSettings) const {
    for (const std::pair<std::string, std::string> &kv : mGlobalSettings) {
        globalSettings->setString(kv.first.c_str(), kv.second.c_str());
    }
}

void MediaCodecListWriter::writeCodecInfos(
        std::vector<sp<MediaCodecInfo>> *codecInfos) const {
    // Since the introduction of the NDK MediaCodecList API, each
    // MediaCodecInfo object can only support a single media type, so infos that
    // support multiple media types are split into multiple infos.
    // This process may result in name collisions that are handled here.

    // Prefer codec names that already support a single media type
    // and also any existing aliases. If an alias matches an existing
    // codec name, it is ignored, which is the right behavior.
    std::set<std::string> reservedNames;
    for (const sp<MediaCodecInfo> &info : mCodecInfos) {
        Vector<AString> mediaTypes;
        info->getSupportedMediaTypes(&mediaTypes);
        if (mediaTypes.size() == 1) {
            reservedNames.insert(info->getCodecName());
        }
        Vector<AString> aliases;
        info->getAliases(&aliases);
        for (const AString &alias : aliases) {
            reservedNames.insert(alias.c_str());
        }
    }

    for (const sp<MediaCodecInfo> &info : mCodecInfos) {
        Vector<AString> mediaTypes;
        info->getSupportedMediaTypes(&mediaTypes);
        if (mediaTypes.size() == 1) {
            codecInfos->push_back(info);
        } else {
            // disambiguate each type
            for (const AString &mediaType : mediaTypes) {
                // get the type name after the first slash (if exists)
                ssize_t slashPosition = mediaType.find("/");
                const char *typeName = mediaType.c_str() + (slashPosition + 1);

                // find a unique name for the split codec info starting with "<name>.<type>"
                AString newName = AStringPrintf("%s.%s", info->getCodecName(), typeName);
                std::string newNameStr = newName.c_str();
                // append increasing suffix of the form ".<number>" until a unique name is found
                for (size_t ix = 1; reservedNames.count(newNameStr) > 0; ++ix) {
                    newNameStr = AStringPrintf("%s.%zu", newName.c_str(), ix).c_str();
                }

                codecInfos->push_back(info->splitOutType(mediaType.c_str(), newNameStr.c_str()));
                reservedNames.insert(newNameStr);
            }
        }
    }
}

}  // namespace android
