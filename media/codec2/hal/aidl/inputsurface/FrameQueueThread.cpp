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

//#define LOG_NDEBUG 0
#define LOG_TAG "Codec2-InputSurfaceQueue"

#include <sys/types.h>

#include <chrono>

#include <android-base/logging.h>
#include <codec2/aidl/BufferTypes.h>
#include <codec2/aidl/inputsurface/FrameQueueThread.h>
#include <media/stagefright/foundation/ColorUtils.h>
#include <ui/Fence.h>
#include <utils/AndroidThreads.h>

#include <Codec2Mapper.h>

namespace aidl::android::hardware::media::c2::implementation {

FrameQueueThread::FrameQueueThread(const std::shared_ptr<IInputSink> &sink)
        : mSink{sink} {
    mThread = std::thread(&FrameQueueThread::run, this);
}

FrameQueueThread::~FrameQueueThread() {
    {
        std::unique_lock<std::mutex> l(mLock);
        mDone = true;
        mCv.notify_all();
    }
    if (mThread.joinable()) {
        mThread.join();
    }
}

void FrameQueueThread::queue(std::unique_ptr<C2Work> &&work, int fenceFd) {
    {
        std::unique_lock<std::mutex> l(mLock);
        mItems.emplace_back(std::move(work), fenceFd);
        if (!mConfigUpdate.empty()) {
            mItems.back().updateConfig(mConfigUpdate);
        }
    }
    mCv.notify_all();
}

void FrameQueueThread::setDataspace(android_dataspace dataspace) {
    std::unique_lock<std::mutex> l(mLock);
    ::android::ColorUtils::convertDataSpaceToV0(dataspace);
    mConfigUpdate.emplace_back(new C2StreamDataSpaceInfo::input(0u, dataspace));
    int32_t standard;
    int32_t transfer;
    int32_t range;
    ::android::ColorUtils::getColorConfigFromDataSpace(dataspace, &range, &standard, &transfer);
    std::unique_ptr<C2StreamColorAspectsInfo::input> colorAspects =
        std::make_unique<C2StreamColorAspectsInfo::input>(0u);
    if (::android::C2Mapper::map(standard, &colorAspects->primaries, &colorAspects->matrix)
            && ::android::C2Mapper::map(transfer, &colorAspects->transfer)
            && ::android::C2Mapper::map(range, &colorAspects->range)) {
        mConfigUpdate.push_back(std::move(colorAspects));
    }
}

void FrameQueueThread::setPriority(int priority) {
    androidSetThreadPriority(gettid(), priority);
}

void FrameQueueThread::run() {
    constexpr nsecs_t kIntervalNs = nsecs_t(10) * 1000 * 1000;  // 10ms
    constexpr nsecs_t kWaitNs = kIntervalNs * 2;

    std::unique_lock<std::mutex> lock(mLock);
    while (!mDone) {
        nsecs_t nowNs = systemTime();
        nsecs_t diffNs = nowNs - mLastQueuedTimestampNs;
        if (mItems.empty() || (mLastQueuedTimestampNs != 0 && diffNs < kIntervalNs)) {
            nsecs_t waitNs = kIntervalNs;
            if (mLastQueuedTimestampNs != 0) {
                waitNs = diffNs < kIntervalNs ? kIntervalNs - diffNs : 1;
            }
            mCv.wait_for(lock, std::chrono::nanoseconds(waitNs));
            continue;
        }
        std::deque<Item> items = std::move(mItems);
        lock.unlock();
        queueItems(items);
        lock.lock();
        mLastQueuedTimestampNs = nowNs;
        mCv.wait_for(lock, std::chrono::nanoseconds(kWaitNs));
    }
}

void FrameQueueThread::queueItems(std::deque<Item> &items) {
    std::shared_ptr<IInputSink> sink = mSink.lock();
    if (!sink) {
        ALOGE("queueItems: sink is not valid");
        return;
    }

    std::list<std::unique_ptr<C2Work>> c2Items;
    std::vector<int> fenceFds;
    while (!items.empty()) {
        c2Items.push_back(std::move(items.front().work));
        fenceFds.push_back(items.front().fenceFd);
        for (const std::unique_ptr<C2Param> &param: items.front().configUpdate) {
            c2Items.back()->input.configUpdate.emplace_back(C2Param::Copy(*param));
        }
        items.pop_front();
    }
    // TODO: Pass fence if an encoder supports receiving fences
    // along with a block.
    for (int fenceFd : fenceFds) {
        ::android::sp<::android::Fence> fence(new ::android::Fence(fenceFd));
        fence->waitForever(LOG_TAG);
    }

    WorkBundle workBundle;
    if (!utils::ToAidl(&workBundle, c2Items, nullptr)) {
        ALOGE("queueItems: conversion from C2Work to workBundle failed");
        return;
    }
    sink->queue(workBundle);
}

}  // namespace aidl::android::hardware::media::c2::implementation
