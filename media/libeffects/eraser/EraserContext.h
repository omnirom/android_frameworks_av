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

#pragma once

#include <memory>
#include <optional>
#include <string>

#include <aidl/android/hardware/audio/effect/Eraser.h>
#include <aidl/android/media/audio/eraser/Capability.h>
#include <aidl/android/media/audio/eraser/Configuration.h>
#include <aidl/android/media/audio/eraser/IEraserCallback.h>

#include "effect-impl/EffectContext.h"
#include "LiteRTInstance.h"

namespace aidl::android::hardware::audio::effect {

class EraserContext final : public EffectContext {
  public:
    EraserContext(int statusDepth, const Parameter::Common& common);
    ~EraserContext() final;

    RetCode enable() override;
    RetCode disable() override;
    RetCode reset() override;

    std::optional<Eraser> getParam(Eraser::Tag tag);
    ndk::ScopedAStatus setParam(Eraser eraser);
    IEffect::Status process(float* in, float* out, int samples);

    using EraserConfiguration = android::media::audio::eraser::Configuration;
    using EraserCapability = android::media::audio::eraser::Capability;

    static const EraserCapability& getCapability();

  private:

    // supported default configurations for the eraser implementation
    static constexpr int kClassifierSampleRate = 16000;
    // 1 second classifier window
    static constexpr int kClassifierWindowSizeMs = 1000;
    // max number of sounds can be separated
    static constexpr int kSeparatorMaxSoundNum = 8;
    // max gain factor for the remixer
    static constexpr float kRemixerGainFactorMax = 1.2f;

    static const EraserConfiguration kDefaultConfig;

    static const android::media::audio::eraser::ClassifierCapability kClassifierCapability;
    static const android::media::audio::eraser::SeparatorCapability kSeparatorCapability;
    static const android::media::audio::eraser::RemixerCapability kRemixerCapability;
    static const EraserCapability kCapability;

    // yamnet model was used for classifier:
    // https://github.com/tensorflow/models/tree/master/research/audioset/yamnet
    const std::string kClassifierModelPath =
            "/apex/com.android.hardware.audio/etc/models/classifier.tflite";

    // neurips2020_mixit model was used for separator:
    // https://github.com/google-research/sound-separation/tree/master/models/neurips2020_mixit
    const std::string kSeparatorModelPath =
            "/apex/com.android.hardware.audio/etc/models/separator.tflite";

    int mChannelCount = 0;
    Parameter::Common mCommon;
    EraserConfiguration mConfig;
    int mSoundId = 0;
    std::shared_ptr<android::media::audio::eraser::IEraserCallback> mCallback;

    std::unique_ptr<LiteRTInstance> mClassifierInstance;
    std::unique_ptr<LiteRTInstance> mSeparatorInstance;
    std::vector<float> mWorkBuffer;

    void init();
};

}  // namespace aidl::android::hardware::audio::effect