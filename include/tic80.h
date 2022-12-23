// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "retro_endianness.h"
#include "tic80_config.h"
#include "tic80_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIC80_WIDTH             240
#define TIC80_HEIGHT            136
#define TIC80_FULLWIDTH_BITS    8
#define TIC80_FULLWIDTH         (1 << TIC80_FULLWIDTH_BITS)
#define TIC80_FULLHEIGHT        (TIC80_FULLWIDTH*9/16)

#define TIC80_MARGIN_TOP        ((TIC80_FULLHEIGHT - TIC80_HEIGHT) / 2)
#define TIC80_MARGIN_BOTTOM     TIC80_MARGIN_TOP
#define TIC80_MARGIN_LEFT       ((TIC80_FULLWIDTH - TIC80_WIDTH) / 2)
#define TIC80_MARGIN_RIGHT      TIC80_MARGIN_LEFT

#define TIC80_KEY_BUFFER        4
#define TIC80_SAMPLERATE        44100
#define TIC80_SAMPLETYPE        s16
#define TIC80_SAMPLESIZE        sizeof(TIC80_SAMPLETYPE)
#define TIC80_SAMPLE_CHANNELS   2
#define TIC80_FRAMERATE         60

typedef enum {
    TIC80_PIXEL_COLOR_ARGB8888 = (1 << 8) | 32,
    TIC80_PIXEL_COLOR_ABGR8888 = (2 << 8) | 32,
    TIC80_PIXEL_COLOR_RGBA8888 = (3 << 8) | 32,
    TIC80_PIXEL_COLOR_BGRA8888 = (4 << 8) | 32
} tic80_pixel_color_format;

typedef struct 
{
    struct
    {
        void (*trace)(const char* text, u8 color);
        void (*error)(const char* info);
        void (*exit)();
    } callback;

    struct
    {
        TIC80_SAMPLETYPE* buffer;
        s32 count;
    } samples;

    u32 *screen;
} tic80;

typedef union
{
    struct
    {
#if RETRO_IS_BIG_ENDIAN
        u8 y:1;
        u8 x:1;
        u8 b:1;
        u8 a:1;
        u8 right:1;
        u8 left:1;
        u8 down:1;
        u8 up:1;
#else
        u8 up:1;
        u8 down:1;
        u8 left:1;
        u8 right:1;
        u8 a:1;
        u8 b:1;
        u8 x:1;
        u8 y:1;
#endif
    };

    u8 data;
} tic80_gamepad;

typedef union
{
    struct
    {
#if RETRO_IS_BIG_ENDIAN
        tic80_gamepad fourth;
        tic80_gamepad third;
        tic80_gamepad second;
        tic80_gamepad first;
#else
        tic80_gamepad first;
        tic80_gamepad second;
        tic80_gamepad third;
        tic80_gamepad fourth;
#endif
    };

    u32 data;
} tic80_gamepads;

typedef struct
{
    union
    {
        // absolute pos
        struct
        {
            u8 x;
            u8 y;
        };

        // releative values
        struct
        {
            s8 rx;
            s8 ry;
        };
    };
    
    union
    {
        struct
        {
            u16 left:1;
            u16 middle:1;
            u16 right:1;

            s16 scrollx:6;
            s16 scrolly:6;

            u16 relative:1;
        };

        u16 btns;
    };
} tic80_mouse;

typedef u8 tic_key;

typedef union
{
    tic_key keys[TIC80_KEY_BUFFER];
    u32 data;
} tic80_keyboard;

typedef struct
{
    tic80_gamepads gamepads;
    tic80_mouse mouse;
    tic80_keyboard keyboard;

} tic80_input;

TIC80_API tic80* tic80_create(s32 samplerate, tic80_pixel_color_format format);
TIC80_API void tic80_load(tic80* tic, void* cart, s32 size);
TIC80_API void tic80_tick(tic80* tic, tic80_input input, u64 (*counter)(), u64 (*freq)());
TIC80_API void tic80_sound(tic80* tic);
TIC80_API void tic80_delete(tic80* tic);

#ifdef __cplusplus
}
#endif
