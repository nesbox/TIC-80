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

#include "code.h"
#include "find.h"
#include "vi.h"
#include "studio/ui.h"
#include "studio/icons.h"
#include "studio/system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <math.h>

#define TEXT_CURSOR_DELAY        (TIC80_FRAMERATE / 2)
#define TEXT_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define MIN_GUTTER_WIDTH         7

// The right outline panel keeps its height when the status bar is hidden.
#define OUTLINE_HEIGHT (TIC80_HEIGHT - STUDIO_BAR_HEIGHT - STUDIO_TEXT_HEIGHT)
#define OUTLINE_ROWS   (OUTLINE_HEIGHT / STUDIO_TEXT_HEIGHT)

static s32 getCodeHeight(const Code* code)
{
    return TIC80_HEIGHT - code->view->editorTop - (code->showStatusbar ? STUDIO_TEXT_HEIGHT : 0);
}

static s32 getCodeTextHeight(const Code* code)
{
    return getCodeHeight(code) / STUDIO_TEXT_HEIGHT;
}
#define SIDEBAR_WIDTH (12 * TIC_FONT_WIDTH)

static bool keyPressed(Code* code, tic_key key)
{
    if(key == tic_key_escape && code->view->frame.escaped)
        return false;

    return keyboard_pressed(code->tic, key);
}

static bool enterPressed(Code* code)
{
    return keyboard_enter(code->tic, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

static char keyboardText(Code* code)
{
    return code->view->frame.captured ? '\0' : keyboard_text(code->tic);
}

static bool mousePos(Code* code, const tic_rect* rect)
{
    return !code->view->frame.pointerOwned && mouse_over(code->tic, rect);
}

static bool mouseClicked(Code* code, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_click(&code->deps.mouse[button], rect);
}

static bool mouseDblClicked(Code* code, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_dblclick(&code->deps.mouse[button], rect);
}

static bool mouseDown(Code* code, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_down(&code->deps.mouse[button], rect);
}

static bool viModeIs(Code* code, ViMode mode)
{
    return code->view->options.vi && code->deps.getViMode(code->deps.host) == mode;
}

#if defined(TIC80_PRO)
// The terminator needs a byte of the same array.
#define MAX_CODE (sizeof(tic_code) - 1)
#else
#define MAX_CODE TIC_BANK_SIZE
#endif

#define noop (void)0

//if pos_undo is true, we set the position to the first character
//that is different between the current source and the state (wanted undo behavior)
//otherwise we set the position to the stored location (wanted redo behavior)
//in either case if no change was detected we do not change the position
//(this occurs when there are no undo's or redo's left to apply)
tic_color getCodeColor(Code* code)
{
    const CodeBattle* bb = code->view->battle;

    if(bb)
    {
        s32 size = (s32)(strlen(code->text.src) - bb->limit.lower);

        if(size <= 0) return tic_color_light_green;

        s32 delta = (bb->limit.current - bb->limit.lower) / 2;

        if(size <= delta) return tic_color_yellow;
        if(size <= delta * 2) return tic_color_orange;
        return tic_color_red;
    }

    return tic_color_white;
}

static void getWidths(Code* code, s32* gutterWidthOut, s32* codeWidthOut)
{
    int charCount = code->lineNumbers ? ((int)log10(code->status.lineCount + 1) + 1) : 0;
    int gutterWidth = 0;

    if(code->showGutter)
        gutterWidth = code->lineNumbers
                          ? MAX(1 + charCount * TIC_ALTFONT_WIDTH, MIN_GUTTER_WIDTH)
                          : MIN_GUTTER_WIDTH;
    int codeWidth = TIC80_WIDTH - gutterWidth;

    if(gutterWidthOut) *gutterWidthOut = gutterWidth;
    if(codeWidthOut) *codeWidthOut = codeWidth;
}

static inline s32 getCodeEditorWidth(Code* code)
{
    s32 width;
    getWidths(code, NULL, &width);
    return width;
}

static inline s32 getGutterWidth(Code* code)
{
    s32 width;
    getWidths(code, &width, NULL);
    return width;
}

static void drawStatus(Code* code)
{
    if(!code->showStatusbar)
        return;

    enum
    {
        Height = STUDIO_TEXT_HEIGHT,
        StatusY = TIC80_HEIGHT - TIC_FONT_HEIGHT
    };

    const CodeBattle* bb = code->view->battle;

    if(bb)
    {
        tic_api_rect(code->tic, 0, TIC80_HEIGHT - Height, TIC80_WIDTH, Height, getCodeColor(code));
        if(!bb->battle.hidetime)
        {
            sprintf(code->status.size, "%i/%i", (u32)strlen(code->text.src), bb->limit.current);

            char buf[sizeof "00:00" + sizeof "-2147483648"];
            s32 sec = bb->battle.left / 1000;
            sprintf(buf, "%02i:%02i", sec / 60, sec % 60);

            tic_api_print(code->tic, buf, (TIC80_WIDTH - sizeof "00:00" * TIC_FONT_WIDTH) / 2,
                          StatusY, code->view->theme.BG, true, 1, false);
        }
    }
    else
    {
        tic_api_rect(code->tic, 0, TIC80_HEIGHT - Height, TIC80_WIDTH, Height, code->status.color);
    }

    tic_api_print(code->tic, code->status.line, 0, StatusY, code->view->theme.BG, true, 1, false);
    tic_api_print(code->tic, code->status.size, TIC80_WIDTH - (s32)strlen(code->status.size) * TIC_FONT_WIDTH,
                  StatusY, code->view->theme.BG, true, 1, false);
}

static void drawGutter(Code* code)
{
    if(!code->showGutter)
        return;

    tic_mem* tic = code->tic;
    const s32 gutterWidth = getGutterWidth(code);
    const s32 gutterHeight = getCodeHeight(code);
    tic_rect rect = {0, code->view->editorTop, gutterWidth, gutterHeight};
    s32 mouseOverLine = -1;

    // draw the background rectangle
    tic_api_rect(code->tic, rect.x, rect.y, rect.w, rect.h, tic_color_grey);

    // then if the mouse is hovering over the gutter...
    if(mousePos(code, &rect))
    {
        // draw a bookmark preview
        ui_cursor(code->tic, tic_cursor_hand);
        code->deps.tooltip(code->deps.host, "BOOKMARK [ctrl+f1]");
        s32 line = mouseOverLine = (tic_api_mouse(tic).y - rect.y) / STUDIO_TEXT_HEIGHT;
        ui_icon(code->tic, code->deps.tiles, tic_icon_bookmark, rect.x + gutterWidth - TIC_SPRITESIZE + 1, rect.y + line * STUDIO_TEXT_HEIGHT - 1, tic_color_dark_grey);
        // and toggle the bookmark if we clicked it
        if(mouseClicked(code, &rect, tic_mouse_left))
            toggleBookmark(&code->text, getPosByLine(code->text.src, line + code->scroll.y));
    }

    // find the first character on the screen, including syntax pointer
    const char* pointer = code->text.src;
    const CodeState* syntaxPointer = code->text.state;
    s32 y = -code->scroll.y;
    while(y < 0)
    {
        syntaxPointer++;
        if(*pointer++ == '\n')
            y++;
    }

    char buf[16] = "";
    bool somethingDrawn = false;
    s32 cursorX, cursorY;
    codeGetPos(code, &cursorX, &cursorY);

    // render all the bookmarks and line numbers
    while(*pointer && y < STUDIO_TEXT_BUFFER_HEIGHT)
    {
        if(syntaxPointer++->bookmark)
        {
            ui_icon(code->tic, code->deps.tiles, tic_icon_bookmark, rect.x + gutterWidth - TIC_SPRITESIZE + 1, rect.y + y * STUDIO_TEXT_HEIGHT, tic_color_black);
            ui_icon(code->tic, code->deps.tiles, tic_icon_bookmark, rect.x + gutterWidth - TIC_SPRITESIZE + 1, rect.y + y * STUDIO_TEXT_HEIGHT - 1, tic_color_yellow);
            somethingDrawn = true;
        }
        else if(!somethingDrawn && code->lineNumbers && y != mouseOverLine)
        {
            s32 lineNumber = y + 1 + code->scroll.y;
            snprintf(buf, sizeof(buf), "%d", lineNumber);
            somethingDrawn = true;
            s32 offset = (s32)strlen(buf) * TIC_ALTFONT_WIDTH;
            tic_api_print(tic, buf, rect.x + gutterWidth - offset, rect.y + y * STUDIO_TEXT_HEIGHT, y + code->scroll.y == cursorY ? tic_color_white : tic_color_dark_grey, true, 1, true);
        }

        if(*pointer++ == '\n')
        {
            somethingDrawn = false;
            y++;
        }
    }
}

static inline s32 getFontWidth(Code* code)
{
    return code->altFont ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH;
}

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color, bool alt)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, true, 1, alt);
}

static void drawTab(Code* code, s32 x, s32 y, s32 width, u8 color)
{
    tic_mem* tic = code->tic;

    for(s32 offset = 0; offset < width; offset += getFontWidth(code))
    {
        drawChar(tic, '\t', x, y, color, code->altFont);
        x += getFontWidth(code);
    }
}

static void drawCursor(Code* code, s32 x, s32 y, char symbol)
{
    bool inverse = code->cursor.delay || code->tickCounter % TEXT_CURSOR_BLINK_PERIOD < TEXT_CURSOR_BLINK_PERIOD / 2;

    if(viModeIs(code, VI_NORMAL))
        inverse = true;

    if(inverse)
    {
        bool alt = code->view->theme.altCaret;
        s32 width = alt ? (code->altFont ? 1 : 2) : getFontWidth(code) + 1;

        if(code->shadowText)
            tic_api_rect(code->tic, x, y, width, TIC_FONT_HEIGHT + 1, 0);

        tic_api_rect(code->tic, x - 1, y - 1, width, TIC_FONT_HEIGHT + 1, code->view->theme.cursor);

        if(symbol && !alt)
            drawChar(code->tic, symbol, x, y, code->view->theme.BG, code->altFont);
    }
}

static void drawMatchedDelim(Code* code, s32 x, s32 y, char symbol, u8 color)
{
    tic_api_rectb(code->tic, x - 1, y - 1, (getFontWidth(code)) + 1, TIC_FONT_HEIGHT + 1,
                  code->view->theme.cursor);
    drawChar(code->tic, symbol, x, y, color, code->altFont);
}

static void drawCode(Code* code, bool withCursor)
{
    s32 gutterWidth, codeWidth;
    getWidths(code, &gutterWidth, &codeWidth);
    tic_rect rect = {gutterWidth, code->view->editorTop, codeWidth, getCodeHeight(code)};

    s32 xStart = rect.x - code->scroll.x * getFontWidth(code);
    s32 x = xStart;
    s32 y = rect.y;
    const char* pointer = code->text.src;

    u8 selectColor = code->view->theme.select;

    const u8* colors = (const u8*)&code->view->theme;
    const CodeState* syntaxPointer = code->text.state;

    // Skip lines above the viewport
    s32 skippedLines = 0;
    while(*pointer && skippedLines < code->scroll.y)
    {
        if(*pointer == '\n')
        {
            skippedLines++;
        }
        pointer++;
        syntaxPointer++;
    }

    struct
    {
        char* start;
        char* end;
    } selection =
        {
            MIN(code->text.cursor.selection, code->text.cursor.position),
            MAX(code->text.cursor.selection, code->text.cursor.position)};

    struct
    {
        s32 x;
        s32 y;
        char symbol;
    } cursor = {-1, -1, 0};
    struct
    {
        s32 x;
        s32 y;
        char symbol;
        u8 color;
    } matchedDelim = {-1, -1, 0, 0};

    while(*pointer)
    {
        char symbol = *pointer;
        s32 x_offset = getFontWidth(code);
        s32 column = (x - xStart) / getFontWidth(code);

        if(symbol == '\t')
            x_offset *= getTabColumnWidth(&code->text, column);

        if(x >= -getFontWidth(code) && x < TIC80_WIDTH && y >= -TIC_FONT_HEIGHT && y < TIC80_HEIGHT)
        {
            if(code->text.cursor.selection && pointer >= selection.start && pointer < selection.end)
            {
                if(code->shadowText)
                    tic_api_rect(code->tic, x, y, getFontWidth(code) + 1, TIC_FONT_HEIGHT + 1, tic_color_black);

                if(symbol == '\t')
                {
                    //NOTE: this logic assumes that the tab character is blank
                    //is someone made a custom character for tab it won't show up
                    //in the selection
                    drawTab(code, x, y, x_offset, tic_color_dark_grey);
                    tic_api_rect(code->tic, x - 1, y - 1, x_offset, TIC_FONT_HEIGHT + 1, selectColor);
                }
                else
                {
                    tic_api_rect(code->tic, x - 1, y - 1, getFontWidth(code) + 1, TIC_FONT_HEIGHT + 1, selectColor);
                    drawChar(code->tic, symbol, x, y, tic_color_dark_grey, code->altFont);
                }
            }
            else
            {
                if(code->shadowText)
                    drawChar(code->tic, symbol, x + 1, y + 1, 0, code->altFont);

                if(symbol == '\t')
                    drawTab(code, x, y, x_offset, colors[syntaxPointer->syntax]);
                else
                    drawChar(code->tic, symbol, x, y, colors[syntaxPointer->syntax], code->altFont);
            }
        }

        if(code->text.cursor.position == pointer)
            cursor.x = x, cursor.y = y, cursor.symbol = symbol;

        if(code->matchedDelim == pointer)
        {
            matchedDelim.x = x, matchedDelim.y = y, matchedDelim.symbol = symbol,
            matchedDelim.color = colors[syntaxPointer->syntax];
        }

        if(symbol == '\n')
        {
            x = xStart;
            y += STUDIO_TEXT_HEIGHT;
            if(y >= TIC80_HEIGHT)
            {
                // break early if we are below the visible screen viewport
                break;
            }
        }
        else
            x += x_offset;

        pointer++;
        syntaxPointer++;
    }

    drawGutter(code);

    if(code->text.cursor.position == pointer)
        cursor.x = x, cursor.y = y;

    if(withCursor && cursor.x >= gutterWidth && cursor.y >= 0)
        drawCursor(code, cursor.x, cursor.y, cursor.symbol);

    if(matchedDelim.symbol)
    {
        drawMatchedDelim(code, matchedDelim.x, matchedDelim.y,
                         matchedDelim.symbol, matchedDelim.color);
    }
}

static void getCursorPosition(Code* code, s32* x, s32* y)
{
    s32 column = 0;
    s32 line = 0;

    const char* pointer = code->text.src;

    while(*pointer && code->text.cursor.position != pointer)
    {
        if(*pointer == '\n')
        {
            column = 0;
            line++;
        }
        else
            column++;

        pointer++;
    }

    if(x) *x = column;
    if(y) *y = line;
}

// The column the caret is drawn in: a tab covers the cells up to its stop, so
// the screen math counts it as the width it takes.
static s32 getVisualCursorColumn(Code* code)
{
    const char* pointer = code->text.src;
    s32 column = 0;

    while(*pointer && code->text.cursor.position != pointer)
    {
        if(*pointer == '\n')
            column = 0;
        else if(*pointer == '\t')
            column += getTabColumnWidth(&code->text, column);
        else
            column++;

        pointer++;
    }

    return column;
}

void codeGetPos(Code* code, s32* x, s32* y)
{
    getCursorPosition(code, x, y);
}

static void setCursorPosition(Code* code, s32 x, s32 y);

void codeSetPos(Code* code, s32 x, s32 y)
{
    setCursorPosition(code, x, y);
    parseSyntaxColor(&code->text);
    code->cursor.delay = 0;
}

static void removeInvalidChars(char* code)
{
    // remove \r symbol
    char* s;
    char* d;
    for(s = d = code; (*d = *s); d += (*s++ != '\r'))
        ;
}

static void updateEditor(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    if(code->view->options.matchDelimiters)
        code->matchedDelim = findMatchedDelim(&code->text, code->text.cursor.position);

    const s32 BufferWidth = getCodeEditorWidth(code) / getFontWidth(code);
    const s32 visualColumn = getVisualCursorColumn(code);

    if(visualColumn < code->scroll.x)
        code->scroll.x = visualColumn;
    else if(visualColumn >= code->scroll.x + BufferWidth)
        code->scroll.x = visualColumn - BufferWidth + 1;

    if(line < code->scroll.y)
        code->scroll.y = line;
    else if(line >= code->scroll.y + getCodeTextHeight(code))
        code->scroll.y = line - getCodeTextHeight(code) + 1;

    code->cursor.delay = TEXT_CURSOR_DELAY;

    code->status.lineCount = getLinesCount(&code->text);
    sprintf(code->status.line, "line %i/%i col %i", line + 1, code->status.lineCount + 1, column + 1);
    {
        s32 codeLen = (s32)strlen(code->text.src);
        sprintf(code->status.size, "size %i/%i", codeLen, (s32)MAX_CODE);
        code->status.color = codeLen > (s32)MAX_CODE ? tic_color_red : tic_color_white;
    }
}

static inline bool islineend(char c)
{
    return c == '\n' || c == '\0';
}
static inline bool isdoublequote(char c)
{
    return c == '"';
}
static inline bool isopenparen_(Code* code, char c)
{
    (void)code;
    return c == '(' || c == '{' || c == '[';
}

static void updateCursorPosition(Code* code, char* position)
{
    code->text.cursor.position = position;
    updateColumn(&code->text);
    updateEditor(code);
}

static void setCursorPositionVisual(Code* code, s32 cx, s32 cy)
{
    char* line = getPosByLine(code->text.src, cy);
    updateCursorPosition(code, getVisualPosByLine(&code->text, line, cx));
}

static void setCursorPosition(Code* code, s32 cx, s32 cy)
{
    s32 x = 0;
    s32 y = 0;
    char* pointer = code->text.src;

    while(*pointer)
    {
        if(y == cy && x == cx)
        {
            updateCursorPosition(code, pointer);
            return;
        }

        if(*pointer == '\n')
        {
            if(y == cy && cx > x)
            {
                updateCursorPosition(code, pointer);
                return;
            }

            x = 0;
            y++;
        }
        else
            x++;

        pointer++;
    }

    updateCursorPosition(code, pointer);
}

static void pageUp(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    setCursorPosition(code, column, line > getCodeTextHeight(code) ? line - getCodeTextHeight(code) : 0);
}

static void pageDown(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    s32 lines = getLinesCount(&code->text);
    setCursorPosition(code, column, line < lines - getCodeTextHeight(code) ? line + getCodeTextHeight(code) : lines);
}

static void halfPageUp(Code* code)
{
    s32 half = getCodeTextHeight(code) / 2;
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    setCursorPosition(code, column, line > half ? line - half : 0);
}
static void halfPageDown(Code* code)
{
    s32 half = getCodeTextHeight(code) / 2;
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    s32 lines = getLinesCount(&code->text);
    setCursorPosition(code, column, line < lines - half ? line + half : lines);
}

static void deleteChar(Code* code)
{
    text_delete_char(&code->text);
    updateEditor(code);
}
static void backspaceChar(Code* code)
{
    text_backspace_char(&code->text);
    updateEditor(code);
}
static void deleteLine(Code* code)
{
    char* linestart = code->text.cursor.position;
    char* lineend = linestart + 1;

    if(text_structured(&code->text))
    {
        if(islineend(*linestart) || islineend(*lineend))
            noop;
        else if(isopenparen_(code, *linestart))
            lineend = rightSexp(&code->text, linestart + 1) + 1;
        else if(isdoublequote(*linestart))
        {
            char* nextQuote = rightQuote(&code->text, linestart + 1);
            lineend = findLineEnd(&code->text, linestart);
            if(nextQuote < lineend)
                lineend = nextQuote + 1;
            else
                return; // skip deletion if on last " of a string
        }
        else
        {
            const char* start = code->text.src;
            const char* sexpStart = leftSexp(&code->text, linestart);
            const char* strStart = leftQuote(&code->text, linestart);
            char* sexpEnd = rightSexp(&code->text, linestart);
            char* strEnd = rightQuote(&code->text, linestart);
            const bool isInsideStr = strStart > sexpStart && strEnd < sexpEnd;
            if(isInsideStr)
                lineend = strEnd;
            else if(sexpStart != start)
                lineend = sexpEnd;
            else
                lineend = findLineEnd(&code->text, linestart);
        }
    }
    else
    {
        lineend = findLineEnd(&code->text, linestart);
    }

    const size_t linesize = lineend - linestart;
    char* clipboard = (char*)malloc(linesize + 1);
    if(clipboard)
    {
        memcpy(clipboard, linestart, linesize);
        clipboard[linesize] = '\0';
        tic_sys_clipboard_set(clipboard);
        free(clipboard);
    }

    deleteCode(&code->text, linestart, lineend);
    code->text.cursor.position = linestart;
    history(&code->text);
    parseSyntaxColor(&code->text);
    updateEditor(code);
}

static void newLine(Code* code)
{
    if(text_new_line(&code->text))
        updateEditor(code);
}
static void copyToClipboard(Code* code, bool killSelection)
{
    char* pos = code->text.cursor.position;
    char* sel = code->text.cursor.selection;

    char* start = NULL;
    size_t size = 0;

    if(sel && sel != pos)
    {
        start = MIN(sel, pos);
        size = MAX(sel, pos) - start;
    }
    else
    {
        start = getLine(&code->text);
        size = getNextLine(&code->text) - start;
    }

    char* clipboard = (char*)malloc(size + 1);

    if(clipboard)
    {
        memcpy(clipboard, start, size);
        clipboard[size] = '\0';
        tic_sys_clipboard_set(clipboard);
        free(clipboard);
    }

    if(killSelection)
        code->text.cursor.selection = NULL;
}

static void cutToClipboard(Code* code, bool killSelection)
{
    if(code->text.cursor.selection == NULL || code->text.cursor.position == code->text.cursor.selection)
    {
        code->text.cursor.position = getLine(&code->text);
        code->text.cursor.selection = getNextLine(&code->text);
    }

    copyToClipboard(code, false);
    replaceSelection(&code->text);

    if(killSelection)
        code->text.cursor.selection = NULL;

    //no call to history because it gets called in replaceSelection
}

static void copyFromClipboard(Code* code, bool killSelection)
{
    if(tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        if(clipboard)
        {
            removeInvalidChars(clipboard);
            size_t size = strlen(clipboard);

            if(size)
            {
                replaceSelection(&code->text);

                // cut clipboard code if overall code > max code size
                {
                    size_t codeSize = strlen(code->text.src);

                    if(codeSize >= MAX_CODE)
                        return;

                    if(codeSize + size > MAX_CODE)
                    {
                        size = MAX_CODE - codeSize;
                        clipboard[size] = '\0';
                    }
                }

                insertCode(&code->text, code->text.cursor.position, clipboard);
                code->text.cursor.position += size;

                if(killSelection)
                    code->text.cursor.selection = NULL;

                history(&code->text);
                parseSyntaxColor(&code->text);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void updateTextOptions(Code* code)
{
    const CodeOptions* options = &code->view->options;
    const struct tic_script* script = options->script;
    const bool scriptChanged = code->text.options.script != script;

    code->text.options = (TextOptions){
        .script = script,
        .tabSize = options->tabSize,
        .spacesForTab = options->spacesForTab,
        .structuredEdit = options->structuredEdit,
        .autoDelimiters = options->autoDelimiters,
    };

    // The language changed, or the first real view replaced the placeholder:
    // re-colour the text; the edits parse on their own.
    if(scriptChanged)
        parseSyntaxColor(&code->text);
}

// A vi insert session keeps its edits out of the history until it ends.
static bool textHistoryEnabled(void* host)
{
    Code* code = host;

    return !viModeIs(code, VI_INSERT);
}

static void update(Code* code)
{
    updateTextOptions(code);
    updateColumn(&code->text);
    updateEditor(code);
    parseSyntaxColor(&code->text);
}

static void undo(Code* code)
{
    text_undo(&code->text);

    update(code);
}

static void redo(Code* code)
{
    text_redo(&code->text);

    update(code);
}

//has no effect if pos is not a valid tab character
static void doTab(Code* code, bool shift, bool ctrl)
{
    if(text_do_tab(&code->text, shift, ctrl))
        updateEditor(code);
}
// Add a block-ending keyword or symbol, and put the cursor in the line between.
static void newLineAutoClose(Code* code)
{
    text_new_line_auto_close(&code->text);
    updateEditor(code);
}
static void setFindOrReplaceMode(Code* code)
{
    if(code->text.cursor.selection)
    {
        const char* end = MAX(code->text.cursor.position, code->text.cursor.selection);
        const char* start = MIN(code->text.cursor.position, code->text.cursor.selection);
        size_t len = end - start;

        if(len > 0 && len < sizeof code->popup.text - 1)
        {
            memset(code->popup.text, 0, sizeof code->popup.text);
            memcpy(code->popup.text, start, len);
        }
    }
}

static void setGotoMode(Code* code)
{
    code->jump.line = -1;
}

static void normalizeScroll(Code* code)
{
    if(code->scroll.x < 0) code->scroll.x = 0;
    if(code->scroll.y < 0)
        code->scroll.y = 0;
    else
    {
        s32 lines = getLinesCount(&code->text);
        if(code->scroll.y > lines) code->scroll.y = lines;
    }
}

static void recenterScroll(Code* code, bool emacsMode)
{
    s32 line = 0;
    getCursorPosition(code, NULL, &line);

    s32 desiredCol = getVisualCursorColumn(code) - getCodeEditorWidth(code) / getFontWidth(code) / 2;
    s32 desiredLine = MAX(0, line - getCodeTextHeight(code) / 2);

    if(emacsMode && code->scroll.y == desiredLine)
    {
        desiredLine = line;
    }
    else if(emacsMode && code->scroll.y == line)
    {
        desiredLine = line - getCodeTextHeight(code);
    }

    code->scroll.x = desiredCol;
    code->scroll.y = desiredLine;

    normalizeScroll(code);
}

static void centerScroll(Code* code)
{
    static const bool emacsMode = false;
    recenterScroll(code, emacsMode);
}

static void updateSidebarCode(Code* code)
{

    const tic_outline_item* item = code->sidebar.items + code->sidebar.index;

    if(code->sidebar.size && item && item->pos)
    {
        code->text.cursor.position = (char*)item->pos;
        code->text.cursor.selection = (char*)item->pos + item->size;
    }
    else
    {
        code->text.cursor.position = code->text.src;
        code->text.cursor.selection = NULL;
    }

    centerScroll(code);
    updateEditor(code);
}

static void drawFilterMatch(Code* code, s32 x, s32 y, const char* orig, s32 size, const char* filter)
{
    while(size--)
    {
        bool match = tolower((u8)(*orig)) == tolower((u8)(*filter));
        u8 color = match ? tic_color_orange : tic_color_white;

        if(code->shadowText)
            drawChar(code->tic, *orig, x + 1, y + 1, tic_color_black, code->altFont);

        drawChar(code->tic, *orig, x, y, color, code->altFont);
        x += getFontWidth(code);
        if(match)
            filter++;

        orig++;
    }
}

static void initSidebarMode(Code* code)
{
    CodeItems items;
    code_outline(&code->text, code->popup.text, &items);

    free(code->sidebar.items);
    code->sidebar.items = items.items;
    code->sidebar.size = items.size;
}

static void setBookmarkMode(Code* code)
{
    code->sidebar.index = 0;
    code->sidebar.scroll = 0;

    CodeItems items;
    code_bookmarks(&code->text, &items);

    free(code->sidebar.items);
    code->sidebar.items = items.items;
    code->sidebar.size = items.size;

    updateSidebarCode(code);
}

static void setOutlineMode(Code* code)
{
    code->sidebar.index = 0;
    code->sidebar.scroll = 0;

    initSidebarMode(code);

    updateSidebarCode(code);
}

static bool isIdle(Code* code)
{
    return code->anim.movie == &code->anim.idle;
}

static void setCodeMode(Code* code, s32 mode)
{
    if(isIdle(code) && (s32)code->mode != mode)
    {
        code->anim.movie = resetMovie(&code->anim.show);

        //so that in edit mode we can refer back to the most recently
        //searched thing if we want
        if(mode != TEXT_EDIT_MODE)
            strcpy(code->popup.text, "");

        code->popup.offset = NULL;
        code->popup.prevPos = code->text.cursor.position;
        code->popup.prevSel = code->text.cursor.selection;

        switch(mode)
        {
            case TEXT_FIND_MODE: setFindOrReplaceMode(code); break;
            case TEXT_REPLACE_MODE: setFindOrReplaceMode(code); break;
            case TEXT_GOTO_MODE: setGotoMode(code); break;
            case TEXT_BOOKMARK_MODE: setBookmarkMode(code); break;
            case TEXT_OUTLINE_MODE: setOutlineMode(code); break;
            default: break;
        }

        code->mode = mode;
    }
}

void codeFind(Code* code)
{
    setCodeMode(code, TEXT_FIND_MODE);
}

void codeGoto(Code* code)
{
    setCodeMode(code, TEXT_GOTO_MODE);
}

void codeBookmarks(Code* code)
{
    setCodeMode(code, TEXT_BOOKMARK_MODE);
}

void codeOutline(Code* code)
{
    setCodeMode(code, TEXT_OUTLINE_MODE);
}

// moves a cursor end past an edit of delta chars at at, so that it stays with
// its text: an insert pushes later ends on, a delete pulls the ends inside it
// back to its start
static void sexpify(Code* code)
{
    if(text_sexpify(&code->text))
        updateEditor(code);
}
static void extirpSExp(Code* code)
{
    if(text_extirp_sexp(&code->text))
        updateEditor(code);
}
static void toggleMark(Code* code)
{
    text_toggle_mark(&code->text);
    updateEditor(code);
}
static void dupLine(Code* code)
{
    if(text_duplicate_line(&code->text))
        updateEditor(code);
}
static bool goPrevBookmark(Code* code, char* ptr)
{
    const char* pos = code_bookmark_prev(&code->text, ptr);

    if(pos)
    {
        updateCursorPosition(code, (char*)pos);
        centerScroll(code);
        return true;
    }

    return false;
}

static bool goNextBookmark(Code* code, char* ptr)
{
    const char* pos = code_bookmark_next(&code->text, ptr);

    if(pos)
    {
        updateCursorPosition(code, (char*)pos);
        centerScroll(code);
        return true;
    }

    return false;
}

static bool clipboardHasNewline()
{
    bool found = false;
    if(tic_sys_clipboard_has())
    {

        //from what I can see in other usage, this should be a
        //null terminated string with the clipboard contents
        char* clipboard = tic_sys_clipboard_get();

        char* c = clipboard;
        while(*c != 0)
        {
            if(*c == '\n')
            {
                found = true;
                break;
            }

            c++;
        }

        tic_sys_clipboard_free(clipboard);
    }
    return found;
}

static void updateGotoCode(Code* code);
//pass in pointer to beginnign of word and its length
//so you can just use the word in src
// The host the vi machine runs on: the editor's own state and screen.
static ViMode viGetViMode(void* host)
{
    Code* code = host;

    return code->deps.getViMode(code->deps.host);
}

static void viSetViMode(void* host, ViMode mode)
{
    Code* code = host;

    code->deps.setViMode(code->deps.host, mode);
}

static void viSetEditorMode(void* host, CodeViEdit mode)
{
    static const s32 Modes[] =
        {
            TEXT_FIND_MODE,
            TEXT_REPLACE_MODE,
            TEXT_GOTO_MODE,
            TEXT_BOOKMARK_MODE,
            TEXT_OUTLINE_MODE,
        };

    setCodeMode(host, Modes[mode]);
}

static void viUpdateView(void* host)
{
    updateEditor(host);
}

static void viRecenter(void* host, bool emacs)
{
    recenterScroll(host, emacs);
}

static void viPage(void* host, s32 dir, bool half)
{
    Code* code = host;

    if(half)
    {
        if(dir < 0)
            halfPageUp(code);
        else
            halfPageDown(code);
    }
    else
    {
        if(dir < 0)
            pageUp(code);
        else
            pageDown(code);
    }
}

static void viGotoUpdate(void* host)
{
    updateGotoCode(host);
}

static void viRunGame(void* host)
{
    Code* code = host;

    code->deps.runGame(code->deps.host);
}

static void viSaveProject(void* host)
{
    Code* code = host;

    code->deps.saveProject(code->deps.host);
}

static void viViewToggle(void* host, bool font)
{
    Code* code = host;

    code->deps.viewToggle(code->deps.host, font ? CODE_VIEW_FONT : CODE_VIEW_SHADOW);
}

static void viClipboard(void* host, CodeViClipboard op)
{
    Code* code = host;

    switch(op)
    {
        case CODE_VI_COPY: copyToClipboard(code, false); break;
        case CODE_VI_CUT: cutToClipboard(code, false); break;
        case CODE_VI_PASTE: copyFromClipboard(code, false); break;
    }
}

static bool viClipboardNewline(void* host)
{
    (void)host;

    return clipboardHasNewline();
}

static void processKeyboard(Code* code)
{
    tic_mem* tic = code->tic;

    const bool emacsMode = code->view->options.emacs;

    if(code->view->options.vi)
    {
        CodeViHost vi =
            {
                .host = code,
                .tic = code->tic,
                .captured = code->view->frame.captured,
                .escaped = code->view->frame.escaped,
                .clipboardEvent = code->view->frame.clipboard,
                .search = code->popup.text,
                .searchSize = sizeof code->popup.text,

                .getViMode = viGetViMode,
                .setViMode = viSetViMode,
                .setEditorMode = viSetEditorMode,
                .updateView = viUpdateView,
                .recenter = viRecenter,
                .page = viPage,
                .gotoUpdate = viGotoUpdate,
                .runGame = viRunGame,
                .saveProject = viSaveProject,
                .viewToggle = viViewToggle,
                .clipboard = viClipboard,
                .clipboardNewline = viClipboardNewline,
            };

        code_vi_tick(&code->text, &vi);
        return;
    }

    if(!emacsMode)
    {
        bool usedClipboard = true;
        switch(code->view->frame.clipboard)
        {
            case TIC_CLIPBOARD_CUT: cutToClipboard(code, false); break;
            case TIC_CLIPBOARD_COPY: copyToClipboard(code, false); break;
            case TIC_CLIPBOARD_PASTE: copyFromClipboard(code, false); break;
            default: usedClipboard = false; break;
        }

        if(usedClipboard)
        {
            updateEditor(code);
            return;
        }
    }

    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool alt = tic_api_key(tic, tic_key_alt);

    bool changedSelection = false;
    // clang-format off
    if(keyPressed(code, tic_key_up)
        || keyPressed(code, tic_key_down)
        || keyPressed(code, tic_key_left)
        || keyPressed(code, tic_key_right)
        || keyPressed(code, tic_key_home)
        || keyPressed(code, tic_key_end)
        || keyPressed(code, tic_key_pageup)
        || keyPressed(code, tic_key_pagedown))
    // clang-format on
    {
        changedSelection = true;
        if(!shift)
            code->text.cursor.selection = NULL;
        else if(code->text.cursor.selection == NULL)
            code->text.cursor.selection = code->text.cursor.position;
        else
            changedSelection = false;
    }

    bool usedKeybinding = true;
    // The Alt branch reads the text even when no binding uses it.
    char text = '\0';

    if(code->justSwitched)
    {
        code->justSwitched = false;
        return; // Skip processing other inputs for this frame
    }

    // handle bookmarks
    if(keyPressed(code, tic_key_f1))
    {
        if(ctrl && shift)
        {
            for(CodeState *s = code->text.state, *end = s + TIC_CODE_SIZE; s != end; ++s)
                s->bookmark = 0;
        }
        else if(ctrl)
        {
            toggleBookmark(&code->text, getLineByPos(&code->text, code->text.cursor.position));
        }
        else if(shift)
        {
            if(!goPrevBookmark(code, getPrevLineByPos(&code->text, code->text.cursor.position)))
                goPrevBookmark(code, code->text.src + strlen(code->text.src));
        }
        else
        {
            if(!goNextBookmark(code, getNextLineByPos(&code->text, code->text.cursor.position)))
                goNextBookmark(code, code->text.src);
        }
    }
    else if(ctrl || alt)
    {
        bool ctrlHandled = false;
        if(ctrl)
        {
            ctrlHandled = true;
            if(keyPressed(code, tic_key_tab))
                doTab(code, shift, ctrl);
            else if(keyPressed(code, tic_key_a))
                emacsMode ? startLine(&code->text) : selectAll(&code->text);
            else if(keyPressed(code, tic_key_z))
                undo(code);
            else if(keyPressed(code, tic_key_y))
                emacsMode ? copyFromClipboard(code, true) : redo(code);
            else if(keyPressed(code, tic_key_w))
                emacsMode ? cutToClipboard(code, true) : noop;
            else if(keyPressed(code, tic_key_f))
                emacsMode ? rightColumn(&code->text) : setCodeMode(code, TEXT_FIND_MODE);
            else if(keyPressed(code, tic_key_g))
                emacsMode ? killSelection(&code->text) : setCodeMode(code, TEXT_GOTO_MODE);
            else if(keyPressed(code, tic_key_b))
                emacsMode ? leftColumn(&code->text) : setCodeMode(code, TEXT_BOOKMARK_MODE);
            else if(keyPressed(code, tic_key_o))
                setCodeMode(code, TEXT_OUTLINE_MODE);
            else if(keyPressed(code, tic_key_n))
                downLine(&code->text);
            else if(keyPressed(code, tic_key_p))
                upLine(&code->text);
            else if(keyPressed(code, tic_key_e))
                endLine(&code->text);
            else if(keyPressed(code, tic_key_d))
                emacsMode ? deleteChar(code) : dupLine(code);
            else if(keyPressed(code, tic_key_j))
                newLine(code);
            else if(keyPressed(code, tic_key_slash))
                emacsMode ? undo(code) : commentLine(&code->text);
            else if(keyPressed(code, tic_key_home))
                goCodeHome(&code->text);
            else if(keyPressed(code, tic_key_end))
                goCodeEnd(&code->text);
            else if(keyPressed(code, tic_key_up))
                extirpSExp(code);
            else if(keyPressed(code, tic_key_down))
                sexpify(code);
            else if(keyPressed(code, tic_key_k))
                deleteLine(code);
            else if(keyPressed(code, tic_key_space))
                emacsMode ? toggleMark(code) : noop;
            else if(keyPressed(code, tic_key_l))
                recenterScroll(code, emacsMode);
            else if(keyPressed(code, tic_key_v))
                emacsMode ? pageDown(code) : noop;
            else if(shift && keyPressed(code, tic_key_minus))
                emacsMode ? undo(code) : noop;
            else
                ctrlHandled = false;
        }

        bool altHandled = false;
        if(alt)
        {
            text = keyboardText(code);
            char sym = text;
            altHandled = true;
            if(keyPressed(code, tic_key_p))
                pageUp(code);
            else if(keyPressed(code, tic_key_n))
                pageDown(code);
            else if(keyPressed(code, tic_key_v))
                emacsMode ? pageUp(code) : noop;
            else if(keyPressed(code, tic_key_b))
                leftWord(&code->text);
            else if(keyPressed(code, tic_key_f))
                rightWord(&code->text);
            else if(keyPressed(code, tic_key_k))
                emacsMode ? dupLine(code) : noop;
            else if(keyPressed(code, tic_key_d))
                emacsMode ? text_delete_word(&code->text) : noop;
            else if(keyPressed(code, tic_key_r))
                emacsMode ? extirpSExp(code) : noop;
            else if(keyPressed(code, tic_key_w))
                emacsMode ? copyToClipboard(code, true) : noop;
            else if(keyPressed(code, tic_key_g))
                emacsMode ? setCodeMode(code, TEXT_GOTO_MODE) : noop;
            else if(keyPressed(code, tic_key_s))
                emacsMode ? setCodeMode(code, TEXT_FIND_MODE) : noop;
            else if(shift && sym && sym == '<')
                emacsMode ? goCodeHome(&code->text) : noop;
            else if(shift && sym && sym == '>')
                emacsMode ? goCodeEnd(&code->text) : noop;
            else if(shift && sym && sym == '(')
                emacsMode ? sexpify(code) : noop;
            else if(keyPressed(code, tic_key_slash))
                emacsMode ? redo(code) : noop;
            else if(keyPressed(code, tic_key_semicolon))
                emacsMode ? commentLine(&code->text) : noop;
            else if(keyPressed(code, tic_key_l) && !ctrl)
                code->deps.viewToggle(code->deps.host, CODE_VIEW_LINE_NUMBERS);
            else
                altHandled = false;
        }

        bool ctrlAltHandled = true;
        if(keyPressed(code, tic_key_left))
            leftWord(&code->text);
        else if(keyPressed(code, tic_key_right))
            rightWord(&code->text);
        else if(keyPressed(code, tic_key_delete))
            text_delete_word(&code->text);
        else if(keyPressed(code, tic_key_backspace))
            text_backspace_word(&code->text);
        else
            ctrlAltHandled = false;

        usedKeybinding = (ctrlHandled || altHandled || ctrlAltHandled);
    }
    else
    {
        if(keyPressed(code, tic_key_up))
            upLine(&code->text);
        else if(keyPressed(code, tic_key_down))
            downLine(&code->text);
        else if(keyPressed(code, tic_key_left))
            leftColumn(&code->text);
        else if(keyPressed(code, tic_key_right))
            rightColumn(&code->text);
        else if(keyPressed(code, tic_key_home))
            goHome(&code->text);
        else if(keyPressed(code, tic_key_end))
            goEnd(&code->text);
        else if(keyPressed(code, tic_key_pageup))
            pageUp(code);
        else if(keyPressed(code, tic_key_pagedown))
            pageDown(code);
        else if(keyPressed(code, tic_key_delete))
            deleteChar(code);
        else if(keyPressed(code, tic_key_backspace))
            backspaceChar(code);
        else if(enterPressed(code))
            newLine(code);
        else if(keyPressed(code, tic_key_tab))
            doTab(code, shift, ctrl);
        else
            usedKeybinding = false;
    }

    if(!usedKeybinding)
    {
        if(shift && enterPressed(code))
        {
            newLineAutoClose(code);
            usedKeybinding = true;
        }
    }

    if(changedSelection || usedKeybinding)
        updateEditor(code);
    else
    {
        char sym = text ? text : keyboardText(code);
        if(sym)
        {
            inputSymbol(&code->text, sym);
            updateEditor(code);
        }
    }
}

static void processMouse(Code* code)
{
    tic_mem* tic = code->tic;
    s32 gutterWidth, codeWidth;
    getWidths(code, &gutterWidth, &codeWidth);
    tic_rect rect = {gutterWidth, code->view->editorTop, codeWidth, getCodeHeight(code)};

    if(mousePos(code, &rect))
    {
        bool useDrag = mouseDown(code, &rect, tic_mouse_right);
        ui_cursor(code->tic, useDrag ? tic_cursor_hand : tic_cursor_ibeam);

        if(code->scroll.active)
        {
            if(useDrag)
            {
                code->scroll.x = (code->scroll.start.x - tic_api_mouse(tic).x) / getFontWidth(code);
                code->scroll.y = (code->scroll.start.y - tic_api_mouse(tic).y) / STUDIO_TEXT_HEIGHT;

                normalizeScroll(code);
            }
            else
                code->scroll.active = false;
        }
        else
        {
            if(useDrag)
            {
                code->scroll.active = true;

                code->scroll.start.x = tic_api_mouse(tic).x + code->scroll.x * getFontWidth(code);
                code->scroll.start.y = tic_api_mouse(tic).y + code->scroll.y * STUDIO_TEXT_HEIGHT;
            }
            else
            {
                if(mouseDblClicked(code, &rect, tic_mouse_left))
                {
                    code->text.cursor.selection = leftWordPos(&code->text);
                    code->text.cursor.position = rightWordPos(&code->text);
                }
                else if(mouseDown(code, &rect, tic_mouse_left))
                {
                    s32 mx = tic_api_mouse(tic).x;
                    s32 my = tic_api_mouse(tic).y;

                    s32 x = (mx - rect.x) / getFontWidth(code);
                    s32 y = (my - rect.y) / STUDIO_TEXT_HEIGHT;

                    char* position = code->text.cursor.position;
                    setCursorPositionVisual(code, x + code->scroll.x, y + code->scroll.y);

                    if(tic_api_key(tic, tic_key_shift))
                    {
                        code->text.cursor.selection = code->text.cursor.position;
                        code->text.cursor.position = position;
                    }
                    else if(!code->cursor.mouseDownPosition)
                    {
                        code->text.cursor.selection = code->text.cursor.position;
                        code->cursor.mouseDownPosition = code->text.cursor.position;
                    }
                }
                else
                {
                    if(code->cursor.mouseDownPosition == code->text.cursor.position)
                        code->text.cursor.selection = NULL;

                    code->cursor.mouseDownPosition = NULL;
                }
            }
        }
    }
}

static void textEditTick(Code* code)
{
    tic_mem* tic = code->tic;

    // process scroll
    {
        tic80_input* input = &code->tic->ram->input;

        tic_point scroll = {input->mouse.scrollx, input->mouse.scrolly};

        if(tic_api_key(tic, tic_key_shift))
            scroll.x = scroll.y;

        s32* val = scroll.x ? &code->scroll.x : scroll.y ? &code->scroll.y
                                                         : NULL;

        if(val)
        {
            enum
            {
                Scroll = 3
            };
            s32 delta = scroll.x ? scroll.x : scroll.y;
            *val += delta > 0 ? -Scroll : Scroll;
            normalizeScroll(code);
        }
    }

    processKeyboard(code);
    processMouse(code);

    tic_api_cls(code->tic, code->view->theme.BG);

    drawCode(code, true);
    drawStatus(code);
}

static void drawPopupBar(Code* code, const char* title)
{
    s32 pos = code->anim.pos;
    const s32 TextX = getGutterWidth(code);

    const s32 bar = code->view->editorTop;
    tic_api_rect(code->tic, 0, bar + pos, TIC80_WIDTH, TIC_FONT_HEIGHT + 1, tic_color_grey);

    s32 textY = (bar + 1) + pos;

    if(code->shadowText)
        tic_api_print(code->tic, title, TextX + 1, textY + 1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, title, TextX, textY, tic_color_white, true, 1, code->altFont);

    if(code->shadowText)
        tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code) + 1, textY + 1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code), textY, tic_color_white, true, 1, code->altFont);

    drawCursor(code, TextX + (s32)(strlen(title) + strlen(code->popup.text)) * getFontWidth(code), textY, ' ');
}

static void updateFindCode(Code* code, const char* pos)
{
    if(pos)
    {
        code->text.cursor.position = (char*)pos;
        code->text.cursor.selection = (char*)pos + strlen(code->popup.text);

        centerScroll(code);
        updateColumn(&code->text);
        updateEditor(code);
    }
}

static void textFindTick(Code* code)
{
    if(enterPressed(code))
        setCodeMode(code, TEXT_EDIT_MODE);
    else if(keyPressed(code, tic_key_up) || keyPressed(code, tic_key_down) || keyPressed(code, tic_key_left) || keyPressed(code, tic_key_right))
    {
        if(*code->popup.text)
        {
            bool reverse = keyPressed(code, tic_key_up) || keyPressed(code, tic_key_left);
            const char* (*func)(const char*, const char*, const char*) = reverse ? code_search_up : code_search_down;
            char* from = reverse ? MIN(code->text.cursor.position, code->text.cursor.selection) : MAX(code->text.cursor.position, code->text.cursor.selection);
            const char* pos = func(code->text.src, from, code->popup.text);
            if(!pos && !reverse)
            {
                // If not found in forward search, try from the beginning
                pos = func(code->text.src, code->text.src, code->popup.text);
            }
            else if(!pos && reverse)
            {
                // If not found in reverse search, try from the end
                pos = func(code->text.src, code->text.src + strlen(code->text.src), code->popup.text);
            }
            updateFindCode(code, pos);
        }
    }
    else if(keyPressed(code, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text) - 1] = '\0';
            updateFindCode(code, strstr(code->text.src, code->popup.text));
        }
    }

    char sym = keyboardText(code);
    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);

            // Start searching after the current cursor position
            char* pos = strstr(code->text.cursor.position, code->popup.text);
            if(!pos)
            {
                // If no match, search from the beginning
                pos = strstr(code->text.src, code->popup.text);
            }

            updateFindCode(code, pos);
        }
    }

    tic_api_cls(code->tic, code->view->theme.BG);

    drawCode(code, false);
    drawPopupBar(code, "FIND:");
    drawStatus(code);
}

static void textReplaceTick(Code* code)
{

    if(enterPressed(code))
    {
        if(*code->popup.text && code->popup.offset == NULL) //still in "find" mode
        {
            code->popup.offset = code->popup.text + strlen(code->popup.text);
            strcat(code->popup.text, " WITH:");
        }
        else if(*code->popup.text)
        {
            *code->popup.offset = 0;
            code->popup.offset += strlen(" WITH:");
            //execute the replace

            //if we have a selection only replace within the selection
            char* start = code->text.src;
            if(code->text.cursor.selection != NULL)
                start = code->text.cursor.selection;

            char* pos = (char*)code_search_down(start, start, code->popup.text);
            size_t src_length = strlen(code->text.src);
            while(pos != NULL)
            {
                if(strlen(code->text.src) + strlen(code->popup.offset) - strlen(code->popup.text) > MAX_CODE)
                    break;

                deleteCode(&code->text, pos, pos + strlen(code->popup.text));
                insertCode(&code->text, pos, code->popup.offset);
                pos += strlen(code->popup.offset);
                if(pos - code->text.src > (ptrdiff_t)src_length) pos = code->text.src + src_length;

                pos = (char*)code_search_down(code->text.src, pos, code->popup.text);
                if(code->text.cursor.selection != NULL && pos > code->text.cursor.position)
                    break;
            }
            history(&code->text);
            updateEditor(code);
            parseSyntaxColor(&code->text);
            setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    else if(keyPressed(code, tic_key_backspace))
    {
        if(*code->popup.text && code->popup.offset == NULL)
        {
            code->popup.text[strlen(code->popup.text) - 1] = '\0';
        }
        else if(*code->popup.text)
        {
            if(strlen(code->popup.offset) - strlen(" WITH:") > 0)
                code->popup.text[strlen(code->popup.text) - 1] = '\0';
        }
    }

    char sym = keyboardText(code);
    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
        }
    }

    tic_api_cls(code->tic, code->view->theme.BG);

    drawCode(code, false);
    drawPopupBar(code, "REPL:");
    drawStatus(code);
}

static void updateGotoCode(Code* code)
{
    s32 line = 0;
    const char* pos = code_goto_line(&code->text, code->popup.text, &line);

    code->text.cursor.selection = NULL;
    code->text.cursor.position = (char*)pos;
    code->jump.line = line;

    updateColumn(&code->text);
    updateEditor(code);

    centerScroll(code);
    updateEditor(code);
}

static void textGoToTick(Code* code)
{
    tic_mem* tic = code->tic;

    if(enterPressed(code))
    {
        if(*code->popup.text)
            updateGotoCode(code);

        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyPressed(code, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text) - 1] = '\0';
            updateGotoCode(code);
        }
    }

    char sym = keyboardText(code);

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text && sym >= '0' && sym <= '9')
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            updateGotoCode(code);
        }
    }

    tic_api_cls(tic, code->view->theme.BG);

    if(code->jump.line >= 0)
        tic_api_rect(tic, 0, (code->jump.line - code->scroll.y) * (TIC_FONT_HEIGHT + 1) + code->view->editorTop,
                     TIC80_WIDTH, TIC_FONT_HEIGHT + 2, code->view->theme.select);

    drawCode(code, false);
    drawPopupBar(code, "GOTO:");
    drawStatus(code);
}

static void drawSidebarBar(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;
    tic_rect rect = {x, y, TIC80_WIDTH - x, TIC80_HEIGHT - y};

    if(mousePos(code, &rect))
    {
        s32 mx = tic_api_mouse(tic).y - rect.y;
        mx /= STUDIO_TEXT_HEIGHT;
        mx += code->sidebar.scroll;

        if(mx >= 0 && mx < code->sidebar.size && code->sidebar.items[mx].pos)
        {
            ui_cursor(code->tic, tic_cursor_hand);

            if(mouseDown(code, &rect, tic_mouse_left))
            {
                code->sidebar.index = mx;
                updateSidebarCode(code);
            }

            if(mouseClicked(code, &rect, tic_mouse_left))
                setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    tic_api_rect(code->tic, rect.x - 1, rect.y, rect.w + 1, rect.h, tic_color_grey);

    y -= code->sidebar.scroll * STUDIO_TEXT_HEIGHT - 1;

    char filter[STUDIO_TEXT_BUFFER_WIDTH];
    strncpy(filter, code->popup.text, sizeof(filter));

    if(code->sidebar.size)
    {
        tic_api_rect(code->tic, rect.x - 1, rect.y + (code->sidebar.index - code->sidebar.scroll) * STUDIO_TEXT_HEIGHT,
                     rect.w + 1, TIC_FONT_HEIGHT + 2, tic_color_red);

        for(const tic_outline_item *ptr = code->sidebar.items, *end = ptr + code->sidebar.size;
            ptr != end; ptr++, y += STUDIO_TEXT_HEIGHT)
        {
            // find the first non-space character
            const char* trimmed = ptr->pos;
            while(*trimmed && *trimmed != '\n' && *trimmed != '\r' && isspace((u8)(*trimmed)))
                trimmed++;

            // calculate the new size after trimming
            size_t trimmed_size = ptr->size - (trimmed - ptr->pos);

            drawFilterMatch(code, x, y, trimmed, (s32)trimmed_size, filter);
        }
    }
    else
    {
        if(code->shadowText)
            tic_api_print(code->tic, "(empty)", x + 1, y + 1, tic_color_black, true, 1, code->altFont);

        tic_api_print(code->tic, "(empty)", x, y, tic_color_white, true, 1, code->altFont);
    }
}

static void normSidebarScroll(Code* code)
{
    code->sidebar.scroll = code->sidebar.size > OUTLINE_ROWS
                               ? CLAMP(code->sidebar.scroll, 0, code->sidebar.size - OUTLINE_ROWS)
                               : 0;
}

static void updateSidebarIndex(Code* code, s32 value)
{
    if(code->sidebar.size == 0)
        return;

    code->sidebar.index = CLAMP(value, 0, code->sidebar.size - 1);

    if(code->sidebar.index - code->sidebar.scroll < 0)
        code->sidebar.scroll -= OUTLINE_ROWS;
    else if(code->sidebar.index - code->sidebar.scroll >= OUTLINE_ROWS)
        code->sidebar.scroll += OUTLINE_ROWS;

    updateSidebarCode(code);
    normSidebarScroll(code);
}

static void processSidebar(Code* code)
{
    // process scroll
    {
        tic80_input* input = &code->tic->ram->input;

        if(input->mouse.scrolly)
        {
            enum
            {
                Scroll = 3
            };
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            code->sidebar.scroll += delta;
            normSidebarScroll(code);
        }
    }

    if(keyPressed(code, tic_key_up))
        updateSidebarIndex(code, code->sidebar.index - 1);

    else if(keyPressed(code, tic_key_down))
        updateSidebarIndex(code, code->sidebar.index + 1);

    else if(keyPressed(code, tic_key_left) || keyPressed(code, tic_key_pageup))
        updateSidebarIndex(code, code->sidebar.index - OUTLINE_ROWS);

    else if(keyPressed(code, tic_key_right) || keyPressed(code, tic_key_pagedown))
        updateSidebarIndex(code, code->sidebar.index + OUTLINE_ROWS);

    else if(keyPressed(code, tic_key_home))
        updateSidebarIndex(code, 0);

    else if(keyPressed(code, tic_key_end))
        updateSidebarIndex(code, code->sidebar.size - 1);

    else if(enterPressed(code))
    {
        updateSidebarCode(code);
        setCodeMode(code, TEXT_EDIT_MODE);
    }
}

static void textBookmarkTick(Code* code)
{
    processSidebar(code);

    tic_api_cls(code->tic, code->view->theme.BG);

    drawCode(code, false);
    drawStatus(code);
    drawSidebarBar(code, (TIC80_WIDTH - SIDEBAR_WIDTH) + code->anim.sidebar, TIC_FONT_HEIGHT + 1);
}

static void textOutlineTick(Code* code)
{
    processSidebar(code);

    if(keyPressed(code, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text) - 1] = '\0';
            setOutlineMode(code);
        }
    }

    char sym = keyboardText(code);

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            setOutlineMode(code);
        }
    }

    tic_api_cls(code->tic, code->view->theme.BG);

    drawCode(code, false);
    drawStatus(code);
    drawSidebarBar(code, (TIC80_WIDTH - SIDEBAR_WIDTH) + code->anim.sidebar, 2 * (TIC_FONT_HEIGHT + 1));
    drawPopupBar(code, "FUNC:");
}

static void applyView(Code* code)
{
    const CodeOptions* options = &code->view->options;

    code->altFont = options->altFont;
    code->shadowText = options->shadowText;
    code->lineNumbers = options->lineNumbers;
    code->showGutter = options->sidebar;
    code->showStatusbar = options->statusbar;
}

void code_set_view(Code* code, const CodeView* view)
{
    code->view = view;
}

void code_just_switched(Code* code)
{
    code->justSwitched = true;
}

void code_tick(Code* code)
{
    // The host's view carries the config values the menubar may change without
    // re-initialising the editor; apply them before anything reads them.
    applyView(code);
    updateTextOptions(code);

    processAnim(code->anim.movie, code);

    if(code->cursor.delay)
        code->cursor.delay--;

    switch(code->mode)
    {
        case TEXT_EDIT_MODE: textEditTick(code); break;
        case TEXT_FIND_MODE: textFindTick(code); break;
        case TEXT_REPLACE_MODE: textReplaceTick(code); break;
        case TEXT_GOTO_MODE: textGoToTick(code); break;
        case TEXT_BOOKMARK_MODE: textBookmarkTick(code); break;
        case TEXT_OUTLINE_MODE: textOutlineTick(code); break;
    }

    code->tickCounter++;
}

static void escape(Code* code)
{
    switch(code->mode)
    {
        case TEXT_EDIT_MODE:
            break;
        default:
            if(code->popup.prevPos == NULL)
                return;

            code->anim.movie = resetMovie(&code->anim.hide);

            code->text.cursor.position = code->popup.prevPos;
            code->text.cursor.selection = code->popup.prevSel;
            code->popup.prevSel = code->popup.prevPos = NULL;

            updateEditor(code);
    }
}

static void emptyDone(void* data)
{
    (void)data;
}

static void setIdle(void* data)
{
    Code* code = data;
    code->anim.movie = resetMovie(&code->anim.idle);
}

static void setEditMode(void* data)
{
    Code* code = data;
    code->anim.movie = resetMovie(&code->anim.idle);
    setCodeMode(code, TEXT_EDIT_MODE);
}

static void freeAnim(Code* code)
{
    FREE(code->anim.show.items);
    FREE(code->anim.hide.items);
}

// The clipboard buttons take no arguments, so each editor's own operation gets
// a uniform entry point here. The argument the old dispatch passed is folded in.
static void clipboardCut(void* app)
{
    Code* code = app;
    cutToClipboard(code, false);
}
static void clipboardCopy(void* app)
{
    Code* code = app;
    copyToClipboard(code, false);
}
static void clipboardPaste(void* app)
{
    Code* code = app;
    copyFromClipboard(code, false);
}
static void clipboardUndo(void* app)
{
    Code* code = app;
    undo(code);
}
static void clipboardRedo(void* app)
{
    Code* code = app;
    redo(code);
}

const ClipboardOps CodeClipboard = {clipboardCut, clipboardCopy, clipboardPaste, clipboardUndo, clipboardRedo};

void initCode(Code* code, const CodeDeps* deps)
{
    text_free(&code->text);
    freeAnim(code);

    tic_code* src = &deps->tic->cart.code;

    *code = (Code){
        .deps = *deps,
        .tic = deps->tic,
        .escape = escape,
        .scroll = {0, 0, {0, 0}, false},
        .tickCounter = 0,
        .mode = TEXT_EDIT_MODE,
        .jump = {.line = -1},
        .popup =
            {
                .prevPos = NULL,
                .prevSel = NULL,
            },
        .sidebar =
            {
                .items = NULL,
                .size = 0,
                .index = 0,
                .scroll = 0,
            },
        .matchedDelim = NULL,
        .anim =
            {
                .idle = {
                    .done = emptyDone,
                },

                .show = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle, {
                                                                 {-STUDIO_BAR_HEIGHT, 0, STUDIO_ANIM_TIME, &code->anim.pos, AnimEaseIn},
                                                                 {SIDEBAR_WIDTH, 0, STUDIO_ANIM_TIME, &code->anim.sidebar, AnimEaseIn},
                                                             }),

                .hide = MOVIE_DEF(STUDIO_ANIM_TIME, setEditMode, {
                                                                     {0, -STUDIO_BAR_HEIGHT, STUDIO_ANIM_TIME, &code->anim.pos, AnimEaseIn},
                                                                     {0, SIDEBAR_WIDTH, STUDIO_ANIM_TIME, &code->anim.sidebar, AnimEaseIn},
                                                                 }),
            },
        .update = update,
    };

    code->anim.movie = resetMovie(&code->anim.idle);

    text_init(&code->text, src->data, (TextOptions){0}, textHistoryEnabled, code);

    // The host sets the real view at the first tick; this one keeps the initial
    // update from reading a null view.
    static const CodeView empty;
    code_set_view(code, &empty);

    update(code);
}

void freeCode(Code* code)
{
    freeAnim(code);
    free(code->sidebar.items);

    text_free(&code->text);

    free(code);
}

void trimWhitespace(Code* code)
{
    text_trim(&code->text);
    update(code);
}