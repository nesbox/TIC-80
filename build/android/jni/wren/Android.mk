LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := wren
LOCAL_SRC_FILES := ../../../../3rd-party/pre-built/android/wren/$(TARGET_ARCH_ABI)/libwren.so
include $(PREBUILT_SHARED_LIBRARY)
