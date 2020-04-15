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

#include "tic.h"

typedef struct { u8 index; tic_flip flip; tic_rotate rotate; } RemapResult;
typedef void(*RemapFunc)(void*, s32 x, s32 y, RemapResult* result);

typedef void(*TraceOutput)(void*, const char*, u8 color);
typedef void(*ErrorOutput)(void*, const char*);
typedef void(*ExitCallback)(void*);
typedef bool(*CheckForceExit)(void*);

typedef struct
{
    TraceOutput trace;
    ErrorOutput error;
    ExitCallback exit;
    CheckForceExit forceExit;
    
    u64 (*counter)();
    u64 (*freq)();
    u64 start;

    // !TODO: get rid of this flag, because pmem now peekable through api
    bool syncPMEM;

    void* data;
} tic_tick_data;

typedef struct tic_mem tic_mem;
typedef void(*tic_tick)(tic_mem* memory);
typedef void(*tic_scanline)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_overline)(tic_mem* memory, void* data);

typedef struct
{
    s32 pos;
    s32 size;
} tic_outline_item;

typedef struct
{
    struct
    {
        bool(*init)(tic_mem* memory, const char* code);
        void(*close)(tic_mem* memory);

        tic_tick tick;
        tic_scanline scanline;
        tic_overline overline;
    };

    const tic_outline_item* (*getOutline)(const char* code, s32* size);
    void (*eval)(tic_mem* tic, const char* code);

    const char* blockCommentStart;
    const char* blockCommentEnd;
    const char* blockStringStart;
    const char* blockStringEnd;
    const char* singleComment;

    const char* const * keywords;
    s32 keywordsCount;
} tic_script_config;

#define TIC_FN "TIC"
#define SCN_FN "SCN"
#define OVR_FN "OVR"

//                  API DEFINITION TABLE
//      .----------------------------------------------- - - - 
//      |   NAME  | COUNT | RETURN  |     ARGUMENTS     
//      |---------+-------+---------+------------------- - - -
//      |         |       |         |
#define TIC_API_LIST(macro) \
    macro(print,        7,  s32,    tic_mem*, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt) \
    macro(cls,          1,  void,   tic_mem*, u8 color) \
    macro(pix,          3,  u8,     tic_mem*, s32 x, s32 y, u8 color, bool get) \
    macro(line,         5,  void,   tic_mem*, s32 x1, s32 y1, s32 x2, s32 y2, u8 color) \
    macro(rect,         5,  void,   tic_mem*, s32 x, s32 y, s32 width, s32 height, u8 color) \
    macro(rectb,        5,  void,   tic_mem*, s32 x, s32 y, s32 width, s32 height, u8 color) \
    macro(spr,          9,  void,   tic_mem*, const tic_tiles* src, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate) \
    macro(btn,          1,  u32,    tic_mem*, s32 id) \
    macro(btnp,         3,  u32,    tic_mem*, s32 id, s32 hold, s32 period) \
    macro(sfx,          6,  void,   tic_mem*, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 volume, s32 speed) \
    macro(map,          9,  void,   tic_mem*, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data) \
    macro(mget,         2,  u8,     tic_mem*, const tic_map* src, s32 x, s32 y) \
    macro(mset,         3,  void,   tic_mem*, tic_map* src, s32 x, s32 y, u8 value) \
    macro(peek,         1,  u8,     tic_mem*, s32 address) \
    macro(poke,         2,  void,   tic_mem*, s32 address, u8 value) \
    macro(peek4,        1,  u8,     tic_mem*, s32 address) \
    macro(poke4,        2,  void,   tic_mem*, s32 address, u8 value) \
    macro(memcpy,       3,  void,   tic_mem*, s32 dst, s32 src, s32 size) \
    macro(memset,       3,  void,   tic_mem*, s32 dst, u8 val, s32 size) \
    macro(trace,        2,  void,   tic_mem*, const char* text, u8 color) \
    macro(pmem,         2,  u32,    tic_mem*, s32 index, u32 value, bool get) \
    macro(time,         0,  double, tic_mem*) \
    macro(tstamp,       0,  s32,    tic_mem*) \
    macro(exit,         0,  void,   tic_mem*) \
    macro(font,         8,  s32,    tic_mem*, const char* text, s32 x, s32 y, u8 chromakey, s32 w, s32 h, bool fixed, s32 scale, bool alt) \
    macro(mouse,        0,  void,   tic_mem*) \
    macro(circ,         4,  void,   tic_mem*, s32 x, s32 y, s32 radius, u8 color) \
    macro(circb,        4,  void,   tic_mem*, s32 x, s32 y, s32 radius, u8 color) \
    macro(tri,          7,  void,   tic_mem*, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color) \
    macro(textri,       14, void,   tic_mem*, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8 chroma) \
    macro(clip,         4,  void,   tic_mem*, s32 x, s32 y, s32 width, s32 height) \
    macro(music,        4,  void,   tic_mem*, s32 track, s32 frame, s32 row, bool loop) \
    macro(sync,         3,  void,   tic_mem*, u32 mask, s32 bank, bool toCart) \
    macro(reset,        0,  void,   tic_mem*) \
    macro(key,          1,  bool,   tic_mem*, tic_key key) \
    macro(keyp,         3,  bool,   tic_mem*, tic_key key, s32 hold, s32 period) \
    macro(fget,         2,  bool,   tic_mem*, s32 index, u8 flag) \
    macro(fset,         3,  void,   tic_mem*, s32 index, u8 flag, bool value)
//      |         |       |         |
//      '----------------------------------------------- - - - 

#define TIC_API_DEF(name, _, ret, ...) ret tic_api_##name(__VA_ARGS__);
TIC_API_LIST(TIC_API_DEF)
#undef TIC_API_DEF

struct tic_mem
{
    tic_ram             ram;
    tic_cartridge       cart;
    tic_font            font;

    char saveid[TIC_SAVEID_SIZE];

    union
    {
        struct
        {
            u8 gamepad:1;
            u8 mouse:1;
            u8 keyboard:1;
        };

        u8 data;
    } input;

    struct
    {
        s16* buffer;
        s32 size;
    } samples;

    u32 screen[TIC80_FULLWIDTH * TIC80_FULLHEIGHT];
};

tic_mem* tic_core_create(s32 samplerate);
void tic_core_close(tic_mem* memory);
void tic_core_pause(tic_mem* memory);
void tic_core_resume(tic_mem* memory);
void tic_core_load(tic_cartridge* rom, const u8* buffer, s32 size);
s32  tic_core_save(const tic_cartridge* rom, u8* buffer);
void tic_core_tick_start(tic_mem* memory, const tic_sfx* sfx, const tic_music* music);
void tic_core_tick(tic_mem* memory, tic_tick_data* data);
void tic_core_tick_end(tic_mem* memory);
void tic_core_blit(tic_mem* tic);
void tic_core_blit_ex(tic_mem* tic, tic_scanline scanline, tic_overline overline, void* data);
const tic_script_config* tic_core_script_config(tic_mem* memory);

typedef struct
{
    tic80 tic;
    tic_mem* memory;
    tic_tick_data tickData;
} tic80_local;
