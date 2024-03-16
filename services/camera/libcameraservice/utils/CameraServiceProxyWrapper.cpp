/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "CameraServiceProxyWrapper"
#define ATRACE_TAG ATRACE_TAG_CAMERA
//#define LOG_NDEBUG 0

#include <inttypes.h>
#include <utils/Log.h>
#include <utils/String16.h>
#include <camera/StringUtils.h>
#include <binder/IServiceManager.h>

#include "CameraServiceProxyWrapper.h"

namespace android {

using hardware::CameraExtensionSessionStats;
using hardware::CameraSessionStats;
using hardware::ICameraServiceProxy;

namespace {
// Sentinel value to be returned when extension session with a stale or invalid key is reported.
const std::string POISON_EXT_STATS_KEY("poisoned_stats");
} // anonymous namespace

/**
 * CameraSessionStatsWrapper functions
 */

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::updateProxyDeviceState(
        sp<hardware::ICameraServiceProxy>& proxyBinder) {
    if (proxyBinder == nullptr) return;
    proxyBinder->notifyCameraState(mSessionStats);
}

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::onOpen(
        sp<hardware::ICameraServiceProxy>& proxyBinder) {
    Mutex::Autolock l(mLock);
    updateProxyDeviceState(proxyBinder);
}

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::onClose(
    sp<hardware::ICameraServiceProxy>& proxyBinder, int32_t latencyMs,
    bool deviceError) {
    Mutex::Autolock l(mLock);

    mSessionStats.mNewCameraState = CameraSessionStats::CAMERA_STATE_CLOSED;
    mSessionStats.mLatencyMs = latencyMs;
    mSessionStats.mDeviceError = deviceError;
    mSessionStats.mSessionIndex = 0;
    updateProxyDeviceState(proxyBinder);
}

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::onStreamConfigured(
        int operatingMode, bool internalReconfig, int32_t latencyMs) {
    Mutex::Autolock l(mLock);

    if (internalReconfig) {
        mSessionStats.mInternalReconfigure++;
    } else {
        mSessionStats.mLatencyMs = latencyMs;
        mSessionStats.mSessionType = operatingMode;
    }
}

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::onActive(
    sp<hardware::ICameraServiceProxy>& proxyBinder, float maxPreviewFps) {
    Mutex::Autolock l(mLock);

    mSessionStats.mNewCameraState = CameraSessionStats::CAMERA_STATE_ACTIVE;
    mSessionStats.mMaxPreviewFps = maxPreviewFps;
    mSessionStats.mSessionIndex++;
    updateProxyDeviceState(proxyBinder);

    // Reset mCreationDuration to -1 to distinguish between 1st session
    // after configuration, and all other sessions after configuration.
    mSessionStats.mLatencyMs = -1;
}

void CameraServiceProxyWrapper::CameraSessionStatsWrapper::onIdle(
        sp<hardware::ICameraServiceProxy>& proxyBinder,
        int64_t requestCount, int64_t resultErrorCount, bool deviceError,
        const std::string& userTag, int32_t videoStabilizationMode, bool usedUltraWide,
        bool usedZoomOverride, const std::vector<hardware::CameraStreamStats>& streamStats) {
    Mutex::Autolock l(mLock);

    mSessionStats.mNewCameraState = CameraSessionStats::CAMERA_STATE_IDLE;
    mSessionStats.mRequestCount = requestCount;
    mSessionStats.mResultErrorCount = resultErrorCount;
    mSessionStats.mDeviceError = deviceError;
    mSessionStats.mUserTag = userTag;
    mSessionStats.mVideoStabilizationMode = videoStabilizationMode;
    mSessionStats.mUsedUltraWide = usedUltraWide;
    mSessionStats.mUsedZoomOverride = usedZoomOverride;
    mSessionStats.mStreamStats = streamStats;

    updateProxyDeviceState(proxyBinder);

    mSessionStats.mInternalReconfigure = 0;
    mSessionStats.mStreamStats.clear();
    mSessionStats.mCameraExtensionSessionStats = {};
}

int64_t CameraServiceProxyWrapper::CameraSessionStatsWrapper::getLogId() {
    Mutex::Autolock l(mLock);
    return mSessionStats.mLogId;
}

std::string CameraServiceProxyWrapper::CameraSessionStatsWrapper::updateExtensionSessionStats(
        const hardware::CameraExtensionSessionStats& extStats) {
    Mutex::Autolock l(mLock);
    CameraExtensionSessionStats& currStats = mSessionStats.mCameraExtensionSessionStats;
    if (currStats.key != extStats.key) {
        // Mismatched keys. Extensions stats likely reported for a closed session
        ALOGW("%s: mismatched extensions stats key: current='%s' reported='%s'. Dropping stats.",
              __FUNCTION__, toStdString(currStats.key).c_str(), toStdString(extStats.key).c_str());
        return POISON_EXT_STATS_KEY; // return poisoned key to so future calls are
                                     // definitely dropped.
    }

    // Matching keys...
    if (currStats.key.size()) {
        // non-empty matching keys. overwrite.
        ALOGV("%s: Overwriting extension session stats: %s", __FUNCTION__,
              extStats.toString().c_str());
        currStats = extStats;
        return toStdString(currStats.key);
    }

    // Matching empty keys...
    if (mSessionStats.mClientName != toStdString(extStats.clientName)) {
        ALOGW("%s: extension stats reported for unexpected package: current='%s' reported='%s'. "
              "Dropping stats.", __FUNCTION__,
              mSessionStats.mClientName.c_str(),
              toStdString(extStats.clientName).c_str());
        return POISON_EXT_STATS_KEY;
    }

    // Matching empty keys for the current client...
    if (mSessionStats.mNewCameraState == CameraSessionStats::CAMERA_STATE_OPEN ||
        mSessionStats.mNewCameraState == CameraSessionStats::CAMERA_STATE_IDLE) {
        // Camera is open, but not active. It is possible that the active callback hasn't
        // occurred yet. Keep the stats, but don't associate it with any session.
        ALOGV("%s: extension stat reported for an open, but not active camera. "
              "Saving stats, but not generating key.", __FUNCTION__);
        currStats = extStats;
        return {}; // Subsequent calls will handle setting the correct key.
    }

    if (mSessionStats.mNewCameraState == CameraSessionStats::CAMERA_STATE_ACTIVE) {
        // camera is active. First call for the session!
        currStats = extStats;

        // Generate a new key from logId and sessionIndex.
        std::ostringstream key;
        key << mSessionStats.mSessionIndex << '/' << mSessionStats.mLogId;
        currStats.key = String16(key.str().c_str());
        ALOGV("%s: New extension session stats: %s", __FUNCTION__, currStats.toString().c_str());
        return toStdString(currStats.key);
    }

    // Camera is closed. Probably a stale call.
    ALOGW("%s: extension stats reported for closed camera id '%s'. Dropping stats.",
          __FUNCTION__, mSessionStats.mCameraId.c_str());
    return {};
}

/**
 * CameraServiceProxyWrapper functions
 */

sp<ICameraServiceProxy> CameraServiceProxyWrapper::getCameraServiceProxy() {
#ifndef __BRILLO__
    Mutex::Autolock al(mProxyMutex);
    if (mCameraServiceProxy == nullptr) {
        mCameraServiceProxy = getDefaultCameraServiceProxy();
    }
#endif
    return mCameraServiceProxy;
}

sp<hardware::ICameraServiceProxy> CameraServiceProxyWrapper::getDefaultCameraServiceProxy() {
#ifndef __BRILLO__
    sp<IServiceManager> sm = defaultServiceManager();
    // Use checkService because cameraserver normally starts before the
    // system server and the proxy service. So the long timeout that getService
    // has before giving up is inappropriate.
    sp<IBinder> binder = sm->checkService(String16("media.camera.proxy"));
    if (binder != nullptr) {
        return interface_cast<ICameraServiceProxy>(binder);
    }
#endif
    return nullptr;
}

void CameraServiceProxyWrapper::pingCameraServiceProxy() {
    sp<ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    if (proxyBinder == nullptr) return;
    proxyBinder->pingForUserUpdate();
}

int CameraServiceProxyWrapper::getRotateAndCropOverride(const std::string &packageName,
        int lensFacing, int userId) {
    sp<ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    if (proxyBinder == nullptr) return true;
    int ret = 0;
    auto status = proxyBinder->getRotateAndCropOverride(packageName, lensFacing,
            userId, &ret);
    if (!status.isOk()) {
        ALOGE("%s: Failed during top activity orientation query: %s", __FUNCTION__,
                status.exceptionMessage().c_str());
    }

    return ret;
}

int CameraServiceProxyWrapper::getAutoframingOverride(const std::string& packageName) {
    sp<ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    if (proxyBinder == nullptr) {
        return ANDROID_CONTROL_AUTOFRAMING_OFF;
    }
    int ret = 0;
    auto status = proxyBinder->getAutoframingOverride(packageName, &ret);
    if (!status.isOk()) {
        ALOGE("%s: Failed during autoframing override query: %s", __FUNCTION__,
                status.exceptionMessage().c_str());
    }

    return ret;
}

void CameraServiceProxyWrapper::logStreamConfigured(const std::string& id,
        int operatingMode, bool internalConfig, int32_t latencyMs) {
    std::shared_ptr<CameraSessionStatsWrapper> sessionStats;
    {
        Mutex::Autolock l(mLock);
        if (mSessionStatsMap.count(id) == 0) {
            ALOGE("%s: SessionStatsMap should contain camera %s",
                    __FUNCTION__, id.c_str());
            return;
        }
        sessionStats = mSessionStatsMap[id];
    }

    ALOGV("%s: id %s, operatingMode %d, internalConfig %d, latencyMs %d",
            __FUNCTION__, id.c_str(), operatingMode, internalConfig, latencyMs);
    sessionStats->onStreamConfigured(operatingMode, internalConfig, latencyMs);
}

void CameraServiceProxyWrapper::logActive(const std::string& id, float maxPreviewFps) {
    std::shared_ptr<CameraSessionStatsWrapper> sessionStats;
    {
        Mutex::Autolock l(mLock);
        if (mSessionStatsMap.count(id) == 0) {
            ALOGE("%s: SessionStatsMap should contain camera %s when logActive is called",
                    __FUNCTION__, id.c_str());
            return;
        }
        sessionStats = mSessionStatsMap[id];
    }

    ALOGV("%s: id %s", __FUNCTION__, id.c_str());
    sp<hardware::ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    sessionStats->onActive(proxyBinder, maxPreviewFps);
}

void CameraServiceProxyWrapper::logIdle(const std::string& id,
        int64_t requestCount, int64_t resultErrorCount, bool deviceError,
        const std::string& userTag, int32_t videoStabilizationMode, bool usedUltraWide,
        bool usedZoomOverride, const std::vector<hardware::CameraStreamStats>& streamStats) {
    std::shared_ptr<CameraSessionStatsWrapper> sessionStats;
    {
        Mutex::Autolock l(mLock);
        if (mSessionStatsMap.count(id) == 0) {
            ALOGE("%s: SessionStatsMap should contain camera %s when logIdle is called",
                __FUNCTION__, id.c_str());
            return;
        }
        sessionStats = mSessionStatsMap[id];
    }

    ALOGV("%s: id %s, requestCount %" PRId64 ", resultErrorCount %" PRId64 ", deviceError %d"
            ", userTag %s, videoStabilizationMode %d", __FUNCTION__, id.c_str(), requestCount,
            resultErrorCount, deviceError, userTag.c_str(), videoStabilizationMode);
    for (size_t i = 0; i < streamStats.size(); i++) {
        ALOGV("%s: streamStats[%zu]: w %d h %d, requestedCount %" PRId64 ", dropCount %"
                PRId64 ", startTimeMs %d" ,
                __FUNCTION__, i, streamStats[i].mWidth, streamStats[i].mHeight,
                streamStats[i].mRequestCount, streamStats[i].mErrorCount,
                streamStats[i].mStartLatencyMs);
    }

    sp<hardware::ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    sessionStats->onIdle(proxyBinder, requestCount, resultErrorCount, deviceError, userTag,
            videoStabilizationMode, usedUltraWide, usedZoomOverride, streamStats);
}

void CameraServiceProxyWrapper::logOpen(const std::string& id, int facing,
            const std::string& clientPackageName, int effectiveApiLevel, bool isNdk,
            int32_t latencyMs) {
    std::shared_ptr<CameraSessionStatsWrapper> sessionStats;
    {
        Mutex::Autolock l(mLock);
        if (mSessionStatsMap.count(id) > 0) {
            ALOGE("%s: SessionStatsMap shouldn't contain camera %s",
                    __FUNCTION__, id.c_str());
            return;
        }

        int apiLevel = CameraSessionStats::CAMERA_API_LEVEL_1;
        if (effectiveApiLevel == 2) {
            apiLevel = CameraSessionStats::CAMERA_API_LEVEL_2;
        }

        // Generate a new log ID for open events
        int64_t logId = generateLogId(mRandomDevice);

        sessionStats = std::make_shared<CameraSessionStatsWrapper>(
                id, facing, CameraSessionStats::CAMERA_STATE_OPEN, clientPackageName,
                apiLevel, isNdk, latencyMs, logId);
        mSessionStatsMap.emplace(id, sessionStats);
        ALOGV("%s: Adding id %s", __FUNCTION__, id.c_str());
    }

    ALOGV("%s: id %s, facing %d, effectiveApiLevel %d, isNdk %d, latencyMs %d",
            __FUNCTION__, id.c_str(), facing, effectiveApiLevel, isNdk, latencyMs);
    sp<hardware::ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    sessionStats->onOpen(proxyBinder);
}

void CameraServiceProxyWrapper::logClose(const std::string& id, int32_t latencyMs,
        bool deviceError) {
    std::shared_ptr<CameraSessionStatsWrapper> sessionStats;
    {
        Mutex::Autolock l(mLock);
        if (mSessionStatsMap.count(id) == 0) {
            ALOGE("%s: SessionStatsMap should contain camera %s before it's closed",
                    __FUNCTION__, id.c_str());
            return;
        }

        sessionStats = mSessionStatsMap[id];
        if (sessionStats == nullptr) {
            ALOGE("%s: SessionStatsMap should contain camera %s",
                    __FUNCTION__, id.c_str());
            return;
        }

        mSessionStatsMap.erase(id);
        ALOGV("%s: Erasing id %s, deviceError %d", __FUNCTION__, id.c_str(), deviceError);
    }

    ALOGV("%s: id %s, latencyMs %d, deviceError %d", __FUNCTION__,
            id.c_str(), latencyMs, deviceError);
    sp<hardware::ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    sessionStats->onClose(proxyBinder, latencyMs, deviceError);
}

bool CameraServiceProxyWrapper::isCameraDisabled(int userId) {
    sp<ICameraServiceProxy> proxyBinder = getCameraServiceProxy();
    if (proxyBinder == nullptr) return true;
    bool ret = false;
    auto status = proxyBinder->isCameraDisabled(userId, &ret);
    if (!status.isOk()) {
        ALOGE("%s: Failed during camera disabled query: %s", __FUNCTION__,
                status.exceptionMessage().c_str());
    }
    return ret;
}

int64_t CameraServiceProxyWrapper::getCurrentLogIdForCamera(const std::string& cameraId) {
    std::shared_ptr<CameraSessionStatsWrapper> stats;
    {
        Mutex::Autolock _l(mLock);
        if (mSessionStatsMap.count(cameraId) == 0) {
            ALOGE("%s: SessionStatsMap should contain camera %s before asking for its logging ID.",
                  __FUNCTION__, cameraId.c_str());
            return 0;
        }

        stats = mSessionStatsMap[cameraId];
    }
    return stats->getLogId();
}

int64_t CameraServiceProxyWrapper::generateLogId(std::random_device& randomDevice) {
    int64_t ret = 0;
    do {
        // std::random_device generates 32 bits per call, so we call it twice
        ret = randomDevice();
        ret = ret << 32;
        ret = ret | randomDevice();
    } while (ret == 0); // 0 is not a valid identifier

    return ret;
}

std::string CameraServiceProxyWrapper::updateExtensionStats(
        const hardware::CameraExtensionSessionStats& extStats) {
    std::shared_ptr<CameraSessionStatsWrapper> stats;
    std::string cameraId = toStdString(extStats.cameraId);
    {
        Mutex::Autolock _l(mLock);
        if (mSessionStatsMap.count(cameraId) == 0) {
            ALOGE("%s CameraExtensionSessionStats reported for camera id that isn't open: %s",
                  __FUNCTION__, cameraId.c_str());
            return {};
        }

        stats = mSessionStatsMap[cameraId];
        return stats->updateExtensionSessionStats(extStats);
    }
}

}  // namespace android
