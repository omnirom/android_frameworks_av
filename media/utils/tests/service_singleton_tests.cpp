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

#define LOG_TAG "service_singleton_tests"

#include <mediautils/ServiceSingleton.h>

#include "BnServiceSingletonTest.h"
#include "aidl/BnServiceSingletonTest.h"
#include <audio_utils/RunRemote.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <gtest/gtest.h>
#include <utils/Log.h>

#include <thread>
#include <vector>

using namespace android;

/**
 * Service Singleton Test uses a worker process to spawn new binder services.
 *
 * A worker process is required since we cannot fork after registering
 * with the binder driver.
 *
 * Test Process -> Worker_Process -> Service Process(1)
 *                                -> Service Process(2)
 *                                -> ....
 */

// Service implementation.
class ServiceSingletonTestCpp : public BnServiceSingletonTest {
public:
    binder::Status inc(int32_t* _aidl_return) final {
        *_aidl_return = ++mValue;
        return binder::Status::ok();
    }
    std::atomic_int32_t mValue = 0;
};

// The service traits increment static atomic counters, which
// validates that the trait callbacks are invoked.
static std::atomic_int32_t sNewService = 0;
static std::atomic_int32_t sServiceDied = 0;

template <typename Service>
struct TestServiceTraits : public mediautils::DefaultServiceTraits<Service> {
    static constexpr const char* getServiceName() { return ""; }
    static constexpr void onNewService(const mediautils::InterfaceType<Service>&) {
        ++sNewService;
    }
    static constexpr void onServiceDied(const mediautils::InterfaceType<Service>&) {
        ++sServiceDied;
    }
};

// SetServiceTraits just increments a counter for each onNewService called.
// This is used to check calling set on the service.
template <typename Service>
struct SetServiceTraits : public mediautils::DefaultServiceTraits<Service> {
    static constexpr const char* getServiceName() { return ""; }
    static constexpr void onNewService(const mediautils::InterfaceType<Service>&) {
        ++newCounter;
    }
    static constexpr void onServiceDied(const mediautils::InterfaceType<Service>&) {
        ++diedCounter;
    }
    static inline std::atomic_int32_t newCounter = 0;
    static inline std::atomic_int32_t diedCounter = 0;
};

// ConcurrentServiceTraits checks that concurrent requests to get the service
// return only after the onNewService callback has completed.
template <typename Service>
struct ConcurrentServiceTraits : public mediautils::DefaultServiceTraits<Service> {
    static constexpr const char* getServiceName() { return ""; }
    static constexpr void onNewService(const mediautils::InterfaceType<Service>&) {
        sleep(1); // delay - this ensures that a concurrent request that doesn't block sees 0.
        ++newCounter;
    }
    static constexpr void onServiceDied(const mediautils::InterfaceType<Service>&) {
        ++diedCounter;
    }
    static inline std::atomic_int32_t newCounter = 0;
    static inline std::atomic_int32_t diedCounter = 0;
};

// Here we have an alternative set of service traits,
// used to validate that we can switch traits for the service singleton.
static std::atomic_int32_t sNewService2 = 0;
static std::atomic_int32_t sServiceDied2 = 0;

template <typename Service>
struct TestServiceTraits2 : public mediautils::DefaultServiceTraits<Service> {
    static constexpr const char* getServiceName() { return ""; }
    static constexpr void onNewService(const mediautils::InterfaceType<Service>&) {
        ++sNewService2;
    }
    static constexpr void onServiceDied(const mediautils::InterfaceType<Service>&) {
        ++sServiceDied2;
    }
};

/*
 * ServiceThreads run in a remote process.
 *
 * The WorkerThread is used to launch and kill the ServiceThread in a remote process.
 */
static void ServiceThread(audio_utils::RunRemote& runRemote) {
    int c = runRemote.getChar();  // requires any character to launch
    auto service = sp<IServiceSingletonTest>::cast(sp<ServiceSingletonTestCpp>::make());
    mediautils::addService(service);
    ProcessState::self()->startThreadPool();
    runRemote.putChar(c);  // echo character.
    IPCThreadState::self()->joinThreadPool();
}

/*
 * The WorkerThread is run in a remote process from the test.  It communicates with
 * the test process through pipes.
 */
static void WorkerThread(audio_utils::RunRemote& runRemote) {
    std::shared_ptr<audio_utils::RunRemote> remoteService;
    while (true) {
        const int c = runRemote.getChar();
        switch (c) {
            case 'a':  // launch a new service.
                // if the old service isn't destroyed, it will be destroyed here
                // when the RunRemote is replaced.
                remoteService = std::make_shared<audio_utils::RunRemote>(ServiceThread);
                remoteService->run();
                remoteService->putChar('a');  // create service.
                (void)remoteService->getChar(); // ensure it is created.
                runRemote.putChar(c);  // echo
                break;
            case 'b':  // destroys the old service.
                remoteService.reset();  // this kills the service.
                runRemote.putChar(c);  // echo
                break;
            default:  // respond that we don't know what happened!
                runRemote.putChar('?');
                break;
        }
    }
}

class ServiceSingletonTests : public ::testing::Test {
protected:
    std::shared_ptr<audio_utils::RunRemote> mRemoteWorker = getRemoteWorker();

    // Use a worker singleton because once we set up binder,
    // we can't spawn the remote worker again.
    static std::shared_ptr<audio_utils::RunRemote> getRemoteWorker() {
        [[clang::no_destroy]]
        static std::shared_ptr<audio_utils::RunRemote> remoteWorker = []() {
            // create worker that spawns service in a different process.
            const auto worker = std::make_shared<audio_utils::RunRemote>(WorkerThread);
            worker->run();

            // now we are ready for binder.
            ProcessState::self()->startThreadPool();

            return worker;
        }();
        return remoteWorker;
    }
};

// This is a monolithic test.
TEST_F(ServiceSingletonTests, Basic) {
    std::atomic_int32_t listenerServiceCreated = 0;
    std::atomic_int32_t listenerServiceDied = 0;

    // initialize the service cache with a custom handler.
    mediautils::initService<
        IServiceSingletonTest, TestServiceTraits<IServiceSingletonTest>>({});
    mediautils::initService<
        aidl::IServiceSingletonTest, TestServiceTraits<aidl::IServiceSingletonTest>>({});

    // check that our service isn't preexisting.
    {
        auto service = mediautils::checkServicePassThrough<IServiceSingletonTest>();
        EXPECT_FALSE(service);

        auto service2 = mediautils::checkServicePassThrough<aidl::IServiceSingletonTest>();
        EXPECT_FALSE(service2);
    }
    EXPECT_EQ(0, sNewService);
    EXPECT_EQ(0, sServiceDied);

    {
        auto service = mediautils::checkService<IServiceSingletonTest>();
        EXPECT_FALSE(service);

        auto service2 = mediautils::checkService<aidl::IServiceSingletonTest>();
        EXPECT_FALSE(service2);
    }
    EXPECT_EQ(0, sNewService);
    EXPECT_EQ(0, sServiceDied);

    // getService will register a notification handler that fetches the
    // service in the background.
    {
        auto service = mediautils::getService<IServiceSingletonTest>();
        EXPECT_FALSE(service);

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>();
        EXPECT_FALSE(service2);
    }
    EXPECT_EQ(0, sNewService);
    EXPECT_EQ(0, sServiceDied);

    // now spawn the service.
    mRemoteWorker->putChar('a');
    EXPECT_EQ('a', mRemoteWorker->getChar());

    sleep(1);  // In the background, 2 services were fetched.

    EXPECT_EQ(2, sNewService);
    EXPECT_EQ(0, sServiceDied);

    // we repeat the prior checks, but the service is cached now.
    {
        auto service = mediautils::checkServicePassThrough<IServiceSingletonTest>();
        EXPECT_TRUE(service);

        auto service2 = mediautils::checkServicePassThrough<aidl::IServiceSingletonTest>();
        EXPECT_TRUE(service2);
    }
    EXPECT_EQ(2, sNewService);
    EXPECT_EQ(0, sServiceDied);

    {
        auto service = mediautils::checkService<IServiceSingletonTest>();
        EXPECT_TRUE(service);

        auto service2 = mediautils::checkService<aidl::IServiceSingletonTest>();
        EXPECT_TRUE(service2);
    }
    EXPECT_EQ(2, sNewService);
    EXPECT_EQ(0, sServiceDied);

    {
        auto service = mediautils::getService<IServiceSingletonTest>();
        EXPECT_TRUE(service);

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>();
        EXPECT_TRUE(service2);
    }
    EXPECT_EQ(2, sNewService);
    EXPECT_EQ(0, sServiceDied);

    // destroy the service.
    mRemoteWorker->putChar('b');
    EXPECT_EQ('b', mRemoteWorker->getChar());

    sleep(1);

    // We expect the died callbacks.
    EXPECT_EQ(2, sNewService);
    EXPECT_EQ(2, sServiceDied);

    // we can also manually check whether there is a new service by
    // requesting service notifications.  This is outside of the service singleton
    // traits.
    auto handle1 = mediautils::requestServiceNotification<IServiceSingletonTest>(
            [&](const sp<IServiceSingletonTest>&) { ++listenerServiceCreated; });
    auto handle2 = mediautils::requestServiceNotification<aidl::IServiceSingletonTest>(
            [&](const std::shared_ptr<aidl::IServiceSingletonTest>&) {
                ++listenerServiceCreated; });

    // Spawn the service again.
    mRemoteWorker->putChar('a');
    EXPECT_EQ('a', mRemoteWorker->getChar());

    sleep(1);  // In the background, 2 services were fetched.

    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(2, sServiceDied);

    EXPECT_EQ(2, listenerServiceCreated);  // our listener picked up the service creation.

    std::shared_ptr<void> handle3, handle4;
    std::shared_ptr<aidl::IServiceSingletonTest> keepAlive;  // NDK Workaround!
    {
        auto service = mediautils::getService<IServiceSingletonTest>();
        EXPECT_TRUE(service);

        // mediautils::getService<> is a cached service.
        // pointer equality is preserved for subsequent requests.
        auto service_equal = mediautils::getService<IServiceSingletonTest>();
        EXPECT_EQ(service, service_equal);
        EXPECT_TRUE(mediautils::isSameInterface(service, service_equal));

        // we can create an alias to the service by requesting it outside of the cache.
        // this is a different pointer, but same underlying binder object.
        auto service_equivalent =
                mediautils::checkServicePassThrough<IServiceSingletonTest>();
        EXPECT_NE(service, service_equivalent);
        EXPECT_TRUE(mediautils::isSameInterface(service, service_equivalent));

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>();
        EXPECT_TRUE(service2);

        // mediautils::getService<> is a cached service.
        // pointer equality is preserved for subsequent requests.
        auto service2_equal = mediautils::getService<aidl::IServiceSingletonTest>();
        EXPECT_EQ(service2, service2_equal);
        EXPECT_TRUE(mediautils::isSameInterface(service2, service2_equal));

        // we can create an alias to the service by requesting it outside of the cache.
        // this is a different pointer, but same underlying binder object.
        auto service2_equivalent =
                mediautils::checkServicePassThrough<aidl::IServiceSingletonTest>();
        EXPECT_NE(service2, service2_equivalent);
        EXPECT_TRUE(mediautils::isSameInterface(service2, service2_equivalent));

        keepAlive = service2;

        // we can also request our own death notifications (outside of the service traits).
        handle3 = mediautils::requestDeathNotification(service, [&] { ++listenerServiceDied; });
        EXPECT_TRUE(handle3);
        handle4 = mediautils::requestDeathNotification(service2, [&] { ++listenerServiceDied; });
        EXPECT_TRUE(handle4);
    }

    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(2, sServiceDied);

    // destroy the service.

    mRemoteWorker->putChar('b');
    EXPECT_EQ('b', mRemoteWorker->getChar());

    sleep(1);

    // We expect the died callbacks.
    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(4, sServiceDied);

    EXPECT_EQ(2, listenerServiceCreated);
    EXPECT_EQ(2, listenerServiceDied);  // NDK Workaround - without keepAlive, this is 1.
                                        // the death notification is invalidated without a
                                        // pointer to the binder object.

    keepAlive.reset();

    // Cancel the singleton cache.
    mediautils::skipService<IServiceSingletonTest>();
    mediautils::skipService<aidl::IServiceSingletonTest>();

    // Spawn the service again.
    mRemoteWorker->putChar('a');
    EXPECT_EQ('a', mRemoteWorker->getChar());

    sleep(1);

    // We expect no change from the service traits (service not cached).
    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(4, listenerServiceCreated);  // our listener picks it up.

    {
        // in default mode (kNull) a null is returned when the service is skipped and
        // wait time is ignored.

        const auto ref1 = std::chrono::steady_clock::now();
        auto service = mediautils::getService<IServiceSingletonTest>(std::chrono::seconds(2));
        EXPECT_FALSE(service);
        const auto ref2 = std::chrono::steady_clock::now();
        EXPECT_LT(ref2 - ref1, std::chrono::seconds(1));

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>(
                std::chrono::seconds(2));
        EXPECT_FALSE(service2);
        const auto ref3 = std::chrono::steady_clock::now();
        EXPECT_LT(ref3 - ref2, std::chrono::seconds(1));
    }

    // Cancel the singleton cache but use wait mode.
    mediautils::skipService<IServiceSingletonTest>(mediautils::SkipMode::kWait);
    mediautils::skipService<aidl::IServiceSingletonTest>(mediautils::SkipMode::kWait);

    {
        // in wait mode, the timeouts are respected
        const auto ref1 = std::chrono::steady_clock::now();
        auto service = mediautils::getService<IServiceSingletonTest>(std::chrono::seconds(1));
        EXPECT_FALSE(service);
        const auto ref2 = std::chrono::steady_clock::now();
        EXPECT_GT(ref2 - ref1, std::chrono::seconds(1));

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>(
                std::chrono::seconds(1));
        EXPECT_FALSE(service2);
        const auto ref3 = std::chrono::steady_clock::now();
        EXPECT_GT(ref3 - ref2, std::chrono::seconds(1));
    }

    // remove service
    mRemoteWorker->putChar('b');
    EXPECT_EQ('b', mRemoteWorker->getChar());

    sleep(1);

    // We expect no change from the service traits (service not cached).
    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(4, listenerServiceCreated);
    EXPECT_EQ(2, listenerServiceDied);  // binder died is associated with the actual handle.

    // replace the service traits.
    {
        auto previous = mediautils::initService<
                IServiceSingletonTest, TestServiceTraits2<IServiceSingletonTest>>({});
        auto previous2 = mediautils::initService<
                aidl::IServiceSingletonTest, TestServiceTraits2<aidl::IServiceSingletonTest>>({});

        EXPECT_FALSE(previous);
        EXPECT_FALSE(previous2);
    }

    // We expect no change with old counters.
    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(0, sNewService2);
    EXPECT_EQ(0, sServiceDied2);

    {
        auto service = mediautils::getService<IServiceSingletonTest>();
        EXPECT_FALSE(service);

        auto service2 = mediautils::getService<aidl::IServiceSingletonTest>();
        EXPECT_FALSE(service2);
    }

    EXPECT_EQ(4, sNewService);
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(0, sNewService2);
    EXPECT_EQ(0, sServiceDied2);

    // Spawn the service again.
    mRemoteWorker->putChar('a');
    EXPECT_EQ('a', mRemoteWorker->getChar());

    sleep(1);

    EXPECT_EQ(4, sNewService);   // old counters do not change.
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(2, sNewService2);  // new counters change
    EXPECT_EQ(0, sServiceDied2);

    EXPECT_EQ(6, listenerServiceCreated);  // listener associated with service name picks up info.

    // get service pointers that will be made stale later.
    auto stale_service = mediautils::getService<IServiceSingletonTest>();
    EXPECT_TRUE(stale_service);  // not stale yet.

    auto stale_service2 = mediautils::getService<aidl::IServiceSingletonTest>();
    EXPECT_TRUE(stale_service2);  // not stale yet.

    // Release the service.
    mRemoteWorker->putChar('b');
    EXPECT_EQ('b', mRemoteWorker->getChar());

    sleep(1);

    EXPECT_EQ(4, sNewService);    // old counters do not change.
    EXPECT_EQ(4, sServiceDied);
    EXPECT_EQ(2, sNewService2);   // new counters change
    EXPECT_EQ(2, sServiceDied2);

    // The service handles are now stale, verify that we can't register a death notification.
    {
        std::atomic_int32_t postDied = 0;
        // we cannot register death notification so handles are null.
        auto handle1 = mediautils::requestDeathNotification(stale_service, [&] { ++postDied; });
        EXPECT_FALSE(handle1);
        auto handle2= mediautils::requestDeathNotification(stale_service2, [&] { ++postDied; });
        EXPECT_FALSE(handle2);
        EXPECT_EQ(0, postDied);  // no callbacks issued.
    }

    // Cancel the singleton cache.
    mediautils::skipService<IServiceSingletonTest>();
    mediautils::skipService<aidl::IServiceSingletonTest>();
}

TEST_F(ServiceSingletonTests, Set) {
    // Initialize the service cache with a custom handler for set service request

    mediautils::initService<
            IServiceSingletonTest,
            SetServiceTraits<IServiceSingletonTest>>({});
    mediautils::initService<
            aidl::IServiceSingletonTest,
            SetServiceTraits<aidl::IServiceSingletonTest>>({});

    constexpr int32_t kThreads = 4;
    std::vector<std::shared_ptr<std::thread>> threads;
    std::atomic_int32_t services = 0;

    auto test1 = [&]() {
        auto service = mediautils::getService<IServiceSingletonTest>(std::chrono::seconds(2000));
        EXPECT_TRUE(service);
        EXPECT_EQ(1, SetServiceTraits<IServiceSingletonTest>::newCounter);
        ++services;
    };

    for (int32_t i = 0; i < kThreads; ++i) {
        threads.push_back(std::make_shared<std::thread>(test1));
    }
    EXPECT_EQ(0, services);
    EXPECT_EQ(0, SetServiceTraits<IServiceSingletonTest>::newCounter);
    EXPECT_EQ(0, SetServiceTraits<IServiceSingletonTest>::diedCounter);

    // The threads are blocked since there is no registered service.
    // Once we set the service, then we are able to join.
    sp<IServiceSingletonTest> testLocalService = sp<ServiceSingletonTestCpp>::make();
    mediautils::setService<IServiceSingletonTest>(testLocalService);

    for (auto& thread : threads) {
        thread->join();
    }
    threads.clear();
    EXPECT_EQ(kThreads, services);
    EXPECT_EQ(0, SetServiceTraits<IServiceSingletonTest>::diedCounter);

    // Test concurrent getService (AIDL).
    // A slightly different test, we check if we can cancel the service.

    std::atomic_int32_t services2 = 0;
    auto test2 = [&]() {
        auto service = mediautils::getService<aidl::IServiceSingletonTest>(
                std::chrono::seconds(2000));
        // we expect to cancel the request, so no service is obtained.
        EXPECT_FALSE(service);
        EXPECT_EQ(0, SetServiceTraits<aidl::IServiceSingletonTest>::newCounter);
        ++services2;
    };
    for (size_t i = 0; i < kThreads; ++i) {
        threads.push_back(std::make_shared<std::thread>(test2));
    }

    EXPECT_EQ(0, services2);
    EXPECT_EQ(0, SetServiceTraits<aidl::IServiceSingletonTest>::newCounter);

    // The threads are blocked since there is no registered service.
    // Once we cancel the service, then we are able to join.

    // Cancel the singleton cache.
    mediautils::skipService<IServiceSingletonTest>();
    mediautils::skipService<aidl::IServiceSingletonTest>();

    for (auto& thread : threads) {
        thread->join();
    }
    threads.clear();

    EXPECT_EQ(4, services2);
    EXPECT_EQ(0, SetServiceTraits<aidl::IServiceSingletonTest>::newCounter);
    EXPECT_EQ(0, SetServiceTraits<aidl::IServiceSingletonTest>::diedCounter);
}

TEST_F(ServiceSingletonTests, ConcurrentWait) {
    // Initialize the service cache with a custom handler for concurrent service request
    // testing.  ConcurrentServiceTraits has a very slow onNewService(),
    // and all concurrent service requests must wait for the onNewService to complete.

    mediautils::initService<
            IServiceSingletonTest,
            ConcurrentServiceTraits<IServiceSingletonTest>>({});
    mediautils::initService<
            aidl::IServiceSingletonTest,
            ConcurrentServiceTraits<aidl::IServiceSingletonTest>>({});

    // Spawn the service again.
    mRemoteWorker->putChar('a');
    EXPECT_EQ('a', mRemoteWorker->getChar());

    constexpr size_t kThreads = 4;
    std::vector<std::shared_ptr<std::thread>> threads;

    // Test concurrent getService.
    auto test1 = []() {
        auto service = mediautils::getService<IServiceSingletonTest>(std::chrono::seconds(2));
        EXPECT_TRUE(service);
        EXPECT_EQ(1, ConcurrentServiceTraits<IServiceSingletonTest>::newCounter);
    };
    for (size_t i = 0; i < kThreads; ++i) {
        threads.push_back(std::make_shared<std::thread>(test1));
    }
    for (auto& thread : threads) {
        thread->join();
    }
    threads.clear();

    // Test concurrent getService (AIDL).
    auto test2 = []() {
        auto service = mediautils::getService<aidl::IServiceSingletonTest>(
                std::chrono::seconds(2));
        EXPECT_TRUE(service);
        EXPECT_EQ(1, ConcurrentServiceTraits<aidl::IServiceSingletonTest>::newCounter);
    };
    for (size_t i = 0; i < kThreads; ++i) {
        threads.push_back(std::make_shared<std::thread>(test2));
    }
    for (auto& thread : threads) {
        thread->join();
    }
    threads.clear();

    EXPECT_EQ(0, ConcurrentServiceTraits<IServiceSingletonTest>::diedCounter);
    EXPECT_EQ(0, ConcurrentServiceTraits<aidl::IServiceSingletonTest>::diedCounter);

    // Release the service.
    mRemoteWorker->putChar('b');
    EXPECT_EQ('b', mRemoteWorker->getChar());

    sleep(1);

    EXPECT_EQ(1, ConcurrentServiceTraits<IServiceSingletonTest>::diedCounter);
    EXPECT_EQ(1, ConcurrentServiceTraits<aidl::IServiceSingletonTest>::diedCounter);

    // Cancel the singleton cache.
    mediautils::skipService<IServiceSingletonTest>();
    mediautils::skipService<aidl::IServiceSingletonTest>();
}
