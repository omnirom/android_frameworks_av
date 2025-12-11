/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_MMAP_STREAM_CALLBACK_H
#define ANDROID_AUDIO_MMAP_STREAM_CALLBACK_H

#include <audio_utils/TimerQueue.h>
#include <media/AudioContainers.h>
#include <system/audio.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

namespace android {


class MmapStreamCallback : public virtual RefBase {
  public:

    /**
     * The mmap stream should be torn down because conditions that permitted its creation with
     * the requested parameters have changed and do not allow it to operate with the requested
     * constraints any more.
     * \param[in] handle handle for the client stream to tear down.
     */
    virtual void onTearDown(audio_port_handle_t handle) = 0;

    /**
     * The volume to be applied to the use case specified when opening the stream has changed
     * \param[in] volume the new target volume
     */
    virtual void onVolumeChanged(float volume) = 0;

    /**
     * The devices the stream is routed to/from has changed
     * \param[in] deviceIds a set of the device IDs of the new devices.
     */
    virtual void onRoutingChanged(const DeviceIdVector& deviceIds) = 0;

    /**
     * The sound dose computation state has changed
     * \param[in] active true if the sound dose computation is active and
     *            the client is required to use IMmapStream::reportData,
     *            false otherwise.
     */
    virtual void onSoundDoseChanged(bool active) = 0;

    /**
     * Notify the stream to wake up.
     *
     * @param handle the handle of the wake up task.
     */
    virtual void onWakeUp(android::audio_utils::TimerQueue::handle_t handle) = 0;

  protected:
    MmapStreamCallback() {}
    virtual ~MmapStreamCallback() {}
};


} // namespace android

#endif // ANDROID_AUDIO_MMAP_STREAM_CALLBACK_H
