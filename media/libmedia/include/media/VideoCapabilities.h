/*
 * Copyright 2024, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VIDEO_CAPABILITIES_H_

#define VIDEO_CAPABILITIES_H_

#include <media/CodecCapabilitiesUtils.h>
#include <media/stagefright/foundation/AMessage.h>

#include <utils/StrongPointer.h>

namespace android {

struct VideoCapabilities {
    struct PerformancePoint {
        /**
         * Maximum number of macroblocks in the frame.
         *
         * Video frames are conceptually divided into 16-by-16 pixel blocks called macroblocks.
         * Most coding standards operate on these 16-by-16 pixel blocks; thus, codec performance
         * is characterized using such blocks.
         *
         * Test API
         */
        int32_t getMaxMacroBlocks() const;

        /**
         * Return the width.
         *
         * For internal use only.
         */
        int32_t getWidth() const;

        /**
         * Return the height.
         *
         * For internal use only.
         */
        int32_t getHeight() const;

        /**
         * Maximum frame rate in frames per second.
         *
         * Test API
         */
        int32_t getMaxFrameRate() const;

        /**
         * Maximum number of macroblocks processed per second.
         *
         * Test API
         */
        int64_t getMaxMacroBlockRate() const;

        /**
         * Return the block size.
         *
         * For internal use only.
         */
        VideoSize getBlockSize() const;

        /**
         * convert to a debug string
         *
         * Be careful about the serializable compatibility across API revisions.
         */
        std::string toString() const;

        /**
         * Create a detailed performance point with custom max frame rate and macroblock size.
         *
         * @param width  frame width in pixels
         * @param height frame height in pixels
         * @param frameRate frames per second for frame width and height
         * @param maxFrameRate maximum frames per second for any frame size
         * @param blockSize block size for codec implementation. Must be powers of two in both
         *        width and height.
         *
         * Test API
         */
        PerformancePoint(int32_t width, int32_t height, int32_t frameRate, int32_t maxFrameRate,
                VideoSize blockSize);

        /**
         * Create a detailed performance point with custom max frame rate and macroblock size.
         *
         * @param width  frame width in pixels
         * @param height frame height in pixels
         * @param maxFrameRate maximum frames per second for any frame size
         * @param maxMacroBlockRate maximum number of macroblocks processed per second.
         * @param blockSize block size for codec implementation. Must be powers of two in both
         *        width and height.
         *
         * Test API
         */
        PerformancePoint(VideoSize blockSize, int32_t width, int32_t height, int maxFrameRate,
                int64_t maxMacroBlockRate);

        /**
         * Convert a performance point to a larger blocksize.
         *
         * @param pp performance point. NonNull
         * @param blockSize block size for codec implementation. NonNull.
         *
         * Test API
         */
        PerformancePoint(const PerformancePoint &pp, VideoSize newBlockSize);

        /**
         * Create a performance point for a given frame size and frame rate.
         *
         * @param width width of the frame in pixels
         * @param height height of the frame in pixels
         * @param frameRate frame rate in frames per second
         */
        PerformancePoint(int32_t width, int32_t height, int32_t frameRate);

        /**
         * Checks whether the performance point covers a media format.
         *
         * @param format Stream format considered
         *
         * @return {@code true} if the performance point covers the format.
         */
        bool covers(const sp<AMessage> &format) const;

        /**
         * Checks whether the performance point covers another performance point. Use this
         * method to determine if a performance point advertised by a codec covers the
         * performance point required. This method can also be used for loose ordering as this
         * method is transitive.
         *
         * @param other other performance point considered
         *
         * @return {@code true} if the performance point covers the other.
         */
        bool covers(const PerformancePoint &other) const;

        /**
         * Check if two PerformancePoint instances are equal.
         *
         * @param other other PerformancePoint instance for comparison.
         *
         * @return true if two PerformancePoint are equal.
         */
        bool equals(const PerformancePoint &other) const;

    private:
        VideoSize mBlockSize; // codec block size in macroblocks
        int32_t mWidth; // width in macroblocks
        int32_t mHeight; // height in macroblocks
        int32_t mMaxFrameRate; // max frames per second
        int64_t mMaxMacroBlockRate; // max macro block rate

        void init(int32_t width, int32_t height, int32_t frameRate, int32_t maxFrameRate,
                VideoSize blockSize);

        /** Saturates a 64 bit integer value to a 32 bit integer */
        int32_t saturateInt64ToInt32(int64_t value) const;

        /** This method may overflow */
        int32_t align(int32_t value, int32_t alignment) const;

        /** @return NonNull */
        VideoSize getCommonBlockSize(const PerformancePoint &other) const;
    };

    /**
     * Find the equivalent VP9 profile level.
     *
     * Not a public API to developers.
     */
    static int32_t EquivalentVP9Level(const sp<AMessage> &format);

    /**
     * Returns the range of supported bitrates in bits/second.
     */
    const Range<int32_t>& getBitrateRange() const;

    /**
     * Returns the range of supported video widths.
     * 32-bit processes will not support resolutions larger than 4096x4096 due to
     * the limited address space.
     */
    const Range<int32_t>& getSupportedWidths() const;

    /**
     * Returns the range of supported video heights.
     * 32-bit processes will not support resolutions larger than 4096x4096 due to
     * the limited address space.
     */
    const Range<int32_t>& getSupportedHeights() const;

    /**
     * Returns the alignment requirement for video width (in pixels).
     *
     * This is a power-of-2 value that video width must be a
     * multiple of.
     */
    int32_t getWidthAlignment() const;

    /**
     * Returns the alignment requirement for video height (in pixels).
     *
     * This is a power-of-2 value that video height must be a
     * multiple of.
     */
    int32_t getHeightAlignment() const;

    /**
     * Return the upper limit on the smaller dimension of width or height.
     *
     * Some codecs have a limit on the smaller dimension, whether it be
     * the width or the height.  E.g. a codec may only be able to handle
     * up to 1920x1080 both in landscape and portrait mode (1080x1920).
     * In this case the maximum width and height are both 1920, but the
     * smaller dimension limit will be 1080. For other codecs, this is
     * {@code Math.min(getSupportedWidths().getUpper(),
     * getSupportedHeights().getUpper())}.
     */
    int32_t getSmallerDimensionUpperLimit() const;

    /**
     * Returns the range of supported frame rates.
     *
     * This is not a performance indicator.  Rather, it expresses the
     * limits specified in the coding standard, based on the complexities
     * of encoding material for later playback at a certain frame rate,
     * or the decoding of such material in non-realtime.
     */
    const Range<int32_t>& getSupportedFrameRates() const;

    /**
     * Returns the range of supported video widths for a video height.
     * @param height the height of the video
     */
    std::optional<Range<int32_t>> getSupportedWidthsFor(int32_t height) const;

    /**
     * Returns the range of supported video heights for a video width
     * @param width the width of the video
     */
    std::optional<Range<int32_t>> getSupportedHeightsFor(int32_t width) const;

    /**
     * Returns the range of supported video frame rates for a video size.
     *
     * This is not a performance indicator.  Rather, it expresses the limits specified in
     * the coding standard, based on the complexities of encoding material of a given
     * size for later playback at a certain frame rate, or the decoding of such material
     * in non-realtime.

     * @param width the width of the video
     * @param height the height of the video
     */
    std::optional<Range<double>> getSupportedFrameRatesFor(int32_t width, int32_t height) const;

    /**
     * Returns the range of achievable video frame rates for a video size.
     * May return {@code null}, if the codec did not publish any measurement
     * data.
     * <p>
     * This is a performance estimate provided by the device manufacturer based on statistical
     * sampling of full-speed decoding and encoding measurements in various configurations
     * of common video sizes supported by the codec. As such it should only be used to
     * compare individual codecs on the device. The value is not suitable for comparing
     * different devices or even different android releases for the same device.
     * <p>
     * <em>On {@link android.os.Build.VERSION_CODES#M} release</em> the returned range
     * corresponds to the fastest frame rates achieved in the tested configurations. As
     * such, it should not be used to gauge guaranteed or even average codec performance
     * on the device.
     * <p>
     * <em>On {@link android.os.Build.VERSION_CODES#N} release</em> the returned range
     * corresponds closer to sustained performance <em>in tested configurations</em>.
     * One can expect to achieve sustained performance higher than the lower limit more than
     * 50% of the time, and higher than half of the lower limit at least 90% of the time
     * <em>in tested configurations</em>.
     * Conversely, one can expect performance lower than twice the upper limit at least
     * 90% of the time.
     * <p class=note>
     * Tested configurations use a single active codec. For use cases where multiple
     * codecs are active, applications can expect lower and in most cases significantly lower
     * performance.
     * <p class=note>
     * The returned range value is interpolated from the nearest frame size(s) tested.
     * Codec performance is severely impacted by other activity on the device as well
     * as environmental factors (such as battery level, temperature or power source), and can
     * vary significantly even in a steady environment.
     * <p class=note>
     * Use this method in cases where only codec performance matters, e.g. to evaluate if
     * a codec has any chance of meeting a performance target. Codecs are listed
     * in {@link MediaCodecList} in the preferred order as defined by the device
     * manufacturer. As such, applications should use the first suitable codec in the
     * list to achieve the best balance between power use and performance.
     *
     * @param width the width of the video
     * @param height the height of the video
     */
    std::optional<Range<double>> getAchievableFrameRatesFor(int32_t width, int32_t height) const;

    /**
     * Returns the supported performance points. May return {@code null} if the codec did not
     * publish any performance point information (e.g. the vendor codecs have not been updated
     * to the latest android release). May return an empty list if the codec published that
     * if does not guarantee any performance points.
     * <p>
     * This is a performance guarantee provided by the device manufacturer for hardware codecs
     * based on hardware capabilities of the device.
     * <p>
     * The returned list is sorted first by decreasing number of pixels, then by decreasing
     * width, and finally by decreasing frame rate.
     * Performance points assume a single active codec. For use cases where multiple
     * codecs are active, should use that highest pixel count, and add the frame rates of
     * each individual codec.
     * <p class=note>
     * 32-bit processes will not support resolutions larger than 4096x4096 due to
     * the limited address space, but performance points will be presented as is.
     * In other words, even though a component publishes a performance point for
     * a resolution higher than 4096x4096, it does not mean that the resolution is supported
     * for 32-bit processes.
     */
    const std::vector<PerformancePoint>& getSupportedPerformancePoints() const;

    /**
     * Returns whether a given video size ({@code width} and
     * {@code height}) and {@code frameRate} combination is supported.
     */
    bool areSizeAndRateSupported(int32_t width, int32_t height, double frameRate) const;

    /**
     * Returns whether a given video size ({@code width} and
     * {@code height}) is supported.
     */
    bool isSizeSupported(int32_t width, int32_t height) const;

    /**
     * Returns if a media format is supported.
     *
     * Not exposed to public
     */
    bool supportsFormat(const sp<AMessage> &format) const;

    /**
     * Create VideoCapabilities.
     */
    static std::shared_ptr<VideoCapabilities> Create(std::string mediaType,
            std::vector<ProfileLevel> profLevs, const sp<AMessage> &format);

    /**
     * Get the block size.
     *
     * Not a public API to developers
     */
    VideoSize getBlockSize() const;

    /**
     * Get the block count range.
     *
     * Not a public API to developers
     */
    const Range<int32_t>& getBlockCountRange() const;

    /**
     * Get the blocks per second range.
     *
     * Not a public API to developers
     */
    const Range<int64_t>& getBlocksPerSecondRange() const;

    /**
     * Get the aspect ratio range.
     *
     * Not a public API to developers
     */
    Range<Rational> getAspectRatioRange(bool blocks) const;

    /**
     * Query if the given video size and frame rate combination is supported.
     *
     * with, height and framerate are optional.
     *
     * Not a public API to developers.
     */
    bool supports(std::optional<int32_t> width, std::optional<int32_t> height,
            std::optional<double> rate) const;

private:
    std::string mMediaType;
    std::vector<ProfileLevel> mProfileLevels;
    int mError;

    Range<int32_t> mBitrateRange;
    Range<int32_t> mHeightRange;
    Range<int32_t> mWidthRange;
    Range<int32_t> mBlockCountRange;
    Range<int32_t> mHorizontalBlockRange;
    Range<int32_t> mVerticalBlockRange;
    Range<Rational> mAspectRatioRange;
    Range<Rational> mBlockAspectRatioRange;
    Range<int64_t> mBlocksPerSecondRange;
    std::map<VideoSize, Range<int64_t>, VideoSizeCompare> mMeasuredFrameRates;
    std::vector<PerformancePoint> mPerformancePoints;
    Range<int32_t> mFrameRateRange;

    int32_t mBlockWidth;
    int32_t mBlockHeight;
    int32_t mWidthAlignment;
    int32_t mHeightAlignment;
    int mSmallerDimensionUpperLimit;

    bool mAllowMbOverride; // allow XML to override calculated limits

    int32_t getBlockCount(int32_t width, int32_t height) const;
    std::optional<VideoSize> findClosestSize(int32_t width, int32_t height) const;
    std::optional<Range<double>> estimateFrameRatesFor(int32_t width, int32_t height) const;

    /* no public constructor */
    VideoCapabilities() {}
    void init(std::string mediaType, std::vector<ProfileLevel> profLevs,
            const sp<AMessage> &format);
    void initWithPlatformLimits();
    std::vector<PerformancePoint> getPerformancePoints(const sp<AMessage> &format) const;
    std::map<VideoSize, Range<int64_t>, VideoSizeCompare>
            getMeasuredFrameRates(const sp<AMessage> &format) const;

    static std::optional<std::pair<Range<int32_t>, Range<int32_t>>> ParseWidthHeightRanges(
            const std::string &str);
    void parseFromInfo(const sp<AMessage> &format);
    void applyBlockLimits(int32_t blockWidth, int32_t blockHeight,
            Range<int32_t> counts, Range<int64_t> rates, Range<Rational> ratios);
    void applyAlignment(int32_t widthAlignment, int32_t heightAlignment);
    void updateLimits();
    void applyMacroBlockLimits(
            int32_t maxHorizontalBlocks, int32_t maxVerticalBlocks,
            int32_t maxBlocks, int64_t maxBlocksPerSecond,
            int32_t blockWidth, int32_t blockHeight,
            int32_t widthAlignment, int32_t heightAlignment);
    void applyMacroBlockLimits(
            int32_t minHorizontalBlocks, int32_t minVerticalBlocks,
            int32_t maxHorizontalBlocks, int32_t maxVerticalBlocks,
            int32_t maxBlocks, int64_t maxBlocksPerSecond,
            int32_t blockWidth, int32_t blockHeight,
            int32_t widthAlignment, int32_t heightAlignment);
    void applyLevelLimits();

    friend struct CodecCapabilities;
};

}  // namespace android

#endif // VIDEO_CAPABILITIES_H_