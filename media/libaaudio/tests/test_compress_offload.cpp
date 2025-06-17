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

// Compress offload

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include <aaudio/AAudio.h>
#include <android-base/thread_annotations.h>

#include "AAudioArgsParser.h"
#include "AAudioSimplePlayer.h"
#include "SineGenerator.h"

const static int DEFAULT_TIME_TO_RUN_IN_SECOND = 60;

aaudio_data_callback_result_t MyDatacallback(AAudioStream* stream,
                                             void* userData,
                                             void* audioData,
                                             int32_t numFrames);

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error);

void MyPresentationEndCallback(AAudioStream* /*stream*/, void* userData);

class FileDataProvider {
public:
    bool loadData(const std::string& filePath) {
        mPosition = 0;
        std::ifstream is(filePath, std::ios::in | std::ios::binary);
        if (!is.good()) {
            printf("Failed to open file %s\n", filePath.c_str());
            return false;
        }
        is.seekg(0, is.end);
        mData.reserve(mData.size() + is.tellg());
        is.seekg(0, is.beg);
        mData.insert(mData.end(), std::istreambuf_iterator<char>(is),
                     std::istreambuf_iterator<char>());
        if (is.fail()) {
            printf("Failed to read from file %s\n", filePath.c_str());
            return false;
        }
        return true;
    }

    std::pair<bool, int> copyData(void* audioData, int32_t numFrames) {
        bool endOfFile = false;
        int dataToCopy = std::min((int)mData.size() - mPosition, numFrames);
        std::copy(mData.begin() + mPosition, mData.begin() + mPosition + dataToCopy,
                  static_cast<uint8_t*>(audioData));
        mPosition += dataToCopy;
        if (mPosition >= mData.size()) {
            endOfFile = true;
            mPosition = 0;
        }
        return {endOfFile, dataToCopy};
    }

private:
    std::vector<uint8_t> mData;
    int mPosition;
};

class CompressOffloadPlayer : public AAudioSimplePlayer {
public:
    CompressOffloadPlayer(AAudioArgsParser& argParser, int delay, int padding,
                          bool useDataCallback, const std::string& filePath)
            : mArgParser(argParser), mDelay(delay), mPadding(padding),
              mUseDataCallback(useDataCallback), mFilePath(filePath) {
    }

    aaudio_result_t open() {
        if (!mDataProvider.loadData(mFilePath)) {
            return AAUDIO_ERROR_ILLEGAL_ARGUMENT;
        }
        return AAudioSimplePlayer::open(
                mArgParser,
                mUseDataCallback ? &MyDatacallback : nullptr,
                &MyErrorCallback,
                this,
                &MyPresentationEndCallback);
    }

    aaudio_data_callback_result_t renderAudio(AAudioStream* /*stream*/,
                                              void* audioData,
                                              int32_t numFrames) {
        {
            std::lock_guard lk(mWaitForExitingLock);
            mReadyToExit = false;
        }
        auto [endOfFile, dataCopied] = mDataProvider.copyData(audioData, numFrames);
        if (endOfFile) {
            printf("%s(%d): endOfFile=%d, dataCopied=%d\n", __func__, numFrames, endOfFile,
                   dataCopied);
            setOffloadEndOfStream();
            return AAUDIO_CALLBACK_RESULT_STOP;
        }
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    void presentationEnd() {
        printf("Presentation end\n");
        {
            std::lock_guard lk(mWaitForExitingLock);
            mReadyToExit = true;
        }
        mCV.notify_one();
        setOffloadDelayPadding(mDelay, mPadding);
        if (!mUseDataCallback) {
            std::thread(&CompressOffloadPlayer::writeAllStreamData, this).detach();
        }
    }

    void writeData() {
        writeAllStreamData();
    }

    void waitForExiting() {
        printf("%s\n", __func__);
        std::unique_lock lk(mWaitForExitingLock);
        mCV.wait(lk, [this]{ return mReadyToExit; });
    }

private:
    void writeAllStreamData() {
        int dataSize = mArgParser.getSampleRate();
        uint8_t data[dataSize];
        static constexpr int64_t kTimeOutNanos = 1e9;
        while (true) {
            auto [endOfFile, dataCopied] = mDataProvider.copyData(data, dataSize);
            auto result = AAudioStream_write(getStream(), data, dataCopied, kTimeOutNanos);
            if (result < AAUDIO_OK) {
                printf("Failed to write data, error=%d\n", result);
                break;
            }
            if (endOfFile) {
                printf("All data from the file is written, set offload end of stream\n");
                setOffloadEndOfStream();
                break;
            }
        }
    }

    const AAudioArgsParser mArgParser;
    const int mDelay;
    const int mPadding;
    const bool mUseDataCallback;
    const std::string mFilePath;

    FileDataProvider mDataProvider;
    std::mutex mWaitForExitingLock;
    std::condition_variable mCV;
    bool mReadyToExit GUARDED_BY(mWaitForExitingLock);
};

aaudio_data_callback_result_t MyDatacallback(AAudioStream* stream,
                                             void* userData,
                                             void* audioData,
                                             int32_t numFrames) {
    CompressOffloadPlayer* player = static_cast<CompressOffloadPlayer*>(userData);
    return player->renderAudio(stream, audioData, numFrames);
}

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error) {
    printf("Error callback, error=%d\n", error);
}

void MyPresentationEndCallback(AAudioStream* /*stream*/, void* userData) {
    CompressOffloadPlayer* player = static_cast<CompressOffloadPlayer*>(userData);
    return player->presentationEnd();
}

static void usage() {
    AAudioArgsParser::usage();
    printf("      -D{delay} offload delay in frames\n");
    printf("      -P{padding} offload padding in frames\n");
    printf("      -T{seconds} time to run the test\n");
    printf("      -F{filePath} file path for the compressed data\n");
    printf("      -B use blocking write instead of data callback\n");
}

int main(int argc, char **argv) {
    AAudioArgsParser argParser;
    int delay = 0;
    int padding = 0;
    int timeToRun = DEFAULT_TIME_TO_RUN_IN_SECOND;
    bool useDataCallback = true;
    std::string filePath;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (argParser.parseArg(arg)) {
            if (arg[0] == '-') {
                char option = arg[1];
                switch (option) {
                    case 'D':
                        delay = atoi(&arg[2]);
                        break;
                    case 'P':
                        padding = atoi(&arg[2]);
                        break;
                    case 'T':
                        timeToRun = atoi(&arg[2]);
                        break;
                    case 'B':
                        useDataCallback = false;
                        break;
                    case 'F':
                        filePath = &arg[2];
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

    if (filePath.empty()) {
        printf("A file path must be specified\n");
        usage();
        exit(EXIT_FAILURE);
    }

    // Force to use offload mode
    argParser.setPerformanceMode(AAUDIO_PERFORMANCE_MODE_POWER_SAVING_OFFLOADED);

    CompressOffloadPlayer player(
            argParser, delay, padding, useDataCallback, filePath);
    if (auto result = player.open(); result != AAUDIO_OK) {
        printf("Failed to open stream, error=%d\n", result);
        exit(EXIT_FAILURE);
    }

    // Failed to set offload delay and padding will affect the gapless transition between tracks
    // but doesn't affect playback.
    (void) player.setOffloadDelayPadding(delay, padding);

    if (auto result = player.start(); result != AAUDIO_OK) {
        printf("Failed to start stream, error=%d", result);
        exit(EXIT_FAILURE);
    } else if (!useDataCallback) {
        player.writeData();
    }

    sleep(timeToRun);

    player.stop();

    player.waitForExiting();

    return EXIT_SUCCESS;
}
