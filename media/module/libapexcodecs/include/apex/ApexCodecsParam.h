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

#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

#include <android/api-level.h>
#include <android/versioning.h>

__BEGIN_DECLS

/**
 * Enums and types that represent parameters in ApexCodecs.
 *
 * NOTE: Many of the constants and types mirror the ones in the Codec 2.0 API.
 */

/**
 * Enum that represents the query type for the supported values.
 *
 * Introduced in API 36.
 */
typedef enum ApexCodec_SupportedValuesQueryType : uint32_t {
    /** Query all possible supported values regardless of current configuration */
    APEXCODEC_SUPPORTED_VALUES_QUERY_POSSIBLE,
    /** Query supported values at current configuration */
    APEXCODEC_SUPPORTED_VALUES_QUERY_CURRENT,
} ApexCodec_SupportedValuesQueryType;

/**
 * Enum that represents the type of the supported values.
 *
 * Introduced in API 36.
 */
typedef enum ApexCodec_SupportedValuesType : uint32_t {
    /** The supported values are empty. */
    APEXCODEC_SUPPORTED_VALUES_EMPTY,
    /**
     * The supported values are represented by a range defined with {min, max, step, num, den}.
     *
     * If step is 0 and num and denom are both 1, the supported values are any value, for which
     * min <= value <= max.
     *
     * Otherwise, the range represents a geometric/arithmetic/multiply-accumulate series, where
     * successive supported values can be derived from previous values (starting at min), using the
     * following formula:
     *  v[0] = min
     *  v[i] = v[i-1] * num / denom + step for i >= 1, while min < v[i] <= max.
     */
    APEXCODEC_SUPPORTED_VALUES_RANGE,
    /** The supported values are represented by a list of values. */
    APEXCODEC_SUPPORTED_VALUES_VALUES,
    /** The supported values are represented by a list of flags. */
    APEXCODEC_SUPPORTED_VALUES_FLAGS,
} ApexCodec_SupportedValuesType;

/**
 * Enum that represents numeric types of the supported values.
 *
 * Introduced in API 36.
 */
typedef enum ApexCodec_SupportedValuesNumberType : uint32_t {
    APEXCODEC_SUPPORTED_VALUES_TYPE_NONE   = 0,
    APEXCODEC_SUPPORTED_VALUES_TYPE_INT32  = 1,
    APEXCODEC_SUPPORTED_VALUES_TYPE_UINT32 = 2,
    // RESERVED                            = 3,
    APEXCODEC_SUPPORTED_VALUES_TYPE_INT64  = 4,
    APEXCODEC_SUPPORTED_VALUES_TYPE_UINT64 = 5,
    // RESERVED                            = 6,
    APEXCODEC_SUPPORTED_VALUES_TYPE_FLOAT  = 7,
} ApexCodec_SupportedValuesNumberType;

/**
 * Union of primitive types.
 *
 * Introduced in API 36.
 */
typedef union {
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f;
} ApexCodec_Value;

/**
 * Enum that represents the failure code of ApexCodec_SettingResults.
 *
 * Introduced in API 36.
 */
typedef enum ApexCodec_SettingResultFailure : uint32_t {
    /** parameter type is not supported */
    APEXCODEC_SETTING_RESULT_BAD_TYPE,
    /** parameter is not supported on the specific port */
    APEXCODEC_SETTING_RESULT_BAD_PORT,
    /** parameter is not supported on the specific stream */
    APEXCODEC_SETTING_RESULT_BAD_INDEX,
    /** parameter is read-only */
    APEXCODEC_SETTING_RESULT_READ_ONLY,
    /** parameter mismatches input data */
    APEXCODEC_SETTING_RESULT_MISMATCH,
    /** strict parameter does not accept value for the field at all */
    APEXCODEC_SETTING_RESULT_BAD_VALUE,
    /** strict parameter field value conflicts with another settings */
    APEXCODEC_SETTING_RESULT_CONFLICT,
    /** strict parameter field is out of range due to other settings */
    APEXCODEC_SETTING_RESULT_UNSUPPORTED,
    /**
     * field does not accept the requested parameter value at all. It has been corrected to
     * the closest supported value. This failure mode is provided to give guidance as to what
     * are the currently supported values for this field (which may be a subset of the at-all-
     * potential values)
     */
    APEXCODEC_SETTING_RESULT_INFO_BAD_VALUE,
    /**
     * requested parameter value is in conflict with an/other setting(s)
     * and has been corrected to the closest supported value. This failure
     * mode is given to provide guidance as to what are the currently supported values as well
     * as to optionally provide suggestion to the client as to how to enable the requested
     * parameter value.
     */
    APEXCODEC_SETTING_RESULT_INFO_CONFLICT,
} ApexCodec_SettingResultFailure;

/* forward-declaration for an opaque struct */
struct ApexCodec_SupportedValues;

/**
 * Struct that represents a field and its supported values of a parameter.
 *
 * The offset and size of the field are where the field is located in the blob representation of
 * the parameter, as used in the ApexCodec_Configurable_query() and ApexCodec_Configurable_config(),
 * for example.
 *
 * Introduced in API 36.
 */
typedef struct ApexCodec_ParamFieldValues {
    /** index of the param */
    uint32_t index;
    /** offset of the param field */
    uint32_t offset;
    /** size of the param field */
    uint32_t size;
    /** currently supported values of the param field */
    struct ApexCodec_SupportedValues *_Nullable values;
} ApexCodec_ParamFieldValues;

/**
 * Enum that represents the attributes of a parameter.
 *
 * Introduced in API 36.
 */
typedef enum ApexCodec_ParamAttribute : uint32_t {
    /** parameter is required to be specified */
    APEXCODEC_PARAM_IS_REQUIRED   = 1u << 0,
    /** parameter retains its value */
    APEXCODEC_PARAM_IS_PERSISTENT = 1u << 1,
    /** parameter is strict */
    APEXCODEC_PARAM_IS_STRICT     = 1u << 2,
    /**
     * parameter is read-only; the value may change if other parameters are changed,
     * but the client cannot modify the value directly.
     */
    APEXCODEC_PARAM_IS_READ_ONLY  = 1u << 3,
    /** parameter shall not be visible to clients */
    APEXCODEC_PARAM_IS_HIDDEN     = 1u << 4,
    /** parameter shall not be used by framework (other than testing) */
    APEXCODEC_PARAM_IS_INTERNAL   = 1u << 5,
    /**
     * parameter is publicly const (hence read-only); the parameter never changes.
     */
    APEXCODEC_PARAM_IS_CONSTANT   = 1u << 6 | APEXCODEC_PARAM_IS_READ_ONLY,
} ApexCodec_ParamAttribute;

__END_DECLS