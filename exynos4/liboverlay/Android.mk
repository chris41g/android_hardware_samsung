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

LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include 
LOCAL_SRC_FILES := v4l2_utils.cpp overlay.cpp

LOCAL_MODULE_TAGS := eng
#LOCAL_MODULE := overlay.$(TARGET_DEVICE)
LOCAL_MODULE := overlay.$(TARGET_BOARD_PLATFORM)
LOCAL_CFLAGS += -DSLSI_S5PC210

ifeq ($(BOARD_USES_HDMI),true)
LOCAL_C_INCLUDES += \
    frameworks/base/slsi/java/com/slsi/libhdmiservice

    LOCAL_CFLAGS += -DBOARD_USES_HDMI
    LOCAL_SHARED_LIBRARIES += libhdmiservice libhdmi libfimc

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
    LOCAL_CFLAGS  += -DBOARD_USES_HDMI_SUBTITLES
endif
endif

include $(BUILD_SHARED_LIBRARY)
