LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := z
LOCAL_SRC_FILES := ../../../../3rd-party/pre-built/android/zlib/$(TARGET_ARCH_ABI)/libz.so
include $(PREBUILT_SHARED_LIBRARY)
