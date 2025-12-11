/*
 *
 * Copyright (C) 2025 The Android Open Source Project
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

#pragma once

#include <condition_variable>
#include <deque>
#include <thread>

#include <aidl/android/hardware/media/c2/BnInputSink.h>
#include <utils/Timers.h>

#include <C2Config.h>
#include <C2Work.h>

namespace aidl::android::hardware::media::c2::implementation {

/**
 * This class runs a thread which receives encoder frames and queues them
 * to an encoder component. Frames queued in a specific short duration can be
 * batched for queueing to an encoder component.
 */
class FrameQueueThread {
public:
    FrameQueueThread(const std::shared_ptr<IInputSink> &sink);

    ~FrameQueueThread();

    /**
     * Queue a frame for an encoder.
     */
    void queue(std::unique_ptr<C2Work> &&work, int fenceFd);

    /**
     * Set/update a dataspace for upcoming frames.
     */
    void setDataspace(android_dataspace dataspace);

    /**
     * Set/update thread priority for the frame queueing thread.
     */
    void setPriority(int priority);

private:
    bool mDone = false;
    std::thread mThread;
    std::weak_ptr<IInputSink> mSink;

    std::mutex mLock;
    std::condition_variable mCv;
    struct Item {
        Item(std::unique_ptr<C2Work> &&w, int fd) : work(std::move(w)), fenceFd(fd) {}

        void updateConfig(std::deque<std::unique_ptr<C2Param>> &newConfig) {
            configUpdate = std::move(newConfig);
        }

        std::unique_ptr<C2Work> work;
        int fenceFd;
        std::deque<std::unique_ptr<C2Param>> configUpdate;
    };
    std::deque<Item> mItems;
    std::deque<std::unique_ptr<C2Param>> mConfigUpdate;
    nsecs_t mLastQueuedTimestampNs = 0;

private:
    void run();

    void queueItems(std::deque<Item> &items);
};

}  // namespace aidl::android::hardware::media::c2::implementation
