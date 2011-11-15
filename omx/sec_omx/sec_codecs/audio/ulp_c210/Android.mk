LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/s5p-rp_api.c

LOCAL_MODULE := libs5prp

LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_COPY_HEADERS := \
	include/s5p-rp_api.h \
	include/s5p-rp_ioctl.h

include $(BUILD_STATIC_LIBRARY)
