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

#define LOG_TAG "Camera3-Utils"

#include "Utils.h"
#include <android-base/properties.h>
#include <com_android_internal_camera_flags.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <vendorsupport/api_level.h>

#include <camera/CameraUtils.h>

namespace android {

/**
 * Returns defaultVersion if the property is not found.
 */
int getVNDKVersionFromProp(int defaultVersion) {
    int vendorApiLevel = AVendorSupport_getVendorApiLevel();
    if (vendorApiLevel == 0) {
        // Couldn't find vendor API level, return default
        return defaultVersion;
    }

    // Vendor API level for Android V and above are of the format YYYYMM starting with 202404.
    // AVendorSupport_getSdkApiLevelOf maps them back to SDK API levels while leaving older
    // values unchanged.
    return AVendorSupport_getSdkApiLevelOf(vendorApiLevel);
}

int getVNDKVersion() {
    static int kVndkVersion = getVNDKVersionFromProp(__ANDROID_API_FUTURE__);
    return kVndkVersion;
}

int32_t getDeviceId(const CameraMetadata& cameraInfo) {
    if (!cameraInfo.exists(ANDROID_INFO_DEVICE_ID)) {
        return kDefaultDeviceId;
    }

    const auto &deviceIdEntry = cameraInfo.find(ANDROID_INFO_DEVICE_ID);
    return deviceIdEntry.data.i32[0];
}

RunThreadWithRealtimePriority::RunThreadWithRealtimePriority(int tid)
    : mTid(tid), mPreviousPolicy(sched_getscheduler(tid)) {
    auto res = sched_getparam(mTid, &mPreviousParams);
    if (res != OK) {
        ALOGE("Can't retrieve thread scheduler parameters: %s (%d)", strerror(-res), res);
        return;
    }

    struct sched_param param = {0};
    param.sched_priority = kRequestThreadPriority;

    res = sched_setscheduler(mTid, SCHED_FIFO, &param);
    if (res != OK) {
        ALOGW("Can't set realtime priority for thread: %s (%d)", strerror(-res), res);
    } else {
        ALOGD("Set real time priority for thread (tid %d)", mTid);
        mPolicyBumped = true;
    }
}

RunThreadWithRealtimePriority::~RunThreadWithRealtimePriority() {
    if (mPolicyBumped) {
        auto res = sched_setscheduler(mTid, mPreviousPolicy, &mPreviousParams);
        if (res != OK) {
            ALOGE("Can't set regular priority for thread: %s (%d)", strerror(-res), res);
        } else {
            ALOGD("Set regular priority for thread (tid %d)", mTid);
        }
    }
}

}  // namespace android
