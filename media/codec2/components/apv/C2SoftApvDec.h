/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef ANDROID_C2_SOFT_APV_DEC_H_
#define ANDROID_C2_SOFT_APV_DEC_H_

#include <media/stagefright/foundation/ColorUtils.h>

#include <SimpleC2Component.h>
#include <util/C2InterfaceHelper.h>
#include <inttypes.h>
#include <atomic>

#include "oapv.h"
#include <C2SoftApvCommon.h>

namespace android {

struct C2SoftApvDec final : public SimpleC2Component {
    class IntfImpl;

    C2SoftApvDec(const char* name, c2_node_id_t id, const std::shared_ptr<IntfImpl>& intfImpl);
    C2SoftApvDec(const char* name, c2_node_id_t id,
                 const std::shared_ptr<C2ReflectorHelper>& helper);
    virtual ~C2SoftApvDec();

    // From SimpleC2Component
    c2_status_t onInit() override;
    c2_status_t onStop() override;
    void onReset() override;
    void onRelease() override;
    c2_status_t onFlush_sm() override;
    void process(const std::unique_ptr<C2Work>& work,
                 const std::shared_ptr<C2BlockPool>& pool) override;
    c2_status_t drain(uint32_t drainMode, const std::shared_ptr<C2BlockPool>& pool) override;

  private:
    status_t createDecoder();
    status_t initDecoder();
    bool isConfigured() const;
    void drainDecoder();
    status_t setFlushMode();
    status_t resetDecoder();
    void resetPlugin();
    status_t deleteDecoder();
    void finishWork(uint64_t index, const std::unique_ptr<C2Work>& work,
                    const std::shared_ptr<C2GraphicBlock>& block);
    void drainRingBuffer(const std::unique_ptr<C2Work>& work,
                         const std::shared_ptr<C2BlockPool>& pool, bool eos);
    c2_status_t drainInternal(uint32_t drainMode, const std::shared_ptr<C2BlockPool>& pool,
                              const std::unique_ptr<C2Work>& work);

    status_t outputBuffer(const std::shared_ptr<C2BlockPool>& pool,
                          const std::unique_ptr<C2Work>& work);

    std::shared_ptr<IntfImpl> mIntf;
    uint8_t *mOutBufferFlush;
    uint32_t mOutputDelay;
    std::atomic_uint64_t mOutIndex;
    std::shared_ptr<C2GraphicBlock> mOutBlock;

    std::shared_ptr<C2StreamPixelFormatInfo::output> mPixelFormatInfo;

    std::shared_ptr<C2StreamPictureSizeInfo::input> mSize;
    uint32_t mHalPixelFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    bool mSignalledOutputEos;
    bool mSignalledError;

    C2StreamHdrStaticMetadataInfo::output mHdrStaticMetadataInfo;
    std::unique_ptr<C2StreamHdr10PlusInfo::output> mHdr10PlusInfo = nullptr;

    // Color aspects. These are ISO values and are meant to detect changes in aspects to avoid
    // converting them to C2 values for each frame
    struct VuiColorAspects {
        uint8_t primaries;
        uint8_t transfer;
        uint8_t coeffs;
        uint8_t fullRange;

        // default color aspects
        VuiColorAspects()
            : primaries(C2Color::PRIMARIES_UNSPECIFIED),
            transfer(C2Color::TRANSFER_UNSPECIFIED),
            coeffs(C2Color::MATRIX_UNSPECIFIED),
            fullRange(C2Color::RANGE_UNSPECIFIED) { }

        bool operator==(const VuiColorAspects &o) const {
            return primaries == o.primaries && transfer == o.transfer && coeffs == o.coeffs
                && fullRange == o.fullRange;
        }
    } mBitstreamColorAspects;
    struct VuiColorAspects vuiColorAspects;

    // HDR info that can be carried in APV bitstream
    // Section 10.3.1 of APV syntax https://www.ietf.org/archive/id/draft-lim-apv-02.html
    struct ApvHdrInfo {
        bool has_hdr_mdcv;
        bool has_itut_t35;
        bool has_hdr_cll;

        ApvHdrInfo()
            : has_hdr_mdcv(false),
            has_itut_t35(false),
            has_hdr_cll(false) { }

        // Master Display Color Volume
        struct HdrMdcv {
            uint16_t primary_chromaticity_x[3];
            uint16_t primary_chromaticity_y[3];
            uint16_t white_point_chromaticity_x;
            uint16_t white_point_chromaticity_y;
            uint32_t max_mastering_luminance;
            uint32_t min_mastering_luminance;
        } hdr_mdcv;

        // Content Light Level info
        struct HdrCll {
            uint16_t max_cll;
            uint16_t max_fall;
        } hdr_cll;

        // ITU-T35 info
        struct ItutT35 {
            char country_code;
            char country_code_extension_byte;
            char *payload_bytes;
            int payload_size;
        } itut_t35;
    };

    oapvd_t mDecHandle;
    oapvm_t mMetadataHandle;
    oapv_frms_t mOutFrames;

    int mOutCsp;

    void getVuiParams(const std::unique_ptr<C2Work> &work);
    void getHdrInfo(struct ApvHdrInfo *buffer, int id);
    void getHDRStaticParams(const struct ApvHdrInfo *buffer, const std::unique_ptr<C2Work>& work);
    void getHDR10PlusInfoData(const struct ApvHdrInfo *buffer, const std::unique_ptr<C2Work>& work);

    C2_DO_NOT_COPY(C2SoftApvDec);
};

}  // namespace android

#endif
