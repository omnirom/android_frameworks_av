/*
**
** Copyright 2010, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


//#define LOG_NDEBUG 0
#define LOG_TAG "MediaProfiles"

#include <stdlib.h>
#include <utils/misc.h>
#include <utils/Log.h>
#include <utils/Vector.h>
#include <cutils/properties.h>
#include <expat.h>
#include <media/MediaProfiles.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <OMX_Video.h>
#include <sys/stat.h>

#include <array>
#include <string>
#include <vector>

namespace android {

namespace /* unnamed */ {

// Returns a list of possible paths for the media_profiles XML file.
std::array<char const*, 5> const& getXmlPaths() {
    static std::array<std::string const, 5> const paths =
        []() -> decltype(paths) {
            // Directories for XML file that will be searched (in this order).
            constexpr std::array<char const*, 4> searchDirs = {
                "product/etc/",
                "odm/etc/",
                "vendor/etc/",
                "system/etc/",
            };

            // The file name may contain a variant if the vendor property
            // ro.vendor.media_profiles_xml_variant is set.
            char variant[PROPERTY_VALUE_MAX];
            property_get("ro.media.xml_variant.profiles",
                         variant,
                         "_V1_0");

            std::string fileName =
                std::string("media_profiles") + variant + ".xml";

            return { searchDirs[0] + fileName,
                     searchDirs[1] + fileName,
                     searchDirs[2] + fileName,
                     searchDirs[3] + fileName,
                     "system/etc/media_profiles.xml" // System fallback
                   };
        }();
    static std::array<char const*, 5> const cPaths = {
            paths[0].data(),
            paths[1].data(),
            paths[2].data(),
            paths[3].data(),
            paths[4].data()
        };
    return cPaths;
}

} // unnamed namespace

Mutex MediaProfiles::sLock;
bool MediaProfiles::sIsInitialized = false;
MediaProfiles *MediaProfiles::sInstance = NULL;

const MediaProfiles::NameToTagMap MediaProfiles::sVideoEncoderNameMap[] = {
    {"h263", VIDEO_ENCODER_H263},
    {"h264", VIDEO_ENCODER_H264},
    {"m4v",  VIDEO_ENCODER_MPEG_4_SP},
    {"vp8",  VIDEO_ENCODER_VP8},
    {"hevc", VIDEO_ENCODER_HEVC},
    {"vp9",  VIDEO_ENCODER_VP9},
    {"dolbyvision", VIDEO_ENCODER_DOLBY_VISION},
};

const MediaProfiles::NameToTagMap MediaProfiles::sChromaSubsamplingNameMap[] = {
    {"yuv 4:2:0", CHROMA_SUBSAMPLING_YUV_420},
    {"yuv 4:2:2", CHROMA_SUBSAMPLING_YUV_422},
    {"yuv 4:4:4", CHROMA_SUBSAMPLING_YUV_444},
};

const MediaProfiles::NameToTagMap MediaProfiles::sHdrFormatNameMap[] = {
    {"sdr", HDR_FORMAT_NONE},
    {"hlg", HDR_FORMAT_HLG},
    {"hdr10", HDR_FORMAT_HDR10},
    {"hdr10+", HDR_FORMAT_HDR10PLUS},
    {"dolbyvision", HDR_FORMAT_DOLBY_VISION},
};

const MediaProfiles::NameToTagMap MediaProfiles::sAudioEncoderNameMap[] = {
    {"amrnb",  AUDIO_ENCODER_AMR_NB},
    {"amrwb",  AUDIO_ENCODER_AMR_WB},
    {"aac",    AUDIO_ENCODER_AAC},
    {"heaac",  AUDIO_ENCODER_HE_AAC},
    {"aaceld", AUDIO_ENCODER_AAC_ELD},
    {"opus",   AUDIO_ENCODER_OPUS}
};

const MediaProfiles::NameToTagMap MediaProfiles::sFileFormatMap[] = {
    {"3gp", OUTPUT_FORMAT_THREE_GPP},
    {"mp4", OUTPUT_FORMAT_MPEG_4}
};

const MediaProfiles::NameToTagMap MediaProfiles::sVideoDecoderNameMap[] = {
    {"wmv", VIDEO_DECODER_WMV}
};

const MediaProfiles::NameToTagMap MediaProfiles::sAudioDecoderNameMap[] = {
    {"wma", AUDIO_DECODER_WMA}
};

const MediaProfiles::NameToTagMap MediaProfiles::sCamcorderQualityNameMap[] = {
    {"low", CAMCORDER_QUALITY_LOW},
    {"high", CAMCORDER_QUALITY_HIGH},
    {"qcif", CAMCORDER_QUALITY_QCIF},
    {"cif", CAMCORDER_QUALITY_CIF},
    {"480p", CAMCORDER_QUALITY_480P},
    {"720p", CAMCORDER_QUALITY_720P},
    {"1080p", CAMCORDER_QUALITY_1080P},
    {"2160p", CAMCORDER_QUALITY_2160P},
    {"qvga", CAMCORDER_QUALITY_QVGA},
    {"vga", CAMCORDER_QUALITY_VGA},
    {"4kdci", CAMCORDER_QUALITY_4KDCI},
    {"qhd", CAMCORDER_QUALITY_QHD},
    {"2k", CAMCORDER_QUALITY_2K},
    {"8kuhd", CAMCORDER_QUALITY_8KUHD},

    {"timelapselow",  CAMCORDER_QUALITY_TIME_LAPSE_LOW},
    {"timelapsehigh", CAMCORDER_QUALITY_TIME_LAPSE_HIGH},
    {"timelapseqcif", CAMCORDER_QUALITY_TIME_LAPSE_QCIF},
    {"timelapsecif", CAMCORDER_QUALITY_TIME_LAPSE_CIF},
    {"timelapse480p", CAMCORDER_QUALITY_TIME_LAPSE_480P},
    {"timelapse720p", CAMCORDER_QUALITY_TIME_LAPSE_720P},
    {"timelapse1080p", CAMCORDER_QUALITY_TIME_LAPSE_1080P},
    {"timelapse2160p", CAMCORDER_QUALITY_TIME_LAPSE_2160P},
    {"timelapseqvga", CAMCORDER_QUALITY_TIME_LAPSE_QVGA},
    {"timelapsevga", CAMCORDER_QUALITY_TIME_LAPSE_VGA},
    {"timelapse4kdci", CAMCORDER_QUALITY_TIME_LAPSE_4KDCI},
    {"timelapseqhd", CAMCORDER_QUALITY_TIME_LAPSE_QHD},
    {"timelapse2k", CAMCORDER_QUALITY_TIME_LAPSE_2K},
    {"timelapse8kuhd", CAMCORDER_QUALITY_TIME_LAPSE_8KUHD},

    {"highspeedlow",  CAMCORDER_QUALITY_HIGH_SPEED_LOW},
    {"highspeedhigh", CAMCORDER_QUALITY_HIGH_SPEED_HIGH},
    {"highspeed480p", CAMCORDER_QUALITY_HIGH_SPEED_480P},
    {"highspeed720p", CAMCORDER_QUALITY_HIGH_SPEED_720P},
    {"highspeed1080p", CAMCORDER_QUALITY_HIGH_SPEED_1080P},
    {"highspeed2160p", CAMCORDER_QUALITY_HIGH_SPEED_2160P},
    {"highspeedcif", CAMCORDER_QUALITY_HIGH_SPEED_CIF},
    {"highspeedvga", CAMCORDER_QUALITY_HIGH_SPEED_VGA},
    {"highspeed4kdci", CAMCORDER_QUALITY_HIGH_SPEED_4KDCI},

    // Vendor-specific profiles
};

#if LOG_NDEBUG
#define UNUSED __unused
#else
#define UNUSED
#endif

/*static*/ void
MediaProfiles::logVideoCodec(const MediaProfiles::VideoCodec& codec UNUSED)
{
    ALOGV("video codec:");
    ALOGV("codec = %d (%s)", codec.mCodec,
            findNameForTag(sVideoEncoderNameMap, NELEM(sVideoEncoderNameMap), codec.mCodec));
    ALOGV("bit rate: %d", codec.mBitRate);
    ALOGV("frame width: %d", codec.mFrameWidth);
    ALOGV("frame height: %d", codec.mFrameHeight);
    ALOGV("frame rate: %d", codec.mFrameRate);
    ALOGV("profile: %d", codec.mProfile);
    ALOGV("chroma: %s", findNameForTag(sChromaSubsamplingNameMap, NELEM(sChromaSubsamplingNameMap),
                                       codec.mChromaSubsampling));
    ALOGV("bit depth: %d", codec.mBitDepth);
    ALOGV("hdr format: %s", findNameForTag(sHdrFormatNameMap, NELEM(sHdrFormatNameMap),
                                           codec.mHdrFormat));
}

/*static*/ void
MediaProfiles::logAudioCodec(const MediaProfiles::AudioCodec& codec UNUSED)
{
    ALOGV("audio codec:");
    ALOGV("codec = %d", codec.mCodec);
    ALOGV("bit rate: %d", codec.mBitRate);
    ALOGV("sample rate: %d", codec.mSampleRate);
    ALOGV("number of channels: %d", codec.mChannels);
    ALOGV("profile: %d", codec.mProfile);
}

/*static*/ void
MediaProfiles::logVideoEncoderCap(const MediaProfiles::VideoEncoderCap& cap UNUSED)
{
    ALOGV("video encoder cap:");
    ALOGV("codec = %d", cap.mCodec);
    ALOGV("bit rate: min = %d and max = %d", cap.mMinBitRate, cap.mMaxBitRate);
    ALOGV("frame width: min = %d and max = %d", cap.mMinFrameWidth, cap.mMaxFrameWidth);
    ALOGV("frame height: min = %d and max = %d", cap.mMinFrameHeight, cap.mMaxFrameHeight);
    ALOGV("frame rate: min = %d and max = %d", cap.mMinFrameRate, cap.mMaxFrameRate);
}

/*static*/ void
MediaProfiles::logAudioEncoderCap(const MediaProfiles::AudioEncoderCap& cap UNUSED)
{
    ALOGV("audio encoder cap:");
    ALOGV("codec = %d", cap.mCodec);
    ALOGV("bit rate: min = %d and max = %d", cap.mMinBitRate, cap.mMaxBitRate);
    ALOGV("sample rate: min = %d and max = %d", cap.mMinSampleRate, cap.mMaxSampleRate);
    ALOGV("number of channels: min = %d and max = %d", cap.mMinChannels, cap.mMaxChannels);
}

/*static*/ void
MediaProfiles::logVideoDecoderCap(const MediaProfiles::VideoDecoderCap& cap UNUSED)
{
    ALOGV("video decoder cap:");
    ALOGV("codec = %d", cap.mCodec);
}

/*static*/ void
MediaProfiles::logAudioDecoderCap(const MediaProfiles::AudioDecoderCap& cap UNUSED)
{
    ALOGV("audio codec cap:");
    ALOGV("codec = %d", cap.mCodec);
}

/*static*/ int
MediaProfiles::findTagForName(const MediaProfiles::NameToTagMap *map, size_t nMappings,
        const char *name)
{
    int tag = -1;
    for (size_t i = 0; i < nMappings; ++i) {
        if (!strcmp(map[i].name, name)) {
            tag = map[i].tag;
            break;
        }
    }
    return tag;
}

/*static*/ const char *
MediaProfiles::findNameForTag(
        const MediaProfiles::NameToTagMap *map, size_t nMappings, int tag, const char *def_)
{
    for (size_t i = 0; i < nMappings; ++i) {
        if (map[i].tag == tag) {
            return map[i].name;
        }
    }
    return def_;
}

/*static*/ bool
MediaProfiles::detectAdvancedVideoProfile(
        video_encoder codec, int profile,
        chroma_subsampling *chroma, int *bitDepth, hdr_format *hdr)
{
    // default values
    *chroma = CHROMA_SUBSAMPLING_YUV_420;
    *bitDepth = 8;
    *hdr = HDR_FORMAT_NONE;

    switch (codec) {
    case VIDEO_ENCODER_H263:
    case VIDEO_ENCODER_MPEG_4_SP:
    case VIDEO_ENCODER_VP8:
        // these are always 4:2:0 SDR 8-bit
        return true;

    case VIDEO_ENCODER_H264:
        switch (profile) {
        case AVCProfileBaseline:
        case AVCProfileConstrainedBaseline:
        case AVCProfileMain:
        case AVCProfileExtended:
        case AVCProfileHigh:
        case AVCProfileConstrainedHigh:
            return true;
        case AVCProfileHigh10:
            // returning false here as this could be an HLG stream
            *bitDepth = 10;
            return false;
        case AVCProfileHigh422:
            *chroma = CHROMA_SUBSAMPLING_YUV_422;
            // returning false here as bit-depth could be 8 or 10
            return false;
        case AVCProfileHigh444:
            *chroma = CHROMA_SUBSAMPLING_YUV_444;
            // returning false here as bit-depth could be 8 or 10
            return false;
        default:
            return false;
        }
        // flow does not get here

    case VIDEO_ENCODER_HEVC:
        switch (profile) {
        case HEVCProfileMain:
            return true;
        case HEVCProfileMain10:
            *bitDepth = 10;
            // returning false here as this could be an HLG stream
            return false;
        case HEVCProfileMain10HDR10:
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10;
            return true;
        case HEVCProfileMain10HDR10Plus:
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10PLUS;
            return true;
        default:
            return false;
        }
        // flow does not get here

    case VIDEO_ENCODER_VP9:
        switch (profile) {
        case VP9Profile0:
            return true;
        case VP9Profile2:
            // this is always 10-bit on Android */
            *bitDepth = 10;
            // returning false here as this could be an HLG stream
            return false;
        case VP9Profile2HDR:
            // this is always 10-bit on Android */
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10;
            return true;
        case VP9Profile2HDR10Plus:
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10PLUS;
            return true;
        default:
            return false;
        }
        // flow does not get here

    case VIDEO_ENCODER_DOLBY_VISION:
    {
        // for Dolby Vision codec we always assume 10-bit DV
        *bitDepth = 10;
        *hdr = HDR_FORMAT_DOLBY_VISION;

        switch (profile) {
        case DolbyVisionProfileDvheDer /* profile 2 deprecated */:
        case DolbyVisionProfileDvheDen /* profile 3 deprecated */:
        case DolbyVisionProfileDvavPer /* profile 0 deprecated */:
        case DolbyVisionProfileDvavPen /* profile 1 deprecated */:
        case DolbyVisionProfileDvheDtr /* dvhe.04 */:
        case DolbyVisionProfileDvheStn /* dvhe.05 */:
        case DolbyVisionProfileDvheDth /* profile 6 deprecated */:
        case DolbyVisionProfileDvheDtb /* dvhe.07 */:
        case DolbyVisionProfileDvheSt  /* dvhe.08 */:
        case DolbyVisionProfileDvavSe  /* dvav.09 */:
        case DolbyVisionProfileDvav110 /* dvav1.10 */:
            return true;
        default:
            return false;
        }
        // flow does not get here
    }

    case VIDEO_ENCODER_AV1:
        switch (profile) {
        case AV1ProfileMain10:
            *bitDepth = 10;
            // returning false here as this could be an HLG stream
            return false;
        case AV1ProfileMain10HDR10:
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10;
            return true;
        case AV1ProfileMain10HDR10Plus:
            *bitDepth = 10;
            *hdr = HDR_FORMAT_HDR10PLUS;
            return true;
        default:
            return false;
        }
        // flow does not get here

    default:
        return false;
    }
    // flow does not get here
}

/*static*/ void
MediaProfiles::createVideoCodec(const char **atts, size_t natts, MediaProfiles *profiles)
{
    CHECK(natts >= 10 &&
          !strcmp("codec",     atts[0]) &&
          !strcmp("bitRate",   atts[2]) &&
          !strcmp("width",     atts[4]) &&
          !strcmp("height",    atts[6]) &&
          !strcmp("frameRate", atts[8]));

    const size_t nMappings = sizeof(sVideoEncoderNameMap)/sizeof(sVideoEncoderNameMap[0]);
    const int codec = findTagForName(sVideoEncoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
        ALOGE("MediaProfiles::createVideoCodec failed to locate codec %s", atts[1]);
        return;
    }

    int profile = -1;
    chroma_subsampling chroma = CHROMA_SUBSAMPLING_YUV_420;
    int bitDepth = 8;
    hdr_format hdr = HDR_FORMAT_NONE;
    if (codec == VIDEO_ENCODER_DOLBY_VISION) {
        bitDepth = 10;
        hdr = HDR_FORMAT_DOLBY_VISION;
    }

    if (natts >= 12 && !strcmp("profile", atts[10])) {
        profile = atoi(atts[11]);
        if (!detectAdvancedVideoProfile(
                (video_encoder)codec, profile, &chroma, &bitDepth, &hdr)) {
            // if not detected read values from the attributes
            for (size_t ix = 12; natts >= ix + 2; ix += 2) {
                if (!strcmp("chroma", atts[ix])) {
                    int chromaTag = findTagForName(sChromaSubsamplingNameMap,
                                         NELEM(sChromaSubsamplingNameMap), atts[ix + 1]);
                    if (chromaTag == -1) {
                        ALOGE("MediaProfiles::createVideoCodec invalid chroma %s", atts[ix + 1]);
                        return;
                    } else {
                        chroma = (chroma_subsampling)chromaTag;
                    }
                } else if (!strcmp("bitDepth", atts[ix])) {
                    bitDepth = atoi(atts[ix + 1]);
                    if (bitDepth < 8 || bitDepth > 16) {
                        ALOGE("MediaProfiles::createVideoCodec invalid bidDepth %s", atts[ix + 1]);
                        return;
                    }
                } else if (!strcmp("hdr", atts[ix])) {
                    int hdrTag = findTagForName(sHdrFormatNameMap,
                                                NELEM(sHdrFormatNameMap), atts[ix + 1]);
                    if (hdrTag == -1) {
                        ALOGE("MediaProfiles::createVideoCodec invalid hdr %s", atts[ix + 1]);
                        return;
                    } else {
                        hdr = (hdr_format)hdrTag;
                    }
                } else {
                    // ignoring here. TODO: rewrite this whole file to ignore invalid attrs
                    ALOGD("MediaProfiles::createVideoCodec ignoring invalid attr %s", atts[ix]);
                }
            }
        }
    }

    VideoCodec videoCodec{
            static_cast<video_encoder>(codec),
            atoi(atts[3]) /* bitRate */, atoi(atts[5]) /* width */, atoi(atts[7]) /* height */,
            atoi(atts[9]) /* frameRate */, profile, chroma, bitDepth, hdr };
    logVideoCodec(videoCodec);

    size_t nCamcorderProfiles;
    CHECK((nCamcorderProfiles = profiles->mCamcorderProfiles.size()) >= 1);
    profiles->mCamcorderProfiles[nCamcorderProfiles - 1]->mVideoCodecs.emplace_back(videoCodec);
}

/*static*/ void
MediaProfiles::createAudioCodec(const char **atts, size_t natts, MediaProfiles *profiles)
{
    CHECK(natts >= 8 &&
          !strcmp("codec",      atts[0]) &&
          !strcmp("bitRate",    atts[2]) &&
          !strcmp("sampleRate", atts[4]) &&
          !strcmp("channels",   atts[6]));
    const size_t nMappings = sizeof(sAudioEncoderNameMap)/sizeof(sAudioEncoderNameMap[0]);
    const int codec = findTagForName(sAudioEncoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
        ALOGE("MediaProfiles::createAudioCodec failed to locate codec %s", atts[1]);
        return;
    }

    int profile = -1;
    if (natts >= 10 && !strcmp("profile", atts[8])) {
        profile = atoi(atts[9]);
    }

    AudioCodec audioCodec{
            static_cast<audio_encoder>(codec),
            atoi(atts[3]), atoi(atts[5]), atoi(atts[7]), profile };
    logAudioCodec(audioCodec);

    size_t nCamcorderProfiles;
    CHECK((nCamcorderProfiles = profiles->mCamcorderProfiles.size()) >= 1);
    profiles->mCamcorderProfiles[nCamcorderProfiles - 1]->mAudioCodecs.emplace_back(audioCodec);
}

/*static*/ MediaProfiles::AudioDecoderCap*
MediaProfiles::createAudioDecoderCap(const char **atts, size_t natts)
{
    CHECK(natts >= 4 &&
          !strcmp("name",    atts[0]) &&
          !strcmp("enabled", atts[2]));

    const size_t nMappings = sizeof(sAudioDecoderNameMap)/sizeof(sAudioDecoderNameMap[0]);
    const int codec = findTagForName(sAudioDecoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
      ALOGE("MediaProfiles::createAudioDecoderCap failed to locate codec %s", atts[1]);
      return nullptr;
    }

    MediaProfiles::AudioDecoderCap *cap =
        new MediaProfiles::AudioDecoderCap(static_cast<audio_decoder>(codec));
    logAudioDecoderCap(*cap);
    return cap;
}

/*static*/ MediaProfiles::VideoDecoderCap*
MediaProfiles::createVideoDecoderCap(const char **atts, size_t natts)
{
    CHECK(natts >= 4 &&
          !strcmp("name",    atts[0]) &&
          !strcmp("enabled", atts[2]));

    const size_t nMappings = sizeof(sVideoDecoderNameMap)/sizeof(sVideoDecoderNameMap[0]);
    const int codec = findTagForName(sVideoDecoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
      ALOGE("MediaProfiles::createVideoDecoderCap failed to locate codec %s", atts[1]);
      return nullptr;
    }

    MediaProfiles::VideoDecoderCap *cap =
        new MediaProfiles::VideoDecoderCap(static_cast<video_decoder>(codec));
    logVideoDecoderCap(*cap);
    return cap;
}

/*static*/ MediaProfiles::VideoEncoderCap*
MediaProfiles::createVideoEncoderCap(const char **atts, size_t natts)
{
    CHECK(natts >= 20 &&
          !strcmp("name",           atts[0])  &&
          !strcmp("enabled",        atts[2])  &&
          !strcmp("minBitRate",     atts[4])  &&
          !strcmp("maxBitRate",     atts[6])  &&
          !strcmp("minFrameWidth",  atts[8])  &&
          !strcmp("maxFrameWidth",  atts[10]) &&
          !strcmp("minFrameHeight", atts[12]) &&
          !strcmp("maxFrameHeight", atts[14]) &&
          !strcmp("minFrameRate",   atts[16]) &&
          !strcmp("maxFrameRate",   atts[18]));

    const size_t nMappings = sizeof(sVideoEncoderNameMap)/sizeof(sVideoEncoderNameMap[0]);
    const int codec = findTagForName(sVideoEncoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
      ALOGE("MediaProfiles::createVideoEncoderCap failed to locate codec %s", atts[1]);
      return nullptr;
    }

    MediaProfiles::VideoEncoderCap *cap =
        new MediaProfiles::VideoEncoderCap(static_cast<video_encoder>(codec),
            atoi(atts[5]), atoi(atts[7]), atoi(atts[9]), atoi(atts[11]), atoi(atts[13]),
            atoi(atts[15]), atoi(atts[17]), atoi(atts[19]));
    logVideoEncoderCap(*cap);
    return cap;
}

/*static*/ MediaProfiles::AudioEncoderCap*
MediaProfiles::createAudioEncoderCap(const char **atts, size_t natts)
{
    CHECK(natts >= 16 &&
          !strcmp("name",          atts[0])  &&
          !strcmp("enabled",       atts[2])  &&
          !strcmp("minBitRate",    atts[4])  &&
          !strcmp("maxBitRate",    atts[6])  &&
          !strcmp("minSampleRate", atts[8])  &&
          !strcmp("maxSampleRate", atts[10]) &&
          !strcmp("minChannels",   atts[12]) &&
          !strcmp("maxChannels",   atts[14]));

    const size_t nMappings = sizeof(sAudioEncoderNameMap)/sizeof(sAudioEncoderNameMap[0]);
    const int codec = findTagForName(sAudioEncoderNameMap, nMappings, atts[1]);
    if (codec == -1) {
      ALOGE("MediaProfiles::createAudioEncoderCap failed to locate codec %s", atts[1]);
      return nullptr;
    }

    MediaProfiles::AudioEncoderCap *cap =
        new MediaProfiles::AudioEncoderCap(static_cast<audio_encoder>(codec), atoi(atts[5]),
            atoi(atts[7]), atoi(atts[9]), atoi(atts[11]), atoi(atts[13]), atoi(atts[15]));
    logAudioEncoderCap(*cap);
    return cap;
}

/*static*/ output_format
MediaProfiles::createEncoderOutputFileFormat(const char **atts, size_t natts)
{
    CHECK(natts >= 2 &&
          !strcmp("name", atts[0]));

    const size_t nMappings =sizeof(sFileFormatMap)/sizeof(sFileFormatMap[0]);
    const int format = findTagForName(sFileFormatMap, nMappings, atts[1]);
    CHECK(format != -1);

    return static_cast<output_format>(format);
}

static bool isCameraIdFound(int cameraId, const Vector<int>& cameraIds) {
    for (int i = 0, n = cameraIds.size(); i < n; ++i) {
        if (cameraId == cameraIds[i]) {
            return true;
        }
    }
    return false;
}

/*static*/ MediaProfiles::CamcorderProfile*
MediaProfiles::createCamcorderProfile(
        int cameraId, const char **atts, size_t natts, Vector<int>& cameraIds)
{
    CHECK(natts >= 6 &&
          !strcmp("quality",    atts[0]) &&
          !strcmp("fileFormat", atts[2]) &&
          !strcmp("duration",   atts[4]));

    const size_t nProfileMappings = sizeof(sCamcorderQualityNameMap)/
            sizeof(sCamcorderQualityNameMap[0]);
    const int quality = findTagForName(sCamcorderQualityNameMap, nProfileMappings, atts[1]);
    if (quality == -1) {
      ALOGE("MediaProfiles::createCamcorderProfile failed to locate quality %s", atts[1]);
      return nullptr;
    }

    const size_t nFormatMappings = sizeof(sFileFormatMap)/sizeof(sFileFormatMap[0]);
    const int fileFormat = findTagForName(sFileFormatMap, nFormatMappings, atts[3]);
    if (fileFormat == -1) {
      ALOGE("MediaProfiles::createCamcorderProfile failed to locate file format %s", atts[1]);
      return nullptr;
    }

    MediaProfiles::CamcorderProfile *profile = new MediaProfiles::CamcorderProfile;
    profile->mCameraId = cameraId;
    if (!isCameraIdFound(cameraId, cameraIds)) {
        cameraIds.add(cameraId);
    }
    profile->mFileFormat = static_cast<output_format>(fileFormat);
    profile->mQuality = static_cast<camcorder_quality>(quality);
    profile->mDuration = atoi(atts[5]);
    return profile;
}

MediaProfiles::ImageEncodingQualityLevels*
MediaProfiles::findImageEncodingQualityLevels(int cameraId) const
{
    int n = mImageEncodingQualityLevels.size();
    for (int i = 0; i < n; i++) {
        ImageEncodingQualityLevels *levels = mImageEncodingQualityLevels[i];
        if (levels->mCameraId == cameraId) {
            return levels;
        }
    }
    return NULL;
}

void MediaProfiles::addImageEncodingQualityLevel(int cameraId, const char** atts, size_t natts)
{
    CHECK(natts >= 2 &&
          !strcmp("quality", atts[0]));
    int quality = atoi(atts[1]);
    ALOGV("%s: cameraId=%d, quality=%d", __func__, cameraId, quality);
    ImageEncodingQualityLevels *levels = findImageEncodingQualityLevels(cameraId);

    if (levels == NULL) {
        levels = new ImageEncodingQualityLevels();
        levels->mCameraId = cameraId;
        mImageEncodingQualityLevels.add(levels);
    }

    levels->mLevels.add(quality);
}

/*static*/ int
MediaProfiles::getCameraId(const char** atts, size_t natts)
{
    if (!atts[0]) return 0;  // default cameraId = 0
    CHECK(natts >= 2 &&
          !strcmp("cameraId", atts[0]));
    return atoi(atts[1]);
}

void MediaProfiles::addStartTimeOffset(int cameraId, const char** atts, size_t natts)
{
    int offsetTimeMs = 1000;
    if (natts >= 3 && atts[2]) {
        CHECK(natts >= 4 && !strcmp("startOffsetMs", atts[2]));
        offsetTimeMs = atoi(atts[3]);
    }

    ALOGV("%s: cameraId=%d, offset=%d ms", __func__, cameraId, offsetTimeMs);
    mStartTimeOffsets.replaceValueFor(cameraId, offsetTimeMs);
}

/*static*/ void
MediaProfiles::startElementHandler(void *userData, const char *name, const char **atts)
{
    // determine number of attributes
    size_t natts = 0;
    while (atts[natts]) {
        ++natts;
    }

    MediaProfiles *profiles = (MediaProfiles *)userData;
    if (strcmp("Video", name) == 0) {
        createVideoCodec(atts, natts, profiles);
    } else if (strcmp("Audio", name) == 0) {
        createAudioCodec(atts, natts, profiles);
    } else if (strcmp("VideoEncoderCap", name) == 0 &&
               natts >= 4 &&
               strcmp("true", atts[3]) == 0) {
        MediaProfiles::VideoEncoderCap* cap = createVideoEncoderCap(atts, natts);
        if (cap != nullptr) {
          profiles->mVideoEncoders.add(cap);
        }
    } else if (strcmp("AudioEncoderCap", name) == 0 &&
               natts >= 4 &&
               strcmp("true", atts[3]) == 0) {
        MediaProfiles::AudioEncoderCap* cap = createAudioEncoderCap(atts, natts);
        if (cap != nullptr) {
          profiles->mAudioEncoders.add(cap);
        }
    } else if (strcmp("VideoDecoderCap", name) == 0 &&
               natts >= 4 &&
               strcmp("true", atts[3]) == 0) {
        MediaProfiles::VideoDecoderCap* cap = createVideoDecoderCap(atts, natts);
        if (cap != nullptr) {
          profiles->mVideoDecoders.add(cap);
        }
    } else if (strcmp("AudioDecoderCap", name) == 0 &&
               natts >= 4 &&
               strcmp("true", atts[3]) == 0) {
        MediaProfiles::AudioDecoderCap* cap = createAudioDecoderCap(atts, natts);
        if (cap != nullptr) {
          profiles->mAudioDecoders.add(cap);
        }
    } else if (strcmp("EncoderOutputFileFormat", name) == 0) {
        profiles->mEncoderOutputFileFormats.add(createEncoderOutputFileFormat(atts, natts));
    } else if (strcmp("CamcorderProfiles", name) == 0) {
        profiles->mCurrentCameraId = getCameraId(atts, natts);
        profiles->addStartTimeOffset(profiles->mCurrentCameraId, atts, natts);
    } else if (strcmp("EncoderProfile", name) == 0) {
      MediaProfiles::CamcorderProfile* profile = createCamcorderProfile(
          profiles->mCurrentCameraId, atts, natts, profiles->mCameraIds);
      if (profile != nullptr) {
        profiles->mCamcorderProfiles.add(profile);
      }
    } else if (strcmp("ImageEncoding", name) == 0) {
        profiles->addImageEncodingQualityLevel(profiles->mCurrentCameraId, atts, natts);
    }
}

static bool isCamcorderProfile(camcorder_quality quality) {
    return quality >= CAMCORDER_QUALITY_LIST_START &&
           quality <= CAMCORDER_QUALITY_LIST_END;
}

static bool isTimelapseProfile(camcorder_quality quality) {
    return quality >= CAMCORDER_QUALITY_TIME_LAPSE_LIST_START &&
           quality <= CAMCORDER_QUALITY_TIME_LAPSE_LIST_END;
}

static bool isHighSpeedProfile(camcorder_quality quality) {
    return quality >= CAMCORDER_QUALITY_HIGH_SPEED_LIST_START &&
           quality <= CAMCORDER_QUALITY_HIGH_SPEED_LIST_END;
}

void MediaProfiles::initRequiredProfileRefs(const Vector<int>& cameraIds) {
    ALOGV("Number of camera ids: %zu", cameraIds.size());
    CHECK(cameraIds.size() > 0);
    mRequiredProfileRefs = new RequiredProfiles[cameraIds.size()];
    for (size_t i = 0, n = cameraIds.size(); i < n; ++i) {
        mRequiredProfileRefs[i].mCameraId = cameraIds[i];
        for (size_t j = 0; j < kNumRequiredProfiles; ++j) {
            mRequiredProfileRefs[i].mRefs[j].mHasRefProfile = false;
            mRequiredProfileRefs[i].mRefs[j].mRefProfileIndex = -1;
            if ((j & 1) == 0) {  // low resolution
                mRequiredProfileRefs[i].mRefs[j].mResolutionProduct = 0x7FFFFFFF;
            } else {             // high resolution
                mRequiredProfileRefs[i].mRefs[j].mResolutionProduct = 0;
            }
        }
    }
}

int MediaProfiles::getRequiredProfileRefIndex(int cameraId) {
    for (size_t i = 0, n = mCameraIds.size(); i < n; ++i) {
        if (mCameraIds[i] == cameraId) {
            return i;
        }
    }
    return -1;
}

void MediaProfiles::checkAndAddRequiredProfilesIfNecessary() {
    if (sIsInitialized) {
        return;
    }

    initRequiredProfileRefs(mCameraIds);

    for (size_t i = 0, n = mCamcorderProfiles.size(); i < n; ++i) {
        // ensure at least one video and audio profile is added
        if (mCamcorderProfiles[i]->mVideoCodecs.empty()) {
            mCamcorderProfiles[i]->mVideoCodecs.emplace_back(
                    VIDEO_ENCODER_H263, 192000 /* bitrate */,
                    176 /* width */, 144 /* height */, 20 /* frameRate */);
        }
        if (mCamcorderProfiles[i]->mAudioCodecs.empty()) {
            mCamcorderProfiles[i]->mAudioCodecs.emplace_back(
                    AUDIO_ENCODER_AMR_NB, 12200 /* bitrate */,
                    8000 /* sampleRate */, 1 /* channels */);
        }

        int product = mCamcorderProfiles[i]->mVideoCodecs[0].mFrameWidth *
                      mCamcorderProfiles[i]->mVideoCodecs[0].mFrameHeight;

        camcorder_quality quality = mCamcorderProfiles[i]->mQuality;
        int cameraId = mCamcorderProfiles[i]->mCameraId;
        int index = -1;
        int refIndex = getRequiredProfileRefIndex(cameraId);
        CHECK(refIndex != -1);
        RequiredProfileRefInfo *info;
        camcorder_quality refQuality;

        // Check high and low from either camcorder profile, timelapse profile
        // or high speed profile, but not all of them. Default, check camcorder profile
        size_t j = 0;
        size_t o = 2;
        if (isTimelapseProfile(quality)) {
            // Check timelapse profile instead.
            j = 2;
            o = kNumRequiredProfiles;
        } else if (isHighSpeedProfile(quality)) {
            // Skip the check for high speed profile.
            continue;
        } else {
            // Must be camcorder profile.
            CHECK(isCamcorderProfile(quality));
        }
        for (; j < o; ++j) {
            info = &(mRequiredProfileRefs[refIndex].mRefs[j]);
            if ((j % 2 == 0 && product > info->mResolutionProduct) ||  // low
                (j % 2 != 0 && product < info->mResolutionProduct)) {  // high
                continue;
            }
            switch (j) {
                case 0:
                   refQuality = CAMCORDER_QUALITY_LOW;
                   break;
                case 1:
                   refQuality = CAMCORDER_QUALITY_HIGH;
                   break;
                case 2:
                   refQuality = CAMCORDER_QUALITY_TIME_LAPSE_LOW;
                   break;
                case 3:
                   refQuality = CAMCORDER_QUALITY_TIME_LAPSE_HIGH;
                   break;
                default:
                    CHECK(!"Should never reach here");
            }

            if (!info->mHasRefProfile) {
                index = getCamcorderProfileIndex(cameraId, refQuality);
            }
            if (index == -1) {
                // New high or low quality profile is found.
                // Update its reference.
                info->mHasRefProfile = true;
                info->mRefProfileIndex = i;
                info->mResolutionProduct = product;
            }
        }
    }

    for (size_t cameraId = 0; cameraId < mCameraIds.size(); ++cameraId) {
        for (size_t j = 0; j < kNumRequiredProfiles; ++j) {
            int refIndex = getRequiredProfileRefIndex(cameraId);
            CHECK(refIndex != -1);
            RequiredProfileRefInfo *info =
                    &mRequiredProfileRefs[refIndex].mRefs[j];

            if (info->mHasRefProfile) {

                std::unique_ptr<CamcorderProfile> profile =
                    std::make_unique<CamcorderProfile>(
                            *mCamcorderProfiles[info->mRefProfileIndex]);

                // Overwrite the quality
                switch (j % kNumRequiredProfiles) {
                    case 0:
                        profile->mQuality = CAMCORDER_QUALITY_LOW;
                        break;
                    case 1:
                        profile->mQuality = CAMCORDER_QUALITY_HIGH;
                        break;
                    case 2:
                        profile->mQuality = CAMCORDER_QUALITY_TIME_LAPSE_LOW;
                        break;
                    case 3:
                        profile->mQuality = CAMCORDER_QUALITY_TIME_LAPSE_HIGH;
                        break;
                    default:
                        CHECK(!"Should never come here");
                }

                int index = getCamcorderProfileIndex(cameraId, profile->mQuality);
                if (index != -1) {
                    ALOGV("Profile quality %d for camera %zu already exists",
                        profile->mQuality, cameraId);
                    CHECK(index == refIndex);
                    continue;
                }

                // Insert the new profile
                ALOGV("Add a profile: quality %d=>%d for camera %zu",
                        mCamcorderProfiles[info->mRefProfileIndex]->mQuality,
                        profile->mQuality, cameraId);

                mCamcorderProfiles.add(profile.release());
            }
        }
    }
}

/*static*/ MediaProfiles*
MediaProfiles::getInstance()
{
    char platform[PROPERTY_VALUE_MAX] = {0};
    ALOGV("getInstance");
    Mutex::Autolock lock(sLock);
    if (!sIsInitialized) {
        char value[PROPERTY_VALUE_MAX];
        if (property_get("media.settings.xml", value, NULL) <= 0) {
            const char* xmlFile = nullptr;
            for (auto const& f : getXmlPaths()) {
                if (checkXmlFile(f)) {
                    xmlFile = f;
                    break;
                }
            }
            if (xmlFile == nullptr) {
                ALOGW("Could not find a validated xml file. "
                        "Using the default instance instead.");
                sInstance = createDefaultInstance();
            } else {
                sInstance = createInstanceFromXmlFile(xmlFile);
            }
        } else {
                if (!strncmp(value, "/vendor/etc", strlen("/vendor/etc"))) {
                    property_get("ro.board.platform", platform, NULL);
                    char variant[PROPERTY_VALUE_MAX];
                    if (property_get("ro.media.xml_variant.codecs", variant, NULL) > 0) {
                        std::string xmlPath = std::string("/vendor/etc/media_profiles") +
                                              std::string(variant) + std::string(".xml");
                        strlcpy(value, xmlPath.c_str(), PROPERTY_VALUE_MAX);
                        ALOGI("Profiles xml path: %s", value);
                    }
                }
            sInstance = createInstanceFromXmlFile(value);
        }
        CHECK(sInstance != NULL);
        sInstance->checkAndAddRequiredProfilesIfNecessary();
        sIsInitialized = true;
    }

    return sInstance;
}

/*static*/ MediaProfiles::VideoEncoderCap*
MediaProfiles::createDefaultH263VideoEncoderCap()
{
    return new MediaProfiles::VideoEncoderCap(
        VIDEO_ENCODER_H263, 192000, 420000, 176, 352, 144, 288, 1, 20);
}

/*static*/ MediaProfiles::VideoEncoderCap*
MediaProfiles::createDefaultM4vVideoEncoderCap()
{
    return new MediaProfiles::VideoEncoderCap(
        VIDEO_ENCODER_MPEG_4_SP, 192000, 420000, 176, 352, 144, 288, 1, 20);
}


/*static*/ void
MediaProfiles::createDefaultVideoEncoders(MediaProfiles *profiles)
{
    profiles->mVideoEncoders.add(createDefaultH263VideoEncoderCap());
    profiles->mVideoEncoders.add(createDefaultM4vVideoEncoderCap());
}

/*static*/ MediaProfiles::CamcorderProfile*
MediaProfiles::createDefaultCamcorderTimeLapseQcifProfile(camcorder_quality quality)
{
    CamcorderProfile *profile = new MediaProfiles::CamcorderProfile;
    profile->mCameraId = 0;
    profile->mFileFormat = OUTPUT_FORMAT_THREE_GPP;
    profile->mQuality = quality;
    profile->mDuration = 60;
    profile->mVideoCodecs.emplace_back(
            VIDEO_ENCODER_H263, 1000000 /* bitrate */,
            176 /* width */, 144 /* height */, 20 /* frameRate */);
    profile->mAudioCodecs.emplace_back(
            AUDIO_ENCODER_AMR_NB, 12200 /* bitrate */,
            8000 /* sampleRate */, 1 /* channels */);

    return profile;
}

/*static*/ MediaProfiles::CamcorderProfile*
MediaProfiles::createDefaultCamcorderTimeLapse480pProfile(camcorder_quality quality)
{
    CamcorderProfile *profile = new MediaProfiles::CamcorderProfile;
    profile->mCameraId = 0;
    profile->mFileFormat = OUTPUT_FORMAT_THREE_GPP;
    profile->mQuality = quality;
    profile->mDuration = 60;
    profile->mVideoCodecs.emplace_back(
            VIDEO_ENCODER_H263, 20000000 /* bitrate */,
            720 /* width */, 480 /* height */, 20 /* frameRate */);
    profile->mAudioCodecs.emplace_back(
            AUDIO_ENCODER_AMR_NB, 12200 /* bitrate */,
            8000 /* sampleRate */, 1 /* channels */);
    return profile;
}

/*static*/ void
MediaProfiles::createDefaultCamcorderTimeLapseLowProfiles(
        MediaProfiles::CamcorderProfile **lowTimeLapseProfile,
        MediaProfiles::CamcorderProfile **lowSpecificTimeLapseProfile) {
    *lowTimeLapseProfile = createDefaultCamcorderTimeLapseQcifProfile(
            CAMCORDER_QUALITY_TIME_LAPSE_LOW);
    *lowSpecificTimeLapseProfile = createDefaultCamcorderTimeLapseQcifProfile(
            CAMCORDER_QUALITY_TIME_LAPSE_QCIF);
}

/*static*/ void
MediaProfiles::createDefaultCamcorderTimeLapseHighProfiles(
        MediaProfiles::CamcorderProfile **highTimeLapseProfile,
        MediaProfiles::CamcorderProfile **highSpecificTimeLapseProfile) {
    *highTimeLapseProfile = createDefaultCamcorderTimeLapse480pProfile(
            CAMCORDER_QUALITY_TIME_LAPSE_HIGH);
    *highSpecificTimeLapseProfile = createDefaultCamcorderTimeLapse480pProfile(
            CAMCORDER_QUALITY_TIME_LAPSE_480P);
}

/*static*/ MediaProfiles::CamcorderProfile*
MediaProfiles::createDefaultCamcorderQcifProfile(camcorder_quality quality)
{
    CamcorderProfile *profile = new MediaProfiles::CamcorderProfile;
    profile->mCameraId = 0;
    profile->mFileFormat = OUTPUT_FORMAT_THREE_GPP;
    profile->mQuality = quality;
    profile->mDuration = 30;
    profile->mVideoCodecs.emplace_back(
            VIDEO_ENCODER_H263, 192000 /* bitrate */,
            176 /* width */, 144 /* height */, 20 /* frameRate */);
    profile->mAudioCodecs.emplace_back(
            AUDIO_ENCODER_AMR_NB, 12200 /* bitrate */,
            8000 /* sampleRate */, 1 /* channels */);
    return profile;
}

/*static*/ MediaProfiles::CamcorderProfile*
MediaProfiles::createDefaultCamcorderCifProfile(camcorder_quality quality)
{
    CamcorderProfile *profile = new MediaProfiles::CamcorderProfile;
    profile->mCameraId = 0;
    profile->mFileFormat = OUTPUT_FORMAT_THREE_GPP;
    profile->mQuality = quality;
    profile->mDuration = 60;
    profile->mVideoCodecs.emplace_back(
            VIDEO_ENCODER_H263, 360000 /* bitrate */,
            352 /* width */, 288 /* height */, 20 /* frameRate */);
    profile->mAudioCodecs.emplace_back(
            AUDIO_ENCODER_AMR_NB, 12200 /* bitrate */,
            8000 /* sampleRate */, 1 /* channels */);
    return profile;
}

/*static*/ void
MediaProfiles::createDefaultCamcorderLowProfiles(
        MediaProfiles::CamcorderProfile **lowProfile,
        MediaProfiles::CamcorderProfile **lowSpecificProfile) {
    *lowProfile = createDefaultCamcorderQcifProfile(CAMCORDER_QUALITY_LOW);
    *lowSpecificProfile = createDefaultCamcorderQcifProfile(CAMCORDER_QUALITY_QCIF);
}

/*static*/ void
MediaProfiles::createDefaultCamcorderHighProfiles(
        MediaProfiles::CamcorderProfile **highProfile,
        MediaProfiles::CamcorderProfile **highSpecificProfile) {
    *highProfile = createDefaultCamcorderCifProfile(CAMCORDER_QUALITY_HIGH);
    *highSpecificProfile = createDefaultCamcorderCifProfile(CAMCORDER_QUALITY_CIF);
}

/*static*/ void
MediaProfiles::createDefaultCamcorderProfiles(MediaProfiles *profiles)
{
    // low camcorder profiles.
    MediaProfiles::CamcorderProfile *lowProfile, *lowSpecificProfile;
    createDefaultCamcorderLowProfiles(&lowProfile, &lowSpecificProfile);
    profiles->mCamcorderProfiles.add(lowProfile);
    profiles->mCamcorderProfiles.add(lowSpecificProfile);

    // high camcorder profiles.
    MediaProfiles::CamcorderProfile* highProfile, *highSpecificProfile;
    createDefaultCamcorderHighProfiles(&highProfile, &highSpecificProfile);
    profiles->mCamcorderProfiles.add(highProfile);
    profiles->mCamcorderProfiles.add(highSpecificProfile);

    // low camcorder time lapse profiles.
    MediaProfiles::CamcorderProfile *lowTimeLapseProfile, *lowSpecificTimeLapseProfile;
    createDefaultCamcorderTimeLapseLowProfiles(&lowTimeLapseProfile, &lowSpecificTimeLapseProfile);
    profiles->mCamcorderProfiles.add(lowTimeLapseProfile);
    profiles->mCamcorderProfiles.add(lowSpecificTimeLapseProfile);

    // high camcorder time lapse profiles.
    MediaProfiles::CamcorderProfile *highTimeLapseProfile, *highSpecificTimeLapseProfile;
    createDefaultCamcorderTimeLapseHighProfiles(&highTimeLapseProfile,
            &highSpecificTimeLapseProfile);
    profiles->mCamcorderProfiles.add(highTimeLapseProfile);
    profiles->mCamcorderProfiles.add(highSpecificTimeLapseProfile);

    // For emulator and other legacy devices which does not have a
    // media_profiles.xml file, We assume that the default camera id
    // is 0 and that is the only camera available.
    profiles->mCameraIds.push(0);
}

/*static*/ void
MediaProfiles::createDefaultAudioEncoders(MediaProfiles *profiles)
{
    profiles->mAudioEncoders.add(createDefaultAmrNBEncoderCap());
}

/*static*/ void
MediaProfiles::createDefaultVideoDecoders(MediaProfiles *profiles)
{
    MediaProfiles::VideoDecoderCap *cap =
        new MediaProfiles::VideoDecoderCap(VIDEO_DECODER_WMV);

    profiles->mVideoDecoders.add(cap);
}

/*static*/ void
MediaProfiles::createDefaultAudioDecoders(MediaProfiles *profiles)
{
    MediaProfiles::AudioDecoderCap *cap =
        new MediaProfiles::AudioDecoderCap(AUDIO_DECODER_WMA);

    profiles->mAudioDecoders.add(cap);
}

/*static*/ void
MediaProfiles::createDefaultEncoderOutputFileFormats(MediaProfiles *profiles)
{
    profiles->mEncoderOutputFileFormats.add(OUTPUT_FORMAT_THREE_GPP);
    profiles->mEncoderOutputFileFormats.add(OUTPUT_FORMAT_MPEG_4);
}

/*static*/ MediaProfiles::AudioEncoderCap*
MediaProfiles::createDefaultAmrNBEncoderCap()
{
    return new MediaProfiles::AudioEncoderCap(
        AUDIO_ENCODER_AMR_NB, 5525, 12200, 8000, 8000, 1, 1);
}

/*static*/ void
MediaProfiles::createDefaultImageEncodingQualityLevels(MediaProfiles *profiles)
{
    ImageEncodingQualityLevels *levels = new ImageEncodingQualityLevels();
    levels->mCameraId = 0;
    levels->mLevels.add(70);
    levels->mLevels.add(80);
    levels->mLevels.add(90);
    profiles->mImageEncodingQualityLevels.add(levels);
}

/*static*/ MediaProfiles*
MediaProfiles::createDefaultInstance()
{
    MediaProfiles *profiles = new MediaProfiles;
    createDefaultCamcorderProfiles(profiles);
    createDefaultVideoEncoders(profiles);
    createDefaultAudioEncoders(profiles);
    createDefaultVideoDecoders(profiles);
    createDefaultAudioDecoders(profiles);
    createDefaultEncoderOutputFileFormats(profiles);
    createDefaultImageEncodingQualityLevels(profiles);
    return profiles;
}

bool MediaProfiles::checkXmlFile(const char* xmlFile) {
    struct stat fStat;
    return stat(xmlFile, &fStat) == 0 && S_ISREG(fStat.st_mode);
    // TODO: Add validation
}

/*static*/ MediaProfiles*
MediaProfiles::createInstanceFromXmlFile(const char *xml)
{
    FILE *fp = NULL;
    CHECK((fp = fopen(xml, "r")));

    XML_Parser parser = ::XML_ParserCreate(NULL);
    CHECK(parser != NULL);

    MediaProfiles *profiles = new MediaProfiles();
    ::XML_SetUserData(parser, profiles);
    ::XML_SetElementHandler(parser, startElementHandler, NULL);

    /*
      FIXME:
      expat is not compiled with -DXML_DTD. We don't have DTD parsing support.

      if (!::XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS)) {
          ALOGE("failed to enable DTD support in the xml file");
          return UNKNOWN_ERROR;
      }

    */

    const int BUFF_SIZE = 512;
    for (;;) {
        void *buff = ::XML_GetBuffer(parser, BUFF_SIZE);
        if (buff == NULL) {
            ALOGE("failed to in call to XML_GetBuffer()");
            delete profiles;
            profiles = NULL;
            goto exit;
        }

        int bytes_read = ::fread(buff, 1, BUFF_SIZE, fp);
        if (bytes_read < 0) {
            ALOGE("failed in call to read");
            delete profiles;
            profiles = NULL;
            goto exit;
        }

        CHECK(::XML_ParseBuffer(parser, bytes_read, bytes_read == 0));

        if (bytes_read == 0) break;  // done parsing the xml file
    }

exit:
    ::XML_ParserFree(parser);
    ::fclose(fp);
    return profiles;
}

Vector<output_format> MediaProfiles::getOutputFileFormats() const
{
    return mEncoderOutputFileFormats;  // copy out
}

Vector<video_encoder> MediaProfiles::getVideoEncoders() const
{
    Vector<video_encoder> encoders;
    for (size_t i = 0; i < mVideoEncoders.size(); ++i) {
        encoders.add(mVideoEncoders[i]->mCodec);
    }
    return encoders;  // copy out
}

int MediaProfiles::getVideoEncoderParamByName(const char *name, video_encoder codec) const
{
    ALOGV("getVideoEncoderParamByName: %s for codec %d", name, codec);
    int index = -1;
    for (size_t i = 0, n = mVideoEncoders.size(); i < n; ++i) {
        if (mVideoEncoders[i]->mCodec == codec) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        ALOGE("The given video encoder %d is not found", codec);
        return -1;
    }

    if (!strcmp("enc.vid.width.min", name)) return mVideoEncoders[index]->mMinFrameWidth;
    if (!strcmp("enc.vid.width.max", name)) return mVideoEncoders[index]->mMaxFrameWidth;
    if (!strcmp("enc.vid.height.min", name)) return mVideoEncoders[index]->mMinFrameHeight;
    if (!strcmp("enc.vid.height.max", name)) return mVideoEncoders[index]->mMaxFrameHeight;
    if (!strcmp("enc.vid.bps.min", name)) return mVideoEncoders[index]->mMinBitRate;
    if (!strcmp("enc.vid.bps.max", name)) return mVideoEncoders[index]->mMaxBitRate;
    if (!strcmp("enc.vid.fps.min", name)) return mVideoEncoders[index]->mMinFrameRate;
    if (!strcmp("enc.vid.fps.max", name)) return mVideoEncoders[index]->mMaxFrameRate;

    ALOGE("The given video encoder param name %s is not found", name);
    return -1;
}

Vector<audio_encoder> MediaProfiles::getAudioEncoders() const
{
    Vector<audio_encoder> encoders;
    for (size_t i = 0; i < mAudioEncoders.size(); ++i) {
        encoders.add(mAudioEncoders[i]->mCodec);
    }
    return encoders;  // copy out
}

int MediaProfiles::getAudioEncoderParamByName(const char *name, audio_encoder codec) const
{
    ALOGV("getAudioEncoderParamByName: %s for codec %d", name, codec);
    int index = -1;
    for (size_t i = 0, n = mAudioEncoders.size(); i < n; ++i) {
        if (mAudioEncoders[i]->mCodec == codec) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        ALOGE("The given audio encoder %d is not found", codec);
        return -1;
    }

    if (!strcmp("enc.aud.ch.min", name)) return mAudioEncoders[index]->mMinChannels;
    if (!strcmp("enc.aud.ch.max", name)) return mAudioEncoders[index]->mMaxChannels;
    if (!strcmp("enc.aud.bps.min", name)) return mAudioEncoders[index]->mMinBitRate;
    if (!strcmp("enc.aud.bps.max", name)) return mAudioEncoders[index]->mMaxBitRate;
    if (!strcmp("enc.aud.hz.min", name)) return mAudioEncoders[index]->mMinSampleRate;
    if (!strcmp("enc.aud.hz.max", name)) return mAudioEncoders[index]->mMaxSampleRate;

    ALOGE("The given audio encoder param name %s is not found", name);
    return -1;
}

Vector<video_decoder> MediaProfiles::getVideoDecoders() const
{
    Vector<video_decoder> decoders;
    for (size_t i = 0; i < mVideoDecoders.size(); ++i) {
        decoders.add(mVideoDecoders[i]->mCodec);
    }
    return decoders;  // copy out
}

Vector<audio_decoder> MediaProfiles::getAudioDecoders() const
{
    Vector<audio_decoder> decoders;
    for (size_t i = 0; i < mAudioDecoders.size(); ++i) {
        decoders.add(mAudioDecoders[i]->mCodec);
    }
    return decoders;  // copy out
}

int MediaProfiles::getCamcorderProfileIndex(int cameraId, camcorder_quality quality) const
{
    int index = -1;
    for (size_t i = 0, n = mCamcorderProfiles.size(); i < n; ++i) {
        if (mCamcorderProfiles[i]->mCameraId == cameraId &&
            mCamcorderProfiles[i]->mQuality == quality) {
            index = i;
            break;
        }
    }
    return index;
}

const MediaProfiles::CamcorderProfile *MediaProfiles::getCamcorderProfile(
            int cameraId, camcorder_quality quality) const {
    int index = getCamcorderProfileIndex(cameraId, quality);
    if (index == -1) {
        ALOGE("The given camcorder profile camera %d quality %d is not found",
            cameraId, quality);
        return nullptr;
    }

    return mCamcorderProfiles[index];
}

std::vector<const MediaProfiles::AudioCodec *>
MediaProfiles::CamcorderProfile::getAudioCodecs() const {
    std::vector<const MediaProfiles::AudioCodec *> res;
    for (const MediaProfiles::AudioCodec &ac : mAudioCodecs) {
        res.push_back(&ac);
    }
    return res;
}

std::vector<const MediaProfiles::VideoCodec *>
MediaProfiles::CamcorderProfile::getVideoCodecs() const {
    std::vector<const MediaProfiles::VideoCodec *> res;
    for (const MediaProfiles::VideoCodec &vc : mVideoCodecs) {
        res.push_back(&vc);
    }
    return res;
}

int MediaProfiles::getCamcorderProfileParamByName(const char *name,
                                                  int cameraId,
                                                  camcorder_quality quality) const
{
    ALOGV("getCamcorderProfileParamByName: %s for camera %d, quality %d",
        name, cameraId, quality);

    int index = getCamcorderProfileIndex(cameraId, quality);
    if (index == -1) {
        ALOGE("The given camcorder profile camera %d quality %d is not found",
            cameraId, quality);
        return -1;
    }

    if (!strcmp("duration", name)) return mCamcorderProfiles[index]->mDuration;
    if (!strcmp("file.format", name)) return mCamcorderProfiles[index]->mFileFormat;
    if (!strcmp("vid.codec", name)) return mCamcorderProfiles[index]->mVideoCodecs[0].mCodec;
    if (!strcmp("vid.width", name)) return mCamcorderProfiles[index]->mVideoCodecs[0].mFrameWidth;
    if (!strcmp("vid.height", name)) return mCamcorderProfiles[index]->mVideoCodecs[0].mFrameHeight;
    if (!strcmp("vid.bps", name)) return mCamcorderProfiles[index]->mVideoCodecs[0].mBitRate;
    if (!strcmp("vid.fps", name)) return mCamcorderProfiles[index]->mVideoCodecs[0].mFrameRate;
    if (!strcmp("aud.codec", name)) return mCamcorderProfiles[index]->mAudioCodecs[0].mCodec;
    if (!strcmp("aud.bps", name)) return mCamcorderProfiles[index]->mAudioCodecs[0].mBitRate;
    if (!strcmp("aud.ch", name)) return mCamcorderProfiles[index]->mAudioCodecs[0].mChannels;
    if (!strcmp("aud.hz", name)) return mCamcorderProfiles[index]->mAudioCodecs[0].mSampleRate;

    ALOGE("The given camcorder profile param id %d name %s is not found", cameraId, name);
    return -1;
}

bool MediaProfiles::hasCamcorderProfile(int cameraId, camcorder_quality quality) const
{
    return (getCamcorderProfileIndex(cameraId, quality) != -1);
}

Vector<int> MediaProfiles::getImageEncodingQualityLevels(int cameraId) const
{
    Vector<int> result;
    ImageEncodingQualityLevels *levels = findImageEncodingQualityLevels(cameraId);
    if (levels != NULL) {
        result = levels->mLevels;  // copy out
    }
    return result;
}

int MediaProfiles::getStartTimeOffsetMs(int cameraId) const {
    int offsetTimeMs = -1;
    ssize_t index = mStartTimeOffsets.indexOfKey(cameraId);
    if (index >= 0) {
        offsetTimeMs = mStartTimeOffsets.valueFor(cameraId);
    }
    ALOGV("offsetTime=%d ms and cameraId=%d", offsetTimeMs, cameraId);
    return offsetTimeMs;
}

MediaProfiles::~MediaProfiles()
{
    CHECK("destructor should never be called" == 0);
#if 0
    for (size_t i = 0; i < mAudioEncoders.size(); ++i) {
        delete mAudioEncoders[i];
    }
    mAudioEncoders.clear();

    for (size_t i = 0; i < mVideoEncoders.size(); ++i) {
        delete mVideoEncoders[i];
    }
    mVideoEncoders.clear();

    for (size_t i = 0; i < mVideoDecoders.size(); ++i) {
        delete mVideoDecoders[i];
    }
    mVideoDecoders.clear();

    for (size_t i = 0; i < mAudioDecoders.size(); ++i) {
        delete mAudioDecoders[i];
    }
    mAudioDecoders.clear();

    for (size_t i = 0; i < mCamcorderProfiles.size(); ++i) {
        delete mCamcorderProfiles[i];
    }
    mCamcorderProfiles.clear();
#endif
}
} // namespace android
