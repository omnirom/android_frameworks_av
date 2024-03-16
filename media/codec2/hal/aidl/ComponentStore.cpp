/*
 * Copyright 2021 The Android Open Source Project
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
#define LOG_TAG "Codec2-ComponentStore-Aidl"
#include <android-base/logging.h>

#include <bufferpool2/ClientManager.h>
#include <codec2/aidl/Component.h>
#include <codec2/aidl/ComponentInterface.h>
#include <codec2/aidl/ComponentStore.h>
#include <codec2/aidl/ParamTypes.h>

#include <android-base/file.h>
#include <utils/Errors.h>

#include <C2PlatformSupport.h>
#include <util/C2InterfaceHelper.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>

#ifndef __ANDROID_APEX__
#include <codec2/hidl/plugin/FilterPlugin.h>
#include <dlfcn.h>
#include <C2Config.h>
#include <DefaultFilterPlugin.h>
#include <FilterWrapper.h>
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
namespace utils {

#ifndef __ANDROID_APEX__
using ::android::DefaultFilterPlugin;
using ::android::FilterWrapper;
#endif

using ::ndk::ScopedAStatus;

namespace /* unnamed */ {

struct StoreIntf : public ConfigurableC2Intf {
    StoreIntf(const std::shared_ptr<C2ComponentStore>& store)
          : ConfigurableC2Intf{store ? store->getName() : "", 0},
            mStore{store} {
    }

    virtual c2_status_t config(
            const std::vector<C2Param*> &params,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2SettingResult>> *const failures
            ) override {
        // Assume all params are blocking
        // TODO: Filter for supported params
        if (mayBlock == C2_DONT_BLOCK && params.size() != 0) {
            return C2_BLOCKING;
        }
        return mStore->config_sm(params, failures);
    }

    virtual c2_status_t query(
            const std::vector<C2Param::Index> &indices,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2Param>> *const params) const override {
        // Assume all params are blocking
        // TODO: Filter for supported params
        if (mayBlock == C2_DONT_BLOCK && indices.size() != 0) {
            return C2_BLOCKING;
        }
        return mStore->query_sm({}, indices, params);
    }

    virtual c2_status_t querySupportedParams(
            std::vector<std::shared_ptr<C2ParamDescriptor>> *const params
            ) const override {
        return mStore->querySupportedParams_nb(params);
    }

    virtual c2_status_t querySupportedValues(
            std::vector<C2FieldSupportedValuesQuery> &fields,
            c2_blocking_t mayBlock) const override {
        // Assume all params are blocking
        // TODO: Filter for supported params
        if (mayBlock == C2_DONT_BLOCK && fields.size() != 0) {
            return C2_BLOCKING;
        }
        return mStore->querySupportedValues_sm(fields);
    }

protected:
    std::shared_ptr<C2ComponentStore> mStore;
};

} // unnamed namespace

struct ComponentStore::StoreParameterCache : public ParameterCache {
    std::mutex mStoreMutex;
    ComponentStore* mStore;

    StoreParameterCache(ComponentStore* store): mStore{store} {
    }

    virtual c2_status_t validate(
            const std::vector<std::shared_ptr<C2ParamDescriptor>>& params
            ) override {
        std::scoped_lock _lock(mStoreMutex);
        return mStore ? mStore->validateSupportedParams(params) : C2_NO_INIT;
    }

    void onStoreDestroyed() {
        std::scoped_lock _lock(mStoreMutex);
        mStore = nullptr;
    }
};

ComponentStore::ComponentStore(const std::shared_ptr<C2ComponentStore>& store)
      : mConfigurable{SharedRefBase::make<CachedConfigurable>(std::make_unique<StoreIntf>(store))},
        mParameterCache{std::make_shared<StoreParameterCache>(this)},
        mStore{store} {

    std::shared_ptr<C2ComponentStore> platformStore =
        ::android::GetCodec2PlatformComponentStore();
    ::android::SetPreferredCodec2ComponentStore(store);

    // Retrieve struct descriptors
    mParamReflector = mStore->getParamReflector();

    // Retrieve supported parameters from store
    using namespace std::placeholders;
    mInit = mConfigurable->init(mParameterCache);
}

ComponentStore::~ComponentStore() {
    mParameterCache->onStoreDestroyed();
}

c2_status_t ComponentStore::status() const {
    return mInit;
}

c2_status_t ComponentStore::validateSupportedParams(
        const std::vector<std::shared_ptr<C2ParamDescriptor>>& params) {
    c2_status_t res = C2_OK;

    for (const std::shared_ptr<C2ParamDescriptor> &desc : params) {
        if (!desc) {
            // All descriptors should be valid
            res = res ? res : C2_BAD_VALUE;
            continue;
        }
        C2Param::CoreIndex coreIndex = desc->index().coreIndex();
        std::lock_guard<std::mutex> lock(mStructDescriptorsMutex);
        auto it = mStructDescriptors.find(coreIndex);
        if (it == mStructDescriptors.end()) {
            std::shared_ptr<C2StructDescriptor> structDesc =
                    mParamReflector->describe(coreIndex);
            if (!structDesc) {
                // All supported params must be described
                res = C2_BAD_INDEX;
            }
            mStructDescriptors.insert({ coreIndex, structDesc });
        }
    }
    return res;
}

std::shared_ptr<ParameterCache> ComponentStore::getParameterCache() const {
    return mParameterCache;
}

#ifndef __ANDROID_APEX__
// static
std::shared_ptr<FilterWrapper> ComponentStore::GetFilterWrapper() {
    constexpr const char kPluginPath[] = "libc2filterplugin.so";
    static std::shared_ptr<FilterWrapper> wrapper = FilterWrapper::Create(
            std::make_unique<DefaultFilterPlugin>(kPluginPath));
    return wrapper;
}
#endif

// Methods from ::aidl::android::hardware::media::c2::IComponentStore
ScopedAStatus ComponentStore::createComponent(
        const std::string& name,
        const std::shared_ptr<IComponentListener>& listener,
        const std::shared_ptr<IClientManager>& pool,
        std::shared_ptr<IComponent> *component) {

    std::shared_ptr<C2Component> c2component;
    c2_status_t status =
            mStore->createComponent(name, &c2component);

    if (status == C2_OK) {
#ifndef __ANDROID_APEX__
        c2component = GetFilterWrapper()->maybeWrapComponent(c2component);
#endif
        onInterfaceLoaded(c2component->intf());
        std::shared_ptr<Component> comp =
            SharedRefBase::make<Component>(c2component, listener, ref<ComponentStore>(), pool);
        *component = comp;
        if (!component) {
            status = C2_CORRUPTED;
        } else {
            reportComponentBirth(comp.get());
            if (comp->status() != C2_OK) {
                status = comp->status();
            } else {
                comp->initListener(comp);
                if (comp->status() != C2_OK) {
                    status = comp->status();
                }
            }
        }
    }
    if (status == C2_OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(status);
}

ScopedAStatus ComponentStore::createInterface(
        const std::string& name,
        std::shared_ptr<IComponentInterface> *intf) {
    std::shared_ptr<C2ComponentInterface> c2interface;
    c2_status_t res = mStore->createInterface(name, &c2interface);
    if (res == C2_OK) {
#ifndef __ANDROID_APEX__
        c2interface = GetFilterWrapper()->maybeWrapInterface(c2interface);
#endif
        onInterfaceLoaded(c2interface);
        *intf = SharedRefBase::make<ComponentInterface>(c2interface, mParameterCache);
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(res);
}

ScopedAStatus ComponentStore::listComponents(
        std::vector<IComponentStore::ComponentTraits> *traits) {
    std::vector<std::shared_ptr<const C2Component::Traits>> c2traits =
            mStore->listComponents();
    traits->resize(c2traits.size());
    size_t ix = 0;
    for (const std::shared_ptr<const C2Component::Traits> &c2trait : c2traits) {
        if (c2trait) {
            if (ToAidl(&traits->at(ix), *c2trait)) {
                ++ix;
            } else {
                break;
            }
        }
    }
    traits->resize(ix);
    return ScopedAStatus::ok();
}

void ComponentStore::onInterfaceLoaded(const std::shared_ptr<C2ComponentInterface> &intf) {
    // invalidate unsupported struct descriptors if a new interface is loaded as it may have
    // exposed new descriptors
    std::lock_guard<std::mutex> lock(mStructDescriptorsMutex);
    if (!mLoadedInterfaces.count(intf->getName())) {
        mUnsupportedStructDescriptors.clear();
        mLoadedInterfaces.emplace(intf->getName());
    }
}

ScopedAStatus ComponentStore::getStructDescriptors(
        const std::vector<int32_t>& indices,
        std::vector<StructDescriptor> *descriptors) {
    descriptors->resize(indices.size());
    size_t dstIx = 0;
    int32_t res = Status::OK;
    for (size_t srcIx = 0; srcIx < indices.size(); ++srcIx) {
        std::lock_guard<std::mutex> lock(mStructDescriptorsMutex);
        const C2Param::CoreIndex coreIndex =
            C2Param::CoreIndex(uint32_t(indices[srcIx])).coreIndex();
        const auto item = mStructDescriptors.find(coreIndex);
        if (item == mStructDescriptors.end()) {
            // not in the cache, and not known to be unsupported, query local reflector
            if (!mUnsupportedStructDescriptors.count(coreIndex)) {
                std::shared_ptr<C2StructDescriptor> structDesc =
                    mParamReflector->describe(coreIndex);
                if (!structDesc) {
                    mUnsupportedStructDescriptors.emplace(coreIndex);
                } else {
                    mStructDescriptors.insert({ coreIndex, structDesc });
                    if (ToAidl(&descriptors->at(dstIx), *structDesc)) {
                        ++dstIx;
                        continue;
                    }
                    res = Status::CORRUPTED;
                    break;
                }
            }
            res = Status::NOT_FOUND;
        } else if (item->second) {
            if (ToAidl(&descriptors->at(dstIx), *item->second)) {
                ++dstIx;
                continue;
            }
            res = Status::CORRUPTED;
            break;
        } else {
            res = Status::NO_MEMORY;
            break;
        }
    }
    descriptors->resize(dstIx);
    if (res == Status::OK) {
        return ScopedAStatus::ok();
    }
    return ScopedAStatus::fromServiceSpecificError(res);
}

ScopedAStatus ComponentStore::getPoolClientManager(
        std::shared_ptr<IClientManager> *manager) {
    using ::aidl::android::hardware::media::bufferpool2::implementation::ClientManager;
    *manager = ClientManager::getInstance();
    return ScopedAStatus::ok();
}

ScopedAStatus ComponentStore::copyBuffer(const Buffer& src, const Buffer& dst) {
    // TODO implement
    (void)src;
    (void)dst;
    return ScopedAStatus::fromServiceSpecificError(Status::OMITTED);
}

ScopedAStatus ComponentStore::getConfigurable(
        std::shared_ptr<IConfigurable> *configurable) {
    *configurable = mConfigurable;
    return ScopedAStatus::ok();
}

// Called from createComponent() after a successful creation of `component`.
void ComponentStore::reportComponentBirth(Component* component) {
    ComponentStatus componentStatus;
    componentStatus.c2Component = component->mComponent;
    componentStatus.birthTime = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(mComponentRosterMutex);
    mComponentRoster.emplace(component, componentStatus);
}

// Called from within the destructor of `component`. No virtual function calls
// are made on `component` here.
void ComponentStore::reportComponentDeath(Component* component) {
    std::lock_guard<std::mutex> lock(mComponentRosterMutex);
    mComponentRoster.erase(component);
}

// Dumps component traits.
std::ostream& ComponentStore::dump(
        std::ostream& out,
        const std::shared_ptr<const C2Component::Traits>& comp) {

    constexpr const char indent[] = "    ";

    out << indent << "name: " << comp->name << std::endl;
    out << indent << "domain: " << comp->domain << std::endl;
    out << indent << "kind: " << comp->kind << std::endl;
    out << indent << "rank: " << comp->rank << std::endl;
    out << indent << "mediaType: " << comp->mediaType << std::endl;
    out << indent << "aliases:";
    for (const auto& alias : comp->aliases) {
        out << ' ' << alias;
    }
    out << std::endl;

    return out;
}

// Dumps component status.
std::ostream& ComponentStore::dump(
        std::ostream& out,
        ComponentStatus& compStatus) {

    constexpr const char indent[] = "    ";

    // Print birth time.
    std::chrono::milliseconds ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                compStatus.birthTime.time_since_epoch());
    std::time_t birthTime = std::chrono::system_clock::to_time_t(
            compStatus.birthTime);
    std::tm tm = *std::localtime(&birthTime);
    out << indent << "Creation time: "
        << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() % 1000
        << std::endl;

    // Print name and id.
    std::shared_ptr<C2ComponentInterface> intf = compStatus.c2Component->intf();
    if (!intf) {
        out << indent << "Unknown component -- null interface" << std::endl;
        return out;
    }
    out << indent << "Name: " << intf->getName() << std::endl;
    out << indent << "Id: " << intf->getId() << std::endl;

    return out;
}

// Dumps information when lshal is called.
binder_status_t ComponentStore::dump(
        int fd, [[maybe_unused]] const char** args, [[maybe_unused]] uint32_t numArgs) {
    LOG(INFO) << "debug -- dumping...";
    std::ostringstream out;

    { // Populate "out".

        constexpr const char indent[] = "  ";

        // Show name.
        out << "Beginning of dump -- C2ComponentStore: "
                << mStore->getName() << std::endl << std::endl;

        // Retrieve the list of supported components.
        std::vector<std::shared_ptr<const C2Component::Traits>> traitsList =
                mStore->listComponents();

        // Dump the traits of supported components.
        out << indent << "Supported components:" << std::endl << std::endl;
        if (traitsList.size() == 0) {
            out << indent << indent << "NONE" << std::endl << std::endl;
        } else {
            for (const auto& traits : traitsList) {
                dump(out, traits) << std::endl;
            }
        }

        // Dump active components.
        {
            out << indent << "Active components:" << std::endl << std::endl;
            std::lock_guard<std::mutex> lock(mComponentRosterMutex);
            if (mComponentRoster.size() == 0) {
                out << indent << indent << "NONE" << std::endl << std::endl;
            } else {
                for (auto& pair : mComponentRoster) {
                    dump(out, pair.second) << std::endl;
                }
            }
        }

        out << "End of dump -- C2ComponentStore: "
                << mStore->getName() << std::endl;
    }

    if (!::android::base::WriteStringToFd(out.str(), fd)) {
        PLOG(WARNING) << "debug -- dumping failed -- write()";
    } else {
        LOG(INFO) << "debug -- dumping succeeded";
    }
    return STATUS_OK;
}

} // namespace utils
} // namespace c2
} // namespace media
} // namespace hardware
} // namespace android
} // namespace aidl
