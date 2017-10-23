/*
 * Copyright (c) 2013 - 2017, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "AVUtils"
#include <utils/Log.h>

#include <media/IMediaExtractor.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/MetaData.h>
#include <gui/Surface.h>
#include <media/stagefright/ACodec.h>
#include <media/stagefright/MediaCodec.h>

#include "common/ExtensionsLoader.hpp"
#include "stagefright/AVExtensions.h"
#include "omx/OMXUtils.h"


namespace android {

status_t AVUtils::convertMetaDataToMessage(
        const sp<MetaData> &, sp<AMessage> *) {
    return OK;
}

status_t AVUtils::convertMessageToMetaData(
        const sp<AMessage> &, sp<MetaData> &) {
    return OK;
}

status_t AVUtils::mapMimeToAudioFormat(
        audio_format_t&, const char* ) {
    return OK;
}

status_t AVUtils::sendMetaDataToHal(
        const sp<MetaData>&, AudioParameter *){
    return OK;
}

bool AVUtils::hasAudioSampleBits(const sp<MetaData> &) {
    return false;
}

bool AVUtils::hasAudioSampleBits(const sp<AMessage> &) {
    return false;
}

int AVUtils::getAudioSampleBits(const sp<MetaData> &) {
    return 16;
}

int AVUtils::getAudioSampleBits(const sp<AMessage> &) {
    return 16;
}

audio_format_t AVUtils::updateAudioFormat(audio_format_t audioFormat,
        const sp<MetaData> &){
    return audioFormat;
}

audio_format_t AVUtils::updateAudioFormat(audio_format_t audioFormat,
        const sp<AMessage> &){
    return audioFormat;
}

static bool dumbSniffer(
        const sp<DataSource> &, String8 *,
        float *, sp<AMessage> *) {
    return false;
}

MediaExtractor::SnifferFunc AVUtils::getExtendedSniffer() {
    return dumbSniffer;
}

sp<MediaCodec> AVUtils::createCustomComponentByName(
           const sp<ALooper> &, const char* , bool, const sp<AMessage> &) {
               return NULL;
}

int32_t AVUtils::getAudioMaxInputBufferSize(audio_format_t, const sp<AMessage> &) {
    return 0;
}

bool AVUtils::mapAACProfileToAudioFormat(const sp<MetaData> &, audio_format_t &,
                 uint64_t  /*eAacProfile*/) {
    return false ;
}

bool AVUtils::mapAACProfileToAudioFormat(const sp<AMessage> &,  audio_format_t &,
                 uint64_t  /*eAacProfile*/) {
    return false ;
}

bool AVUtils::canOffloadAPE(const sp<MetaData> &) {
   return true;
}

bool AVUtils::isEnhancedExtension(const char *) {
    return false;
}

bool AVUtils::isAudioMuxFormatSupported(const char *) {
    return true;
}

void AVUtils::cacheCaptureBuffers(sp<hardware::ICamera>, video_encoder) {
    return;
}

void AVUtils::getHFRParams(bool*, int32_t*, sp<AMessage>) {
    return;
}

int64_t AVUtils::overwriteTimeOffset(bool, int64_t inputBufferTimeOffsetUs, int64_t*,
           int64_t, int32_t) {
    return inputBufferTimeOffsetUs;
}

void AVUtils::getCustomCodecsLocation(char *, int32_t) {
    return;
}

void AVUtils::setIntraPeriod(
        int, int, sp<IOMXNode>) {
    return;
}

void AVUtils::getCustomCodecsPerformanceLocation(char *, int32_t) {
    return;
}

const char *AVUtils::getComponentRole(bool isEncoder, const char *mime) {
    return GetComponentRole(isEncoder,mime);
}

bool AVUtils::IsHevcIDR(const sp<ABuffer> &) {
   return false;
}

sp<DataSource> AVUtils::wrapTraceDataSource(const sp<DataSource> &dataSource) {
    return dataSource;
}

sp<IMediaExtractor> AVUtils::wrapTraceMediaExtractor(const sp<IMediaExtractor> &extractor) {
    return extractor;
}

sp<AMessage> AVUtils::fillExtradata(sp<MediaCodecBuffer>&, sp<AMessage> &format) {
    return format;
}

// ----- NO TRESSPASSING BEYOND THIS LINE ------
AVUtils::AVUtils() {}

AVUtils::~AVUtils() {}

//static
AVUtils *AVUtils::sInst =
        ExtensionsLoader<AVUtils>::createInstance("createExtendedUtils");

} //namespace android

