/*
 * Copyright 2013 The Android Open Source Project
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
 *
 * Per article 5 of the Apache 2.0 License, some modifications to this code
 * were made by the OmniROM Project.
 *
 * Modifications Copyright (C) 2013 The OmniROM Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define LOG_TAG "ScreenRecord"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/Thread.h>
#include <utils/Timers.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <media/AudioSystem.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/stagefright/AudioSource.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaMuxer.h>
#include <media/stagefright/MetaData.h>
#include <media/ICrypto.h>
#include <system/audio_policy.h>
#include <system/audio.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <sys/wait.h>

using namespace android;

static const uint32_t kMinBitRate = 100000;         // 0.1Mbps
static const uint32_t kMaxBitRate = 100 * 1000000;  // 100Mbps
static const uint32_t kMaxTimeLimitSec = 30*60;     // 30 minutes
static const uint32_t kFallbackWidth = 1280;        // 720p
static const uint32_t kFallbackHeight = 720;

// Build-time parameters.
#ifdef LANDSCAPE_ONLY
static bool gLandscapeOnly = true;          // Only encode in landscape
#else
static bool gLandscapeOnly = false;
#endif

// Command-line parameters.
static bool gVerbose = false;               // chatty on stdout
static bool gRotate = false;                // rotate 90 degrees
static bool gSizeSpecified = false;         // was size explicitly requested?
static uint32_t gVideoWidth = 0;            // default width+height
static uint32_t gVideoHeight = 0;
static uint32_t gBitRate = 4000000;         // 4Mbps
static uint32_t gTimeLimitSec = kMaxTimeLimitSec;
static audio_source_t gAudioInput = AUDIO_SOURCE_REMOTE_SUBMIX;

// Set by signal handler to stop recording.
static bool gStopRequested;

// Previous signal handler state, restored after first hit.
static struct sigaction gOrigSigactionINT;
static struct sigaction gOrigSigactionHUP;


/*
 * Catch keyboard interrupt signals.  On receipt, the "stop requested"
 * flag is raised, and the original handler is restored (so that, if
 * we get stuck finishing, a second Ctrl-C will kill the process).
 */
static void signalCatcher(int signum)
{
    gStopRequested = true;
    switch (signum) {
    case SIGINT:
    case SIGHUP:
        sigaction(SIGINT, &gOrigSigactionINT, NULL);
        sigaction(SIGHUP, &gOrigSigactionHUP, NULL);
        break;
    default:
        abort();
        break;
    }
}

/*
 * Configures signal handlers.  The previous handlers are saved.
 *
 * If the command is run from an interactive adb shell, we get SIGINT
 * when Ctrl-C is hit.  If we're run from the host, the local adb process
 * gets the signal, and we get a SIGHUP when the terminal disconnects.
 */
static status_t configureSignals()
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signalCatcher;
    if (sigaction(SIGINT, &act, &gOrigSigactionINT) != 0) {
        status_t err = -errno;
        fprintf(stderr, "Unable to configure SIGINT handler: %s\n",
                strerror(errno));
        return err;
    }
    if (sigaction(SIGHUP, &act, &gOrigSigactionHUP) != 0) {
        status_t err = -errno;
        fprintf(stderr, "Unable to configure SIGHUP handler: %s\n",
                strerror(errno));
        return err;
    }
    return NO_ERROR;
}

/*
 * Returns "true" if the device is rotated 90 degrees.
 */
static bool isDeviceRotated(int orientation) {
    return orientation != DISPLAY_ORIENTATION_0 &&
            orientation != DISPLAY_ORIENTATION_180;
}

static status_t setAudioRouteStatus(const audio_source_t input,
        audio_policy_dev_state_t state) {
    return AudioSystem::setDeviceConnectionState(
            input,
            state,
            NULL /* device_address */);
}

static void setAudioSubMixRouting(bool enabled) {
    status_t err = setAudioRouteStatus((audio_source_t) AUDIO_DEVICE_IN_REMOTE_SUBMIX,
            enabled ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE : AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE);
    if (err != OK) {
        fprintf(stderr, "WARN: Unable to set device connection state for audio submix IN\n");
    }

    err = setAudioRouteStatus((audio_source_t) AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
            enabled ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE : AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE);
    if (err != OK) {
        fprintf(stderr, "WARN: Unable to set device connection state for audio submix OUT\n");
    }
}

static status_t prepareAudioEncoder(sp<MediaCodec>* pCodec,
        sp<AudioSource>* pSource, Vector< sp<ABuffer> >* inputBuffers) {
    status_t err = OK;

    // Setup the audio source. Note that audio will be bypassed from the
    // actual device speakers.
    if (gAudioInput == AUDIO_SOURCE_REMOTE_SUBMIX) {
        // We make sure that the audio source is enabled
        setAudioSubMixRouting(true);
    }

    // Then we capture that source
    sp<AudioSource> source = new AudioSource(
            gAudioInput,
            48000 /* sampleRate */,
            2 /* channelCount */);

    err = source->initCheck();
    if (err != OK) {
        fprintf(stderr, "Unable to instantiate audio source (error %d)!\n", err);
        source = NULL;
        return err;
    }

    sp<MetaData> params = new MetaData;
    params->setInt64(kKeyTime, 1ll);
    err = source->start(params.get());
    if (err != OK) {
        fprintf(stderr, "Cannot start AudioSource\n");
        source = NULL;
        return err;
    }

    // AAC Encoder: 128kbps, 48KHz, Stereo
    sp<AMessage> format = new AMessage;
    format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC, strlen(MEDIA_MIMETYPE_AUDIO_AAC));
    format->setInt32("bitrate", 128000);
    format->setInt32("sample-rate", 48000);
    format->setInt32("channel-count", 2);

    sp<ALooper> looper = new ALooper;
    looper->setName("audiorecord_looper");
    looper->start();

    sp<MediaCodec> encoder = MediaCodec::CreateByType(looper, MEDIA_MIMETYPE_AUDIO_AAC, true);
    if (encoder == NULL) {
        fprintf(stderr, "ERROR: unable to create %s codec instance\n",
                MEDIA_MIMETYPE_AUDIO_AAC);
        return -1;
    }
    err = encoder->configure(format, NULL, NULL,
            MediaCodec::CONFIGURE_FLAG_ENCODE);
    if (err != NO_ERROR) {
        encoder->release();
        encoder.clear();

        fprintf(stderr, "ERROR: unable to configure codec (err=%d)\n", err);
        return -1;
    }

    encoder->start();

    // We don't have any buffer producer from audio source, so we'll be in charge
    // of feeding our RAW PCM buffers to the audio encoder ourselves.
    err = encoder->getInputBuffers(inputBuffers);

    if (err != OK) {
        fprintf(stderr, "ERROR: unable to set input buffers to the encoder (err=%d)\n", err);
        encoder->release();
        encoder.clear();

        return err;
    }

    *pCodec = encoder;
    *pSource = source;

    return NO_ERROR;
}

/*
 * Configures and starts the MediaCodec encoder.  Obtains an input surface
 * from the codec.
 */
static status_t prepareEncoder(float displayFps, sp<MediaCodec>* pCodec,
        sp<IGraphicBufferProducer>* pBufferProducer) {
    status_t err;

    if (gVerbose) {
        printf("Configuring recorder for %dx%d video at %.2fMbps\n",
                gVideoWidth, gVideoHeight, gBitRate / 1000000.0);
    }

    sp<AMessage> format = new AMessage;
    format->setInt32("width", gVideoWidth);
    format->setInt32("height", gVideoHeight);
    format->setString("mime", "video/avc");
    format->setInt32("color-format", OMX_COLOR_FormatAndroidOpaque);
    format->setInt32("bitrate", gBitRate);
    format->setFloat("frame-rate", displayFps);
    format->setInt32("i-frame-interval", 10);

    sp<ALooper> looper = new ALooper;
    looper->setName("screenrecord_looper");
    looper->start();
    ALOGV("Creating codec");
    sp<MediaCodec> codec = MediaCodec::CreateByType(looper, "video/avc", true);
    if (codec == NULL) {
        fprintf(stderr, "ERROR: unable to create video/avc codec instance\n");
        return UNKNOWN_ERROR;
    }
    err = codec->configure(format, NULL, NULL,
            MediaCodec::CONFIGURE_FLAG_ENCODE);
    if (err != NO_ERROR) {
        codec->release();
        codec.clear();

        fprintf(stderr, "ERROR: unable to configure codec (err=%d)\n", err);
        return err;
    }

    ALOGV("Creating buffer producer");
    sp<IGraphicBufferProducer> bufferProducer;
    err = codec->createInputSurface(&bufferProducer);
    if (err != NO_ERROR) {
        codec->release();
        codec.clear();

        fprintf(stderr,
            "ERROR: unable to create encoder input surface (err=%d)\n", err);
        return err;
    }

    ALOGV("Starting codec");
    err = codec->start();
    if (err != NO_ERROR) {
        codec->release();
        codec.clear();

        fprintf(stderr, "ERROR: unable to start codec (err=%d)\n", err);
        return err;
    }

    ALOGV("Codec prepared");
    *pCodec = codec;
    *pBufferProducer = bufferProducer;
    return 0;
}

/*
 * Configures the virtual display.  When this completes, virtual display
 * frames will start being sent to the encoder's surface.
 */
static status_t prepareVirtualDisplay(const DisplayInfo& mainDpyInfo,
        const sp<IGraphicBufferProducer>& bufferProducer,
        sp<IBinder>* pDisplayHandle) {
    status_t err;

    // Set the region of the layer stack we're interested in, which in our
    // case is "all of it".  If the app is rotated (so that the width of the
    // app is based on the height of the display), reverse width/height.
    bool deviceRotated = isDeviceRotated(mainDpyInfo.orientation);
    uint32_t sourceWidth, sourceHeight;
    if (!deviceRotated) {
        sourceWidth = mainDpyInfo.w;
        sourceHeight = mainDpyInfo.h;
    } else {
        ALOGV("using rotated width/height");
        sourceHeight = mainDpyInfo.w;
        sourceWidth = mainDpyInfo.h;
    }
    Rect layerStackRect(sourceWidth, sourceHeight);

    // We need to preserve the aspect ratio of the display.
    float displayAspect = (float) sourceHeight / (float) sourceWidth;


    // Set the way we map the output onto the display surface (which will
    // be e.g. 1280x720 for a 720p video).  The rect is interpreted
    // post-rotation, so if the display is rotated 90 degrees we need to
    // "pre-rotate" it by flipping width/height, so that the orientation
    // adjustment changes it back.
    //
    // We might want to encode a portrait display as landscape to use more
    // of the screen real estate.  (If players respect a 90-degree rotation
    // hint, we can essentially get a 720x1280 video instead of 1280x720.)
    // In that case, we swap the configured video width/height and then
    // supply a rotation value to the display projection.
    uint32_t videoWidth, videoHeight;
    uint32_t outWidth, outHeight;
    if (!gRotate) {
        videoWidth = gVideoWidth;
        videoHeight = gVideoHeight;
    } else {
        videoWidth = gVideoHeight;
        videoHeight = gVideoWidth;
    }
    if (videoHeight > (uint32_t)(videoWidth * displayAspect)) {
        // limited by narrow width; reduce height
        outWidth = videoWidth;
        outHeight = (uint32_t)(videoWidth * displayAspect);
    } else {
        // limited by short height; restrict width
        outHeight = videoHeight;
        outWidth = (uint32_t)(videoHeight / displayAspect);
    }
    uint32_t offX, offY;
    offX = (videoWidth - outWidth) / 2;
    offY = (videoHeight - outHeight) / 2;
    Rect displayRect(offX, offY, offX + outWidth, offY + outHeight);

    if (gVerbose) {
        if (gRotate) {
            printf("Rotated content area is %ux%u at offset x=%d y=%d\n",
                    outHeight, outWidth, offY, offX);
        } else {
            printf("Content area is %ux%u at offset x=%d y=%d\n",
                    outWidth, outHeight, offX, offY);
        }
    }


    sp<IBinder> dpy = SurfaceComposerClient::createDisplay(
            String8("ScreenRecorder"), false /* secure */);

    SurfaceComposerClient::openGlobalTransaction();
    SurfaceComposerClient::setDisplaySurface(dpy, bufferProducer);
    SurfaceComposerClient::setDisplayProjection(dpy,
            gRotate ? DISPLAY_ORIENTATION_90 : DISPLAY_ORIENTATION_0,
            layerStackRect, displayRect);
    SurfaceComposerClient::setDisplayLayerStack(dpy, 0);    // default stack
    SurfaceComposerClient::closeGlobalTransaction();

    *pDisplayHandle = dpy;

    return NO_ERROR;
}

static status_t processDequeue(const sp<MediaCodec>& encoder,
        ssize_t* trackIdx,
        const uint32_t* debugNumFrames,
        Vector<sp<ABuffer> >* buffers,
        const sp<MediaMuxer>& muxer,
        bool isAudio) {
    static int kTimeout = 1;   // be responsive on signal
    size_t bufIndex, offset, size;
    int64_t ptsUsec;
    uint32_t flags;
    status_t err = NO_ERROR;

    ALOGV("Calling dequeueOutputBuffer");
    err = encoder->dequeueOutputBuffer(&bufIndex, &offset, &size, &ptsUsec,
            &flags, kTimeout);
    ALOGV("dequeueOutputBuffer returned %d", err);

    switch (err) {
    case NO_ERROR:
        // got a buffer
        if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
            // ignore this -- we passed the CSD into MediaMuxer when
            // we got the format change notification
            ALOGV("Got codec config buffer (%u bytes); ignoring", size);
            size = 0;
        }
        if (size != 0) {
            ALOGV("Got data in buffer %d, size=%d, pts=%lld",
                    bufIndex, size, ptsUsec);
            CHECK(*trackIdx != -1);

            // If the virtual display isn't providing us with timestamps,
            // use the current time.
            if (ptsUsec <= 0) {
                ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
            }

            // The MediaMuxer docs are unclear, but it appears that we
            // need to pass either the full set of BufferInfo flags, or
            // (flags & BUFFER_FLAG_SYNCFRAME).
            err = muxer->writeSampleData((*buffers)[bufIndex], *trackIdx,
                    ptsUsec, flags);
            if (err != NO_ERROR) {
                fprintf(stderr, "Failed writing data to muxer (err=%d)\n",
                        err);
                return err;
            }
            debugNumFrames++;
        }
        err = encoder->releaseOutputBuffer(bufIndex);
        if (err != NO_ERROR) {
            fprintf(stderr, "Unable to release output buffer (err=%d)\n",
                    err);
            return err;
        }
        if ((flags & MediaCodec::BUFFER_FLAG_EOS) != 0) {
            // Not expecting EOS from SurfaceFlinger.  Go with it.
            ALOGD("Received end-of-stream");
            gStopRequested = false;
        }
        break;
    case -EAGAIN:                       // INFO_TRY_AGAIN_LATER
        ALOGV("Got -EAGAIN, looping");
        break;
    case INFO_FORMAT_CHANGED:           // INFO_OUTPUT_FORMAT_CHANGED
        {
            // format includes CSD, which we must provide to muxer
            ALOGV("Encoder format changed");
            sp<AMessage> newFormat;
            encoder->getOutputFormat(&newFormat);
            *trackIdx = muxer->addTrack(newFormat);

            // Audio is dequeued after video, so in order to have both
            // tracks set in the muxer, we start it only on audio
            if (isAudio) {
                ALOGV("Starting muxer");
                err = muxer->start();
            } else {
                err = NO_ERROR;
            }
            if (err != NO_ERROR) {
                fprintf(stderr, "Unable to start muxer (err=%d)\n", err);
                return err;
            }
        }
        break;
    case INFO_OUTPUT_BUFFERS_CHANGED:   // INFO_OUTPUT_BUFFERS_CHANGED
        // not expected for an encoder; handle it anyway
        ALOGV("Encoder buffers changed");
        err = encoder->getOutputBuffers(buffers);
        if (err != NO_ERROR) {
            fprintf(stderr,
                    "Unable to get new output buffers (err=%d)\n", err);
            return err;
        }
        break;
    case INVALID_OPERATION:
        fprintf(stderr, "Request for encoder buffer failed\n");
        return err;
    default:
        fprintf(stderr,
                "Got weird result %d from dequeueOutputBuffer\n", err);
        return err;
    }

    return err;
}

/*
 * Runs the MediaCodec encoder, sending the output to the MediaMuxer.  The
 * input frames are coming from the virtual display as fast as SurfaceFlinger
 * wants to send them.
 *
 * The muxer must *not* have been started before calling.
 */
static status_t runEncoder(const sp<MediaCodec>& encoder,
        const sp<MediaCodec>& audioEncoder,
        const sp<AudioSource>& audioSource,
        const Vector< sp<ABuffer> >& audioEncoderInBuf,
        const sp<MediaMuxer>& muxer) {
    status_t err;
    ssize_t trackIdx = -1,
        audioTrackIdx = -1;
    uint32_t debugNumFrames = 0,
        debugNumAudioFrames = 0;
    int64_t startWhenNsec = systemTime(CLOCK_MONOTONIC);
    int64_t endWhenNsec = startWhenNsec + seconds_to_nanoseconds(gTimeLimitSec);

    Vector<sp<ABuffer> > buffers, audioBuffers;
    err = encoder->getOutputBuffers(&buffers);
    if (err != NO_ERROR) {
        fprintf(stderr, "Unable to get video output buffers (err=%d)\n", err);
        return err;
    }

    err = audioEncoder->getOutputBuffers(&audioBuffers);
    if (err != NO_ERROR) {
        fprintf(stderr, "Unable to get audio output buffers (err=%d)\n", err);
        return err;
    }

    // This is set by the signal handler.
    gStopRequested = false;

    // Run until we're signaled.
    while (!gStopRequested) {
        int64_t ptsUsec;
        size_t bufIndex;
        MediaBuffer* mbuf;
        Vector<int> bufferIndexes;

        if (systemTime(CLOCK_MONOTONIC) > endWhenNsec) {
            if (gVerbose) {
                printf("Time limit reached\n");
            }
            break;
        }

        // Process audio input (route PCM to AAC encoder)
        while (err == NO_ERROR) {
            err = audioEncoder->dequeueInputBuffer(&bufIndex);

            if (err == NO_ERROR) {
                bufferIndexes.push_back(bufIndex);
            }
        }

        err = NO_ERROR;
        while (err == NO_ERROR && bufferIndexes.size() > 0) {
            err = audioSource->read(&mbuf);

            if (err == NO_ERROR) {
                bufIndex = *bufferIndexes.begin();
                bufferIndexes.erase(bufferIndexes.begin());

                memcpy(audioEncoderInBuf.itemAt(bufIndex)->data(), mbuf->data(), mbuf->size());
                ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
                err = audioEncoder->queueInputBuffer(bufIndex, 0, mbuf->size(), ptsUsec, 0);

                if (err != NO_ERROR) {
                    fprintf(stderr, "Unable to queue PCM data to AAC encoder (err=%d)\n", err);
                }

                mbuf->release();
                mbuf = NULL;
            }
        }

        // Process video frame
        processDequeue(encoder, &trackIdx, &debugNumFrames, &buffers, muxer, false);

        // Process audio output (route AAC to muxer)
        processDequeue(audioEncoder, &audioTrackIdx, &debugNumAudioFrames, &audioBuffers, muxer, true);
    }

    ALOGV("Encoder stopping (req=%d)", gStopRequested);
    if (gVerbose) {
        printf("Encoder stopping; recorded %u frames in %lld seconds\n",
                debugNumFrames,
                nanoseconds_to_seconds(systemTime(CLOCK_MONOTONIC) - startWhenNsec));
    }
    return NO_ERROR;
}

/*
 * Main "do work" method.
 *
 * Configures codec, muxer, and virtual display, then starts moving bits
 * around.
 */
static status_t recordScreen(const char* fileName) {
    status_t err;

    // Configure signal handler.
    err = configureSignals();
    if (err != NO_ERROR) return err;

    // Start Binder thread pool.  MediaCodec needs to be able to receive
    // messages from mediaserver.
    sp<ProcessState> self = ProcessState::self();
    self->startThreadPool();

    // Get main display parameters.
    sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(
            ISurfaceComposer::eDisplayIdMain);
    DisplayInfo mainDpyInfo;
    err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to get display characteristics\n");
        return err;
    }
    if (gVerbose) {
        printf("Main display is %dx%d @%.2ffps (orientation=%u)\n",
                mainDpyInfo.w, mainDpyInfo.h, mainDpyInfo.fps,
                mainDpyInfo.orientation);
    }

    bool rotated = isDeviceRotated(mainDpyInfo.orientation);
    if (gVideoWidth == 0) {
        gVideoWidth = rotated ? mainDpyInfo.h : mainDpyInfo.w;
    }
    if (gVideoHeight == 0) {
        gVideoHeight = rotated ? mainDpyInfo.w : mainDpyInfo.h;
    }

    // Some devices cannot handle encoding tall height (> 720), so we
    // compensate by encoding in landscape and rotating
    bool autoRotated = false;
    if (gLandscapeOnly && !gSizeSpecified && gVideoHeight > gVideoWidth) {
        int newWidth = gVideoHeight;
        gVideoHeight = gVideoWidth;
        gVideoWidth = newWidth;
        gRotate = !gRotate;
        autoRotated = true;
    }

    // Configure and start the encoder.
    sp<MediaCodec> encoder;
    sp<IGraphicBufferProducer> bufferProducer;
    err = prepareEncoder(mainDpyInfo.fps, &encoder, &bufferProducer);

    if (err != NO_ERROR && !gSizeSpecified) {
        // fallback is defined for landscape; swap if we're in portrait
        bool needSwap = gVideoWidth < gVideoHeight;
        uint32_t newWidth = needSwap ? kFallbackHeight : kFallbackWidth;
        uint32_t newHeight = needSwap ? kFallbackWidth : kFallbackHeight;
        if (gVideoWidth != newWidth && gVideoHeight != newHeight) {
            ALOGV("Retrying with 720p");
            fprintf(stderr, "WARNING: failed at %dx%d, retrying at %dx%d\n",
                    gVideoWidth, gVideoHeight, newWidth, newHeight);
            gVideoWidth = newWidth;
            gVideoHeight = newHeight;
            err = prepareEncoder(mainDpyInfo.fps, &encoder, &bufferProducer);
        }
    }
    if (err != NO_ERROR) {
        return err;
    }

    // Configure virtual display.
    sp<IBinder> dpy;
    err = prepareVirtualDisplay(mainDpyInfo, bufferProducer, &dpy);
    if (err != NO_ERROR) {
        encoder->release();
        encoder.clear();

        return err;
    }

    // Configure and start the audio encoder.
    Vector< sp<ABuffer> > audioEncoderInBuf;
    sp<MediaCodec> audioEncoder;
    sp<AudioSource> audioSource;
    err = prepareAudioEncoder(&audioEncoder, &audioSource, &audioEncoderInBuf);

    // Configure, but do not start, muxer.
    sp<MediaMuxer> muxer = new MediaMuxer(fileName,
            MediaMuxer::OUTPUT_FORMAT_MPEG_4);
    if (gRotate) {
        muxer->setOrientationHint(autoRotated ? 270 : 90);
    }

    // Main encoder loop.
    err = runEncoder(encoder, audioEncoder, audioSource, audioEncoderInBuf, muxer);
    if (err != NO_ERROR) {
        encoder->release();
        encoder.clear();

        return err;
    }

    if (gVerbose) {
        printf("Stopping encoder and muxer\n");
    }

    // Shut everything down, starting with the producer side.
    bufferProducer = NULL;
    SurfaceComposerClient::destroyDisplay(dpy);

    encoder->stop();
    audioEncoder->stop();
    audioSource->stop();
    muxer->stop();
    encoder->release();
    audioEncoder->release();

    // Reset audio routing status
    setAudioSubMixRouting(false);

    return 0;
}

/*
 * Sends a broadcast to the media scanner to tell it about the new video.
 *
 * This is optional, but nice to have.
 */
static status_t notifyMediaScanner(const char* fileName) {
    pid_t pid = fork();
    if (pid < 0) {
        int err = errno;
        ALOGW("fork() failed: %s", strerror(err));
        return -err;
    } else if (pid > 0) {
        // parent; wait for the child, mostly to make the verbose-mode output
        // look right, but also to check for and log failures
        int status;
        pid_t actualPid = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
        if (actualPid != pid) {
            ALOGW("waitpid() returned %d (errno=%d)", actualPid, errno);
        } else if (status != 0) {
            ALOGW("'am broadcast' exited with status=%d", status);
        } else {
            ALOGV("'am broadcast' exited successfully");
        }
    } else {
        const char* kCommand = "/system/bin/am";

        // child; we're single-threaded, so okay to alloc
        String8 fileUrl("file://");
        fileUrl.append(fileName);
        const char* const argv[] = {
                kCommand,
                "broadcast",
                "-a",
                "android.intent.action.MEDIA_SCANNER_SCAN_FILE",
                "-d",
                fileUrl.string(),
                NULL
        };
        if (gVerbose) {
            printf("Executing:");
            for (int i = 0; argv[i] != NULL; i++) {
                printf(" %s", argv[i]);
            }
            putchar('\n');
        } else {
            // non-verbose, suppress 'am' output
            ALOGV("closing stdout/stderr in child");
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        execv(kCommand, const_cast<char* const*>(argv));
        ALOGE("execv(%s) failed: %s\n", kCommand, strerror(errno));
        exit(1);
    }
    return NO_ERROR;
}

/*
 * Parses a string of the form "1280x720".
 *
 * Returns true on success.
 */
static bool parseWidthHeight(const char* widthHeight, uint32_t* pWidth,
        uint32_t* pHeight) {
    long width, height;
    char* end;

    // Must specify base 10, or "0x0" gets parsed differently.
    width = strtol(widthHeight, &end, 10);
    if (end == widthHeight || *end != 'x' || *(end+1) == '\0') {
        // invalid chars in width, or missing 'x', or missing height
        return false;
    }
    height = strtol(end + 1, &end, 10);
    if (*end != '\0') {
        // invalid chars in height
        return false;
    }

    *pWidth = width;
    *pHeight = height;
    return true;
}

/*
 * Dumps usage on stderr.
 */
static void usage() {
    fprintf(stderr,
        "Usage: screenrecord [options] <filename>\n"
        "\n"
        "Records the device's display to a .mp4 file.\n"
        "\n"
        "Options:\n"
        "--size WIDTHxHEIGHT\n"
        "    Set the video size, e.g. \"1280x720\".  Default is the device's main\n"
        "    display resolution (if supported), 1280x720 if not.  For best results,\n"
        "    use a size supported by the AVC encoder.\n"
        "--bit-rate RATE\n"
        "    Set the video bit rate, in megabits per second.  Default %dMbps.\n"
        "--time-limit TIME\n"
        "    Set the maximum recording time, in seconds.  Default / maximum is %d.\n"
        "--rotate\n"
        "    Rotate the output 90 degrees.\n"
        "--microphone\n"
        "    Uses the microphone instead of the mix output\n"
        "--verbose\n"
        "    Display interesting information on stdout.\n"
        "--help\n"
        "    Show this message.\n"
        "\n"
        "Recording continues until Ctrl-C is hit or the time limit is reached.\n"
        "\n",
        gBitRate / 1000000, gTimeLimitSec
        );
}

/*
 * Parses args and kicks things off.
 */
int main(int argc, char* const argv[]) {
    static const struct option longOptions[] = {
        { "help",       no_argument,        NULL, 'h' },
        { "verbose",    no_argument,        NULL, 'v' },
        { "size",       required_argument,  NULL, 's' },
        { "bit-rate",   required_argument,  NULL, 'b' },
        { "time-limit", required_argument,  NULL, 't' },
        { "rotate",     no_argument,        NULL, 'r' },
        { "microphone", no_argument,        NULL, 'm' },
        { NULL,         0,                  NULL, 0 }
    };

    while (true) {
        int optionIndex = 0;
        int ic = getopt_long(argc, argv, "", longOptions, &optionIndex);
        if (ic == -1) {
            break;
        }

        switch (ic) {
        case 'h':
            usage();
            return 0;
        case 'v':
            gVerbose = true;
            break;
        case 's':
            if (!parseWidthHeight(optarg, &gVideoWidth, &gVideoHeight)) {
                fprintf(stderr, "Invalid size '%s', must be width x height\n",
                        optarg);
                return 2;
            }
            if (gVideoWidth == 0 || gVideoHeight == 0) {
                fprintf(stderr,
                    "Invalid size %ux%u, width and height may not be zero\n",
                    gVideoWidth, gVideoHeight);
                return 2;
            }
            gSizeSpecified = true;
            break;
        case 'b':
            gBitRate = atoi(optarg);
            if (gBitRate < kMinBitRate || gBitRate > kMaxBitRate) {
                fprintf(stderr,
                        "Bit rate %dbps outside acceptable range [%d,%d]\n",
                        gBitRate, kMinBitRate, kMaxBitRate);
                return 2;
            }
            break;
        case 't':
            gTimeLimitSec = atoi(optarg);
            if (gTimeLimitSec == 0 || gTimeLimitSec > kMaxTimeLimitSec) {
                fprintf(stderr,
                        "Time limit %ds outside acceptable range [1,%d]\n",
                        gTimeLimitSec, kMaxTimeLimitSec);
                return 2;
            }
            break;
        case 'r':
            gRotate = true;
            break;
        case 'm':
            gAudioInput = AUDIO_SOURCE_MIC;
            break;
        default:
            if (ic != '?') {
                fprintf(stderr, "getopt_long returned unexpected value 0x%x\n", ic);
            }
            return 2;
        }
    }

    if (optind != argc - 1) {
        fprintf(stderr, "Must specify output file (see --help).\n");
        return 2;
    }

    // MediaMuxer tries to create the file in the constructor, but we don't
    // learn about the failure until muxer.start(), which returns a generic
    // error code without logging anything.  We attempt to create the file
    // now for better diagnostics.
    const char* fileName = argv[optind];
    int fd = open(fileName, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        fprintf(stderr, "Unable to open '%s': %s\n", fileName, strerror(errno));
        return 1;
    }
    close(fd);

    status_t err = recordScreen(fileName);
    if (err == NO_ERROR) {
        // Try to notify the media scanner.  Not fatal if this fails.
        notifyMediaScanner(fileName);
    }
    ALOGD(err == NO_ERROR ? "success" : "failed");
    return (int) err;
}
