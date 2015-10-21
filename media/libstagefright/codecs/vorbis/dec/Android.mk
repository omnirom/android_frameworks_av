LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        SoftVorbis.cpp

LOCAL_C_INCLUDES := \
        external/tremolo \
        frameworks/av-caf/media/libstagefright/include \
        frameworks/native-caf/include/media/openmax \

LOCAL_SHARED_LIBRARIES := \
        libvorbisidec libstagefright libstagefright_omx \
        libstagefright_foundation libutils liblog

LOCAL_MODULE := libstagefright_soft_vorbisdec
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Werror

include $(BUILD_SHARED_LIBRARY)
