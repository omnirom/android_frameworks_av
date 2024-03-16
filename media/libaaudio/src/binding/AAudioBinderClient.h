/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ANDROID_AAUDIO_AAUDIO_BINDER_CLIENT_H
#define ANDROID_AAUDIO_AAUDIO_BINDER_CLIENT_H

#include <mutex>

#include <utils/RefBase.h>
#include <utils/Singleton.h>

#include <aaudio/AAudio.h>
#include <binder/IInterface.h>

#include "aaudio/BnAAudioClient.h"
#include "aaudio/IAAudioService.h"
#include "AAudioServiceInterface.h"
#include "binding/AAudioBinderAdapter.h"
#include "binding/AAudioStreamRequest.h"
#include "binding/AudioEndpointParcelable.h"
#include "core/AAudioStreamParameters.h"

/**
 * Implements the AAudioServiceInterface by talking to the service through Binder.
 */

namespace aaudio {

class AAudioBinderClient : public virtual android::RefBase
        , public AAudioServiceInterface
        , public android::Singleton<AAudioBinderClient> {

public:

    AAudioBinderClient();

    virtual ~AAudioBinderClient();

    void registerClient(const android::sp<IAAudioClient>& client __unused) override {}

    /**
     * @param request info needed to create the stream
     * @param configuration contains resulting information about the created stream
     * @return an object for aaudio handle information, which includes the connected
     *         aaudio service lifetime id to recognize the connected aaudio service
     *         and aaudio handle to recognize the stream. If an error occurs, the
     *         aaudio handle will be set as the negative error.
     */
    AAudioHandleInfo openStream(const AAudioStreamRequest &request,
                                AAudioStreamConfiguration &configurationOutput) override;

    aaudio_result_t closeStream(const AAudioHandleInfo& streamHandleInfo) override;

    /* Get an immutable description of the in-memory queues
    * used to communicate with the underlying HAL or Service.
    */
    aaudio_result_t getStreamDescription(const AAudioHandleInfo& streamHandleInfo,
                                         AudioEndpointParcelable &endpointOut) override;

    /**
     * Start the flow of data.
     * This is asynchronous. When complete, the service will send a STARTED event.
     */
    aaudio_result_t startStream(const AAudioHandleInfo& streamHandleInfo) override;

    /**
     * Stop the flow of data such that start() can resume without loss of data.
     * This is asynchronous. When complete, the service will send a PAUSED event.
     */
    aaudio_result_t pauseStream(const AAudioHandleInfo& streamHandleInfo) override;

    aaudio_result_t stopStream(const AAudioHandleInfo& streamHandleInfo) override;

    /**
     *  Discard any data held by the underlying HAL or Service.
     * This is asynchronous. When complete, the service will send a FLUSHED event.
     */
    aaudio_result_t flushStream(const AAudioHandleInfo& streamHandleInfo) override;

    /**
     * Manage the specified thread as a low latency audio thread.
     * TODO Consider passing this information as part of the startStream() call.
     */
    aaudio_result_t registerAudioThread(const AAudioHandleInfo& streamHandleInfo,
                                        pid_t clientThreadId,
                                        int64_t periodNanoseconds) override;

    aaudio_result_t unregisterAudioThread(const AAudioHandleInfo& streamHandleInfo,
                                          pid_t clientThreadId) override;

    aaudio_result_t startClient(const AAudioHandleInfo& streamHandleInfo __unused,
                                const android::AudioClient& client __unused,
                                const audio_attributes_t *attr __unused,
                                audio_port_handle_t *clientHandle __unused) override {
        return AAUDIO_ERROR_UNAVAILABLE;
    }

    aaudio_result_t stopClient(const AAudioHandleInfo& streamHandleInfo __unused,
                               audio_port_handle_t clientHandle __unused)  override {
        return AAUDIO_ERROR_UNAVAILABLE;
    }

    aaudio_result_t exitStandby(const AAudioHandleInfo& streamHandleInfo,
                                AudioEndpointParcelable &endpointOut) override;

    void onStreamChange(aaudio_handle_t /*handle*/, int32_t /*opcode*/, int32_t /*value*/) {
        // TODO This is just a stub so we can have a client Binder to pass to the service.
        // TODO Implemented in a later CL.
        ALOGW("onStreamChange called!");
    }

    int32_t getServiceLifetimeId() const {
        return mAAudioClient->getServiceLifetimeId();
    }

    class AAudioClient : public android::IBinder::DeathRecipient, public BnAAudioClient {
    public:
        explicit AAudioClient(const android::wp<AAudioBinderClient>& aaudioBinderClient)
                : mBinderClient(aaudioBinderClient) {
        }

        // implement DeathRecipient
        virtual void binderDied(const android::wp<android::IBinder>& who __unused) {
            mServiceLifetimeId++;
            android::sp<AAudioBinderClient> client = mBinderClient.promote();
            if (client.get() != nullptr) {
                client->dropAAudioService();
            }
            ALOGW("AAudio service binderDied()!");
        }

        // implement BnAAudioClient
        android::binder::Status onStreamChange(int32_t handle, int32_t opcode, int32_t value) {
            static_assert(std::is_same_v<aaudio_handle_t, int32_t>);
            android::sp<AAudioBinderClient> client = mBinderClient.promote();
            if (client.get() != nullptr) {
                client->onStreamChange(handle, opcode, value);
            }
            return android::binder::Status::ok();
        }

        int32_t getServiceLifetimeId() const {
            return mServiceLifetimeId.load();
        }
    private:
        android::wp<AAudioBinderClient> mBinderClient;
        std::atomic_int                 mServiceLifetimeId{0};
    };

    // This adapter is used to convert the binder interface (delegate) to the AudioServiceInterface
    // conventions (translating between data types and respective parcelables, translating error
    // codes and calling conventions).
    // The adapter also owns the underlying service object and is responsible to unlink its death
    // listener when destroyed.
    class Adapter : public AAudioBinderAdapter {
    public:
        Adapter(const android::sp<IAAudioService>& delegate,
                android::sp<AAudioClient> aaudioClient,
                int32_t serviceLifetimeId)
                : AAudioBinderAdapter(delegate.get(), serviceLifetimeId),
                  mDelegate(delegate),
                  mAAudioClient(std::move(aaudioClient)) {}

        virtual ~Adapter() {
            if (mDelegate != nullptr) {
                android::IInterface::asBinder(mDelegate)->unlinkToDeath(mAAudioClient);
            }
        }

        // This should never be called (call is rejected at the AudioBinderClient level).
        aaudio_result_t startClient(const AAudioHandleInfo& streamHandle __unused,
                                    const android::AudioClient& client __unused,
                                    const audio_attributes_t* attr __unused,
                                    audio_port_handle_t* clientHandle __unused) override {
            LOG_ALWAYS_FATAL("Shouldn't get here");
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        // This should never be called (call is rejected at the AudioBinderClient level).
        aaudio_result_t stopClient(const AAudioHandleInfo& streamHandle __unused,
                                   audio_port_handle_t clientHandle __unused) override {
            LOG_ALWAYS_FATAL("Shouldn't get here");
            return AAUDIO_ERROR_UNAVAILABLE;
        }

    private:
        android::sp<IAAudioService> mDelegate;
        android::sp<AAudioClient> mAAudioClient;
    };

private:
    android::Mutex                          mServiceLock;
    std::shared_ptr<AAudioServiceInterface> mAdapter PT_GUARDED_BY(mServiceLock);
    android::sp<AAudioClient>               mAAudioClient;

    std::shared_ptr<AAudioServiceInterface> getAAudioService() EXCLUDES(mServiceLock);

    void dropAAudioService() EXCLUDES(mServiceLock);

};


} /* namespace aaudio */

#endif //ANDROID_AAUDIO_AAUDIO_BINDER_CLIENT_H
