LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gif
LOCAL_SRC_FILES := ../../../../lib/android/gif/$(TARGET_ARCH_ABI)/libgif.so
include $(PREBUILT_SHARED_LIBRARY)