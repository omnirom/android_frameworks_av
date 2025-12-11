/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef FRAME_CAPTURE_LAYER_H_
#define FRAME_CAPTURE_LAYER_H_

#include <com_android_graphics_libgui_flags.h>
#include <media/stagefright/foundation/ABase.h>
#include <gui/BufferItemConsumer.h>
#include <gui/IConsumerListener.h>
#include <ui/GraphicTypes.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include "gui/BufferItemConsumer.h"

namespace android {

#if !COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
class IGraphicBufferConsumer;
#endif

class GraphicBuffer;
class Rect;
class Surface;

/*
 * This class is a simple BufferQueue consumer implementation to
 * obtain a decoded buffer output from MediaCodec. The output
 * buffer is then sent to FrameCaptureProcessor to be converted
 * to sRGB properly.
 */
#if COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
struct FrameCaptureLayer : public BufferItemConsumer::FrameAvailableListener {
#else
struct FrameCaptureLayer : public ConsumerListener {
#endif
    FrameCaptureLayer();
    ~FrameCaptureLayer() = default;

    // ConsumerListener
    void onFrameAvailable(const BufferItem& /*item*/) override;
#if !COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
    void onBuffersReleased() override;
    void onSidebandStreamChanged() override;
#endif

    status_t init();

    sp<Surface> getSurface() { return mSurface; }

    status_t capture(const ui::PixelFormat reqPixelFormat,
            const Rect &sourceCrop, sp<GraphicBuffer> *outBuffer);

private:
    struct BufferLayer;
    // Note: do not hold any sp ref to GraphicBufferSource
    // GraphicBufferSource is holding an sp to us, holding any sp ref
    // to GraphicBufferSource will cause circular dependency and both
    // object will not be released.
    sp<Surface> mSurface;
#if COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_MEDIA_MIGRATION)
    sp<BufferItemConsumer> mConsumer;
#else
    sp<IGraphicBufferConsumer> mConsumer;
    std::map<int32_t, sp<GraphicBuffer> > mSlotToBufferMap;
#endif

    Mutex mLock;
    Condition mCondition;
    bool mFrameAvailable GUARDED_BY(mLock);

    status_t acquireBuffer(BufferItem *bi);
    status_t releaseBuffer(const BufferItem &bi);

    DISALLOW_EVIL_CONSTRUCTORS(FrameCaptureLayer);
};

}  // namespace android

#endif  // FRAME_CAPTURE_LAYER_H_
