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
#include "text.h"

#define MINIMAP_WIDTH       32
#define MINIMAP_LINE_HEIGHT 2

// The colours the map draws with, in the theme's order.
typedef struct
{
#define MINIMAP_COLOR_DEF(VAR) u8 VAR;
    CODE_COLORS_LIST(MINIMAP_COLOR_DEF)
#undef MINIMAP_COLOR_DEF

    u8 select;
    u8 cursor;

} MinimapTheme;

// What the map draws this frame: the text, the theme, and the column's place
// on the screen.
typedef struct
{
    tic_mem* tic;
    Text* text;
    MinimapTheme theme;

    s32 x;
    s32 y;
    s32 height;
    s32 scroll;
    s32 visible;
    s32 cursorColumn;
    bool cursorVisible;

} MinimapView;

typedef struct
{
    s32 y;
    s32 height;

} MinimapFrame;

// The map's geometry: a fixed two pixels per line and a viewport frame that
// stays one size. A file that fits shows from its first line with the frame
// at the viewport; a longer one scrolls its window with the frame, which is
// the map's scrollbar.
s32 minimap_map_lines(s32 height);
s32 minimap_top(s32 lines, s32 height, s32 scroll, s32 visible);
MinimapFrame minimap_viewport(s32 lines, s32 height, s32 scroll, s32 visible);
s32 minimap_line_at(s32 y, s32 lines, s32 height, s32 scroll, s32 visible);
s32 minimap_drag_scroll(s32 scroll, s32 delta, s32 lines, s32 height, s32 visible);

void minimap_draw(const MinimapView* view);
