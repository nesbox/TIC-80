LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -O3 -Wall -std=gnu99 -D"log2(x)=(log(x)/log(2))"
LOCAL_MODULE := main

SRC_PATH := ../../../../src
THIRD_PARTY_PATH := ../../../../3rd-party

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/SDL2-2.0.7/include \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/lua-5.3.1/src \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/giflib-5.1.4/lib \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/zlib-1.2.8 \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/duktape-2.2.0/src \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/blip-buf \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/SDL2_net-2.0.1 \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/moonscript \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/wren-0.1.0/src/include \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/sdl-gpu/include \
	$(LOCAL_PATH)/$(THIRD_PARTY_PATH)/fennel \
	$(LOCAL_PATH)/../../../../include

# Add your application source files here...
LOCAL_SRC_FILES := \
	$(THIRD_PARTY_PATH)/duktape-2.2.0/src/duktape.c \
	$(THIRD_PARTY_PATH)/blip-buf/blip_buf.c \
	$(THIRD_PARTY_PATH)/SDL2_net-2.0.1/SDLnet.c \
	$(THIRD_PARTY_PATH)/SDL2_net-2.0.1/SDLnetTCP.c \
	$(THIRD_PARTY_PATH)/SDL2_net-2.0.1/SDLnetselect.c \
	$(SRC_PATH)/studio.c \
	$(SRC_PATH)/console.c \
	$(SRC_PATH)/html.c \
	$(SRC_PATH)/run.c \
	$(SRC_PATH)/ext/file_dialog.c \
	$(SRC_PATH)/ext/md5.c \
	$(SRC_PATH)/ext/gif.c \
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
	$(SRC_PATH)/net.c \
	$(SRC_PATH)/luaapi.c \
	$(SRC_PATH)/jsapi.c \
	$(SRC_PATH)/wrenapi.c \
	$(SRC_PATH)/tic.c \
	$(SRC_PATH)/dialog.c \
	$(SRC_PATH)/menu.c \
	$(SRC_PATH)/surf.c \
	$(SRC_PATH)/tic80.c \
	$(SRC_PATH)/system.c

LOCAL_SHARED_LIBRARIES := SDL2 lua z gif wren sdlgpu

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
