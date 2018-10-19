/*
 * Copyright (C) 2014, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2007 The Android Open Source Project
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

#include <stdint.h>
#include <sys/types.h>
#include <cutils/log.h>

#include <media/AudioResampler.h>

namespace android {
// ----------------------------------------------------------------------------

class AudioResamplerQTI : public AudioResampler {
public:
    AudioResamplerQTI(int format, int inChannelCount, int32_t sampleRate);
    ~AudioResamplerQTI();
    size_t resample(int32_t* out, size_t outFrameCount,
                  AudioBufferProvider* provider);
    void setSampleRate(int32_t inSampleRate);
    size_t getNumInSample(size_t outFrameCount);

    int16_t *mState;
    int32_t *mTmpBuf;
    int32_t *mResamplerOutBuf;
    size_t mFrameIndex;
    size_t stateSize;
    size_t mOutFrameCount;
    size_t mInFrameRequest;

    static const int kNumTmpBufSize = 1024;

    void init();
    void reset();
};

class QCT_Resampler {

public:
    static size_t MemAlloc(int bitDepth, int inChannelCount,
                           int32_t inSampleRate, int32_t sampleRate);
    static void Init(int16_t *pState, int32_t inChannelCount, int32_t inSampleRate,int32_t mSampleRate,
                     int32_t is32BitIn=0,int32_t is32BitOut=1, int32_t dynamicEnable=1);
    static void Resample90dB(int16_t* pState, void* in, void* out, size_t inFrameCount,
                             size_t outFrameCount);
    static size_t GetNumInSamp(int16_t* pState, size_t outFrameCount);
    static void ReInitDynamicResamp(int16_t *pState, int32_t inSampleRate, int32_t mSampleRate);
};

// ----------------------------------------------------------------------------
}; // namespace android

