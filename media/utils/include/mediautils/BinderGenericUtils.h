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

#include <android/binder_auto_utils.h>
#include <android/binder_interface_utils.h>
#include <android/binder_manager.h>
#include <binder/IServiceManager.h>

namespace android::mediautils {
// General Template Binder Utilities.
//
// In order to write generic Template methods, we need to have utility methods
// that provide seamless template overload resolution between NDK and CPP variants.
//

// Returns true or false based on whether the Interface is a NDK Interface.
template<typename Interface>
inline constexpr bool is_ndk = std::derived_from<Interface, ::ndk::ICInterface>;

// Returns the Interface ptr type (shared_ptr or sp) based on the Interface.
template<typename Interface>
using InterfaceType =
        std::conditional_t <is_ndk<Interface>, std::shared_ptr<Interface>, sp<Interface>>;

template<typename Interface>
using BaseInterfaceType = std::conditional_t <is_ndk<Interface>,
std::shared_ptr<::ndk::ICInterface>, sp<::android::IInterface>>;

/**
 * Returns either a sp<IBinder> or an SpAIBinder object
 * for the AIDL interface given.
 *
 * A -cpp interface will return sp<IBinder>.
 * A -ndk interface will return SpAIBinder
 */
template<typename Interface>
sp<IBinder> binderFromInterface(const sp<Interface> &interface) {
    return IInterface::asBinder(interface);
}

template<typename Interface>
::ndk::SpAIBinder binderFromInterface(const std::shared_ptr<Interface> &interface) {
    return interface->asBinder();
}

/**
 * Returns true if two interfaces pointer-match, or represent identical binder objects.
 *
 * C++ with C++ and NDK with NDK interfaces may be compared.
 *
 * It currently isn't possible through the NDK public interface to extract
 * the underlying C++ binder object, so we don't allow NDK and C++ interfaces to
 * be cross-checked even though they might be backed by the same binder object.
 */
static inline bool isSameInterface(const sp<IInterface>& a, const sp<IInterface>& b) {
    return a == b || (a && b && IInterface::asBinder(a) == IInterface::asBinder(b));
}

static inline bool isSameInterface(const std::shared_ptr<::ndk::ICInterface>& a,
        const std::shared_ptr<::ndk::ICInterface>& b) {
    return a == b || (a && b && a->asBinder() == b->asBinder());
}

/**
 * Returns either a sp<Interface> or a std::shared_ptr<Interface> from a Binder object.
 *
 * A -cpp interface will return sp<Interface>.
 * A -ndk interface will return std::shared_ptr<Interface>
 */
template<typename Interface>
sp<Interface> interfaceFromBinder(const sp<IBinder> &binder) {
    return interface_cast<Interface>(binder);
}

template<typename Interface>
std::shared_ptr<Interface> interfaceFromBinder(const ::ndk::SpAIBinder &binder) {
    return Interface::fromBinder(binder);
}

/**
 * Returns either a sp<Interface> or a std::shared_ptr<Interface> from
 * the NDK/CPP base interface class.
 */
template<typename Interface>
sp<Interface> interfaceFromBase(const sp<::android::IInterface> &interface) {
    // this is unvalidated, though could verify getInterfaceDescriptor() == Interface::descriptor
    return sp<Interface>::cast(interface);
}

template<typename Interface>
std::shared_ptr<Interface> interfaceFromBase(
        const std::shared_ptr<::ndk::ICInterface> &interface) {
    // this is unvalidated, though could verify
    // !strcmp(AIBinder_Class_getDescriptor(AIBinder_getClass(...), Interface::descriptor)
    return std::static_pointer_cast<Interface>(interface);
}

/**
 * Returns a fully qualified service name.
 *
 * @param name
 * If name is empty, it returns the name from the Service descriptor.
 * If name starts with '/', it appends the name as a version to the Service descriptor,
 * e.g. "/default".
 * Otherwise the name is assumed to be the full Service name, overriding the
 * Service descriptor.
 */
template<typename Service>
auto fullyQualifiedServiceName(const char* const name) {
    using StringType = std::conditional_t<is_ndk<Service>, std::string, String16>;
    return name == nullptr ? StringType(Service::descriptor)
            : name[0] != 0 && name[0] != '/' ? StringType(name)
                    : StringType(Service::descriptor) + StringType(name);
}

/**
 * Returns either a std::shared_ptr<Interface> or sp<Interface>
 * for the AIDL interface given.
 *
 * A -cpp interface will return sp<Service>.
 * A -ndk interface will return std::shared_ptr<Service>
 *
 * @param name if non-empty should contain either a suffix if it starts
 * with a '/' such as "/default", or the full service name.
 */
template<typename Service>
auto checkServicePassThrough(const char *const name = "") {
    if constexpr(is_ndk<Service>)
    {
        const auto serviceName = fullyQualifiedServiceName<Service>(name);
        return Service::fromBinder(
                ::ndk::SpAIBinder(AServiceManager_checkService(serviceName.c_str())));
    } else /* constexpr */ {
        const auto serviceName = fullyQualifiedServiceName<Service>(name);
        auto binder = defaultServiceManager()->checkService(serviceName);
        return interface_cast<Service>(binder);
    }
}

template<typename Service>
void addService(const std::shared_ptr<Service> &service) {
    AServiceManager_addService(binderFromInterface(service), Service::descriptor);
}

template<typename Service>
void addService(const sp<Service> &service) {
    defaultServiceManager()->addService(Service::descriptor, binderFromInterface(service));
}

namespace details {

// Use the APIs below, not the details here.

/**
 * RequestServiceManagerCallback(Cpp|Ndk) is a RAII class that
 * requests a ServiceManager callback.
 *
 * Note the ServiceManager is a single threaded "apartment" and only one
 * transaction is active, hence:
 *
 * 1) After the RequestServiceManagerCallback object is destroyed no
 *    calls to the onBinder function is pending or will occur.
 * 2) To prevent deadlock, do not construct or destroy the class with
 *    a lock held that the onService function also requires.
 */
template<typename Service>
class RequestServiceManagerCallbackCpp {
public:
    explicit RequestServiceManagerCallbackCpp(
            std::function<void(const sp<Service> &)> &&onService,
            const char *const serviceName = ""
    )
            : mServiceName{fullyQualifiedServiceName<Service>(serviceName)},
              mWaiter{sp<Waiter>::make(std::move(onService))},
              mStatus{defaultServiceManager()->registerForNotifications(mServiceName,
                                                                        mWaiter)} {
    }

    ~RequestServiceManagerCallbackCpp() {
        if (mStatus == OK) {
            defaultServiceManager()->unregisterForNotifications(mServiceName, mWaiter);
        }
    }

    status_t getStatus() const {
        return mStatus;
    }

private:
    const String16 mServiceName;
    const sp<IServiceManager::LocalRegistrationCallback> mWaiter;
    const status_t mStatus;

    // With some work here, we could make this a singleton to improve
    // performance and reduce binder clutter.
    class Waiter : public IServiceManager::LocalRegistrationCallback {
    public:
        explicit Waiter(std::function<void(const sp<Service> &)> &&onService)
                : mOnService{std::move(onService)} {}

    private:
        void onServiceRegistration(
                const String16 & /*name*/, const sp<IBinder> &binder) final {
            mOnService(interface_cast<Service>(binder));
        }

        const std::function<void(const sp<Service> &)> mOnService;
    };
};

template<typename Service>
class RequestServiceManagerCallbackNdk {
public:
    explicit RequestServiceManagerCallbackNdk(
            std::function<void(const std::shared_ptr<Service> &)> &&onService,
            const char *const serviceName = ""
    )
            : mServiceName{fullyQualifiedServiceName<Service>(serviceName)},
              mOnService{std::move(onService)},
              mWaiter{AServiceManager_registerForServiceNotifications(
                      mServiceName.c_str(),
                      onRegister, this)}  // must be registered after mOnService.
    {}

    ~RequestServiceManagerCallbackNdk() {
        if (mWaiter) {
            AServiceManager_NotificationRegistration_delete(mWaiter);
        }
    }

    status_t getStatus() const {
        return mWaiter != nullptr ? OK : INVALID_OPERATION;
    }

private:
    const std::string mServiceName;  // must keep a local copy.
    const std::function<void(const std::shared_ptr<Service> &)> mOnService;
    AServiceManager_NotificationRegistration *const mWaiter;  // last.

    static void onRegister(const char *instance, AIBinder *registered, void *cookie) {
        (void) instance;
        auto *callbackHandler = static_cast<RequestServiceManagerCallbackNdk<Service> *>(cookie);
        callbackHandler->mOnService(Service::fromBinder(::ndk::SpAIBinder(registered)));
    }
};

/**
 * RequestDeathNotification(Cpp|Ndk) is a RAII class that
 * requests a death notification.
 *
 * Note the ServiceManager is a single threaded "apartment" and only one
 * transaction is active, hence:
 *
 * 1) After the RequestDeathNotification object is destroyed no
 *    calls to the onBinder function is pending or will occur.
 * 2) To prevent deadlock, do not construct or destroy the class with
 *    a lock held that the onBinderDied function also requires.
 */

class RequestDeathNotificationCpp {
    class DeathRecipientHelper : public IBinder::DeathRecipient {
    public:
        explicit DeathRecipientHelper(std::function<void()> &&onBinderDied)
                : mOnBinderDied{std::move(onBinderDied)} {
        }

        void binderDied(const wp<IBinder> &weakBinder) final {
            (void) weakBinder;
            mOnBinderDied();
        }

    private:
        const std::function<void()> mOnBinderDied;
    };

public:
    RequestDeathNotificationCpp(const sp<IBinder> &binder,
                                std::function<void()> &&onBinderDied)
            : mHelper{sp<DeathRecipientHelper>::make(std::move(onBinderDied))},
              mWeakBinder{binder}, mStatus{binder->linkToDeath(mHelper)} {
        ALOGW_IF(mStatus != OK, "%s: linkToDeath status:%d", __func__, mStatus);
    }

    ~RequestDeathNotificationCpp() {
        if (mStatus == OK) {
            const auto binder = mWeakBinder.promote();
            if (binder) binder->unlinkToDeath(mHelper);
        }
    }

    status_t getStatus() const {
        return mStatus;
    }

private:
    const sp<DeathRecipientHelper> mHelper;
    const wp<IBinder> mWeakBinder;
    const status_t mStatus;
};

class RequestDeathNotificationNdk {
public:
    RequestDeathNotificationNdk(
            const ::ndk::SpAIBinder &binder, std::function<void()>&& onBinderDied)
            : mRecipient(::AIBinder_DeathRecipient_new(OnBinderDiedStatic),
                         &AIBinder_DeathRecipient_delete),
              mStatus{(AIBinder_DeathRecipient_setOnUnlinked(  // sets cookie deleter
                              mRecipient.get(), OnBinderDiedUnlinkedStatic),
                      AIBinder_linkToDeath(  // registers callback
                              binder.get(), mRecipient.get(),
                              // we create functional cookie ptr which may outlive this object.
                              new std::function<void()>(std::move(onBinderDied))))} {
        ALOGW_IF(mStatus != OK, "%s: AIBinder_linkToDeath status:%d", __func__, mStatus);
    }

    ~RequestDeathNotificationNdk() {
        // mRecipient's unique_ptr calls AIBinder_DeathRecipient_delete to unlink the recipient.
        // Then OnBinderDiedUnlinkedStatic eventually deletes the cookie.
    }

    status_t getStatus() const {
        return mStatus;
    }

private:
    static void OnBinderDiedUnlinkedStatic(void* cookie) {
        delete reinterpret_cast<std::function<void()>*>(cookie);
    }

    static void OnBinderDiedStatic(void* cookie) {
        (*reinterpret_cast<std::function<void()>*>(cookie))();
    }

    const std::unique_ptr<AIBinder_DeathRecipient, decltype(
            &AIBinder_DeathRecipient_delete)>
            mRecipient;
    const status_t mStatus;  // binder_status_t is a limited subset of status_t
};

} // details

/**
 * Requests a notification that service is available.
 *
 * An opaque handle is returned - after clearing it is guaranteed that
 * no callback will occur.
 *
 * The callback will be of form:
 *     onService(const sp<Service>& service);
 *     onService(const std::shared_ptr<Service>& service);
 */
template<typename Service, typename F>
std::shared_ptr<void> requestServiceNotification(
        F onService, const char *const serviceName = "") {
    // the following are used for callbacks but placed here for invalidate.
    using RequestServiceManagerCallback = std::conditional_t<is_ndk<Service>,
            details::RequestServiceManagerCallbackNdk<Service>,
            details::RequestServiceManagerCallbackCpp<Service>>;
    const auto ptr = std::make_shared<RequestServiceManagerCallback>(
            onService, serviceName);
    const auto status = ptr->getStatus();
    return status == OK ? ptr : nullptr;
}

/**
 * Requests a death notification.
 *
 * An opaque handle is returned.  If the service is already dead, the
 * handle will be null.
 *
 * Implementation detail: A callback may occur after the handle is released
 * if a death notification is in progress.
 *
 * The callback will be of form void onBinderDied();
 */
template<typename Service>
std::shared_ptr<void> requestDeathNotification(
        const sp<Service> &service, std::function<void()> &&onBinderDied) {
    const auto ptr = std::make_shared<details::RequestDeathNotificationCpp>(
            binderFromInterface(service), std::move(onBinderDied));
    const auto status = ptr->getStatus();
    return status == OK ? ptr : nullptr;
}

template<typename Service>
std::shared_ptr<void> requestDeathNotification(
        const std::shared_ptr<Service> &service, std::function<void()> &&onBinderDied) {
    const auto ptr = std::make_shared<details::RequestDeathNotificationNdk>(
            binderFromInterface(service), std::move(onBinderDied));
    const auto status = ptr->getStatus();
    return status == OK ? ptr : nullptr;
}

} // namespace android::mediautils
