/*
 * Copyright (c) 2013 - 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _AV_EXTENSIONS_H_
#define _AV_EXTENSIONS_H_

#include <media/stagefright/DataSource.h>
#include <common/AVExtensionsCommon.h>
#include <system/audio.h>
#include <camera/ICamera.h>
#include <media/mediarecorder.h>
#include <media/IOMX.h>
#include "ESQueue.h"

namespace android {

class AudioParameter;
class MetaData;
class MediaExtractor;
class MPEG4Writer;
struct ABuffer;
struct ACodec;
struct ALooper;
struct IMediaHTTPConnection;
struct MediaCodec;
struct MediaHTTP;
struct NuCachedSource2;
class CameraParameters;
class MediaBuffer;
struct AudioSource;
class CameraSource;
class CameraSourceTimeLapse;
class ICamera;
class ICameraRecordingProxy;
class String16;
class IGraphicBufferProducer;
struct Size;
class MPEG4Writer;

/*
 * Factory to create objects of base-classes in libstagefright
 */
struct AVFactory {
    virtual sp<ACodec> createACodec();
    virtual MediaExtractor* createExtendedExtractor(
            const sp<DataSource> &source, const char *mime,
            const sp<AMessage> &meta, const uint32_t flags);
    virtual sp<MediaExtractor> updateExtractor(
            sp<MediaExtractor> ext, const sp<DataSource> &source,
            const char *mime, const sp<AMessage> &meta, const uint32_t flags);
    virtual sp<NuCachedSource2> createCachedSource(
            const sp<DataSource> &source,
            const char *cacheConfig = NULL,
            bool disconnectAtHighwatermark = false);
    virtual MediaHTTP* createMediaHTTP(
            const sp<IMediaHTTPConnection> &conn);
    virtual ElementaryStreamQueue* createESQueue(
            ElementaryStreamQueue::Mode mode, uint32_t flags = 0);

    virtual AudioSource* createAudioSource(
            audio_source_t inputSource,
            const String16 &opPackageName,
            uint32_t sampleRate,
            uint32_t channels,
            uint32_t outSampleRate = 0);

    virtual CameraSource *CreateCameraSourceFromCamera(
            const sp<ICamera> &camera,
            const sp<ICameraRecordingProxy> &proxy,
            int32_t cameraId,
            const String16& clientName,
            uid_t clientUid,
            Size videoSize,
            int32_t frameRate,
            const sp<IGraphicBufferProducer>& surface,
            bool storeMetaDataInVideoBuffers = true);

    virtual CameraSourceTimeLapse *CreateCameraSourceTimeLapseFromCamera(
            const sp<ICamera> &camera,
            const sp<ICameraRecordingProxy> &proxy,
            int32_t cameraId,
            const String16& clientName,
            uid_t clientUid,
            Size videoSize,
            int32_t videoFrameRate,
            const sp<IGraphicBufferProducer>& surface,
            int64_t timeBetweenFrameCaptureUs,
            bool storeMetaDataInVideoBuffers = true);

    virtual MPEG4Writer *CreateMPEG4Writer(int fd);
    // ----- NO TRESSPASSING BEYOND THIS LINE ------
    DECLARE_LOADABLE_SINGLETON(AVFactory);
};

/*
 * Common delegate to the classes in libstagefright
 */
struct AVUtils {

    virtual status_t convertMetaDataToMessage(
            const sp<MetaData> &meta, sp<AMessage> *format);
    virtual DataSource::SnifferFunc getExtendedSniffer();
    virtual status_t mapMimeToAudioFormat( audio_format_t& format, const char* mime);
    virtual status_t sendMetaDataToHal(const sp<MetaData>& meta, AudioParameter *param);

    virtual sp<MediaCodec> createCustomComponentByName(const sp<ALooper> &looper,
                const char* mime, bool encoder, const sp<AMessage> &format);
    virtual bool isEnhancedExtension(const char *extension);

    virtual bool is24bitPCMOffloadEnabled();
    virtual bool is16bitPCMOffloadEnabled();
    virtual int getAudioSampleBits(const sp<MetaData> &);
    virtual int getAudioSampleBits(const sp<AMessage> &);
    virtual void setPcmSampleBits(const sp<MetaData> &, int32_t /*bitWidth*/);
    virtual void setPcmSampleBits(const sp<AMessage> &, int32_t /*bitWidth*/);

    virtual audio_format_t updateAudioFormat(audio_format_t audioFormat,
            const sp<MetaData> &);

    virtual audio_format_t updateAudioFormat(audio_format_t audioFormat,
            const sp<AMessage> &);

    virtual bool canOffloadAPE(const sp<MetaData> &meta);

    virtual int32_t getAudioMaxInputBufferSize(audio_format_t audioFormat,
            const sp<AMessage> &);

    virtual bool mapAACProfileToAudioFormat(const sp<MetaData> &,
            audio_format_t &,
            uint64_t /*eAacProfile*/);

    virtual bool mapAACProfileToAudioFormat(const sp<AMessage> &,
            audio_format_t &,
            uint64_t /*eAacProfile*/);

    virtual void extractCustomCameraKeys(
            const CameraParameters& /*params*/, sp<MetaData> &/*meta*/) {}
    virtual void printFileName(int /*fd*/) {}
    virtual void addDecodingTimesFromBatch(MediaBuffer * /*buf*/,
            List<int64_t> &/*decodeTimeQueue*/) {}

    virtual bool useQCHWEncoder(const sp<AMessage> &, AString &) { return false; }

    virtual bool canDeferRelease(const sp<MetaData> &/*meta*/) { return false; }
    virtual void setDeferRelease(sp<MetaData> &/*meta*/) {}

    struct HEVCMuxer {

        virtual bool reassembleHEVCCSD(const AString &mime, sp<ABuffer> csd0, sp<MetaData> &meta);

        virtual void writeHEVCFtypBox(MPEG4Writer *writer);

        virtual status_t makeHEVCCodecSpecificData(const uint8_t *data,
                  size_t size, void** codecSpecificData,
                  size_t *codecSpecificDataSize);

        virtual const char *getFourCCForMime(const char *mime);

        virtual void writeHvccBox(MPEG4Writer *writer,
                  void* codecSpecificData, size_t codecSpecificDataSize,
                  bool useNalLengthFour);

        virtual bool isVideoHEVC(const char* mime);

        virtual void getHEVCCodecSpecificDataFromInputFormatIfPossible(
                  sp<MetaData> meta, void **codecSpecificData,
                  size_t *codecSpecificDataSize, bool *gotAllCodecSpecificData);

    protected:
        HEVCMuxer() {};
        virtual ~HEVCMuxer() {};
        friend struct AVUtils;
    };

    virtual inline HEVCMuxer& HEVCMuxerUtils() {
         return mHEVCMuxer;
    }

    virtual bool isAudioMuxFormatSupported(const char *mime);
    virtual void cacheCaptureBuffers(sp<ICamera> camera, video_encoder encoder);
    virtual const char *getCustomCodecsLocation();
    virtual const char *getCustomCodecsPerformanceLocation();

    virtual void setIntraPeriod(
                int nPFrames, int nBFrames, const sp<IOMX> OMXhandle,
                IOMX::node_id nodeID);

    // Used by ATSParser
    virtual bool IsHevcIDR(const sp<ABuffer> &accessUnit);

private:
    HEVCMuxer mHEVCMuxer;
    // ----- NO TRESSPASSING BEYOND THIS LINE ------
    DECLARE_LOADABLE_SINGLETON(AVUtils);
};

}

#endif // _AV_EXTENSIONS__H_
