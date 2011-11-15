LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include   $(SEC_CODECS)/video/mfc_c210/dec/Android.mk
include   $(SEC_CODECS)/video/mfc_c210/enc/Android.mk
include   $(SEC_CODECS)/audio/ulp_c210/Android.mk
