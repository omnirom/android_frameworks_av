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

#include "dsp/core/dynamic_range_compression.h"
#include <audio_effects/effect_loudnessenhancer.h>
#include <audio_utils/dsp_utils.h>
#include <gtest/gtest.h>
#include <log/log.h>
#include <system/audio_effects/audio_effects_test.h>

using status_t = int32_t;
extern audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM;
effect_uuid_t loudness_uuid = {0xfa415329, 0x2034, 0x4bea, 0xb5dc,
    {0x5b, 0x38, 0x1c, 0x8d, 0x1e, 0x2c}};

using namespace android::audio_utils;
using namespace android::effect::utils;

/*
Android 16:
expectedEnergydB: -24.771212  energyIndB: -24.739433
gaindB: 0.000000  measureddB: 0.000000  energyIndB: -24.739433  energyOutdB: -24.739433
gaindB: 1.000000  measureddB: 1.000004  energyIndB: -24.739433  energyOutdB: -23.739429
gaindB: 2.000000  measureddB: 2.000002  energyIndB: -24.739433  energyOutdB: -22.739431
gaindB: 5.000000  measureddB: 5.000006  energyIndB: -24.739433  energyOutdB: -19.739428
gaindB: 10.000000  measureddB: 10.000004  energyIndB: -24.739433  energyOutdB: -14.739429
gaindB: 20.000000  measureddB: 13.513464  energyIndB: -24.739433  energyOutdB: -11.225969
gaindB: 50.000000  measureddB: 18.649250  energyIndB: -24.739433  energyOutdB: -6.090182
gaindB: 100.000000  measureddB: 22.874735  energyIndB: -24.739433  energyOutdB: -1.864698
 */

static constexpr audio_channel_mask_t kOutputChannelMasks[] = {
AUDIO_CHANNEL_OUT_STEREO,
AUDIO_CHANNEL_OUT_5POINT1,
AUDIO_CHANNEL_OUT_7POINT1,
AUDIO_CHANNEL_OUT_7POINT1POINT4,
AUDIO_CHANNEL_OUT_9POINT1POINT6,
};

using LoudnessEnhancerGainParam = std::tuple<int /* channel mask */>;

enum {
    GAIN_CHANNEL_MASK_POSITION = 0,
    //GAIN_ACCUMULATE_POSITION = 1,
};

class LoudnessEnhancerGainTest : public ::testing::TestWithParam<LoudnessEnhancerGainParam> {
public:

    void testGain(audio_channel_mask_t channelMask) {
        effect_handle_t handle;
        ASSERT_EQ(0, AUDIO_EFFECT_LIBRARY_INFO_SYM.create_effect(
                &loudness_uuid, 0 /* sessionId */, 0 /* ioId */, &handle));

        constexpr size_t frameCount = 1024;
        constexpr uint32_t sampleRate = 48000;
        const size_t channelCount = audio_channel_count_from_out_mask(channelMask);
        if (channelCount > FCC_LIMIT) return;
        constexpr float amplitude = 0.1;
        const size_t sampleCount = channelCount * frameCount;
        std::vector<float> originalData(sampleCount);
        initUniformDistribution(originalData, -amplitude, amplitude);
        std::vector<float> outData(sampleCount);

        ASSERT_EQ(0, effect_set_config(handle, sampleRate, channelMask));
        ASSERT_EQ(0, effect_enable(handle));

        // expected energy in dB for a uniform distribution from -amplitude to amplitude.
        const float expectedEnergydB = energyOfUniformDistribution(-amplitude, amplitude);
        const float energyIndB = energy(originalData);
        ALOGD("%s: expectedEnergydB: %f  energyIndB: %f", __func__, expectedEnergydB, energyIndB);
        EXPECT_NEAR(energyIndB, expectedEnergydB, 0.1);  // within 0.1dB.
        float lastMeasuredGaindB = 0;
        for (int gainmB : { 0, 100, 200, 500, 1'000, 2'000, 5'000, 10'000 }) {  // millibel Power
            ASSERT_EQ(0, effect_set_param(
                    handle, LOUDNESS_ENHANCER_PARAM_TARGET_GAIN_MB, gainmB));

            auto inData = originalData;
            audio_buffer_t inBuffer{ .frameCount = frameCount, .f32 = inData.data() };
            audio_buffer_t outBuffer{ .frameCount = frameCount, .f32 = outData.data() };
            ASSERT_EQ(0, effect_process(handle, &inBuffer, &outBuffer));
            const float energyOutdB = energy(inData);
            const float gaindB = gainmB * 1e-2;
            const float measuredGaindB = energyOutdB - energyIndB;

            // Log our gain and power levels
            ALOGD("%s: gaindB: %f  measureddB: %f  energyIndB: %f  energyOutdB: %f",
                  __func__, gaindB, measuredGaindB, energyIndB, energyOutdB);

            // Gain curve testing (move to VTS)?
            if (gaindB == 0) {
                EXPECT_EQ(energyIndB, energyOutdB);
            } else if (energyIndB + gaindB < -10.f) {
                // less than -10dB from overflow, signal does not saturate.
                EXPECT_NEAR(gaindB, measuredGaindB, 0.1);
            } else {  // effective gain saturates.
                EXPECT_LT(measuredGaindB, gaindB);       // less than the desired gain.
                EXPECT_GT(measuredGaindB, lastMeasuredGaindB);  // more than the previous gain.
            }
            lastMeasuredGaindB = measuredGaindB;
        }
        ASSERT_EQ(0, AUDIO_EFFECT_LIBRARY_INFO_SYM.release_effect(handle));
    }
};

/**
 * The Gain test checks that gain that does not saturate the input signal
 * will be applied as expected.  Gain that would cause the input signal to
 * exceed the nominal limit is reduced.
 */

TEST_P(LoudnessEnhancerGainTest, gain) {
    testGain(kOutputChannelMasks[std::get<GAIN_CHANNEL_MASK_POSITION>(GetParam())]);
}

INSTANTIATE_TEST_SUITE_P(
        LoudnessEnhancerTestAll, LoudnessEnhancerGainTest,
        ::testing::Combine(
                ::testing::Range(0, (int)std::size(kOutputChannelMasks))),
        [](const testing::TestParamInfo<LoudnessEnhancerGainTest::ParamType>& info) {
            const int index = std::get<GAIN_CHANNEL_MASK_POSITION>(info.param);
            const audio_channel_mask_t channelMask = kOutputChannelMasks[index];
            const std::string name =
                    std::string(audio_channel_out_mask_to_string(channelMask)) +
                    std::to_string(index);
            return name;
        });
