/*
 * Copyright (C) 2025 The Android Open Source Project
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
package com.android.media.benchmark.library;
import android.media.MediaCodec;

import androidx.annotation.NonNull;
import android.util.Log;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;

import java.lang.Math.*;
import java.util.ArrayDeque;
import java.util.Iterator;

import java.io.FileInputStream;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

import java.util.ArrayDeque;

public class RawMediaFileStreamer implements IBufferXfer.IProducer {
    private static String TAG = RawMediaFileStreamer.class.getSimpleName();
    private static boolean DEBUG = false;
    private static float MEMORY_USE_RATIO = 0.5f;
    private static final int TIMEOUT_IN_SEC_FOR_TASK = 2;
    private IBufferXfer.IConsumer mConsumer = null;
    private FileInputStream mFile = null;
    private FileChannel mChannel = null;
    private int mMaxMemory = 0;
    private int mBytesTobeConsumed = 0;
    private boolean mEOFReached = false;
    private int mChunkSize = 0;
    private int mMaxChunks = 0;
    private long mChunkTime = 0;
    private int mChunkCounter = 0;
    private final Object mObject = new Object();
    private final Object mFuturesLock = new Object();
    // a softlimit
    private int mLowWater = 0;
    private final ArrayDeque<IBufferXfer.IProducerData> mFileDataCache = new ArrayDeque<>();
    private final ArrayDeque<MediaCodec.BufferInfo> mBufferInfoCache = new ArrayDeque<>();
    private final ArrayDeque<IBufferXfer.IProducerData> mPreRoll = new ArrayDeque<>();
    private final ArrayDeque<Future<?>> mSchedulerFutures = new ArrayDeque<>();
    private final ExecutorService mScheduler = Executors.newFixedThreadPool(1);

    private static class FileData implements IBufferXfer.IProducerData {
        public ByteBuffer mBuffer = null;
        public final ArrayDeque<MediaCodec.BufferInfo> mInfos = new ArrayDeque<>();

        public ByteBuffer getBuffer() {
            return mBuffer;
        }
        public ArrayDeque<MediaCodec.BufferInfo> getInfo() {
            return mInfos;
        }
    }

    public RawMediaFileStreamer (@NonNull FileInputStream file,
            int memoryMax, int chunkSize, int maxChunks, long chunkTime) {
        mFile = file;
        mChannel = mFile.getChannel();
        if (chunkSize <= 0 || memoryMax <= 0 || maxChunks <= 0) {
            Log.e(TAG, "Cannot feed as memory is "
                    + chunkSize + "/" + memoryMax + " with max chunks " + maxChunks);
            return;
        }
        mMaxMemory = Math.max(memoryMax, chunkSize  * maxChunks);
        mMaxMemory = (mMaxMemory / chunkSize) * chunkSize;
        mChunkSize = chunkSize;
        mMaxChunks = maxChunks;
        // 70% of max memory
        mLowWater = (int)
                ((MEMORY_USE_RATIO * ((mMaxMemory / (float)mChunkSize) + 0.5f)) * mChunkSize);
        mChunkTime = chunkTime;
        Log.d(TAG, "Max " + mMaxMemory + " LW " + mLowWater
                + " CS " + mChunkSize + "MaxChunks " + mMaxChunks + " CT " + mChunkTime);
    }

    public void handleFuture(Future<?> future) {
        synchronized(mFuturesLock) {
            if (future != null) {
                mSchedulerFutures.add(future);
            }
            while (mSchedulerFutures.isEmpty() == false
                && mSchedulerFutures.peekFirst().isDone() == true) {
                Future<?> task = mSchedulerFutures.pollFirst();
                try {
                    task.get();
                } catch(Exception e) {
                    Log.d (TAG, "Scheduler encountered an exception " + e.toString());
                }
            }
        }
    }

    private boolean schedule(ArrayDeque<IBufferXfer.IProducerData> buffers) {
        if (mScheduler == null || mScheduler.isShutdown()) {
            Log.e(TAG, "Scheduler is shutdown, cannot schedule");
            return false;
        }
        if (!buffers.isEmpty()) {
            Future<?> future = mScheduler.submit(() -> { mConsumer.consume(buffers); });
            handleFuture(future);
        }
        return true;
    }

    private boolean streamFile(
        @NonNull ArrayDeque<IBufferXfer.IProducerData> datas, boolean preroll) {
        if (mChunkSize == 0) {
            Log.d(TAG, "Nothing to send ChunkSize == 0");
            return false;
        }
        int bytesToBeConsumed = 0, addedData = 0;
        boolean eofReached = false;
        synchronized (mObject) {
            bytesToBeConsumed = mBytesTobeConsumed;
            eofReached = mEOFReached;
        }
        ArrayDeque<IBufferXfer.IProducerData> buffersToSend = new ArrayDeque<>();
        ArrayDeque<IBufferXfer.IProducerData> activeDeque =
                (preroll == true) ? mPreRoll : buffersToSend;
        if (datas != null && datas.isEmpty() == false) {
            for ( IBufferXfer.IProducerData buffer : datas) {
                mBufferInfoCache.addAll(buffer.getInfo());
                buffer.getInfo().clear();
            }
            mFileDataCache.addAll(datas);
        }
        if (preroll == false && !mPreRoll.isEmpty()) {
            if (DEBUG) {
                Log.d(TAG, "Sending preroll buffers");
            }
            ArrayDeque<IBufferXfer.IProducerData> prerollBuffers = mPreRoll.clone();
            mPreRoll.clear();
            schedule(prerollBuffers);
        }
        FileData currentData = null;
        Iterator<IBufferXfer.IProducerData> dataCacheIter = mFileDataCache.iterator();
        ByteBuffer currentBuffer = null;
        Iterator<MediaCodec.BufferInfo> infoCacheIter = mBufferInfoCache.iterator();
        MediaCodec.BufferInfo currentInfo = null;
        int offset = 0;
        while (((bytesToBeConsumed + addedData) < mLowWater) && (eofReached == false)) {
            if ((mMaxMemory - (bytesToBeConsumed + addedData)) < mChunkSize) {
                Log.d(TAG, "Enough memory already for consumption");
                break;
            }
            if (currentData == null) {
                if (dataCacheIter.hasNext() == true) {
                    currentData = (FileData)(dataCacheIter.next());
                    dataCacheIter.remove();
                } else {
                    currentData = new FileData();
                }
                currentData.getInfo().clear();
                currentBuffer = currentData.getBuffer();
                if(currentBuffer == null) {
                    currentBuffer = ByteBuffer.allocate(mChunkSize * mMaxChunks);
                    currentData.mBuffer = currentBuffer;
                }
                offset = 0;
                currentBuffer.clear();
            }
            if (currentInfo == null) {
                if (infoCacheIter.hasNext() == true) {
                    currentInfo = infoCacheIter.next();
                    infoCacheIter.remove();
                } else {
                    currentInfo = new MediaCodec.BufferInfo();
                }
            }
            try {
                long available = (mChannel.size() - mChannel.position());
                int availableChunks = (int)(available / mChunkSize);
                int readChunks = availableChunks >= mMaxChunks ? mMaxChunks  : availableChunks;
                if (readChunks == 0) {
                    eofReached = true;
                    currentInfo.set(
                            offset,
                            0,
                            mChunkTime * mChunkCounter,
                            MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    currentData.mInfos.add(currentInfo);
                    currentBuffer.position(0);
                    currentData.mBuffer = currentBuffer;
                    activeDeque.add(currentData);
                    if (DEBUG) {
                        Log.d(TAG, "Signaling EOF -> file ending");
                        Log.d(TAG, "readBytes " + 0
                                + " readChunks " + readChunks
                                + " ts " + currentInfo.presentationTimeUs
                                + " flags " + currentInfo.flags);

                    }
                    offset = 0;
                    currentData = null; currentBuffer = null; currentInfo = null;
                } else {
                    // try to read from the file and update the actual read.
                    // if the file reaches EOF, then signal that in the flag.
                    if (currentBuffer.remaining() < (readChunks * mChunkSize)) {
                        if (currentBuffer.position() == 0) {
                            Log.d(TAG, "Producer buffer seems to be small to read chunks");
                        }
                        offset = 0;
                        currentBuffer.position(0);
                        currentData.mBuffer = currentBuffer;
                        activeDeque.add(currentData);
                        currentData = null; currentBuffer = null;
                    } else {
                        if (DEBUG) {
                            Log.d(TAG, "Reading: offset " + offset + "readChunks " + readChunks
                                    + " length " + readChunks * mChunkSize);
                        }
                        int readBytes = mFile.read(
                            currentBuffer.array(), offset , (int)(readChunks * mChunkSize));
                        if (readBytes == -1) {
                            eofReached = true;
                            currentInfo.set(
                                    offset,
                                    0,
                                    mChunkTime * mChunkCounter,
                                    MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                            currentData.mInfos.add(currentInfo);
                            currentBuffer.position(0);
                            currentData.mBuffer = currentBuffer;
                            activeDeque.add(currentData);
                            currentData = null; currentBuffer = null;
                            if (DEBUG) {
                                Log.d(TAG, "EOF reached");
                                Log.d(TAG, "readBytes " + readBytes
                                        + " readChunks " + readChunks
                                        + " ts " + currentInfo.presentationTimeUs
                                        + " flags " + currentInfo.flags);

                            }
                        } else {
                            // set as many info as it relates to one chunk size.
                            int nInfos = readBytes / mChunkSize;
                            for (int i = 0; i < nInfos; i++) {
                                if (currentInfo == null) {
                                    if (infoCacheIter.hasNext() == true) {
                                        currentInfo = infoCacheIter.next();
                                        infoCacheIter.remove();
                                    } else {
                                        currentInfo = new MediaCodec.BufferInfo();
                                    }
                                }
                                currentInfo.set(
                                        offset + (i * mChunkSize),
                                        mChunkSize,
                                        mChunkTime * mChunkCounter,
                                        0);
                                currentData.mInfos.add(currentInfo);
                                mChunkCounter++;
                                currentInfo = null;
                            }
                            currentBuffer.position(readBytes);
                            addedData += readBytes;
                            offset += readBytes;
                            if (DEBUG) {
                                Log.d(TAG, "readBytes " + readBytes
                                        + " readChunks " + readChunks);
                            }
                        }
                        currentInfo = null;
                    }
                }
            } catch (NullPointerException e) {
                Log.e(TAG, "Buffer array return null");
                e.printStackTrace();
                return false;
            } catch (IndexOutOfBoundsException | IOException e) {
                Log.e(TAG, "Error with len " + mChunkSize + " msg: " + e.getMessage());
                e.printStackTrace();
                return false;
            }
        }
        if (currentData != null) {
            if (currentBuffer == null) {
                Log.d(TAG, "Error with data, buffers not present");
            } else {
                currentBuffer.position(0);
            }
            currentData.mBuffer = currentBuffer;
            activeDeque.add(currentData);
            currentData = null; currentBuffer = null; currentInfo = null;
        }
        if (DEBUG) {
            Log.d(TAG, "(Preroll " + preroll + ")Memory for consumption "
                + bytesToBeConsumed + "/" + mMaxMemory
                + "(" + bytesToBeConsumed/(float)mMaxMemory * 100.0f + "%)");
        }
        synchronized(mObject) {
            mBytesTobeConsumed += addedData;
            mEOFReached = eofReached;
        }
        schedule(buffersToSend);
        return true;
    }

    @Override
    public boolean returnBuffers(ArrayDeque<IBufferXfer.IProducerData> datas) {
        int usedSize = 0, numBuffers = 0;
        boolean shouldSchedule = false;
        ArrayDeque<IBufferXfer.IProducerData> deque = null;
        if (datas != null) {
            deque = datas.clone();
            for (IBufferXfer.IProducerData data : datas) {
                ArrayDeque<MediaCodec.BufferInfo> infos = data.getInfo();
                for (MediaCodec.BufferInfo info : infos) {
                    usedSize += info.size;
                    numBuffers++;
                }
            }
            if (DEBUG) {
                Log.d(TAG, "Memory for consumption inf-all-used " + usedSize
                        + " #buf " + numBuffers);
            }
        }
        int bytesConsumed = 0;
        synchronized (mObject) {
            mBytesTobeConsumed -= usedSize;
            bytesConsumed = mBytesTobeConsumed;
            if (mEOFReached == false) {
                shouldSchedule = true;
            }
        }
        Future<?> future = null;
        if (shouldSchedule == true) {
            final ArrayDeque<IBufferXfer.IProducerData> cloned = deque;
            future = mScheduler.submit(() -> { streamFile(cloned, false); });
        }
        handleFuture(future);
        if (DEBUG) {
            Log.d(TAG, "Memory for consumption "
                + bytesConsumed + "/" + mMaxMemory
                + "(" + bytesConsumed/(float)mMaxMemory * 100.0f + "%)");
        }
        return true;
    }

    @Override
    public boolean setConsumer (@NonNull IBufferXfer.IConsumer consumer) {
        mConsumer = consumer;
        Future<?> future = mScheduler.submit(() -> {streamFile(null, false);});
        handleFuture(future);
        return true;
    }

    public boolean preroll() {
        return streamFile(null, true);
    }

    public void stop() {
        while (mSchedulerFutures.isEmpty() == false) {
            Future<?> future = mSchedulerFutures.pollFirst();
            try {
                future.get(TIMEOUT_IN_SEC_FOR_TASK, TimeUnit.SECONDS);;
            } catch(Exception e) {
                Log.d (TAG, "Scheduler encountered an exception " + e.toString());
            }
        }
        mScheduler.shutdown();
    }
}