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

/**
 * This is the system APIs for AAudio.
 */
#ifndef SYSTEM_AAUDIO_H
#define SYSTEM_AAUDIO_H

#include <aaudio/AAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add one vendor extension tag that the output stream will carry.
 *
 * The total size of all added tags, plus one for each tag terminator, must not be greater than
 * <a href="/reference/android/system/media/audio">AUDIO_ATTRIBUTES_TAGS_MAX_SIZE</a>.
 *
 * The tag can be used by the audio policy engine for routing purpose.
 * Routing is based on audio attributes, translated into legacy stream type.
 * The stream types cannot be extended, so the product strategies have been introduced to allow
 * vendor extension of routing capabilities.
 * This could, for example, affect how volume and routing is handled for the stream.
 *
 * The tag can also be used by a System App to pass vendor specific information through the
 * framework to the HAL. That info could affect routing, ducking or other audio behavior in the HAL.
 *
 * By default, audio attributes tags are empty if this method is not called.
 *
 * When opening a stream with audio attributes tags, the client should hold
 * MODIFY_AUDIO_SETTINGS_PRIVILEGED permission. Otherwise, the stream will fail to open.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param tag the desired tag to add, which must be UTF-8 format and null-terminated.
 * @return {@link #AAUDIO_OK} on success or {@link #AAUDIO_ERROR_ILLEGAL_ARGUMENT} if the given
 *         tags is null or {@link #AAUDIO_ERROR_OUT_OF_RANGE} if there is not room for more tags.
 */
aaudio_result_t AAudioStreamBuilder_addTag(AAudioStreamBuilder* _Nonnull builder,
                                           const char* _Nonnull tag);

/**
 * Clear all the tags that has been added from calling
 * {@link #AAudioStreamBuilder_addTag}.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 */
void AAudioStreamBuilder_clearTags(AAudioStreamBuilder* _Nonnull builder);

/**
 * Allocate and read the audio attributes' tags for the stream into a buffer.
 * The client is responsible to free the memory for tags by calling
 * {@link #AAudioStream_destroyTags} unless the number of tags is 0.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param tags a pointer to a variable that will be set to a pointer to an array of char* pointers
 * @return number of tags or
 *         {@link #AAUDIO_ERROR_NO_MEMORY} if it fails to allocate memory for tags.
 */
int32_t AAudioStream_obtainTags(AAudioStream* _Nonnull stream,
                                char* _Nullable* _Nullable* _Nonnull tags);

/**
 * Free the memory containing the tags that is allocated when calling
 * {@link #AAudioStream_obtainTags}.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param tags reference provided by AAudioStream_obtainTags()
 */
void AAudioStream_destroyTags(AAudioStream* _Nonnull stream, char* _Nonnull * _Nullable tags);

#ifdef __cplusplus
}
#endif

#endif //SYSTEM_AAUDIO_H
