LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                          \
        stagefright/ExtendedMediaDefs.cpp  \
        stagefright/AVUtils.cpp            \
        stagefright/AVFactory.cpp          \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av-caf/include/media/ \
        $(TOP)/frameworks/av-caf/media/libavextensions \
        $(TOP)/frameworks/native-caf/include/media/hardware \
        $(TOP)/frameworks/native-caf/include/media/openmax \
        $(TOP)/external/flac/include \
        $(TOP)/frameworks/av-caf/media/libstagefright \
        $(TOP)/frameworks/av-caf/media/libstagefright/mpeg2ts \

ifneq ($(TARGET_QCOM_MEDIA_VARIANT),)
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media-$(TARGET_QCOM_MEDIA_VARIANT)/mm-core/inc
else
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media/mm-core/inc
endif

LOCAL_CFLAGS += -Wno-multichar -Werror

ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS),true)
       LOCAL_CFLAGS += -DENABLE_AV_ENHANCEMENTS
endif

LOCAL_MODULE:= libavextensions
LOCAL_CLANG := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

########################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                          \
        media/AVMediaUtils.cpp             \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av-caf/include/media/ \
        $(TOP)/frameworks/av-caf/media/libavextensions \
        $(TOP)/frameworks/native-caf/include/media/hardware \
        $(TOP)/frameworks/native-caf/include/media/openmax \
        $(TOP)/external/flac/include

ifneq ($(TARGET_QCOM_MEDIA_VARIANT),)
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media-$(TARGET_QCOM_MEDIA_VARIANT)/mm-core/inc
else
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media/mm-core/inc
endif

LOCAL_CFLAGS += -Wno-multichar -Werror

ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS),true)
       LOCAL_CFLAGS += -DENABLE_AV_ENHANCEMENTS
endif

LOCAL_MODULE:= libavmediaextentions
LOCAL_CLANG := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

########################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                                      \
        mediaplayerservice/AVMediaServiceFactory.cpp   \
        mediaplayerservice/AVMediaServiceUtils.cpp     \
        mediaplayerservice/AVNuFactory.cpp             \
        mediaplayerservice/AVNuUtils.cpp               \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av-caf/include/media/ \
        $(TOP)/frameworks/av-caf/media/libmediaplayerservice \
        $(TOP)/frameworks/av-caf/media/libavextensions \
        $(TOP)/frameworks/av-caf/media/libstagefright/include \
        $(TOP)/frameworks/av-caf/media/libstagefright/rtsp \
        $(TOP)/frameworks/native-caf/include/media/hardware \
        $(TOP)/frameworks/native-caf/include/media/openmax \
        $(TOP)/external/flac/include

ifneq ($(TARGET_QCOM_MEDIA_VARIANT),)
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media-$(TARGET_QCOM_MEDIA_VARIANT)/mm-core/inc
else
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media/mm-core/inc
endif

LOCAL_CFLAGS += -Wno-multichar -Werror

ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS),true)
       LOCAL_CFLAGS += -DENABLE_AV_ENHANCEMENTS
endif

LOCAL_MODULE:= libavmediaserviceextensions
LOCAL_CLANG := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

