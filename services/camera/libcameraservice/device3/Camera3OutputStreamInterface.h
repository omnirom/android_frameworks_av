/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_SERVERS_CAMERA3_OUTPUT_STREAM_INTERFACE_H
#define ANDROID_SERVERS_CAMERA3_OUTPUT_STREAM_INTERFACE_H

#include "Camera3StreamInterface.h"
#include <utils/KeyedVector.h>

namespace android {

namespace camera3 {

/**
 * An interface for managing a single stream of output data from the camera
 * device.
 */
class Camera3OutputStreamInterface : public virtual Camera3StreamInterface {
  public:
    /**
     * Set the transform on the output stream; one of the
     * HAL_TRANSFORM_* / NATIVE_WINDOW_TRANSFORM_* constants.
     */
    virtual status_t setTransform(int transform, bool mayChangeMirror) = 0;

    /**
     * Return if this output stream is for video encoding.
     */
    virtual bool isVideoStream() const = 0;

    /**
     * Return if the consumer configuration of this stream is deferred.
     */
    virtual bool isConsumerConfigurationDeferred(size_t surface_id = 0) const = 0;

    /**
     * Set the consumer surfaces to the output stream.
     */
    virtual status_t setConsumers(const std::vector<sp<Surface>>& consumers) = 0;

    /**
     * Detach an unused buffer from the stream.
     *
     * buffer must be non-null; fenceFd may null, and if it is non-null, but
     * there is no valid fence associated with the detached buffer, it will be
     * set to -1.
     *
     */
    virtual status_t detachBuffer(sp<GraphicBuffer>* buffer, int* fenceFd) = 0;

    /**
     * Query the surface id.
     */
    virtual ssize_t getSurfaceId(const sp<Surface> &surface) = 0;

    /**
     * Query the unique surface IDs of current surfaceIds.
     * When passing unique surface IDs in returnBuffer(), if the
     * surfaceId has been removed from the stream, the output corresponding to
     * the unique surface ID will be ignored and not delivered to client.
     *
     * Return INVALID_OPERATION if and only if the stream does not support
     * surface sharing.
     */
    virtual status_t getUniqueSurfaceIds(const std::vector<size_t>& surfaceIds,
            /*out*/std::vector<size_t>* outUniqueIds) = 0;

    /**
     * Update the stream output surfaces.
     */
    virtual status_t updateStream(const std::vector<sp<Surface>> &outputSurfaces,
            const std::vector<OutputStreamInfo> &outputInfo,
            const std::vector<size_t> &removedSurfaceIds,
            KeyedVector<sp<Surface>, size_t> *outputMap/*out*/) = 0;

    /**
     * Drop buffers if dropping is true. If dropping is false, do not drop buffers.
     */
    virtual status_t dropBuffers(bool /*dropping*/) = 0;

    /**
     * Query the physical camera id for the output stream.
     */
    virtual const String8& getPhysicalCameraId() const = 0;

    /**
     * Set the batch size for buffer operations. The output stream will request
     * buffers from buffer queue on a batch basis. Currently only video streams
     * are allowed to set the batch size. Also if the stream is managed by
     * buffer manager (Surface group in Java API) then batching is also not
     * supported. Changing batch size on the fly while there is already batched
     * buffers in the stream is also not supported.
     * If the batch size is larger than the max dequeue count set
     * by the camera HAL, the batch size will be set to the max dequeue count
     * instead.
     */
    virtual status_t setBatchSize(size_t batchSize = 1) = 0;

    /**
     * Notify the output stream that the minimum frame duration has changed, or
     * frame rate has switched between variable and fixed.
     *
     * The minimum frame duration is calculated based on the upper bound of
     * AE_TARGET_FPS_RANGE in the capture request.
     */
    virtual void onMinDurationChanged(nsecs_t duration, bool fixedFps) = 0;
};

// Helper class to organize a synchronized mapping of stream IDs to stream instances
class StreamSet {
  public:
    status_t add(int streamId, sp<camera3::Camera3OutputStreamInterface>);
    ssize_t remove(int streamId);
    sp<camera3::Camera3OutputStreamInterface> get(int streamId);
    // get by (underlying) vector index
    sp<camera3::Camera3OutputStreamInterface> operator[] (size_t index);
    size_t size() const;
    std::vector<int> getStreamIds();
    void clear();

    StreamSet() {};
    StreamSet(const StreamSet& other);

  private:
    mutable std::mutex mLock;
    KeyedVector<int, sp<camera3::Camera3OutputStreamInterface>> mData;
};

} // namespace camera3

} // namespace android

#endif
