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

#pragma once

#include <android-base/thread_annotations.h>
#include <com/android/internal/app/BnAppOpsCallback.h>
#include <cutils/android_filesystem_config.h>
#include <log/log.h>
#include <utils/RefBase.h>

#include <functional>

#include "media/ValidatedAttributionSourceState.h"

namespace android::media::permission {

using ValidatedAttributionSourceState =
        com::android::media::permission::ValidatedAttributionSourceState;

struct Ops {
    int attributedOp = -1;  // same as OP_NONE
    int additionalOp = -1;
};

/**
 * This session manages an ongoing data access corresponding with appops.
 *
 * This access can be temporarily stopped by appops or the data source. When access is revoked by
 * AppOps, the registered callback will be called in order to ensure that the data delivery is
 * halted. When halted by the data source, AppOps will be notified that the access ended.
 * Note, this session does not ref-count on itself. It should represent a single access, which
 * necessarily cannot nest.
 * This class is fully locked since notifications from appops are async. Public interface can be
 * slow due to binder calls.
 */
template <typename AppOpsFacade>
// Abstract interface that permits minor differences in how appops is called per client usage
    requires requires(AppOpsFacade x, const ValidatedAttributionSourceState attr) {
        { x.startAccess(attr, Ops{}) } -> std::same_as<bool>;  // true if permitted
        { x.stopAccess(attr, Ops{}) } -> std::same_as<void>;
        { x.checkAccess(attr, Ops{}) } -> std::same_as<bool>;  // true if permitted
        {
            x.addChangeCallback(attr, Ops{}, std::function<void(bool)>{})
        } -> std::same_as<uintptr_t>;
        // no more calls after return is required
        { x.removeChangeCallback(uintptr_t{}) } -> std::same_as<void>;
    }
class AppOpsSession {
  public:
    /**
     * @param attr - AttributionChain which the access is attributed to.
     * @param ops - The ops required for this delivery
     * @param opChangedCb - A callback (async) which  notifies the data source that the permitted
     * state due to appops has changed. This is only called if a delivery request is ongoing (i.e.
     * after a `beginDeliveryRequest` but before a `endDeliveryRequest`, regardless of the return
     * value of the former). Upon calling the cb, appops has been updated, so the post-condition is
     * that the data source delivers data iff the parameter is true. If the delivery fails for some
     * reason, `endDeliveryRequest` should be called shortly, however, there is no re-entrancy into
     * this class. The client should never change the access request state based on this cb.
     * @param appOpsFacade - See the requires clause -- an interface which encapsulates the calls to
     * AppOpsService.
     */
    AppOpsSession(ValidatedAttributionSourceState attr, Ops ops,
                  std::function<void(bool)> opChangedCb, AppOpsFacade appOpsFacade = {})
        : mAttr(std::move(attr)),
          mOps(ops),
          mCb(std::move(opChangedCb)),
          mAppOps(std::move(appOpsFacade)),
          mCookie(mAppOps.addChangeCallback(mAttr, ops,
                                            [this](bool x) { this->onPermittedChanged(x); })),
          mDeliveryRequested(false),
          mDeliveryPermitted(mAppOps.checkAccess(mAttr, ops)) { }

    ~AppOpsSession() {
        endDeliveryRequest();
        mAppOps.removeChangeCallback(mCookie);
    }

    /**
     * Source intends to start delivering data. Updates AppOps if applicable.
     * @return true if data should be delivered (i.e. AppOps also permits delivery)
     */
    bool beginDeliveryRequest() {
        std::lock_guard l{mLock};
        if (mDeliveryRequested) {
            ALOG(LOG_WARN, "AppOpsSession", "Redundant beginDeliveryRequest ignored");
            return mDeliveryPermitted;
        }
        mDeliveryRequested = true;
        if (mDeliveryPermitted) {
            mDeliveryPermitted = mAppOps.startAccess(mAttr, mOps);
        }
        return mDeliveryPermitted;
    }

    /**
     * Source intends to stop delivering data. Updates AppOps if applicable.
     */
    void endDeliveryRequest() {
        std::lock_guard l{mLock};
        if (!mDeliveryRequested) return;
        mDeliveryRequested = false;
        if (mDeliveryPermitted) {
            mAppOps.stopAccess(mAttr, mOps);
        }
    }

    /**
     * Check if delivery is permitted.
     */
    bool isDeliveryPermitted() const {
        std::lock_guard l{mLock};
        return mDeliveryPermitted;
    }

  private:
    /**
     * AppOps permitted state has changed. From callback thread.
     */
    void onPermittedChanged(bool isPermitted) {
        std::lock_guard l{mLock};
        if (mDeliveryPermitted == isPermitted) return;
        const bool oldIsPermitted = mDeliveryPermitted;
        mDeliveryPermitted = isPermitted;
        if (!mDeliveryRequested) return;
        if (mDeliveryPermitted) {
            mDeliveryPermitted = mAppOps.startAccess(mAttr, mOps);
        } else {
            mAppOps.stopAccess(mAttr, mOps);
        }
        if (oldIsPermitted != mDeliveryPermitted) {
            mCb(mDeliveryPermitted);
        }
    }

    mutable std::mutex mLock{};
    const ValidatedAttributionSourceState mAttr;
    const Ops mOps;
    const std::function<void(bool)> mCb;
    AppOpsFacade mAppOps GUARDED_BY(mLock);
    const uintptr_t mCookie;
    bool mDeliveryRequested GUARDED_BY(mLock);
    bool mDeliveryPermitted GUARDED_BY(mLock);
};

class DefaultAppOpsFacade {
  public:
    bool startAccess(const ValidatedAttributionSourceState&, Ops);
    void stopAccess(const ValidatedAttributionSourceState&, Ops);
    bool checkAccess(const ValidatedAttributionSourceState&, Ops);
    uintptr_t addChangeCallback(const ValidatedAttributionSourceState&, Ops,
                                std::function<void(bool)> cb);
    void removeChangeCallback(uintptr_t);

    class OpMonitor : public com::android::internal::app::BnAppOpsCallback {
      public:
        OpMonitor(ValidatedAttributionSourceState attr, Ops ops, std::function<void(bool)> cb)
            : mAttr(std::move(attr)), mOps(ops), mCb(std::move(cb)) { }

        binder::Status opChanged(int32_t op, int32_t uid, const String16& packageName,
                                 const String16& persistenDeviceId) override;

        void stopListening() {
            std::lock_guard l_{mLock};
            mCb = nullptr;
        }

      private:
        const ValidatedAttributionSourceState mAttr;
        const Ops mOps;
        std::mutex mLock;
        std::function<void(bool)> mCb GUARDED_BY(mLock);
    };

  private:
    static inline std::mutex sMapLock{};
    static inline std::unordered_map<uintptr_t, sp<OpMonitor>> sCbMap{};
};

}  // namespace android::media::permission
