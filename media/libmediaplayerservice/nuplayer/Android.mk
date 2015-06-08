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
	$(TOP)/frameworks/av-caf/media/libstagefright/httplive            \
	$(TOP)/frameworks/av-caf/media/libstagefright/include             \
	$(TOP)/frameworks/av-caf/media/libstagefright/mpeg2ts             \
	$(TOP)/frameworks/av-caf/media/libstagefright/rtsp                \
	$(TOP)/frameworks/av-caf/media/libstagefright/timedtext           \
	$(TOP)/frameworks/av-caf/media/libmediaplayerservice              \
	$(TOP)/frameworks/native/include/media/openmax

LOCAL_MODULE:= libstagefright_nuplayer

LOCAL_MODULE_TAGS := eng

include $(BUILD_STATIC_LIBRARY)

