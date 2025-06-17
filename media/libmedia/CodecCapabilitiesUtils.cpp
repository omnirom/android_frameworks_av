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

//#define LOG_NDEBUG 0
#define LOG_TAG "CodecCapabilitiesUtils"

#include <android-base/properties.h>
#include <utils/Log.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <media/CodecCapabilitiesUtils.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AUtils.h>

namespace android {

static int32_t SaturateDoubleToInt32(double d) {
    if (d >= static_cast<double>(std::numeric_limits<int32_t>::max())) {
        return std::numeric_limits<int32_t>::max();
    } else if (d <= static_cast<double>(std::numeric_limits<int32_t>::min())) {
        return std::numeric_limits<int32_t>::min();
    } else {
        return static_cast<int32_t>(d);
    }
}

// VideoSize

VideoSize::VideoSize(int32_t width, int32_t height) : mWidth(width), mHeight(height) {}

VideoSize::VideoSize() : mWidth(0), mHeight(0) {}

int32_t VideoSize::getWidth() const { return mWidth; }

int32_t VideoSize::getHeight() const { return mHeight; }

bool VideoSize::equals(VideoSize other) const {
    return mWidth == other.mWidth && mHeight == other.mHeight;
}

bool VideoSize::empty() const {
    return mWidth <= 0 || mHeight <= 0;
}

std::string VideoSize::toString() const {
    return std::to_string(mWidth) + "x" + std::to_string(mHeight);
}

std::optional<VideoSize> VideoSize::ParseSize(std::string str) {
    if (str.empty()) {
        return std::nullopt;
    }

    std::regex regex("([0-9]+)([*x])([0-9]+)");
    std::smatch match;
    if (std::regex_match(str, match, regex)) {
        long int w = strtol(match[1].str().c_str(), NULL, 10);
        long int h = strtol(match[3].str().c_str(), NULL, 10);
        return std::make_optional(VideoSize(w, h));
    } else {
        ALOGW("could not parse size %s", str.c_str());
        return std::nullopt;
    }
}

std::optional<std::pair<VideoSize, VideoSize>> VideoSize::ParseSizeRange(const std::string str) {
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        std::optional<VideoSize> lowerOpt = VideoSize::ParseSize(str.substr(0, ix));
        std::optional<VideoSize> upperOpt = VideoSize::ParseSize(str.substr(ix + 1));
        if (!lowerOpt || !upperOpt) {
            return std::nullopt;
        }
        return std::make_optional(
                std::pair<VideoSize, VideoSize>(lowerOpt.value(), upperOpt.value()));
    } else {
        std::optional<VideoSize> opt = VideoSize::ParseSize(str);
        if (!opt) {
            return std::nullopt;
        }
        return std::make_optional(std::pair<VideoSize, VideoSize>(opt.value(), opt.value()));
    }
}

Range<int32_t> VideoSize::GetAllowedDimensionRange() {
#ifdef __LP64__
    return Range<int32_t>(1, 32768);
#else
    int32_t value = base::GetIntProperty("media.resolution.limit.32bit", (int32_t)4096);
    return Range<int32_t>(1, value);
#endif
}

// Rational

std::optional<Rational> Rational::Parse(std::string str) {
    if (str.compare("NaN") == 0) {
        return std::make_optional(NaN);
    } else if (str.compare("Infinity") == 0) {
        return std::make_optional(POSITIVE_INFINITY);
    } else if (str.compare("-Infinity") == 0) {
        return std::make_optional(NEGATIVE_INFINITY);
    }

    std::regex regex("([0-9]+)([:/])([0-9]+)");
    std::smatch match;
    if (std::regex_match(str, match, regex)) {
        long int numerator = strtol(match[1].str().c_str(), NULL, 10);
        long int denominator = strtol(match[3].str().c_str(), NULL, 10);
        return std::make_optional(Rational(numerator, denominator));
    } else {
        ALOGW("could not parse string: %s to Rational", str.c_str());
        return std::nullopt;
    }
}

Rational Rational::scale(int32_t num, int32_t den) {
    int32_t common = std::gcd(num, den);
    num /= common;
    den /= common;
    // ToDo: Reevaluate how to satureate double to int without drastically changing it.
    return Rational(
            SaturateDoubleToInt32(mNumerator * (double)num),
            SaturateDoubleToInt32(mDenominator * (double)den));
}

Range<Rational> Rational::ScaleRange(Range<Rational> range, int32_t num, int32_t den) {
    if (num == den) {
        return range;
    }
    return Range(
            range.lower().scale(num, den),
            range.upper().scale(num, den));
}

std::optional<Range<Rational>> Rational::ParseRange(const std::string str) {
    size_t ix = str.find_first_of('-');
    if (ix != std::string::npos) {
        std::optional<Rational> lower = Parse(str.substr(0, ix));
        std::optional<Rational> upper = Parse(str.substr(ix + 1));
        if (!lower || !upper) {
            return std::nullopt;
        }
        return std::make_optional<Range<Rational>>(lower.value(), upper.value());
    } else {
        std::optional<Rational> value = Parse(str);
        if (!value) {
            return std::nullopt;
        }
        return std::make_optional<Range<Rational>>(value.value(), value.value());
    }
}

}  // namespace android