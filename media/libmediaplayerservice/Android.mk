LOCAL_PATH:= $(call my-dir)

#
# libmediaplayerservice
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    ActivityManager.cpp         \
    HDCP.cpp                    \
    MediaPlayerFactory.cpp      \
    MediaPlayerService.cpp      \
    MediaRecorderClient.cpp     \
    MetadataRetrieverClient.cpp \
    RemoteDisplay.cpp           \
    StagefrightRecorder.cpp     \
    TestPlayerStub.cpp          \

LOCAL_SHARED_LIBRARIES :=       \
    libbinder                   \
    libcrypto                   \
    libcutils                   \
    libdrmframework             \
    liblog                      \
    libdl                       \
    libgui                      \
    libaudioclient              \
    libmedia                    \
    libmediametrics             \
    libmediadrm                 \
    libmediautils               \
    libmemunreachable           \
    libstagefright              \
    libstagefright_foundation   \
    libstagefright_httplive     \
    libstagefright_omx          \
    libstagefright_wfd          \
    libutils                    \
    libnativewindow             \
    libhidlbase                 \
    android.hardware.media.omx@1.0 \

LOCAL_STATIC_LIBRARIES :=       \
    libstagefright_nuplayer     \
    libstagefright_rtsp         \
    libstagefright_timedtext    \

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := libmedia

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libavmediaserviceextensions \

LOCAL_C_INCLUDES :=                                                 \
    $(TOP)/frameworks/av/media/libstagefright/include               \
    $(TOP)/frameworks/av/media/libstagefright/rtsp                  \
    $(TOP)/frameworks/av/media/libstagefright/wifi-display          \
    $(TOP)/frameworks/av/media/libstagefright/webm                  \
    $(LOCAL_PATH)/include/media                              \
    $(TOP)/frameworks/av/include/camera                             \
    $(TOP)/frameworks/native/include/media/openmax                  \
    $(TOP)/frameworks/native/include/media/hardware                 \
    $(TOP)/external/tremolo/Tremolo                                 \
    $(TOP)/frameworks/av/media/libavextensions                      \
    $(TOP)/frameworks/av/media/libstagefright/mpeg2ts               \

LOCAL_CFLAGS += -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_MODULE:= libmediaplayerservice

#LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
