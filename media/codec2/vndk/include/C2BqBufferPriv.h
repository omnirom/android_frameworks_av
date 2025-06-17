/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef STAGEFRIGHT_CODEC2_BQ_BUFFER_PRIV_H_
#define STAGEFRIGHT_CODEC2_BQ_BUFFER_PRIV_H_

#include <android/hardware/graphics/bufferqueue/2.0/IGraphicBufferProducer.h>

#include <C2Buffer.h>
#include <C2BlockInternal.h>

#include <atomic>
#include <functional>
#include <mutex>

namespace android {
class GraphicBuffer;
}  // namespace android

/**
 * BufferQueue based BlockPool.
 *
 * This creates graphic blocks from BufferQueue. BufferQueue here is HIDL-ized IGBP.
 * HIDL-ized IGBP enables vendor HAL to call IGBP interfaces via HIDL over process boundary.
 * HIDL-ized IGBP is called as HGBP. HGBP had been used from multiple places in android,
 * but now this is the only place HGBP is still used.
 *
 * Initially there is no HGBP configured, in the case graphic blocks are allocated
 * from gralloc directly upon \fetchGraphicBlock() requests.
 *
 * HGBP can be configured as null as well, in the case graphic blocks are allocated
 * from gralloc directly upon \fetchGraphicBlock() requests.
 *
 * If a specific HGBP is configured, the HGBP acts as an allocator for creating graphic blocks.
 *
 *
 * HGBP/IGBP and the BlockPool
 *
 * GraphicBuffer(s) from BufferQueue(IGBP/IGBC) are based on slot id.
 * A created GraphicBuffer occupies a slot(so the GraphicBuffer has a slot-id).
 * A GraphicBuffer is produced and consumed and recyled based on the slot-id
 * w.r.t. BufferQueue.
 *
 * HGBP::dequeueBuffer() returns a slot id where the slot has an available GraphicBuffer.
 * If it is necessary, HGBP allocates a new GraphicBuffer to the slot and indicates
 * that a new buffer is allocated as return flag.
 * To retrieve the GraphicBuffer, HGBP::requestBuffer() along with the slot id
 * is required. In order to save HGBP remote calls, the blockpool caches the
 * allocated GraphicBuffer(s) along with the slot information.
 *
 * The blockpool provides C2GraphicBlock upon \fetchGraphicBlock().
 * The C2GraphicBlock has a native handle, which is extracted from a GraphicBuffer
 * and then cloned for independent life-cycle with the GraphicBuffer. The GraphicBuffer
 * is allocated by HGBP::dequeueBuffer() and retrieved by HGBP::requestBuffer()
 * if there is a HGBP configured.
 *
 *
 * Life-cycle of C2GraphicBlock
 *
 * The decoder HAL writes a decoded frame into C2GraphicBlock. Upon
 * completion, the component sends the block to the client in the remote process
 * (i.e. to MediaCodec). The remote process renders the frame into the output surface
 * via IGBP::queueBuffer() (Note: this is not hidlized.).
 *
 * If the decoder HAL destroys the C2GraphicBlock without transferring to the
 * client, the destroy request goes to the BlockPool. Then
 * the BlockPool free the associated GraphicBuffer from a slot to
 * HGBP in order to recycle via HGBP::cancelBuffer().
 *
 *
 * Clearing the Cache(GraphicBuffer)
 *
 * When the output surface is switched to a new surface, The GraphicBuffers from
 * the old surface is either migrated or cleared.
 *
 * The GraphicBuffer(s) still in use are migrated to a new surface during
 * configuration via HGBP::attachBuffer(). The GraphicBuffer(s) not in use are
 * cleared from the cache inside the BlockPool.
 *
 * When the surface is switched to a null surface, all the
 * GraphicBuffers in the cache are cleared.
 *
 *
 * Workaround w.r.t. b/322731059 (Deferring cleaning the cache)
 *
 * Some vendor devices have issues with graphic buffer lifecycle management,
 * where the graphic buffers get released even when the cloned native handles
 * in the remote process are not closed yet. This issue led to rare crashes
 * for those devices when the cache is cleared early.
 *
 * We workarounded the crash by deferring the cleaning of the cache.
 * The workaround is not enabled by default, and can be enabled via a
 * system property as shown below:
 *
 *        'debug.codec2.bqpool_dealloc_after_stop' = 1
 *
 * Configuring the debug flag will call \::setDeferDeallocationAfterStop()
 * after the blockpool creation. This will enable the deferring.
 *
 * After enabling the deferring, clearing the GraphicBuffer is delayed until
 *  1) \::clearDeferredBlocks() is called.
 *        Typically after HAL processes stop() request.
 *  2) Or a new ::fetchGraphicBlock() is called.
 *
 *  Since the deferring will delay the deallocation, the deferring will result
 *  in more memory consumption during the brief period.
 */
class C2BufferQueueBlockPool : public C2BlockPool {
public:
    C2BufferQueueBlockPool(const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId);

    virtual ~C2BufferQueueBlockPool() override;

    virtual C2Allocator::id_t getAllocatorId() const override {
        return mAllocator->getId();
    };

    virtual local_id_t getLocalId() const override {
        return mLocalId;
    };

    virtual c2_status_t fetchGraphicBlock(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override;

    virtual c2_status_t fetchGraphicBlock(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */,
            C2Fence *fence /* nonnull */) override;

    typedef std::function<void(uint64_t producer, int32_t slot, int64_t nsecs)> OnRenderCallback;

    /**
     * Sets render callback.
     *
     * \param renderCallbak callback to call for all dequeue buffer.
     */
    virtual void setRenderCallback(const OnRenderCallback &renderCallback = OnRenderCallback());

    typedef ::android::hardware::graphics::bufferqueue::V2_0::
            IGraphicBufferProducer HGraphicBufferProducer;
    /**
     * Configures an IGBP in order to create blocks. A newly created block is
     * dequeued from the configured IGBP. Unique Id of IGBP and the slot number of
     * blocks are passed via native_handle. Managing IGBP is responsibility of caller.
     * When IGBP is not configured, block will be created via allocator.
     * Since zero is not used for Unique Id of IGBP, if IGBP is not configured or producer
     * is configured as nullptr, unique id which is bundled in native_handle is zero.
     *
     * \param producer      the IGBP, which will be used to fetch blocks
     *                      This could be null, in the case this blockpool will
     *                      allocate backed GraphicBuffer via allocator(gralloc).
     */
    virtual void configureProducer(const android::sp<HGraphicBufferProducer> &producer);

    /**
     * Configures an IGBP in order to create blocks. A newly created block is
     * dequeued from the configured IGBP. Unique Id of IGBP and the slot number of
     * blocks are passed via native_handle. Managing IGBP is responsibility of caller.
     * When IGBP is not configured, block will be created via allocator.
     * Since zero is not used for Unique Id of IGBP, if IGBP is not configured or producer
     * is configured as nullptr, unique id which is bundled in native_handle is zero.
     *
     * \param producer      the IGBP, which will be used to fetch blocks
     *                      This could be null, in the case this blockpool will
     *                      allocate backed GraphicBuffer via allocator(gralloc).
     * \param syncMemory    Shared memory for synchronization of allocation & deallocation.
     * \param bqId          Id of IGBP
     * \param generationId  Generation Id for rendering output
     * \param consumerUsage consumerUsage flagof the IGBP
     */
    virtual void configureProducer(
            const android::sp<HGraphicBufferProducer> &producer,
            native_handle_t *syncMemory,
            uint64_t bqId,
            uint32_t generationId,
            uint64_t consumerUsage);

    virtual void getConsumerUsage(uint64_t *consumerUsage);

    /**
     * Invalidate the class.
     *
     * After the call, fetchGraphicBlock() will return C2_BAD_STATE.
     */
    virtual void invalidate();

    /**
     * Defer deallocation of cached blocks.
     *
     * Deallocation of cached blocks will be deferred until
     * \clearDeferredBlocks() is called. Or a new block allocation is
     * requested by \fetchGraphicBlock().
     */
    void setDeferDeallocationAfterStop();


    /**
     * Clear deferred blocks.
     *
     * Deallocation of cached blocks can be deferred by
     * \setDeferDeallocationAfterStop().
     * clear(deallocate) those deferred cached blocks explicitly.
     * Use this interface, if the blockpool could be inactive indefinitely.
     */
    void clearDeferredBlocks();

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    const local_id_t mLocalId;

    class Impl;
    std::shared_ptr<Impl> mImpl;

    friend struct C2BufferQueueBlockPoolData;
};

class C2SurfaceSyncMemory;

struct C2BufferQueueBlockPoolData : public _C2BlockPoolData {
public:
    typedef ::android::hardware::graphics::bufferqueue::V2_0::
            IGraphicBufferProducer HGraphicBufferProducer;

    // Create a remote BlockPoolData.
    C2BufferQueueBlockPoolData(
            uint32_t generation, uint64_t bqId, int32_t bqSlot,
            const std::shared_ptr<int> &owner,
            const android::sp<HGraphicBufferProducer>& producer);

    // Create a local BlockPoolData.
    C2BufferQueueBlockPoolData(
            uint32_t generation, uint64_t bqId, int32_t bqSlot,
            const std::shared_ptr<int> &owner,
            const android::sp<HGraphicBufferProducer>& producer,
            std::shared_ptr<C2SurfaceSyncMemory>);

    virtual ~C2BufferQueueBlockPoolData() override;

    virtual type_t getType() const override;

    int migrate(const android::sp<HGraphicBufferProducer>& producer,
                uint32_t toGeneration, uint64_t toUsage, uint64_t toBqId,
                android::sp<android::GraphicBuffer>& graphicBuffer, uint32_t oldGeneration,
                std::shared_ptr<C2SurfaceSyncMemory> syncMem);
private:
    friend struct _C2BlockFactory;

    // Methods delegated from _C2BlockFactory.
    void getBufferQueueData(uint32_t* generation, uint64_t* bqId, int32_t* bqSlot) const;
    bool holdBlockFromBufferQueue(const std::shared_ptr<int>& owner,
                                  const android::sp<HGraphicBufferProducer>& igbp,
                                  std::shared_ptr<C2SurfaceSyncMemory> syncMem);
    bool beginTransferBlockToClient();
    bool endTransferBlockToClient(bool transfer);
    bool beginAttachBlockToBufferQueue();
    bool endAttachBlockToBufferQueue(const std::shared_ptr<int>& owner,
                                     const android::sp<HGraphicBufferProducer>& igbp,
                                     std::shared_ptr<C2SurfaceSyncMemory> syncMem,
                                     uint32_t generation, uint64_t bqId, int32_t bqSlot);
    bool displayBlockToBufferQueue();

    const bool mLocal;
    bool mHeld;

    // Data of the corresponding buffer.
    uint32_t mGeneration;
    uint64_t mBqId;
    int32_t mBqSlot;

    // Data of the current IGBP, updated at migrate(). If the values are
    // mismatched, then the corresponding buffer will not be cancelled back to
    // IGBP at the destructor.
    uint32_t mCurrentGeneration;
    uint64_t mCurrentBqId;

    bool mTransfer; // local transfer to remote
    bool mAttach; // attach on remote
    bool mDisplay; // display on remote;
    std::weak_ptr<int> mOwner;
    android::sp<HGraphicBufferProducer> mIgbp;
    std::shared_ptr<C2SurfaceSyncMemory> mSyncMem;
    mutable std::mutex mLock;
};

#endif // STAGEFRIGHT_CODEC2_BUFFER_PRIV_H_
