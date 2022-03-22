// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "project.h"
#include "tools.h"
#include "api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

static const struct BinarySection{const char* tag; s32 count; s32 offset; s32 size; bool flip;} BinarySections[] = 
{
    {"TILES",       TIC_BANK_SPRITES,   offsetof(tic_bank, tiles),          sizeof(tic_tile),                   true},
    {"SPRITES",     TIC_BANK_SPRITES,   offsetof(tic_bank, sprites),        sizeof(tic_tile),                   true},
    {"MAP",         TIC_MAP_HEIGHT,     offsetof(tic_bank, map),            TIC_MAP_WIDTH,                      true},
    {"WAVES",       WAVES_COUNT,        offsetof(tic_bank, sfx.waveforms),  sizeof(tic_waveform),               true},
    {"SFX",         SFX_COUNT,          offsetof(tic_bank, sfx.samples),    sizeof(tic_sample),                 true},
    {"PATTERNS",    MUSIC_PATTERNS,     offsetof(tic_bank, music.patterns), sizeof(tic_track_pattern),          true},
    {"TRACKS",      MUSIC_TRACKS,       offsetof(tic_bank, music.tracks),   sizeof(tic_track),                  true},
    {"FLAGS",       TIC_SPRITE_BANKS,   offsetof(tic_bank, flags),          TIC_BANK_SPRITES,                   true},
    {"SCREEN",      TIC80_HEIGHT,       offsetof(tic_bank, screen),         sizeof(tic_screen) / TIC80_HEIGHT,  true},
    {"PALETTE",     TIC_PALETTES,       offsetof(tic_bank, palette),        sizeof(tic_palette),                false},
};

static const struct BinarySection LangSection = {"LANG", 1, 0, sizeof (tic_cartridge){0}.lang, false};

static void makeTag(const char* tag, char* out, s32 bank)
{
    if(bank) sprintf(out, "%s%i", tag, bank);
    else strcpy(out, tag);
}

static void buf2str(const void* data, s32 size, char* ptr, bool flip)
{
    enum {Len = 2};

    for(s32 i = 0; i < size; i++, ptr+=Len)
    {
        sprintf(ptr, "%02x", ((u8*)data)[i]);
        if(flip) SWAP(ptr[0], ptr[1], char);
    }
}

static bool bufferEmpty(const u8* data, s32 size)
{
    for(s32 i = 0; i < size; i++)
        if(*data++)
            return false;

    return true;
}

static char* saveTextSection(char* ptr, const char* data)
{
    if(data[0] == '\0')
        return ptr;

    sprintf(ptr, "%s\n", data);
    ptr += strlen(ptr);

    return ptr;
}

static char* saveBinaryBuffer(char* ptr, const char* comment, const void* data, s32 size, s32 row, bool flip)
{
    if(bufferEmpty(data, size)) 
        return ptr;

    sprintf(ptr, "%s %03i:", comment, row);
    ptr += strlen(ptr);

    buf2str(data, size, ptr, flip);
    ptr += strlen(ptr);

    sprintf(ptr, "\n");
    ptr += strlen(ptr);

    return ptr;
}

static char* saveBinarySection(char* ptr, const char* comment, const char* tag, s32 count, const void* data, s32 size, bool flip)
{
    if(bufferEmpty(data, size * count)) 
        return ptr;

    sprintf(ptr, "%s <%s>\n", comment, tag);
    ptr += strlen(ptr);

    for(s32 i = 0; i < count; i++, data = (u8*)data + size)
        ptr = saveBinaryBuffer(ptr, comment, data, size, i, flip);

    sprintf(ptr, "%s </%s>\n\n", comment, tag);
    ptr += strlen(ptr);

    return ptr;
}

static const char* projectComment(const char* name)
{
    FOR_EACH_LANG(ln)
    {
        if(tic_tool_has_ext(name, ln->fileExtension))
            return ln->projectComment;
    }
    FOR_EACH_LANG_END
    return Languages[0]->projectComment;
}

s32 tic_project_save(const char* name, void* data, const tic_cartridge* cart)
{
    const char* comment = projectComment(name);
    char* stream = data;
    char* ptr = saveTextSection(stream, cart->code.data);
    char tag[16];

    FOR(const struct BinarySection*, section, BinarySections)
        for(s32 b = 0; b < TIC_BANKS; b++)
        {
            makeTag(section->tag, tag, b);

            ptr = saveBinarySection(ptr, comment, tag, section->count, 
                (u8*)&cart->banks[b] + section->offset, section->size, section->flip);
        }

    if(cart->lang)
        ptr = saveBinarySection(ptr, comment, LangSection.tag, LangSection.count, &cart->lang, LangSection.size, LangSection.flip);

    return (s32)strlen(stream);
}

static bool loadTextSection(const char* project, const char* comment, char* dst, s32 size)
{
    bool done = false;

    const char* start = project;
    const char* end = project + strlen(project);

    {
        char tagstart[16];
        sprintf(tagstart, "\n%s <", comment);

        const char* ptr = strstr(project, tagstart);

        if(ptr && ptr < end)
            end = ptr;
    }

    if(end > start)
    {
        memcpy(dst, start, MIN(size, end - start));
        done = true;
    }

    return done;
}

static inline const char* getLineEnd(const char* ptr)
{
    while(*ptr && isspace(*ptr) && *ptr++ != '\n');

    return ptr;
}

static bool loadBinarySection(const char* project, const char* comment, const char* tag, s32 count, void* dst, s32 size, bool flip)
{
    char tagbuf[64];
    sprintf(tagbuf, "%s <%s>", comment, tag);

    const char* start = strstr(project, tagbuf);
    bool done = false;

    if(start)
    {
        start += strlen(tagbuf);
        start = getLineEnd(start);

        sprintf(tagbuf, "\n%s </%s>", comment, tag);
        const char* end = strstr(start, tagbuf);

        if(end > start)
        {
            const char* ptr = start;

            if(size > 0)
            {
                while(ptr < end)
                {
                    static char lineStr[] = "999";
                    memcpy(lineStr, ptr + strlen(comment) + 1, sizeof lineStr - 1);

                    s32 index = atoi(lineStr);
                    
                    if(index < count)
                    {
                        ptr += strlen(comment) + sizeof(" 999:") - 1;
                        tic_tool_str2buf(ptr, size*2, (u8*)dst + size*index, flip);
                        ptr += size*2 + 1;

                        ptr = getLineEnd(ptr);
                    }
                    else break;
                }               
            }
            else
            {
                ptr += strlen(comment) + sizeof(" 999:") - 1;
                tic_tool_str2buf(ptr, (s32)(end - ptr), (u8*)dst, flip);
            }

            done = true;
        }
    }

    return done;
}

bool tic_project_load(const char* name, const char* data, s32 size, tic_cartridge* dst)
{
    char* project = (char*)malloc(size+1);

    bool done = false;

    if(project)
    {
        memcpy(project, data, size);
        project[size] = '\0';

        // remove all the '\r' chars
        {
            char *s, *d;
            for(s = d = project; (*d = *s); d += (*s++ != '\r'));
        }

        tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));

        if(cart)
        {
            const char* comment = projectComment(name);
            char tag[16];

            if(loadTextSection(project, comment, cart->code.data, sizeof(tic_code)))
                done = true;

            if(done)
            {
                FOR(const struct BinarySection*, section, BinarySections)
                    for(s32 b = 0; b < TIC_BANKS; b++)
                    {
                        makeTag(section->tag, tag, b);
                        loadBinarySection(project, comment, tag, section->count, (u8*)&cart->banks[b] + section->offset, section->size, section->flip);
                    }

                loadBinarySection(project, comment, LangSection.tag, LangSection.count, &cart->lang, LangSection.size, LangSection.flip);
            }

            if(done)
                memcpy(dst, cart, sizeof(tic_cartridge));

            free(cart);
        }

        free(project);
    }

    return done;
}
