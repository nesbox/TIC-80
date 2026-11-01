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

#include "find.h"
#include "script.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool isFilterMatch(const char* buffer, s32 size, const char* filter)
{
    while(size--)
        if(tolower((u8)(*buffer++)) == tolower((u8)(*filter)))
            filter++;

    return *filter == 0;
}

static int funcCompare(const void* a, const void* b)
{
    const tic_outline_item* item1 = (const tic_outline_item*)a;
    const tic_outline_item* item2 = (const tic_outline_item*)b;

    return strcmp(item1->pos, item2->pos);
}

void code_outline(Text* text, const char* filter, CodeItems* out)
{
    out->items = NULL;
    out->size = 0;

    const tic_script* config = text->options.script;

    if(config && config->getOutline)
    {
        s32 size = 0;
        const tic_outline_item* items = config->getOutline(text->src, &size);

        if(items)
        {
            for(const tic_outline_item *it = items, *end = items + size; it != end; ++it)
            {
                if(text->state[it->pos - text->src].syntax == SyntaxType_COMMENT)
                    continue;

                if(*filter && !isFilterMatch(it->pos, it->size, filter))
                    continue;

                out->items = realloc(out->items, (out->size + 1) * sizeof *out->items);
                out->items[out->size++] = *it;
            }
        }
    }

    if(out->size > 1)
        qsort(out->items, out->size, sizeof *out->items, funcCompare);
}

// The first byte after the given line, or the line's own terminator.
static const char* nextLine(const char* line)
{
    while(*line && *line != '\n')
        line++;

    return *line ? line + 1 : line;
}

void code_bookmarks(Text* text, CodeItems* out)
{
    out->items = NULL;
    out->size = 0;

    for(const char* line = text->src; *line; line = nextLine(line))
    {
        s32 size = 0;
        const char* note = text_bookmark_note(text, line, &size);

        if(note)
        {
            out->items = realloc(out->items, (out->size + 1) * sizeof *out->items);
            out->items[out->size++] = (tic_outline_item){.pos = (char*)note, .size = size};
        }
    }
}

const char* code_bookmark_prev(Text* text, const char* from)
{
    const char* found = NULL;

    for(const char* line = text->src; *line && line <= from; line = nextLine(line))
    {
        const char* note = text_bookmark_note(text, line, NULL);

        if(note)
            found = note;
    }

    return found;
}

const char* code_bookmark_next(Text* text, const char* from)
{
    const char* line = getLineByPos(text, (char*)from);

    // A position inside the line is past that line's bookmark.
    if(from > line)
        line = nextLine(line);

    for(; *line; line = nextLine(line))
    {
        const char* note = text_bookmark_note(text, line, NULL);

        if(note)
            return note;
    }

    return NULL;
}

const char* code_goto_line(Text* text, const char* lineText, s32* line)
{
    s32 at = atoi(lineText);

    if(at)
        at--;

    s32 count = getLinesCount(text);

    if(at > count)
        at = count;

    *line = at;

    // A line before the first leaves the caret at the end, as the walking
    // setCursorPosition did.
    if(at < 0)
        return text->src + strlen(text->src);

    const char* ptr = text->src;

    for(s32 y = 0; y < at && *ptr; y++)
        ptr = strchr(ptr, '\n') + 1;

    return ptr;
}

const char* code_search_down(const char* start, const char* from, const char* needle)
{
    (void)start;
    return strstr(from, needle);
}

const char* code_search_up(const char* start, const char* from, const char* needle)
{
    const char* ptr = from - 1;
    size_t len = strlen(needle);

    if(len > 0)
    {
        while(ptr >= start)
        {
            if(memcmp(ptr, needle, len) == 0)
                return ptr;

            ptr--;
        }
    }

    return NULL;
}
