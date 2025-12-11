# Virtual Camera

The virtual camera feature allows 3rd party application to expose a remote or
virtual camera to the standard Android camera frameworks (Camera2/CameraX, NDK,
camera1).

The stack is composed into 4 different parts:

1.  The **Virtual Camera Service** (this directory), implementing the Camera HAL
    and acts as an interface between the Android Camera Server and the *Virtual
    Camera Owner* (via the VirtualDeviceManager APIs).

2.  The **VirtualDeviceManager** running in the system process and handling the
    communication between the Virtual Camera service and the Virtual Camera
    owner

3.  The **Virtual Camera Owner**, the client application declaring the Virtual
    Camera and handling the production of image data. We will also refer to this
    part as the **producer**

4.  The **Consumer Application**, the client application consuming camera data,
    which can be any application using the camera APIs

This document describes the functionalities of the *Virtual Camera Service*

## Before reading

The service implements the Camera HAL. It's best to have a bit of an
understanding of how it works by reading the
[HAL documentation first](https://source.android.com/docs/core/camera)

![](https://source.android.com/static/docs/core/camera/images/ape_fwk_camera2.png)

The HAL implementations are declared in: -
[VirtualCameraDevice](./VirtualCameraDevice.h) -
[VirtualCameraProvider](./VirtualCameraProvider.h) -
[VirtualCameraSession](./VirtualCameraSession.h)

## Current supported features

Virtual Cameras report `EXTERNAL`
[hardware level](https://developer.android.com/reference/android/hardware/camera2/CameraCharacteristics#INFO_SUPPORTED_HARDWARE_LEVEL)
but some
[functionalities of `EXTERNAL`](https://developer.android.com/reference/android/hardware/camera2/CameraMetadata#INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL)
hardware level are not fully supported.

Here is a list of supported features - Single input multiple output stream and
capture:

-   Support for YUV and JPEG

Notable missing features:

-   Support for auto 3A (AWB, AE, AF): virtual camera will announce convergence
    of 3A algorithm even though it can't receive any information about this from
    the owner.

-   No flash/torch support

## Overview

Graphic data are exchanged using the Surface infrastructure. Like any other
Camera HAL, the Surfaces to write data into are received from the client.
Virtual Camera exposes a **different** Surface onto which the owner can write
data. In the middle, we use an EGL Texture which adapts (if needed) the producer
data to the required consumer format (scaling only for now, but we might also
add support for rotation and cropping in the future).

When the client application requires multiple resolutions, the closest one among
supported resolutions is used for the input data and the image data is down
scaled for the lower resolutions.

Depending on the type of output, the rendering pipelines change. Here is an
overview of the YUV and JPEG pipelines.

**YUV Rendering:**

```
Virtual Device Owner Surface[1] (Producer) --{binds to}--> EGL
Texture[1] --{renders into}--> Client Surface[1-n] (Consumer)
```

**JPEG Rendering:**

```
Virtual Device Owner Surface[1] (Producer) --{binds to}--> EGL
Texture[1] --{compress data into}--> temporary buffer --{renders into}-->
Client Surface[1-n] (Consumer)
```

## Life of a capture request

> Before reading the following, you must understand the concepts of
> [CaptureRequest](https://developer.android.com/reference/android/hardware/camera2/CaptureRequest)
> and
> [OutputConfiguration](https://developer.android.com/reference/android/hardware/camera2/OutputConfiguration).

1.  The consumer creates a session with one or more `Surfaces`

2.  The VirtualCamera owner will receive a call to
    `VirtualCameraCallback#onStreamConfigured` with a reference to another
    `Suface` where it can write into.

3.  The consumer will then start sending `CaptureRequests`. The owner will
    receive a call to `VirtualCameraCallback#onProcessCaptureRequest`, at which
    points it should write the required data into the surface it previously
    received. At the same time, a new task will be enqueued in the render thread

4.  The [VirtualCameraRenderThread](./VirtualCameraRenderThread.cc) will consume
    the enqueued tasks as they come. It will wait for the producer to write into
    the input Surface (using `Surface::waitForNextFrame`).

    > **Note:** Since the Surface API allows us to wait for the next frame,
    > there is no need for the producer to notify when the frame is ready by
    > calling a `processCaptureResult()` equivalent.

5.  The EGL Texture is updated with the content of the Surface.

6.  The EGL Texture renders into the output Surfaces.

7.  The Camera client is notified of the "shutter" event and the `CaptureResult`
    is sent to the consumer.

### Timeout and frame duplication

Most camera applications are made to handle only the on-device camera. When an
application runs on a virtual device with a virtual camera, the client might not
expect potential remote camera latency and/or disconnections.
For [CAPTURE_INTENT_PREVIEW](https://developer.android.com/reference/android/hardware/camera2/CameraMetadata#CONTROL_CAPTURE_INTENT_PREVIEW)
use case, to counterbalance these disruptions, virtual camera duplicates
the last frames if the producer does not post a new frame in time.

When a frame is duplicated, the timestamp must increase to meet the camera framework
expectations. In some use cases, this frame duplication is not wanted, like for motion
tracking, where the timestamp must match the graphic data.

No frame duplication takes place for other capture intents. The virtual camera
waits at most `1/minFps` second (see CaptureRequest#CONTROL_AE_TARGET_FPS_RANGE) or the
current FPS range and notifies the framework of a timeout.

## EGL Rendering

### The render thread

The [VirtualCameraRenderThread](./VirtualCameraRenderThread.h) module takes care
of rendering the input from the owner to the output via the EGL Texture. The
rendering is done either to a JPEG buffer, which is the BLOB rendering for
creating a JPEG or to a YUV buffer used mainly for preview Surfaces or video.
Two EGLPrograms (shaders) defined in [EglProgram](./util/EglProgram.cc) handle
the rendering of the data.

### Initialization

[EGlDisplayContext](./util/EglDisplayContext.h) initializes the whole EGL
environment (Display, Surface, Context, and Config).

The EGL Rendering is backed by a
[ANativeWindow](https://developer.android.com/ndk/reference/group/a-native-window)
which is just the native counterpart of the
[Surface](https://developer.android.com/reference/android/view/Surface), which
itself is the producer side of buffer queue, the consumer being either the
display (Camera preview) or some encoder (to save the data or send it across the
network).

### More about OpenGL

To better understand how the EGL rendering works the following resources can be
used:

Introduction to OpenGL: https://learnopengl.com/

The official documentation of EGL API can be queried at:
https://www.khronos.org/registry/egl/sdk/docs/man/xhtml/

And using Google search with the following query:

```
[function name] site:https://registry.khronos.org/EGL/sdk/docs/man/html/

// example: eglSwapBuffers site:https://registry.khronos.org/EGL/sdk/docs/man/html/
```
