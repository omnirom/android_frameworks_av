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

#include <media/stagefright/foundation/MediaDefs.h>

namespace android {

const char *MEDIA_MIMETYPE_IMAGE_JPEG = "image/jpeg";
const char *MEDIA_MIMETYPE_IMAGE_ANDROID_HEIC = "image/vnd.android.heic";

const char *MEDIA_MIMETYPE_VIDEO_VP8 = "video/x-vnd.on2.vp8";
const char *MEDIA_MIMETYPE_VIDEO_VP9 = "video/x-vnd.on2.vp9";
const char *MEDIA_MIMETYPE_VIDEO_AVC = "video/avc";
const char *MEDIA_MIMETYPE_VIDEO_HEVC = "video/hevc";
const char *MEDIA_MIMETYPE_VIDEO_MPEG4 = "video/mp4v-es";
const char *MEDIA_MIMETYPE_VIDEO_H263 = "video/3gpp";
const char *MEDIA_MIMETYPE_VIDEO_MPEG2 = "video/mpeg2";
const char *MEDIA_MIMETYPE_VIDEO_RAW = "video/raw";
const char *MEDIA_MIMETYPE_VIDEO_DOLBY_VISION = "video/dolby-vision";
const char *MEDIA_MIMETYPE_VIDEO_SCRAMBLED = "video/scrambled";

const char *MEDIA_MIMETYPE_AUDIO_AMR_NB = "audio/3gpp";
const char *MEDIA_MIMETYPE_AUDIO_AMR_WB = "audio/amr-wb";
const char *MEDIA_MIMETYPE_AUDIO_MPEG = "audio/mpeg";
const char *MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I = "audio/mpeg-L1";
const char *MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II = "audio/mpeg-L2";
const char *MEDIA_MIMETYPE_AUDIO_MIDI = "audio/midi";
const char *MEDIA_MIMETYPE_AUDIO_AAC = "audio/mp4a-latm";
const char *MEDIA_MIMETYPE_AUDIO_QCELP = "audio/qcelp";
const char *MEDIA_MIMETYPE_AUDIO_VORBIS = "audio/vorbis";
const char *MEDIA_MIMETYPE_AUDIO_OPUS = "audio/opus";
const char *MEDIA_MIMETYPE_AUDIO_G711_ALAW = "audio/g711-alaw";
const char *MEDIA_MIMETYPE_AUDIO_G711_MLAW = "audio/g711-mlaw";
const char *MEDIA_MIMETYPE_AUDIO_RAW = "audio/raw";
const char *MEDIA_MIMETYPE_AUDIO_FLAC = "audio/flac";
const char *MEDIA_MIMETYPE_AUDIO_AAC_ADTS = "audio/aac-adts";
const char *MEDIA_MIMETYPE_AUDIO_MSGSM = "audio/gsm";
const char *MEDIA_MIMETYPE_AUDIO_AC3 = "audio/ac3";
const char *MEDIA_MIMETYPE_AUDIO_EAC3 = "audio/eac3";
const char *MEDIA_MIMETYPE_AUDIO_SCRAMBLED = "audio/scrambled";

const char *MEDIA_MIMETYPE_CONTAINER_MPEG4 = "video/mp4";
const char *MEDIA_MIMETYPE_CONTAINER_WAV = "audio/x-wav";
const char *MEDIA_MIMETYPE_CONTAINER_OGG = "application/ogg";
const char *MEDIA_MIMETYPE_CONTAINER_MATROSKA = "video/x-matroska";
const char *MEDIA_MIMETYPE_CONTAINER_MPEG2TS = "video/mp2ts";
const char *MEDIA_MIMETYPE_CONTAINER_AVI = "video/avi";
const char *MEDIA_MIMETYPE_CONTAINER_MPEG2PS = "video/mp2p";
const char *MEDIA_MIMETYPE_CONTAINER_HEIF = "image/heif";

const char *MEDIA_MIMETYPE_TEXT_3GPP = "text/3gpp-tt";
const char *MEDIA_MIMETYPE_TEXT_SUBRIP = "application/x-subrip";
const char *MEDIA_MIMETYPE_TEXT_VTT = "text/vtt";
const char *MEDIA_MIMETYPE_TEXT_CEA_608 = "text/cea-608";
const char *MEDIA_MIMETYPE_TEXT_CEA_708 = "text/cea-708";
const char *MEDIA_MIMETYPE_DATA_TIMED_ID3 = "application/x-id3v4";

#ifdef __ANDROID_VNDK_EXT__
const char *MEDIA_MIMETYPE_AUDIO_EVRC = "audio/evrc";
const char *MEDIA_MIMETYPE_VIDEO_WMV = "video/x-ms-wmv";
const char *MEDIA_MIMETYPE_VIDEO_WMV_VC1 = "video/wvc1";
const char *MEDIA_MIMETYPE_AUDIO_WMA = "audio/x-ms-wma";
const char *MEDIA_MIMETYPE_AUDIO_WMA_PRO = "audio/x-ms-wma-pro";
const char *MEDIA_MIMETYPE_AUDIO_WMA_LOSSLESS = "audio/x-ms-wma-lossless";
const char *MEDIA_MIMETYPE_CONTAINER_ASF = "video/x-ms-asf";
const char *MEDIA_MIMETYPE_VIDEO_DIVX = "video/divx";
const char *MEDIA_MIMETYPE_CONTAINER_AAC = "audio/aac";
const char *MEDIA_MIMETYPE_CONTAINER_QCP = "audio/vnd.qcelp";
const char *MEDIA_MIMETYPE_VIDEO_DIVX311 = "video/divx311";
const char *MEDIA_MIMETYPE_VIDEO_DIVX4 = "video/divx4";
const char *MEDIA_MIMETYPE_CONTAINER_MPEG2 = "video/mp2";
const char *MEDIA_MIMETYPE_CONTAINER_3G2 = "video/3g2";
const char *MEDIA_MIMETYPE_AUDIO_DTS = "audio/dts";
const char *MEDIA_MIMETYPE_AUDIO_DTS_LBR = "audio/dts-lbr";
const char *MEDIA_MIMETYPE_AUDIO_AMR_WB_PLUS = "audio/amr-wb-plus";
const char *MEDIA_MIMETYPE_AUDIO_AIFF = "audio/x-aiff";
const char *MEDIA_MIMETYPE_AUDIO_ALAC = "audio/alac";
const char *MEDIA_MIMETYPE_AUDIO_APE = "audio/x-ape";
const char *MEDIA_MIMETYPE_CONTAINER_QCAMR_NB = "audio/qc-amr";
const char *MEDIA_MIMETYPE_CONTAINER_QCAMR_WB = "audio/qc-amr-wb";
const char *MEDIA_MIMETYPE_CONTAINER_QCMPEG = "audio/qc-mpeg";
const char *MEDIA_MIMETYPE_CONTAINER_QCWAV = "audio/qc-wav";
const char *MEDIA_MIMETYPE_CONTAINER_QCMPEG2TS = "video/qc-mp2ts";
const char *MEDIA_MIMETYPE_CONTAINER_QCMPEG2PS = "video/qc-mp2ps";
const char *MEDIA_MIMETYPE_CONTAINER_QCMPEG4 = "video/qc-mp4";
const char *MEDIA_MIMETYPE_CONTAINER_QCMATROSKA = "video/qc-matroska";
const char *MEDIA_MIMETYPE_CONTAINER_QCOGG = "video/qc-ogg";
const char *MEDIA_MIMETYPE_CONTAINER_QCFLV = "video/qc-flv";
const char *MEDIA_MIMETYPE_VIDEO_VPX = "video/x-vnd.on2.vp8"; //backward compatibility
const char *MEDIA_MIMETYPE_CONTAINER_QTIFLAC = "audio/qti-flac";
const char *MEDIA_MIMETYPE_VIDEO_MPEG4_DP = "video/mp4v-esdp";
const char *MEDIA_MIMETYPE_CONTAINER_DSF = "audio/x-dsf"; // For DSF clip
const char *MEDIA_MIMETYPE_CONTAINER_DFF = "audio/x-dff"; // For DFF or DIF clip
const char *MEDIA_MIMETYPE_AUDIO_DSD = "audio/dsd";
const char *MEDIA_MIMETYPE_CONTAINER_MOV = "video/quicktime"; //mov clip
const char *MEDIA_MIMETYPE_VIDEO_TME = "video/tme";
#endif
}  // namespace android
