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
#include <cstring>
#define LOG_TAG "EffectHalAidl"
//#define LOG_NDEBUG 0

#include <algorithm>
#include <memory>

#include <audio_utils/primitives.h>
#include <error/expected_utils.h>
#include <media/AidlConversionCppNdk.h>
#include <media/AidlConversionEffect.h>
#include <media/AidlConversionUtil.h>
#include <media/EffectsFactoryApi.h>
#include <mediautils/TimeCheck.h>
#include <system/audio.h>
#include <system/audio_effects/effect_uuid.h>
#include <utils/Log.h>

#include "EffectHalAidl.h"
#include "EffectProxy.h"

#include <aidl/android/hardware/audio/effect/IEffect.h>

#include "effectsAidlConversion/AidlConversionAec.h"
#include "effectsAidlConversion/AidlConversionAgc1.h"
#include "effectsAidlConversion/AidlConversionAgc2.h"
#include "effectsAidlConversion/AidlConversionBassBoost.h"
#include "effectsAidlConversion/AidlConversionDownmix.h"
#include "effectsAidlConversion/AidlConversionDynamicsProcessing.h"
#include "effectsAidlConversion/AidlConversionEnvReverb.h"
#include "effectsAidlConversion/AidlConversionEq.h"
#include "effectsAidlConversion/AidlConversionHapticGenerator.h"
#include "effectsAidlConversion/AidlConversionLoudnessEnhancer.h"
#include "effectsAidlConversion/AidlConversionNoiseSuppression.h"
#include "effectsAidlConversion/AidlConversionPresetReverb.h"
#include "effectsAidlConversion/AidlConversionSpatializer.h"
#include "effectsAidlConversion/AidlConversionVendorExtension.h"
#include "effectsAidlConversion/AidlConversionVirtualizer.h"
#include "effectsAidlConversion/AidlConversionVisualizer.h"

using ::aidl::android::aidl_utils::statusTFromBinderStatus;
using ::aidl::android::hardware::audio::effect::CommandId;
using ::aidl::android::hardware::audio::effect::Descriptor;
using ::aidl::android::hardware::audio::effect::IEffect;
using ::aidl::android::hardware::audio::effect::IFactory;
using ::aidl::android::hardware::audio::effect::kEventFlagDataMqNotEmpty;
using ::aidl::android::hardware::audio::effect::kEventFlagDataMqUpdate;
using ::aidl::android::hardware::audio::effect::kEventFlagNotEmpty;
using ::aidl::android::hardware::audio::effect::kReopenSupportedVersion;
using ::aidl::android::hardware::audio::effect::Parameter;
using ::aidl::android::hardware::audio::effect::State;
using ::aidl::android::media::audio::common::AudioDeviceDescription;

namespace android {
namespace effect {

EffectHalAidl::EffectHalAidl(const std::shared_ptr<IFactory>& factory,
                             const std::shared_ptr<IEffect>& effect, int32_t sessionId,
                             int32_t ioId, const Descriptor& desc, bool isProxyEffect)
    : mFactory(factory),
      mEffect(effect),
      mSessionId(sessionId),
      mIoId(ioId),
      mIsProxyEffect(isProxyEffect),
      mHalVersion([factory]() {
          int version = 0;
          // use factory HAL version because effect can be an EffectProxy instance
          return factory->getInterfaceVersion(&version).isOk() ? version : 0;
      }()),
      mEventFlagDataMqNotEmpty(mHalVersion >= kReopenSupportedVersion ? kEventFlagDataMqNotEmpty
                                                                      : kEventFlagNotEmpty) {
    assert(mFactory != nullptr);
    assert(mEffect != nullptr);
    createAidlConversion(effect, sessionId, ioId, desc);
}

EffectHalAidl::~EffectHalAidl() {
    if (mEffect) {
        if (mIsProxyEffect) {
            std::static_pointer_cast<EffectProxy>(mEffect)->destroy();
        } else if (mFactory) {
            mFactory->destroyEffect(mEffect);
        }
    }
}

status_t EffectHalAidl::createAidlConversion(
        std::shared_ptr<IEffect> effect,
        int32_t sessionId, int32_t ioId,
        const Descriptor& desc) {
    const auto& typeUuid = desc.common.id.type;
    ALOGI("%s create UUID %s", __func__, typeUuid.toString().c_str());
    if (typeUuid ==
        ::aidl::android::hardware::audio::effect::getEffectTypeUuidAcousticEchoCanceler()) {
        mConversion = std::make_unique<android::effect::AidlConversionAec>(effect, sessionId, ioId,
                                                                           desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::
                                   getEffectTypeUuidAutomaticGainControlV1()) {
        mConversion = std::make_unique<android::effect::AidlConversionAgc1>(effect, sessionId, ioId,
                                                                            desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::
                                   getEffectTypeUuidAutomaticGainControlV2()) {
        mConversion = std::make_unique<android::effect::AidlConversionAgc2>(effect, sessionId, ioId,
                                                                            desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::getEffectTypeUuidBassBoost()) {
        mConversion = std::make_unique<android::effect::AidlConversionBassBoost>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::getEffectTypeUuidDownmix()) {
        mConversion = std::make_unique<android::effect::AidlConversionDownmix>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidDynamicsProcessing()) {
        mConversion = std::make_unique<android::effect::AidlConversionDp>(effect, sessionId, ioId,
                                                                          desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::getEffectTypeUuidEnvReverb()) {
        mConversion = std::make_unique<android::effect::AidlConversionEnvReverb>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid == ::aidl::android::hardware::audio::effect::getEffectTypeUuidEqualizer()) {
        mConversion = std::make_unique<android::effect::AidlConversionEq>(effect, sessionId, ioId,
                                                                          desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidHapticGenerator()) {
        mConversion = std::make_unique<android::effect::AidlConversionHapticGenerator>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
        mIsHapticGenerator = true;
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidLoudnessEnhancer()) {
        mConversion = std::make_unique<android::effect::AidlConversionLoudnessEnhancer>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidNoiseSuppression()) {
        mConversion = std::make_unique<android::effect::AidlConversionNoiseSuppression>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidPresetReverb()) {
        mConversion = std::make_unique<android::effect::AidlConversionPresetReverb>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidSpatializer()) {
        mConversion = std::make_unique<android::effect::AidlConversionSpatializer>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidVirtualizer()) {
        mConversion = std::make_unique<android::effect::AidlConversionVirtualizer>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else if (typeUuid ==
               ::aidl::android::hardware::audio::effect::getEffectTypeUuidVisualizer()) {
        mConversion = std::make_unique<android::effect::AidlConversionVisualizer>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    } else {
        // For unknown UUID, use vendor extension implementation
        mConversion = std::make_unique<android::effect::AidlConversionVendorExtension>(
                effect, sessionId, ioId, desc, mIsProxyEffect);
    }
    mEffectName = mConversion->getDescriptor().common.name;
    return OK;
}

status_t EffectHalAidl::setInBuffer(const sp<EffectBufferHalInterface>& buffer) {
    mInBuffer = buffer;
    return OK;
}

status_t EffectHalAidl::setOutBuffer(const sp<EffectBufferHalInterface>& buffer) {
    mOutBuffer = buffer;
    return OK;
}

// write to input FMQ here, wait for statusMQ STATUS_OK, and read from output FMQ
status_t EffectHalAidl::process() {
    State state = State::INIT;
    if (mConversion->isBypassing() || !mEffect->getState(&state).isOk() ||
        (state != State::PROCESSING && state != State::DRAINING)) {
        ALOGI("%s skipping process because it's %s", mEffectName.c_str(),
              mConversion->isBypassing()
                      ? "bypassing"
                      : aidl::android::hardware::audio::effect::toString(state).c_str());
        return -ENODATA;
    }

    const std::shared_ptr<android::hardware::EventFlag> efGroup = mConversion->getEventFlagGroup();
    if (!efGroup) {
        ALOGE("%s invalid efGroup", mEffectName.c_str());
        return INVALID_OPERATION;
    }

    // reopen if halVersion >= kReopenSupportedVersion and receive kEventFlagDataMqUpdate
    RETURN_STATUS_IF_ERROR(maybeReopen(efGroup));
    const size_t samplesWritten = writeToHalInputFmqAndSignal(efGroup);
    if (0 == samplesWritten) {
        return INVALID_OPERATION;
    }

    RETURN_STATUS_IF_ERROR(waitHalStatusFmq(samplesWritten));
    RETURN_STATUS_IF_ERROR(readFromHalOutputFmq(samplesWritten));
    return OK;
}

status_t EffectHalAidl::maybeReopen(
        const std::shared_ptr<android::hardware::EventFlag>& efGroup) const {
    if (mHalVersion < kReopenSupportedVersion) {
        return OK;
    }

    // check if the DataMq needs any update, timeout at 1ns to avoid being blocked
    if (uint32_t efState = 0; ::android::OK == efGroup->wait(kEventFlagDataMqUpdate, &efState,
                                                             1 /* ns */, true /* retry */) &&
                              efState & kEventFlagDataMqUpdate) {
        ALOGD("%s V%d receive dataMQUpdate eventFlag from HAL", mEffectName.c_str(), mHalVersion);
        return mConversion->reopen();
    }
    return OK;
}

size_t EffectHalAidl::writeToHalInputFmqAndSignal(
        const std::shared_ptr<android::hardware::EventFlag>& efGroup) const {
    const auto inputQ = mConversion->getInputMQ();
    if (!inputQ || !inputQ->isValid()) {
        ALOGE("%s invalid input FMQ", mEffectName.c_str());
        return 0;
    }

    const size_t fmqSpaceSamples = inputQ->availableToWrite();
    const size_t samplesInBuffer =
            mInBuffer->audioBuffer()->frameCount * mConversion->getInputChannelCount();
    const size_t samplesToWrite = std::min(fmqSpaceSamples, samplesInBuffer);
    if (samplesToWrite == 0) {
        ALOGE("%s not able to write, samplesInBuffer %zu, fmqSpaceSamples %zu", mEffectName.c_str(),
              samplesInBuffer, fmqSpaceSamples);
        return 0;
    }

    const float* const inputRawBuffer = static_cast<const float*>(mInBuffer->audioBuffer()->f32);
    if (!inputQ->write(inputRawBuffer, samplesToWrite)) {
        ALOGE("%s failed to write %zu samples to inputQ [avail %zu]", mEffectName.c_str(),
              samplesToWrite, inputQ->availableToWrite());
        return 0;
    }

    efGroup->wake(mEventFlagDataMqNotEmpty);
    return samplesToWrite;
}

void EffectHalAidl::writeHapticGeneratorData(size_t totalSamples, float* const outputRawBuffer,
                                             float* const fmqOutputBuffer) const {
    const auto audioChNum = mConversion->getAudioChannelCount();
    const auto audioSamples =
            totalSamples * audioChNum / (audioChNum + mConversion->getHapticChannelCount());

    static constexpr float kHalFloatSampleLimit = 2.0f;
    // for HapticGenerator, the input data buffer will be updated
    float* const inputRawBuffer = static_cast<float*>(mInBuffer->audioBuffer()->f32);
    // accumulate or copy input to output, haptic samples remains all zero
    if (mConversion->mOutputAccessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE) {
        accumulate_float(outputRawBuffer, inputRawBuffer, audioSamples);
    } else {
        memcpy_to_float_from_float_with_clamping(outputRawBuffer, inputRawBuffer, audioSamples,
                                                 kHalFloatSampleLimit);
    }
    // append the haptic sample at the end of input audio samples
    memcpy_to_float_from_float_with_clamping(inputRawBuffer + audioSamples,
                                             fmqOutputBuffer + audioSamples,
                                             totalSamples - audioSamples, kHalFloatSampleLimit);
}

status_t EffectHalAidl::waitHalStatusFmq(size_t samplesWritten) const {
    const auto statusQ = mConversion->getStatusMQ();
    if (const bool statusValid = statusQ && statusQ->isValid(); !statusValid) {
        ALOGE("%s statusFMQ %s", mEffectName.c_str(), statusValid ? "valid" : "invalid");
        return INVALID_OPERATION;
    }

    IEffect::Status retStatus{};
    if (!statusQ->readBlocking(&retStatus, 1)) {
        ALOGE("%s V%d read status from status FMQ failed", mEffectName.c_str(), mHalVersion);
        return INVALID_OPERATION;
    }
    if (retStatus.status != OK || (size_t)retStatus.fmqConsumed != samplesWritten ||
        retStatus.fmqProduced == 0) {
        ALOGE("%s read status failed: %s, FMQ consumed %d (of %zu) produced %d",
              mEffectName.c_str(), retStatus.toString().c_str(), retStatus.fmqConsumed,
              samplesWritten, retStatus.fmqProduced);
        return INVALID_OPERATION;
    }

    return OK;
}

status_t EffectHalAidl::readFromHalOutputFmq(size_t samplesWritten) const {
    const auto outputQ = mConversion->getOutputMQ();
    if (const bool outputValid = outputQ && outputQ->isValid(); !outputValid) {
        ALOGE("%s outputFMQ %s", mEffectName.c_str(), outputValid ? "valid" : "invalid");
        return INVALID_OPERATION;
    }

    const size_t fmqProducedSamples = outputQ->availableToRead();
    const size_t bufferSpaceSamples =
            mOutBuffer->audioBuffer()->frameCount * mConversion->getOutputChannelCount();
    const size_t samplesToRead = std::min(fmqProducedSamples, bufferSpaceSamples);
    if (samplesToRead == 0) {
        ALOGE("%s unable to read, bufferSpace %zu, fmqProduced %zu samplesWritten %zu",
              mEffectName.c_str(), bufferSpaceSamples, fmqProducedSamples, samplesWritten);
        return INVALID_OPERATION;
    }

    float* const outputRawBuffer = static_cast<float*>(mOutBuffer->audioBuffer()->f32);
    float* fmqOutputBuffer = outputRawBuffer;
    std::vector<float> tempBuffer;
    // keep original data in the output buffer for accumulate mode or HapticGenerator effect
    if (mConversion->mOutputAccessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE || mIsHapticGenerator) {
        tempBuffer.resize(samplesToRead, 0);
        fmqOutputBuffer = tempBuffer.data();
    }
    // always read floating point data for AIDL
    if (!outputQ->read(fmqOutputBuffer, samplesToRead)) {
        ALOGE("%s failed to read %zu from outputQ to audioBuffer %p", mEffectName.c_str(),
              samplesToRead, fmqOutputBuffer);
        return INVALID_OPERATION;
    }

    // HapticGenerator needs special handling because the generated haptic samples should append to
    // the end of audio samples, the generated haptic data pass back from HAL in output FMQ at same
    // offset as input buffer, here we skip the audio samples in output FMQ and append haptic
    // samples to the end of input buffer
    if (mIsHapticGenerator) {
        assert(samplesRead == samplesWritten);
        writeHapticGeneratorData(samplesToRead, outputRawBuffer, fmqOutputBuffer);
    } else if (mConversion->mOutputAccessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE) {
        accumulate_float(outputRawBuffer, fmqOutputBuffer, samplesToRead);
    }

    return OK;
}

// TODO: no one using, maybe deprecate this interface
status_t EffectHalAidl::processReverse() {
    ALOGW("%s not implemented yet", __func__);
    return OK;
}

status_t EffectHalAidl::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData,
                                uint32_t* replySize, void* pReplyData) {
    TIME_CHECK();
    if (!mConversion) {
        ALOGE("%s can not handle command %d when conversion not exist", __func__, cmdCode);
        return INVALID_OPERATION;
    }

    return mConversion->handleCommand(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

status_t EffectHalAidl::getDescriptor(effect_descriptor_t* pDescriptor) {
    TIME_CHECK();
    if (pDescriptor == nullptr) {
        ALOGE("%s null descriptor pointer", __func__);
        return BAD_VALUE;
    }
    Descriptor aidlDesc;
    RETURN_STATUS_IF_ERROR(statusTFromBinderStatus(mEffect->getDescriptor(&aidlDesc)));

    *pDescriptor = VALUE_OR_RETURN_STATUS(
            ::aidl::android::aidl2legacy_Descriptor_effect_descriptor(aidlDesc));
    return OK;
}

status_t EffectHalAidl::close() {
    TIME_CHECK();
    mEffect->command(CommandId::STOP);
    return statusTFromBinderStatus(mEffect->close());
}

status_t EffectHalAidl::dump(int fd) {
    TIME_CHECK();
    return mEffect->dump(fd, nullptr, 0);
}

status_t EffectHalAidl::setDevices(const AudioDeviceTypeAddrVector& deviceTypes) {
    TIME_CHECK();

    // TODO: b/397236443 - add this as part of effect dumpsys
    ALOGD("%s %s", __func__,
          dumpAudioDeviceTypeAddrVector(deviceTypes, false /*includeSensitiveInfo*/).c_str());

    std::vector<AudioDeviceDescription> deviceDescs;
    for (const AudioDeviceTypeAddr& deviceType : deviceTypes) {
        AudioDeviceDescription deviceDesc = VALUE_OR_RETURN_STATUS(
                ::aidl::android::legacy2aidl_audio_devices_t_AudioDeviceDescription(
                        deviceType.mType));
        deviceDescs.emplace_back(std::move(deviceDesc));
    }

    return statusTFromBinderStatus(
            mEffect->setParameter(Parameter::make<Parameter::deviceDescription>(deviceDescs)));
}

} // namespace effect
} // namespace android
