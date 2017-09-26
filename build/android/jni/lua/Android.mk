LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := lua
LOCAL_SRC_FILES := ../../../../lib/android/lua/$(TARGET_ARCH_ABI)/liblua.so
include $(PREBUILT_SHARED_LIBRARY)