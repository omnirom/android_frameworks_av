/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
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
 *
 */

#define LOG_TAG "Camera2-QTIParameters"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <utils/Trace.h>
#include <utils/Vector.h>
#include <utils/SortedVector.h>

#include <math.h>
#include <stdlib.h>
#include <cutils/properties.h>

#include "QTIParameters.h"
#include "Parameters.h"
#include "system/camera.h"
#include "hardware/camera_common.h"
#include <android/hardware/ICamera.h>
#include <media/MediaProfiles.h>
#include <media/mediarecorder.h>
#include "api1/Camera2Client.h"

namespace android {
namespace camera2 {

//Sharpness
const char KEY_QTI_VENDOR_SHARPNESS_RANGE[] = "org.codeaurora.qcamera3.sharpness.range";
const char KEY_QTI_VENDOR_SHARPNESS_STRENGTH[] = "org.codeaurora.qcamera3.sharpness.strength";
const char KEY_QTI_MAX_SHARPNESS[] = "max-sharpness";
const char KEY_QTI_SHARPNESS[] = "sharpness";

//saturation
const char KEY_QTI_VENDOR_SATURATION_RANGE[] = "org.codeaurora.qcamera3.saturation.range";
const char KEY_QTI_VENDOR_SATURATION[] = "org.codeaurora.qcamera3.saturation.use_saturation";
const char KEY_QTI_MAX_SATURATION[] = "max-saturation";
const char KEY_QTI_SATURATION[] = "saturation";

//instant aec
const char KEY_QTI_VENDOR_INSTANT_MODE[] = "org.codeaurora.qcamera3.instant_aec.instant_aec_mode";
const char KEY_QTI_VENDOR_INSTANT_MODES[] =
        "org.codeaurora.qcamera3.instant_aec.instant_aec_available_modes";
const char KEY_QTI_INSTANT_AEC_SUPPORTED_MODES[] = "instant-aec-values";
const char KEY_QTI_INSTANT_AEC[] = "instant-aec";
// Values for instant AEC modes
const char KEY_QTI_INSTANT_AEC_DISABLE[] = "0";
const char KEY_QTI_INSTANT_AEC_AGGRESSIVE_AEC[] = "1";
const char KEY_QTI_INSTANT_AEC_FAST_AEC[] = "2";

//exposure metering
const char KEY_QTI_VENDOR_EXPOSURE_METER_MODES[] =
        "org.codeaurora.qcamera3.exposure_metering.available_modes";
const char KEY_QTI_VENDOR_EXPOSURE_METER[] =
        "org.codeaurora.qcamera3.exposure_metering.exposure_metering_mode";
const char KEY_QTI_AUTO_EXPOSURE_VALUES[] = "auto-exposure-values";
const char KEY_QTI_AUTO_EXPOSURE[] = "auto-exposure";
//values for exposure metering
const char AUTO_EXPOSURE_FRAME_AVG[] = "frame-average";
const char AUTO_EXPOSURE_CENTER_WEIGHTED[] = "center-weighted";
const char AUTO_EXPOSURE_SPOT_METERING[] = "spot-metering";
const char AUTO_EXPOSURE_SMART_METERING[] = "smart-metering";
const char AUTO_EXPOSURE_USER_METERING[] = "user-metering";
const char AUTO_EXPOSURE_SPOT_METERING_ADV[] = "spot-metering-adv";
const char AUTO_EXPOSURE_CENTER_WEIGHTED_ADV[] = "center-weighted-adv";

//iso-exp priority
const char KEY_QTI_VENDOR_ISO_EXP_SELECT_PRIORITY[]  =
        "org.codeaurora.qcamera3.iso_exp_priority.select_priority";
const char KEY_QTI_VENDOR_ISO_EXP_USE_VALUE[]  =
        "org.codeaurora.qcamera3.iso_exp_priority.use_iso_exp_priority";
//Manual Exposure
const char KEY_QTI_SUPPORTED_MANUAL_EXPOSURE_MODES[] = "manual-exposure-modes";
const char KEY_QTI_EXP_TIME_PRIORITY[] = "exp-time-priority";
const char KEY_QTI_MIN_EXPOSURE_TIME[] = "min-exposure-time";
const char KEY_QTI_MAX_EXPOSURE_TIME[] = "max-exposure-time";
const char KEY_QTI_EXPOSURE_TIME[] = "exposure-time";
const char KEY_QTI_USER_SETTING[] = "user-setting";
const char KEY_QTI_MIN_ISO[] = "min-iso";
const char KEY_QTI_MAX_ISO[] = "max-iso";
const char KEY_QTI_ISO_PRIORITY[] = "iso-priority";
const char KEY_QTI_SUPPORTED_ISO_MODES[] = "iso-values";
const char KEY_QTI_ISO_MODE[] = "iso";
const char ISO_MANUAL[] = "manual";
const char KEY_QTI_CONTINUOUS_ISO[] = "continuous-iso";
// Values for ISO Settings
const char ISO_AUTO[] = "auto";
const char ISO_100[] = "ISO100";
const char ISO_200[] = "ISO200";
const char ISO_400[] = "ISO400";
const char ISO_800[] = "ISO800";
const char ISO_1600[] = "ISO1600";
const char ISO_3200[] = "ISO3200";
const char VALUE_OFF[] = "off";
const char VALUE_ON[] = "on";

//Manual White Balance
const char KEY_QTI_WB_CCT_MODE[] = "color-temperature";
const char KEY_QTI_WB_GAIN_MODE[] = "rbgb-gains";
const char KEY_QTI_MIN_WB_CCT[] = "min-wb-cct";
const char KEY_QTI_MAX_WB_CCT[] = "max-wb-cct";
const char KEY_QTI_MIN_WB_GAIN[] = "min-wb-gain";
const char KEY_QTI_MAX_WB_GAIN[] = "max-wb-gain";
const char KEY_QTI_SUPPORTED_MANUAL_WB_MODES[] = "manual-wb-modes";
const char KEY_WHITE_BALANCE[] = "whitebalance";
const char WHITE_BALANCE_MANUAL[] = "manual";
const char KEY_QTI_MANUAL_WB_TYPE[] = "manual-wb-type";
const char KEY_QTI_MANUAL_WB_VALUE[] = "manual-wb-value";
const char KEY_QTI_MANUAL_WB_GAINS[] = "manual-wb-gains";

//redeye-reduction
const char KEY_QTI_REDEYE_REDUCTION[] = "redeye-reduction";
//face-detection
const char  KEY_QTI_FACE_DETECTION_MODES[] = "face-detection-values";

camera_metadata_ro_entry_t g_availableSensitivityRange;
double minExposureTime;
double maxExposureTime;
const char minWbGain[] = "1.0";
const char maxWbGain[] = "4.0";


status_t QTIParameters::initialize(void *parametersParent,
        sp<CameraDeviceBase> device, sp<CameraProviderManager> manager) {
    status_t res = OK;

    Parameters* ParentParams = (Parameters*)parametersParent;
    mVendorTagId = manager->getProviderTagIdLocked(device->getId().string());
    sp<VendorTagDescriptor> vTags =
        VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if ((nullptr == vTags.get()) || (0 >= vTags->getTagCount())) {
        sp<VendorTagDescriptorCache> cache =
                VendorTagDescriptorCache::getGlobalVendorTagCache();
        if (cache.get()) {
            cache->getVendorTagDescriptor(mVendorTagId, &vTags);
        }
    }
    uint32_t tag = 0;
    isoValue = -1;
    exposureTime = -1;

    // Temp Initialize
    ParentParams->params.set("max-contrast", 10);

    ParentParams->params.set("redeye-reduction-values",
            "disable,enable");

    ParentParams->params.set(KEY_QTI_REDEYE_REDUCTION,
            "disable");

    ParentParams->params.set("num-snaps-per-shutter", 1);

    ParentParams->params.set("ae-bracket-hdr-values","Off,AE-Bracket");
    ParentParams->params.set("ae-bracket-hdr","Off");

    // ISO
    // Get the supported sensitivity range from device3 static info
    camera_metadata_ro_entry_t availableSensitivityRange =
        ParentParams->staticInfo(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE);
    if (availableSensitivityRange.count == 2) {
        int32_t isoMin = availableSensitivityRange.data.i32[0];
        int32_t isoMax = availableSensitivityRange.data.i32[1];
        g_availableSensitivityRange = availableSensitivityRange;

        String8 supportedIsoModes;
        supportedIsoModes += ISO_AUTO;
        if (100 > isoMin && 100 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_100;
        }
        if (200 > isoMin && 200 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_200;
        }
        if (400 > isoMin && 400 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_400;
        }
        if (800 > isoMin && 800 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_800;
        }
        if (1600 > isoMin && 1600 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_1600;
        }
        if (3200 > isoMin && 3200 <= isoMax) {
            supportedIsoModes += ",";
            supportedIsoModes += ISO_3200;
        }
        ParentParams->params.set(KEY_QTI_SUPPORTED_ISO_MODES,
                supportedIsoModes);
        // Set default value
        ParentParams->params.set(KEY_QTI_ISO_MODE,
                ISO_AUTO);
    }

    //Sharpness
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_SHARPNESS_RANGE, vTags.get(), &tag);
    camera_metadata_ro_entry_t availableSharpnessRange = ParentParams->staticInfo(tag);
    if (availableSharpnessRange.count == 2) {
        ParentParams->params.set(KEY_QTI_MAX_SHARPNESS,availableSharpnessRange.data.i32[1]);
        //Default value
        ParentParams->params.set(KEY_QTI_SHARPNESS,availableSharpnessRange.data.i32[1]);
    }

    //Saturation
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_SATURATION_RANGE, vTags.get(), &tag);
    camera_metadata_ro_entry_t availableSaturationRange =
            ParentParams->staticInfo(tag);
    if (availableSaturationRange.count == 4) {
        ParentParams->params.set(KEY_QTI_MAX_SATURATION,availableSaturationRange.data.i32[1]);
        //Default value
        ParentParams->params.set(KEY_QTI_SATURATION,availableSaturationRange.data.i32[2]);
    }

    //Exposure Metering
    tag=0;
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_EXPOSURE_METER_MODES, vTags.get(), &tag);
    camera_metadata_ro_entry_t availableMeteringModes =
            ParentParams->staticInfo(tag);

    String8 MeteringModes;
    for(int meterModes=0;meterModes<(int)availableMeteringModes.count;meterModes++) {
        if((availableMeteringModes.data.i32[meterModes] < 0) ||
                (availableMeteringModes.data.i32[meterModes] > 6))
            continue;

        if(meterModes != 0) {
            MeteringModes += ",";
        }

        if(availableMeteringModes.data.i32[meterModes] == 0 ) {
            MeteringModes += AUTO_EXPOSURE_FRAME_AVG;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 1 ) {
            MeteringModes += AUTO_EXPOSURE_CENTER_WEIGHTED;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 2 ) {
            MeteringModes += AUTO_EXPOSURE_SPOT_METERING;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 3 ) {
            MeteringModes += AUTO_EXPOSURE_SMART_METERING;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 4 ) {
            MeteringModes += AUTO_EXPOSURE_USER_METERING;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 5 ) {
            MeteringModes += AUTO_EXPOSURE_SPOT_METERING_ADV;
        }
        else if(availableMeteringModes.data.i32[meterModes] == 6 ) {
            MeteringModes += AUTO_EXPOSURE_CENTER_WEIGHTED_ADV;
        }
    }

    ParentParams->params.set(KEY_QTI_AUTO_EXPOSURE_VALUES,
                    MeteringModes);

    ParentParams->params.set(KEY_QTI_AUTO_EXPOSURE,
                    AUTO_EXPOSURE_FRAME_AVG);

    //Instant AEC
    tag=0;
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_INSTANT_MODES, vTags.get(), &tag);
    camera_metadata_ro_entry_t availableInstantAecModes =
            ParentParams->staticInfo(tag);
    String8 instantAecModes;
    for(int aecModes=0;aecModes<(int)availableInstantAecModes.count;aecModes++) {
        if((availableInstantAecModes.data.i32[aecModes] < 0) ||
                (availableInstantAecModes.data.i32[aecModes] > 2))
            continue;

        if(aecModes != 0) {
            instantAecModes += ",";
        }

        if(availableInstantAecModes.data.i32[aecModes] == 0) {
            instantAecModes += KEY_QTI_INSTANT_AEC_DISABLE;
        } else if(availableInstantAecModes.data.i32[aecModes] == 1) {
            instantAecModes += KEY_QTI_INSTANT_AEC_AGGRESSIVE_AEC;
        } else if(availableInstantAecModes.data.i32[aecModes] == 2) {
            instantAecModes += KEY_QTI_INSTANT_AEC_FAST_AEC;
        }
    }
    if (availableInstantAecModes.count > 0) {
        ParentParams->params.set(KEY_QTI_INSTANT_AEC_SUPPORTED_MODES, instantAecModes);
        //default Instance AEC
        ParentParams->params.set(KEY_QTI_INSTANT_AEC, KEY_QTI_INSTANT_AEC_DISABLE);
    }

    //Manual Exposure
    String8 manualExpModes(VALUE_OFF);
    manualExpModes += ",";
    manualExpModes += KEY_QTI_EXP_TIME_PRIORITY;
    manualExpModes += ",";
    manualExpModes += KEY_QTI_ISO_PRIORITY;
    manualExpModes += ",";
    manualExpModes += KEY_QTI_USER_SETTING;

    if (availableSensitivityRange.count == 2) {
        ParentParams->params.set(KEY_QTI_MIN_ISO,availableSensitivityRange.data.i32[0]);
        ParentParams->params.set(KEY_QTI_MAX_ISO,availableSensitivityRange.data.i32[1]);
    }

    tag=0;
    camera_metadata_ro_entry_t availableExposureTimeRange =
            ParentParams->staticInfo(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE);
    if (availableExposureTimeRange.count == 2) {
        char expTimeStr[30];
        //values are in nano sec, convert to milli sec for upper layers
        minExposureTime = (double) availableExposureTimeRange.data.i64[0] / 1000000.0;
        maxExposureTime = (double) availableExposureTimeRange.data.i64[1] / 1000000.0;
        snprintf(expTimeStr, sizeof(expTimeStr), "%f", minExposureTime);
        ParentParams->params.set(KEY_QTI_MIN_EXPOSURE_TIME,expTimeStr);
        snprintf(expTimeStr, sizeof(expTimeStr), "%f", maxExposureTime);
        ParentParams->params.set(KEY_QTI_MAX_EXPOSURE_TIME,expTimeStr);
        ParentParams->params.set(KEY_QTI_SUPPORTED_MANUAL_EXPOSURE_MODES,manualExpModes.string());
    }

    //Manual White Balance
    String8 supportedWbModes;
    const char *awbModes= ParentParams->params.get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE);
    supportedWbModes += WHITE_BALANCE_MANUAL;
    supportedWbModes += ",";
    supportedWbModes += awbModes;
    ParentParams->params.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,
            supportedWbModes.string());

    String8 manualWbModes(VALUE_OFF);
    manualWbModes += ",";
    manualWbModes += KEY_QTI_WB_CCT_MODE;
    manualWbModes += ",";
    manualWbModes += KEY_QTI_WB_GAIN_MODE;
    ParentParams->params.set(KEY_QTI_MIN_WB_CCT,"2000");
    ParentParams->params.set(KEY_QTI_MAX_WB_CCT,"8000");
    ParentParams->params.set(KEY_QTI_MIN_WB_GAIN,minWbGain);
    ParentParams->params.set(KEY_QTI_MAX_WB_GAIN,maxWbGain);
    ParentParams->params.set(KEY_QTI_SUPPORTED_MANUAL_WB_MODES, manualWbModes.string());

    //Face detection
    String8 faceDetectionModes(VALUE_OFF);
    faceDetectionModes += ",";
    faceDetectionModes += VALUE_ON;
    ParentParams->params.set(KEY_QTI_FACE_DETECTION_MODES,faceDetectionModes.string());

    return res;
}

status_t QTIParameters::set(CameraParameters2& newParams) {
    status_t res = OK;
    char prop[PROPERTY_VALUE_MAX];

    // ISO
    const char *isoMode = newParams.get(KEY_QTI_ISO_MODE);
    if (isoMode) {
        if (!strcmp(isoMode, ISO_MANUAL)) {
            const char *str = newParams.get(KEY_QTI_CONTINUOUS_ISO);
            if (str != NULL) {
                res = setContinuousISO(str,newParams);
                if(res !=OK){
                    return res;
                }
            }
        } else if (!strcmp(isoMode, ISO_100)) {
            isoValue = 100;
        } else if (!strcmp(isoMode, ISO_200)) {
            isoValue = 200;
        } else if (!strcmp(isoMode, ISO_400)) {
            isoValue = 400;
        } else if (!strcmp(isoMode, ISO_800)) {
            isoValue = 800;
        } else if (!strcmp(isoMode, ISO_1600)) {
            isoValue = 1600;
        } else if (!strcmp(isoMode, ISO_3200)) {
            isoValue = 3200;
        } else {
            isoValue = 0;
        }
    }

    //exposure time
    const char *str = newParams.get(KEY_QTI_EXPOSURE_TIME);

    if (str != NULL) {
        res = setExposureTime(str,newParams);
        if(res !=OK){
            return res;
        }
    }

    //Sharpness value
    const char *sharpness=newParams.get(KEY_QTI_SHARPNESS);
    if(sharpness != NULL) {
        sharpnessValue= atoi(sharpness);
    }

    //Saturation
    const char *saturation=newParams.get(KEY_QTI_SATURATION);
    if(saturation != NULL) {
        saturationValue= atoi(saturation);
    }

    //Exposure Metering
    const char *exmeter=newParams.get(KEY_QTI_AUTO_EXPOSURE);
    if(!strcmp(exmeter,AUTO_EXPOSURE_FRAME_AVG)) {
        exposureMetering = 0;
    } else if (!strcmp(exmeter,AUTO_EXPOSURE_CENTER_WEIGHTED)) {
        exposureMetering = 1;
    } else if(!strcmp(exmeter,AUTO_EXPOSURE_SPOT_METERING)) {
        exposureMetering = 2;
    } else if(!strcmp(exmeter,AUTO_EXPOSURE_SMART_METERING)) {
        exposureMetering = 3;
    } else if(!strcmp(exmeter,AUTO_EXPOSURE_USER_METERING)) {
        exposureMetering = 4;
    } else if(!strcmp(exmeter,AUTO_EXPOSURE_SPOT_METERING_ADV)) {
        exposureMetering = 5;
    } else if(!strcmp(exmeter,AUTO_EXPOSURE_CENTER_WEIGHTED_ADV)) {
        exposureMetering = 6;
    }

    //Instant AEC
    const char *instantAec=newParams.get(KEY_QTI_INSTANT_AEC);
    if(instantAec != NULL) {
        instantAecValue= atoi(instantAec);
    } else {
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.instant.aec", prop, "0");
        instantAecValue= (int32_t)atoi(prop);
    }

    //Manual White Balance
    const char *whiteBalance = newParams.get(KEY_WHITE_BALANCE);
    if(whiteBalance) {
        if (!strcmp(whiteBalance, WHITE_BALANCE_MANUAL)) {
            const char *value = newParams.get(KEY_QTI_MANUAL_WB_VALUE);
            const char *type = newParams.get(KEY_QTI_MANUAL_WB_TYPE);
            if ((value != NULL) && (type != NULL)) {
                newParams.set(KEY_QTI_MANUAL_WB_TYPE, type);
                newParams.set(KEY_QTI_MANUAL_WB_VALUE, value);
                int32_t wbType = atoi(type);

                if (wbType == CAM_MANUAL_WB_MODE_GAIN) {
                    res = setManualWBGains(value,newParams);
                    if(res != OK) {
                        return res;
                    }
                } else {
                    res = BAD_VALUE;
                }
            }
        }
    }

    //redeye-reduction
    if(!strcmp(newParams.get(KEY_QTI_REDEYE_REDUCTION),"enable")) {
        flashMode = (flashMode_t)Parameters::FLASH_MODE_RED_EYE;
        newParams.set(CameraParameters::KEY_FLASH_MODE,flashModeEnumToString(flashMode));
    }
    else {
        flashMode = (flashMode_t)Parameters::FLASH_MODE_INVALID;
    }

    return res;
}

const char *QTIParameters::flashModeEnumToString(flashMode_t flashMode) {
    switch (flashMode) {
        case FLASH_MODE_RED_EYE:
            return CameraParameters::FLASH_MODE_RED_EYE;
        default:
            ALOGE("%s: Unknown flash mode enum %d",
                    __FUNCTION__, flashMode);
            return "unknown";
    }
}


int QTIParameters::wbModeStringToEnum(const char *wbMode) {
    return
        !strcmp(wbMode, WHITE_BALANCE_MANUAL) ?
            ANDROID_CONTROL_AWB_MODE_OFF :
        -1;
}

const char* QTIParameters::wbModeEnumToString(uint8_t wbMode) {
    switch (wbMode) {
        case ANDROID_CONTROL_AWB_MODE_OFF:
            return WHITE_BALANCE_MANUAL;
        default:
            ALOGE("%s: Unknown wb mode enum %d",
                    __FUNCTION__, wbMode);
            return "unknown";
    }
}

status_t QTIParameters::updateRequest(CameraMetadata *request) const {
    status_t res = OK;
    uint32_t tag = 0;
    int64_t isoVal;
    sp<VendorTagDescriptor> vTags =
        VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if ((nullptr == vTags.get()) || (0 >= vTags->getTagCount())) {
        sp<VendorTagDescriptorCache> cache =
                VendorTagDescriptorCache::getGlobalVendorTagCache();
        if (cache.get()) {
            cache->getVendorTagDescriptor(mVendorTagId, &vTags);
        }
    }

    if (!request) {
       return BAD_VALUE;
    }

    if (isoValue != -1) {
        int32_t selectPriority = 0; // 0 for iso, 1 for exp.
        isoVal = isoValue;

        res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_ISO_EXP_SELECT_PRIORITY,
                vTags.get(), &tag);
        res = request->update(tag, &selectPriority, 1);
        res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_ISO_EXP_USE_VALUE, vTags.get(), &tag);
        res = request->update(tag, &(isoVal),  1);
        if (res != OK) {
            return res;
        }

        //erase the default value of construct_default_setting.
        res = request->erase(ANDROID_SENSOR_SENSITIVITY);
        if (res != OK) {
            return res;
        }
        res = request->erase(ANDROID_SENSOR_EXPOSURE_TIME);
        if (res != OK) {
            return res;
        }

    }

    if (exposureTime > 0) {
        int32_t selectPriority = 1; // 0 for iso, 1 for exp.
        res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_ISO_EXP_SELECT_PRIORITY,
                vTags.get(), &tag);
        res = request->update(tag, &selectPriority, 1);
        res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_ISO_EXP_USE_VALUE, vTags.get(), &tag);
        res = request->update(tag, &(exposureTime),  1);
        if (res != OK) {
            return res;
        }

        //erase the default value of construct_default_setting.
        res = request->erase(ANDROID_SENSOR_SENSITIVITY);
        if (res != OK) {
            return res;
        }
        res = request->erase(ANDROID_SENSOR_EXPOSURE_TIME);
        if (res != OK) {
            return res;
        }
    }

    //Sharpness value
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_SHARPNESS_STRENGTH, vTags.get(), &tag);
    res = request->update(tag,&sharpnessValue, 1);
    if (res != OK) {
        return res;
    }

    //Saturation value
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_SATURATION, vTags.get(), &tag);
    res = request->update(tag,&saturationValue, 1);
    if (res != OK) {
        return res;
    }

    //Exposure Metering
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_EXPOSURE_METER, vTags.get(), &tag);
    res = request->update(tag,&exposureMetering, 1);
    if (res != OK) {
        return res;
    }

    //Instant AEC
    res = CameraMetadata::getTagFromName(KEY_QTI_VENDOR_INSTANT_MODE, vTags.get(), &tag);
    res = request->update(tag,&instantAecValue, 1);
    if (res != OK) {
        return res;
    }

    //Color Correction gains
    res = request->update(ANDROID_COLOR_CORRECTION_GAINS,(float *)&(manualWb.gains),4);
    if (res != OK) {
        return res;
    }

    //redeye-reduction
    if(flashMode == (flashMode_t)Parameters::FLASH_MODE_RED_EYE) {
        uint8_t reqFlashMode = ANDROID_FLASH_MODE_OFF;
        uint8_t reqAeMode = flashMode;

        res = request->update(ANDROID_FLASH_MODE, &reqFlashMode, 1);
        if (res != OK) return res;
        res = request->update(ANDROID_CONTROL_AE_MODE, &reqAeMode, 1);
        if (res != OK) return res;
    }

    return res;
}

int32_t QTIParameters::setManualWBGains(const char *gainStr, CameraParameters2& newParams)
{
    int32_t res = OK;
    if (gainStr != NULL) {
        double rGain,gGain,bGain;
        res = parseGains(gainStr, rGain, gGain, bGain);
        if (res != OK) {
            return res;
        }

        double minGain = atof(minWbGain);
        double maxGain = atof(maxWbGain);

        if (rGain >= minGain && rGain <= maxGain &&
                gGain >= minGain && gGain <= maxGain &&
                bGain >= minGain && bGain <= maxGain) {
            newParams.set(KEY_QTI_MANUAL_WB_GAINS, gainStr);

            manualWb.type = CAM_MANUAL_WB_MODE_GAIN;
            manualWb.gains.rGain = rGain;
            manualWb.gains.gEvenGain = gGain;
            manualWb.gains.gOddGain = gGain;
            manualWb.gains.bGain = bGain;
            return res;
        }
        return BAD_VALUE;
    }
    return BAD_VALUE;
}

int32_t QTIParameters::parseGains(const char *gainStr, double &rGain,
                                          double &gGain, double &bGain)
{
    int32_t res = OK;
    char *saveptr = NULL;
    size_t gainsSize = strlen(gainStr) + 1;
    char* gains = (char*) calloc(1, gainsSize);
    if (NULL == gains) {
        ALOGE("No memory for gains");
        return NO_MEMORY;
    }
    strlcpy(gains, gainStr, gainsSize);
    char *token = strtok_r(gains, ",", &saveptr);

    if (NULL != token) {
        rGain = (float) atof(token);
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (NULL != token) {
        gGain = (float) atof(token);
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (NULL != token) {
        bGain = (float) atof(token);
    } else {
        ALOGE("Malformed string for gains");
        res = BAD_VALUE;
    }

    free(gains);
    return res;
}


int32_t  QTIParameters::setExposureTime(const char *expTimeStr, CameraParameters2& newParams)
{
    double expTimeMs = atof(expTimeStr);
    //input is in milli seconds. Convert to nano sec
    int64_t expTimeNs = (int64_t)(expTimeMs*1000000L);

    // expTime == 0 means not to use manual exposure time.
    if ((0 <= expTimeMs) &&
            ((expTimeMs == 0) ||
            ((expTimeMs >= (int64_t) minExposureTime) &&
            (expTimeMs <= (int64_t) maxExposureTime)))) {
        newParams.set(KEY_QTI_EXPOSURE_TIME, expTimeStr);
        exposureTime = expTimeNs;

        return OK;
    }
    return BAD_VALUE;
}

int32_t  QTIParameters::setContinuousISO(const char *isoVal, CameraParameters2& newParams)
{
    char iso[PROPERTY_VALUE_MAX];
    int32_t continousIso = 0;

    // Check if continuous ISO is set through setproperty
    property_get("persist.camera.continuous.iso", iso, "");
    if (strlen(iso) > 0) {
        continousIso = atoi(iso);
    } else {
        continousIso = atoi(isoVal);
    }

    if ((continousIso >= 0) &&
            (continousIso <= g_availableSensitivityRange.data.i32[1])) {
        newParams.set(KEY_QTI_CONTINUOUS_ISO, isoVal);
        isoValue = continousIso;
        return OK;
    }
    ALOGE("Invalid iso value: %d", continousIso);
    return BAD_VALUE;
}

}; // namespace camera2
}; // namespace android

