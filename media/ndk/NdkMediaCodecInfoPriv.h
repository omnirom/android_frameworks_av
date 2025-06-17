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

#ifndef _NDK_MEDIA_CODEC_INFO_PRIV_H
#define _NDK_MEDIA_CODEC_INFO_PRIV_H

#include <media/MediaCodecInfo.h>
#include <media/NdkMediaCodecInfo.h>

struct ACodecAudioCapabilities {
    std::shared_ptr<android::AudioCapabilities> mAudioCaps;

    std::vector<int> mSampleRates;
    std::vector<AIntRange> mSampleRateRanges;
    std::vector<AIntRange> mInputChannelCountRanges;

    void initSampleRates() {
        mSampleRates = mAudioCaps->getSupportedSampleRates();
    }

    void initSampleRateRanges() {
        const std::vector<android::Range<int>>& sampleRateRanges
                = mAudioCaps->getSupportedSampleRateRanges();
        for (auto it = sampleRateRanges.begin(); it != sampleRateRanges.end(); it++) {
            mSampleRateRanges.emplace_back(it->lower(), it->upper());
        }
    }

    void initInputChannelCountRanges() {
        const std::vector<android::Range<int>>& inputChannels
                = mAudioCaps->getInputChannelCountRanges();
        for (auto it = inputChannels.begin(); it != inputChannels.end(); it++) {
            mInputChannelCountRanges.emplace_back(it->lower(), it->upper());
        }
    }

    ACodecAudioCapabilities(std::shared_ptr<android::AudioCapabilities> audioCaps)
            : mAudioCaps(audioCaps) {
        initSampleRates();
        initSampleRateRanges();
        initInputChannelCountRanges();
    }
};

struct ACodecPerformancePoint {
    std::shared_ptr<const android::VideoCapabilities::PerformancePoint> mPerformancePoint;

    ACodecPerformancePoint(std::shared_ptr<const android::VideoCapabilities::PerformancePoint>
            performancePoint) : mPerformancePoint(performancePoint) {}
};

struct ACodecVideoCapabilities {
    std::shared_ptr<android::VideoCapabilities> mVideoCaps;

    std::vector<ACodecPerformancePoint> mPerformancePoints;

    void initPerformancePoints() {
        const std::vector<android::VideoCapabilities::PerformancePoint>& performancePoints
            = mVideoCaps->getSupportedPerformancePoints();
        for (auto it = performancePoints.begin(); it != performancePoints.end(); it++) {
            mPerformancePoints.emplace_back(
                    std::shared_ptr<const android::VideoCapabilities::PerformancePoint>(&(*it)));
        }
    }

    ACodecVideoCapabilities(std::shared_ptr<android::VideoCapabilities> videoCaps)
            : mVideoCaps(videoCaps) {
        initPerformancePoints();
    }
};

struct ACodecEncoderCapabilities {
    std::shared_ptr<android::EncoderCapabilities> mEncoderCaps;

    ACodecEncoderCapabilities(std::shared_ptr<android::EncoderCapabilities> encoderCaps)
            : mEncoderCaps(encoderCaps) {}
};

struct AMediaCodecInfo {
    std::string mName;
    android::sp<android::MediaCodecInfo> mInfo;
    std::string mMediaType;
    std::shared_ptr<android::CodecCapabilities> mCodecCaps;

    std::shared_ptr<const ACodecAudioCapabilities> mAAudioCaps;
    std::shared_ptr<const ACodecVideoCapabilities> mAVideoCaps;
    std::shared_ptr<const ACodecEncoderCapabilities> mAEncoderCaps;

    AMediaCodecInfo(const std::string &name, android::sp<android::MediaCodecInfo> info,
            std::shared_ptr<android::CodecCapabilities> codecCaps, const std::string &mediaType)
            : mName(name), mInfo(info), mMediaType(mediaType), mCodecCaps(codecCaps) {
        if (!mName.empty() && mInfo != nullptr && !mMediaType.empty() && mCodecCaps != nullptr) {
            if (mCodecCaps->getAudioCapabilities() != nullptr) {
                mAAudioCaps = std::make_shared<const ACodecAudioCapabilities>(
                        mCodecCaps->getAudioCapabilities());
            }
            if (mCodecCaps->getVideoCapabilities() != nullptr) {
                mAVideoCaps = std::make_shared<const ACodecVideoCapabilities>(
                        mCodecCaps->getVideoCapabilities());
            }
            if (mCodecCaps->getEncoderCapabilities() != nullptr) {
                mAEncoderCaps = std::make_shared<const ACodecEncoderCapabilities>(
                    mCodecCaps->getEncoderCapabilities());
            }
        }
    }
};

#endif //_NDK_MEDIA_CODEC_INFO_PRIV_H