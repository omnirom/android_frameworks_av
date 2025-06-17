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

#ifndef _VNDK_IMAGE_READER_H
#define _VNDK_IMAGE_READER_H

#include <sys/cdefs.h>
// vndk is a superset of the NDK
#include <media/NdkImageReader.h>

__BEGIN_DECLS

/**
 * Set the usage of this image reader.
 *
 * <p>Note that calling this method will replace the previously set usage.</p>
 *
 * <p>Note: This will trigger re-allocation, could cause producer failures mid-stream
 * if the new usage combination isn't supported, and thus should be avoided as much as
 * possible regardless.</p>
 *
 * Available since API level 36.
 *
 * @param reader The image reader of interest.
 * @param usage specifies how the consumer will access the AImage.
 *              See {@link AImageReader_newWithUsage} parameter description for more details.
 * @return <ul>
 *         <li>{@link AMEDIA_OK} if the method call succeeds.</li>
 *         <li>{@link AMEDIA_ERROR_INVALID_PARAMETER} if reader is NULL.</li>
 *         <li>{@link AMEDIA_ERROR_UNKNOWN} if the method fails for some other reasons.</li></ul>
 *
 * @see AImage_getHardwareBuffer
 */
media_status_t AImageReader_setUsage(
        AImageReader* _Nonnull reader, uint64_t usage) __INTRODUCED_IN(36);

__END_DECLS

#endif //_VNDK_IMAGE_READER_H
