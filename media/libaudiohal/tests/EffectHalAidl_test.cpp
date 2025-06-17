/*
 * Copyright 2025 The Android Open Source Project
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

#define LOG_TAG "EffectHalAidlTest"

#include "EffectHalAidl.h"

#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include <aidl/android/hardware/audio/effect/BnFactory.h>

#include <aidl/android/hardware/audio/effect/Parameter.h>
#include <android/binder_status.h>
#include <media/AudioDeviceTypeAddr.h>
#include <utils/Log.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {

using ::aidl::android::hardware::audio::effect::CommandId;
using ::aidl::android::hardware::audio::effect::Descriptor;
using ::aidl::android::hardware::audio::effect::IEffect;
using ::aidl::android::hardware::audio::effect::Parameter;
using ::aidl::android::hardware::audio::effect::Processing;
using ::aidl::android::hardware::audio::effect::State;
using ::aidl::android::media::audio::common::AudioDeviceDescription;
using ::aidl::android::media::audio::common::AudioDeviceType;
using ::aidl::android::media::audio::common::AudioUuid;
using android::AudioDeviceTypeAddr;
using android::AudioDeviceTypeAddrVector;
using android::OK;
using android::sp;
using android::effect::EffectHalAidl;

using ::testing::_;
using ::testing::Return;

class IFactoryMock : public ::aidl::android::hardware::audio::effect::BnFactory {
  public:
    IFactoryMock() = default;

    ndk::ScopedAStatus queryEffects(const std::optional<AudioUuid>&,
                                    const std::optional<AudioUuid>&,
                                    const std::optional<AudioUuid>&,
                                    std::vector<Descriptor>*) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus queryProcessing(const std::optional<Processing::Type>&,
                                       std::vector<Processing>*) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus createEffect(const AudioUuid&, std::shared_ptr<IEffect>*) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus destroyEffect(const std::shared_ptr<IEffect>&) override {
        return ndk::ScopedAStatus::ok();
    }
};

class IEffectMock : public ::aidl::android::hardware::audio::effect::BnEffect {
  public:
    IEffectMock() = default;

    MOCK_METHOD(ndk::ScopedAStatus, open,
                (const Parameter::Common& common, const std::optional<Parameter::Specific>& spec,
                 IEffect::OpenEffectReturn* ret),
                (override));
    MOCK_METHOD(ndk::ScopedAStatus, close, (), (override));
    MOCK_METHOD(ndk::ScopedAStatus, getDescriptor, (Descriptor * desc), (override));
    MOCK_METHOD(ndk::ScopedAStatus, command, (CommandId commandId), (override));
    MOCK_METHOD(ndk::ScopedAStatus, getState, (State * state), (override));
    MOCK_METHOD(ndk::ScopedAStatus, getParameter, (const Parameter::Id& id, Parameter* param),
                (override));
    MOCK_METHOD(ndk::ScopedAStatus, reopen, (IEffect::OpenEffectReturn * ret), (override));

    ndk::ScopedAStatus setParameter(const Parameter& param) {
        if (param == mExpectParam)
            return ndk::ScopedAStatus::ok();
        else {
            ALOGW("%s mismatch, %s vs %s", __func__, param.toString().c_str(),
                  mExpectParam.toString().c_str());
            return ndk::ScopedAStatus::fromStatus(STATUS_BAD_VALUE);
        }
    }

    void setExpectParameter(const Parameter& param) { mExpectParam = param; }

  private:
    Parameter mExpectParam;
};

// Predefined vector of {audio_devices_t, AudioDeviceDescription} pair
static const std::vector<std::pair<audio_devices_t, AudioDeviceDescription>>& kAudioDevicePairs = {
        {AUDIO_DEVICE_NONE, AudioDeviceDescription{.type = AudioDeviceType::NONE}},
        {AUDIO_DEVICE_OUT_EARPIECE,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_SPEAKER_EARPIECE}},
        {AUDIO_DEVICE_OUT_SPEAKER, AudioDeviceDescription{.type = AudioDeviceType::OUT_SPEAKER}},
        {AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADPHONE,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_SCO,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_BT_SCO}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_CARKIT,
                                .connection = AudioDeviceDescription::CONNECTION_BT_SCO}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADPHONE,
                                .connection = AudioDeviceDescription::CONNECTION_BT_A2DP}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_SPEAKER,
                                .connection = AudioDeviceDescription::CONNECTION_BT_A2DP}},
        {AUDIO_DEVICE_OUT_TELEPHONY_TX,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_TELEPHONY_TX}},
        {AUDIO_DEVICE_OUT_AUX_LINE, AudioDeviceDescription{.type = AudioDeviceType::OUT_LINE_AUX}},
        {AUDIO_DEVICE_OUT_SPEAKER_SAFE,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_SPEAKER_SAFE}},
        {AUDIO_DEVICE_OUT_HEARING_AID,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEARING_AID,
                                .connection = AudioDeviceDescription::CONNECTION_WIRELESS}},
        {AUDIO_DEVICE_OUT_ECHO_CANCELLER,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_ECHO_CANCELLER}},
        {AUDIO_DEVICE_OUT_BLE_SPEAKER,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_SPEAKER,
                                .connection = AudioDeviceDescription::CONNECTION_BT_LE}},
        {AUDIO_DEVICE_OUT_BLE_BROADCAST,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_BROADCAST,
                                .connection = AudioDeviceDescription::CONNECTION_BT_LE}},
        {AUDIO_DEVICE_OUT_MULTICHANNEL_GROUP,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_MULTICHANNEL_GROUP,
                                .connection = AudioDeviceDescription::CONNECTION_VIRTUAL}},

        {AUDIO_DEVICE_OUT_DEFAULT, AudioDeviceDescription{.type = AudioDeviceType::OUT_DEFAULT}},
        {AUDIO_DEVICE_OUT_WIRED_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_BT_SCO}},
        {AUDIO_DEVICE_OUT_HDMI,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI}},
        {AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DOCK,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DOCK,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_OUT_USB_ACCESSORY,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_ACCESSORY,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_OUT_USB_DEVICE,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_OUT_FM, AudioDeviceDescription{.type = AudioDeviceType::OUT_FM}},
        {AUDIO_DEVICE_OUT_LINE,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_OUT_SPDIF,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_SPDIF}},
        {AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_BT_A2DP}},
        {AUDIO_DEVICE_OUT_IP,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_IP_V4}},
        {AUDIO_DEVICE_OUT_BUS, AudioDeviceDescription{.type = AudioDeviceType::OUT_BUS}},
        {AUDIO_DEVICE_OUT_PROXY,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_AFE_PROXY,
                                .connection = AudioDeviceDescription::CONNECTION_VIRTUAL}},
        {AUDIO_DEVICE_OUT_USB_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_OUT_HDMI_ARC,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI_ARC}},
        {AUDIO_DEVICE_OUT_HDMI_EARC,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI_EARC}},
        {AUDIO_DEVICE_OUT_BLE_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_BT_LE}},
        {AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
         AudioDeviceDescription{.type = AudioDeviceType::OUT_SUBMIX,
                                .connection = AudioDeviceDescription::CONNECTION_VIRTUAL}},
        {AUDIO_DEVICE_IN_BUILTIN_MIC,
         AudioDeviceDescription{.type = AudioDeviceType::IN_MICROPHONE}},
        {AUDIO_DEVICE_IN_BACK_MIC,
         AudioDeviceDescription{.type = AudioDeviceType::IN_MICROPHONE_BACK}},
        {AUDIO_DEVICE_IN_TELEPHONY_RX,
         AudioDeviceDescription{.type = AudioDeviceType::IN_TELEPHONY_RX}},
        {AUDIO_DEVICE_IN_TV_TUNER, AudioDeviceDescription{.type = AudioDeviceType::IN_TV_TUNER}},
        {AUDIO_DEVICE_IN_LOOPBACK, AudioDeviceDescription{.type = AudioDeviceType::IN_LOOPBACK}},
        {AUDIO_DEVICE_IN_BLUETOOTH_BLE,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_BT_LE}},
        {AUDIO_DEVICE_IN_ECHO_REFERENCE,
         AudioDeviceDescription{.type = AudioDeviceType::IN_ECHO_REFERENCE}},
        {AUDIO_DEVICE_IN_DEFAULT, AudioDeviceDescription{.type = AudioDeviceType::IN_DEFAULT}},
        {AUDIO_DEVICE_IN_WIRED_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_BT_SCO}},
        {AUDIO_DEVICE_IN_HDMI,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI}},
        {AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DOCK,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DOCK,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_IN_USB_ACCESSORY,
         AudioDeviceDescription{.type = AudioDeviceType::IN_ACCESSORY,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_IN_USB_DEVICE,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_IN_FM_TUNER, AudioDeviceDescription{.type = AudioDeviceType::IN_FM_TUNER}},
        {AUDIO_DEVICE_IN_LINE,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_ANALOG}},
        {AUDIO_DEVICE_IN_SPDIF,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_SPDIF}},
        {AUDIO_DEVICE_IN_BLUETOOTH_A2DP,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_BT_A2DP}},
        {AUDIO_DEVICE_IN_IP,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_IP_V4}},
        {AUDIO_DEVICE_IN_BUS, AudioDeviceDescription{.type = AudioDeviceType::IN_BUS}},
        {AUDIO_DEVICE_IN_PROXY,
         AudioDeviceDescription{.type = AudioDeviceType::IN_AFE_PROXY,
                                .connection = AudioDeviceDescription::CONNECTION_VIRTUAL}},
        {AUDIO_DEVICE_IN_USB_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_USB}},
        {AUDIO_DEVICE_IN_HDMI_ARC,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI_ARC}},
        {AUDIO_DEVICE_IN_HDMI_EARC,
         AudioDeviceDescription{.type = AudioDeviceType::IN_DEVICE,
                                .connection = AudioDeviceDescription::CONNECTION_HDMI_EARC}},
        {AUDIO_DEVICE_IN_BLE_HEADSET,
         AudioDeviceDescription{.type = AudioDeviceType::IN_HEADSET,
                                .connection = AudioDeviceDescription::CONNECTION_BT_LE}},
        {AUDIO_DEVICE_IN_REMOTE_SUBMIX,
         AudioDeviceDescription{.type = AudioDeviceType::IN_SUBMIX,
                                .connection = AudioDeviceDescription::CONNECTION_VIRTUAL}},
};

}  // namespace

class EffectHalAidlTest : public testing::Test {
  public:
    void SetUp() override {
        mIEffectMock = ndk::SharedRefBase::make<IEffectMock>();
        mIFactoryMock = ndk::SharedRefBase::make<IFactoryMock>();
        mEffect = sp<EffectHalAidl>::make(mIFactoryMock, mIEffectMock, 0 /*session*/, 0 /*ioId*/,
                                          mDescMock /* descriptor */, false /*isProxyEffect*/);
    }
    void TearDown() override {
        mIEffectMock.reset();
        mIFactoryMock.reset();
        mEffect.clear();
    }

    // Helper function to setDevice with one to multi (window size) device pairs set to effect, and
    // expect the same from IEffect mocking object.
    void setDevicesWithWindow(size_t window = 1) {
        for (size_t i = 0; i + window < kAudioDevicePairs.size(); i += window) {
            AudioDeviceTypeAddrVector deviceTypes;
            std::vector<AudioDeviceDescription> deviceDescs;
            for (size_t w = 0; w < window; w++) {
                deviceTypes.emplace_back(kAudioDevicePairs[i + w].first /* audio_device_t */, "");
                deviceDescs.emplace_back(
                        kAudioDevicePairs[i + w].second /* AudioDeviceDescription */);
            }
            const Parameter expect = Parameter::make<Parameter::deviceDescription>(deviceDescs);
            mIEffectMock->setExpectParameter(expect);
            EXPECT_EQ(OK, mEffect->setDevices(deviceTypes))
                    << " setDevices: "
                    << dumpAudioDeviceTypeAddrVector(deviceTypes, false /*includeSensitiveInfo*/)
                    << " expect: " << expect.toString();
        }
    }

  protected:
    std::shared_ptr<IEffectMock> mIEffectMock;
    std::shared_ptr<IFactoryMock> mIFactoryMock;
    Descriptor mDescMock;
    sp<EffectHalAidl> mEffect;
};

TEST_F(EffectHalAidlTest, emptyDeviceSet) {
    AudioDeviceTypeAddr deviceType(AUDIO_DEVICE_NONE, "");
    AudioDeviceTypeAddrVector deviceTypes{deviceType};
    std::vector<AudioDeviceDescription> deviceDescs;

    Parameter expect = Parameter::make<Parameter::deviceDescription>(deviceDescs);
    mIEffectMock->setExpectParameter(expect);
    EXPECT_NE(OK, mEffect->setDevices(deviceTypes))
            << " expecting error with setDevices: "
            << dumpAudioDeviceTypeAddrVector(deviceTypes, false /*includeSensitiveInfo*/)
            << " expect: " << expect.toString();

    deviceDescs.emplace_back(AudioDeviceDescription{.type = AudioDeviceType::NONE});
    expect = Parameter::make<Parameter::deviceDescription>(deviceDescs);
    mIEffectMock->setExpectParameter(expect);
    EXPECT_EQ(OK, mEffect->setDevices(deviceTypes))
            << " setDevices: "
            << dumpAudioDeviceTypeAddrVector(deviceTypes, false /*includeSensitiveInfo*/)
            << " expect: " << expect.toString();
}

// go over the `kAudioDevicePairs` pair, and setDevice for each pair
TEST_F(EffectHalAidlTest, deviceSinglePairSet) {
    ASSERT_NO_FATAL_FAILURE(setDevicesWithWindow());
}

// SetDevice with multiple device pairs from `kAudioDevicePairs`
TEST_F(EffectHalAidlTest, deviceMultiplePairSet) {
    for (size_t window = 2; window < kAudioDevicePairs.size(); window++) {
        ASSERT_NO_FATAL_FAILURE(setDevicesWithWindow(window));
    }
}
