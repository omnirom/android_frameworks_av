/*
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

#include <deque>
#include <mutex>

#include "Runnable.h"
#include "jthread.h"

namespace android::mediautils {

/**
 * A C++ implementation similar to a Java executor, which manages a thread which runs enqueued
 * runnable tasks in queue order. Spawns thread on construction and joins destruction
 */
class SingleThreadExecutor {
  public:
    SingleThreadExecutor() : thread_([this](stop_token stok) { run(stok); }) {}

    ~SingleThreadExecutor() { shutdown(/* dropTasks= */ true); }

    void enqueue(Runnable r) {
        if (!r) {
            return;
        } else {
            std::lock_guard l{mutex_};
            if (thread_.stop_requested()) return;
            task_list_.push_back(std::move(r));
        }
        cv_.notify_one();
    }

    /**
     * Request thread termination, optionally dropping any enqueued tasks.
     * Note: does not join thread in this method and no task cancellation.
     */
    void shutdown(bool dropTasks = false) {
        {
            std::lock_guard l{mutex_};
            if (thread_.stop_requested()) return;
            if (dropTasks) {
                task_list_.clear();
            }
            thread_.request_stop();  // fancy atomic bool, so no deadlock risk
        }
        // This condition variable notification is necessary since the stop_callback functionality
        // of stop_token is not fully implemented
        cv_.notify_one();
    }


  private:
    void run(stop_token stok) {
        std::unique_lock l{mutex_};
        while (true) {
            cv_.wait_for(l, std::chrono::seconds(3), [this, stok]() {
                return !task_list_.empty() || stok.stop_requested();
            });
            if (!task_list_.empty()) {
                Runnable r {std::move(task_list_.front())};
                task_list_.pop_front();
                l.unlock();
                r();
                l.lock();
            } else if (stok.stop_requested()) {
                break;
            } // else cv timeout
        }
    }

    std::condition_variable cv_;
    std::mutex mutex_;
    std::deque<Runnable> task_list_;
    jthread thread_;
};
}  // namespace android::mediautils
