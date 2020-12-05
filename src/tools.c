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

#include "tools.h"

#include <string.h>
#include <stdlib.h>
#include <zlib.h>

extern void tic_tool_poke4(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek4(const void* addr, u32 index);
extern void tic_tool_poke2(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek2(const void* addr, u32 index);
extern void tic_tool_poke1(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek1(const void* addr, u32 index);

extern s32 tic_tool_sfx_pos(s32 speed, s32 ticks);

s32 tic_tool_get_pattern_id(const tic_track* track, s32 frame, s32 channel)
{
    u32 patternData = 0;
    for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
        patternData |= track->data[frame * TRACK_PATTERNS_SIZE + b] << (BITS_IN_BYTE * b);

    return (patternData >> (channel * TRACK_PATTERN_BITS)) & TRACK_PATTERN_MASK;
}

bool tic_tool_parse_note(const char* noteStr, s32* note, s32* octave)
{
    if(noteStr && strlen(noteStr) == 3)
    {
        static const char* Notes[] = SFX_NOTES;

        for(s32 i = 0; i < COUNT_OF(Notes); i++)
        {
            if(memcmp(Notes[i], noteStr, 2) == 0)
            {
                *note = i;
                *octave = noteStr[2] - '1';
                break;
            }
        }

        return true;
    }

    return false;
}

u32 tic_tool_find_closest_color(const tic_rgb* palette, const gif_color* color)
{
    u32 minDst = -1;
    u32 closetColor = 0;

    enum{Size = TIC_PALETTE_SIZE};
    
    for (s32 i = 0; i < Size; i++)
    {
        const tic_rgb* rgb = palette + i;

        s32 r = color->r - rgb->r;
        s32 g = color->g - rgb->g;
        s32 b = color->b - rgb->b;

        u32 dst = r*r + g*g + b*b;

        if (dst < minDst)
        {
            minDst = dst;
            closetColor = i;
        }
    }

    return closetColor;
}

u32* tic_tool_palette_blit(const tic_palette* srcpal, tic80_pixel_color_format fmt)
{
    static u32 pal[TIC_PALETTE_SIZE];

    const tic_rgb* src = srcpal->colors;
    const tic_rgb* end = src + TIC_PALETTE_SIZE;
    u8* dst = (u8*)pal;

    while(src != end)
    {
        switch(fmt){
            case TIC80_PIXEL_COLOR_BGRA8888:
                *dst++ = src->b;
                *dst++ = src->g;
                *dst++ = src->r;
                *dst++ = 0xff;
                break;
            case TIC80_PIXEL_COLOR_RGBA8888:
                *dst++ = src->r;
                *dst++ = src->g;
                *dst++ = src->b;
                *dst++ = 0xff;
                break;
            case TIC80_PIXEL_COLOR_ABGR8888:
                *dst++ = 0xff;
                *dst++ = src->b;
                *dst++ = src->g;
                *dst++ = src->r;
                break;
            case TIC80_PIXEL_COLOR_ARGB8888:
                *dst++ = 0xff;
                *dst++ = src->r;
                *dst++ = src->g;
                *dst++ = src->b;
                break;
        }
        src++;
    }

    return pal;
}

bool tic_tool_has_ext(const char* name, const char* ext)
{
    return strcmp(name + strlen(name) - strlen(ext), ext) == 0;
}

s32 tic_tool_get_track_row_sfx(const tic_track_row* row)
{
    return (row->sfxhi << MUSIC_SFXID_LOW_BITS) | row->sfxlow;
}

void tic_tool_set_track_row_sfx(tic_track_row* row, s32 sfx)
{
    if(sfx >= SFX_COUNT) sfx = SFX_COUNT-1;        

    row->sfxhi = (sfx & 0b00100000) >> MUSIC_SFXID_LOW_BITS;
    row->sfxlow = sfx & 0b00011111;
}

bool tic_tool_is_noise(const tic_waveform* wave)
{
    for(s32 i = 0; i < WAVE_SIZE; i++)
        if(wave->data[i])
            return false;

    return true;
}

void tic_tool_str2buf(const char* str, s32 size, void* buf, bool flip)
{
    char val[] = "0x00";
    const char* ptr = str;

    for(s32 i = 0; i < size/2; i++)
    {
        if(flip)
        {
            val[3] = *ptr++;
            val[2] = *ptr++;
        }
        else
        {
            val[2] = *ptr++;
            val[3] = *ptr++;
        }

        ((u8*)buf)[i] = (u8)strtol(val, NULL, 16);
    }
}

u32 tic_tool_zip(void* dest, s32 destSize, const void* source, s32 size)
{
    unsigned long destSizeLong = destSize;
    return compress2(dest, &destSizeLong, source, size, Z_BEST_COMPRESSION) == Z_OK ? destSizeLong : 0;
}

u32 tic_tool_unzip(void* dest, s32 destSize, const void* source, s32 size)
{
    unsigned long destSizeLong = destSize;
    return uncompress(dest, &destSizeLong, source, size) == Z_OK ? destSizeLong : 0;
}
