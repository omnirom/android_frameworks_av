LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                          \
        FrameDropper.cpp                   \
        GraphicBufferSource.cpp            \
        BWGraphicBufferSource.cpp          \
        OMX.cpp                            \
        OMXMaster.cpp                      \
        OMXNodeInstance.cpp                \
        OMXUtils.cpp                       \
        SimpleSoftOMXComponent.cpp         \
        SoftOMXComponent.cpp               \
        SoftOMXPlugin.cpp                  \
        SoftVideoDecoderOMXComponent.cpp   \
        SoftVideoEncoderOMXComponent.cpp   \
        1.0/Omx.cpp                        \
        1.0/OmxStore.cpp                   \
        1.0/WGraphicBufferProducer.cpp     \
        1.0/WProducerListener.cpp          \
        1.0/WGraphicBufferSource.cpp       \
        1.0/WOmxNode.cpp                   \
        1.0/WOmxObserver.cpp               \
        1.0/WOmxBufferSource.cpp           \

LOCAL_C_INCLUDES += \
        $(TOP)/frameworks/av/media/libstagefright \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/system/libhidl/base/include \

LOCAL_SHARED_LIBRARIES :=                         \
        libbase                                   \
        libbinder                                 \
        libmedia                                  \
        libutils                                  \
        liblog                                    \
        libui                                     \
        libgui                                    \
        libcutils                                 \
        libstagefright_foundation                 \
        libdl                                     \
        libhidlbase                               \
        libhidlmemory                             \
        libstagefright_xmlparser@1.0              \
        android.hidl.base@1.0                     \
        android.hidl.memory@1.0                   \
        android.hardware.media@1.0                \
        android.hardware.media.omx@1.0            \
        android.hardware.graphics.common@1.0      \
        android.hardware.graphics.bufferqueue@1.0 \

LOCAL_EXPORT_C_INCLUDES := \
        $(TOP)/frameworks/av/include

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        android.hidl.memory@1.0

ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_EXTN_FLAC_DECODER)),true)
LOCAL_CFLAGS += -DQTI_FLAC_DECODER
endif
endif

LOCAL_MODULE:= libstagefright_omx
LOCAL_CFLAGS += -Werror -Wall -Wno-unused-parameter -Wno-documentation
LOCAL_SANITIZE := unsigned-integer-overflow signed-integer-overflow cfi
LOCAL_SANITIZE_DIAG := cfi

include $(BUILD_SHARED_LIBRARY)

################################################################################

include $(call all-makefiles-under,$(LOCAL_PATH)/hal)
include $(call all-makefiles-under,$(LOCAL_PATH))
