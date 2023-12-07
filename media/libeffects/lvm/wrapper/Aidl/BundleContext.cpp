/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <cstddef>
#include <cstdio>

#define LOG_TAG "BundleContext"
#include <android-base/logging.h>
#include <audio_utils/power.h>
#include <Utils.h>

#include "BundleContext.h"
#include "BundleTypes.h"
#include "math.h"

namespace aidl::android::hardware::audio::effect {

using ::aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::media::audio::common::AudioDeviceDescription;
using ::aidl::android::media::audio::common::AudioDeviceType;

RetCode BundleContext::init() {
    std::lock_guard lg(mMutex);
    // init with pre-defined preset NORMAL
    for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
        mBandGainMdB[i] = lvm::kSoftPresets[0 /* normal */][i] * 100;
    }

    // allocate lvm instance
    LVM_ReturnStatus_en status;
    LVM_InstParams_t params = {.BufferMode = LVM_UNMANAGED_BUFFERS,
                               .MaxBlockSize = lvm::MAX_CALL_SIZE,
                               .EQNB_NumBands = lvm::MAX_NUM_BANDS,
                               .PSA_Included = LVM_PSA_ON};
    status = LVM_GetInstanceHandle(&mInstance, &params);
    GOTO_IF_LVM_ERROR(status, deinit, "LVM_GetInstanceHandleFailed");

    // set control
    LVM_ControlParams_t controlParams;
    initControlParameter(controlParams);
    status = LVM_SetControlParameters(mInstance, &controlParams);
    GOTO_IF_LVM_ERROR(status, deinit, "LVM_SetControlParametersFailed");

    /* Set the headroom parameters */
    LVM_HeadroomParams_t headroomParams;
    initHeadroomParameter(headroomParams);
    status = LVM_SetHeadroomParams(mInstance, &headroomParams);
    GOTO_IF_LVM_ERROR(status, deinit, "LVM_SetHeadroomParamsFailed");

    return RetCode::SUCCESS;

deinit:
    deInit();
    return RetCode::ERROR_EFFECT_LIB_ERROR;
}

void BundleContext::deInit() {
    std::lock_guard lg(mMutex);
    if (mInstance) {
        LVM_DelInstanceHandle(&mInstance);
        mInstance = nullptr;
    }
}

RetCode BundleContext::enable() {
    if (mEnabled) return RetCode::ERROR_ILLEGAL_PARAMETER;
    // Bass boost or Virtualizer can be temporarily disabled if playing over device speaker due to
    // their nature.
    bool tempDisabled = false;
    switch (mType) {
        case lvm::BundleEffectType::EQUALIZER:
            LOG(DEBUG) << __func__ << " enable bundle EQ";
            if (mSamplesToExitCountEq <= 0) mNumberEffectsEnabled++;
            mSamplesToExitCountEq = (mSamplesPerSecond * 0.1);
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::EQUALIZER));
            break;
        case lvm::BundleEffectType::BASS_BOOST:
            LOG(DEBUG) << __func__ << " enable bundle BB";
            if (mSamplesToExitCountBb <= 0) mNumberEffectsEnabled++;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::BASS_BOOST));
            mSamplesToExitCountBb = (mSamplesPerSecond * 0.1);
            tempDisabled = mBassTempDisabled;
            break;
        case lvm::BundleEffectType::VIRTUALIZER:
            LOG(DEBUG) << __func__ << " enable bundle VR";
            if (mSamplesToExitCountVirt <= 0) mNumberEffectsEnabled++;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VIRTUALIZER));
            mSamplesToExitCountVirt = (mSamplesPerSecond * 0.1);
            tempDisabled = mVirtualizerTempDisabled;
            break;
        case lvm::BundleEffectType::VOLUME:
            LOG(DEBUG) << __func__ << " enable bundle VOL";
            if ((mEffectInDrain & (1 << int(lvm::BundleEffectType::VOLUME))) == 0)
                mNumberEffectsEnabled++;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VOLUME));
            break;
    }
    mEnabled = true;
    return (tempDisabled ? RetCode::SUCCESS : enableOperatingMode());
}

RetCode BundleContext::enableOperatingMode() {
    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "failGetControlParams");
        switch (mType) {
            case lvm::BundleEffectType::EQUALIZER:
                LOG(DEBUG) << __func__ << " enable bundle EQ";
                params.EQNB_OperatingMode = LVM_EQNB_ON;
                break;
            case lvm::BundleEffectType::BASS_BOOST:
                LOG(DEBUG) << __func__ << " enable bundle BB";
                params.BE_OperatingMode = LVM_BE_ON;
                break;
            case lvm::BundleEffectType::VIRTUALIZER:
                LOG(DEBUG) << __func__ << " enable bundle VR";
                params.VirtualizerOperatingMode = LVM_MODE_ON;
                break;
            case lvm::BundleEffectType::VOLUME:
                LOG(DEBUG) << __func__ << " enable bundle VOL";
                break;
        }
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "failSetControlParams");
    }
    return limitLevel();
}

RetCode BundleContext::disable() {
    if (!mEnabled) return RetCode::ERROR_ILLEGAL_PARAMETER;
    switch (mType) {
        case lvm::BundleEffectType::EQUALIZER:
            LOG(DEBUG) << __func__ << " disable bundle EQ";
            mEffectInDrain |= 1 << int(lvm::BundleEffectType::EQUALIZER);
            break;
        case lvm::BundleEffectType::BASS_BOOST:
            LOG(DEBUG) << __func__ << " disable bundle BB";
            mEffectInDrain |= 1 << int(lvm::BundleEffectType::BASS_BOOST);
            break;
        case lvm::BundleEffectType::VIRTUALIZER:
            LOG(DEBUG) << __func__ << " disable bundle VR";
            mEffectInDrain |= 1 << int(lvm::BundleEffectType::VIRTUALIZER);
            break;
        case lvm::BundleEffectType::VOLUME:
            LOG(DEBUG) << __func__ << " disable bundle VOL";
            mEffectInDrain |= 1 << int(lvm::BundleEffectType::VOLUME);
            break;
    }
    mEnabled = false;
    return disableOperatingMode();
}

RetCode BundleContext::disableOperatingMode() {
    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "failGetControlParams");
        switch (mType) {
            case lvm::BundleEffectType::EQUALIZER:
                LOG(DEBUG) << __func__ << " disable bundle EQ";
                params.EQNB_OperatingMode = LVM_EQNB_OFF;
                break;
            case lvm::BundleEffectType::BASS_BOOST:
                LOG(DEBUG) << __func__ << " disable bundle BB";
                params.BE_OperatingMode = LVM_BE_OFF;
                break;
            case lvm::BundleEffectType::VIRTUALIZER:
                LOG(DEBUG) << __func__ << " disable bundle VR";
                params.VirtualizerOperatingMode = LVM_MODE_OFF;
                break;
            case lvm::BundleEffectType::VOLUME:
                LOG(DEBUG) << __func__ << " disable bundle VOL";
                break;
        }
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "failSetControlParams");
    }
    mEnabled = false;
    return limitLevel();
}

RetCode BundleContext::limitLevel() {
    int gainCorrection = 0;
    // Count the energy contribution per band for EQ and BassBoost only if they are active.
    float energyContribution = 0;
    float energyCross = 0;
    float energyBassBoost = 0;
    float crossCorrection = 0;
    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " getControlParamFailed");

        bool eqEnabled = params.EQNB_OperatingMode == LVM_EQNB_ON;
        bool bbEnabled = params.BE_OperatingMode == LVM_BE_ON;
        bool viEnabled = params.VirtualizerOperatingMode == LVM_MODE_ON;

        if (eqEnabled) {
            for (int i = 0; i < lvm::MAX_NUM_BANDS; i++) {
                float bandFactor = mBandGainMdB[i] / 1500.0;
                float bandCoefficient = lvm::kBandEnergyCoefficient[i];
                float bandEnergy = bandFactor * bandCoefficient * bandCoefficient;
                if (bandEnergy > 0) energyContribution += bandEnergy;
            }

            // cross EQ coefficients
            float bandFactorSum = 0;
            for (int i = 0; i < lvm::MAX_NUM_BANDS - 1; i++) {
                float bandFactor1 = mBandGainMdB[i] / 1500.0;
                float bandFactor2 = mBandGainMdB[i + 1] / 1500.0;

                if (bandFactor1 > 0 && bandFactor2 > 0) {
                    float crossEnergy =
                            bandFactor1 * bandFactor2 * lvm::kBandEnergyCrossCoefficient[i];
                    bandFactorSum += bandFactor1 * bandFactor2;

                    if (crossEnergy > 0) energyCross += crossEnergy;
                }
            }
            bandFactorSum -= 1.0;
            if (bandFactorSum > 0) crossCorrection = bandFactorSum * 0.7;
        }
        // BassBoost contribution
        if (bbEnabled) {
            float boostFactor = mBassStrengthSaved / 1000.0;
            float boostCoefficient = lvm::kBassBoostEnergyCoefficient;

            energyContribution += boostFactor * boostCoefficient * boostCoefficient;

            if (eqEnabled) {
                for (int i = 0; i < lvm::MAX_NUM_BANDS; i++) {
                    float bandFactor = mBandGainMdB[i] / 1500.0;
                    float bandCrossCoefficient = lvm::kBassBoostEnergyCrossCoefficient[i];
                    float bandEnergy = boostFactor * bandFactor * bandCrossCoefficient;
                    if (bandEnergy > 0) energyBassBoost += bandEnergy;
                }
            }
        }
        // Virtualizer contribution
        if (viEnabled) {
            energyContribution += lvm::kVirtualizerContribution * lvm::kVirtualizerContribution;
        }

        double totalEnergyEstimation =
                sqrt(energyContribution + energyCross + energyBassBoost) - crossCorrection;
        LOG(INFO) << " TOTAL energy estimation: " << totalEnergyEstimation << " dB";

        // roundoff
        int maxLevelRound = (int)(totalEnergyEstimation + 0.99);
        if (maxLevelRound + mVolume > 0) {
            gainCorrection = maxLevelRound + mVolume;
        }

        params.VC_EffectLevel = mVolume - gainCorrection;
        if (params.VC_EffectLevel < -96) {
            params.VC_EffectLevel = -96;
        }
        LOG(INFO) << "\tVol: " << mVolume << ", GainCorrection: " << gainCorrection
                  << ", Actual vol: " << params.VC_EffectLevel;

        /* Activate the initial settings */
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " setControlParamFailed");

        if (mFirstVolume) {
            RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetVolumeNoSmoothing(mInstance, &params),
                            RetCode::ERROR_EFFECT_LIB_ERROR, " setVolumeNoSmoothingFailed");
            LOG(INFO) << "\tLVM_VOLUME: Disabling Smoothing for first volume change to remove "
                         "spikes/clicks";
            mFirstVolume = false;
        }
    }

    return RetCode::SUCCESS;
}

bool BundleContext::isDeviceSupportedBassBoost(
        const std::vector<aidl::android::media::audio::common::AudioDeviceDescription>& devices) {
    for (const auto& device : devices) {
        if (device != AudioDeviceDescription{AudioDeviceType::OUT_SPEAKER, ""} &&
            device != AudioDeviceDescription{AudioDeviceType::OUT_CARKIT,
                                             AudioDeviceDescription::CONNECTION_BT_SCO} &&
            device != AudioDeviceDescription{AudioDeviceType::OUT_SPEAKER,
                                             AudioDeviceDescription::CONNECTION_BT_A2DP}) {
            return false;
        }
    }
    return true;
}

bool BundleContext::isDeviceSupportedVirtualizer(
        const std::vector<aidl::android::media::audio::common::AudioDeviceDescription>& devices) {
    for (const auto& device : devices) {
        if (device != AudioDeviceDescription{AudioDeviceType::OUT_HEADSET,
                                             AudioDeviceDescription::CONNECTION_ANALOG} &&
            device != AudioDeviceDescription{AudioDeviceType::OUT_HEADPHONE,
                                             AudioDeviceDescription::CONNECTION_ANALOG} &&
            device != AudioDeviceDescription{AudioDeviceType::OUT_HEADPHONE,
                                             AudioDeviceDescription::CONNECTION_BT_A2DP} &&
            device != AudioDeviceDescription{AudioDeviceType::OUT_HEADSET,
                                             AudioDeviceDescription::CONNECTION_USB}) {
            return false;
        }
    }
    return true;
}

bool BundleContext::isConfigSupportedVirtualizer(size_t channelCount,
                                                 const AudioDeviceDescription& device) {
    return (channelCount >= 1 && channelCount <= FCC_2) && isDeviceSupportedVirtualizer({device});
}

RetCode BundleContext::setOutputDevice(
        const std::vector<aidl::android::media::audio::common::AudioDeviceDescription>& devices) {
    mOutputDevice = devices;
    switch (mType) {
        case lvm::BundleEffectType::BASS_BOOST:
            if (!isDeviceSupportedBassBoost(devices)) {
                // If a device doesn't support bass boost, the effect must be temporarily disabled.
                // The effect must still report its original state as this can only be changed by
                // the start/stop commands.
                if (mEnabled) {
                    disableOperatingMode();
                }
                mBassTempDisabled = true;
            } else {
                // If a device supports bass boost and the effect has been temporarily disabled
                // previously then re-enable it
                if (!mEnabled) {
                    enableOperatingMode();
                }
                mBassTempDisabled = false;
            }
            break;
        case lvm::BundleEffectType::VIRTUALIZER:
            if (!isDeviceSupportedVirtualizer(devices)) {
                if (mEnabled) {
                    disableOperatingMode();
                }
                mVirtualizerTempDisabled = true;
            } else {
                if (!mEnabled) {
                    enableOperatingMode();
                }
                mVirtualizerTempDisabled = false;
            }
            break;
        default:
            break;
    }
    return RetCode::SUCCESS;
}

LVM_INT16 BundleContext::LVC_ToDB_s32Tos16(LVM_INT32 Lin_fix) const {
    LVM_INT16 db_fix;
    LVM_INT16 Shift;
    LVM_INT16 SmallRemainder;
    LVM_UINT32 Remainder = (LVM_UINT32)Lin_fix;

    /* Count leading bits, 1 cycle in assembly*/
    for (Shift = 0; Shift < 32; Shift++) {
        if ((Remainder & 0x80000000U) != 0) {
            break;
        }
        Remainder = Remainder << 1;
    }

    /*
     * Based on the approximation equation (for Q11.4 format):
     *
     * dB = -96 * Shift + 16 * (8 * Remainder - 2 * Remainder^2)
     */
    db_fix = (LVM_INT16)(-96 * Shift); /* Six dB steps in Q11.4 format*/
    SmallRemainder = (LVM_INT16)((Remainder & 0x7fffffff) >> 24);
    db_fix = (LVM_INT16)(db_fix + SmallRemainder);
    SmallRemainder = (LVM_INT16)(SmallRemainder * SmallRemainder);
    db_fix = (LVM_INT16)(db_fix - (LVM_INT16)((LVM_UINT16)SmallRemainder >> 9));

    /* Correct for small offset */
    db_fix = (LVM_INT16)(db_fix - 5);

    return db_fix;
}

/* static */
float BundleContext::VolToDb(float vol) {
    float dB = audio_utils_power_from_amplitude(vol);
    return std::max(dB, -96.f);
}

RetCode BundleContext::setVolumeStereo(const Parameter::VolumeStereo& volume) {
    LVM_ControlParams_t params;
    LVM_ReturnStatus_en status = LVM_SUCCESS;

    // Convert volume to dB
    float leftdB = VolToDb(volume.left);
    float rightdB = VolToDb(volume.right);

    float maxdB = std::max(leftdB, rightdB);
    float pandB = rightdB - leftdB;
    setVolumeLevel(maxdB);
    LOG(DEBUG) << __func__ << " pandB: " << pandB << " maxdB " << maxdB;

    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "");
        params.VC_Balance = pandB;

        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, "");
    }
    mVolumeStereo = volume;
    return RetCode::SUCCESS;
}

RetCode BundleContext::setEqualizerPreset(const std::size_t presetIdx) {
    if (presetIdx < 0 || presetIdx >= lvm::MAX_NUM_PRESETS) {
        return RetCode::ERROR_ILLEGAL_PARAMETER;
    }

    std::vector<Equalizer::BandLevel> bandLevels;
    bandLevels.reserve(lvm::MAX_NUM_BANDS);
    for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
        bandLevels.emplace_back(Equalizer::BandLevel{static_cast<int32_t>(i),
                                                     lvm::kSoftPresets[presetIdx][i] * 100});
    }

    RetCode ret = updateControlParameter(bandLevels);
    if (RetCode::SUCCESS == ret) {
        mCurPresetIdx = presetIdx;
        LOG(INFO) << __func__ << " success with " << presetIdx;
    } else {
        LOG(ERROR) << __func__ << " failed to setPreset " << presetIdx;
    }
    return ret;
}

RetCode BundleContext::setEqualizerBandLevels(const std::vector<Equalizer::BandLevel>& bandLevels) {
    RETURN_VALUE_IF(bandLevels.size() > lvm::MAX_NUM_BANDS || bandLevels.empty(),
                    RetCode::ERROR_ILLEGAL_PARAMETER, "sizeExceedMax");
    RetCode ret = updateControlParameter(bandLevels);
    if (RetCode::SUCCESS == ret) {
        mCurPresetIdx = lvm::PRESET_CUSTOM;
        LOG(INFO) << __func__ << " succeed with " << ::android::internal::ToString(bandLevels);
    } else {
        LOG(ERROR) << __func__ << " failed with " << ::android::internal::ToString(bandLevels);
    }
    return ret;
}

std::vector<Equalizer::BandLevel> BundleContext::getEqualizerBandLevels() const {
    std::vector<Equalizer::BandLevel> bandLevels;
    bandLevels.reserve(lvm::MAX_NUM_BANDS);
    for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
        bandLevels.emplace_back(
                Equalizer::BandLevel{static_cast<int32_t>(i), mBandGainMdB[i]});
    }
    return bandLevels;
}

std::vector<int32_t> BundleContext::getEqualizerCenterFreqs() {
    std::vector<int32_t> freqs;

    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        /* Get the current settings */
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params), freqs,
                        " getControlParamFailed");
        for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
            freqs.push_back((int32_t)params.pEQNB_BandDefinition[i].Frequency * 1000);
        }
    }

    return freqs;
}

bool BundleContext::isBandLevelIndexInRange(
        const std::vector<Equalizer::BandLevel>& bandLevels) const {
    const auto [min, max] =
            std::minmax_element(bandLevels.begin(), bandLevels.end(),
                                [](const auto& a, const auto& b) { return a.index < b.index; });
    return min->index >= 0 && max->index < lvm::MAX_NUM_BANDS;
}

RetCode BundleContext::updateControlParameter(const std::vector<Equalizer::BandLevel>& bandLevels) {
    RETURN_VALUE_IF(!isBandLevelIndexInRange(bandLevels), RetCode::ERROR_ILLEGAL_PARAMETER,
                    "indexOutOfRange");

    std::array<int, lvm::MAX_NUM_BANDS> tempLevel(mBandGainMdB);
    for (const auto& it : bandLevels) {
        tempLevel[it.index] = it.levelMb;
    }

    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " getControlParamFailed");

        for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
            params.pEQNB_BandDefinition[i].Frequency = lvm::kPresetsFrequencies[i];
            params.pEQNB_BandDefinition[i].QFactor = lvm::kPresetsQFactors[i];
            params.pEQNB_BandDefinition[i].Gain =
                    tempLevel[i] > 0 ? (tempLevel[i] + 50) / 100 : (tempLevel[i] - 50) / 100;
        }

        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " setControlParamFailed");
    }
    mBandGainMdB = tempLevel;
    LOG(DEBUG) << __func__ << " update bandGain to " << ::android::internal::ToString(mBandGainMdB)
               << "mdB";

    return RetCode::SUCCESS;
}

RetCode BundleContext::setBassBoostStrength(int strength) {
    // Update Control Parameter
    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " getControlParamFailed");

        params.BE_EffectLevel = (LVM_INT16)((15 * strength) / 1000);
        params.BE_CentreFreq = LVM_BE_CENTRE_90Hz;

        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " setControlParamFailed");
    }
    mBassStrengthSaved = strength;
    LOG(INFO) << __func__ << " success with strength " << strength;
    return limitLevel();
}

RetCode BundleContext::setVolumeLevel(float level) {
    if (mMuteEnabled) {
        mLevelSaved = level;
    } else {
        mVolume = level;
    }
    LOG(INFO) << __func__ << " success with level " << level;
    return limitLevel();
}

float BundleContext::getVolumeLevel() const {
    return (mMuteEnabled ? mLevelSaved : mVolume);
}

RetCode BundleContext::setVolumeMute(bool mute) {
    mMuteEnabled = mute;
    if (mMuteEnabled) {
        mLevelSaved = mVolume;
        mVolume = -96;
    } else {
        mVolume = mLevelSaved;
    }
    return limitLevel();
}

RetCode BundleContext::setVirtualizerStrength(int strength) {
    // Update Control Parameter
    LVM_ControlParams_t params;
    {
        std::lock_guard lg(mMutex);
        RETURN_VALUE_IF(LVM_SUCCESS != LVM_GetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " getControlParamFailed");

        params.CS_EffectLevel = ((strength * 32767) / 1000);

        RETURN_VALUE_IF(LVM_SUCCESS != LVM_SetControlParameters(mInstance, &params),
                        RetCode::ERROR_EFFECT_LIB_ERROR, " setControlParamFailed");
    }

    mVirtStrengthSaved = strength;
    LOG(INFO) << __func__ << " success with strength " << strength;
    return limitLevel();
}


RetCode BundleContext::setForcedDevice(
        const ::aidl::android::media::audio::common::AudioDeviceDescription& device) {
    RETURN_VALUE_IF(true != isDeviceSupportedVirtualizer({device}), RetCode::ERROR_EFFECT_LIB_ERROR,
                    " deviceNotSupportVirtualizer");
    mForceDevice = device;
    return RetCode::SUCCESS;
}

void BundleContext::initControlParameter(LVM_ControlParams_t& params) const {
    /* General parameters */
    params.OperatingMode = LVM_MODE_ON;
    params.SampleRate = LVM_FS_44100;
    params.SourceFormat = LVM_STEREO;
    params.SpeakerType = LVM_HEADPHONES;

    /* Concert Sound parameters */
    params.VirtualizerOperatingMode = LVM_MODE_OFF;
    params.VirtualizerType = LVM_CONCERTSOUND;
    params.VirtualizerReverbLevel = 100;
    params.CS_EffectLevel = LVM_CS_EFFECT_NONE;

    params.EQNB_OperatingMode = LVM_EQNB_OFF;
    params.EQNB_NBands = lvm::MAX_NUM_BANDS;
    params.pEQNB_BandDefinition = getDefaultEqualizerBandDefs();

    /* Volume Control parameters */
    params.VC_EffectLevel = 0;
    params.VC_Balance = 0;

    /* Treble Enhancement parameters */
    params.TE_OperatingMode = LVM_TE_OFF;
    params.TE_EffectLevel = 0;

    /* PSA Control parameters */
    params.PSA_Enable = LVM_PSA_OFF;
    params.PSA_PeakDecayRate = (LVM_PSA_DecaySpeed_en)0;

    /* Bass Enhancement parameters */
    params.BE_OperatingMode = LVM_BE_OFF;
    params.BE_EffectLevel = 0;
    params.BE_CentreFreq = LVM_BE_CENTRE_90Hz;
    params.BE_HPF = LVM_BE_HPF_ON;

    /* PSA Control parameters */
    params.PSA_Enable = LVM_PSA_OFF;
    params.PSA_PeakDecayRate = LVM_PSA_SPEED_MEDIUM;

    /* TE Control parameters */
    params.TE_OperatingMode = LVM_TE_OFF;
    params.TE_EffectLevel = 0;

    params.NrChannels = audio_channel_count_from_out_mask(AUDIO_CHANNEL_OUT_STEREO);
    params.ChMask = AUDIO_CHANNEL_OUT_STEREO;
    params.SourceFormat = LVM_STEREO;
}

void BundleContext::initHeadroomParameter(LVM_HeadroomParams_t& params) const {
    params.pHeadroomDefinition = getDefaultEqualizerHeadroomBanDefs();
    params.NHeadroomBands = 2;
    params.Headroom_OperatingMode = LVM_HEADROOM_OFF;
}

LVM_EQNB_BandDef_t *BundleContext::getDefaultEqualizerBandDefs() {
    static LVM_EQNB_BandDef_t* BandDefs = []() {
        static LVM_EQNB_BandDef_t tempDefs[lvm::MAX_NUM_BANDS];
        /* N-Band Equaliser parameters */
        for (std::size_t i = 0; i < lvm::MAX_NUM_BANDS; i++) {
            tempDefs[i].Frequency = lvm::kPresetsFrequencies[i];
            tempDefs[i].QFactor = lvm::kPresetsQFactors[i];
            tempDefs[i].Gain = lvm::kSoftPresets[0/* normal */][i];
        }
        return tempDefs;
    }();

    return BandDefs;
}

LVM_HeadroomBandDef_t *BundleContext::getDefaultEqualizerHeadroomBanDefs() {
    static LVM_HeadroomBandDef_t HeadroomBandDef[LVM_HEADROOM_MAX_NBANDS] = {
            {
                    .Limit_Low = 20,
                    .Limit_High = 4999,
                    .Headroom_Offset = 0,
            },
            {
                    .Limit_Low = 5000,
                    .Limit_High = 24000,
                    .Headroom_Offset = 0,
            },
    };
    return HeadroomBandDef;
}

std::vector<Virtualizer::ChannelAngle> BundleContext::getSpeakerAngles(
        const Virtualizer::SpeakerAnglesPayload payload) {
    std::vector<Virtualizer::ChannelAngle> angles;
    auto chCount = ::aidl::android::hardware::audio::common::getChannelCount(payload.layout);
    RETURN_VALUE_IF(!isConfigSupportedVirtualizer(chCount, payload.device), angles,
                    "payloadNotSupported");

    if (chCount == 1) {
        angles = {{.channel = (int32_t)AudioChannelLayout::CHANNEL_FRONT_LEFT,
                   .azimuthDegree = 0,
                   .elevationDegree = 0}};
    } else {
        angles = {{.channel = (int32_t)AudioChannelLayout::CHANNEL_FRONT_LEFT,
                   .azimuthDegree = -90,
                   .elevationDegree = 0},
                  {.channel = (int32_t)AudioChannelLayout::CHANNEL_FRONT_RIGHT,
                   .azimuthDegree = 90,
                   .elevationDegree = 0}};
    }
    return angles;
}

IEffect::Status BundleContext::lvmProcess(float* in, float* out, int samples) {
    IEffect::Status status = {EX_NULL_POINTER, 0, 0};
    RETURN_VALUE_IF(!in, status, "nullInput");
    RETURN_VALUE_IF(!out, status, "nullOutput");
    status = {EX_ILLEGAL_STATE, 0, 0};
    int64_t inputFrameCount = getCommon().input.frameCount;
    int64_t outputFrameCount = getCommon().output.frameCount;
    RETURN_VALUE_IF(inputFrameCount != outputFrameCount, status, "FrameCountMismatch");
    int isDataAvailable = true;

    auto frameSize = getInputFrameSize();
    RETURN_VALUE_IF(0 == frameSize, status, "zeroFrameSize");

    LOG(DEBUG) << __func__ << " start processing";
    if ((mEffectProcessCalled & 1 << int(mType)) != 0) {
        const int undrainedEffects = mEffectInDrain & ~mEffectProcessCalled;
        if ((undrainedEffects & 1 << int(lvm::BundleEffectType::EQUALIZER)) != 0) {
            LOG(DEBUG) << "Draining EQUALIZER";
            mSamplesToExitCountEq = 0;
            --mNumberEffectsEnabled;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::EQUALIZER));
        }
        if ((undrainedEffects & 1 << int(lvm::BundleEffectType::BASS_BOOST)) != 0) {
            LOG(DEBUG) << "Draining BASS_BOOST";
            mSamplesToExitCountBb = 0;
            --mNumberEffectsEnabled;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::BASS_BOOST));
        }
        if ((undrainedEffects & 1 << int(lvm::BundleEffectType::VIRTUALIZER)) != 0) {
            LOG(DEBUG) << "Draining VIRTUALIZER";
            mSamplesToExitCountVirt = 0;
            --mNumberEffectsEnabled;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VIRTUALIZER));
        }
        if ((undrainedEffects & 1 << int(lvm::BundleEffectType::VOLUME)) != 0) {
            LOG(DEBUG) << "Draining VOLUME";
            --mNumberEffectsEnabled;
            mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VOLUME));
        }
    }
    mEffectProcessCalled |= 1 << int(mType);
    if (!mEnabled) {
        switch (mType) {
            case lvm::BundleEffectType::EQUALIZER:
                if (mSamplesToExitCountEq > 0) {
                    mSamplesToExitCountEq -= samples;
                }
                if (mSamplesToExitCountEq <= 0) {
                    isDataAvailable = false;
                    if ((mEffectInDrain & 1 << int(lvm::BundleEffectType::EQUALIZER)) != 0) {
                        mNumberEffectsEnabled--;
                        mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::EQUALIZER));
                    }
                    LOG(DEBUG) << "Effect_process() this is the last frame for EQUALIZER";
                }
                break;
            case lvm::BundleEffectType::BASS_BOOST:
                if (mSamplesToExitCountBb > 0) {
                    mSamplesToExitCountBb -= samples;
                }
                if (mSamplesToExitCountBb <= 0) {
                    isDataAvailable = false;
                    if ((mEffectInDrain & 1 << int(lvm::BundleEffectType::BASS_BOOST)) != 0) {
                        mNumberEffectsEnabled--;
                        mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::BASS_BOOST));
                    }
                    LOG(DEBUG) << "Effect_process() this is the last frame for BASS_BOOST";
                }
                break;
            case lvm::BundleEffectType::VIRTUALIZER:
                if (mSamplesToExitCountVirt > 0) {
                    mSamplesToExitCountVirt -= samples;
                }
                if (mSamplesToExitCountVirt <= 0) {
                    isDataAvailable = false;
                    if ((mEffectInDrain & 1 << int(lvm::BundleEffectType::VIRTUALIZER)) != 0) {
                        mNumberEffectsEnabled--;
                        mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VIRTUALIZER));
                    }
                    LOG(DEBUG) << "Effect_process() this is the last frame for VIRTUALIZER";
                }
                break;
            case lvm::BundleEffectType::VOLUME:
                isDataAvailable = false;
                if ((mEffectInDrain & 1 << int(lvm::BundleEffectType::VOLUME)) != 0) {
                    mNumberEffectsEnabled--;
                    mEffectInDrain &= ~(1 << int(lvm::BundleEffectType::VOLUME));
                }
                LOG(DEBUG) << "Effect_process() LVM_VOLUME Effect is not enabled";
                break;
        }
    }
    if (isDataAvailable) {
        mNumberEffectsCalled++;
    }
    bool accumulate = false;
    if (mNumberEffectsCalled >= mNumberEffectsEnabled) {
        // We expect the # effects called to be equal to # effects enabled in sequence (including
        // draining effects).  Warn if this is not the case due to inconsistent calls.
        ALOGW_IF(mNumberEffectsCalled > mNumberEffectsEnabled,
                 "%s Number of effects called %d is greater than number of effects enabled %d",
                 __func__, mNumberEffectsCalled, mNumberEffectsEnabled);
        mEffectProcessCalled = 0;  // reset our consistency check.
        if (!isDataAvailable) {
            LOG(DEBUG) << "Effect_process() processing last frame";
        }
        mNumberEffectsCalled = 0;
        LVM_UINT16 frames = samples * sizeof(float) / frameSize;
        float* outTmp = (accumulate ? getWorkBuffer() : out);
        /* Process the samples */
        LVM_ReturnStatus_en lvmStatus;
        {
            std::lock_guard lg(mMutex);
            lvmStatus = LVM_Process(mInstance, in, outTmp, frames, 0);
            if (lvmStatus != LVM_SUCCESS) {
                LOG(ERROR) << __func__ << lvmStatus;
                return {EX_UNSUPPORTED_OPERATION, 0, 0};
            }
            if (accumulate) {
                for (int i = 0; i < samples; i++) {
                    out[i] += outTmp[i];
                }
            }
        }
    } else {
        for (int i = 0; i < samples; i++) {
            if (accumulate) {
                out[i] += in[i];
            } else {
                out[i] = in[i];
            }
        }
    }
    LOG(DEBUG) << __func__ << " done processing";
    return {STATUS_OK, samples, samples};
}

}  // namespace aidl::android::hardware::audio::effect
