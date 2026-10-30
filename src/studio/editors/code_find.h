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
#include "code_text.h"

#include <stdbool.h>

// The code editor's search model: the outline, the bookmarks, goto and the
// plain-text search. It answers data; the editor owns the sidebar, the caret
// and the screen, and the module links without the studio.

typedef struct
{
    tic_outline_item* items;
    s32 size;

} CodeItems;

bool isFilterMatch(const char* buffer, s32 size, const char* filter);

// The language's outline, minus the items in comments, matching the filter and
// sorted by position. The caller owns the items.
void code_outline(Text* text, const char* filter, CodeItems* items);

// Every line whose first byte is bookmarked, in buffer order.
void code_bookmarks(Text* text, CodeItems* items);

// The nearest bookmarked byte in the given direction, or NULL at the edge.
const char* code_bookmark_prev(Text* text, const char* from);
const char* code_bookmark_next(Text* text, const char* from);

// The first byte of the line the goto popup names, clamped the way the editor
// clamps it; `line` is the resolved 0-based line.
const char* code_goto_line(Text* text, const char* lineText, s32* line);

// The first occurrence at or after `from`, or none; the wrapping is the
// caller's.
const char* code_search_down(const char* start, const char* from, const char* needle);
const char* code_search_up(const char* start, const char* from, const char* needle);
