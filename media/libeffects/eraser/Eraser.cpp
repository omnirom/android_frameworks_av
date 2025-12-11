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

#define LOG_TAG "AHAL_EraserEffect"

#include <android-base/logging.h>
#include <aidl/android/hardware/audio/effect/Eraser.h>
#include <system/audio_effects/effect_uuid.h>

#include "Eraser.h"
#include "EraserContext.h"

#include <cstddef>
#include <optional>

using aidl::android::hardware::audio::common::getChannelCount;
using aidl::android::hardware::audio::effect::Descriptor;
using aidl::android::hardware::audio::effect::Eraser;
using aidl::android::hardware::audio::effect::getEffectImplUuidEraser;
using aidl::android::hardware::audio::effect::getEffectTypeUuidEraser;
using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::State;
using aidl::android::media::audio::common::AudioUuid;

extern "C" binder_exception_t createEffect(const AudioUuid* in_impl_uuid,
                                           std::shared_ptr<IEffect>* instanceSpp) {
    android::base::InitLogging(nullptr);
    if (!in_impl_uuid || *in_impl_uuid != getEffectImplUuidEraser()) {
        LOG(ERROR) << __func__ << "uuid not supported";
        return EX_ILLEGAL_ARGUMENT;
    }

    if (!instanceSpp) {
        LOG(ERROR) << __func__ << " invalid input parameter!";
        return EX_ILLEGAL_ARGUMENT;
    }

    *instanceSpp = ndk::SharedRefBase::make<aidl::android::hardware::audio::effect::EraserImpl>();
    LOG(DEBUG) << __func__ << " instance " << instanceSpp->get() << " created";
    return EX_NONE;
}

extern "C" binder_exception_t queryEffect(const AudioUuid* in_impl_uuid, Descriptor* _aidl_return) {
    if (!in_impl_uuid || *in_impl_uuid != getEffectImplUuidEraser()) {
        LOG(ERROR) << __func__ << "uuid not supported";
        return EX_ILLEGAL_ARGUMENT;
    }
    *_aidl_return = aidl::android::hardware::audio::effect::EraserImpl::kDescriptor;
    return EX_NONE;
}

namespace aidl::android::hardware::audio::effect {

const std::string EraserImpl::kEffectName = "AOSP Audio Eraser";
const EraserImpl::EraserCapability EraserImpl::kCapability = EraserContext::getCapability();

const Descriptor EraserImpl::kDescriptor = {
        .common = {.id = {.type = getEffectTypeUuidEraser(), .uuid = getEffectImplUuidEraser()},
                   .flags = {.hwAcceleratorMode = Flags::HardwareAccelerator::NONE},
                   .name = EraserImpl::kEffectName,
                   .implementor = "The Android Open Source Project"}};

ndk::ScopedAStatus EraserImpl::getDescriptor(Descriptor* _aidl_return) {
    LOG(DEBUG) << __func__ << kDescriptor.toString();
    *_aidl_return = kDescriptor;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EraserImpl::setParameterSpecific(const Parameter::Specific& specific) {
    RETURN_IF(Parameter::Specific::eraser != specific.getTag(), EX_ILLEGAL_ARGUMENT,
              "EffectNotSupported");
    RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

    auto param = specific.get<Parameter::Specific::eraser>();
    return mContext->setParam(param);
}

ndk::ScopedAStatus EraserImpl::getParameterSpecific(const Parameter::Id& id,
                                                    Parameter::Specific* specific) {
    RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

    auto tag = id.getTag();
    RETURN_IF(Parameter::Id::eraserTag != tag, EX_ILLEGAL_ARGUMENT, "wrongIdTag");
    auto eraserId = id.get<Parameter::Id::eraserTag>();
    auto eraserTag = eraserId.getTag();
    switch (eraserTag) {
        case Eraser::Id::commonTag: {
            auto specificTag = eraserId.get<Eraser::Id::commonTag>();
            std::optional<Eraser> param = mContext->getParam(specificTag);
            if (!param.has_value()) {
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                        "EraserTagNotSupported");
            }
            specific->set<Parameter::Specific::eraser>(param.value());
            break;
        }
        default: {
            LOG(ERROR) << __func__ << " unsupported tag: " << toString(tag);
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "EraserTagNotSupported");
        }
    }
    return ndk::ScopedAStatus::ok();
}

std::shared_ptr<EffectContext> EraserImpl::createContext(const Parameter::Common& common) {
    if (mContext) {
        LOG(DEBUG) << __func__ << " context already exist";
    } else {
        const auto& supportedLayouts = kCapability.channelLayouts;
        const auto& supportedSampleRates = kCapability.sampleRates;
        if (common.input.base != common.output.base) {
            LOG(ERROR) << __func__
                       << " input/output not supported, input: " << common.input.base.toString()
                       << " output: " << common.output.base.toString();
            return nullptr;
        }
        if (supportedLayouts.end() == std::find(supportedLayouts.begin(), supportedLayouts.end(),
                                                common.input.base.channelMask)) {
            LOG(ERROR) << __func__ << " channelMask not supported: "
                       << common.input.base.channelMask.toString();
            return nullptr;
        }
        if (supportedSampleRates.end() == std::find(supportedSampleRates.begin(),
                                                    supportedSampleRates.end(),
                                                    common.input.base.sampleRate)) {
            LOG(ERROR) << __func__
                       << " sampleRate not supported: " << common.input.base.sampleRate;
        }
        mContext = std::make_shared<EraserContext>(1 /* statusFmqDepth */, common);
    }
    return mContext;
}

RetCode EraserImpl::releaseContext() {
    if (mContext) {
        mContext.reset();
    }
    return RetCode::SUCCESS;
}

EraserImpl::~EraserImpl() {
    cleanUp();
    LOG(DEBUG) << __func__;
}

ndk::ScopedAStatus EraserImpl::command(CommandId command) {
    std::lock_guard lg(mImplMutex);
    RETURN_IF(mState == State::INIT, EX_ILLEGAL_STATE, "instanceNotOpen");

    switch (command) {
        case CommandId::START:
            RETURN_OK_IF(mState == State::PROCESSING);
            mState = State::PROCESSING;
            mContext->enable();
            startThread();
            RETURN_IF(notifyEventFlag(mDataMqNotEmptyEf) != RetCode::SUCCESS, EX_ILLEGAL_STATE,
                      "notifyEventFlagNotEmptyFailedOnStart");
            break;
        case CommandId::STOP:
            RETURN_OK_IF(mState == State::IDLE);
            mState = State::IDLE;
            RETURN_IF(notifyEventFlag(mDataMqNotEmptyEf) != RetCode::SUCCESS, EX_ILLEGAL_STATE,
                      "notifyEventFlagNotEmptyFailedOnStop");
            stopThread();
            mContext->disable();
            break;
        case CommandId::RESET:
            mState = State::IDLE;
            RETURN_IF(notifyEventFlag(mDataMqNotEmptyEf) != RetCode::SUCCESS, EX_ILLEGAL_STATE,
                      "notifyEventFlagNotEmptyFailedOnReset");
            stopThread();
            mImplContext->disable();
            mImplContext->reset();
            mImplContext->resetBuffer();
            break;
        default:
            LOG(ERROR) << getEffectNameWithVersion() << __func__ << " instance still processing";
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "CommandIdNotSupported");
    }
    LOG(VERBOSE) << getEffectNameWithVersion() << __func__
                 << " transfer to state: " << toString(mState);
    return ndk::ScopedAStatus::ok();
}

// Processing method running in EffectWorker thread.
IEffect::Status EraserImpl::effectProcessImpl(float* in, float* out, int samples) {
    RETURN_VALUE_IF(!mContext, (IEffect::Status{EX_NULL_POINTER, 0, 0}), "nullContext");
    return mContext->process(in, out, samples);
}

}  // namespace aidl::android::hardware::audio::effect
