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

#ifndef ANDROID_SERVERS_CAMERA_CAMERAFLASHLIGHT_H
#define ANDROID_SERVERS_CAMERA_CAMERAFLASHLIGHT_H

#include <string>
#include <gui/GLConsumer.h>
#include <gui/Surface.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include "common/CameraProviderManager.h"
#include "common/CameraDeviceBase.h"

namespace android {

/**
 * FlashControlBase is a base class for flash control. It defines the functions
 * that a flash control for each camera module/device version should implement.
 */
class FlashControlBase : public virtual VirtualLightRefBase {
    public:
        virtual ~FlashControlBase();

        // Whether a camera device has a flash unit. Calling this function may
        // cause the torch mode to be turned off in HAL v1 devices. If
        // previously-on torch mode is turned off,
        // callbacks.torch_mode_status_change() should be invoked.
        virtual status_t hasFlashUnit(const std::string& cameraId,
                    bool *hasFlash) = 0;

        // set the torch mode to on or off.
        virtual status_t setTorchMode(const std::string& cameraId,
                    bool enabled) = 0;

        // Change the brightness level of the torch. If the torch is OFF and
        // torchStrength >= 1, then the torch will also be turned ON.
        virtual status_t turnOnTorchWithStrengthLevel(const std::string& cameraId,
                    int32_t torchStrength) = 0;

        // Returns the torch strength level.
        virtual status_t getTorchStrengthLevel(const std::string& cameraId,
                int32_t* torchStrength) = 0;
};

/**
 * CameraFlashlight can be used by camera service to control flashflight.
 */
class CameraFlashlight : public virtual VirtualLightRefBase {
    public:
        CameraFlashlight(sp<CameraProviderManager> providerManager,
                CameraProviderManager::StatusListener* callbacks);
        virtual ~CameraFlashlight();

        // Find all flash units. This must be called before other methods. All
        // camera devices must be closed when it's called because HAL v1 devices
        // need to be opened to query available flash modes.
        status_t findFlashUnits();

        // Whether a camera device has a flash unit. Before findFlashUnits() is
        // called, this function always returns false.
        bool hasFlashUnit(const std::string& cameraId);

        // set the torch mode to on or off.
        status_t setTorchMode(const std::string& cameraId, bool enabled);

        // Change the torch strength level of the flash unit in torch mode.
        status_t turnOnTorchWithStrengthLevel(const std::string& cameraId, int32_t torchStrength);

        // Get the torch strength level
        status_t getTorchStrengthLevel(const std::string& cameraId, int32_t* torchStrength);

        // Notify CameraFlashlight that camera service is going to open a camera
        // device. CameraFlashlight will free the resources that may cause the
        // camera open to fail. Camera service must call this function before
        // opening a camera device.
        status_t prepareDeviceOpen(const std::string& cameraId);

        // Notify CameraFlashlight that camera service has closed a camera
        // device. CameraFlashlight may invoke callbacks for torch mode
        // available depending on the implementation.
        status_t deviceClosed(const std::string& cameraId);

    private:
        // create flashlight control based on camera module API and camera
        // device API versions.
        status_t createFlashlightControl(const std::string& cameraId);

        // mLock should be locked.
        bool hasFlashUnitLocked(const std::string& cameraId);

        // Check if flash control is in backward compatible mode (simulated torch API by
        // opening cameras)
        bool isBackwardCompatibleMode(const std::string& cameraId);

        sp<FlashControlBase> mFlashControl;

        sp<CameraProviderManager> mProviderManager;

        CameraProviderManager::StatusListener* mCallbacks;
        SortedVector<std::string> mOpenedCameraIds;

        // camera id -> if it has a flash unit
        KeyedVector<std::string, bool> mHasFlashlightMap;
        bool mFlashlightMapInitialized;

        Mutex mLock; // protect CameraFlashlight API
};

/**
 * Flash control for camera provider v2.4 and above.
 */
class ProviderFlashControl : public FlashControlBase {
    public:
        ProviderFlashControl(sp<CameraProviderManager> providerManager);
        virtual ~ProviderFlashControl();

        // FlashControlBase
        status_t hasFlashUnit(const std::string& cameraId, bool *hasFlash);
        status_t setTorchMode(const std::string& cameraId, bool enabled);
        status_t turnOnTorchWithStrengthLevel(const std::string& cameraId, int32_t torchStrength);
        status_t getTorchStrengthLevel(const std::string& cameraId, int32_t* torchStrength);

    private:
        sp<CameraProviderManager> mProviderManager;

        Mutex mLock;
};

} // namespace android

#endif
