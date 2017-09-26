LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := z
LOCAL_SRC_FILES := ../../../../lib/android/zlib/$(TARGET_ARCH_ABI)/libz.so
include $(PREBUILT_SHARED_LIBRARY)