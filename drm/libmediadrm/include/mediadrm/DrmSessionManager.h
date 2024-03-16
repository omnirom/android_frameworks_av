/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef DRM_SESSION_MANAGER_H_

#define DRM_SESSION_MANAGER_H_

#include <aidl/android/media/IResourceManagerClient.h>
#include <aidl/android/media/IResourceManagerService.h>
#include <android/binder_auto_utils.h>
#include <media/stagefright/foundation/ABase.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include <future>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace android {

class DrmSessionManagerTest;

using aidl::android::media::IResourceManagerClient;
using aidl::android::media::IResourceManagerService;
using aidl::android::media::MediaResourceParcel;

bool isEqualSessionId(const Vector<uint8_t> &sessionId1, const Vector<uint8_t> &sessionId2);

struct SessionInfo {
    pid_t pid;
    uid_t uid;
    int64_t clientId;
    std::shared_ptr<IResourceManagerClient> drm;
    int64_t resourceValue;

};

typedef std::map<std::vector<uint8_t>, SessionInfo> SessionInfoMap;

struct DrmSessionManager : public RefBase {
    static sp<DrmSessionManager> Instance();

    DrmSessionManager();
    explicit DrmSessionManager(const std::shared_ptr<IResourceManagerService> &service);

    void addSession(int pid,
            const std::shared_ptr<IResourceManagerClient>& drm,
            const Vector<uint8_t>& sessionId);
    void useSession(const Vector<uint8_t>& sessionId);
    void removeSession(const Vector<uint8_t>& sessionId);
    bool reclaimSession(int callingPid);

    // inspection APIs
    size_t getSessionCount() const;
    bool containsSession(const Vector<uint8_t>& sessionId) const;

protected:
    virtual ~DrmSessionManager();

private:
    status_t init();

    // To set up the binder interface with the resource manager service.
    void getResourceManagerService() {
        Mutex::Autolock lock(mLock);
        getResourceManagerService_l();
    }
    void getResourceManagerService_l();

    // To add/register all the resources currently added/registered with
    // the ResourceManagerService.
    // This function will be called right after the death of the Resource
    // Manager to make sure that the newly started ResourceManagerService
    // knows about the current resource usage.
    void reRegisterAllResources_l();

    // For binder death handling
    static void ResourceManagerServiceDied(void* cookie);
    static void BinderUnlinkedCallback(void* cookie);
    void binderDied();

    // BinderDiedContext defines the cookie that is passed as DeathRecipient.
    // Since this can maintain more context than a raw pointer, we can
    // validate the scope of DrmSessionManager,
    // before deferencing it upon the binder death.
    struct BinderDiedContext {
        wp<DrmSessionManager> mDrmSessionManager;
    };

    std::shared_ptr<IResourceManagerService> mService = nullptr;
    mutable Mutex mLock;
    SessionInfoMap mSessionMap;
    bool mBinderDied = false;
    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
    /**
     * Reconnecting with the ResourceManagerService, after its binder interface dies,
     * is done asynchronously. It will also make sure that, all the resources
     * asssociated with this DrmSessionManager are added with the new instance
     * of the ResourceManagerService to persist the state of resources.
     * We must store the reference of the furture to guarantee real asynchronous operation.
     */
    std::future<void> mGetServiceFuture;

    DISALLOW_EVIL_CONSTRUCTORS(DrmSessionManager);
};

}  // namespace android

#endif  // DRM_SESSION_MANAGER_H_
