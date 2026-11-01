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

#include "minimap.h"

#define CHARS_PER_PIXEL 2

s32 minimap_map_lines(s32 height)
{
    if(height < 0) height = 0;

    return height / MINIMAP_LINE_HEIGHT;
}

s32 minimap_top(s32 lines, s32 height, s32 scroll, s32 visible)
{
    if(lines < 1) lines = 1;
    if(visible < 0) visible = 0;
    if(scroll < 0) scroll = 0;

    s32 mapLines = minimap_map_lines(height);
    s32 maxTop = MAX(lines - mapLines, 0);

    // A file that fits shows from its first line.
    if(lines <= mapLines)
        return 0;

    s32 mapHeight = mapLines * MINIMAP_LINE_HEIGHT;
    s32 frameHeight = MIN(visible * MINIMAP_LINE_HEIGHT, mapHeight);
    s32 track = MAX(mapHeight - frameHeight, 0);
    s32 maxScroll = MAX(lines - visible, 0);

    if(track <= 0 || maxScroll <= 0)
        return CLAMP(scroll, 0, maxTop);

    // The frame is the map's scrollbar: its travel is the whole file.
    s32 scrolled = MIN(scroll, maxScroll);
    s32 frameY = scrolled * track / maxScroll;

    return CLAMP(scrolled - frameY / 2, 0, maxTop);
}

MinimapFrame minimap_viewport(s32 lines, s32 height, s32 scroll, s32 visible)
{
    if(lines < 1) lines = 1;
    if(visible < 0) visible = 0;
    if(scroll < 0) scroll = 0;
    if(height < 0) height = 0;

    s32 mapLines = minimap_map_lines(height);
    s32 mapHeight = mapLines * MINIMAP_LINE_HEIGHT;
    s32 frameHeight = MIN(visible * MINIMAP_LINE_HEIGHT, mapHeight);
    s32 top = minimap_top(lines, height, scroll, visible);
    s32 y = MAX(scroll - top, 0) * MINIMAP_LINE_HEIGHT;

    if(y + frameHeight > mapHeight)
        y = MAX(0, mapHeight - frameHeight);

    return (MinimapFrame){y, frameHeight};
}

s32 minimap_line_at(s32 y, s32 lines, s32 height, s32 scroll, s32 visible)
{
    if(lines < 1) lines = 1;

    s32 mapLines = minimap_map_lines(height);
    s32 top = minimap_top(lines, height, scroll, visible);

    if(mapLines <= 0)
        return top;

    y = CLAMP(y, 0, mapLines * MINIMAP_LINE_HEIGHT - 1);

    return CLAMP(top + y / MINIMAP_LINE_HEIGHT, 0, lines - 1);
}

s32 minimap_drag_scroll(s32 scroll, s32 delta, s32 lines, s32 height, s32 visible)
{
    if(lines < 1) lines = 1;
    if(visible < 0) visible = 0;
    if(scroll < 0) scroll = 0;

    s32 mapLines = minimap_map_lines(height);

    // A fitting file moves line by line, as the editor scrolls.
    if(lines <= mapLines)
        return CLAMP(scroll + delta / MINIMAP_LINE_HEIGHT, 0, lines - 1);

    s32 mapHeight = mapLines * MINIMAP_LINE_HEIGHT;
    s32 frameHeight = MIN(visible * MINIMAP_LINE_HEIGHT, mapHeight);
    s32 track = MAX(mapHeight - frameHeight, 0);
    s32 maxScroll = MAX(lines - visible, 0);

    if(track <= 0 || maxScroll <= 0)
        return CLAMP(scroll + delta, 0, maxScroll);

    // The frame is the map's scrollbar: the drag is even over the file.
    return CLAMP(scroll + delta * maxScroll / track, 0, maxScroll);
}

static bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static const char* lineEnd(const char* p)
{
    while(*p && *p != '\n')
        p++;

    return p;
}

static u8 syntaxColor(const MinimapView* view, s32 syntax)
{
    return ((const u8*)&view->theme)[syntax];
}

static void drawRun(const MinimapView* view, s32 row, s32 start, s32 end, u8 color)
{
    s32 x1 = start / CHARS_PER_PIXEL;
    s32 x2 = MAX(x1 + 1, (end + CHARS_PER_PIXEL - 1) / CHARS_PER_PIXEL);

    if(x1 >= MINIMAP_WIDTH)
        return;

    x2 = MIN(x2, MINIMAP_WIDTH);

    tic_api_rect(view->tic, view->x + x1, view->y + row, x2 - x1, 1, color);
}

static void drawLine(const MinimapView* view, s32 row, const char* start, const char* end,
                     const char* selStart, const char* selEnd)
{
    s32 column = 0;
    s32 run = -1;
    u8 runColor = 0;
    s32 selectedFrom = -1;
    s32 selectedTo = -1;

    for(const char* p = start; p < end; p++)
    {
        s32 width = *p == '\t' ? getTabColumnWidth(view->text, column) : 1;

        if(selStart && p >= selStart && p < selEnd)
        {
            if(selectedFrom < 0)
                selectedFrom = column;

            selectedTo = column + width;
        }

        if(isSpace(*p))
        {
            if(run >= 0)
            {
                drawRun(view, row, run, column, runColor);
                run = -1;
            }
        }
        else
        {
            u8 color = syntaxColor(view, getState(view->text, p)->syntax);

            if(run >= 0 && color != runColor)
            {
                drawRun(view, row, run, column, runColor);
                run = -1;
            }

            if(run < 0)
            {
                run = column;
                runColor = color;
            }
        }

        column += width;
    }

    if(run >= 0)
        drawRun(view, row, run, column, runColor);

    if(selectedFrom >= 0)
        drawRun(view, row, selectedFrom, selectedTo, view->theme.select);
}

void minimap_draw(const MinimapView* view)
{
    if(view->x >= TIC80_WIDTH)
        return;

    Text* text = view->text;
    const s32 lines = getLinesCount(text) + 1;
    const s32 height = view->height;
    const s32 mapLines = minimap_map_lines(height);
    const s32 topLine = minimap_top(lines, height, view->scroll, view->visible);
    MinimapFrame frame = minimap_viewport(lines, height, view->scroll, view->visible);
    bool bookmarked[TIC80_HEIGHT] = {0};
    s32 cursorRow = -1;
    const char* p = text->src;
    s32 line = 0;
    const char* selStart = NULL;
    const char* selEnd = NULL;

    if(text->cursor.selection)
    {
        selStart = MIN(text->cursor.selection, text->cursor.position);
        selEnd = MAX(text->cursor.selection, text->cursor.position);
    }

    // The map is darker than the editor; the viewport keeps the editor's own
    // background, so the frame reads as the part of the code on screen.
    tic_api_rect(view->tic, view->x, view->y, MINIMAP_WIDTH, height, tic_color_black);
    tic_api_rect(view->tic, view->x, view->y + frame.y, MINIMAP_WIDTH, frame.height, view->theme.BG);

    while(line < topLine && *p)
    {
        p = lineEnd(p);
        p += *p == '\n';
        line++;
    }

    for(; line < lines && line < topLine + mapLines; line++)
    {
        const char* start = p;
        const char* end = lineEnd(start);
        s32 row = (line - topLine) * MINIMAP_LINE_HEIGHT;

        if(text->cursor.position >= start && text->cursor.position <= end)
            cursorRow = row;

        if(text_bookmark_note(text, start, NULL))
            bookmarked[row] = true;

        drawLine(view, row, start, end, selStart, selEnd);

        p = *end == '\n' ? end + 1 : end;
    }

    tic_api_rectb(view->tic, view->x, view->y + frame.y, MINIMAP_WIDTH, frame.height, tic_color_white);

    // A bookmark line is one full-width yellow row on its stripe.
    for(s32 row = 0; row < height; row++)
        if(bookmarked[row])
            tic_api_rect(view->tic, view->x, view->y + row, MINIMAP_WIDTH, 1, tic_color_yellow);

    if(cursorRow >= 0 && view->cursorVisible)
    {
        s32 x = MIN(view->x + 1 + view->cursorColumn / CHARS_PER_PIXEL, view->x + MINIMAP_WIDTH - 2);

        tic_api_rect(view->tic, x, view->y + cursorRow, 1, 1, view->theme.cursor);
    }
}
