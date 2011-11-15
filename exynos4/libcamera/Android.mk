# When zero we link against libqcamera; when 1, we dlopen libqcamera.
ifeq ($(BOARD_CAMERA_LIBRARIES),libcamera)

DLOPEN_LIBSECCAMERA:=1

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS:=-fno-short-enums
LOCAL_CFLAGS+=-DDLOPEN_LIBSECCAMERA=$(DLOPEN_LIBSECCAMERA)


LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
ifeq ($(BOARD_USE_JPEG),true)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libs5pjpeg
endif

LOCAL_SRC_FILES:= \
	SecCamera.cpp \
	SecCameraHWInterface.cpp

LOCAL_CFLAGS += -DSLSI_S5PC210

LOCAL_SHARED_LIBRARIES:= libutils libui liblog libbinder
LOCAL_SHARED_LIBRARIES+= libcamera_client
ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_SHARED_LIBRARIES+= libMali
endif

ifeq ($(BOARD_USE_JPEG),true)
LOCAL_SHARED_LIBRARIES+= libs5pjpeg
endif

ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_CFLAGS+=-DBOARD_SUPPORT_SYSMMU
endif

ifeq ($(BOARD_USES_CAMERA_OVERLAY),true)
LOCAL_CFLAGS += -DBOARD_USES_CAMERA_OVERLAY
endif

#ifeq ($(BOARD_USES_HDMI), true)
#LOCAL_CFLAGS += -DBOARD_USES_HDMI
#endif

ifeq ($(DLOPEN_LIBSECCAMERA),1)
LOCAL_SHARED_LIBRARIES+= libdl
endif

ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_C_INCLUDES += device/sec/sec_proprietary/include
endif

LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:= libcamera

include $(BUILD_SHARED_LIBRARY)

endif

