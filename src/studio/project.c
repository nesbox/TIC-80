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
#include "script.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

static const struct BinarySection
{
    const char* tag;
    s32 count;
    s32 offset;
    s32 size;
    bool flip;
} BinarySections[] =
    {
        {"TILES", TIC_BANK_SPRITES, offsetof(tic_bank, tiles), sizeof(tic_tile), true},
        {"SPRITES", TIC_BANK_SPRITES, offsetof(tic_bank, sprites), sizeof(tic_tile), true},
        {"MAP", TIC_MAP_HEIGHT, offsetof(tic_bank, map), TIC_MAP_WIDTH, true},
        {"WAVES", WAVES_COUNT, offsetof(tic_bank, sfx.waveforms), sizeof(tic_waveform), true},
        {"SFX", SFX_COUNT, offsetof(tic_bank, sfx.samples), sizeof(tic_sample), true},
        {"PATTERNS", MUSIC_PATTERNS, offsetof(tic_bank, music.patterns), sizeof(tic_track_pattern), true},
        {"TRACKS", MUSIC_TRACKS, offsetof(tic_bank, music.tracks), sizeof(tic_track), true},
        {"FLAGS", TIC_SPRITE_BANKS, offsetof(tic_bank, flags), TIC_BANK_SPRITES, true},
        {"SCREEN", TIC80_HEIGHT, offsetof(tic_bank, screen), sizeof(tic_screen) / TIC80_HEIGHT, true},
        {"PALETTE", TIC_PALETTES, offsetof(tic_bank, palette), sizeof(tic_palette), false},
};

static const struct BinarySection LangSection = {"LANG", 1, 0, sizeof(((tic_cartridge*)0)->lang), false};

// What a row line puts between the comment and its data: a space, the row's
// three digits and the colon.
enum
{
    RowPrefix = sizeof(" 000:") - 1,
};

static void makeTag(const char* tag, char* out, s32 bank)
{
    if(bank)
        sprintf(out, "%s%i", tag, bank);
    else
        strcpy(out, tag);
}

// The code is a buffer, not a string: a cart can fill it to the last byte, and
// strlen would read on past it. The line end is the writer's, not the code's.
static char* saveTextSection(char* ptr, const char* data, s32 size)
{
    const char* terminator = memchr(data, '\0', size);
    s32 len = terminator ? (s32)(terminator - data) : size;

    if(len)
    {
        memcpy(ptr, data, len);
        ptr += len;
        *ptr++ = '\n';
    }

    *ptr = '\0';

    return ptr;
}

static char* saveBinaryBuffer(char* ptr, const char* comment, const void* data, s32 size, s32 row, bool flip)
{
    if(tic_tool_empty(data, size))
        return ptr;

    sprintf(ptr, "%s %03i:", comment, row);
    ptr += strlen(ptr);

    tic_tool_buf2str(data, size, ptr, flip);
    ptr += strlen(ptr);

    sprintf(ptr, "\n");
    ptr += strlen(ptr);

    return ptr;
}

static char* saveBinarySection(char* ptr, const char* comment, const char* tag, s32 count, const void* data, s32 size, bool flip)
{
    if(tic_tool_empty(data, size * count))
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
    FOREACH_LANG(ln)
    {
        if(tic_tool_has_ext(name, ln->fileExtension))
            return ln->projectComment;
    }

    return NULL;
}

s32 tic_project_save(const char* name, void* data, const tic_cartridge* cart)
{
    const char* comment = projectComment(name);

    if(!comment)
        return 0;

    char* stream = data;
    char* ptr = saveTextSection(stream, cart->code.data, sizeof(tic_code));
    char tag[16];

    FOR(const struct BinarySection*, section, BinarySections)
    for(s32 b = 0; b < TIC_BANKS; b++)
    {
        makeTag(section->tag, tag, b);

        ptr = saveBinarySection(ptr, comment, tag, section->count,
                                (u8*)&cart->banks[b] + section->offset, section->size, section->flip);
    }

    if(cart->lang)
        saveBinarySection(ptr, comment, LangSection.tag, LangSection.count, &cart->lang, LangSection.size, LangSection.flip);

    return (s32)strlen(stream);
}

// A file whose first line is a known section tag has no code, only data. Any
// other line — a comment with a bracket, another tag — is code.
static bool startsWithTag(const char* project, const char* comment)
{
    char tag[16];
    char tagbuf[64];

    FOR(const struct BinarySection*, section, BinarySections)
    for(s32 b = 0; b < TIC_BANKS; b++)
    {
        makeTag(section->tag, tag, b);
        sprintf(tagbuf, "%s <%s>", comment, tag);

        if(strncmp(project, tagbuf, strlen(tagbuf)) == 0)
            return true;
    }

    sprintf(tagbuf, "%s <%s>", comment, LangSection.tag);

    return strncmp(project, tagbuf, strlen(tagbuf)) == 0;
}

static bool loadTextSection(const char* project, const char* comment, char* dst, s32 size)
{
    const char* start = project;
    const char* end = project + strlen(project);
    char tagstart[16];

    if(startsWithTag(project, comment))
        return true;

    sprintf(tagstart, "\n%s <", comment);

    const char* ptr = strstr(project, tagstart);

    if(ptr)
        end = ptr;
    else if(end > start && end[-1] == '\n')
        end--; // the writer's line end, not the code's

    if(end > start)
    {
        memcpy(dst, start, MIN(size, end - start));
        return true;
    }

    return false;
}

static inline const char* getLineEnd(const char* ptr)
{
    while(*ptr && isspace((u8)*ptr))
        if(*ptr++ == '\n')
            break;

    return ptr;
}

// The three digits a row line is indexed by, or -1 when the line holds none.
static s32 rowIndex(const char* ptr)
{
    for(s32 i = 0; i < 3; i++)
        if(!isdigit((u8)ptr[i]))
            return -1;

    return (ptr[0] - '0') * 100 + (ptr[1] - '0') * 10 + (ptr[2] - '0');
}

static void loadBinarySection(const char* project, const char* comment, const char* tag, s32 count, void* dst, s32 size, bool flip)
{
    const s32 commentLen = (s32)strlen(comment);
    char tagbuf[64];
    sprintf(tagbuf, "%s <%s>", comment, tag);

    const char* start = strstr(project, tagbuf);

    if(!start)
        return;

    start += strlen(tagbuf);
    start = getLineEnd(start);

    sprintf(tagbuf, "\n%s </%s>", comment, tag);
    const char* end = strstr(start, tagbuf);

    // A row is `<comment> 000:<2*size hex>` and a line end. Anything shorter,
    // or not starting with its own index, ends the section rather than reading
    // past it: a project is a text an author can cut.
    if(!end || end <= start)
        return;

    const char* ptr = start;

    while(ptr < end && end - ptr >= commentLen + RowPrefix + size * 2)
    {
        s32 index = rowIndex(ptr + commentLen + 1);

        if(index < 0 || index >= count)
            break;

        ptr += commentLen + RowPrefix;
        tic_tool_str2buf(ptr, size * 2, (u8*)dst + size * index, flip);
        ptr += size * 2;

        ptr = getLineEnd(ptr);
    }
}

bool tic_project_load(const char* name, const char* data, s32 size, tic_cartridge* dst)
{
    const char* comment = projectComment(name);

    // A name no script knows has no comment to read the sections by.
    if(!comment)
        return false;

    char* project = (char*)malloc(size + 1);

    bool done = false;

    if(project)
    {
        memcpy(project, data, size);
        project[size] = '\0';

        // remove all the '\r' chars
        {
            char *s, *d;
            for(s = d = project; (*d = *s); d += (*s++ != '\r'))
                ;
        }

        tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));

        if(cart)
        {
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
