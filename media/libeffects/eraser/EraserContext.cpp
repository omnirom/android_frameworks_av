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

#include <cstring>
#define LOG_TAG "AHAL_EraserContext"

#include <android-base/logging.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "EraserContext.h"
#include "EraserUtils.h"
#include "LiteRTInstance.h"

using aidl::android::media::audio::common::AudioChannelLayout;
using aidl::android::media::audio::eraser::Classification;
using aidl::android::media::audio::eraser::ClassificationMetadata;
using aidl::android::media::audio::eraser::ClassificationMetadataList;
using aidl::android::media::audio::eraser::ClassifierCapability;
using aidl::android::media::audio::eraser::Mode;
using aidl::android::media::audio::eraser::RemixerCapability;
using aidl::android::media::audio::eraser::SeparatorCapability;
using aidl::android::media::audio::eraser::SoundClassification;

namespace aidl::android::hardware::audio::effect {

const ClassifierCapability EraserContext::kClassifierCapability = {
        .windowSizeMs = kClassifierWindowSizeMs,
        .supportedClassifications = {
                Classification{.classification = SoundClassification::HUMAN},
                Classification{.classification = SoundClassification::ANIMAL},
                Classification{.classification = SoundClassification::NATURE},
                Classification{.classification = SoundClassification::MUSIC},
                Classification{.classification = SoundClassification::THINGS},
                Classification{.classification = SoundClassification::AMBIGUOUS},
                Classification{.classification = SoundClassification::ENVIRONMENT},
        }};

const SeparatorCapability EraserContext::kSeparatorCapability = {
        .supported = true, .maxSoundSources = kSeparatorMaxSoundNum};

const RemixerCapability EraserContext::kRemixerCapability = {
        .supported = true, .maxGainFactor = kRemixerGainFactorMax};

const EraserContext::EraserConfiguration EraserContext::kDefaultConfig = {.mode = Mode::CLASSIFIER};

const EraserContext::EraserCapability& EraserContext::getCapability() {
    static const EraserCapability cap({
            .sampleRates = {EraserContext::kClassifierSampleRate},
            .channelLayouts = {AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_MONO)},
            .modes = {Mode::ERASER, Mode::CLASSIFIER},
            .separator = EraserContext::kSeparatorCapability,
            .classifier = EraserContext::kClassifierCapability,
            .remixer = EraserContext::kRemixerCapability,
    });

    return cap;
}

EraserContext::EraserContext(int statusDepth, const Parameter::Common& common)
    : EffectContext(statusDepth, common), mCommon(common), mConfig(kDefaultConfig) {
    LOG(DEBUG) << __func__ << ": Creating EraserContext";
    init();
}

EraserContext::~EraserContext() {
    LOG(DEBUG) << __func__ << ": Destroying EraserContext";
}

void EraserContext::init() {
    mChannelCount = static_cast<int>(::aidl::android::hardware::audio::common::getChannelCount(
            mCommon.input.base.channelMask));
    if (mCommon.input.base.sampleRate != kClassifierSampleRate) {
        LOG(FATAL) << "Mismatched sample rate! Expected " << kClassifierSampleRate << " but got "
                   << mCommon.input.base.sampleRate;
    }
}

RetCode EraserContext::enable() {
    if (mConfig.mode == Mode::ERASER) {
        if (!mSeparatorInstance) {
            mSeparatorInstance = std::make_unique<LiteRTInstance>(kSeparatorModelPath);
        }
        if (mSeparatorInstance && mSeparatorInstance->initialize()) {
            mSeparatorInstance->warmup();
        } else {
            LOG(ERROR) << __func__ << ": failed to enable separator";
            return RetCode::ERROR_EFFECT_LIB_ERROR;
        }
    } else {
        mSeparatorInstance.reset();
    }

    if (!mClassifierInstance) {
        mClassifierInstance = std::make_unique<LiteRTInstance>(kClassifierModelPath);
    }
    if (mClassifierInstance && mClassifierInstance->initialize()) {
        mClassifierInstance->warmup();
    } else {
        LOG(ERROR) << __func__ << ": failed to enable classifier";
        return RetCode::ERROR_EFFECT_LIB_ERROR;
    }

    return RetCode::SUCCESS;
}

RetCode EraserContext::disable() {
    mClassifierInstance.reset();
    mSeparatorInstance.reset();
    return RetCode::SUCCESS;
}

RetCode EraserContext::reset() {
    mWorkBuffer.clear();
    return RetCode::SUCCESS;
}

std::optional<Eraser> EraserContext::getParam(Eraser::Tag tag) {
    switch (tag) {
        case Eraser::capability:
            return Eraser::make<Eraser::capability>(getCapability());
        case Eraser::configuration:
            return Eraser::make<Eraser::configuration>(mConfig);
        default: {
            LOG(ERROR) << __func__ << " unsupported tag: " << toString(tag);
            return std::nullopt;
        }
    }
}

ndk::ScopedAStatus EraserContext::setParam(Eraser eraser) {
    const auto tag = eraser.getTag();
    switch (tag) {
        case Eraser::configuration: {
            mConfig = eraser.get<Eraser::configuration>();
            mCallback = mConfig.callback;
            return ndk::ScopedAStatus::ok();
        }
        default: {
            LOG(ERROR) << __func__ << " unsupported tag: " << toString(tag);
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }
}

IEffect::Status EraserContext::process(float* in, float* out, int samples) {
    IEffect::Status procStatus = {EX_ILLEGAL_ARGUMENT, 0, 0};
    RETURN_VALUE_IF(!in, procStatus, "nullInput");
    RETURN_VALUE_IF(!out, procStatus, "nullOutput");
    RETURN_VALUE_IF(!mClassifierInstance, procStatus, "nullClassifier");

    const auto inputChCount = common::getChannelCount(mCommon.input.base.channelMask);
    const auto outputChCount = common::getChannelCount(mCommon.output.base.channelMask);
    if (inputChCount < outputChCount) {
        LOG(ERROR) << __func__ << " invalid channel count, in: " << inputChCount
                   << " out: " << outputChCount;
        return procStatus;
    }

    const size_t tensorSize = mClassifierInstance->inputTensorSize();
    // Half of the Tensor size overlapping for more robust classification
    const size_t hopSize = tensorSize >> 1;
    // Add incoming samples to end of buffer
    mWorkBuffer.insert(mWorkBuffer.end(), in, in + samples);

    while (mWorkBuffer.size() >= tensorSize) {
        std::span<float> classifierInput = mClassifierInstance->typedInputTensor<float>();
        // Copy Tensor size of data from buffer to the model's input
        std::memcpy(classifierInput.data(), mWorkBuffer.data(), tensorSize * sizeof(float));

        // Invoke the model
        if (!mClassifierInstance->invoke()) {
            LOG(ERROR) << __func__ << " classifier instance invoke failed, dropping data";
            mWorkBuffer.clear();
            break;
        }

        // Read the result directly from the model's output tensor
        const std::span<float> scores = mClassifierInstance->typedOutputTensor<float>();
        if (scores.empty()) {
            LOG(ERROR) << __func__ << " read classifier output with size "
                       << mClassifierInstance->outputTensorSize() << " failed or size mismatch";
            mWorkBuffer.clear();
            break;
        }

        // Get max scores by category
        std::unordered_map<SoundClassification, float> categoryScores;
        const auto& categoryMap = getYamnetToCustomCategoryMap();
        for (const auto& [category, indices] : categoryMap) {
            float maxScore = 0;
            for (int index : indices) {
                maxScore = std::max(scores[index], maxScore);
            }

            if (maxScore > 0) {
                categoryScores.insert({category, maxScore});
            }
        }

        // per-category thresholds
        const auto& thresholds = getCategoryThresholds();
        std::vector<ClassificationMetadata> activeCategories;
        for (const auto& [category, score] : categoryScores) {
            if (score >= thresholds.at(category)) {
                activeCategories.push_back(ClassificationMetadata{
                        .confidenceScore = score,
                        .classification = Classification{.classification = category}});
                LOG(DEBUG) << __func__ << " detected active category: " << toString(category)
                           << ", score: " << score;
            }
        }

        // TODO: update mSoundId with separator support
        // TODO: update timeMs with the calculated timestamp within the audio stream
        if (mCallback) {
            mCallback->onClassifierUpdate(
                    mSoundId,
                    ClassificationMetadataList{.timeMs = 0, .metadatas = activeCategories});
        }

        // slide the window by erasing the hop data from the front of our buffer
        mWorkBuffer.erase(mWorkBuffer.begin(), mWorkBuffer.begin() + hopSize);
    }

    // TODO: for ERASER mode, copy separated audio to output channels
    std::memcpy(out, in, samples * sizeof(float));

    const int inputFrames = samples / inputChCount;
    return IEffect::Status{.status = STATUS_OK,
                           .fmqConsumed = static_cast<int32_t>(inputFrames * inputChCount),
                           .fmqProduced = static_cast<int32_t>(inputFrames * outputChCount)};
}

}  // namespace aidl::android::hardware::audio::effect
