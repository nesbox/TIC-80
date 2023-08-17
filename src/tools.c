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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern void tic_tool_poke4(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek4(const void* addr, u32 index);
extern void tic_tool_poke2(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek2(const void* addr, u32 index);
extern void tic_tool_poke1(void* addr, u32 index, u8 value);
extern u8 tic_tool_peek1(const void* addr, u32 index);
extern s32 tic_tool_sfx_pos(s32 speed, s32 ticks);
extern u32 tic_rgba(const tic_rgb* c);
extern s32 tic_modulo(s32 x, s32 m);

static u32 getPatternData(const tic_track* track, s32 frame)
{
    u32 patternData = 0;
    for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
        patternData |= track->data[frame * TRACK_PATTERNS_SIZE + b] << (BITS_IN_BYTE * b);

    return patternData;
}

s32 tic_tool_get_pattern_id(const tic_track* track, s32 frame, s32 channel)
{
    return (getPatternData(track, frame) >> (channel * TRACK_PATTERN_BITS)) & TRACK_PATTERN_MASK;
}

void tic_tool_set_pattern_id(tic_track* track, s32 frame, s32 channel, s32 pattern)
{
    u32 patternData = getPatternData(track, frame);
    s32 shift = channel * TRACK_PATTERN_BITS;

    patternData &= ~(TRACK_PATTERN_MASK << shift);
    patternData |= pattern << shift;

    for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
        track->data[frame * TRACK_PATTERNS_SIZE + b] = (patternData >> (b * BITS_IN_BYTE)) & 0xff;
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

u32 tic_nearest_color(const tic_rgb* palette, const tic_rgb* color, s32 count)
{
    u32 min = -1;
    s32 nearest, i = 0;
    
    for(const tic_rgb *rgb = palette, *end = rgb + count; rgb < end; rgb++, i++)
    {
        s32 d[] = {color->r - rgb->r, color->g - rgb->g, color->b - rgb->b};

        u32 dst = 0;
        for(const s32 *v = d, *end = v + COUNT_OF(d); v < end; v++)
            dst += (*v) * (*v);

        if (dst < min)
        {
            min = dst;
            nearest = i;
        }
    }

    return nearest;
}

tic_blitpal tic_tool_palette_blit(const tic_palette* srcpal, tic80_pixel_color_format fmt)
{
    tic_blitpal pal;

    const tic_rgb* src = srcpal->colors;
    const tic_rgb* end = src + TIC_PALETTE_SIZE;
    u8* dst = (u8*)pal.data;

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

bool tic_project_ext(const char* name)
{
    FOR_EACH_LANG(ln)
    {
        if(tic_tool_has_ext(name, ln->fileExtension))
            return true;
    }
    FOR_EACH_LANG_END
    return false;
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

    row->sfxhi = (sfx & 0x20) >> MUSIC_SFXID_LOW_BITS;
    row->sfxlow = sfx & 0x1f;
}

bool tic_tool_empty(const void* buffer, s32 size)
{
    for(const u8 *ptr = buffer, *end = ptr + size; ptr < end;)
        if(*ptr++)
            return false;

    return true;
}

bool tic_tool_flat4(const void* buffer, s32 size)
{
    u8 first = (*(u8*)buffer) & 0xf;
    first |= first << 4;
    for(const u8 *ptr = buffer, *end = ptr + size; ptr < end;)
        if(*ptr++ != first)
            return false;

    return true;
}

bool tic_tool_noise(const tic_waveform* wave)
{
    return FLAT4(wave->data) && *wave->data % 0xff == 0;
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

char* tic_tool_metatag(const char* code, const char* tag, const char* comment)
{
    const char* start = NULL;

    {
        static char format[] = "%s %s:";

        char* tagBuffer = malloc(sizeof format + strlen(tag));

        SCOPE(free(tagBuffer))
        {
            sprintf(tagBuffer, format, comment, tag);
            if ((start = strstr(code, tagBuffer)))
                start += strlen(tagBuffer);
        }
    }

    if (start)
    {
        const char* end = strstr(start, "\n");

        if (end)
        {
            while (isspace(*start) && start < end) start++;
            while (isspace(*(end - 1)) && end > start) end--;

            const s32 size = (s32)(end - start);

            char* value = (char*)malloc(size + 1);

            if (value)
            {
                memset(value, 0, size + 1);
                memcpy(value, start, size);

                return value;
            }
        }
    }

    return NULL;
}