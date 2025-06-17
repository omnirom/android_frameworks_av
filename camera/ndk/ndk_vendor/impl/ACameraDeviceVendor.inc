/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "ACameraCaptureSession.h"
#include "ACameraMetadata.h"
#include "ACaptureRequest.h"
#include "utils.h"
#include <CameraMetadata.h>
#include <inttypes.h>
#include <ndk_vendor/impl/ACameraDevice.h>
#include <vector>

using namespace android;

namespace android {
namespace acam {

template<class T>
camera_status_t CameraDevice::captureLocked(
        sp<ACameraCaptureSession> session,
        /*optional*/T* cbs,
        int numRequests,
        ACaptureRequest** requests,
        /*optional*/int* captureSequenceId) {
    return submitRequestsLocked(
            session, cbs, numRequests, requests, captureSequenceId, /*isRepeating*/false);
}

template<class T>
camera_status_t CameraDevice::setRepeatingRequestsLocked(
        sp<ACameraCaptureSession> session,
        /*optional*/T* cbs,
        int numRequests,
        ACaptureRequest** requests,
        /*optional*/int* captureSequenceId) {
    return submitRequestsLocked(
            session, cbs, numRequests, requests, captureSequenceId, /*isRepeating*/true);
}

template<class T>
camera_status_t CameraDevice::submitRequestsLocked(
        sp<ACameraCaptureSession> session,
        /*optional*/T* cbs, int numRequests,
        ACaptureRequest** requests,
        /*out*/int* captureSequenceId,
        bool isRepeating) {
    camera_status_t ret = checkCameraClosedOrErrorLocked();
    if (ret != ACAMERA_OK) {
        ALOGE("Camera %s submit capture request failed! ret %d", getId(), ret);
        return ret;
    }

    // Form two vectors of capture request, one for internal tracking

    std::vector<::aidl::android::frameworks::cameraservice::device::CaptureRequest> requestList;
    std::vector<sp<CaptureRequest>> requestsV;
    requestsV.reserve(numRequests);
    for (int i = 0; i < numRequests; i++) {
        sp<CaptureRequest> req;
        ret = allocateCaptureRequestLocked(requests[i], req);
        // We need to call this method since after submitRequestList is called,
        // the request metadata queue might have removed the capture request
        // metadata. Therefore we simply add the metadata to its wrapper class,
        // so that it can be retrieved later.
        addRequestSettingsMetadata(requests[i], req);
        if (ret != ACAMERA_OK) {
            ALOGE("Convert capture request to internal format failure! ret %d", ret);
            return ret;
        }
        if (req->mCaptureRequest.streamAndWindowIds.size() == 0) {
            ALOGE("Capture request without output target cannot be submitted!");
            return ACAMERA_ERROR_INVALID_PARAMETER;
        }
        requestList.push_back(utils::convertToAidl(req.get()));
        requestsV.push_back(req);
    }
    if (isRepeating) {
        ret = stopRepeatingLocked();
        if (ret != ACAMERA_OK) {
            ALOGE("Camera %s stop repeating failed! ret %d", getId(), ret);
            return ret;
        }
    }

    SubmitInfo info;
    Status status;
    ndk::ScopedAStatus remoteRet = mRemote->submitRequestList(requestList, isRepeating, &info);
        if (!remoteRet.isOk()) {
            if (remoteRet.getExceptionCode() == EX_SERVICE_SPECIFIC) {
            Status errStatus = static_cast<Status>(remoteRet.getServiceSpecificError());
            ALOGE("%s: submitRequestList call failed: %s",
                  __FUNCTION__, toString(errStatus).c_str());
            return utils::convertFromAidl(errStatus);
        } else {
            ALOGE("%s: Transaction error for submitRequestList call: %d", __FUNCTION__,
                  remoteRet.getExceptionCode());
            return ACAMERA_ERROR_UNKNOWN;
        }
    }

    int32_t sequenceId = info.requestId;
    int64_t lastFrameNumber = info.lastFrameNumber;
    if (sequenceId < 0) {
        ALOGE("Camera %s submit request remote failure: ret %d", getId(), sequenceId);
        return ACAMERA_ERROR_UNKNOWN;
    }

    CallbackHolder cbHolder(session, requestsV, isRepeating, cbs);
    mSequenceCallbackMap.insert(std::make_pair(sequenceId, cbHolder));
    if (isRepeating) {
        // stopRepeating above should have cleanup repeating sequence id
        if (mRepeatingSequenceId != REQUEST_ID_NONE) {
            setCameraDeviceErrorLocked(ACAMERA_ERROR_CAMERA_DEVICE);
            return ACAMERA_ERROR_CAMERA_DEVICE;
        }
        mRepeatingSequenceId = sequenceId;
    } else {
        mSequenceLastFrameNumberMap.insert(std::make_pair(sequenceId, lastFrameNumber));
    }

    if (mIdle) {
        sp<AMessage> msg = new AMessage(kWhatSessionStateCb, mHandler);
        msg->setPointer(kContextKey, session->mUserSessionCallback.context);
        msg->setObject(kSessionSpKey, session);
        msg->setPointer(kCallbackFpKey, (void*) session->mUserSessionCallback.onActive);
        postSessionMsgAndCleanup(msg);
    }
    mIdle = false;
    mBusySession = session;

    if (captureSequenceId) {
        *captureSequenceId = sequenceId;
    }
    return ACAMERA_OK;
}

template<class T>
camera_status_t CameraDevice::startStreamingLocked(ACameraCaptureSession* session,
        /*optional*/T* callbacks, int numOutputWindows,
        ANativeWindow** windows, /*optional*/int* captureSequenceId) {
    camera_status_t ret = checkCameraClosedOrErrorLocked();
    if (ret != ACAMERA_OK) {
        ALOGE("%s: camera is in closed or error state %d", __FUNCTION__, ret);
        return ret;
    }
    utils::AidlCameraMetadata aidlMetadata;
    ndk::ScopedAStatus remoteRet = mRemote->createDefaultRequest(utils::TemplateId::PREVIEW,
            &aidlMetadata);
    if (!remoteRet.isOk()) {
        if (remoteRet.getExceptionCode() == EX_SERVICE_SPECIFIC) {
            Status errStatus = static_cast<Status>(remoteRet.getServiceSpecificError());
            ALOGE("%s: submitRequestList call failed: %s",
                  __FUNCTION__, toString(errStatus).c_str());
            return utils::convertFromAidl(errStatus);
        } else {
            ALOGE("%s: Transaction error for submitRequestList call: %d", __FUNCTION__,
                  remoteRet.getExceptionCode());
            return ACAMERA_ERROR_UNKNOWN;
        }
    }
    camera_metadata_t* rawPreviewRequest;
    utils::cloneFromAidl(aidlMetadata, &rawPreviewRequest);
    // ToDo: Check if the memory allocation can be freed automatically using either default_delete
    // or ScopedAResource.
    mPreviewRequest = new ACaptureRequest();
    mPreviewRequest->settings = new ACameraMetadata(rawPreviewRequest,
            ACameraMetadata::ACM_REQUEST);
    mPreviewRequest->targets  = new ACameraOutputTargets();
    for (int i = 0; i < numOutputWindows ; i++) {
        ACameraOutputTarget* outputTarget = nullptr;
        ret = ACameraOutputTarget_create(windows[i], &outputTarget);
        if (ret != ACAMERA_OK) {
            ALOGE("%s: error while ACameraOutputTarget_create %d", __FUNCTION__, ret);
            return ret;
        }
        ret = ACaptureRequest_addTarget(mPreviewRequest, outputTarget);
        if (ret != ACAMERA_OK) {
            ALOGE("%s: error while ACaptureRequest_addTarget %d", __FUNCTION__, ret);
            return ret;
        }
        mPreviewRequestOutputs.push_back(outputTarget);
    }

    std::vector<sp<CaptureRequest>> requestsV;
    sp<CaptureRequest> req;
    ret = allocateCaptureRequestLocked(mPreviewRequest, req);
    // We need to call this method since after submitRequestList is called,
    // the request metadata queue might have removed the capture request
    // metadata. Therefore we simply add the metadata to its wrapper class,
    // so that it can be retrieved later.
    addRequestSettingsMetadata(mPreviewRequest, req);
    if (ret != ACAMERA_OK) {
        ALOGE("Convert capture request to internal format failure! ret %d", ret);
        return ret;
    }
    if (req->mCaptureRequest.streamAndWindowIds.size() == 0) {
        ALOGE("Capture request without output target cannot be submitted!");
        return ACAMERA_ERROR_INVALID_PARAMETER;
    }
    requestsV.push_back(req);
    // In shared session mode, if there are other active clients streaming then
    // stoprepeating does not actually send request to HAL to cancel the request.
    // Cameraservice will use this call to remove this client surfaces provided in its
    // previous streaming request. If this is the only client for the shared camera device
    // then camerservice will ask HAL to cancel the previous repeating request.
    ret = stopRepeatingLocked();
    if (ret != ACAMERA_OK) {
        ALOGE("Camera %s stop repeating failed! ret %d", getId(), ret);
        return ret;
    }
    SubmitInfo info;
    std::vector<int> streamIds;
    std::vector<int> surfaceIds;
    for (const auto& streamAndWindowId : req->mCaptureRequest.streamAndWindowIds) {
        streamIds.push_back(streamAndWindowId.streamId);
        surfaceIds.push_back(streamAndWindowId.windowId);
    }
    remoteRet = mRemote->startStreaming(streamIds, surfaceIds, &info);
    if (!remoteRet.isOk()) {
        if (remoteRet.getExceptionCode() == EX_SERVICE_SPECIFIC) {
            Status errStatus = static_cast<Status>(remoteRet.getServiceSpecificError());
            ALOGE("%s: startStreaming call failed: %s",
                  __FUNCTION__, toString(errStatus).c_str());
            return utils::convertFromAidl(errStatus);
        } else {
            ALOGE("%s: Transaction error for startStreaming call: %d", __FUNCTION__,
                  remoteRet.getExceptionCode());
            return ACAMERA_ERROR_UNKNOWN;
        }
    }

    int32_t sequenceId = info.requestId;
    int64_t lastFrameNumber = info.lastFrameNumber;
    if (sequenceId < 0) {
        ALOGE("Camera %s submit request remote failure: ret %d", getId(), sequenceId);
        return ACAMERA_ERROR_UNKNOWN;
    }
    CallbackHolder cbHolder(session, requestsV, true, callbacks);
    mSequenceCallbackMap.insert(std::make_pair(sequenceId, cbHolder));
    // stopRepeating above should have cleanup repeating sequence id
    if (mRepeatingSequenceId != REQUEST_ID_NONE) {
        setCameraDeviceErrorLocked(ACAMERA_ERROR_CAMERA_DEVICE);
        return ACAMERA_ERROR_CAMERA_DEVICE;
    }
    mRepeatingSequenceId = sequenceId;

    if (mIdle) {
        sp<AMessage> msg = new AMessage(kWhatSessionStateCb, mHandler);
        msg->setPointer(kContextKey, session->mUserSessionCallback.context);
        msg->setObject(kSessionSpKey, session);
        msg->setPointer(kCallbackFpKey, (void*) session->mUserSessionCallback.onActive);
        postSessionMsgAndCleanup(msg);
    }
    mIdle = false;
    mBusySession = session;

    if (captureSequenceId) {
        *captureSequenceId = sequenceId;
    }
    return ACAMERA_OK;
}

} // namespace acam
} // namespace android
