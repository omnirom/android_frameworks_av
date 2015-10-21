LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                       \
        GenericSource.cpp               \
        HTTPLiveSource.cpp              \
        NuPlayer.cpp                    \
        NuPlayerCCDecoder.cpp           \
        NuPlayerDecoder.cpp             \
        NuPlayerDecoderBase.cpp         \
        NuPlayerDecoderPassThrough.cpp  \
        NuPlayerDriver.cpp              \
        NuPlayerRenderer.cpp            \
        NuPlayerStreamListener.cpp      \
        RTSPSource.cpp                  \
        StreamingSource.cpp             \

LOCAL_C_INCLUDES := \
	$(TOP)/frameworks/av-caf/media/libstagefright                     \
	$(TOP)/frameworks/av-caf/media/libstagefright/httplive            \
	$(TOP)/frameworks/av-caf/media/libstagefright/include             \
	$(TOP)/frameworks/av-caf/media/libstagefright/mpeg2ts             \
	$(TOP)/frameworks/av-caf/media/libstagefright/rtsp                \
	$(TOP)/frameworks/av-caf/media/libstagefright/timedtext           \
	$(TOP)/frameworks/av-caf/media/libmediaplayerservice              \
	$(TOP)/frameworks/native-caf/include/media/openmax                \
        $(TOP)/frameworks/av-caf/media/libavextensions                    \

LOCAL_CFLAGS += -Werror -Wall

# enable experiments only in userdebug and eng builds
ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CFLAGS += -DENABLE_STAGEFRIGHT_EXPERIMENTS
endif

LOCAL_CLANG := true

LOCAL_MODULE:= libstagefright_nuplayer

LOCAL_MODULE_TAGS := eng

include $(BUILD_STATIC_LIBRARY)

