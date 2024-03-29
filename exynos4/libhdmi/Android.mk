# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(BOARD_USES_HDMI),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_PRELINK_MODULE := false
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := libutils liblog libedid libcec
LOCAL_SRC_FILES := SecHdmi.cpp fimd_api.c
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include \

LOCAL_CFLAGS += -DSLSI_S5PC210

LOCAL_CFLAGS  += \
        -DDEFAULT_FB_NUM=$(DEFAULT_FB_NUM)

LOCAL_SHARED_LIBRARIES += libfimc

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
LOCAL_CFLAGS     += -DBOARD_USES_HDMI_SUBTITLES
#ifeq ($(BOARD_USES_FIMGAPI),true)
#LOCAL_CFLAGS  += -DBOARD_USES_FIMGAPI
#LOCAL_SHARED_LIBRARIES += libfimg
#LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libfimg
#endif
endif

ifeq ($(BOARD_HDMI_STD), STD_NTSC_M)
LOCAL_CFLAGS  += -DSTD_NTSC_M
endif

ifeq ($(BOARD_HDMI_STD),STD_480P)
LOCAL_CFLAGS  += -DSTD_480P
endif

ifeq ($(BOARD_HDMI_STD),STD_720P)
LOCAL_CFLAGS  += -DSTD_720P
endif

ifeq ($(BOARD_HDMI_STD),STD_1080P)
LOCAL_CFLAGS  += -DSTD_1080P
endif

LOCAL_MODULE := libhdmi
include $(BUILD_SHARED_LIBRARY)

endif
