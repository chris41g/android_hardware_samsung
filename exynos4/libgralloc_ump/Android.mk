#
# Copyright (C) 2010 ARM Limited. All rights reserved.
#
# Portions of this code have been modified from the original.
# These modifications are:
#    * The build configuration for the Gralloc module
#
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

ifeq ($(filter-out s5pc210,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libMali libGLESv1_CM

# Include the UMP header files
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	gralloc_module.cpp \
	alloc_device.cpp \
	framebuffer_device.cpp

LOCAL_MODULE_TAGS := eng
#LOCAL_MODULE := gralloc.default
#LOCAL_MODULE := gralloc.$(TARGET_DEVICE)
LOCAL_MODULE := gralloc.$(TARGET_BOARD_PLATFORM)
LOCAL_CFLAGS := -DLOG_TAG=\"gralloc\"

LOCAL_CFLAGS += -DSLSI_S5PC210
ifeq ($(BOARD_CACHEABLE_UMP),true)
LOCAL_CFLAGS += -DSLSI_S5PC210_CACHE_UMP
endif
include $(BUILD_SHARED_LIBRARY)

endif