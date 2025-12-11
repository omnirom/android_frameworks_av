/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef PERSISTENT_SURFACE_H_

#define PERSISTENT_SURFACE_H_

#include <android/binder_auto_utils.h>
#include <android/binder_libbinder.h>
#include <binder/Parcel.h>
#include <hidl/HidlSupport.h>
#include <hidl/HybridInterface.h>
#include <gui/Flags.h> // Remove with MediaSurfaceType
#include <gui/Surface.h>
#include <gui/view/Surface.h>
#include <media/stagefright/foundation/ABase.h>

namespace android {

struct PersistentSurface : public RefBase {
    enum SurfaceType : int {
        TYPE_UNKNOWN = 0,
        TYPE_HIDLSOURCE,    // android::hardware::media::omx::V1_0::IGraphicBufferSource
        TYPE_AIDLSOURCE,    // aidl::android::media::IAidlGraphicBufferSource
        TYPE_INPUTSURFACE,  // aidl::android::hardware::media::c2::IInputSurface
    };

    PersistentSurface() : mType(TYPE_UNKNOWN) {}

    // create a persistent surface in HIDL
    PersistentSurface(
            const sp<MediaSurfaceType>& surface,
            const sp<hidl::base::V1_0::IBase>& hidlTarget) :
        mType(TYPE_HIDLSOURCE),
        mSurface(surface),
        mHidlGraphicBufferSource(hidlTarget),
        mAidlGraphicBufferSource(nullptr),
        mHalInputSurface(nullptr) {}

    // create a persistent surface in AIDL (or HAL InputSurface)
    PersistentSurface(
            const SurfaceType type,
            const sp<MediaSurfaceType>& surface,
            const ::ndk::SpAIBinder& aidlTarget) :
        mType(type),
        mSurface(surface),
        mHidlGraphicBufferSource(nullptr),
        mAidlGraphicBufferSource(type == TYPE_AIDLSOURCE ? aidlTarget : nullptr),
        mHalInputSurface(type == TYPE_INPUTSURFACE ? aidlTarget : nullptr) {}

    sp<MediaSurfaceType> getSurface() const {
        return mSurface;
    }

    SurfaceType getType() const {
        return mType;
    }

    sp<hidl::base::V1_0::IBase> getHidlTarget() const {
        return mType == TYPE_HIDLSOURCE ? mHidlGraphicBufferSource : nullptr;
    }

    ::ndk::SpAIBinder getAidlTarget() const {
        return mType == TYPE_AIDLSOURCE ? mAidlGraphicBufferSource : nullptr;
    }

    ::ndk::SpAIBinder getHalInputSurface() const {
        return mType == TYPE_INPUTSURFACE ? mHalInputSurface : nullptr;
    }

    status_t writeToParcel(Parcel *parcel) const {
#if COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
        parcel->writeParcelable(view::Surface::fromSurface(mSurface));
#else
        parcel->writeStrongBinder(IInterface::asBinder(mSurface));
#endif
        switch(mType) {
            case TYPE_HIDLSOURCE: {
                parcel->writeInt32(TYPE_HIDLSOURCE);
                HalToken token;
                bool result = createHalToken(mHidlGraphicBufferSource, &token);
                parcel->writeBool(result);
                if (result) {
                    parcel->writeByteArray(token.size(), token.data());
                }
            }
            break;
            case TYPE_AIDLSOURCE: {
                parcel->writeInt32(TYPE_AIDLSOURCE);
                AIBinder *binder = mAidlGraphicBufferSource.get();
                if (binder != nullptr) {
                    ::android::sp<::android::IBinder> intf = AIBinder_toPlatformBinder(binder);
                    if (intf) {
                        parcel->writeBool(true);
                        parcel->writeStrongBinder(intf);
                        break;
                    }
                }
                parcel->writeBool(false);
            }
            break;
            case TYPE_INPUTSURFACE: {
                parcel->writeInt32(TYPE_INPUTSURFACE);
                AIBinder *binder = mHalInputSurface.get();
                if (binder != nullptr) {
                    ::android::sp<::android::IBinder> intf = AIBinder_toPlatformBinder(binder);
                    if (intf) {
                        parcel->writeBool(true);
                        parcel->writeStrongBinder(intf);
                        break;
                    }
                }
                parcel->writeBool(false);
            }
            break;
            default: {
                parcel->writeInt32(TYPE_UNKNOWN);
            }
            break;
        }
        return NO_ERROR;
    }

    status_t readFromParcel(const Parcel *parcel) {
#if COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
        view::Surface surface;
        parcel->readParcelable(&surface);
        mSurface = surface.toSurface();
#else
        mSurface = interface_cast<IGraphicBufferProducer>(
                parcel->readStrongBinder());
#endif
        mHidlGraphicBufferSource.clear();
        mAidlGraphicBufferSource.set(nullptr);
        mHalInputSurface.set(nullptr);

        SurfaceType type = static_cast<SurfaceType>(parcel->readInt32());
        switch (type) {
            case TYPE_HIDLSOURCE: {
                mType = TYPE_HIDLSOURCE;
                bool haveHidlSource = parcel->readBool();
                if (haveHidlSource) {
                    std::vector<uint8_t> tokenVector;
                    parcel->readByteVector(&tokenVector);
                    HalToken token = HalToken(tokenVector);
                    mHidlGraphicBufferSource = retrieveHalInterface(token);
                    deleteHalToken(token);
                }
            }
            break;
            case TYPE_AIDLSOURCE: {
                mType = TYPE_AIDLSOURCE;
                bool haveAidlSource = parcel->readBool();
                if (haveAidlSource) {
                    ::android::sp<::android::IBinder> intf = parcel->readStrongBinder();
                    AIBinder *ndkBinder = AIBinder_fromPlatformBinder(intf);
                    if (ndkBinder) {
                        mAidlGraphicBufferSource.set(ndkBinder);
                    }
                }
            }
            break;
            case TYPE_INPUTSURFACE: {
                mType = TYPE_INPUTSURFACE;
                bool haveHalInputSurface = parcel->readBool();
                if (haveHalInputSurface) {
                    ::android::sp<::android::IBinder> intf = parcel->readStrongBinder();
                    AIBinder *ndkBinder = AIBinder_fromPlatformBinder(intf);
                    if (ndkBinder) {
                        mHalInputSurface.set(ndkBinder);
                    }
                }
            }
            break;
            default: {
                mType = TYPE_UNKNOWN;
            }
            break;
        }
        return NO_ERROR;
    }

private:
    SurfaceType mType;

    sp<MediaSurfaceType> mSurface;
    // Either one of the below three is valid according to mType.
    sp<hidl::base::V1_0::IBase> mHidlGraphicBufferSource;
    ::ndk::SpAIBinder mAidlGraphicBufferSource;
    ::ndk::SpAIBinder mHalInputSurface;

    DISALLOW_EVIL_CONSTRUCTORS(PersistentSurface);
};

}  // namespace android

#endif  // PERSISTENT_SURFACE_H_
