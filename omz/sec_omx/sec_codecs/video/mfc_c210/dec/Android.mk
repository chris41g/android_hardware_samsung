ifeq ($(filter-out s5pc210 exynos4,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	src/SsbSipMfcDecAPI.c \
	src/tile_to_linear_64x32_4x2_neon.s

LOCAL_MODULE := libsecmfcdecapi

LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS :=

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_C_INCLUDES := \
	$(SEC_CODECS)/video/mfc_c210/include

ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_C_INCLUDES += vendor/sec/sec_proprietary/include
LOCAL_CFLAGS += -DCONFIG_VIDEO_MFC_VCM_UMP
endif

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS += -DCONFIG_MFC_FPS
endif

include $(BUILD_STATIC_LIBRARY)

endif
