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

#include <chrono>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

#include <aaudio/AAudio.h>

#include "AAudioArgsParser.h"
#include "AAudioSimplePlayer.h"
#include "SineGenerator.h"

/**
 * This tests AAudioStream_flushFromPosition. This test will do playback in offload mode
 * and try to flush from frame every 2 seconds. It will switch the sine generator between
 * 440Hz and 880Hz when flush from frame.
 */

constexpr static int DEFAULT_TIME_TO_RUN_IN_SECOND = 10;
constexpr static int PLAYBACK_LENGTH_SECONDS = 2;
constexpr static int MORE_DATA_IN_SECONDS = 3;

int32_t MyPartialDataCallback(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error);

class MyPlayer : public AAudioSimplePlayer {
public:
    MyPlayer(AAudioArgsParser& argParser, bool useDataCallback)
            : mArgParser(argParser), mUseDataCallback(useDataCallback) {}

    aaudio_result_t open() {
        aaudio_result_t result = AAudioSimplePlayer::open(
                mArgParser,
                nullptr /*dataCallback*/,
                &MyErrorCallback,
                this,
                nullptr /*presentationEndCallback*/,
                mUseDataCallback ? &MyPartialDataCallback : nullptr);
        if (result != AAUDIO_OK) {
            return result;
        }
        mChannelCount = getChannelCount();
        mSampleRate = getSampleRate();
        mSines.resize(2);
        for (int i = 0; i < mChannelCount; ++i) {
            SineGenerator sine;
            sine.setup(440.0, mSampleRate);
            mSines[0].push_back(sine);
            SineGenerator sine1;
            sine1.setup(880.0, mSampleRate);
            mSines[1].push_back(sine1);
        }
        return result;
    }

    int32_t renderAudio(AAudioStream* stream, void* audioData, int32_t numFrames) {
        int index = 0;
        {
            std::lock_guard _l(mFlushMutex);
            if (mPendingFlush) {
                return 0;
            }
            index = mSineGeneratorIndex;
        }
        // Just handle PCM_16 and PCM_FLOAT for testing
        if (!fillData(stream, audioData, numFrames, index)) {
            printf("Failed to render data, stop the stream\n");
            return -1;
        }
        return numFrames;
    }

    void writeData() {
        for (int i = 0; i < MORE_DATA_IN_SECONDS; ++i) {
            writeOneSecondData();
        }
    }

    aaudio_result_t flushFromFrame(AAudio_FlushFromAccuracy accuracy, int64_t* position) {
        if (position == nullptr) {
            return AAUDIO_ERROR_ILLEGAL_ARGUMENT;
        }
        aaudio_result_t result = AAUDIO_OK;
        {
            std::lock_guard _l(mFlushMutex);
            mPendingFlush = true;
            mSineGeneratorIndex ^= 1;
        }
        {
            std::lock_guard _l(mStreamMutex);
            auto framesWritten = AAudioStream_getFramesWritten(getStream());
            int64_t requestedPosition = *position;
            result = AAudioStream_flushFromFrame(getStream(), accuracy, position);
            printf("%s(%d, %jd) result=%d actual position=%jd, frames written before "
                   "flushFromFrame=%jd frames written after flushFromFrame=%jd\n",
                   __func__, accuracy, requestedPosition, result, *position, framesWritten,
                   AAudioStream_getFramesWritten(getStream()));
        }
        {
            std::lock_guard _l(mFlushMutex);
            mPendingFlush = false;
        }
        return result;
    }

    aaudio_result_t close() override {
        std::lock_guard _l(mStreamMutex);
        return AAudioSimplePlayer::close();
    }

private:
    void writeOneSecondData() {
        int index = 0;
        {
            std::lock_guard _l(mFlushMutex);
            if (mPendingFlush) {
                return;
            }
            index = mSineGeneratorIndex;
        }
        // Lock to prevent the stream is released
        std::lock_guard _l(mStreamMutex);
        AAudioStream* stream = getStream();
        if (stream == nullptr) {
            return;
        }
        int bytesPerFrame = mChannelCount;
        std::shared_ptr<uint8_t[]> data;
        switch (AAudioStream_getFormat(stream)) {
            case AAUDIO_FORMAT_PCM_I16: {
                bytesPerFrame *= 2;
            } break;
            case AAUDIO_FORMAT_PCM_FLOAT: {
                bytesPerFrame *= 4;
            } break;
            default:
                printf("Unsupported format %d\n", AAudioStream_getFormat(stream));
                return;
        }
        data = std::make_shared<uint8_t[]>(bytesPerFrame * mSampleRate);
        fillData(stream, static_cast<void*>(data.get()), mSampleRate, index);
        int bytesWritten = 0;
        int framesLeft = mSampleRate;
        while (framesLeft > 0) {
            auto framesWritten = AAudioStream_write(
                    stream, static_cast<void *>(&data[bytesWritten]),
                    framesLeft, 0 /*timeoutNanoseconds*/);
            if (framesWritten < 0) {
                printf("Failed to write data %d\n", framesWritten);
                return;
            }
            printf("Write data succeed, frames=%d\n", framesWritten);
            framesLeft -= framesWritten;
            bytesWritten += framesWritten * bytesPerFrame;
        }
    }

    bool fillData(AAudioStream* stream, void* data, int numFrames, int sineGeneratorIndex) {
        switch (AAudioStream_getFormat(stream)) {
            case AAUDIO_FORMAT_PCM_I16: {
                int16_t *audioBuffer = static_cast<int16_t *>(data);
                for (int i = 0; i < mChannelCount; ++i) {
                    mSines[sineGeneratorIndex][i].render(&audioBuffer[i], mChannelCount, numFrames);
                }
            } break;
            case AAUDIO_FORMAT_PCM_FLOAT: {
                float *audioBuffer = static_cast<float *>(data);
                for (int i = 0; i < mChannelCount; ++i) {
                    mSines[sineGeneratorIndex][i].render(&audioBuffer[i], mChannelCount, numFrames);
                }
            } break;
            default:
                return false;
        }
        return true;
    }

    const AAudioArgsParser mArgParser;
    const bool mUseDataCallback;

    int mSampleRate;
    int mChannelCount;
    std::vector<std::vector<SineGenerator>> mSines;
    int mSineGeneratorIndex = 0;

    std::mutex mStreamMutex;

    std::mutex mFlushMutex;
    bool mPendingFlush = false;
};

int32_t MyPartialDataCallback(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames) {
    MyPlayer* player = static_cast<MyPlayer*>(userData);
    return player->renderAudio(stream, audioData, numFrames);
}

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error) {
    printf("Error callback, error=%d\n", error);
}

static void usage() {
    printf("This test will playback in offload mode and try to flush from frame "
           "every 2 seconds\n");
    AAudioArgsParser::usage();
    printf("      -T{seconds} time to run the test\n");
    printf("      -B use blocking write instead of data callback\n");
}

int main(int argc, char **argv) {
    AAudioArgsParser argParser;
    int timeToRun = DEFAULT_TIME_TO_RUN_IN_SECOND;
    bool useDataCallback = true;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (argParser.parseArg(arg)) {
            if (arg[0] == '-') {
                char option = arg[1];
                switch (option) {
                    case 'T':
                        timeToRun = atoi(&arg[2]);
                        break;
                    case 'B':
                        useDataCallback = false;
                        break;
                    default:
                        usage();
                        exit(EXIT_FAILURE);
                }
            } else {
                usage();
                exit(EXIT_FAILURE);
            }
        }
    }

    // Force to use offload mode
    argParser.setPerformanceMode(AAUDIO_PERFORMANCE_MODE_POWER_SAVING_OFFLOADED);
    argParser.setNumberOfBursts(0);

    MyPlayer player(argParser, useDataCallback);
    if (auto result = player.open(); result != AAUDIO_OK) {
        printf("Failed to open stream, error=%d\n", result);
        exit(EXIT_FAILURE);
    }

    if (auto result = player.start(); result != AAUDIO_OK) {
        printf("Failed to start stream, error=%d", result);
        exit(EXIT_FAILURE);
    }

    auto timeStart = std::chrono::system_clock::now();
    int64_t position = 0;

    while (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - timeStart).count() < timeToRun) {
        if (useDataCallback) {
            std::this_thread::sleep_for(std::chrono::seconds(PLAYBACK_LENGTH_SECONDS));
            player.flushFromFrame(AAUDIO_FLUSH_FROM_ACCURACY_UNDEFINED, &position);
        } else {
            auto timeToWakeUp = std::chrono::system_clock::now() +
                    std::chrono::seconds(PLAYBACK_LENGTH_SECONDS);
            player.writeData();
            std::this_thread::sleep_until(timeToWakeUp);
            player.flushFromFrame(AAUDIO_FLUSH_FROM_ACCURACY_UNDEFINED, &position);
        }
    }

    player.close();
    return EXIT_SUCCESS;
}
