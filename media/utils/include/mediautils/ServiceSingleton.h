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

#include "BinderGenericUtils.h"

#include <android-base/thread_annotations.h>
#include <audio_utils/mutex.h>
#include <chrono>
#include <map>
#include <mutex>
#include <utils/Log.h>
#include <utils/Timers.h>

/**
 * ServiceSingleton provides a non-blocking NDK/CPP compatible service cache.
 *
 * This is a specialized cache that allows per-service configuration.
 *
 * Features:
 *
 * 1) Seamless compatibility with NDK and CPP based interfaces.
 * 2) Time-out based service acquisition.
 *    Set the maximum time to wait for any service.
 * 3) Service prefetch:
 *    Reduce start-up by prefetching service in advance (not on demand).
 *    Prefetch is automatically installed by getService().
 * 4) Manual interface setting for test and non-service manager acquisition support.
 *
 * If both NDK and CPP interfaces are available, we prefer the CPP version
 * for the following reasons:
 * 1) Established sp<> reference counting avoids mistakes. NDK tends to be error-prone.
 * 2) Possible reduced binder object clutter by a singleton notification binder object.
 *    Fewer binder objects are more efficient for the binder driver and ServiceManager.
 *    For example, fewer binder deaths means less ServiceManager (linear time) cleanup.
 *    A single binder object also offers binder access serialization.
 * 3) CPP offers slightly better efficiency as it is closer to the
 *    actual implementation, a minor detail and effect.
 *
 * We use a per-service ServiceHandler object to collect methods and implementation details.
 * Currently this is separate for NDK and CPP interfaces to the same service;
 * unification is possible by using ibinder_internals.h.
 */
namespace android::mediautils {

enum class ServiceOptions {
    kNone = 0,
    kNonNull = (1 << 0),  // don't return a null interface unless disabled.
                          // partially implemented and experimental.
};

enum class SkipMode {
    kNone = 0,       // do not skip the cache (normal behavior for caching services).
    kImmediate = 1,  // do not cache or find the service, return null to the caller immediately,
                     // which is the normal behavior for skipping the service cache.
    kWait = 2,       // do not cache or find the service, but block the caller;
                     // this is used for cases where a local service override is desired.
};

// Traits may come through a constexpr static function collection.
// This participates in small buffer optimization SBO in std::function impl.
template <typename Service>
struct DefaultServiceTraits {
    // getServiceName() returns the name associated with Service.
    //
    // If name is empty, it returns the name from the Service descriptor.
    // If name starts with '/', it appends the name as a version to the Service descriptor,
    // e.g. "/default".
    // Otherwise the name is assumed to be the Service name.
    static constexpr const char* getServiceName() { return "/default"; }

    // This callback is called when a new service is received.
    // The callback requires at least one thread in the Binder threadpool.
    static constexpr void onNewService(const InterfaceType<Service>&) {}

    // This callback is called if the service has died.
    // The callback requires at least one thread in the Binder threadpool.
    static constexpr void onServiceDied(const InterfaceType<Service>&) {}

    // ServiceOptions configured for the Service.
    static constexpr ServiceOptions options() { return ServiceOptions::kNone; }
};

// We store the traits as functors.
template <typename Service>
struct FunctionalServiceTraits {
    template <typename ServiceTraits>
    explicit FunctionalServiceTraits(const ServiceTraits& serviceTraits)
        : getServiceName{serviceTraits.getServiceName}
        , onNewService{serviceTraits.onNewService}
        , onServiceDied{serviceTraits.onServiceDied}
        , options{serviceTraits.options} {
    }
    std::function<const char*()> getServiceName;
    std::function<void(const InterfaceType<Service>& service)> onNewService;
    std::function<void(const InterfaceType<Service>& service)> onServiceDied;
    std::function<ServiceOptions()> options;
};

namespace details {

class ServiceHandler
{
public:
    /**
     * Returns a ServiceHandler, templated type T is String16 for the native type
     * of the CPP service descriptors and const char* for the native type of the NDK
     * service descriptors.
     */
    template<typename T>
    requires (std::is_same_v<T, const char*> || std::is_same_v<T, String16>)
    static std::shared_ptr<ServiceHandler> getInstance(const T& name);

    /**
     * Initializes the service handler with new service traits
     * (methods that are triggered on service events).
     *
     * This is optional.  Default construction of traits is allowed for
     * services that do not require special handling.
     *
     * @param serviceTraits
     * @return true if the service handler had been previously initialized.
     */
    template<typename Service, typename ServiceTraits>
    bool init(const ServiceTraits& serviceTraits) {
        auto traits = std::make_shared<FunctionalServiceTraits<Service>>(serviceTraits);
        std::shared_ptr<void> oldTraits;
        std::lock_guard l(mMutex);
        std::swap(oldTraits, mTraits);
        const bool existing = oldTraits != nullptr;
        mTraits = std::move(traits);
        mSkipMode = SkipMode::kNone;
        return existing;
    }

    /**
     * Returns the service based on a timeout.
     *
     * @param waitNs the time to wait, internally clamped to (0, INT64_MAX / 2) to
     *       avoid numeric overflow.
     * @param useCallback installs a callback instead of polling.
     *       the Callback persists if the call timeouts.  A Callback requires
     *       at least one thread in the threadpool.
     * @return Service interface.
     */
    template <typename Service>
    auto get(std::chrono::nanoseconds waitNs, bool useCallback) {
        audio_utils::unique_lock ul(mMutex);
        auto& service = std::get<BaseInterfaceType<Service>>(mService);

        // early check.
        if (mSkipMode == SkipMode::kImmediate || (service && mValid)) return service;

        // clamp to avoid numeric overflow.  INT64_MAX / 2 is effectively forever for a device.
        std::chrono::nanoseconds kWaitLimitNs(
                std::numeric_limits<decltype(waitNs.count())>::max() / 2);
        waitNs = std::clamp(waitNs, decltype(waitNs)(0), kWaitLimitNs);
        const auto end = std::chrono::steady_clock::now() + waitNs;

        for (bool first = true; true; first = false) {
            // we may have released mMutex, so see if service has been obtained.
            if (mSkipMode == SkipMode::kImmediate || (service && mValid))  return service;

            int options = 0;
            if (mSkipMode == SkipMode::kNone) {
                const auto traits = getTraits_l<Service>();

                // first time or not using callback, check the service.
                if (first || !useCallback) {
                    auto service_new = checkServicePassThrough<Service>(
                            traits->getServiceName());
                    if (service_new) {
                        mValid = true;
                        service = std::move(service_new);
                        // service is a reference, so we copy to service_fixed as
                        // we're releasing the mutex.
                        const auto service_fixed = service;
                        ul.unlock();
                        traits->onNewService(interfaceFromBase<Service>(service_fixed));
                        ul.lock();
                        setDeathNotifier_l<Service>(service_fixed);
                        ul.unlock();
                        mCv.notify_all();
                        return service_fixed;
                    }
                }
                // install service callback if needed.
                if (useCallback && !mServiceNotificationHandle) {
                    setServiceNotifier_l<Service>();
                }
                options = static_cast<int>(traits->options());
            }

            // check time expiration.
            const auto now = std::chrono::steady_clock::now();
            if (now >= end &&
                    (service
                    || mSkipMode != SkipMode::kNone  // skip is set.
                    || !(options & static_cast<int>(ServiceOptions::kNonNull)))) { // null allowed
                return service;
            }

            // compute time to wait, then wait.
            if (mServiceNotificationHandle) {
                mCv.wait_until(ul, end);
            } else {
                const auto target = now + kPollTime;
                mCv.wait_until(ul, std::min(target, end));
            }
            // loop back to see if we have any state change.
        }
    }

    /**
     * Sets an externally provided service override.
     *
     * @param Service
     * @param service_new
     */
    template<typename Service>
    void set(const InterfaceType<Service>& service_new) {
        audio_utils::unique_lock ul(mMutex);
        auto& service = std::get<BaseInterfaceType<Service>>(mService);
        const auto traits = getTraits_l<Service>();
        if (service) {
            auto orig_service = service;
            invalidateService_l<Service>();
            ul.unlock();
            traits->onServiceDied(interfaceFromBase<Service>(orig_service));
        }
        service = service_new;
        ul.unlock();
        // should we set the death notifier?  It could be a local service.
        if (service_new) traits->onNewService(service_new);
        mCv.notify_all();
    }

    /**
     * Disables cache management in the ServiceHandler.  init() needs to be
     * called to restart.
     *
     * All notifiers removed.
     * Service pointer is released.
     *
     * If skipMode is kNone,      then cache management is immediately reenabled.
     * If skipMode is kImmediate, then any new waiters will return null immediately.
     * If skipMode is kWait,      then any new waiters will be blocked until an update occurs
     *                            or the timeout expires.
     */
    template<typename Service>
    void skip(SkipMode skipMode) {
        audio_utils::unique_lock ul(mMutex);
        mSkipMode = skipMode;
        // remove notifiers.  OK to hold lock as presuming notifications one-way
        // or manually triggered outside of lock.
        mDeathNotificationHandle.reset();
        mServiceNotificationHandle.reset();
        auto& service = std::get<BaseInterfaceType<Service>>(mService);
        const auto traits = getTraits_l<Service>();
        std::shared_ptr<void> oldTraits;
        std::swap(oldTraits, mTraits);  // destroyed outside of lock.
        if (service) {
            auto orig_service = service;  // keep reference to service to manually notify death.
            invalidateService_l<Service>();  // sets service to nullptr
            ul.unlock();
            traits->onServiceDied(interfaceFromBase<Service>(orig_service));
        } else {
            ul.unlock();
        }
        mCv.notify_all();
    }

private:

    // invalidateService_l is called to remove the old death notifier,
    // invalidate the service, and optionally clear the service pointer.
    template <typename Service>
    void invalidateService_l() REQUIRES(mMutex) {
        mDeathNotificationHandle.reset();
        const auto traits = getTraits_l<Service>();
        mValid = false;
        if (!(static_cast<int>(traits->options()) & static_cast<int>(ServiceOptions::kNonNull))
                || mSkipMode != SkipMode::kNone) {
            auto &service = std::get<BaseInterfaceType<Service>>(mService);
            service = nullptr;
        }
    }

    // gets the traits set by init(), initializes with default if init() not called.
    template <typename Service>
    std::shared_ptr<FunctionalServiceTraits<Service>> getTraits_l() REQUIRES(mMutex) {
        if (!mTraits) {
            mTraits = std::make_shared<FunctionalServiceTraits<Service>>(
                    DefaultServiceTraits<Service>{});
        }
        return std::static_pointer_cast<FunctionalServiceTraits<Service>>(mTraits);
    }

    // sets the service notification
    template <typename Service>
    void setServiceNotifier_l() REQUIRES(mMutex) {
        const auto traits = getTraits_l<Service>();
        mServiceNotificationHandle = requestServiceNotification<Service>(
                [traits, this](const InterfaceType<Service>& service) {
                    audio_utils::unique_lock ul(mMutex);
                    auto originalService = std::get<BaseInterfaceType<Service>>(mService);
                    // we suppress equivalent services from being set
                    // where either the pointers match or the binder objects match.
                    if (!mediautils::isSameInterface(originalService, service)) {
                        if (originalService != nullptr) {
                            invalidateService_l<Service>();
                        }
                        mService = service;
                        mValid = true;
                        ul.unlock();
                        if (originalService != nullptr) {
                            traits->onServiceDied(interfaceFromBase<Service>(originalService));
                        }
                        traits->onNewService(service);
                        ul.lock();
                        setDeathNotifier_l<Service>(service);
                    } else {
                        ALOGW("%s: ignoring duplicated service: %p",
                                __func__, originalService.get());
                    }
                    ul.unlock();
                    mCv.notify_all();
                }, traits->getServiceName());
        ALOGW_IF(!mServiceNotificationHandle, "%s: cannot register service notification %s"
                                              " (do we have permission?)",
                __func__, toString(Service::descriptor).c_str());
    }

    // sets the death notifier for mService (mService must be non-null).
    template <typename Service>
    void setDeathNotifier_l(const BaseInterfaceType<Service>& base) REQUIRES(mMutex) {
        // here the pointer match should be identical to binder object match
        // since we use a cached service.
        if (base != std::get<BaseInterfaceType<Service>>(mService)) {
            ALOGW("%s: service has changed for %s, skipping death notification registration",
                    __func__, toString(Service::descriptor).c_str());
            return;
        }
        auto service = interfaceFromBase<Service>(base);
        const auto binder = binderFromInterface(service);
        if (binder.get()) {
            auto traits = getTraits_l<Service>();
            mDeathNotificationHandle = requestDeathNotification(
                    base, [traits, service, this]() {
                        // as only one death notification is dispatched,
                        // we do not need to generation count.
                        {
                            std::lock_guard l(mMutex);
                            const auto currentService =
                                    std::get<BaseInterfaceType<Service>>(mService);
                            if (currentService != service) {
                                ALOGW("%s: ignoring death as current service "
                                        "%p != registered death service %p", __func__,
                                        currentService.get(), service.get());
                                return;
                            }
                            invalidateService_l<Service>();
                        }
                        traits->onServiceDied(service);
                    });
            // Implementation detail: if the service has already died,
            // we do not call the death notification, but log the issue here.
            ALOGW_IF(!mDeathNotificationHandle, "%s: cannot register death notification %s"
                                                " (already died?)",
                    __func__, toString(Service::descriptor).c_str());
        }
    }

    // initializes the variant for NDK use (called on first creation in the cache map).
    void init_ndk() EXCLUDES(mMutex) {
        std::lock_guard l(mMutex);
        mService = std::shared_ptr<::ndk::ICInterface>{};
    }

    // initializes the variant for CPP use (called on first creation in the cache map).
    void init_cpp() EXCLUDES(mMutex) {
        std::lock_guard l(mMutex);
        mService = sp<::android::IInterface>{};
    }

    static std::string toString(const std::string& s) { return s; }
    static std::string toString(const String16& s) { return String8(s).c_str(); }

    mutable std::mutex mMutex;
    std::condition_variable mCv;
    static constexpr auto kPollTime = std::chrono::seconds(1);

    std::variant<std::shared_ptr<::ndk::ICInterface>,
            sp<::android::IInterface>> mService GUARDED_BY(mMutex);
    // aesthetically we place these last, but a ServiceHandler is never deleted in
    // current operation, so there is no deadlock on destruction.
    std::shared_ptr<void> mDeathNotificationHandle GUARDED_BY(mMutex);
    std::shared_ptr<void> mServiceNotificationHandle GUARDED_BY(mMutex);
    std::shared_ptr<void> mTraits GUARDED_BY(mMutex);

    // mValid is true iff the service is non-null and alive.
    bool mValid GUARDED_BY(mMutex) = false;

    // mSkipMode indicates the service cache state:
    //
    // one may either wait (blocked) until the service is reinitialized.
    SkipMode mSkipMode GUARDED_BY(mMutex) = SkipMode::kNone;
};

} // details

//----------------------------------
// ServiceSingleton API
//

/*
 * Implementation detail:
 *
 * Each CPP or NDK service interface has a unique ServiceHandler that
 * is stored in a singleton cache.  The cache key is based on the service descriptor string
 * so only one version can be chosen.  (The particular version may be changed using
 * ServiceTraits.getName()).
 */

/**
 * Sets the service trait parameters for acquiring the Service interface.
 *
 * If this is not set before the first service fetch, then default service traits are used.
 *
 * @return true if there is a preexisting (including prior default set) traits.
 */
template<typename Service, typename ServiceTraits>
bool initService(const ServiceTraits& serviceTraits = {}) {
    const auto serviceHandler = details::ServiceHandler::getInstance(Service::descriptor);
    return serviceHandler->template init<Service>(serviceTraits);
}

/**
 * Returns either a std::shared_ptr<Interface> or sp<Interface>
 * for the AIDL service.  If the service is not available within waitNs,
 * the method will return nullptr
 * (or the previous invalidated service if Service.options() & kNonNull).
 *
 * This method installs a callback to obtain the service, so with waitNs == 0, it may be used to
 * prefetch the service before it is actually needed.
 *
 * @param waitNs wait time for the service to become available.
 * @return
 *    a sp<> for a CPP interface
 *    a std::shared_ptr<> for a NDK interface
 *
 */
template<typename Service>
auto getService(std::chrono::nanoseconds waitNs = {}) {
    const auto serviceHandler = details::ServiceHandler::getInstance(Service::descriptor);
    return interfaceFromBase<Service>(serviceHandler->template get<Service>(
            waitNs, true /* useCallback */));
}

/**
 * Returns either a std::shared_ptr<Interface> or sp<Interface>
 * for the AIDL service.  If the service is not available within waitNs,
 * the method will return nullptr
 * (or the previous invalidated service if Service.options() & kNonNull).
 *
 * This method polls to obtain the service, which
 * is useful if the service is restricted due to permissions or
 * one is concerned about ThreadPool starvation.
 *
 * @param waitNs wait time for the service to become available.
 * @return
 *    a sp<> for a CPP interface
 *    a std::shared_ptr<> for a NDK interface
 */
template<typename Service>
auto checkService(std::chrono::nanoseconds waitNs = {}) {
    const auto serviceHandler = details::ServiceHandler::getInstance(Service::descriptor);
    return interfaceFromBase<Service>(serviceHandler->template get<Service>(
            waitNs, false /* useCallback */));
}

/**
 * Sets a service implementation override, replacing any fetched service from ServiceManager.
 *
 * An empty service clears the cache.
 */
template<typename Service>
void setService(const InterfaceType<Service>& service) {
    const auto serviceHandler = details::ServiceHandler::getInstance(Service::descriptor);
    serviceHandler->template set<Service>(service);
}

/**
 * Disables the service cache.
 *
 * This releases any service and notification callbacks.  After this,
 * another initService() can be called seamlessly.
 */
template<typename Service>
void skipService(SkipMode skipMode = SkipMode::kImmediate) {
    const auto serviceHandler = details::ServiceHandler::getInstance(Service::descriptor);
    serviceHandler->template skip<Service>(skipMode);
}

} // namespace android::mediautils
