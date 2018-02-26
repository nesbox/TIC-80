LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := sdlgpu
LOCAL_SRC_FILES := ../../../../3rd-party/pre-built/android/sdlgpu/$(TARGET_ARCH_ABI)/libsdlgpu.so
include $(PREBUILT_SHARED_LIBRARY)
