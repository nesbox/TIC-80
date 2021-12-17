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
#include "ext/history.h"

#include <ctype.h>
#include <assert.h>

#define TEXT_CURSOR_DELAY (TIC80_FRAMERATE / 2)
#define TEXT_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define BOOKMARK_WIDTH 7
#define CODE_EDITOR_WIDTH (TIC80_WIDTH - BOOKMARK_WIDTH)
#define CODE_EDITOR_HEIGHT (TIC80_HEIGHT - TOOLBAR_SIZE - STUDIO_TEXT_HEIGHT)
#define TEXT_BUFFER_HEIGHT (CODE_EDITOR_HEIGHT / STUDIO_TEXT_HEIGHT)

#if defined(TIC80_PRO)
#   define MAX_CODE sizeof(tic_code)
#else
#   define MAX_CODE TIC_BANK_SIZE
#endif

typedef struct CodeState CodeState;

static_assert(sizeof(CodeState) == 2, "CodeStateSize");

enum
{
#define CODE_COLOR_DEF(VAR) SyntaxType_##VAR,
        CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef  CODE_COLOR_DEF
};

static void packState(Code* code)
{
    const char* src = code->src;
    for(CodeState* s = code->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
    {
        s->cursor = src == code->cursor.position;
        s->sym = *src++;
    }
}

static void unpackState(Code* code)
{
    char* src = code->src;
    for(CodeState* s = code->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
    {
        if(s->cursor)
            code->cursor.position = src;

        *src++ = s->sym;
    }
}

static void history(Code* code)
{
    packState(code);
    history_add(code->history);
}

static void drawStatus(Code* code)
{
    enum {Height = TIC_FONT_HEIGHT + 1, StatusY = TIC80_HEIGHT - TIC_FONT_HEIGHT};

    tic_api_rect(code->tic, 0, TIC80_HEIGHT - Height, TIC80_WIDTH, Height, code->status.color);
    tic_api_print(code->tic, code->status.line, 0, StatusY, getConfig()->theme.code.BG, true, 1, false);
    tic_api_print(code->tic, code->status.size, TIC80_WIDTH - (s32)strlen(code->status.size) * TIC_FONT_WIDTH, 
        StatusY, getConfig()->theme.code.BG, true, 1, false);
}

static char* getPosByLine(char* ptr, s32 line)
{
    s32 y = 0;
    while(*ptr)
    {
        if(y == line) break;
        if(*ptr++ == '\n') y++;
    }

    return ptr;    
}

static char* getNextLineByPos(Code* code, char* pos)
{
    while(*pos && *pos++ != '\n');
    return pos;
}

static inline CodeState* getState(Code* code, const char* pos)
{
    return code->state + (pos - code->src);
}

static void toggleBookmark(Code* code, char* codePos)
{
    CodeState* start = getState(code, codePos);
    const CodeState* end = getState(code, getNextLineByPos(code, codePos));

    bool bookmarked = false;
    CodeState* ptr = start;
    while(ptr < end)
        if(ptr++->bookmark)
            bookmarked = true;

    if(bookmarked)
    {
        CodeState* ptr = start;
        while(ptr < end)
            ptr++->bookmark = 0;
    }
    else start->bookmark = 1;

    history(code);
}

static void drawBookmarks(Code* code)
{
    tic_mem* tic = code->tic;

    enum {Width = BOOKMARK_WIDTH, Height = TIC80_HEIGHT - TOOLBAR_SIZE*2};
    tic_rect rect = {0, TOOLBAR_SIZE, Width, Height};

    tic_api_rect(code->tic, rect.x, rect.y, rect.w, rect.h, tic_color_grey);

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("BOOKMARK [ctrl+f1]");

        s32 line = (tic_api_mouse(tic).y - rect.y) / STUDIO_TEXT_HEIGHT;

        drawBitIcon(tic_icon_bookmark, rect.x, rect.y + line * STUDIO_TEXT_HEIGHT - 1, tic_color_dark_grey);

        if(checkMouseClick(&rect, tic_mouse_left))
            toggleBookmark(code, getPosByLine(code->src, line + code->scroll.y));
    }

    const char* pointer = code->src;
    const CodeState* syntaxPointer = code->state;
    s32 y = -code->scroll.y;

    while(*pointer)
    {
        if(syntaxPointer++->bookmark)
        {
            drawBitIcon(tic_icon_bookmark, rect.x, rect.y + y * STUDIO_TEXT_HEIGHT, tic_color_black);
            drawBitIcon(tic_icon_bookmark, rect.x, rect.y + y * STUDIO_TEXT_HEIGHT - 1, tic_color_yellow);
        }

        if(*pointer++ == '\n')y++;
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

static void drawCursor(Code* code, s32 x, s32 y, char symbol)
{
    bool inverse = code->cursor.delay || code->tickCounter % TEXT_CURSOR_BLINK_PERIOD < TEXT_CURSOR_BLINK_PERIOD / 2;

    if(inverse)
    {
        if(code->shadowText)
            tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, 0);

        tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, getConfig()->theme.code.cursor);

        if(symbol)
            drawChar(code->tic, symbol, x, y, getConfig()->theme.code.BG, code->altFont);
    }
}

static void drawMatchedDelim(Code* code, s32 x, s32 y, char symbol, u8 color)
{
    tic_api_rectb(code->tic, x-1, y-1, (getFontWidth(code))+1, TIC_FONT_HEIGHT+1,
                  getConfig()->theme.code.cursor);
    drawChar(code->tic, symbol, x, y, color, code->altFont);
}

static void drawCode(Code* code, bool withCursor)
{
    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    s32 xStart = rect.x - code->scroll.x * getFontWidth(code);
    s32 x = xStart;
    s32 y = rect.y - code->scroll.y * STUDIO_TEXT_HEIGHT;
    const char* pointer = code->src;

    u8 selectColor = getConfig()->theme.code.select;

    const u8* colors = (const u8*)&getConfig()->theme.code;
    const CodeState* syntaxPointer = code->state;

    struct { char* start; char* end; } selection = 
    {
        MIN(code->cursor.selection, code->cursor.position),
        MAX(code->cursor.selection, code->cursor.position)
    };

    struct { s32 x; s32 y; char symbol; } cursor = {-1, -1, 0};
    struct { s32 x; s32 y; char symbol; u8 color; } matchedDelim = {-1, -1, 0, 0};

    while(*pointer)
    {
        char symbol = *pointer;

        if(x >= -getFontWidth(code) && x < TIC80_WIDTH && y >= -TIC_FONT_HEIGHT && y < TIC80_HEIGHT )
        {
            if(code->cursor.selection && pointer >= selection.start && pointer < selection.end)
            {
                if(code->shadowText)
                    tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, tic_color_black);

                tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, selectColor);
                drawChar(code->tic, symbol, x, y, tic_color_dark_grey, code->altFont);
            }
            else 
            {
                if(code->shadowText)
                    drawChar(code->tic, symbol, x+1, y+1, 0, code->altFont);

                drawChar(code->tic, symbol, x, y, colors[syntaxPointer->syntax], code->altFont);
            }
        }

        if(code->cursor.position == pointer)
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
        }
        else x += getFontWidth(code);

        pointer++;
        syntaxPointer++;
    }

    drawBookmarks(code);

    if(code->cursor.position == pointer)
        cursor.x = x, cursor.y = y;

    if(withCursor && cursor.x >= BOOKMARK_WIDTH && cursor.y >= 0)
        drawCursor(code, cursor.x, cursor.y, cursor.symbol);

    if(matchedDelim.symbol) {
        drawMatchedDelim(code, matchedDelim.x, matchedDelim.y,
                         matchedDelim.symbol, matchedDelim.color);
    }
}

static void getCursorPosition(Code* code, s32* x, s32* y)
{
    *x = 0;
    *y = 0;

    const char* pointer = code->src;

    while(*pointer)
    {
        if(code->cursor.position == pointer) return;

        if(*pointer == '\n')
        {
            *x = 0;
            (*y)++;
        }
        else (*x)++;

        pointer++;
    }
}

static s32 getLinesCount(Code* code)
{
    char* text = code->src;
    s32 count = 0;

    while(*text)
        if(*text++ == '\n')
            count++;

    return count;
}

static void removeInvalidChars(char* code)
{
    // remove \r symbol
    char* s; char* d;
    for(s = d = code; (*d = *s); d += (*s++ != '\r'));
}

const char* findMatchedDelim(Code* code, const char* current)
{
    const char* start = code->src;
    // delimiters inside comments and strings don't get to be matched!
    if(code->state[current - start].syntax == SyntaxType_COMMENT ||
       code->state[current - start].syntax == SyntaxType_STRING) return 0;

    char initial = *current;
    char seeking = 0;
    s8 dir = (initial == '(' || initial == '[' || initial == '{') ? 1 : -1;
    switch (initial)
    {
    case '(': seeking = ')'; break;
    case ')': seeking = '('; break;
    case '[': seeking = ']'; break;
    case ']': seeking = '['; break;
    case '{': seeking = '}'; break;
    case '}': seeking = '{'; break;
    default: return NULL;
    }

    while(*current && (start < current))
    {
        current += dir;
        // skip over anything inside a comment or string
        if(code->state[current - start].syntax == SyntaxType_COMMENT ||
           code->state[current - start].syntax == SyntaxType_STRING) continue;
        if(*current == seeking) return current;
        if(*current == initial) current = findMatchedDelim(code, current);
        if(!current) break;
    }

    return NULL;
}

static void updateEditor(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    if(getConfig()->theme.code.matchDelimiters)
        code->matchedDelim = findMatchedDelim(code, code->cursor.position);

    const s32 BufferWidth = CODE_EDITOR_WIDTH / getFontWidth(code);

    if(column < code->scroll.x) code->scroll.x = column;
    else if(column >= code->scroll.x + BufferWidth)
        code->scroll.x = column - BufferWidth + 1;

    if(line < code->scroll.y) code->scroll.y = line;
    else if(line >= code->scroll.y + TEXT_BUFFER_HEIGHT)
        code->scroll.y = line - TEXT_BUFFER_HEIGHT + 1;

    code->cursor.delay = TEXT_CURSOR_DELAY;

    sprintf(code->status.line, "line %i/%i col %i", line + 1, getLinesCount(code) + 1, column + 1);
    {
        s32 codeLen = strlen(code->src);
        sprintf(code->status.size, "size %i/%i", codeLen, MAX_CODE);
        code->status.color = codeLen > MAX_CODE ? tic_color_red : tic_color_white;
    }
}

static inline bool islineend(char c) {return c == '\n' || c == '\0';}
static inline bool isalpha_(char c) {return isalpha(c) || c == '_';}
static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static void setCodeState(CodeState* state, u8 color, s32 start, s32 size)
{
    for(CodeState* s = state + start, *end = s + size; s != end; ++s)
        s->syntax = color;
}

static void parseCode(const tic_script_config* config, const char* start, CodeState* state)
{
    const char* ptr = start;

    const char* blockCommentStart = NULL;
    const char* blockCommentStart2 = NULL;
    const char* blockStringStart = NULL;
    const char* blockStdStringStart = NULL;
    const char* singleCommentStart = NULL;
    const char* wordStart = NULL;
    const char* numberStart = NULL;

start:
    while(true)
    {
        char c = ptr[0];

        if(blockCommentStart)
        {
            const char* end = strstr(ptr, config->blockCommentEnd);

            ptr = end ? end + strlen(config->blockCommentEnd) : blockCommentStart + strlen(blockCommentStart);
            setCodeState(state, SyntaxType_COMMENT, (s32)(blockCommentStart - start), (s32)(ptr - blockCommentStart));
            blockCommentStart = NULL;

            // !TODO: stupid MS compiler doesn't see 'continue' here in release, so lets use 'goto' instead, investigate why
            goto start;
        }
        else if(blockCommentStart2)
        {
            const char* end = strstr(ptr, config->blockCommentEnd2);

            ptr = end ? end + strlen(config->blockCommentEnd2) : blockCommentStart2 + strlen(blockCommentStart2);
            setCodeState(state, SyntaxType_COMMENT, (s32)(blockCommentStart2 - start), (s32)(ptr - blockCommentStart2));
            blockCommentStart2 = NULL;
            goto start;
        }
        else if(blockStringStart)
        {
            const char* end = strstr(ptr, config->blockStringEnd);

            ptr = end ? end + strlen(config->blockStringEnd) : blockStringStart + strlen(blockStringStart);
            setCodeState(state, SyntaxType_STRING, (s32)(blockStringStart - start), (s32)(ptr - blockStringStart));
            blockStringStart = NULL;
            continue;
        }
        else if(blockStdStringStart)
        {
            const char* blockStart = blockStdStringStart+1;

            while(true)
            {
                const char* pos = strchr(blockStart, *blockStdStringStart);
                
                if(pos)
                {
                    if(*(pos-1) == '\\' && *(pos-2) != '\\') blockStart = pos + 1;
                    else
                    {
                        ptr = pos + 1;
                        break;
                    }
                }
                else
                {
                    ptr = blockStdStringStart + strlen(blockStdStringStart);
                    break;
                }
            }

            setCodeState(state, SyntaxType_STRING, (s32)(blockStdStringStart - start), (s32)(ptr - blockStdStringStart));
            blockStdStringStart = NULL;
            continue;
        }
        else if(singleCommentStart)
        {
            while(!islineend(*ptr))ptr++;

            setCodeState(state, SyntaxType_COMMENT, (s32)(singleCommentStart - start), (s32)(ptr - singleCommentStart));
            singleCommentStart = NULL;
            continue;
        }
        else if(wordStart)
        {
            while(!islineend(*ptr) && isalnum_(*ptr)) ptr++;

            s32 len = (s32)(ptr - wordStart);
            bool keyword = false;
            {
                for(s32 i = 0; i < config->keywordsCount; i++)
                    if(len == strlen(config->keywords[i]) && memcmp(wordStart, config->keywords[i], len) == 0)
                    {
                        setCodeState(state, SyntaxType_KEYWORD, (s32)(wordStart - start),len);
                        keyword = true;
                        break;
                    }
            }

            if(!keyword)
            {
                static const char* const ApiKeywords[] = 
                {
#define             TIC_CALLBACK_DEF(name, ...) #name,
                    TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef              TIC_CALLBACK_DEF

#define             API_KEYWORD_DEF(name, ...) #name,
                    TIC_API_LIST(API_KEYWORD_DEF)
#undef              API_KEYWORD_DEF
                };

                for(s32 i = 0; i < COUNT_OF(ApiKeywords); i++)
                    if(len == strlen(ApiKeywords[i]) && memcmp(wordStart, ApiKeywords[i], len) == 0)
                    {
                        setCodeState(state, SyntaxType_API, (s32)(wordStart - start), len);
                        break;
                    }
            }

            wordStart = NULL;
            continue;
        }
        else if(numberStart)
        {
            while(!islineend(*ptr))
            {
                char c = *ptr;

                if(isdigit(c)) ptr++;
                else if(numberStart[0] == '0' 
                    && (numberStart[1] == 'x' || numberStart[1] == 'X') 
                    && isxdigit(numberStart[2]))
                {
                    if((ptr - numberStart < 2) || isxdigit(c)) ptr++;
                    else break;
                }
                else if(c == '.' || c == 'e' || c == 'E')
                {
                    if(isdigit(ptr[1])) ptr++;
                    else break;
                }
                else break;
            }

            setCodeState(state, SyntaxType_NUMBER, (s32)(numberStart - start), (s32)(ptr - numberStart));
            numberStart = NULL;
            continue;
        }
        else
        {
            if(config->blockCommentStart && memcmp(ptr, config->blockCommentStart, strlen(config->blockCommentStart)) == 0)
            {
                blockCommentStart = ptr;
                ptr += strlen(config->blockCommentStart);
                continue;
            }
            if(config->blockCommentStart2 && memcmp(ptr, config->blockCommentStart2, strlen(config->blockCommentStart2)) == 0)
            {
                blockCommentStart2 = ptr;
                ptr += strlen(config->blockCommentStart2);
                continue;
            }
            else if(config->blockStringStart && memcmp(ptr, config->blockStringStart, strlen(config->blockStringStart)) == 0)
            {
                blockStringStart = ptr;
                ptr += strlen(config->blockStringStart);
                continue;
            }
            else if(c == '"' || c == '\'')
            {
                blockStdStringStart = ptr;
                ptr++;
                continue;
            }
            else if(config->singleComment && memcmp(ptr, config->singleComment, strlen(config->singleComment)) == 0)
            {
                singleCommentStart = ptr;
                ptr += strlen(config->singleComment);
                continue;
            }
            else if(isalpha_(c))
            {
                wordStart = ptr;
                ptr++;
                continue;
            }
            else if(isdigit(c) || (c == '.' && isdigit(ptr[1])))
            {
                numberStart = ptr;
                ptr++;
                continue;
            }
            else if(ispunct(c)) state[ptr - start].syntax = SyntaxType_SIGN;
        }

        if(!c) break;

        ptr++;
    }
}

static void parseSyntaxColor(Code* code)
{
    for(CodeState* s = code->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
        s->syntax = SyntaxType_FG;

    tic_mem* tic = code->tic;

    const tic_script_config* config = tic_core_script_config(tic);

    parseCode(config, code->src, code->state);
}

static char* getLineByPos(Code* code, char* pos)
{
    char* text = code->src;
    char* line = text;

    while(text < pos)
        if(*text++ == '\n')
            line = text;

    return line;
}

static char* getLine(Code* code)
{
    return getLineByPos(code, code->cursor.position);
}

static char* getPrevLineByPos(Code* code, char* pos)
{
    char* text = code->src;
    char* prevLine = text;
    char* line = text;

    while(text < pos)
        if(*text++ == '\n')
        {
            prevLine = line;
            line = text;
        }

    return prevLine;
}

static char* getPrevLine(Code* code)
{
    return getPrevLineByPos(code, code->cursor.position);
}

static char* getNextLine(Code* code)
{
    return getNextLineByPos(code, code->cursor.position);
}

static s32 getLineSize(const char* line)
{
    s32 size = 0;
    while(*line != '\n' && *line++) size++;

    return size;
}

static void updateColumn(Code* code)
{
    code->cursor.column = (s32)(code->cursor.position - getLine(code));
}

static void updateCursorPosition(Code* code, char* position)
{
    code->cursor.position = position;
    updateColumn(code);
    updateEditor(code);
}

static void setCursorPosition(Code* code, s32 cx, s32 cy)
{
    s32 x = 0;
    s32 y = 0;
    char* pointer = code->src;

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
        else x++;

        pointer++;
    }

    updateCursorPosition(code, pointer);
}

static void endLine(Code* code)
{
    while(*code->cursor.position)
    {
        code->cursor.position++;
    }
    updateColumn(code);
}

static void upLine(Code* code)
{
    char* prevLine = getPrevLine(code);
    size_t prevSize = getLineSize(prevLine);
    size_t size = code->cursor.column;

    code->cursor.position = prevLine + (prevSize > size ? size : prevSize);
}

static void downLine(Code* code)
{
    char* nextLine = getNextLine(code);
    size_t nextSize = getLineSize(nextLine);
    size_t size = code->cursor.column;

    code->cursor.position = nextLine + (nextSize > size ? size : nextSize);
}

static void leftColumn(Code* code)
{
    char* start = code->src;

    if(code->cursor.position > start)
    {
        code->cursor.position--;
        updateColumn(code);
    }
}

static void rightColumn(Code* code)
{
    if(*code->cursor.position)
    {
        code->cursor.position++;
        updateColumn(code);
    }
}

static void leftWord(Code* code)
{
    const char* start = code->src;
    char* pos = code->cursor.position-1;

    if(pos > start)
    {
        if(isalnum_(*pos)) while(pos > start && isalnum_(*(pos-1))) pos--;
        else while(pos > start && !isalnum_(*(pos-1))) pos--;

        code->cursor.position = pos;

        updateColumn(code);
    }
}

static void rightWord(Code* code)
{
    const char* end = code->src + strlen(code->src);
    char* pos = code->cursor.position;

    if(pos < end)
    {
        if(isalnum_(*pos)) while(pos < end && isalnum_(*pos)) pos++;
        else while(pos < end && !isalnum_(*pos)) pos++;

        code->cursor.position = pos;
        updateColumn(code);
    }
}

static void goHome(Code* code)
{
    code->cursor.position = getLine(code);

    updateColumn(code);
}

static void goEnd(Code* code)
{
    char* line = getLine(code);
    code->cursor.position = line + getLineSize(line);

    updateColumn(code);
}

static void goCodeHome(Code *code)
{
    code->cursor.position = code->src;

    updateColumn(code);
}

static void goCodeEnd(Code *code)
{
    code->cursor.position = code->src + strlen(code->src);

    updateColumn(code);
}

static void pageUp(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    setCursorPosition(code, column, line > TEXT_BUFFER_HEIGHT ? line - TEXT_BUFFER_HEIGHT : 0);
}

static void pageDown(Code* code)
{
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    s32 lines = getLinesCount(code);
    setCursorPosition(code, column, line < lines - TEXT_BUFFER_HEIGHT ? line + TEXT_BUFFER_HEIGHT : lines);
}

static void deleteCode(Code* code, char* start, char* end)
{
    s32 size = (s32)strlen(end) + 1;
    memmove(start, end, size);

    // delete code state
    memmove(getState(code, start), getState(code, end), size * sizeof(CodeState));
}

static void insertCode(Code* code, char* dst, const char* src)
{
    s32 size = (s32)strlen(src);
    s32 restSize = (s32)strlen(dst) + 1;
    memmove(dst + size, dst, restSize);
    memcpy(dst, src, size);

    // insert code state
    {
        CodeState* pos = getState(code, dst);
        memmove(pos + size, pos, restSize * sizeof(CodeState));
        memset(pos, 0, size * sizeof(CodeState));
    }
}

static bool replaceSelection(Code* code)
{
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;

    if(sel && sel != pos)
    {
        char* start = MIN(sel, pos);
        char* end = MAX(sel, pos);

        deleteCode(code, start, end);

        code->cursor.position = start;
        code->cursor.selection = NULL;

        history(code);

        parseSyntaxColor(code);

        return true;
    }

    return false;
}

static void deleteChar(Code* code)
{
    if(!replaceSelection(code))
    {
        deleteCode(code, code->cursor.position, code->cursor.position + 1);
        history(code);
        parseSyntaxColor(code);
    }

    updateEditor(code);
}

static void backspaceChar(Code* code)
{
    if(!replaceSelection(code) && code->cursor.position > code->src)
    {
        char* pos = --code->cursor.position;
        deleteCode(code, pos, pos + 1);
        history(code);
        parseSyntaxColor(code);
    }

    updateEditor(code);
}

static void deleteWord(Code* code)
{
    const char* end = code->src + strlen(code->src);
    char* pos = code->cursor.position;

    if(pos < end)
    {
        if(isalnum_(*pos)) while(pos < end && isalnum_(*pos)) pos++;
        else while(pos < end && !isalnum_(*pos)) pos++;

        deleteCode(code, code->cursor.position, pos);

        history(code);
        parseSyntaxColor(code);
    }
}

static void backspaceWord(Code* code)
{
    const char* start = code->src;
    char* pos = code->cursor.position-1;

    if(pos > start)
    {
        if(isalnum_(*pos)) while(pos > start && isalnum_(*(pos-1))) pos--;
        else while(pos > start && !isalnum_(*(pos-1))) pos--;

        deleteCode(code, pos, code->cursor.position);

        code->cursor.position = pos;
        history(code);
        parseSyntaxColor(code);
    }
}

static void inputSymbolBase(Code* code, char sym)
{
    if (strlen(code->src) >= MAX_CODE)
        return;

    insertCode(code, code->cursor.position++, (const char[]){sym, '\0'});

    history(code);

    updateColumn(code);

    parseSyntaxColor(code);
}

static void inputSymbol(Code* code, char sym)
{
    replaceSelection(code);

    inputSymbolBase(code, sym);
}

static void newLine(Code* code)
{
    if(!replaceSelection(code))
    {
        char* ptr = getLine(code);
        size_t size = 0;
        char firstChar = *ptr;

        while(*ptr == '\t' || *ptr == ' ') ptr++, size++;

        if(ptr > code->cursor.position)
            size -= ptr - code->cursor.position;

        inputSymbol(code, '\n');

        for(size_t i = 0; i < size; i++)
            inputSymbol(code, firstChar);

        updateEditor(code);
    }
}

static void selectAll(Code* code)
{
    code->cursor.selection = code->src;
        code->cursor.position = code->cursor.selection + strlen(code->cursor.selection);
}

static void copyToClipboard(Code* code)
{
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;

    char* start = NULL;
    size_t size = 0;

    if(sel && sel != pos)
    {
        start = MIN(sel, pos);
        size = MAX(sel, pos) - start;
    }
    else
    {
        start = getLine(code);
        size = getNextLine(code) - start;
    }

    char* clipboard = (char*)malloc(size+1);

    if(clipboard)
    {
        memcpy(clipboard, start, size);
        clipboard[size] = '\0';
        tic_sys_clipboard_set(clipboard);
        free(clipboard);
    }
}

static void cutToClipboard(Code* code)
{
    if(code->cursor.selection == NULL || code->cursor.position == code->cursor.selection)
    {
        code->cursor.position = getLine(code);
        code->cursor.selection = getNextLine(code);
    }
    
    copyToClipboard(code);
    replaceSelection(code);
    history(code);
}

static void copyFromClipboard(Code* code)
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
                replaceSelection(code);

                // cut clipboard code if overall code > max code size
                {
                    size_t codeSize = strlen(code->src);

                    if (codeSize + size > MAX_CODE)
                    {
                        size = MAX_CODE - codeSize;
                        clipboard[size] = '\0';
                    }
                }

                insertCode(code, code->cursor.position, clipboard);

                code->cursor.position += size;

                history(code);

                parseSyntaxColor(code);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void update(Code* code)
{
    updateEditor(code);
    parseSyntaxColor(code);
}

static void undo(Code* code)
{
    history_undo(code->history);
    unpackState(code);

    update(code);
}

static void redo(Code* code)
{
    history_redo(code->history);
    unpackState(code);

    update(code);
}

static void doTab(Code* code, bool shift, bool crtl)
{
    char* cursor_position = code->cursor.position;
    char* cursor_selection = code->cursor.selection;
    
    bool has_selection = cursor_selection && cursor_selection != cursor_position;
    bool modifier_key_pressed = shift || crtl;
    
    if(has_selection || modifier_key_pressed)
    {
        char* start;
        char* end;
        
        bool changed = false;
        
        if(cursor_selection) {
            start = MIN(cursor_selection, cursor_position);
            end = MAX(cursor_selection, cursor_position);
        } else {
            start = end = cursor_position;
        }

        char* line = start = getLineByPos(code, start);

        while(line)
        {
            if(shift)
            {
                if(*line == '\t' || *line == ' ')
                {
                    deleteCode(code, line, line + 1);
                    end--;
                    changed = true;
                }
            }
            else
            {
                insertCode(code, line, "\t");
                end++;

                changed = true;
            }
            
            line = getNextLineByPos(code, line);
            if(line >= end) break;
        }
        
        if(changed) {
            
            if(has_selection) {
                code->cursor.position = start;
                code->cursor.selection = end;
            }
            else if (start <= end) code->cursor.position = end;
            
            history(code);
            parseSyntaxColor(code);
        }
    }
    else inputSymbolBase(code, '\t');
}

// Add a block-ending keyword or symbol, and put the cursor in the line between.
static void newLineAutoClose(Code* code)
{
    newLine(code);

    const char* blockEnd = tic_core_script_config(code->tic)->blockEnd;
    if (blockEnd != NULL)
    {
        newLine(code);
        for(size_t i = 0; i < strlen(blockEnd); i++)
            inputSymbol(code, blockEnd[i]);
        upLine(code);
        goEnd(code);
    }

    doTab(code, false, true);
}

static void setFindMode(Code* code)
{
    if(code->cursor.selection)
    {
        const char* end = MAX(code->cursor.position, code->cursor.selection);
        const char* start = MIN(code->cursor.position, code->cursor.selection);
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

static s32 funcCompare(const void* a, const void* b)
{
    const tic_outline_item* item1 = (const tic_outline_item*)a;
    const tic_outline_item* item2 = (const tic_outline_item*)b;

    return strcmp(item1->pos, item2->pos);
}

static void normalizeScroll(Code* code)
{
    if(code->scroll.x < 0) code->scroll.x = 0;
    if(code->scroll.y < 0) code->scroll.y = 0;
    else
    {
        s32 lines = getLinesCount(code);
        if(code->scroll.y > lines) code->scroll.y = lines;
    }
}

static void centerScroll(Code* code)
{
    s32 col, line;
    getCursorPosition(code, &col, &line);
    code->scroll.x = col - CODE_EDITOR_WIDTH / getFontWidth(code) / 2;
    code->scroll.y = line - TEXT_BUFFER_HEIGHT / 2;

    normalizeScroll(code);
}

static void updateOutlineCode(Code* code)
{
    tic_mem* tic = code->tic;

    const tic_outline_item* item = code->outline.items + code->outline.index;

    if(item && item->pos)
    {
        code->cursor.position = (char*)item->pos;
        code->cursor.selection = (char*)item->pos + item->size;
    }
    else
    {
        code->cursor.position = code->src;
        code->cursor.selection = NULL;
    }

    centerScroll(code);
    updateEditor(code);
}

static bool isFilterMatch(const char* buffer, const char* filter)
{
    while(*buffer)
    {
        if(tolower(*buffer) == tolower(*filter))
            filter++;
        buffer++;
    }

    return *filter == 0;
}

static void drawFilterMatch(Code *code, s32 x, s32 y, const char* orig, const char* filter)
{
    while(*orig)
    {
        bool match = tolower(*orig) == tolower(*filter);
        u8 color = match ? tic_color_orange : tic_color_white;

        if(code->shadowText)
            drawChar(code->tic, *orig, x+1, y+1, tic_color_black, code->altFont);

        drawChar(code->tic, *orig, x, y, color, code->altFont);
        x += getFontWidth(code);
        if(match)
            filter++;

        orig++;
    }
}

static void initOutlineMode(Code* code)
{
    tic_mem* tic = code->tic;

    if(code->outline.items)
    {
        free(code->outline.items);
        code->outline.items = NULL;
        code->outline.size = 0;
    }

    const tic_script_config* config = tic_core_script_config(tic);

    if(config->getOutline)
    {
        s32 size = 0;
        const tic_outline_item* items = config->getOutline(code->src, &size);

        if(items)
        {
            char filter[STUDIO_TEXT_BUFFER_WIDTH];
            strncpy(filter, code->popup.text, sizeof(filter));

            for(s32 i = 0; i < size; i++)
            {
                const tic_outline_item* item = items + i;

                char buffer[STUDIO_TEXT_BUFFER_WIDTH];
                memcpy(buffer, item->pos, MIN(item->size, sizeof(buffer)));

                if(code->state[item->pos - code->src].syntax == SyntaxType_COMMENT)
                    continue;

                if(*filter && !isFilterMatch(buffer, filter))
                    continue;

                s32 last = code->outline.size++;
                code->outline.items = realloc(code->outline.items, code->outline.size * sizeof(tic_outline_item));
                code->outline.items[last] = *item;
            }
        }
    }
}

static void setOutlineMode(Code* code)
{
    code->outline.index = 0;
    code->outline.scroll = 0;

    initOutlineMode(code);

    qsort(code->outline.items, code->outline.size, sizeof(tic_outline_item), funcCompare);
    updateOutlineCode(code);
}

static void setCodeMode(Code* code, s32 mode)
{
    if(code->mode != mode)
    {
        strcpy(code->popup.text, "");

        code->popup.prevPos = code->cursor.position;
        code->popup.prevSel = code->cursor.selection;

        switch(mode)
        {
        case TEXT_FIND_MODE: setFindMode(code); break;
        case TEXT_GOTO_MODE: setGotoMode(code); break;
        case TEXT_OUTLINE_MODE: setOutlineMode(code); break;
        default: break;
        }

        code->mode = mode;
    }
}

static int getNumberOfLines(Code* code){
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;

    char* start = MIN(pos, sel);
    while(*start == '\n') start++;

    char* end = MAX(pos, sel);
    while(*end == '\n') end--;

    char* iter = start;
    size_t lines = 1;
    while(iter <= end){
        if(*iter == '\n'){
            ++lines;
        }
        ++iter;
    }
    return lines;
}

static char** getLines(Code* code, int lines){
    char* pos = code->cursor.position;
    char* sel = code->cursor.selection;


    char* start = MIN(pos, sel);
    while(*start == '\n') ++start;

    char* end = MAX(pos, sel);
    while(*end == '\n') --end;

    char* iter = start;
    iter = start;

    char** line_locations = malloc(sizeof(char*)*lines+1);
    line_locations[0] = start;

    for(int i = 1; i < lines; ++i){
       while(iter <=end && *iter!='\n') ++iter;
       line_locations[i] = ++iter;
    }
    return line_locations;
}

static inline bool isLineCommented(const char* comment, const char* line){
    size_t size = strlen(comment);
    return memcmp(line, comment, size) == 0;
};

static void addCommentToLine(Code* code, char* line, size_t size, const char* comment){

    const char* end = line + getLineSize(line);

    while((*line == ' ' || *line == '\t') && line < end) line++;

    if(!isLineCommented(comment, line))
    {
        if (strlen(code->src) + size >= MAX_CODE)
            return;

        insertCode(code, line, comment);

        if(code->cursor.position > line)
            code->cursor.position += size;
    }
    else
    {
        deleteCode(code, line, line + size);

        if(code->cursor.position > line + size)
            code->cursor.position -= size;
    }

    code->cursor.selection = NULL;

    history(code);

    parseSyntaxColor(code);
}

static void commentLine(Code* code)
{
    const char* comment = tic_core_script_config(code->tic)->singleComment;
    size_t size = strlen(comment);

    if(code->cursor.selection){
        int selectionLines = getNumberOfLines(code);
        char** lines = getLines(code, selectionLines);

        int comment_cursor = 0;

        for (int i = 0; i<selectionLines; ++i){

            char* first = lines[i];
            while(*first == ' ' || *first == '\t') ++first;
            bool lineIsComment = isLineCommented(comment, first);

            if(i < selectionLines - 1) {
                if(*first != '\n' && lineIsComment) --comment_cursor;
                else if (*first !='\n') ++comment_cursor;

                lines[i + 1] += (size * comment_cursor);
            }

            if(*first !='\n') {
            addCommentToLine(code, lines[i], size, comment);
            }
        }

        free(lines);

    }
    else addCommentToLine(code, getLine(code), size, comment);
}
static bool goPrevBookmark(Code* code, char* ptr)
{
    const CodeState* state = getState(code, ptr);
    while(ptr >= code->src)
    {
        if(state->bookmark)
        {
            updateCursorPosition(code, ptr);
            centerScroll(code);
            return true;
        }

        ptr--;
        state--;
    }

    return false;
}

static bool goNextBookmark(Code* code, char* ptr)
{
    const CodeState* state = getState(code, ptr);
    while(*ptr)
    {
        if(state->bookmark)
        {
            updateCursorPosition(code, ptr);
            centerScroll(code);
            return true;
        }

        ptr++;
        state++;
    }

    return false;
}

static void processKeyboard(Code* code)
{
    tic_mem* tic = code->tic;

    if(tic->ram.input.keyboard.data == 0) return;

    bool usedClipboard = true;

    switch(getClipboardEvent())
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(code); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(code); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(code); break;
    default: usedClipboard = false; break;
    }

    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool alt = tic_api_key(tic, tic_key_alt);

    bool changedSelection = false;
    if(keyWasPressed(tic_key_up)
        || keyWasPressed(tic_key_down)
        || keyWasPressed(tic_key_left)
        || keyWasPressed(tic_key_right)
        || keyWasPressed(tic_key_home)
        || keyWasPressed(tic_key_end)
        || keyWasPressed(tic_key_pageup)
        || keyWasPressed(tic_key_pagedown))
    {
        if(!shift) code->cursor.selection = NULL;
        else if(code->cursor.selection == NULL) code->cursor.selection = code->cursor.position;
        changedSelection = true;
    }

    bool usedKeybinding = true;

    // handle bookmarks
    if(keyWasPressed(tic_key_f1))
    {
        if(ctrl && shift)
        {
            for(CodeState* s = code->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
                s->bookmark = 0;
        }
        else if(ctrl)
        {
            toggleBookmark(code, getLineByPos(code, code->cursor.position));
        }
        else if(shift)
        {
            if(!goPrevBookmark(code, getPrevLineByPos(code, code->cursor.position)))
                goPrevBookmark(code, code->src + strlen(code->src));
        }
        else
        {
            if(!goNextBookmark(code, getNextLineByPos(code, code->cursor.position)))
                goNextBookmark(code, code->src);
        }
    }
    else usedKeybinding = false;

    if(ctrl)
    {
        if(keyWasPressed(tic_key_left))             leftWord(code);
        else if(keyWasPressed(tic_key_right))       rightWord(code);
        else if(keyWasPressed(tic_key_tab))         doTab(code, shift, ctrl);
        else if(keyWasPressed(tic_key_a))           selectAll(code);
        else if(keyWasPressed(tic_key_z))           undo(code);
        else if(keyWasPressed(tic_key_y))           redo(code);
        else if(keyWasPressed(tic_key_f))           setCodeMode(code, TEXT_FIND_MODE);
        else if(keyWasPressed(tic_key_g))           setCodeMode(code, TEXT_GOTO_MODE);
        else if(keyWasPressed(tic_key_o))           setCodeMode(code, TEXT_OUTLINE_MODE);
        else if(keyWasPressed(tic_key_n))           downLine(code);
        else if(keyWasPressed(tic_key_p))           upLine(code);
        else if(keyWasPressed(tic_key_e))           endLine(code);
        else if(keyWasPressed(tic_key_slash))       commentLine(code);
        else if(keyWasPressed(tic_key_home))        goCodeHome(code);
        else if(keyWasPressed(tic_key_end))         goCodeEnd(code);
        else if(keyWasPressed(tic_key_delete))      deleteWord(code);
        else if(keyWasPressed(tic_key_backspace))   backspaceWord(code);
        else                                        usedKeybinding = false;
    }
    else if(shift)
    {
        if(keyWasPressed(tic_key_return))   newLineAutoClose(code);
        else                                usedKeybinding = false;
    }
    else if(alt)
    {
        if(keyWasPressed(tic_key_left))         leftWord(code);
        else if(keyWasPressed(tic_key_right))   rightWord(code);
        else                                    usedKeybinding = false;
    }
    else
    {
        if(keyWasPressed(tic_key_up))               upLine(code);
        else if(keyWasPressed(tic_key_down))        downLine(code);
        else if(keyWasPressed(tic_key_left))        leftColumn(code);
        else if(keyWasPressed(tic_key_right))       rightColumn(code);
        else if(keyWasPressed(tic_key_home))        goHome(code);
        else if(keyWasPressed(tic_key_end))         goEnd(code);
        else if(keyWasPressed(tic_key_pageup))      pageUp(code);
        else if(keyWasPressed(tic_key_pagedown))    pageDown(code);
        else if(keyWasPressed(tic_key_delete))      deleteChar(code);
        else if(keyWasPressed(tic_key_backspace))   backspaceChar(code);
        else if(keyWasPressed(tic_key_return))      newLine(code);
        else if(keyWasPressed(tic_key_tab))         doTab(code, shift, ctrl);
        else                                        usedKeybinding = false;
    }

    if(usedClipboard || changedSelection || usedKeybinding) updateEditor(code);
}

static void processMouse(Code* code)
{
    tic_mem* tic = code->tic;

    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    if(checkMousePos(&rect))
    {
        bool useDrag = (code->mode == TEXT_DRAG_CODE && checkMouseDown(&rect, tic_mouse_left)) || checkMouseDown(&rect, tic_mouse_right);
        setCursor(code->mode == TEXT_DRAG_CODE || useDrag ? tic_cursor_hand : tic_cursor_ibeam);

        if(code->scroll.active)
        {
            if(useDrag)
            {
                code->scroll.x = (code->scroll.start.x - tic_api_mouse(tic).x) / getFontWidth(code);
                code->scroll.y = (code->scroll.start.y - tic_api_mouse(tic).y) / STUDIO_TEXT_HEIGHT;

                normalizeScroll(code);
            }
            else code->scroll.active = false;
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
                if(checkMouseDown(&rect, tic_mouse_left))
                {
                    s32 mx = tic_api_mouse(tic).x;
                    s32 my = tic_api_mouse(tic).y;

                    s32 x = (mx - rect.x) / getFontWidth(code);
                    s32 y = (my - rect.y) / STUDIO_TEXT_HEIGHT;

                    char* position = code->cursor.position;
                    setCursorPosition(code, x + code->scroll.x, y + code->scroll.y);

                    if(tic_api_key(tic, tic_key_shift))
                    {
                        code->cursor.selection = code->cursor.position;
                        code->cursor.position = position;
                    }
                    else if(!code->cursor.mouseDownPosition)
                    {
                        code->cursor.selection = code->cursor.position;
                        code->cursor.mouseDownPosition = code->cursor.position;
                    }
                }
                else
                {
                    if(code->cursor.mouseDownPosition == code->cursor.position)
                        code->cursor.selection = NULL;

                    code->cursor.mouseDownPosition = NULL;
                }
            }
        }
    }
}

static void textDragTick(Code* code)
{
    tic_mem* tic = code->tic;

    processMouse(code);

    tic_api_cls(code->tic, getConfig()->theme.code.BG);

    drawCode(code, true);   
    drawStatus(code);
}

static void textEditTick(Code* code)
{
    tic_mem* tic = code->tic;

    // process scroll
    {
        tic80_input* input = &code->tic->ram.input;

        tic_point scroll = {input->mouse.scrollx, input->mouse.scrolly};

        if(tic_api_key(tic, tic_key_shift))
            scroll.x = scroll.y;

        s32* val = scroll.x ? &code->scroll.x : scroll.y ? &code->scroll.y : NULL;

        if(val)
        {
            enum{Scroll = 3};
            s32 delta = scroll.x ? scroll.x : scroll.y;
            *val += delta > 0 ? -Scroll : Scroll;
            normalizeScroll(code);
        }
    }

    processKeyboard(code);

    if(!tic_api_key(tic, tic_key_ctrl) && !tic_api_key(tic, tic_key_alt))
    {
        char sym = getKeyboardText();

        if(sym)
        {
            inputSymbol(code, sym);
            updateEditor(code);
        }
    }

    processMouse(code);

    tic_api_cls(code->tic, getConfig()->theme.code.BG);

    drawCode(code, true);   
    drawStatus(code);
}

static void drawPopupBar(Code* code, const char* title)
{
    enum {TextX = BOOKMARK_WIDTH, TextY = TOOLBAR_SIZE + 1};

    tic_api_rect(code->tic, 0, TOOLBAR_SIZE, TIC80_WIDTH, TIC_FONT_HEIGHT + 1, tic_color_grey);

    if(code->shadowText)
        tic_api_print(code->tic, title, TextX+1, TextY+1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, title, TextX, TextY, tic_color_white, true, 1, code->altFont);

    if(code->shadowText)
        tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code) + 1, TextY+1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code), TextY, tic_color_white, true, 1, code->altFont);

    drawCursor(code, TextX+(s32)(strlen(title) + strlen(code->popup.text)) * getFontWidth(code), TextY, ' ');
}

static void updateFindCode(Code* code, char* pos)
{
    if(pos)
    {
        code->cursor.position = pos;
        code->cursor.selection = pos + strlen(code->popup.text);

        centerScroll(code);
        updateEditor(code);
    }
}

static char* upStrStr(const char* start, const char* from, const char* substr)
{
    const char* ptr = from-1;
    size_t len = strlen(substr);

    if(len > 0)
    {
        while(ptr >= start)
        {
            if(memcmp(ptr, substr, len) == 0)
                return (char*)ptr;

            ptr--;
        }
    }

    return NULL;
}

static char* downStrStr(const char* start, const char* from, const char* substr)
{
    return strstr(from, substr);
}

static void textFindTick(Code* code)
{
    if(keyWasPressed(tic_key_return)) setCodeMode(code, TEXT_EDIT_MODE);
    else if(keyWasPressed(tic_key_up)
        || keyWasPressed(tic_key_down)
        || keyWasPressed(tic_key_left)
        || keyWasPressed(tic_key_right))
    {
        if(*code->popup.text)
        {
            bool reverse = keyWasPressed(tic_key_up) || keyWasPressed(tic_key_left);
            char* (*func)(const char*, const char*, const char*) = reverse ? upStrStr : downStrStr;
            char* from = reverse ? MIN(code->cursor.position, code->cursor.selection) : MAX(code->cursor.position, code->cursor.selection);
            char* pos = func(code->src, from, code->popup.text);
            updateFindCode(code, pos);
        }
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym , 0};
            strcat(code->popup.text, str);
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    tic_api_cls(code->tic, getConfig()->theme.code.BG);

    drawCode(code, false);
    drawPopupBar(code, "FIND:");
    drawStatus(code);
}

static void updateGotoCode(Code* code)
{
    s32 line = atoi(code->popup.text);

    if(line) line--;

    s32 count = getLinesCount(code);

    if(line > count) line = count;

    code->cursor.selection = NULL;
    setCursorPosition(code, 0, line);

    code->jump.line = line;

    centerScroll(code);
    updateEditor(code);
}

static void textGoToTick(Code* code)
{
    tic_mem* tic = code->tic;

    if(keyWasPressed(tic_key_return))
    {
        if(*code->popup.text)
            updateGotoCode(code);

        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateGotoCode(code);
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text)+1 < sizeof code->popup.text && sym >= '0' && sym <= '9')
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            updateGotoCode(code);
        }
    }

    tic_api_cls(tic, getConfig()->theme.code.BG);

    if(code->jump.line >= 0)
        tic_api_rect(tic, 0, (code->jump.line - code->scroll.y) * (TIC_FONT_HEIGHT+1) + TOOLBAR_SIZE,
            TIC80_WIDTH, TIC_FONT_HEIGHT+2, getConfig()->theme.code.select);

    drawCode(code, false);
    drawPopupBar(code, "GOTO:");
    drawStatus(code);
}

static void drawOutlineBar(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;
    tic_rect rect = {x, y, TIC80_WIDTH - x, TIC80_HEIGHT - y};

    if(checkMousePos(&rect))
    {
        s32 mx = tic_api_mouse(tic).y - rect.y;
        mx /= STUDIO_TEXT_HEIGHT;
        mx += code->outline.scroll;

        if(mx >= 0 && mx < code->outline.size && code->outline.items[mx].pos)
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                code->outline.index = mx;
                updateOutlineCode(code);
            }

            if(checkMouseClick(&rect, tic_mouse_left))
                setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    tic_api_rect(code->tic, rect.x-1, rect.y, rect.w+1, rect.h, tic_color_grey);

    y -= code->outline.scroll * STUDIO_TEXT_HEIGHT - 1;

    char filter[STUDIO_TEXT_BUFFER_WIDTH];
    strncpy(filter, code->popup.text, sizeof(filter));

    if(code->outline.items)
    {
        tic_api_rect(code->tic, rect.x - 1, rect.y + (code->outline.index - code->outline.scroll) * STUDIO_TEXT_HEIGHT,
            rect.w + 1, TIC_FONT_HEIGHT + 2, tic_color_red);

        for(const tic_outline_item* ptr = code->outline.items, *end = ptr + code->outline.size; 
            ptr < end; ptr++, y += STUDIO_TEXT_HEIGHT)
        {
            char orig[STUDIO_TEXT_BUFFER_WIDTH] = {0};
            strncpy(orig, ptr->pos, MIN(ptr->size, sizeof(orig)));
            drawFilterMatch(code, x, y, orig, filter);
        }
    }
    else
    {
        if(code->shadowText)
            tic_api_print(code->tic, "(empty)", x+1, y+1, tic_color_black, true, 1, code->altFont);

        tic_api_print(code->tic, "(empty)", x, y, tic_color_white, true, 1, code->altFont);
    }
}

static void normOutlineScroll(Code* code)
{
    code->outline.scroll = code->outline.size > TEXT_BUFFER_HEIGHT 
        ? CLAMP(code->outline.scroll, 0, code->outline.size - TEXT_BUFFER_HEIGHT) : 0;
}

static void updateOutlineIndex(Code* code, s32 value)
{
    if(code->outline.size == 0)
        return;

    code->outline.index = CLAMP(value, 0, code->outline.size - 1);

    if(code->outline.index - code->outline.scroll < 0)
        code->outline.scroll -= TEXT_BUFFER_HEIGHT;
    else if(code->outline.index - code->outline.scroll >= TEXT_BUFFER_HEIGHT)
        code->outline.scroll += TEXT_BUFFER_HEIGHT;

    updateOutlineCode(code);
    normOutlineScroll(code);
}

static void textOutlineTick(Code* code)
{
    // process scroll
    {
        tic80_input* input = &code->tic->ram.input;

        if(input->mouse.scrolly)
        {
            enum{Scroll = 3};
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            code->outline.scroll += delta;
            normOutlineScroll(code);
        }
    }

    if(keyWasPressed(tic_key_up))
        updateOutlineIndex(code, code->outline.index - 1);

    else if(keyWasPressed(tic_key_down))
        updateOutlineIndex(code, code->outline.index + 1);

    else if(keyWasPressed(tic_key_left) || keyWasPressed(tic_key_pageup))
        updateOutlineIndex(code, code->outline.index - TEXT_BUFFER_HEIGHT);

    else if(keyWasPressed(tic_key_right) || keyWasPressed(tic_key_pagedown))
        updateOutlineIndex(code, code->outline.index + TEXT_BUFFER_HEIGHT);

    else if(keyWasPressed(tic_key_home))
        updateOutlineIndex(code, 0);

    else if(keyWasPressed(tic_key_end))
        updateOutlineIndex(code, code->outline.size - 1);

    else if(keyWasPressed(tic_key_return))
    {
        updateOutlineCode(code);
        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyWasPressed(tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            setOutlineMode(code);
        }
    }

    char sym = getKeyboardText();

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            setOutlineMode(code);
        }
    }

    tic_api_cls(code->tic, getConfig()->theme.code.BG);

    drawCode(code, false);
    drawStatus(code);
    drawOutlineBar(code, TIC80_WIDTH - 13 * TIC_FONT_WIDTH, 2*(TIC_FONT_HEIGHT+1));
    drawPopupBar(code, "FUNC:");
}

static void drawFontButton(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("SWITCH FONT");

        over = true;

        if(checkMouseClick(&rect, tic_mouse_left))
        {
            code->altFont = !code->altFont;
        }
    }

    drawChar(tic, 'F', x, y, over ? tic_color_grey : tic_color_light_grey, code->altFont);
}

static void drawShadowButton(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("SHOW SHADOW");

        over = true;

        if(checkMouseClick(&rect, tic_mouse_left))
        {
            code->shadowText = !code->shadowText;
        }
    }

    drawBitIcon(tic_icon_shadow, x, y, over && !code->shadowText ? tic_color_grey : tic_color_light_grey);

    if(code->shadowText)
        drawBitIcon(tic_icon_shadow2, x, y, tic_color_black);
}

static void drawCodeToolbar(Code* code)
{
    tic_api_rect(code->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    static u8 Icons[] = {tic_icon_run, tic_icon_hand, tic_icon_find, tic_icon_goto, tic_icon_outline};
    static const char* Tips[] = {"RUN [ctrl+r]", "DRAG [right mouse]", "FIND [ctrl+f]", "GOTO [ctrl+g]", "OUTLINE [ctrl+o]"};

    enum {Count = COUNT_OF(Icons)};
    enum {Size = 7};

    for(s32 i = 0; i < Count; i++)
    {
        tic_rect rect = {TIC80_WIDTH + (i - Count) * Size, 0, Size, Size};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip(Tips[i]);

            over = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                switch(i)
                {
                case TEXT_RUN_CODE:
                    runProject();
                    break;
                default:
                    {
                        s32 mode = TEXT_EDIT_MODE + i;

                        if(code->mode == mode) code->escape(code);
                        else setCodeMode(code, mode);
                    }
                }
            }
        }

        bool active = i == code->mode - TEXT_EDIT_MODE && i != 0;
        if (active)
        {
            tic_api_rect(code->tic, rect.x, rect.y, Size, Size, tic_color_grey);
            drawBitIcon(Icons[i], rect.x, rect.y + 1, tic_color_black);
        }
        drawBitIcon(Icons[i], rect.x, rect.y, active ? tic_color_white : (over ? tic_color_grey : tic_color_light_grey));
    }

    drawFontButton(code, TIC80_WIDTH - (Count+3) * Size, 1);
    drawShadowButton(code, TIC80_WIDTH - (Count+2) * Size, 0);

    drawToolbar(code->tic, false);
}

static void tick(Code* code)
{
    if(code->cursor.delay)
        code->cursor.delay--;

    switch(code->mode)
    {
    case TEXT_RUN_CODE:     runProject();           break;
    case TEXT_DRAG_CODE:    textDragTick(code);     break;
    case TEXT_EDIT_MODE:    textEditTick(code);     break;
    case TEXT_FIND_MODE:    textFindTick(code);     break;
    case TEXT_GOTO_MODE:    textGoToTick(code);     break;
    case TEXT_OUTLINE_MODE: textOutlineTick(code);  break;
    }

    drawCodeToolbar(code);

    code->tickCounter++;
}

static void escape(Code* code)
{
    switch(code->mode)
    {
    case TEXT_EDIT_MODE:
        break;
    case TEXT_DRAG_CODE:
        code->mode = TEXT_EDIT_MODE;
    break;
    default:
        code->cursor.position = code->popup.prevPos;
        code->cursor.selection = code->popup.prevSel;
        code->popup.prevSel = code->popup.prevPos = NULL;

        code->mode = TEXT_EDIT_MODE;

        updateEditor(code);
    }
}

static void onStudioEvent(Code* code, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT: cutToClipboard(code); break;
    case TIC_TOOLBAR_COPY: copyToClipboard(code); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(code); break;
    case TIC_TOOLBAR_UNDO: undo(code); break;
    case TIC_TOOLBAR_REDO: redo(code); break;
    }
}

void initCode(Code* code, tic_mem* tic, tic_code* src)
{
    bool firstLoad = code->state == NULL;
    if(code->state != NULL)
        free(code->state);

    if(code->history) history_delete(code->history);

    *code = (Code)
    {
        .tic = tic,
        .src = src->data,
        .tick = tick,
        .escape = escape,
        .cursor = {{src->data, NULL, 0}, NULL, 0},
        .scroll = {0, 0, {0, 0}, false},
        .state = calloc(TIC_CODE_SIZE, sizeof(CodeState)),
        .tickCounter = 0,
        .history = NULL,
        .mode = TEXT_EDIT_MODE,
        .jump = {.line = -1},
        .popup =
        {
            .prevPos = NULL,
            .prevSel = NULL,
        },
        .outline =
        {
            .items = NULL,
            .size = 0,
            .index = 0,
            .scroll = 0,
        },
        .matchedDelim = NULL,
        .altFont = firstLoad ? getConfig()->theme.code.altFont : code->altFont,
        .shadowText = getConfig()->theme.code.shadow,
        .event = onStudioEvent,
        .update = update,
    };

    packState(code);
    code->history = history_create(code->state, sizeof(CodeState) * TIC_CODE_SIZE);

    update(code);
}

void freeCode(Code* code)
{
    history_delete(code->history);
    free(code->state);
    free(code);
}
