/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ANDROID_AUDIOPOLICYSERVICE_H
#define ANDROID_AUDIOPOLICYSERVICE_H

#include <cutils/misc.h>
#include <cutils/config_utils.h>
#include <cutils/compiler.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/SortedVector.h>
#include <binder/BinderService.h>
#include <system/audio.h>
#include <system/audio_policy.h>
#include <hardware/audio_policy.h>
#include <hardware/power.h>
#include <media/IAudioPolicyService.h>
#include <media/ToneGenerator.h>
#include <media/AudioEffect.h>

namespace android {

// ----------------------------------------------------------------------------

class AudioPolicyService :
    public BinderService<AudioPolicyService>,
    public BnAudioPolicyService,
//    public AudioPolicyClientInterface,
    public IBinder::DeathRecipient
{
    friend class BinderService<AudioPolicyService>;

public:
    // for BinderService
    static const char *getServiceName() ANDROID_API { return "media.audio_policy"; }

    virtual status_t    dump(int fd, const Vector<String16>& args);

    //
    // BnAudioPolicyService (see AudioPolicyInterface for method descriptions)
    //

    virtual status_t setDeviceConnectionState(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address);
    virtual audio_policy_dev_state_t getDeviceConnectionState(
                                                                audio_devices_t device,
                                                                const char *device_address);
    virtual status_t setPhoneState(audio_mode_t state);
    virtual status_t setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config);
    virtual audio_policy_forced_cfg_t getForceUse(audio_policy_force_use_t usage);
    virtual audio_io_handle_t getOutput(audio_stream_type_t stream,
                                        uint32_t samplingRate = 0,
                                        audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                        audio_channel_mask_t channelMask = 0,
                                        audio_output_flags_t flags =
                                                AUDIO_OUTPUT_FLAG_NONE,
                                        const audio_offload_info_t *offloadInfo = NULL);
    virtual status_t startOutput(audio_io_handle_t output,
                                 audio_stream_type_t stream,
                                 int session = 0);
    virtual status_t stopOutput(audio_io_handle_t output,
                                audio_stream_type_t stream,
                                int session = 0);
    virtual void releaseOutput(audio_io_handle_t output);
    virtual audio_io_handle_t getInput(audio_source_t inputSource,
                                    uint32_t samplingRate = 0,
                                    audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                    audio_channel_mask_t channelMask = 0,
                                    int audioSession = 0);
    virtual status_t startInput(audio_io_handle_t input);
    virtual status_t stopInput(audio_io_handle_t input);
    virtual void releaseInput(audio_io_handle_t input);
    virtual status_t initStreamVolume(audio_stream_type_t stream,
                                      int indexMin,
                                      int indexMax);
    virtual status_t setStreamVolumeIndex(audio_stream_type_t stream,
                                          int index,
                                          audio_devices_t device);
    virtual status_t getStreamVolumeIndex(audio_stream_type_t stream,
                                          int *index,
                                          audio_devices_t device);

    virtual uint32_t getStrategyForStream(audio_stream_type_t stream);
    virtual audio_devices_t getDevicesForStream(audio_stream_type_t stream);

    virtual audio_io_handle_t getOutputForEffect(const effect_descriptor_t *desc);
    virtual status_t registerEffect(const effect_descriptor_t *desc,
                                    audio_io_handle_t io,
                                    uint32_t strategy,
                                    int session,
                                    int id);
    virtual status_t unregisterEffect(int id);
    virtual status_t setEffectEnabled(int id, bool enabled);
    virtual bool isStreamActive(audio_stream_type_t stream, uint32_t inPastMs = 0) const;
    virtual bool isStreamActiveRemotely(audio_stream_type_t stream, uint32_t inPastMs = 0) const;
    virtual bool isSourceActive(audio_source_t source) const;

    virtual status_t queryDefaultPreProcessing(int audioSession,
                                              effect_descriptor_t *descriptors,
                                              uint32_t *count);
    virtual     status_t    onTransact(
                                uint32_t code,
                                const Parcel& data,
                                Parcel* reply,
                                uint32_t flags);

    // IBinder::DeathRecipient
    virtual     void        binderDied(const wp<IBinder>& who);

    //
    // Helpers for the struct audio_policy_service_ops implementation.
    // This is used by the audio policy manager for certain operations that
    // are implemented by the policy service.
    //
    virtual void setParameters(audio_io_handle_t ioHandle,
                               const char *keyValuePairs,
                               int delayMs);

    virtual status_t setStreamVolume(audio_stream_type_t stream,
                                     float volume,
                                     audio_io_handle_t output,
                                     int delayMs = 0);
    virtual status_t startTone(audio_policy_tone_t tone, audio_stream_type_t stream);
    virtual status_t stopTone();
    virtual status_t setVoiceVolume(float volume, int delayMs = 0);
    virtual bool isOffloadSupported(const audio_offload_info_t &config);

            status_t doStopOutput(audio_io_handle_t output,
                                  audio_stream_type_t stream,
                                  int session = 0);
            void doReleaseOutput(audio_io_handle_t output);

private:
                        AudioPolicyService() ANDROID_API;
    virtual             ~AudioPolicyService();

            status_t dumpInternals(int fd);

    // Thread used for tone playback and to send audio config commands to audio flinger
    // For tone playback, using a separate thread is necessary to avoid deadlock with mLock because
    // startTone() and stopTone() are normally called with mLock locked and requesting a tone start
    // or stop will cause calls to AudioPolicyService and an attempt to lock mLock.
    // For audio config commands, it is necessary because audio flinger requires that the calling
    // process (user) has permission to modify audio settings.
    class AudioCommandThread : public Thread {
        class AudioCommand;
    public:

        // commands for tone AudioCommand
        enum {
            START_TONE,
            STOP_TONE,
            SET_VOLUME,
            SET_PARAMETERS,
            SET_VOICE_VOLUME,
            STOP_OUTPUT,
            RELEASE_OUTPUT
        };

        AudioCommandThread (String8 name, const wp<AudioPolicyService>& service);
        virtual             ~AudioCommandThread();

                    status_t    dump(int fd);

        // Thread virtuals
        virtual     void        onFirstRef();
        virtual     bool        threadLoop();

                    void        exit();
                    void        startToneCommand(ToneGenerator::tone_type type,
                                                 audio_stream_type_t stream);
                    void        stopToneCommand();
                    status_t    volumeCommand(audio_stream_type_t stream, float volume,
                                            audio_io_handle_t output, int delayMs = 0);
                    status_t    parametersCommand(audio_io_handle_t ioHandle,
                                            const char *keyValuePairs, int delayMs = 0);
                    status_t    voiceVolumeCommand(float volume, int delayMs = 0);
                    void        stopOutputCommand(audio_io_handle_t output,
                                                  audio_stream_type_t stream,
                                                  int session);
                    void        releaseOutputCommand(audio_io_handle_t output);

                    void        insertCommand_l(AudioCommand *command, int delayMs = 0);

    private:
        class AudioCommandData;

        // descriptor for requested tone playback event
        class AudioCommand {

        public:
            AudioCommand()
            : mCommand(-1) {}

            void dump(char* buffer, size_t size);

            int mCommand;   // START_TONE, STOP_TONE ...
            nsecs_t mTime;  // time stamp
            Condition mCond; // condition for status return
            status_t mStatus; // command status
            bool mWaitStatus; // true if caller is waiting for status
            AudioCommandData *mParam;     // command specific parameter data
        };

        class AudioCommandData {
        public:
            virtual ~AudioCommandData() {}
        protected:
            AudioCommandData() {}
        };

        class ToneData : public AudioCommandData {
        public:
            ToneGenerator::tone_type mType; // tone type (START_TONE only)
            audio_stream_type_t mStream;    // stream type (START_TONE only)
        };

        class VolumeData : public AudioCommandData {
        public:
            audio_stream_type_t mStream;
            float mVolume;
            audio_io_handle_t mIO;
        };

        class ParametersData : public AudioCommandData {
        public:
            audio_io_handle_t mIO;
            String8 mKeyValuePairs;
        };

        class VoiceVolumeData : public AudioCommandData {
        public:
            float mVolume;
        };

        class StopOutputData : public AudioCommandData {
        public:
            audio_io_handle_t mIO;
            audio_stream_type_t mStream;
            int mSession;
        };

        class ReleaseOutputData : public AudioCommandData {
        public:
            audio_io_handle_t mIO;
        };

        Mutex   mLock;
        Condition mWaitWorkCV;
        Vector <AudioCommand *> mAudioCommands; // list of pending commands
        ToneGenerator *mpToneGenerator;     // the tone generator
        AudioCommand mLastCommand;          // last processed command (used by dump)
        String8 mName;                      // string used by wake lock fo delayed commands
        wp<AudioPolicyService> mService;
    };

    class EffectDesc {
    public:
        EffectDesc(const char *name, const effect_uuid_t& uuid) :
                        mName(strdup(name)),
                        mUuid(uuid) { }
        EffectDesc(const EffectDesc& orig) :
                        mName(strdup(orig.mName)),
                        mUuid(orig.mUuid) {
                            // deep copy mParams
                            for (size_t k = 0; k < orig.mParams.size(); k++) {
                                effect_param_t *origParam = orig.mParams[k];
                                // psize and vsize are rounded up to an int boundary for allocation
                                size_t origSize = sizeof(effect_param_t) +
                                                  ((origParam->psize + 3) & ~3) +
                                                  ((origParam->vsize + 3) & ~3);
                                effect_param_t *dupParam = (effect_param_t *) malloc(origSize);
                                memcpy(dupParam, origParam, origSize);
                                // This works because the param buffer allocation is also done by
                                // multiples of 4 bytes originally. In theory we should memcpy only
                                // the actual param size, that is without rounding vsize.
                                mParams.add(dupParam);
                            }
                        }
        /*virtual*/ ~EffectDesc() {
            free(mName);
            for (size_t k = 0; k < mParams.size(); k++) {
                free(mParams[k]);
            }
        }
        char *mName;
        effect_uuid_t mUuid;
        Vector <effect_param_t *> mParams;
    };

    class InputSourceDesc {
    public:
        InputSourceDesc() {}
        /*virtual*/ ~InputSourceDesc() {
            for (size_t j = 0; j < mEffects.size(); j++) {
                delete mEffects[j];
            }
        }
        Vector <EffectDesc *> mEffects;
    };


    class InputDesc {
    public:
        InputDesc(int session) : mSessionId(session) {}
        /*virtual*/ ~InputDesc() {}
        const int mSessionId;
        Vector< sp<AudioEffect> >mEffects;
    };

    static const char * const kInputSourceNames[AUDIO_SOURCE_CNT -1];

    void setPreProcessorEnabled(const InputDesc *inputDesc, bool enabled);
    status_t loadPreProcessorConfig(const char *path);
    status_t loadEffects(cnode *root, Vector <EffectDesc *>& effects);
    EffectDesc *loadEffect(cnode *root);
    status_t loadInputSources(cnode *root, const Vector <EffectDesc *>& effects);
    audio_source_t inputSourceNameToEnum(const char *name);
    InputSourceDesc *loadInputSource(cnode *root, const Vector <EffectDesc *>& effects);
    void loadEffectParameters(cnode *root, Vector <effect_param_t *>& params);
    effect_param_t *loadEffectParameter(cnode *root);
    size_t readParamValue(cnode *node,
                          char *param,
                          size_t *curSize,
                          size_t *totSize);
    size_t growParamSize(char *param,
                         size_t size,
                         size_t *curSize,
                         size_t *totSize);

    // Internal dump utilities.
    status_t dumpPermissionDenial(int fd);

    void setPowerHint(bool active);

    mutable Mutex mLock;    // prevents concurrent access to AudioPolicy manager functions changing
                            // device connection state  or routing
    sp<AudioCommandThread> mAudioCommandThread;     // audio commands thread
    sp<AudioCommandThread> mTonePlaybackThread;     // tone playback thread
    sp<AudioCommandThread> mOutputCommandThread;    // process stop and release output
    struct audio_policy_device *mpAudioPolicyDev;
    struct audio_policy *mpAudioPolicy;
    KeyedVector< audio_source_t, InputSourceDesc* > mInputSources;
    KeyedVector< audio_io_handle_t, InputDesc* > mInputs;

    power_module_t *mPowerModule;
};

}; // namespace android

#endif // ANDROID_AUDIOPOLICYSERVICE_H
