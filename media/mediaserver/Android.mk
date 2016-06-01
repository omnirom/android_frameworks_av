LOCAL_PATH:= $(call my-dir)

ifneq ($(BOARD_USE_CUSTOM_MEDIASERVEREXTENSIONS),true)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := register.cpp
LOCAL_MODULE := libregistermsext
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main_mediaserver.cpp

LOCAL_SHARED_LIBRARIES := \
	libaudioflinger \
	libaudiopolicyservice \
	libcamera_metadata\
	libcameraservice \
	libicuuc \
	libmedialogservice \
	libresourcemanagerservice \
	libcutils \
	libnbaio \
	libmedia \
	libmediaplayerservice \
	libutils \
	liblog \
	libbinder \
	libsoundtriggerservice \
	libradioservice

LOCAL_STATIC_LIBRARIES := \
        libicuandroid_utils \
        libregistermsext

LOCAL_C_INCLUDES := \
    frameworks/av-caf/media/libmediaplayerservice \
    frameworks/av-caf/services/medialog \
    frameworks/av-caf/services/audioflinger \
    frameworks/av-caf/services/audiopolicy \
    frameworks/av-caf/services/audiopolicy/common/managerdefinitions/include \
    frameworks/av-caf/services/audiopolicy/common/include \
    frameworks/av-caf/services/audiopolicy/engine/interface \
    frameworks/av-caf/services/camera/libcameraservice \
    frameworks/av-caf/services/mediaresourcemanager \
    $(call include-path-for, audio-utils) \
    frameworks/av-caf/services/soundtrigger \
    frameworks/av-caf/services/radio \
    external/sonic

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_LISTEN)),true)
LOCAL_SHARED_LIBRARIES += liblisten
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-listen
LOCAL_CFLAGS += -DAUDIO_LISTEN_ENABLED
endif

LOCAL_MODULE:= mediaserver
LOCAL_32_BIT_ONLY := true

include $(BUILD_EXECUTABLE)
