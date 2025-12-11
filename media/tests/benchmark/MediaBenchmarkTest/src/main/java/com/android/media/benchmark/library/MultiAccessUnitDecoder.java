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

import android.view.Surface;

import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaFormat;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.android.media.benchmark.library.IBufferXfer;
import com.android.media.benchmark.library.Decoder;

public class MultiAccessUnitDecoder extends Decoder {
    private static final String TAG = "MultiAccessUnitDecoder";
    private static final boolean DEBUG = false;
    private final ArrayDeque<BufferInfo> mInputInfos = new ArrayDeque<>();

    @Override
    public void setCallback(MediaCodec codec) {
        mCodec.setCallback(new MediaCodec.Callback() {
            boolean isUsingLargeFrameMode = false;

            @Override
            public void onInputBufferAvailable(
                    @NonNull MediaCodec mediaCodec, int inputBufferId) {
                try {
                    mStats.addInputTime();
                    if (isUsingLargeFrameMode) {
                        onInputsAvailable(inputBufferId, mediaCodec);
                    } else {
                        onInputAvailable(inputBufferId, mediaCodec);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    Log.e(TAG, e.toString());
                }
            }

            @Override
            public void onOutputBufferAvailable(@NonNull MediaCodec mediaCodec,
                    int outputBufferId, @NonNull MediaCodec.BufferInfo bufferInfo) {
                mStats.addOutputTime();
                onOutputAvailable(mediaCodec, outputBufferId, bufferInfo);
                if (mSawOutputEOS) {
                    synchronized (mLock) { mLock.notify(); }
                }
            }

            @Override
            public void onOutputBuffersAvailable(
                    @NonNull MediaCodec mediaCodec,
                            int outputBufferId, @NonNull ArrayDeque<BufferInfo> infos) {
                double size = 0;
                for (BufferInfo info : infos) {
                    size += info.size;
                }
                mStats.addOutputTime(size / DEFAULT_AUDIO_FRAME_SIZE);
                onOutputsAvailable(mediaCodec, outputBufferId, infos);
                if (mSawOutputEOS) {
                    synchronized (mLock) { mLock.notify(); }
                }
            }

            @Override
            public void onOutputFormatChanged(
                    @NonNull MediaCodec mediaCodec, @NonNull MediaFormat format) {
                Log.i(TAG, "Output format changed. Format: " + format.toString());
                final int maxOutputSize = format.getNumber(
                            MediaFormat.KEY_BUFFER_BATCH_MAX_OUTPUT_SIZE, 0).intValue();
                isUsingLargeFrameMode = (maxOutputSize > 0);
                if (mUseFrameReleaseQueue && mFrameReleaseQueue == null) {
                    int bytesPerSample = AudioFormat.getBytesPerSample(
                            format.getInteger(MediaFormat.KEY_PCM_ENCODING,
                                    AudioFormat.ENCODING_PCM_16BIT));
                    int sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                    int channelCount = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                    mFrameReleaseQueue = new FrameReleaseQueue(
                            mRender, sampleRate, channelCount, bytesPerSample);
                    mFrameReleaseQueue.setMediaCodec(mCodec);
                }
            }

            @Override
            public void onError(
                    @NonNull MediaCodec mediaCodec, @NonNull MediaCodec.CodecException e) {
                mSignalledError = true;
                Log.e(TAG, "Codec Error: " + e.toString());
                e.printStackTrace();
                synchronized (mLock) { mLock.notify(); }
            }
        });

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
            @NonNull List<BufferInfo> inputBufferInfo, final boolean asyncMode,
            @NonNull MediaFormat format, String codecName)
            throws IOException, InterruptedException {
        if (format.containsKey(MediaFormat.KEY_MIME)) {
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (!mime.startsWith("audio")) {
                Log.d(TAG, "Multi access unit decoders are valid only for audio");
                return -1;
            }
        }
        return super.decode(inputBuffer, inputBufferInfo, asyncMode, format, codecName);
    }

    private void onInputsAvailable(int inputBufferId, MediaCodec mediaCodec) {
        if (inputBufferId >= 0) {
            ByteBuffer inputCodecBuffer = mediaCodec.getInputBuffer(inputBufferId);
            BufferInfo bufInfo;
            mInputInfos.clear();
            int offset = 0;
            while (mNumInFramesProvided < mNumInFramesRequired) {
                bufInfo = mInputBufferInfo.get(mIndex);
                mSawInputEOS = (bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                if (inputCodecBuffer.remaining() < bufInfo.size) {
                    if (mInputInfos.size() == 0) {
                        Log.d(TAG, "SampleSize " + inputCodecBuffer.remaining()
                                + "greater than MediaCodec Buffer size " + bufInfo.size);
                    }
                    break;
                }
                inputCodecBuffer.put(mInputBuffer.get(mIndex).array());
                bufInfo.offset = offset; offset += bufInfo.size;
                mInputInfos.add(bufInfo);
                mNumInFramesProvided++;
                mIndex = mNumInFramesProvided % (mInputBufferInfo.size() - 1);
            }
            if (DEBUG) {
                Log.d(TAG, "inputsAvailable ID : " + inputBufferId
                        + " queued info size: " + mInputInfos.size()
                        + " Total queued size: " + offset);
            }
            if (mNumInFramesProvided >= mNumInFramesRequired) {
                mIndex = mInputBufferInfo.size() - 1;
                bufInfo = mInputBufferInfo.get(mIndex);
                if (inputCodecBuffer.remaining() > bufInfo.size) {
                    if ((bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) == 0) {
                        Log.e(TAG, "Error in EOS flag for Decoder");
                    }
                    mSawInputEOS = (bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                    inputCodecBuffer.put(mInputBuffer.get(mIndex).array());
                    bufInfo.offset = offset; offset += bufInfo.size;
                    mInputInfos.add(bufInfo);
                    mNumInFramesProvided++;
                }
            }
            if (mInputInfos.size() == 0) {
                Log.d(TAG, " No inputs to queue");
            } else {
                mStats.addFrameSize(offset);
                mediaCodec.queueInputBuffers(inputBufferId, mInputInfos);
            }
        }
    }

    private void onOutputsAvailable(MediaCodec mc, int outputBufferId,
            ArrayDeque<BufferInfo> infos) {
        if (mSawOutputEOS || outputBufferId < 0) {
            return;
        }
        ByteBuffer outputBuffer = null;
        if (mOutputStream != null) {
            try {
                outputBuffer = mc.getOutputBuffer(outputBufferId);
                if (outputBuffer != null) {
                    int savedPosition = outputBuffer.position();
                    byte[] bytesOutput = new byte[outputBuffer.remaining()];
                    outputBuffer.get(bytesOutput);
                    mOutputStream.write(bytesOutput);
                    outputBuffer.position(savedPosition);
                }
            } catch (IOException e) {
                e.printStackTrace();
                Log.d(TAG, "Error Dumping File: Exception " + e.toString());
            }
        }
        if (DEBUG) {
            Log.d(TAG, "onOutputsAvailable ID : " + outputBufferId
                    + " output info size: " + infos.size());
        }
        mNumOutputFrame += infos.size();
        MediaCodec.BufferInfo last = infos.peekLast();
        if (last != null) {
            mSawOutputEOS |= ((last.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0);
        }
        if (mConsumer != null) {
            if (outputBuffer == null) {
                outputBuffer = mc.getOutputBuffer(outputBufferId);
            }
            DecoderData data = prepareDecoderData(
                    outputBufferId,
                    outputBuffer,
                    infos);
            if (data != null) {
                ArrayDeque<IBufferXfer.IProducerData> buffers = new ArrayDeque<>();
                buffers.add(data);
                sendToConsumer(buffers);
            }
        }
        else if (mFrameReleaseQueue != null) {
            // this should be here for a video stream
            int remaining = 0;
            if (outputBuffer == null) {
                outputBuffer = mc.getOutputBuffer(outputBufferId);
                if (outputBuffer != null) {
                    remaining = outputBuffer.remaining();
                }
            }
            mFrameReleaseQueue.pushFrame(
                    outputBufferId, remaining);
        } else {
            mc.releaseOutputBuffer(outputBufferId, mRender);
        }
        if (mSawOutputEOS) {
            Log.i(TAG, "Large frame - saw output EOS");
        }
    }
}
