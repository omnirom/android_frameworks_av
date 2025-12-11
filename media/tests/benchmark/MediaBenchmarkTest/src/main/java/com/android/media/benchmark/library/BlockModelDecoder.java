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
package com.android.media.benchmark.library;

import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;

import java.util.ArrayDeque;

import com.android.media.benchmark.library.Decoder;

public class BlockModelDecoder extends Decoder {
    private static final String TAG = BlockModelDecoder.class.getSimpleName();
    private final boolean DEBUG = false;
    protected final LinearBlockWrapper mLinearInputBlock = new LinearBlockWrapper();

    /**
     * Wrapper class for {@link MediaCodec.LinearBlock}
     */
    public static class LinearBlockWrapper {
        private MediaCodec.LinearBlock mBlock;
        private ByteBuffer mBuffer;
        private int mOffset;

        public MediaCodec.LinearBlock getBlock() {
            return mBlock;
        }

        public ByteBuffer getBuffer() {
            return mBuffer;
        }

        public int getBufferCapacity() {
            return mBuffer == null ? 0 : mBuffer.capacity();
        }

        public int getOffset() {
            return mOffset;
        }

        public void setOffset(int size) {
            mOffset = size;
        }

        public boolean allocateBlock(String codec, int size) throws RuntimeException{
            recycle();
            mBlock = MediaCodec.LinearBlock.obtain(size, new String[]{codec});
            if (mBlock == null || !mBlock.isMappable()) {
                throw new RuntimeException("Linear Block not allocated/mapped");
            }
            mBuffer = mBlock.map();
            mOffset = 0;
            return true;
        }

        public void recycle() {
            if (mBlock != null) {
                mBlock.recycle();
                mBlock = null;
            }
            mBuffer = null;
            mOffset = 0;
        }
    }

    @Override
    public boolean returnBuffers(ArrayDeque<IBufferXfer.IProducerData> datas) {
        for (IBufferXfer.IProducerData data : datas) {
            DecoderData decoderData = (DecoderData)data;
            if (decoderData.mOutputFrame != null
                    && decoderData.mOutputFrame.getLinearBlock() != null) {
                decoderData.mOutputFrame.getLinearBlock().recycle();
                decoderData.mOutputFrame  = null;
            } else {
                Log.d(TAG, "Error, output frame not found in decoder data");
            }
        }
        return super.returnBuffers(datas);
    }

    public BlockModelDecoder() {
        // empty
    }

    public void tearDown() {
        mLinearInputBlock.recycle();

    }

    protected boolean isOutputBufferLinear() {
        return mMime.startsWith("audio");
    }

    /**
     * Decodes the given input buffer,
     * provided valid list of buffer info and format are passed as inputs.
     *
     * @param inputBuffer     Decode the provided list of ByteBuffers
     * @param inputBufferInfo List of buffer info corresponding to provided input buffers
     * @param asyncMode       Will run on async implementation if true
     * @param format          For creating the decoder if codec name is empty and configuring it
     * @param codecName       Will create the decoder with codecName
     * @return DECODE_SUCCESS if decode was successful, DECODE_DECODER_ERROR for fail,
     *         DECODE_CREATE_ERROR for decoder not created
     * @throws IOException if the codec cannot be created.
     */
    @Override
    public int decode(@NonNull List<ByteBuffer> inputBuffer,
        @NonNull List<MediaCodec.BufferInfo> inputBufferInfo, final boolean asyncMode,
        @NonNull MediaFormat format, String codecName)
        throws IOException, InterruptedException {
        setExtraConfigureFlags(MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL);
        if (format.containsKey(MediaFormat.KEY_MIME)) {
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mConsumer != null && !mime.startsWith("audio/")) {
                Log.e(TAG, "Non-linear input buffers cannot be instantiated for buffer transfer.");
                throw new IOException();
            }
        }
        return super.decode(inputBuffer, inputBufferInfo, asyncMode, format, codecName);
    }

    @Override
    protected void onInputAvailable(int inputBufferId, MediaCodec mediaCodec) {
        if (mNumInFramesProvided >= mNumInFramesRequired) {
            mIndex = mInputBufferInfo.size() - 1;
        }
        MediaCodec.BufferInfo bufInfo = mInputBufferInfo.get(mIndex);
        mSawInputEOS = (bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
        if (mLinearInputBlock.getOffset() + bufInfo.size > mLinearInputBlock.getBufferCapacity()) {
            int requestSize = 8192;
            requestSize = Math.max(bufInfo.size, requestSize);
            mLinearInputBlock.allocateBlock(mediaCodec.getCanonicalName(), requestSize);
        }
        int codecFlags = 0;
        if ((bufInfo.flags & MediaExtractor.SAMPLE_FLAG_SYNC) != 0) {
            codecFlags |= MediaCodec.BUFFER_FLAG_KEY_FRAME;
        }
        if ((bufInfo.flags & MediaExtractor.SAMPLE_FLAG_PARTIAL_FRAME) != 0) {
            codecFlags |= MediaCodec.BUFFER_FLAG_PARTIAL_FRAME;
        }
        codecFlags |= mSawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0;
        if (DEBUG) {
            Log.v(TAG, "Input: id: " + inputBufferId
                    + " size: " + bufInfo.size
                    + " pts: " + bufInfo.presentationTimeUs
                    + " flags: " + codecFlags);
        }
        mLinearInputBlock.getBuffer().put(mInputBuffer.get(mIndex).array());
        mNumInFramesProvided++;
        mIndex = mNumInFramesProvided % (mInputBufferInfo.size() - 1);
        if (mSawInputEOS) {
            Log.i(TAG, "Saw Input EOS");
        }
        mStats.addFrameSize(bufInfo.size);
        MediaCodec.QueueRequest request = mCodec.getQueueRequest(inputBufferId);
        request.setLinearBlock(mLinearInputBlock.getBlock(), mLinearInputBlock.getOffset(),
                bufInfo.size);
        request.setPresentationTimeUs(bufInfo.presentationTimeUs);
        request.setFlags(codecFlags);
        request.queue();
        if (bufInfo.size > 0 && (codecFlags & (MediaCodec.BUFFER_FLAG_CODEC_CONFIG
                | MediaCodec.BUFFER_FLAG_PARTIAL_FRAME)) == 0) {
            mLinearInputBlock.setOffset(mLinearInputBlock.getOffset() + bufInfo.size);
        }
    }

    @Override
    protected void onOutputAvailable(
            MediaCodec mediaCodec, int outputBufferId, MediaCodec.BufferInfo outputBufferInfo) {
        if (mSawOutputEOS || outputBufferId < 0) {
            return;
        }
        boolean canGetBtyeBuffer = isOutputBufferLinear();;

        mNumOutputFrame++;
        if (DEBUG) {
            Log.d(TAG,
                    "In OutputBufferAvailable ,"
                            + " output frame number = " + mNumOutputFrame
                            + " timestamp = " + outputBufferInfo.presentationTimeUs
                            + " size = " + outputBufferInfo.size
                            + " flags = " + outputBufferInfo.flags);
        }
        MediaCodec.OutputFrame outFrame = mediaCodec.getOutputFrame(outputBufferId);
        ByteBuffer outputBuffer = null;
        if (mOutputStream != null && canGetBtyeBuffer == true) {
            try {
                if (outputBuffer == null) {
                    if (outFrame != null && outFrame.getLinearBlock() != null) {
                        outputBuffer = outFrame.getLinearBlock().map();
                    }
                }
                if (outputBuffer != null) {
                    byte[] bytesOutput = new byte[outputBuffer.remaining()];
                    outputBuffer.get(bytesOutput);
                    mOutputStream.write(bytesOutput);
                }
            } catch (IOException e) {
                e.printStackTrace();
                Log.d(TAG, "Error Dumping File: Exception " + e.toString());
            }
        }
        mSawOutputEOS = (outputBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
        if (mConsumer != null && canGetBtyeBuffer == true) {
            ArrayDeque<MediaCodec.BufferInfo> infos = new ArrayDeque<>();
            infos.add(outputBufferInfo);
            if (outputBuffer == null) {
                if (outFrame != null && outFrame.getLinearBlock() != null) {
                    outputBuffer = outFrame.getLinearBlock().map();
                }
            }
            DecoderData data = prepareDecoderData(outputBufferId, outputBuffer, infos);
            if (data != null) {
                ArrayDeque<IBufferXfer.IProducerData> buffers = new ArrayDeque<>();
                data.mOutputFrame = outFrame;
                buffers.add(data);
                sendToConsumer(buffers);
            } else if (outputBuffer != null) {
                outFrame.getLinearBlock().recycle();
                outputBuffer = null;
            }
        } else if (mFrameReleaseQueue != null) {
            if (mMime.startsWith("audio/")) {
                try {
                    mFrameReleaseQueue.pushFrame(outputBufferId, outFrame, outputBufferInfo.size);
                } catch (Exception e) {
                    Log.d(TAG, "Error in getting MediaCodec buffer" + e.toString());
                }
            } else {
                mFrameReleaseQueue.pushFrame(mNumOutputFrame, outputBufferId,
                                                outputBufferInfo.presentationTimeUs);
            }
        } else {
            if (canGetBtyeBuffer == true && outFrame != null && outFrame.getLinearBlock() != null) {
                outFrame.getLinearBlock().recycle();
                outputBuffer = null;
            }
            mediaCodec.releaseOutputBuffer(outputBufferId, mRender);
        }
        if (DEBUG && mSawOutputEOS) {
            Log.i(TAG, "Saw output EOS");
        }
    }

}