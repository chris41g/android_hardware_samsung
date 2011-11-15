LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    stagefright_overlay_output.cpp \
    SecHardwareRenderer.cpp \
    SEC_OMX_Plugin.cpp

LOCAL_CFLAGS += $(PV_CFLAGS_MINUS_VISIBILITY)
LOCAL_CFLAGS += -DSLSI_S5PC210 -DZERO_COPY

ifeq ($(BOARD_USES_OVERLAY),true)
LOCAL_CFLAGS += -DBOARD_USES_OVERLAY
endif
ifeq ($(BOARD_USES_HDMI), true)
LOCAL_CFLAGS += -DBOARD_USES_HDMI
endif

ifeq ($(BOARD_USES_COPYBIT),true)
LOCAL_CFLAGS += -DBOARD_USES_COPYBIT
endif

LOCAL_C_INCLUDES:= \
      $(TOP)/frameworks/base/include/media/stagefright/openmax \
      $(LOCAL_PATH)/../include \
      $(LOCAL_PATH)/../liboverlay

LOCAL_SHARED_LIBRARIES :=    \
        libbinder            \
        libutils             \
        libcutils            \
        libui                \
        libdl                \
        libsurfaceflinger_client

LOCAL_MODULE := libstagefrighthw

ifeq ($(BOARD_USE_SAMSUNG_COLORFORMAT), true)
LOCAL_CFLAGS += -DUSE_SAMSUNG_COLORFORMAT
endif

LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
