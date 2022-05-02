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

#include "api.h"
#include "tic.h"
#include <stddef.h>

inline s32 tic_tool_sfx_pos(s32 speed, s32 ticks)
{
    return speed > 0 ? ticks * (1 + speed) : ticks / (1 - speed);
}

#define POKE_N(P,I,V,A,B,C,D) do        \
{                                       \
    u8* val = (u8*)(P) + ((I) >> (A));  \
    u8 offset = ((I) & (B)) << (C);     \
    *val &= ~((D) << offset);           \
    *val |= ((V) & (D)) << offset;      \
} while(0)

#define PEEK_N(P,I,A,B,C,D) ( ( ((u8*)(P))[((I) >> (A))] >> ( ((I) & (B)) << (C) ) ) & (D) )

inline void tic_tool_poke4(void* addr, u32 index, u8 value)
{
    POKE_N(addr, index, value, 1,1,2,15);
}

inline u8 tic_tool_peek4(const void* addr, u32 index)
{
    return PEEK_N(addr, index, 1,1,2,15);
}

inline void tic_tool_poke2(void* addr, u32 index, u8 value)
{
    POKE_N(addr, index, value, 2,3,1,3);
}

inline u8 tic_tool_peek2(const void* addr, u32 index)
{
    return PEEK_N(addr, index, 2,3,1,3);
}

inline void tic_tool_poke1(void* addr, u32 index, u8 value)
{
    POKE_N(addr, index, value, 3,7,0,1);
}

inline u8 tic_tool_peek1(const void* addr, u32 index)
{
    return PEEK_N(addr, index, 3,7,0,1);
}

#undef PEEK_N
#undef POKE_N

inline u32 tic_rgba(const tic_rgb* c)
{
    return (0xff << 24) | (c->b << 16) | (c->g << 8) | (c->r << 0);
}

inline s32 tic_modulo(s32 x, s32 m)
{
    if(x >= m) return x % m;
    if(x < 0) return x % m + m;

    return x;
}

tic_blitpal tic_tool_palette_blit(const tic_palette* src, tic80_pixel_color_format fmt);

bool    tic_tool_parse_note(const char* noteStr, s32* note, s32* octave);
s32     tic_tool_get_pattern_id(const tic_track* track, s32 frame, s32 channel);
void    tic_tool_set_pattern_id(tic_track* track, s32 frame, s32 channel, s32 id);
bool    tic_project_ext(const char* name);
bool    tic_tool_has_ext(const char* name, const char* ext);
s32     tic_tool_get_track_row_sfx(const tic_track_row* row);
void    tic_tool_set_track_row_sfx(tic_track_row* row, s32 sfx);
void    tic_tool_str2buf(const char* str, s32 size, void* buf, bool flip);

u32     tic_tool_zip(void* dest, s32 destSize, const void* source, s32 size);
u32     tic_tool_unzip(void* dest, s32 bufSize, const void* source, s32 size);

bool    tic_tool_empty(const void* buffer, s32 size);
#define EMPTY(BUFFER) (tic_tool_empty((BUFFER), sizeof (BUFFER)))

bool    tic_tool_flat4(const void* buffer, s32 size);
#define FLAT4(BUFFER) (tic_tool_flat4((BUFFER), sizeof (BUFFER)))

bool    tic_tool_noise(const tic_waveform* wave);
u32     tic_nearest_color(const tic_rgb* palette, const tic_rgb* color, s32 count);
char*   tic_tool_metatag(const char* code, const char* tag, const char* comment);