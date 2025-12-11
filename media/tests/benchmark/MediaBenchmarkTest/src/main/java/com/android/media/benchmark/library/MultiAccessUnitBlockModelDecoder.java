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

import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

import java.util.ArrayDeque;
import java.util.Iterator;
import java.util.List;

import com.android.media.benchmark.library.CodecUtils;
import com.android.media.benchmark.library.BlockModelDecoder;

public class MultiAccessUnitBlockModelDecoder extends BlockModelDecoder {
	private static final String TAG = MultiAccessUnitBlockModelDecoder.class.getSimpleName();
    private final ArrayDeque<MediaCodec.BufferInfo> mInputInfos = new ArrayDeque<>();
    private final boolean DEBUG = false;
    protected int mMaxInputSize = 0;

    public MultiAccessUnitBlockModelDecoder() {
    	// empty
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
        configureMaxInputSize(format);
        if (format.containsKey(MediaFormat.KEY_MIME)) {
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (!mime.startsWith("audio")) {
                Log.d(TAG, "Multi access unit decoders are valid only for audio");
                return -1;
            }
        }
        return super.decode(inputBuffer, inputBufferInfo, asyncMode, format, codecName);
    }

    protected void configureMaxInputSize(MediaFormat format) {
        final String mime = format.getString(MediaFormat.KEY_MIME);
        final int maxOutputSize = format.getNumber(
            MediaFormat.KEY_BUFFER_BATCH_MAX_OUTPUT_SIZE, 0).intValue();
        int maxInputSizeInBytes = 0;
        if (format.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)) {
            maxInputSizeInBytes = format.getNumber(
                    MediaFormat.KEY_MAX_INPUT_SIZE, 0).intValue();
        }
        mMaxInputSize = Math.max(maxInputSizeInBytes,
                (int) (maxOutputSize * CodecUtils.getCompressionRatio(mime)));
    }

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
                            int outputBufferId, @NonNull ArrayDeque<MediaCodec.BufferInfo> infos) {
                double size = 0;
                for (MediaCodec.BufferInfo info : infos) {
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
                configureMaxInputSize(format);
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

    protected void onInputsAvailable(int inputBufferId, MediaCodec mediaCodec) {
        if (inputBufferId >= 0) {
            mLinearInputBlock.allocateBlock(mediaCodec.getCanonicalName(), mMaxInputSize);
            MediaCodec.BufferInfo bufInfo;
            mInputInfos.clear();
            int offset = 0;
            while (mNumInFramesProvided < mNumInFramesRequired) {
                bufInfo = mInputBufferInfo.get(mIndex);
                mSawInputEOS = (bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                int bufferSizeNeeded = mLinearInputBlock.getOffset() + bufInfo.size;
                if (bufferSizeNeeded > mLinearInputBlock.getBufferCapacity()) {
                    break;
                }
                mLinearInputBlock.getBuffer().put(mInputBuffer.get(mIndex).array());
                mLinearInputBlock.setOffset(mLinearInputBlock.getOffset() + bufInfo.size);
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
                int bufferSizeNeeded = mLinearInputBlock.getOffset() + bufInfo.size;
                if (bufferSizeNeeded <= mLinearInputBlock.getBufferCapacity()) {
                    if ((bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) == 0) {
                        Log.e(TAG, "Error in EOS flag for Decoder");
                    }
                    mSawInputEOS = (bufInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                    mLinearInputBlock.getBuffer().put(mInputBuffer.get(mIndex).array());
                    mLinearInputBlock.setOffset(mLinearInputBlock.getOffset() + bufInfo.size);
                    bufInfo.offset = offset; offset += bufInfo.size;
                    //bufInfo.flags = codecFlags;
                    mInputInfos.add(bufInfo);
                    mNumInFramesProvided++;
                }
            }
            if (mInputInfos.size() == 0) {
                Log.d(TAG, " No inputs to queue");
            } else {
                mStats.addFrameSize(offset);
                MediaCodec.QueueRequest request = mediaCodec.getQueueRequest(inputBufferId);
                request.setMultiFrameLinearBlock(mLinearInputBlock.getBlock(), mInputInfos);
                request.queue();
            }
        }
    }

    protected void onOutputsAvailable(MediaCodec mediaCodec, int outputBufferId,
            ArrayDeque<MediaCodec.BufferInfo> infos) {
        if (mSawOutputEOS || outputBufferId < 0) {
            return;
        }
        // Since this is only about audio, we should always check for
        // a valid linear block.
        MediaCodec.OutputFrame outFrame = mediaCodec.getOutputFrame(outputBufferId);
        ByteBuffer outputBuffer = null;
        try {
            if (outFrame.getLinearBlock() != null) {
                outputBuffer = outFrame.getLinearBlock().map();
                if (DEBUG) {
                    Log.d(TAG, "onOutputsAvailable ID : " + outputBufferId
                            + " output info size: " + infos.size()
                            + " Output remaining: " + outputBuffer.remaining());
                }
            }
        } catch(IllegalStateException e) {
            // buffer may not be linear, this is ok
            // as we are handling non-linear buffers below.
        }
        if (mOutputStream != null) {
            try {
                if (outFrame.getLinearBlock() != null) {
                    if (outputBuffer == null) {
                        outputBuffer = outFrame.getLinearBlock().map();
                    }
                    int savedPosition = outputBuffer.position();
                    byte[] bytesOutput = new byte[outputBuffer.remaining()];
                    outputBuffer.get(bytesOutput);
                    mOutputStream.write(bytesOutput);
                    if (DEBUG) {
                        Log.d(TAG, "Received outputs buffer size : " + outputBuffer.remaining()
                                + " infos size " + infos.size());
                    }
                    outputBuffer.position(savedPosition);
                }
            } catch (IOException e) {
                e.printStackTrace();
                Log.d(TAG, "Error Dumping File: Exception " + e.toString());
            }
        }
        mNumOutputFrame += infos.size();
        MediaCodec.BufferInfo last = infos.peekLast();
        if (last != null) {
            mSawOutputEOS |= ((last.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0);
        }
        if (mConsumer != null) {
            if (outFrame.getLinearBlock() != null) {
                if (outputBuffer == null) {
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
            }
        } else if (mFrameReleaseQueue != null) {
            // Should not come here for a video stream.
            int bytesRemaining = 0;
            if (outFrame.getLinearBlock() != null) {
                if (outputBuffer == null) {
                    outputBuffer = outFrame.getLinearBlock().map();
                }
                bytesRemaining = outputBuffer.remaining();
            }
            mFrameReleaseQueue.pushFrame(outputBufferId, outFrame, bytesRemaining);
        } else {
            if (outFrame != null && outFrame.getLinearBlock() != null) {
                outFrame.getLinearBlock().recycle();
                outputBuffer = null;
            }
            mediaCodec.releaseOutputBuffer(outputBufferId, mRender);
        }
        if (mSawOutputEOS) {
            Log.i(TAG, "Large frame - saw output EOS");
        }
    }

}