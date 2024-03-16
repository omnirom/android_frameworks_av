/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <any>
#include <functional>
#include <mutex>

#include <media/AudioSystem.h>
#include <utils/RefBase.h>

namespace android::audioflinger {

class SyncEvent;
using SyncEventCallback = std::function<void(const wp<SyncEvent>& event)>;

class SyncEvent : public RefBase {
public:
    SyncEvent(AudioSystem::sync_event_t type,
              audio_session_t triggerSession,
              audio_session_t listenerSession,
              const SyncEventCallback& callBack,
              const std::any& cookie)
    : mType(type), mTriggerSession(triggerSession), mListenerSession(listenerSession),
      mCookie(cookie), mCallback(callBack)
    {}

    void trigger() {
        std::lock_guard l(mLock);
        if (mCallback) mCallback(wp<SyncEvent>::fromExisting(this));
    }

    bool isCancelled() const {
        std::lock_guard l(mLock);
        return mCallback == nullptr;
    }

    void cancel() {
        std::lock_guard l(mLock);
        mCallback = nullptr;
    }

    AudioSystem::sync_event_t type() const { return mType; }
    audio_session_t triggerSession() const { return mTriggerSession; }
    audio_session_t listenerSession() const { return mListenerSession; }
    const std::any& cookie() const { return mCookie; }

private:
      const AudioSystem::sync_event_t mType;
      const audio_session_t mTriggerSession;
      const audio_session_t mListenerSession;
      const std::any mCookie;
      mutable std::mutex mLock;
      SyncEventCallback mCallback GUARDED_BY(mLock);
};

} // namespace android::audioflinger
