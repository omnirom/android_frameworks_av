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

#include "LayoutTranslation.h"

#include <gtest/gtest.h>
#include <log/log.h>

#include <iamf_tools/iamf_tools_api_types.h>

namespace android {
namespace c2_soft_iamf_internal {

// Helper to count the number of bits set in a channel mask.
size_t GetNumberChannels(uint32_t android_channel_mask) {
    size_t number_channels = 0;
    while (android_channel_mask > 0) {
        number_channels += android_channel_mask & 1;
        android_channel_mask >>= 1;
    }
    return number_channels;
}
using ::iamf_tools::api::OutputLayout;

using ::testing::TestWithParam;
using LayoutAndChannelCount = std::pair<OutputLayout, size_t>;
using LayoutRoundtripTest = TestWithParam<LayoutAndChannelCount>;

// Test converting from IAMF Layout to ChannelMask and back and count bits in the channel mask.
TEST_P(LayoutRoundtripTest, ConvertsToChannelMaskAndBack) {
    const auto& [iamf_layout, expected_number_channels] = GetParam();

    uint32_t channel_mask = GetAndroidChannelMask(iamf_layout);

    EXPECT_EQ(GetNumberChannels(channel_mask), expected_number_channels);
    EXPECT_EQ(GetIamfLayout(channel_mask), iamf_layout);
}

INSTANTIATE_TEST_SUITE_P(
        LayoutRoundtripTest_Instantiation, LayoutRoundtripTest,
        ::testing::Values(LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemA_0_2_0, 2),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemB_0_5_0, 6),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemC_2_5_0, 8),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemD_4_5_0, 10),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemE_4_5_1, 11),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemF_3_7_0, 12),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemG_4_9_0, 14),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemH_9_10_3, 24),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemI_0_7_0, 8),
                          LayoutAndChannelCount(OutputLayout::kItu2051_SoundSystemJ_4_7_0, 12),
                          LayoutAndChannelCount(OutputLayout::kIAMF_SoundSystemExtension_2_7_0, 10),
                          LayoutAndChannelCount(OutputLayout::kIAMF_SoundSystemExtension_2_3_0, 6),
                          LayoutAndChannelCount(OutputLayout::kIAMF_SoundSystemExtension_0_1_0, 1),
                          LayoutAndChannelCount(OutputLayout::kIAMF_SoundSystemExtension_6_9_0,
                                                16)));

// Test that both ChannelMask versions of 5.1.2 give the same IAMF Layout.
TEST(LayoutTranslationTest, EquivalenceOf5point1point2) {
    constexpr uint32_t android_5p1p2 = 0b1100000000000011111100;
    constexpr uint32_t iamf_5p1p2 = 0b0000010100000011111100;

    EXPECT_EQ(GetIamfLayout(android_5p1p2),
              iamf_tools::api::OutputLayout::kItu2051_SoundSystemC_2_5_0);
    EXPECT_EQ(GetIamfLayout(iamf_5p1p2),
              iamf_tools::api::OutputLayout::kItu2051_SoundSystemC_2_5_0);
}

// Test that both ChannelMask versions of 7.1.2 give the same IAMF Layout.
TEST(LayoutTranslationTest, EquivalenceOf7point1point2) {
    constexpr uint32_t android_7p1p2 = 0b1100000001100011111100;
    constexpr uint32_t iamf_7p1p2 = 0b0000010101100011111100;

    EXPECT_EQ(GetIamfLayout(android_7p1p2),
              iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0);
    EXPECT_EQ(GetIamfLayout(iamf_7p1p2),
              iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0);
}

TEST(LayoutTranslationTest, EquivalenceOf9point1point4) {
    constexpr uint32_t android_9p1p4 = 0b1100000010110101100011111100;
    constexpr uint32_t iamf_9p1p4 = 0b10110101101111111100;

    EXPECT_EQ(GetIamfLayout(android_9p1p4),
              iamf_tools::api::OutputLayout::kItu2051_SoundSystemG_4_9_0);
    EXPECT_EQ(GetIamfLayout(iamf_9p1p4),
              iamf_tools::api::OutputLayout::kItu2051_SoundSystemG_4_9_0);
}

TEST(LayoutTranslationTest, EquivalenceOf9point1point6) {
    constexpr uint32_t android_9p1p6 = 0b1100001110110101100011111100;
    constexpr uint32_t iamf_9p1p6 = 0b0000001110110101101111111100;

    EXPECT_EQ(GetIamfLayout(android_9p1p6),
              iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0);
    EXPECT_EQ(GetIamfLayout(iamf_9p1p6),
              iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0);
}

TEST(LayoutTranslationTest, NonMatchingChannelMaskBecomesStereo) {
    constexpr uint32_t arbitrary_value = 0b101010101010101010;

    EXPECT_EQ(GetIamfLayout(arbitrary_value), OutputLayout::kItu2051_SoundSystemA_0_2_0);
}

}  // namespace c2_soft_iamf_internal
}  // namespace android
