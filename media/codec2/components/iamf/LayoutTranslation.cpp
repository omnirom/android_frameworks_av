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

#include <iamf_tools/iamf_tools_api_types.h>
#include <log/log.h>

namespace android {
namespace c2_soft_iamf_internal {

namespace {
// All values of CHANNEL_OUT_* are copied from android.media.AudioFormat.java
// but only including the values needed.
constexpr uint32_t CHANNEL_OUT_FRONT_LEFT = 0x4;
constexpr uint32_t CHANNEL_OUT_FRONT_RIGHT = 0x8;
constexpr uint32_t CHANNEL_OUT_FRONT_CENTER = 0x10;
constexpr uint32_t CHANNEL_OUT_LOW_FREQUENCY = 0x20;
constexpr uint32_t CHANNEL_OUT_BACK_LEFT = 0x40;
constexpr uint32_t CHANNEL_OUT_BACK_RIGHT = 0x80;
constexpr uint32_t CHANNEL_OUT_FRONT_LEFT_OF_CENTER = 0x100;
constexpr uint32_t CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x200;
constexpr uint32_t CHANNEL_OUT_BACK_CENTER = 0x400;
constexpr uint32_t CHANNEL_OUT_SIDE_LEFT = 0x800;
constexpr uint32_t CHANNEL_OUT_SIDE_RIGHT = 0x1000;
constexpr uint32_t CHANNEL_OUT_TOP_CENTER = 0x2000;
constexpr uint32_t CHANNEL_OUT_TOP_FRONT_LEFT = 0x4000;
constexpr uint32_t CHANNEL_OUT_TOP_FRONT_CENTER = 0x8000;
constexpr uint32_t CHANNEL_OUT_TOP_FRONT_RIGHT = 0x10000;
constexpr uint32_t CHANNEL_OUT_TOP_BACK_LEFT = 0x20000;
constexpr uint32_t CHANNEL_OUT_TOP_BACK_CENTER = 0x40000;
constexpr uint32_t CHANNEL_OUT_TOP_BACK_RIGHT = 0x80000;
// These two only used for Android's (non-IAMF/non-ITU) 5.1.2 and 7.1.2.
constexpr uint32_t CHANNEL_OUT_TOP_SIDE_LEFT = 0x100000;
constexpr uint32_t CHANNEL_OUT_TOP_SIDE_RIGHT = 0x200000;
// Bottom channels used by ITU Sound Systems E (4+5+1) and H (9+10+3).
constexpr uint32_t CHANNEL_OUT_BOTTOM_FRONT_LEFT = 0x400000;
constexpr uint32_t CHANNEL_OUT_BOTTOM_FRONT_CENTER = 0x800000;
constexpr uint32_t CHANNEL_OUT_BOTTOM_FRONT_RIGHT = 0x1000000;
// Used by ITU Sound System F (3+7+0) and H (9+10+3).
constexpr uint32_t CHANNEL_OUT_LOW_FREQUENCY_2 = 0x2000000;
// Used by layouts with 9 speakers in the middle plane (9.1.4, 9.1.6, ITU H).
constexpr uint32_t CHANNEL_OUT_FRONT_WIDE_LEFT = 0x4000000;
constexpr uint32_t CHANNEL_OUT_FRONT_WIDE_RIGHT = 0x8000000;

// All of these CHANNEL_OUT_ combinations are Android-defined combinations of ChannelMasks that
// match IAMF OutputLayouts and are copied from AudioFormat.java.
constexpr uint32_t CHANNEL_OUT_MONO = CHANNEL_OUT_FRONT_LEFT;
constexpr uint32_t CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT);
constexpr uint32_t CHANNEL_OUT_5POINT1 =
        (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT | CHANNEL_OUT_FRONT_CENTER |
         CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT);
constexpr uint32_t CHANNEL_OUT_5POINT1POINT4 =
        (CHANNEL_OUT_5POINT1 | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT |
         CHANNEL_OUT_TOP_BACK_LEFT | CHANNEL_OUT_TOP_BACK_RIGHT);
constexpr uint32_t CHANNEL_OUT_7POINT1_SURROUND =
        (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_FRONT_RIGHT |
         CHANNEL_OUT_SIDE_LEFT | CHANNEL_OUT_SIDE_RIGHT | CHANNEL_OUT_BACK_LEFT |
         CHANNEL_OUT_BACK_RIGHT | CHANNEL_OUT_LOW_FREQUENCY);
constexpr uint32_t CHANNEL_OUT_7POINT1POINT4 =
        (CHANNEL_OUT_7POINT1_SURROUND | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT |
         CHANNEL_OUT_TOP_BACK_LEFT | CHANNEL_OUT_TOP_BACK_RIGHT);
constexpr uint32_t CHANNEL_OUT_9POINT1POINT4 =
        (CHANNEL_OUT_7POINT1POINT4 | CHANNEL_OUT_FRONT_WIDE_LEFT | CHANNEL_OUT_FRONT_WIDE_RIGHT);
constexpr uint32_t CHANNEL_OUT_9POINT1POINT6 =
        (CHANNEL_OUT_9POINT1POINT4 | CHANNEL_OUT_TOP_SIDE_LEFT | CHANNEL_OUT_TOP_SIDE_RIGHT);
constexpr uint32_t CHANNEL_OUT_22POINT2 =
        (CHANNEL_OUT_7POINT1POINT4 | CHANNEL_OUT_FRONT_LEFT_OF_CENTER |
         CHANNEL_OUT_FRONT_RIGHT_OF_CENTER | CHANNEL_OUT_BACK_CENTER | CHANNEL_OUT_TOP_CENTER |
         CHANNEL_OUT_TOP_FRONT_CENTER | CHANNEL_OUT_TOP_BACK_CENTER | CHANNEL_OUT_TOP_SIDE_LEFT |
         CHANNEL_OUT_TOP_SIDE_RIGHT | CHANNEL_OUT_BOTTOM_FRONT_LEFT |
         CHANNEL_OUT_BOTTOM_FRONT_RIGHT | CHANNEL_OUT_BOTTOM_FRONT_CENTER |
         CHANNEL_OUT_LOW_FREQUENCY_2);

// These 5.1.2 and 7.1.2, are copied from AudioFormat.java, but do not match the ITU standard/IAMF
// spec because they use top side L/R rather than top front L/R, but we will match it to the
// ITU/IAMF 5.1.2 and 7.1.2.
constexpr uint32_t CHANNEL_OUT_5POINT1POINT2 =
        (CHANNEL_OUT_5POINT1 | CHANNEL_OUT_TOP_SIDE_LEFT | CHANNEL_OUT_TOP_SIDE_RIGHT);
constexpr uint32_t CHANNEL_OUT_7POINT1POINT2 =
        (CHANNEL_OUT_7POINT1_SURROUND | CHANNEL_OUT_TOP_SIDE_LEFT | CHANNEL_OUT_TOP_SIDE_RIGHT);

// The above CHANNEL_OUT_9POINT1POINT4 and CHANNEL_OUT_9POINT1POINT6 use _WIDE_LEFT and _WIDE_RIGHT
// but the ITU spec for Sound System G says "left [/right] screen edge" and the ITU spec for Sound
// System H, which is the basis for the IAMF 9.1.6 uses LEFT/RIGHT_OF_CENTER so we'll allow
// permissive matching.
constexpr uint32_t IAMF_9POINT1POINT4 =
        (CHANNEL_OUT_7POINT1POINT4 | CHANNEL_OUT_FRONT_LEFT_OF_CENTER |
         CHANNEL_OUT_FRONT_RIGHT_OF_CENTER);
constexpr uint32_t IAMF_9POINT1POINT6 =
        (IAMF_9POINT1POINT4 | CHANNEL_OUT_TOP_SIDE_LEFT | CHANNEL_OUT_TOP_SIDE_RIGHT);

// Sound Systems E, F, and H are defined in ITU B.S. 2051-3 but are not defined by Android.
// We can make them from combinations of speakers available in Android.
constexpr uint32_t ITU_2051_SOUND_SYSTEM_E_4_5_1 =
        (CHANNEL_OUT_5POINT1POINT4 | CHANNEL_OUT_BOTTOM_FRONT_CENTER);
constexpr uint32_t ITU_2051_SOUND_SYSTEM_F_3_7_0 =
        (CHANNEL_OUT_7POINT1_SURROUND | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT |
         CHANNEL_OUT_TOP_BACK_CENTER | CHANNEL_OUT_LOW_FREQUENCY_2);

// The Android defined 5.1.2 (ITU Sound System C) and 7.1.2 use top *side* left/right which does not
// match their ITU/IAMF equivalents. We will define the ITU/IAMF versions here.
constexpr uint32_t ITU_2051_SOUND_SYSTEM_C_2_5_0 =
        (CHANNEL_OUT_5POINT1 | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT);
constexpr uint32_t IAMF_7POINT1POINT2 =
        (CHANNEL_OUT_7POINT1_SURROUND | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT);

// This is just an IAMF layout that does not have an Android-defined version.
constexpr uint32_t IAMF_3POINT1POINT2 =
        (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT | CHANNEL_OUT_FRONT_CENTER |
         CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_TOP_FRONT_LEFT | CHANNEL_OUT_TOP_FRONT_RIGHT);
}  // namespace

iamf_tools::api::OutputLayout GetIamfLayout(uint32_t channelMask) {
    switch (channelMask) {
        case CHANNEL_OUT_STEREO:
            // ITU-R B.S. 2051-3 sound system A (0+2+0), commonly known as Stereo.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0;
        case CHANNEL_OUT_5POINT1:
            // ITU-R B.S. 2051-3 sound system B (0+5+0), commonly known as 5.1.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemB_0_5_0;
        case CHANNEL_OUT_5POINT1POINT2:
        case ITU_2051_SOUND_SYSTEM_C_2_5_0:
            // Here we match both the ITU/IAMF 5.1.2 as well as Android's 5.1.2.
            // ITU-R B.S. 2051-3 sound system C (2+5+0), commonly known as 5.1.2.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemC_2_5_0;
        case CHANNEL_OUT_5POINT1POINT4:
            // ITU-R B.S. 2051-3 sound system D (4+5+0), commonly known as 5.1.4.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemD_4_5_0;
        case ITU_2051_SOUND_SYSTEM_E_4_5_1:
            // ITU-R B.S. 2051-3 sound system E (4+5+1).
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemE_4_5_1;
        case ITU_2051_SOUND_SYSTEM_F_3_7_0:
            // ITU-R B.S. 2051-3 sound system F (3+7+0).
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemF_3_7_0;
        case CHANNEL_OUT_9POINT1POINT4:
        case IAMF_9POINT1POINT4:
            // ITU-R B.S. 2051-3 sound system G (4+9+0).
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemG_4_9_0;
        case CHANNEL_OUT_22POINT2:
            // ITU-R B.S. 2051-3 sound system H (9+10+3), commonly known as 22.2.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemH_9_10_3;
        case CHANNEL_OUT_7POINT1_SURROUND:
            // ITU-R B.S. 2051-3 sound system I (0+7+0), commonly known as 7.1.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemI_0_7_0;
        case CHANNEL_OUT_7POINT1POINT4:
            // ITU-R B.S. 2051-3 sound system J (4+7+0), commonly known as 7.1.4.
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemJ_4_7_0;
        case CHANNEL_OUT_7POINT1POINT2:
        case IAMF_7POINT1POINT2:
            // IAMF extension 7.1.2.
            return iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0;
        case IAMF_3POINT1POINT2:
            // IAMF extension 3.1.2.
            return iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0;
        case CHANNEL_OUT_MONO:
            // Mono.
            return iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0;
        case CHANNEL_OUT_9POINT1POINT6:
        case IAMF_9POINT1POINT6:
            // IAMF Extension 9.1.6.
            return iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0;
        default:
            ALOGW("No matching IAMF Layout found for given ChannelMask: %d.  Defaulting to Stereo.",
                  channelMask);
            return iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0;
    }
}

uint32_t GetAndroidChannelMask(iamf_tools::api::OutputLayout iamf_layout) {
    switch (iamf_layout) {
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0:
            return CHANNEL_OUT_STEREO;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemB_0_5_0:
            return CHANNEL_OUT_5POINT1;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemC_2_5_0:
            return ITU_2051_SOUND_SYSTEM_C_2_5_0;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemD_4_5_0:
            return CHANNEL_OUT_5POINT1POINT4;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemE_4_5_1:
            return ITU_2051_SOUND_SYSTEM_E_4_5_1;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemF_3_7_0:
            return ITU_2051_SOUND_SYSTEM_F_3_7_0;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemG_4_9_0:
            return CHANNEL_OUT_9POINT1POINT4;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemH_9_10_3:
            return CHANNEL_OUT_22POINT2;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemI_0_7_0:
            return CHANNEL_OUT_7POINT1_SURROUND;
        case iamf_tools::api::OutputLayout::kItu2051_SoundSystemJ_4_7_0:
            return CHANNEL_OUT_7POINT1POINT4;
        case iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0:
            return IAMF_7POINT1POINT2;
        case iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0:
            return IAMF_3POINT1POINT2;
        case iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0:
            return CHANNEL_OUT_MONO;
        case iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0:
            return CHANNEL_OUT_9POINT1POINT6;
        default:
            ALOGW("Invalid iamf_tools::api::OutputLayout received.  Returning stereo.");
            return CHANNEL_OUT_STEREO;
    }
}

}  // namespace c2_soft_iamf_internal
}  // namespace android