/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef AUDIO_TEST_UTILS_H_
#define AUDIO_TEST_UTILS_H_

#include <sys/stat.h>
#include <unistd.h>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

#include <android-base/thread_annotations.h>
#include <binder/MemoryDealer.h>
#include <media/AidlConversion.h>
#include <media/AudioRecord.h>
#include <media/AudioTrack.h>

using namespace android;

struct MixPort {
    std::string name;
    std::string role;
    std::string flags;
};

struct Route {
    std::string name;
    std::string sources;
    std::string sink;
};

status_t isAutomotivePlatform(bool* isAutomotive);
status_t listAudioPorts(std::vector<audio_port_v7>& portsVec);
status_t listAudioPatches(std::vector<struct audio_patch>& patchesVec);
status_t getAnyPort(audio_port_role_t role, audio_port_type_t type, audio_port_v7& port);
status_t getPortByAttributes(audio_port_role_t role, audio_port_type_t type,
                             audio_devices_t deviceType, const std::string& address,
                             audio_port_v7& port);
status_t getPatchForOutputMix(audio_io_handle_t audioIo, audio_patch& patch);
status_t getPatchForInputMix(audio_io_handle_t audioIo, audio_patch& patch);
bool patchContainsOutputDevices(DeviceIdVector deviceIds, audio_patch patch);
bool patchContainsInputDevice(audio_port_handle_t deviceId, audio_patch patch);
bool checkPatchPlayback(audio_io_handle_t audioIo, const DeviceIdVector& deviceIds);
bool checkPatchCapture(audio_io_handle_t audioIo, audio_port_handle_t deviceId);
std::string dumpPort(const audio_port_v7& port);
std::string dumpPortConfig(const audio_port_config& port);
std::string dumpPatch(const audio_patch& patch);

class OnAudioDeviceUpdateNotifier : public AudioSystem::AudioDeviceCallback {
  public:
    void onAudioDeviceUpdate(audio_io_handle_t audioIo, const DeviceIdVector& deviceIds) override;
    status_t waitForAudioDeviceCb(audio_port_handle_t expDeviceId = AUDIO_PORT_HANDLE_NONE);
    std::pair<audio_io_handle_t, DeviceIdVector> getLastPortAndDevices() const;

  private:
    audio_io_handle_t mAudioIo GUARDED_BY(mMutex) = AUDIO_IO_HANDLE_NONE;
    DeviceIdVector mDeviceIds GUARDED_BY(mMutex);
    mutable std::mutex mMutex;
    std::condition_variable mCondition;
};

namespace {

class TestAudioTrack : public AudioTrack {
  public:
    explicit TestAudioTrack(const AttributionSourceState& attributionSourceState = {})
        : AudioTrack(attributionSourceState) {}
    TestAudioTrack(audio_stream_type_t streamType, uint32_t sampleRate, audio_format_t format,
                   audio_channel_mask_t channelMask, const sp<IMemory>& sharedBuffer,
                   audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
                   const wp<IAudioTrackCallback>& callback = nullptr,
                   int32_t notificationFrames = 0,
                   audio_session_t sessionId = AUDIO_SESSION_ALLOCATE,
                   transfer_type transferType = TRANSFER_DEFAULT,
                   const audio_offload_info_t* offloadInfo = nullptr,
                   const AttributionSourceState& attributionSource = AttributionSourceState(),
                   const audio_attributes_t* pAttributes = nullptr, bool doNotReconnect = false,
                   float maxRequiredSpeed = 1.0f)
        : AudioTrack(streamType, sampleRate, format, channelMask, sharedBuffer, flags, callback,
                     notificationFrames, sessionId, transferType, offloadInfo, attributionSource,
                     pAttributes, doNotReconnect, maxRequiredSpeed) {}
    // The callback thread is normally used for TRANSFER_SYNC_NOTIF_CALLBACK
    // in order to deliver "more data" callback. However, for offload we are
    // interested in the "stream end" event which is also served via the same
    // callback interface.
    void wakeCallbackThread() {
        if (sp<AudioTrackThread> t = mAudioTrackThread; t != nullptr) {
            t->wake();
        }
    }
};

}  // namespace

// Simple AudioPlayback class.
class AudioPlayback : public AudioTrack::IAudioTrackCallback {
    friend sp<AudioPlayback>;
    AudioPlayback(uint32_t sampleRate, audio_format_t format, audio_channel_mask_t channelMask,
                  audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
                  audio_session_t sessionId = AUDIO_SESSION_NONE,
                  AudioTrack::transfer_type transferType = AudioTrack::TRANSFER_SHARED,
                  audio_attributes_t* attributes = nullptr, audio_offload_info_t* info = nullptr);

  public:
    status_t loadResource(const char* name);
    status_t create();
    sp<AudioTrack> getAudioTrackHandle();
    status_t start();
    status_t waitForConsumption(bool testSeek = false) EXCLUDES(mMutex);
    status_t fillBuffer();
    status_t onProcess(bool testSeek = false);
    void pause();
    void resume();
    void stop() EXCLUDES(mMutex);
    bool waitForStreamEnd();

    // IAudioTrackCallback
    void onBufferEnd() override EXCLUDES(mMutex);
    void onStreamEnd() override EXCLUDES(mMutex);

    bool mStopPlaying GUARDED_BY(mMutex) = false;

    enum State {
        PLAY_NO_INIT,
        PLAY_READY,
        PLAY_STARTED,
        PLAY_STOPPED,
    };

  private:
    ~AudioPlayback();
    const uint32_t mSampleRate;
    const audio_format_t mFormat;
    const audio_channel_mask_t mChannelMask;
    const audio_output_flags_t mFlags;
    const audio_session_t mSessionId;
    const AudioTrack::transfer_type mTransferType;
    const audio_attributes_t* mAttributes;
    const audio_offload_info_t* mOffloadInfo;

    size_t mBytesUsedSoFar = 0;
    State mState = PLAY_NO_INIT;
    size_t mMemCapacity = 0;
    sp<MemoryDealer> mMemoryDealer;
    sp<IMemory> mMemory;
    sp<TestAudioTrack> mTrack;
    mutable std::mutex mMutex;
    bool mStreamEndReceived GUARDED_BY(mMutex) = false;
    std::condition_variable mCondition;
};

// hold pcm data sent by AudioRecord
class RawBuffer {
  public:
    RawBuffer(int64_t ptsPipeline = -1, int64_t ptsManual = -1, int32_t capacity = 0);

    std::unique_ptr<uint8_t[]> mData;
    int64_t mPtsPipeline;
    int64_t mPtsManual;
    int32_t mCapacity;
};

// Simple AudioCapture
class AudioCapture : public AudioRecord::IAudioRecordCallback {
  public:
    AudioCapture(audio_source_t inputSource, uint32_t sampleRate, audio_format_t format,
                 audio_channel_mask_t channelMask,
                 audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE,
                 audio_session_t sessionId = AUDIO_SESSION_ALLOCATE,
                 AudioRecord::transfer_type transferType = AudioRecord::TRANSFER_CALLBACK,
                 const audio_attributes_t* attributes = nullptr);
    ~AudioCapture();
    size_t onMoreData(const AudioRecord::Buffer& buffer) override EXCLUDES(mMutex);
    void onOverrun() override;
    void onMarker(uint32_t markerPosition) override EXCLUDES(mMutex);
    void onNewPos(uint32_t newPos) override EXCLUDES(mMutex);
    void onNewIAudioRecord() override;
    status_t create();
    status_t setRecordDuration(float durationInSec);
    status_t enableRecordDump();
    std::string getRecordDumpFileName() const { return mFileName; }
    sp<AudioRecord> getAudioRecordHandle();
    status_t start(AudioSystem::sync_event_t event = AudioSystem::SYNC_EVENT_NONE,
                   audio_session_t triggerSession = AUDIO_SESSION_NONE);
    status_t obtainBufferCb(RawBuffer& buffer) EXCLUDES(mMutex);
    status_t obtainBuffer(RawBuffer& buffer) EXCLUDES(mMutex);
    status_t audioProcess() EXCLUDES(mMutex);
    status_t stop() EXCLUDES(mMutex);
    uint32_t getMarkerPeriod() const EXCLUDES(mMutex);
    uint32_t getMarkerPosition() const EXCLUDES(mMutex);
    void setMarkerPeriod(uint32_t markerPeriod) EXCLUDES(mMutex);
    void setMarkerPosition(uint32_t markerPosition) EXCLUDES(mMutex);
    uint32_t waitAndGetReceivedCbMarkerAtPosition() const EXCLUDES(mMutex);
    uint32_t waitAndGetReceivedCbMarkerCount() const EXCLUDES(mMutex);

    uint32_t mFrameCount = 0;
    uint32_t mNotificationFrames = 0;
    int64_t mNumFramesToRecord = 0;

    enum State {
        REC_NO_INIT,
        REC_READY,
        REC_STARTED,
        REC_STOPPED,
    };

  private:
    const audio_source_t mInputSource;
    const uint32_t mSampleRate;
    const audio_format_t mFormat;
    const audio_channel_mask_t mChannelMask;
    const audio_input_flags_t mFlags;
    const audio_session_t mSessionId;
    const AudioRecord::transfer_type mTransferType;
    const audio_attributes_t* mAttributes;

    size_t mMaxBytesPerCallback = 2048;
    sp<AudioRecord> mRecord;
    State mState = REC_NO_INIT;
    bool mStopRecording GUARDED_BY(mMutex) = false;
    std::string mFileName;
    int mOutFileFd = -1;

    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    std::deque<RawBuffer> mBuffersReceived GUARDED_BY(mMutex);

    mutable std::condition_variable mMarkerCondition;
    uint32_t mMarkerPeriod GUARDED_BY(mMutex) = 0;
    uint32_t mMarkerPosition GUARDED_BY(mMutex) = 0;
    std::optional<uint32_t> mReceivedCbMarkerCount GUARDED_BY(mMutex);
    std::optional<uint32_t> mReceivedCbMarkerAtPosition GUARDED_BY(mMutex);

    int64_t mNumFramesReceived GUARDED_BY(mMutex) = 0;
    int64_t mNumFramesLost GUARDED_BY(mMutex) = 0;
};

#endif  // AUDIO_TEST_UTILS_H_
