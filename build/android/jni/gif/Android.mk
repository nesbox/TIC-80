LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gif
LOCAL_SRC_FILES := ../../../../3rd-party/pre-built/android/gif/$(TARGET_ARCH_ABI)/libgif.so
include $(PREBUILT_SHARED_LIBRARY)
