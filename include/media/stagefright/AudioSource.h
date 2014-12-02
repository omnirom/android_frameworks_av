/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef AUDIO_SOURCE_H_

#define AUDIO_SOURCE_H_

#include <media/AudioRecord.h>
#include <media/AudioSystem.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <utils/List.h>
#ifdef QCOM_HARDWARE
#include <utils/String8.h>
#endif /* QCOM_HARDWARE */

#include <system/audio.h>

namespace android {

class AudioRecord;

struct AudioSource : public MediaSource, public MediaBufferObserver {
    // Note that the "channels" parameter _is_ the number of channels,
    // _not_ a bitmask of audio_channels_t constants.
    AudioSource(
            audio_source_t inputSource,
            uint32_t sampleRate,
            uint32_t channels = 1);

    status_t initCheck() const;

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop() { return reset(); }
    virtual sp<MetaData> getFormat();
#ifdef QCOM_HARDWARE
    status_t pause();
#endif /* QCOM_HARDWARE */

    // Returns the maximum amplitude since last call.
    int16_t getMaxAmplitude();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

    status_t dataCallback(const AudioRecord::Buffer& buffer);
#ifdef QCOM_HARDWARE
    virtual void onEvent(int event, void* info);
#endif /* QCOM_HARDWARE */
    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~AudioSource();

private:
    enum {
#ifndef QCOM_HARDWARE
        kMaxBufferSize = 2048,
#else /* QCOM_HARDWARE */
        //This max buffer size is derived from aggregation of audio
        //buffers for max duration 80 msec with 48K sampling rate.
        kMaxBufferSize = 30720,
#endif /* QCOM_HARDWARE */

        // After the initial mute, we raise the volume linearly
        // over kAutoRampDurationUs.
        kAutoRampDurationUs = 300000,

        // This is the initial mute duration to suppress
        // the video recording signal tone
#ifndef QCOM_HARDWARE
        kAutoRampStartUs = 0,
#else /* QCOM_HARDWARE */
        kAutoRampStartUs = 500000,
#endif /* QCOM_HARDWARE */
    };

    Mutex mLock;
    Condition mFrameAvailableCondition;
    Condition mFrameEncodingCompletionCondition;

#ifdef QCOM_HARDWARE
    AudioRecord::Buffer mTempBuf;
    uint32_t mPrevPosition;
    uint32_t mAllocBytes;
    int32_t mAudioSessionId;
    AudioRecord::transfer_type mTransferMode;
#endif /* QCOM_HARDWARE */
    sp<AudioRecord> mRecord;
    status_t mInitCheck;
    bool mStarted;
#ifdef QCOM_HARDWARE
    bool mRecPaused;
#endif /* QCOM_HARDWARE */
    int32_t mSampleRate;

    bool mTrackMaxAmplitude;
    int64_t mStartTimeUs;
    int16_t mMaxAmplitude;
    int64_t mPrevSampleTimeUs;
    int64_t mInitialReadTimeUs;
    int64_t mNumFramesReceived;
    int64_t mNumClientOwnedBuffers;
#ifdef QCOM_HARDWARE
    int64_t mAutoRampStartUs;
#endif /* QCOM_HARDWARE */

    List<MediaBuffer * > mBuffersReceived;

    void trackMaxAmplitude(int16_t *data, int nSamples);

    // This is used to raise the volume from mute to the
    // actual level linearly.
    void rampVolume(
        int32_t startFrame, int32_t rampDurationFrames,
        uint8_t *data,   size_t bytes);

    void queueInputBuffer_l(MediaBuffer *buffer, int64_t timeUs);
    void releaseQueuedFrames_l();
    void waitOutstandingEncodingFrames_l();
    status_t reset();

    AudioSource(const AudioSource &);
    AudioSource &operator=(const AudioSource &);
#ifdef QCOM_HARDWARE

    //additions for compress capture source
public:
    AudioSource(
        audio_source_t inputSource, const sp<MetaData>& meta);

private:
    audio_format_t mFormat;
    String8 mMime;
    int32_t mMaxBufferSize;
#endif /* QCOM_HARDWARE */
};

}  // namespace android

#endif  // AUDIO_SOURCE_H_
