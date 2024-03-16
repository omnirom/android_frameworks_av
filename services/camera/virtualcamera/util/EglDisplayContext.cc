/*
 * Copyright (C) 2023 The Android Open Source Project
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

// #define LOG_NDEBUG 0
#define LOG_TAG "EglDisplayContext"
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include "EglDisplayContext.h"

#include "EGL/egl.h"
#include "EglDisplayContext.h"
#include "EglFramebuffer.h"
#include "log/log.h"

namespace android {
namespace companion {
namespace virtualcamera {

EglDisplayContext::EglDisplayContext()
    : mEglDisplay(EGL_NO_DISPLAY),
      mEglContext(EGL_NO_CONTEXT),
      mEglConfig(nullptr) {
  EGLBoolean result;

  mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (mEglDisplay == EGL_NO_DISPLAY) {
    ALOGE("eglGetDisplay failed: %#x", eglGetError());
    return;
  }

  EGLint majorVersion, minorVersion;
  result = eglInitialize(mEglDisplay, &majorVersion, &minorVersion);
  if (result != EGL_TRUE) {
    ALOGE("eglInitialize failed: %#x", eglGetError());
    return;
  }
  ALOGV("Initialized EGL v%d.%d", majorVersion, minorVersion);

  EGLint numConfigs = 0;
  EGLint configAttribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      // no alpha
      EGL_NONE};

  result =
      eglChooseConfig(mEglDisplay, configAttribs, &mEglConfig, 1, &numConfigs);
  if (result != EGL_TRUE) {
    ALOGE("eglChooseConfig error: %#x", eglGetError());
    return;
  }

  EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_NONE};
  mEglContext =
      eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, contextAttribs);
  if (mEglContext == EGL_NO_CONTEXT) {
    ALOGE("eglCreateContext error: %#x", eglGetError());
    return;
  }

  if (!makeCurrent()) {
    ALOGE(
        "Failed to set newly initialized EGLContext and EGLDisplay connection "
        "as current.");
  } else {
    ALOGV("EGL successfully initialized.");
  }
}

EglDisplayContext::~EglDisplayContext() {
  if (mEglDisplay != EGL_NO_DISPLAY) {
    eglTerminate(mEglDisplay);
  }
  if (mEglContext != EGL_NO_CONTEXT) {
    eglDestroyContext(mEglDisplay, mEglContext);
  }
  eglReleaseThread();
}

EGLDisplay EglDisplayContext::getEglDisplay() const {
  return mEglDisplay;
}

bool EglDisplayContext::isInitialized() const {
  return mEglContext != EGL_NO_CONTEXT && mEglDisplay != EGL_NO_DISPLAY;
}

bool EglDisplayContext::makeCurrent() {
  if (!eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mEglContext)) {
    ALOGE("eglMakeCurrent failed: %#x", eglGetError());
    return false;
  }
  return true;
}

}  // namespace virtualcamera
}  // namespace companion
}  // namespace android
