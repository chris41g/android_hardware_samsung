ifeq ($(BOARD_USES_GENERIC_AUDIO),false)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE:= arm
LOCAL_CFLAGS:= -D_POSIX_SOURCE
LOCAL_WHOLE_STATIC_LIBRARIES:= libasound

ifneq ($(ALSA_DEFAULT_SAMPLE_RATE),)
  LOCAL_CFLAGS+= -DALSA_DEFAULT_SAMPLE_RATE=$(ALSA_DEFAULT_SAMPLE_RATE)
endif

ifeq ($(BOARD_USES_CIRCULAR_BUF_AUDIO), true)
  LOCAL_CFLAGS+= -DBOARD_USES_CIRCULAR_BUF_AUDIO
endif

ifeq ($(USE_ULP_AUDIO),true)
  LOCAL_CFLAGS+= -DSLSI_ULP_AUDIO
endif

LOCAL_C_INCLUDES+= $(TOP)/external/alsa-lib/include
LOCAL_SRC_FILES:= AudioHardware.cpp
LOCAL_MODULE:= libaudio
LOCAL_STATIC_LIBRARIES:= libaudiointerface 
LOCAL_SHARED_LIBRARIES:= libc libcutils libutils libmedia libhardware_legacy libdl libavcodec libavutil

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_SHARED_LIBRARIES+= liba2dp
endif

LOCAL_C_INCLUDES += external/ffmpeg-android 

PRODUCT_COPY_FILES += $(LOCAL_PATH)/wm8994_asound.conf:system/etc/asound.conf

include $(BUILD_SHARED_LIBRARY)

# To build audiopolicy library
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= AudioPolicyManager.cpp
LOCAL_MODULE:= libaudiopolicy
LOCAL_STATIC_LIBRARIES:= libaudiopolicybase
LOCAL_SHARED_LIBRARIES:= libcutils libutils libmedia

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS+= -DWITH_A2DP
endif

PRODUCT_COPY_FILES += $(LOCAL_PATH)/wm8994_asound.conf:system/etc/asound.conf

include $(BUILD_SHARED_LIBRARY)

endif
