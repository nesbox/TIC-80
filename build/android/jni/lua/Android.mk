LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := lua
LOCAL_SRC_FILES := ../../../../3rd-party/pre-built/android/lua/$(TARGET_ARCH_ABI)/liblua.so
include $(PREBUILT_SHARED_LIBRARY)
