/*
 * Copyright (C) 2013-2018 The Android Open Source Project
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

#ifndef ANDROID_SERVERS_CAMERA3_IO_STREAM_BASE_H
#define ANDROID_SERVERS_CAMERA3_IO_STREAM_BASE_H

#include <utils/RefBase.h>
#include <gui/Surface.h>

#include "Camera3Stream.h"

namespace android {

namespace camera3 {

/**
 * A base class for managing a single stream of I/O data from the camera device.
 */
class Camera3IOStreamBase :
        public Camera3Stream {
  protected:
    Camera3IOStreamBase(int id, camera_stream_type_t type,
            uint32_t width, uint32_t height, size_t maxSize, int format,
            android_dataspace dataSpace, camera_stream_rotation_t rotation,
            const String8& physicalCameraId,
            const std::unordered_set<int32_t> &sensorPixelModesUsed,
            int setId = CAMERA3_STREAM_SET_ID_INVALID, bool isMultiResolution = false,
            int64_t dynamicProfile = ANDROID_REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP_STANDARD,
            int64_t streamUseCase = ANDROID_SCALER_AVAILABLE_STREAM_USE_CASES_DEFAULT,
            bool deviceTimeBaseIsRealtime = false,
            int timestampBase = OutputConfiguration::TIMESTAMP_BASE_DEFAULT);

  public:

    virtual ~Camera3IOStreamBase();

    /**
     * Camera3Stream interface
     */

    virtual void     dump(int fd, const Vector<String16> &args) const;

    int              getMaxTotalBuffers() const { return mTotalBufferCount; }
  protected:
    size_t            mTotalBufferCount;
    // The maximum number of cached buffers allowed for this stream
    size_t            mMaxCachedBufferCount;

    // sum of input and output buffers that are currently acquired by HAL
    size_t            mHandoutTotalBufferCount;
    // number of output buffers that are currently acquired by HAL. This will be
    // Redundant when camera3 streams are no longer bidirectional streams.
    size_t            mHandoutOutputBufferCount;
    // number of cached output buffers that are currently queued in the camera
    // server but not yet queued to the buffer queue.
    size_t            mCachedOutputBufferCount;

    uint32_t          mFrameCount;
    // Last received output buffer's timestamp
    nsecs_t           mLastTimestamp;

    // The merged release fence for all returned buffers
    sp<Fence>         mCombinedFence;

    status_t         returnAnyBufferLocked(
            const camera_stream_buffer &buffer,
            nsecs_t timestamp,
            nsecs_t readoutTimestamp,
            bool output,
            int32_t transform,
            const std::vector<size_t>& surface_ids = std::vector<size_t>());

    virtual status_t returnBufferCheckedLocked(
            const camera_stream_buffer &buffer,
            nsecs_t timestamp,
            nsecs_t readoutTimestamp,
            bool output,
            int32_t transform,
            const std::vector<size_t>& surface_ids,
            /*out*/
            sp<Fence> *releaseFenceOut) = 0;

    /**
     * Internal Camera3Stream interface
     */
    virtual bool     hasOutstandingBuffersLocked() const;

    virtual size_t   getBufferCountLocked();

    virtual size_t   getHandoutOutputBufferCountLocked() const;

    virtual size_t   getHandoutInputBufferCountLocked();

    virtual size_t   getCachedOutputBufferCountLocked() const;
    virtual size_t   getMaxCachedOutputBuffersLocked() const;

    virtual status_t getEndpointUsage(uint64_t *usage) const = 0;

    status_t getBufferPreconditionCheckLocked() const;
    status_t returnBufferPreconditionCheckLocked() const;

    // State check only
    virtual status_t configureQueueLocked();
    // State checks only
    virtual status_t disconnectLocked();

    // Hand out the buffer to a native location,
    //   incrementing the internal refcount and dequeued buffer count
    void handoutBufferLocked(camera_stream_buffer &buffer,
                             buffer_handle_t *handle,
                             int acquire_fence,
                             int release_fence,
                             camera_buffer_status_t status,
                             bool output);

}; // class Camera3IOStreamBase

} // namespace camera3

} // namespace android

#endif
