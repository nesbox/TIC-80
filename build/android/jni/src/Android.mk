LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -O3 -Wall -std=c99 -D"log2(x)=(log(x)/log(2))"
LOCAL_MODULE := main

SDL_PATH := ../../../../../3rd-party/SDL2-2.0.5
SRC_PATH := ../../../../src
INCLUDE_PATH := ../../../../include

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/$(SDL_PATH)/include \
	$(LOCAL_PATH)/$(INCLUDE_PATH)/lua \
	$(LOCAL_PATH)/$(INCLUDE_PATH)/tic80 \
	$(LOCAL_PATH)/$(INCLUDE_PATH)/gif

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(SRC_PATH)/studio.c \
	$(SRC_PATH)/console.c \
	$(SRC_PATH)/html.c \
	$(SRC_PATH)/run.c \
	$(SRC_PATH)/ext/blip_buf.c \
	$(SRC_PATH)/ext/file_dialog.c \
	$(SRC_PATH)/ext/md5.c \
	$(SRC_PATH)/ext/gif.c \
	$(SRC_PATH)/ext/net/SDLnet.c \
	$(SRC_PATH)/ext/net/SDLnetTCP.c \
	$(SRC_PATH)/ext/net/SDLnetselect.c \
	$(SRC_PATH)/ext/duktape/duktape.c \
	$(SRC_PATH)/fs.c \
	$(SRC_PATH)/tools.c \
	$(SRC_PATH)/start.c \
	$(SRC_PATH)/sprite.c \
	$(SRC_PATH)/map.c \
	$(SRC_PATH)/sfx.c \
	$(SRC_PATH)/music.c \
	$(SRC_PATH)/history.c \
	$(SRC_PATH)/world.c \
	$(SRC_PATH)/code.c \
	$(SRC_PATH)/config.c \
	$(SRC_PATH)/keymap.c \
	$(SRC_PATH)/net.c \
	$(SRC_PATH)/luaapi.c \
	$(SRC_PATH)/jsapi.c \
	$(SRC_PATH)/bfapi.c \
	$(SRC_PATH)/tic.c \
	$(SRC_PATH)/dialog.c \
	$(SRC_PATH)/menu.c \
	$(SRC_PATH)/surf.c \
	$(SRC_PATH)/tic80.c

LOCAL_SHARED_LIBRARIES := SDL2 lua z gif

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
