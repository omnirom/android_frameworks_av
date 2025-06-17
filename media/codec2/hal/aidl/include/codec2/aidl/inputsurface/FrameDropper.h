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

#include <utils/Errors.h>

#include <media/stagefright/foundation/ABase.h>

#include <C2.h>

namespace aidl::android::hardware::media::c2::implementation {

/**
 * The class decides whether to drop a frame or not for InputSurface and
 * InputSurfaceConnection.
 */
struct FrameDropper {
    FrameDropper();

    ~FrameDropper();

    /**
     * Sets max frame rate, which is based on for deciding frame drop.
     *
     * @param[in] maxFrameRate  negative value means there is no drop
     *                          zero value is ignored
     */
    void setMaxFrameRate(float maxFrameRate);

    /** Returns false if max frame rate has not been set via setMaxFrameRate. */
    bool shouldDrop(int64_t timeUs);

    /** Returns true if all frame drop logic should be disabled. */
    bool disabled() { return (mMinIntervalUs == -1ll); }

private:
    int64_t mDesiredMinTimeUs;
    int64_t mMinIntervalUs;

    DISALLOW_EVIL_CONSTRUCTORS(FrameDropper);
};

}  // namespace aidl::android::hardware::media::c2::implementation
