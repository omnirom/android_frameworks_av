LOCAL_PATH:= $(call my-dir)

#
# libmediaplayerservice
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    ActivityManager.cpp         \
    Crypto.cpp                  \
    Drm.cpp                     \
    DrmSessionManager.cpp       \
    HDCP.cpp                    \
    MediaPlayerFactory.cpp      \
    MediaPlayerService.cpp      \
    MediaRecorderClient.cpp     \
    MetadataRetrieverClient.cpp \
    RemoteDisplay.cpp           \
    SharedLibrary.cpp           \
    StagefrightRecorder.cpp     \
    TestPlayerStub.cpp          \

LOCAL_SHARED_LIBRARIES :=       \
    libbinder                   \
    libcamera_client            \
    libcrypto                   \
    libcutils                   \
    libdrmframework             \
    liblog                      \
    libdl                       \
    libgui                      \
    libmedia                    \
    libmediautils               \
    libsonivox                  \
    libstagefright              \
    libstagefright_foundation   \
    libstagefright_httplive     \
    libstagefright_omx          \
    libstagefright_wfd          \
    libutils                    \
    libvorbisidec               \

LOCAL_STATIC_LIBRARIES :=       \
    libstagefright_nuplayer     \
    libstagefright_rtsp         \

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libavmediaserviceextensions \

LOCAL_C_INCLUDES :=                                                 \
    $(TOP)/frameworks/av-caf/media/libstagefright/include               \
    $(TOP)/frameworks/av-caf/media/libstagefright/rtsp                  \
    $(TOP)/frameworks/av-caf/media/libstagefright/wifi-display          \
    $(TOP)/frameworks/av-caf/media/libstagefright/webm                  \
    $(TOP)/frameworks/native-caf/include/media/openmax                  \
    $(TOP)/external/tremolo/Tremolo                                 \
    $(TOP)/frameworks/av-caf/media/libavextensions                      \

LOCAL_CFLAGS += -Werror -Wno-error=deprecated-declarations -Wall
LOCAL_CLANG := true

LOCAL_MODULE:= libmediaplayerservice

#LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
