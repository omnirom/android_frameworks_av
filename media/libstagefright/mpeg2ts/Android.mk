LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                 \
        AnotherPacketSource.cpp   \
        ATSParser.cpp             \
        ESQueue.cpp               \
        MPEG2PSExtractor.cpp      \
        MPEG2TSExtractor.cpp      \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/media/libstagefright \
        $(TOP)/frameworks/native/include/media/openmax

ifeq ($(call is-vendor-board-platform,QCOM),true)
ifneq ($(TARGET_QCOM_MEDIA_VARIANT),)
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media-$(TARGET_QCOM_MEDIA_VARIANT)/mm-core/inc
else
       LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media/mm-core/inc
endif
endif

LOCAL_CFLAGS += -Werror

LOCAL_MODULE:= libstagefright_mpeg2ts

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)
