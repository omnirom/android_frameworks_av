/*
 * Copyright 2017, The Android Open Source Project
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

#define LOG_TAG "isAtLeastRelease"

#include <android/api-level.h>
#include <android-base/properties.h>
#include <utils/Log.h>

#include <mutex>
#include <string>

// current SDK for this device; filled in when initializing the parser.
static int mySdk = 0;
static std::string myCodeName;

// to help address b/388925029

/**
 * support code so a plugin (currently the APV codecs) can self-manage
 * whether it is running on a sufficiently new code base.
 *
 * this is here because the XMLparser for Media codec definitions has
 * an off-by-one error in how it handles <MediaCodec name=".." ... minsdk="" >
 *
 * we will want to fix that starting in Android B/16, but devices in Android V/15
 * still have issues [and we build the codecs into module code so that it goes back
 * to older releases].
 *
 */

bool isAtLeastRelease(int minsdk, const char *codename) {

    static std::once_flag sCheckOnce;
    std::call_once(sCheckOnce, [&](){
        mySdk = android_get_device_api_level();
        myCodeName  = android::base::GetProperty("ro.build.version.codename", "<none>");
    });

    bool satisfied = false;
    ALOGI("device sdk %d, minsdk %d", mySdk, minsdk);
    if (mySdk >= minsdk) {
        satisfied = true;
    }

    // allow the called to skip the codename.
    if (codename != nullptr) {
        ALOGI("active codename %s, to match %s", myCodeName.c_str(), codename);
        if (myCodeName == codename) {
            satisfied = true;
        }
    }

    return satisfied;
}
