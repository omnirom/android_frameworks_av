LOCAL_PATH:= $(call my-dir)

# Effect factory library
include $(CLEAR_VARS)

ifeq ($(TARGET_IGNORE_VENDOR_AUDIO_EFFECTS_CONF),true)
  LOCAL_CFLAGS += -DIGNORE_VENDOR_AUDIO_EFFECTS_CONF
endif

LOCAL_SRC_FILES:= \
	EffectsFactory.c

LOCAL_SHARED_LIBRARIES := \
	libcutils liblog

LOCAL_MODULE:= libeffects

LOCAL_SHARED_LIBRARIES += libdl

LOCAL_C_INCLUDES := \
    $(call include-path-for, audio-effects)

include $(BUILD_SHARED_LIBRARY)
