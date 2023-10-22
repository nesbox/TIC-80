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
#include "tic_assert.h"

#define TEXT_CURSOR_DELAY (TIC80_FRAMERATE / 2)
#define TEXT_CURSOR_BLINK_PERIOD TIC80_FRAMERATE
#define BOOKMARK_WIDTH 7
#define CODE_EDITOR_WIDTH (TIC80_WIDTH - BOOKMARK_WIDTH)
#define CODE_EDITOR_HEIGHT (TIC80_HEIGHT - TOOLBAR_SIZE - STUDIO_TEXT_HEIGHT)
#define TEXT_BUFFER_HEIGHT (CODE_EDITOR_HEIGHT / STUDIO_TEXT_HEIGHT)
#define SIDEBAR_WIDTH (12 * TIC_FONT_WIDTH)

#if defined(TIC80_PRO)
#   define MAX_CODE sizeof(tic_code)
#else
#   define MAX_CODE TIC_BANK_SIZE
#endif

#define noop (void)0

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

//if pos_undo is true, we set the position to the first character
//that is different between the current source and the state (wanted undo behavior)
//otherwise we set the position to the stored location (wanted redo behavior)
//in either case if no change was detected we do not change the position
//(this occurs when there are no undo's or redo's left to apply)
static void unpackState(Code* code, bool pos_undo)
{
    char* src = code->src;

    char* first_change = NULL;
    char* stored_pos = NULL;

    for(CodeState* s = code->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
    {

        if (first_change == NULL && *src != s->sym)
            first_change = src;

        if(s->cursor)
            stored_pos = src;

        *src++ = s->sym;
    }

    if (first_change != NULL) {

        //we actually will want to go the one before the first change, as if 
        //we were about to make the change
        //ternary to make sure we don't go one before the beginning
        if (pos_undo) 
            code->cursor.position = first_change == code->src ? code->src : (first_change - 1);

        else code->cursor.position = stored_pos;
    }
}

static void history(Code* code)
{
    //if we are in insert mode we want want all changes we make to be reflected
    //in the undo/redo history only when we leave it
    if (checkStudioViMode(code->studio, VI_INSERT))
        return;
    packState(code);
    history_add(code->history);
}

static void drawStatus(Code* code)
{
    enum {Height = TIC_FONT_HEIGHT + 1, StatusY = TIC80_HEIGHT - TIC_FONT_HEIGHT};

    tic_api_rect(code->tic, 0, TIC80_HEIGHT - Height, TIC80_WIDTH, Height, code->status.color);
    tic_api_print(code->tic, code->status.line, 0, StatusY, getConfig(code->studio)->theme.code.BG, true, 1, false);
    tic_api_print(code->tic, code->status.size, TIC80_WIDTH - (s32)strlen(code->status.size) * TIC_FONT_WIDTH, 
        StatusY, getConfig(code->studio)->theme.code.BG, true, 1, false);
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

    if(checkMousePos(code->studio, &rect))
    {
        setCursor(code->studio, tic_cursor_hand);

        showTooltip(code->studio, "BOOKMARK [ctrl+f1]");

        s32 line = (tic_api_mouse(tic).y - rect.y) / STUDIO_TEXT_HEIGHT;

        drawBitIcon(code->studio, tic_icon_bookmark, rect.x, rect.y + line * STUDIO_TEXT_HEIGHT - 1, tic_color_dark_grey);

        if(checkMouseClick(code->studio, &rect, tic_mouse_left))
            toggleBookmark(code, getPosByLine(code->src, line + code->scroll.y));
    }

    const char* pointer = code->src;
    const CodeState* syntaxPointer = code->state;
    s32 y = -code->scroll.y;

    while(*pointer)
    {
        if(syntaxPointer++->bookmark)
        {
            drawBitIcon(code->studio, tic_icon_bookmark, rect.x, rect.y + y * STUDIO_TEXT_HEIGHT, tic_color_black);
            drawBitIcon(code->studio, tic_icon_bookmark, rect.x, rect.y + y * STUDIO_TEXT_HEIGHT - 1, tic_color_yellow);
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

static int drawTab(Code* code, s32 x, s32 y, u8 color) 
{
    tic_mem* tic = code->tic;
    s32 tab_size = getConfig(code->studio)->options.tabSize;

    s32 count = 0;
    while (count < tab_size) {
        drawChar(tic, '\t', x, y, color, code->altFont);
        count++;
        if (x / getFontWidth(code) % tab_size == 0) 
            break;
        x += getFontWidth(code);
    }
    return getFontWidth(code) * count;
}

static void drawCursor(Code* code, s32 x, s32 y, char symbol)
{
    bool inverse = code->cursor.delay || code->tickCounter % TEXT_CURSOR_BLINK_PERIOD < TEXT_CURSOR_BLINK_PERIOD / 2;

    if (checkStudioViMode(code->studio, VI_NORMAL))
        inverse = true;

    if(inverse)
    {
        if(code->shadowText)
            tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, 0);

        tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, getConfig(code->studio)->theme.code.cursor);

        if(symbol)
            drawChar(code->tic, symbol, x, y, getConfig(code->studio)->theme.code.BG, code->altFont);
    }
}

static void drawMatchedDelim(Code* code, s32 x, s32 y, char symbol, u8 color)
{
    tic_api_rectb(code->tic, x-1, y-1, (getFontWidth(code))+1, TIC_FONT_HEIGHT+1,
                  getConfig(code->studio)->theme.code.cursor);
    drawChar(code->tic, symbol, x, y, color, code->altFont);
}

static void drawCode(Code* code, bool withCursor)
{
    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    s32 xStart = rect.x - code->scroll.x * getFontWidth(code);
    s32 x = xStart;
    s32 y = rect.y - code->scroll.y * STUDIO_TEXT_HEIGHT;
    const char* pointer = code->src;

    u8 selectColor = getConfig(code->studio)->theme.code.select;

    const u8* colors = (const u8*)&getConfig(code->studio)->theme.code;
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
        s32 x_offset = getFontWidth(code);

        if(x >= -getFontWidth(code) && x < TIC80_WIDTH && y >= -TIC_FONT_HEIGHT && y < TIC80_HEIGHT )
        {
            if(code->cursor.selection && pointer >= selection.start && pointer < selection.end)
            {
                if(code->shadowText)
                    tic_api_rect(code->tic, x, y, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, tic_color_black);

                if (symbol == '\t') {
                    //NOTE: this logic assumes that the tab character is blank
                    //is someone made a custom character for tab it won't show up
                    //in the selection
                    x_offset = drawTab(code, x, y, tic_color_dark_grey);
                    tic_api_rect(code->tic, x-1, y-1, x_offset, TIC_FONT_HEIGHT+1, selectColor);
                } else {
                    tic_api_rect(code->tic, x-1, y-1, getFontWidth(code)+1, TIC_FONT_HEIGHT+1, selectColor);
                    drawChar(code->tic, symbol, x, y, tic_color_dark_grey, code->altFont);
                }
            }
            else 
            {
                if(code->shadowText)
                    drawChar(code->tic, symbol, x+1, y+1, 0, code->altFont);

                if (symbol == '\t')
                    x_offset = drawTab(code, x, y, colors[syntaxPointer->syntax]);
                else
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
        else x += x_offset;

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

const char matchingDelim(const char current)
{
    char match = 0;
    switch (current)
    {
    case '(': match = ')'; break;
    case ')': match = '('; break;
    case '[': match = ']'; break;
    case ']': match = '['; break;
    case '{': match = '}'; break;
    case '}': match = '{'; break;
    }
    return match;
}

const char* findMatchedDelim(Code* code, const char* current)
{
    const char* start = code->src;
    // delimiters inside comments and strings don't get to be matched!
    if(code->state[current - start].syntax == SyntaxType_COMMENT ||
       code->state[current - start].syntax == SyntaxType_STRING) return 0;

    char initial = *current;
    char seeking = matchingDelim(initial);
    if(seeking == 0) return NULL;

    s8 dir = (initial == '(' || initial == '[' || initial == '{') ? 1 : -1;

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
    if(getConfig(code->studio)->theme.code.matchDelimiters)
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
static inline bool isdoublequote(char c) {return c == '"'; }
static inline bool isopenparen_(Code* code, char c) {return c == '(' || c == '{' || c == '[';}
static inline bool iscloseparen_(Code* code, char c) {return c == ')' || c == '}' || c == ']';}

static inline bool config_isalnum_(const tic_script_config* config, char c)
{
    if (config->lang_isalnum)
        return config->lang_isalnum(c);
    else
        return isalnum(c) || c == '_';
}

static inline bool isalnum_(Code* code, char c)
{
    tic_mem* tic = code->tic;
    const tic_script_config* config = tic_core_script_config(tic);
    return config_isalnum_(config, c);
}


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
            while(!islineend(*ptr) && config_isalnum_(config, *ptr)) ptr++;

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
                static const char* const ApiKeywords[] = {
#define             TIC_CALLBACK_DEF(name, ...) #name,
                    TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef              TIC_CALLBACK_DEF

#define             API_KEYWORD_DEF(name, ...) #name,
                    TIC_API_LIST(API_KEYWORD_DEF)
#undef              API_KEYWORD_DEF
                };
                const char* const* keywords = config->api_keywordsCount > 0 ? config->api_keywords : ApiKeywords;
                const s32 apiCount = config->api_keywordsCount > 0 ? config->api_keywordsCount : COUNT_OF(ApiKeywords);

                for(s32 i = 0; i < apiCount; i++)
                    if(len == strlen(keywords[i]) && memcmp(wordStart, keywords[i], len) == 0)
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
            else if ((config->stdStringStartEnd == NULL && (c == '"' || c == '\''))
                     || (config->stdStringStartEnd != NULL && c != 0 && strchr(config->stdStringStartEnd, c)))
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

static void startLine(Code* code)
{
    char* start = code->src;
    while(code->cursor.position > start+1)
    {
        if (islineend(*(code->cursor.position-1)))
            break;
        --code->cursor.position;
    }
    updateColumn(code);
}

static void endLine(Code* code)
{
    while(*code->cursor.position)
    {
        if (islineend(*code->cursor.position))
            break;
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

typedef bool(*tic_code_predicate)(Code* code, char c);

static char* leftPos(Code* code, char* pos, tic_code_predicate pred)
{
    const char* start = code->src;
    if(pos > start)
    {
        if(pred(code, *pos)) while(pos > start && pred(code, *(pos-1))) pos--;
        else while(pos > start && !pred(code, *(pos-1))) pos--;
        return pos;
    }

    return code->cursor.position;
}

static char* leftWordPos(Code* code)
{
    return leftPos(code, code->cursor.position-1, isalnum_);
}

static void leftWord(Code* code)
{
    code->cursor.position = leftWordPos(code);
    updateColumn(code);
}

static char* leftQuote(Code* code, char* pos)
{
    char* start = code->src;
    while (pos > start)
    {
        if (isdoublequote(*pos) && *(pos-1) != '\\')
            return pos;
        --pos;
    }
    return start;
}

static char* rightQuote(Code* code, char* pos)
{
    char* end = code->src + strlen(code->src);
    while (pos < end)
    {
        if (isdoublequote(*pos) && *(pos-1) != '\\')
            return pos;
        ++pos;
    }
    return end;
}

static char* leftSexp(Code* code, char* pos)
{
    char* start = code->src;
    int nesting = 0;
    
    while (pos >= start)
    {
        if (isopenparen_(code, *pos))
        {
            if (nesting == 0)
                return pos;
            else
                --nesting;
        }
        else if (iscloseparen_(code, *pos))
        {
            ++nesting;
        }
        --pos;
    }
    return start; // fallback
}

static char* rightPos(Code* code, char* pos, tic_code_predicate pred)
{
    const char* end = code->src + strlen(code->src);

    if(pos < end)
    {
        if(pred(code, *pos)) while(pos < end && pred(code, *pos)) pos++;
        else while(pos < end && !pred(code, *pos)) pos++;
    }

    return pos;
}

static char* rightWordPos(Code* code)
{
    return rightPos(code, code->cursor.position, isalnum_);
}

static void rightWord(Code* code)
{
    code->cursor.position = rightWordPos(code);
    updateColumn(code);
}

static char* rightSexp(Code* code, char* pos)
{
    char* end = code->src + strlen(code->src);
    int nesting = 0;

    while (pos < end)
    {
        if (iscloseparen_(code, *pos))
        {
            if (nesting == 0)
                return pos;
            else
                --nesting;
        }
        else if (isopenparen_(code, *pos))
        {
            ++nesting;
        }
        ++pos;
    }
    return end; // fallback
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

static void halfPageUp(Code* code)
{
    s32 half = TEXT_BUFFER_HEIGHT / 2;
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    setCursorPosition(code, column, line > half ? line - half : 0);
}
static void halfPageDown(Code* code)
{
    s32 half = TEXT_BUFFER_HEIGHT / 2;
    s32 column = 0;
    s32 line = 0;
    getCursorPosition(code, &column, &line);
    s32 lines = getLinesCount(code);
    setCursorPosition(code, column, line < lines - half ? line + half : lines);
}

static void deleteCode(Code* code, char* start, char* end)
{
    s32 size = (s32)strlen(end) + 1;
    memmove(start, end, size);

    // delete code state
    memmove(getState(code, start), getState(code, end), size * sizeof(CodeState));
}

static void insertCodeSize(Code* code, char* dst, const char* src, s32 size)
{
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

static void insertCode(Code* code, char* dst, const char* src)
{
    insertCodeSize(code, dst, src, strlen(src));
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

static inline enum KeybindMode getKeybindMode(Code* code)
{
    return getConfig(code->studio)->options.keybindMode;
}


static inline bool shouldUseStructuredEdit(Code* code)
{
    const bool emacsMode = getKeybindMode(code) == KEYBIND_EMACS;
    return tic_core_script_config(code->tic)->useStructuredEdition && emacsMode;
}


static bool structuredDeleteOverride(Code* code, char* pos)
{
    if (!shouldUseStructuredEdit(code))
        return false;

    const char* end = code->src + strlen(code->src);
    if (pos >= end-1)
        return false;

    const bool isopen = isopenparen_(code, *pos);
    const bool isclose = iscloseparen_(code, *pos);
    if (isopen || isclose)
    {
        if (isopen && iscloseparen_(code, *(pos+1))
            || isclose && isopenparen_(code, *(pos+1)))
        {
            deleteCode(code, pos, pos+2);
            history(code);
            parseSyntaxColor(code);
            updateEditor(code);
        }
        // if not, disallow deletion until it's empty
        return true;
    }

    if (isdoublequote(*pos))
    {
       const bool canRemoveDblQuote = isdoublequote(*(pos+1));
       if (canRemoveDblQuote)
       {
           deleteCode(code, pos, pos+2);
           history(code);
           parseSyntaxColor(code);
           updateEditor(code);
       }
       // only allow deletion when string is empty
       return true;
    }
    
    return false;
}

static void deleteChar(Code* code)
{
    if(!replaceSelection(code))
    {
        if (structuredDeleteOverride(code, code->cursor.position))
            return;

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

        if (structuredDeleteOverride(code, pos))
            return;

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
        if(isalnum_(code, *pos)) while(pos < end && isalnum_(code, *pos)) pos++;
        else while(pos < end && !isalnum_(code, *pos)) pos++;

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
        if(isalnum_(code, *pos)) while(pos > start && isalnum_(code, *(pos-1))) pos--;
        else while(pos > start && !isalnum_(code, *(pos-1))) pos--;

        deleteCode(code, pos, code->cursor.position);

        code->cursor.position = pos;
        history(code);
        parseSyntaxColor(code);
    }
}

char* findLineEnd(Code* code, char* pos)
{
    char* lineend = pos+1;
    const char* end = code->src + strlen(code->src);
    
    while (lineend < end)
    {
        if (islineend(*lineend)) break;
        if (shouldUseStructuredEdit(code) && iscloseparen_(code, *lineend)) break;
        ++lineend;
    }
    return lineend;
}

static void deleteLine(Code* code)
{
    char* linestart = code->cursor.position;
    char* lineend = linestart+1;
    const char* end = code->src + strlen(code->src);
    
    if (shouldUseStructuredEdit(code))
    {
        if (islineend(*linestart) || islineend(*lineend))
            noop;
        else if (isopenparen_(code, *linestart))
            lineend = rightSexp(code, linestart+1)+1;
        else if (isdoublequote(*linestart))
        {
            char* nextQuote = rightQuote(code, linestart+1);
            lineend = findLineEnd(code, linestart);
            if (nextQuote < lineend)
                lineend = nextQuote+1;
            else
                return; // skip deletion if on last " of a string
        }
        else
        {
            const char* start = code->src;
            const char* sexpStart = leftSexp(code, linestart);
            const char* strStart = leftQuote(code, linestart);
            char* sexpEnd = rightSexp(code, linestart);
            char* strEnd = rightQuote(code, linestart);
            const bool isInsideStr = strStart > sexpStart && strEnd < sexpEnd;
            if (isInsideStr)
                lineend = strEnd;
            else if (sexpStart != start)
                lineend = sexpEnd;
            else
                lineend = findLineEnd(code, linestart);
        }
    }
    else
    {
        lineend = findLineEnd(code, linestart);
    }

    const size_t linesize = lineend-linestart;
    char* clipboard = (char*)malloc(linesize+1);
    if(clipboard)
    {
        memcpy(clipboard, linestart, linesize);
        clipboard[linesize] = '\0';
        tic_sys_clipboard_set(clipboard);
        free(clipboard);
    }

    
    deleteCode(code, linestart, lineend);
    code->cursor.position = linestart;
    history(code);
    parseSyntaxColor(code);
    updateEditor(code);
}

static void inputSymbolBase(Code* code, char sym)
{
    if (strlen(code->src) >= MAX_CODE)
        return;

    const bool useStructuredEdit = shouldUseStructuredEdit(code);
    const bool isOpeningDelimiter = sym == '(' || sym == '[' || sym == '{';

    if((useStructuredEdit || getConfig(code->studio)->theme.code.autoDelimiters) && isOpeningDelimiter)
    {
        insertCode(code, code->cursor.position++, (const char[]){sym, matchingDelim(sym), '\0'});
    } else if (useStructuredEdit && isdoublequote(sym)) {
        insertCode(code, code->cursor.position++, (const char[]){sym, sym, '\0'});
    }
    else {
        insertCode(code, code->cursor.position++, (const char[]){sym, '\0'});
    }

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

static void killSelection(Code* code)
{
    code->cursor.selection = NULL;
}

static void copyToClipboard(Code* code, bool killSelection)
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

    if (killSelection)
        code->cursor.selection = NULL;
}

static void cutToClipboard(Code* code, bool killSelection)
{
    if(code->cursor.selection == NULL || code->cursor.position == code->cursor.selection)
    {
        code->cursor.position = getLine(code);
        code->cursor.selection = getNextLine(code);
    }

    copyToClipboard(code, false);
    replaceSelection(code);

    if (killSelection)
        code->cursor.selection = NULL;
    
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
                replaceSelection(code);

                // cut clipboard code if overall code > max code size
                {
                    size_t codeSize = strlen(code->src);

                    if(codeSize >= MAX_CODE)
                        return;

                    if (codeSize + size > MAX_CODE)
                    {
                        size = MAX_CODE - codeSize;
                        clipboard[size] = '\0';
                    }
                }

                insertCode(code, code->cursor.position, clipboard);
                code->cursor.position += size;

                if (killSelection)
                    code->cursor.selection = NULL;

                history(code);
                parseSyntaxColor(code);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void update(Code* code)
{
    updateColumn(code);
    updateEditor(code);
    parseSyntaxColor(code);
}

static void undo(Code* code)
{
    history_undo(code->history);
    unpackState(code, true);

    update(code);
}

static void redo(Code* code)
{
    history_redo(code->history);
    unpackState(code, false);

    update(code);
}

static bool useSpacesForTab(Code* code) {
    enum TabMode tabmode = getConfig(code->studio)->options.tabMode;


    if (tabmode == TAB_SPACE)
        return true;
    else if (tabmode == TAB_TAB)
        return false;
    else { //auto mode
        tic_mem* tic = code->tic;
        const tic_script_config* config = tic_core_script_config(tic);
        if (config->id == 20 || config->id == 13)  //python or moonscript
            return true;
        return false;
    }
}


static s32 insertTab(Code* code, char* line_start, char* pos) {
    if (useSpacesForTab(code)) {
        s32 tab_size = getConfig(code->studio)->options.tabSize;
        s32 count = 0;

        while(count < tab_size) {
            insertCode(code, pos++, " ");
            count++;
            if ((pos - line_start) % tab_size == 0)
                break;
        }

        return count;
    } else {
        insertCode(code, pos, "\t");
        return 1;
    }
}

//has no effect is pos is not a valid tab character
static s32 removeTab(Code* code, char* line_start, char* pos) {
    if (useSpacesForTab(code)) {
        s32 tab_size = getConfig(code->studio)->options.tabSize;
        s32 count = 0;

        while(count < tab_size) {
            if (*pos != ' ')
                break;
            deleteCode(code, pos, pos+1);
            count++;
        }

        return count;
    } else if (*pos == '\t' || *pos == ' ') {
        deleteCode(code, pos, pos+1);
        return 1;
    }
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
                    end -= removeTab(code, line, line);
                    changed = true;
                }
            }
            else
            {
                end += insertTab(code, line, line);
                changed = true;
            }
            
            line = getNextLineByPos(code, line);
            if(line >= end) break;
        }
        
        if(changed) {
            
            if(has_selection) 
            {
                code->cursor.position = start;
                code->cursor.selection = end;
            }
            else if (start <= end) code->cursor.position = end;
            
            history(code);
            update(code);
        }
    }
    else 
    {
        char* line = getLineByPos(code, code->cursor.position);
        code->cursor.position += insertTab(code, line, code->cursor.position);
        history(code);
        update(code);
    }
}

// Add a block-ending keyword or symbol, and put the cursor in the line between.
static void newLineAutoClose(Code* code)
{
    const char* blockEnd = tic_core_script_config(code->tic)->blockEnd;
    if (blockEnd != NULL)
    {
        newLine(code);

        while(*blockEnd)
            inputSymbol(code, *blockEnd++);

        upLine(code);
        goEnd(code);
        doTab(code, false, true);
    }
}

static void setFindOrReplaceMode(Code* code)
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

static void recenterScroll(Code* code, bool emacsMode)
{
    s32 col, line;
    getCursorPosition(code, &col, &line);

    s32 desiredCol = col - CODE_EDITOR_WIDTH / getFontWidth(code) / 2;
    s32 desiredLine = MAX(0, line - TEXT_BUFFER_HEIGHT / 2);

    if (emacsMode && code->scroll.y == desiredLine)
    {
        desiredLine = line;
    }
    else if (emacsMode && code->scroll.y == line)
    {
        desiredLine = line - TEXT_BUFFER_HEIGHT;
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
    tic_mem* tic = code->tic;

    const tic_outline_item* item = code->sidebar.items + code->sidebar.index;

    if(code->sidebar.size && item && item->pos)
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

static bool isFilterMatch(const char* buffer, s32 size, const char* filter)
{
    while(size--)
        if(tolower(*buffer++) == tolower(*filter))
            filter++;

    return *filter == 0;
}

static void drawFilterMatch(Code *code, s32 x, s32 y, const char* orig, s32 size, const char* filter)
{
    while(size--)
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

static void initSidebarMode(Code* code)
{
    tic_mem* tic = code->tic;

    code->sidebar.size = 0;

    const tic_script_config* config = tic_core_script_config(tic);

    if(config->getOutline)
    {
        s32 size = 0;
        const tic_outline_item* items = config->getOutline(code->src, &size);

        if(items)
        {
            for(const tic_outline_item *it = items, *end = items + size; it != end ; ++it)
            {
                if(code->state[it->pos - code->src].syntax == SyntaxType_COMMENT)
                    continue;

                const char* filter = code->popup.text;
                if(*filter && !isFilterMatch(it->pos, it->size, filter))
                    continue;

                s32 last = code->sidebar.size++;
                code->sidebar.items = realloc(code->sidebar.items, code->sidebar.size * sizeof(tic_outline_item));
                code->sidebar.items[last] = *it;
            }
        }
    }
}

static void setBookmarkMode(Code* code)
{
    code->sidebar.index = 0;
    code->sidebar.scroll = 0;
    code->sidebar.size = 0;

    const char* ptr = code->src;
    const CodeState* state = code->state;

    while(*ptr)
    {
        if(state->bookmark)
        {
            s32 last = code->sidebar.size++;
            code->sidebar.items = realloc(code->sidebar.items, code->sidebar.size * sizeof(tic_outline_item));
            tic_outline_item* item = &code->sidebar.items[last];

            item->pos = ptr;
            item->size = getLineSize(ptr);
        }
        
        ptr++;
        state++;
    }

    updateSidebarCode(code);
}

static void setOutlineMode(Code* code)
{
    code->sidebar.index = 0;
    code->sidebar.scroll = 0;

    initSidebarMode(code);

    qsort(code->sidebar.items, code->sidebar.size, sizeof(tic_outline_item), funcCompare);
    updateSidebarCode(code);
}

static bool isIdle(Code* code)
{
    return code->anim.movie == &code->anim.idle;
}

static void setCodeMode(Code* code, s32 mode)
{
    if(isIdle(code) && code->mode != mode)
    {
        code->anim.movie = resetMovie(&code->anim.show);

        //so that in edit mode we can refer back to the most recently
        //searched thing if we want
        if (mode != TEXT_EDIT_MODE)
            strcpy(code->popup.text, "");

        code->popup.offset = NULL;
        code->popup.prevPos = code->cursor.position;
        code->popup.prevSel = code->cursor.selection;

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

    parseSyntaxColor(code);
}

static void sexpify(Code* code)
{
    if (!shouldUseStructuredEdit(code))
        return;

    char* pos = code->cursor.position;
    char* start = NULL;
    char* end = NULL;
    char* sel = code->cursor.selection;
    if (sel)
    {
        start = sel<pos ? sel : pos;
        end = sel<pos ? pos : sel;
    }
    else if (isopenparen_(code, *pos))
    {
        start = pos;
        end = rightSexp(code, pos+1);
    }
    else if (isalnum_(code, *pos))
    {
        start = leftPos(code, pos, isalnum_);
        end = rightPos(code, pos, isalnum_);
    }

    if (start == NULL || end == NULL)
        return;

    insertCode(code, start, (const char[]){'(', '\0'});
    insertCode(code, end+1, (const char[]){')', '\0'});

    ++code->cursor.position;
    history(code);
    parseSyntaxColor(code);
    updateEditor(code);
}

static void extirpSExp(Code* code)
{
    if (!shouldUseStructuredEdit(code))
        return;
    
    const char* start = code->src;
    char* pos = code->cursor.position;
    const char* end = code->src + strlen(code->src);

    char* sexp_start = NULL;
    char* sexp_end = NULL;

    if(pos > start)
    {
        if (isopenparen_(code, *pos))
        {
            sexp_start = pos;
            sexp_end = rightSexp(code, pos+1);
        }
        if (iscloseparen_(code, *pos))
        {
            sexp_start = leftSexp(code, pos-1);
            sexp_end = pos;
        }
        if (isalnum_(code, *pos))
        {
            sexp_start = leftPos(code, pos, isalnum_);
            sexp_end = rightPos(code, pos, isalnum_)-1;
        }
        if (sexp_start != NULL && sexp_end != NULL)
        {
            char* sexp_outer_start = leftSexp(code, sexp_start-1);
            char* sexp_outer_end = rightSexp(code, sexp_end+1);
            if (sexp_start > start && sexp_outer_start > start && sexp_end < end && sexp_outer_end < end)
            {
                deleteCode(code, sexp_end+1, sexp_outer_end+1);
                deleteCode(code, sexp_outer_start, sexp_start);
                code->cursor.position = sexp_outer_start;
                history(code);
                parseSyntaxColor(code);
                updateEditor(code);
            }
        }
    }
}

static void toggleMark(Code* code)
{
    if (code->cursor.selection)
        code->cursor.selection = NULL;
    else
        code->cursor.selection = code->cursor.position;

    parseSyntaxColor(code);
    updateEditor(code);
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

    history(code);
}

static void dupLine(Code* code)
{
    char *start = getLine(code);
    if(*start)
    {
        s32 size = getLineSize(start) + 1;

        insertCodeSize(code, start, start, size);

        code->cursor.position += size;

        history(code);
        parseSyntaxColor(code);
        updateEditor(code);
    }
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

static bool clipboardHasNewline() 
{
    bool found = false;
    if (tic_sys_clipboard_has()) 
    {

        //from what I can see in other usage, this should be a
        //null terminated string with the clipboard contents
        char* clipboard = tic_sys_clipboard_get();

        char* c = clipboard;
        while(*c != 0) 
        {
            if (*c == '\n') 
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


static void seekEmptyLineForward(Code* code) {
    char* pos = code->cursor.position;

    //goto state machine, one of the few ways to use goto well
start:
    if (*pos == '\0') goto end;
    else if (*pos == '\n') { pos++; goto check; }
    else { pos++; goto start; }

check:
    if (*pos == '\0') goto end;
    else if (*pos == '\t') { pos++; goto check; }
    else if (*pos == ' ') { pos++; goto check; }
    else if (*pos == '\n') goto end;
    else goto start;

end:
    code->cursor.position = pos;
    updateColumn(code);
    updateEditor(code);

}

static void seekEmptyLineBackward(Code* code) {
    char* pos = code->cursor.position;

    if (pos == code->src) return;
    else pos--; //need to start one behind so we don't match the one we are on

    //goto state machine, one of the few ways to use goto well
start:
    if (pos == code->src) goto end;
    else if (*pos == '\n') { pos--; goto check; }
    else { pos--; goto start; }

check:
    if (pos == code->src) goto end;
    else if (*pos == '\t') { pos--; goto check; }
    else if (*pos == ' ') { pos--; goto check; }
    else if (*pos == '\n') { pos++; goto end; }
    else goto start;

end:
    code->cursor.position = pos;
    updateColumn(code);
    updateEditor(code);
}

static void seekForward(Code* code, char sought) 
{
    char* start = code->cursor.position;
    code->cursor.position++;
    while(
        *code->cursor.position != sought 
        && *code->cursor.position != '\n' 
        && *code->cursor.position != '\0'
    )
        code->cursor.position++;
    if (*code->cursor.position == '\n' || *code->cursor.position == '\0')
        code->cursor.position = start;

    updateColumn(code);
    updateEditor(code);

}
static void seekBackward(Code* code, char sought) 
{
    char* start = code->cursor.position;
    code->cursor.position--;
    while(
        code->cursor.position > code->src 
        && *code->cursor.position != sought 
        && *code->cursor.position != '\n'
    )
        code->cursor.position--;

    if (code->cursor.position <= code->src || *code->cursor.position == '\n')
        code->cursor.position = start;

    updateColumn(code);
    updateEditor(code);
}

static void findNextPopupText(Code* code) {
    if (*code->popup.text) 
    {
        char* pos = downStrStr(code->src, code->cursor.position, code->popup.text);
        if (pos == code->cursor.position)
        {
            pos += strlen(code->popup.text);
            pos = downStrStr(code->src, pos, code->popup.text);
        }
        if (pos == NULL)
            pos = downStrStr(code->src, code->src, code->popup.text);
        if (pos != NULL)
        {
            code->cursor.position = pos;
            updateColumn(code);
        }
    }
}

static bool processViPosition(Code* code, bool ctrl, bool alt, bool shift) 
{
    bool clear = !(shift || ctrl || alt);

    bool processed = true;
    if (clear && keyWasPressed(code->studio, tic_key_k)) upLine(code);
    else if (clear && keyWasPressed(code->studio, tic_key_j)) downLine(code);
    else if (clear && keyWasPressed(code->studio, tic_key_h)) leftColumn(code);
    else if (clear && keyWasPressed(code->studio, tic_key_l)) rightColumn(code);

    //no need to be elitist, arrow keys can work too
    else if (keyWasPressed(code->studio, tic_key_up)) upLine(code);
    else if (keyWasPressed(code->studio, tic_key_down)) downLine(code);
    else if (keyWasPressed(code->studio, tic_key_left)) leftColumn(code);
    else if (keyWasPressed(code->studio, tic_key_right)) rightColumn(code);

    else if (clear && keyWasPressed(code->studio, tic_key_g)) goCodeHome(code);
    else if (shift && keyWasPressed(code->studio, tic_key_g)) goCodeEnd(code);

    else if (keyWasPressed(code->studio, tic_key_home)) goHome(code);
    else if (clear && keyWasPressed(code->studio, tic_key_0)) goHome(code);
    else if (keyWasPressed(code->studio, tic_key_end)) goEnd(code);
    else if (shift && keyWasPressed(code->studio, tic_key_4)) goEnd(code);

    else if (keyWasPressed(code->studio, tic_key_pageup)) pageUp(code);
    else if (ctrl && keyWasPressed(code->studio, tic_key_u)) pageUp(code);
    else if (keyWasPressed(code->studio, tic_key_pagedown)) pageDown(code);
    else if (ctrl && keyWasPressed(code->studio, tic_key_d)) pageDown(code);

    else if (clear && keyWasPressed(code->studio, tic_key_b)) leftWord(code);
    else if (clear && keyWasPressed(code->studio, tic_key_w)) rightWord(code);

    else if (shift && keyWasPressed(code->studio, tic_key_6))
    {
        goHome(code);
        while (*code->cursor.position == ' ' || *code->cursor.position == '\t') 
            code->cursor.position++;
    }

    else if (shift && keyWasPressed(code->studio, tic_key_j))
        halfPageDown(code);

    else if (shift && keyWasPressed(code->studio, tic_key_k))
        halfPageUp(code);

    else if (shift && keyWasPressed(code->studio, tic_key_leftbracket))
        seekEmptyLineBackward(code);

    else if (shift && keyWasPressed(code->studio, tic_key_rightbracket))
        seekEmptyLineForward(code);

    else if (shift && keyWasPressed(code->studio, tic_key_5))
    {
        const char* pos = findMatchedDelim(code, code->cursor.position);
        if (pos != NULL) 
        {
            code->cursor.position = (char*) pos;
            updateColumn(code);
            updateEditor(code);
        }
    }

    else if (clear && keyWasPressed(code->studio, tic_key_f))
        setStudioViMode(code->studio, VI_SEEK);

    else if (shift && keyWasPressed(code->studio, tic_key_f))
        setStudioViMode(code->studio, VI_SEEK_BACK);

    else if(clear && keyWasPressed(code->studio, tic_key_semicolon))
        seekForward(code, *code->cursor.position);

    else if(shift && keyWasPressed(code->studio, tic_key_semicolon))
        seekBackward(code, *code->cursor.position);

    else processed = false;

    return processed;
}

static void updateGotoCode(Code* code);
static void processViGoto(Code* code, s32 initial) 
{
    setCodeMode(code, TEXT_GOTO_MODE);
    code->popup.text[0] = '0' + initial;
    code->popup.text[1] = '\0';
    updateGotoCode(code);
}

static char* findStringStart(Code* code, char* pos) {
    char sentinel = *pos; 

    char* target = pos; //we will scan to the given position
    pos = getLineByPos(code, pos); //from the beginning of the line
    char* start = NULL;
                         
    //goto state machine!
start:
    if(pos == target) goto end;
    else if(*pos == '\\') { pos++; goto escape; }
    else if (*pos == sentinel) 
    { 
        start=start?NULL:pos; 
        pos++; 
        goto start;
    }
    else { pos++; goto start; }
escape :
    if (pos == target) goto end;
    else { pos++; goto start; }
end:
    return start;
}

static char* findStringEnd(char* pos) {
    char sentinel = *pos; //can handle single or double quotes
                         //or anything else I guess 
    if (*pos == '\0') goto end;
    else pos++; //move past the sentinel so we don't immediately detect the end

    //goto state machine!
start:
    if(*pos == '\0' || *pos == '\n' || *pos == sentinel) goto end;
    else if(*pos == '\\') { pos++; goto escape; }
    else { pos++; goto start; }
escape :
    if (*pos == '\0' || *pos == '\n') goto end;
    else { pos++; goto start; }
end:
    return pos;
}


//pass in pointer to beginnign of word and its length
//so you can just use the word in src
static char* findFunctionDefinition(Code* code, char* name, size_t length) {
    char* result = NULL;

    tic_mem* tic = code->tic;
    const tic_script_config* config = tic_core_script_config(tic);

    if(config->getOutline)
    {
        s32 osize = 0;
        const tic_outline_item* items = config->getOutline(code->src, &osize);

        if(items)
        {
            for(size_t i = 0; i < osize; i++)
            {
                const tic_outline_item* it = items + i;

                if(code->state[it->pos - code->src].syntax == SyntaxType_COMMENT)
                    continue;
                if (strncmp(name, it->pos, length) == 0)
                {
                    result = (char*) it->pos;
                    break;
                }
            }
        }
    }

    return result;
}


static void processViChange(Code* code) {
    //if on a delimiter change the contents of the delimiter
    if (matchingDelim(*code->cursor.position))
    {
        const char* match = findMatchedDelim(code, code->cursor.position);
        if (match == 0)
            deleteChar(code);
        else
        {
            if (code->cursor.position > match)
            {
                char* temp = code->cursor.position;
                code->cursor.position = (char*) match;
                match = temp;
            }
            deleteCode(code, code->cursor.position+1, (char*) match);
            code->cursor.position++;
        }
    }
    //if on a quotation seek to change within the quotation 
    else if(*code->cursor.position == '"' || *code->cursor.position == '\'') 
    {
        char* start = findStringStart(code, code->cursor.position);
        if (start == NULL) start = code->cursor.position;
        char* end = findStringEnd(start);
        start++; //advance one to leave the initial quote alone
        deleteCode(code, start, end);
        code->cursor.position = start;
    }
    else if(!isalnum_(code, *code->cursor.position)) 
        deleteChar(code);
    else  //change the word under the cursor
    {
        //only call left word if we are not already on the word border
        if (
            code->cursor.position > code->src 
            && isalnum_(code, *(code->cursor.position-1))
        )
            leftWord(code);
        deleteWord(code);
    }
}

static char toggleCase(char c) {
    if (isupper(c))
        return tolower(c);
    else if(islower(c))
        return toupper(c);
    else
        return c;
}

static void processViKeyboard(Code* code) 
{

    tic_mem* tic = code->tic;
    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool alt = tic_api_key(tic, tic_key_alt);
    bool clear = !(shift || ctrl || alt);

    ViMode mode = getStudioViMode(code->studio);

    //keep these the same to be consistent with the other editors
    bool usedClipboard = true;
    switch(getClipboardEvent(code->studio))
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(code, false); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(code, false); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(code, false); break;
    default: usedClipboard = false; break;
    }

    if(usedClipboard)
    {
        if (mode != VI_INSERT)
            setStudioViMode(code->studio, VI_NORMAL);
        updateEditor(code);
        return;
    }


    if (mode == VI_INSERT)
    {
        if (keyWasPressed(code->studio, tic_key_escape)) {
            setStudioViMode(code->studio, VI_NORMAL);
            history(code); //needs to come after the mode switch or won't be honored
        }

        else if (keyWasPressed(code->studio, tic_key_backspace)) 
            backspaceChar(code);

        else if (keyWasPressed(code->studio, tic_key_tab)) 
            doTab(code, shift, ctrl);

        else if (enterWasPressed(code->studio))
            newLine(code);

        else if (clear || shift)
        {
            char sym = getKeyboardText(code->studio);
            if(sym)
            {
                inputSymbol(code, sym);
                updateEditor(code);
            }
        }
    } 
    else if(mode == VI_NORMAL)
    {
        bool processed = true;

        code->cursor.selection = NULL;

        if (processViPosition(code, ctrl, alt, shift));

        else if (clear && keyWasPressed(code->studio, tic_key_i)) 
            setStudioViMode(code->studio, VI_INSERT);

        else if (clear && keyWasPressed(code->studio, tic_key_a)) 
        {
            setStudioViMode(code->studio, VI_INSERT);
            rightColumn(code);
        }
        else if (clear && keyWasPressed(code->studio, tic_key_o)) 
        {
            setStudioViMode(code->studio, VI_INSERT);
            goEnd(code);
            newLine(code);
        }

        else if (shift && keyWasPressed(code->studio, tic_key_o)) 
        {
            setStudioViMode(code->studio, VI_INSERT);
            goHome(code);
            newLine(code);
            upLine(code);
        }
        else if (clear && keyWasPressed(code->studio, tic_key_space)) 
        {
            size_t col = code->cursor.column;
            goEnd(code);
            newLine(code);
            code->cursor.column = col;
            upLine(code);
        }
        else if (shift && keyWasPressed(code->studio, tic_key_space)) 
        {
            size_t col = code->cursor.column;
            goHome(code);
            newLine(code);
            code->cursor.position += col;
            updateColumn(code);
        }

        else if (clear && keyWasPressed(code->studio, tic_key_v))
            setStudioViMode(code->studio, VI_SELECT);

        else if (clear && keyWasPressed(code->studio, tic_key_slash)) 
            setCodeMode(code, TEXT_FIND_MODE);

        else if(clear && keyWasPressed(code->studio, tic_key_n))
            findNextPopupText(code);

        else if(shift && keyWasPressed(code->studio, tic_key_3))
        {
            char* word_start = leftPos(code, code->cursor.position, isalnum_);
            char* word_end = rightPos(code, code->cursor.position, isalnum_);
            size_t size = word_end - word_start;
            if (size+1 < STUDIO_TEXT_BUFFER_WIDTH)
            {
                memcpy(code->popup.text, word_start, size);
                code->popup.text[size] = 0;
            }
            findNextPopupText(code);
        }
        else if(shift && keyWasPressed(code->studio, tic_key_n))
        {
            if (*code->popup.text) 
            {
                char* pos = upStrStr(code->src, code->cursor.position, code->popup.text);
                if (pos == NULL)
                    pos = upStrStr(code->src, code->src+strlen(code->src), code->popup.text);
                if (pos != NULL)
                {
                    code->cursor.position = pos;
                    updateColumn(code);
                }
            }
        }

        else if (clear && keyWasPressed(code->studio, tic_key_leftbracket))
        {
            char* word_start = leftPos(code, code->cursor.position, isalnum_);
            char* word_end = rightPos(code, code->cursor.position, isalnum_);
            size_t size = word_end - word_start;

            char* def = findFunctionDefinition(code, word_start, size);
            if (def != NULL)
            {
                code->cursor.position = def;
                updateColumn(code);
            }

        }

        else if (clear && keyWasPressed(code->studio, tic_key_r))
            setCodeMode(code, TEXT_REPLACE_MODE);

        else if (clear && keyWasPressed(code->studio, tic_key_u)) undo(code);
        else if (shift && keyWasPressed(code->studio, tic_key_u)) redo(code);

        else if (clear && keyWasPressed(code->studio, tic_key_p)) 
        {
            if (clipboardHasNewline()) 
            {
                downLine(code);
                goHome(code);
                char* pos = code->cursor.position;
                copyFromClipboard(code, false);
                code->cursor.position = pos;
            } else copyFromClipboard(code, false);
        }

        else if (shift && keyWasPressed(code->studio, tic_key_p)) 
        {
            if (clipboardHasNewline()) 
            {
                goHome(code);
                char* pos = code->cursor.position;
                copyFromClipboard(code, false);
                code->cursor.position = pos;
            } else copyFromClipboard(code, false);
        }
        else if (alt && keyWasPressed(code->studio, tic_key_f)) 
            code->altFont = !code->altFont;

        else if (alt && keyWasPressed(code->studio, tic_key_s)) 
            code->shadowText = !code->shadowText;

        else if (clear && keyWasPressed(code->studio, tic_key_1)) processViGoto(code, 1);
        else if (clear && keyWasPressed(code->studio, tic_key_2)) processViGoto(code, 2);
        else if (clear && keyWasPressed(code->studio, tic_key_3)) processViGoto(code, 3);
        else if (clear && keyWasPressed(code->studio, tic_key_4)) processViGoto(code, 4);
        else if (clear && keyWasPressed(code->studio, tic_key_5)) processViGoto(code, 5);
        else if (clear && keyWasPressed(code->studio, tic_key_6)) processViGoto(code, 6);
        else if (clear && keyWasPressed(code->studio, tic_key_7)) processViGoto(code, 7);
        else if (clear && keyWasPressed(code->studio, tic_key_8)) processViGoto(code, 8);
        else if (clear && keyWasPressed(code->studio, tic_key_9)) processViGoto(code, 9);

        else if (shift && keyWasPressed(code->studio, tic_key_slash)) setCodeMode(code, TEXT_OUTLINE_MODE);

        else if (shift && keyWasPressed(code->studio, tic_key_m)) setCodeMode(code, TEXT_BOOKMARK_MODE);
        else if (clear && keyWasPressed(code->studio, tic_key_m)) toggleBookmark(code, getLineByPos(code, code->cursor.position));
        else if (clear && keyWasPressed(code->studio, tic_key_comma)) 
        {
            if(!goPrevBookmark(code, getPrevLineByPos(code, code->cursor.position)))
                goPrevBookmark(code, code->src + strlen(code->src));
        }
        else if (clear && keyWasPressed(code->studio, tic_key_period))
        {
            if(!goNextBookmark(code, getNextLineByPos(code, code->cursor.position)))
                goNextBookmark(code, code->src);
        }

        else if (clear && keyWasPressed(code->studio, tic_key_z))
            recenterScroll(code, true); //use emacs mode because it is nice

        else if (clear && keyWasPressed(code->studio, tic_key_minus))
            commentLine(code);

        else if (clear && keyWasPressed(code->studio, tic_key_x))
            deleteChar(code);

        else if (shift && keyWasPressed(code->studio, tic_key_grave))
        {
            *code->cursor.position = toggleCase(*code->cursor.position);
            history(code);
        }

        else if (clear && keyWasPressed(code->studio, tic_key_d))
            cutToClipboard(code, false); //it seems like if there is not selection it will act on the current line by default, how nice
        else if (clear && keyWasPressed(code->studio, tic_key_y))
            copyToClipboard(code, false);

        else if (shift && keyWasPressed(code->studio, tic_key_w))
            saveProject(code->studio);
        else if (shift && keyWasPressed(code->studio, tic_key_r))
            runGame(code->studio);

        else if (clear && keyWasPressed(code->studio, tic_key_c)) 
        {
            setStudioViMode(code->studio, VI_INSERT); 
            processViChange(code);
        }

        else if (shift && keyWasPressed(code->studio, tic_key_c))
        {
            setStudioViMode(code->studio, VI_INSERT); 
            char* start = code->cursor.position;
            goEnd(code);
            deleteCode(code, start, code->cursor.position);
            code->cursor.position = start;
        }

        else if (shift && keyWasPressed(code->studio, tic_key_comma))
            doTab(code, true, false);
        else if (shift && keyWasPressed(code->studio, tic_key_period))
            doTab(code, false, true);

        else processed = false;

        if (processed) updateEditor(code);
    }
    else if (mode == VI_SELECT) {
        if (code->cursor.selection == NULL)
            code->cursor.selection = code->cursor.position;

        bool processed = true;

        if (processViPosition(code, ctrl, alt, shift));

        else if (keyWasPressed(code->studio, tic_key_escape))
            setStudioViMode(code->studio, VI_NORMAL);

        else if (clear && keyWasPressed(code->studio, tic_key_minus)) 
        {
            commentLine(code);
            setStudioViMode(code->studio, VI_NORMAL);
        }

        else if (clear && keyWasPressed(code->studio, tic_key_y)) 
        {
            copyToClipboard(code, false);
            setStudioViMode(code->studio, VI_NORMAL);
        }
        else if (clear && keyWasPressed(code->studio, tic_key_d)) 
        {
            cutToClipboard(code, false);
            setStudioViMode(code->studio, VI_NORMAL);
        }
        else if (clear && keyWasPressed(code->studio, tic_key_p)) 
        {
            copyFromClipboard(code, false);
            setStudioViMode(code->studio, VI_NORMAL);

        } else if (clear && keyWasPressed(code->studio, tic_key_c))
        {
            setStudioViMode(code->studio, VI_INSERT);
            if (code->cursor.selection != NULL)
                deleteCode(code, code->cursor.selection, code->cursor.position);
            code->cursor.position = code->cursor.selection;
            code->cursor.selection = NULL;
        }

        else if (shift && keyWasPressed(code->studio, tic_key_comma))
            doTab(code, true, false);
        else if (shift && keyWasPressed(code->studio, tic_key_period))
            doTab(code, false, true);

        else if (clear && keyWasPressed(code->studio, tic_key_slash)) 
        {
            setStudioViMode(code->studio, VI_NORMAL);
            setCodeMode(code, TEXT_FIND_MODE);
        }

        else if (clear && keyWasPressed(code->studio, tic_key_r))
        {
            setStudioViMode(code->studio, VI_NORMAL);
            setCodeMode(code, TEXT_REPLACE_MODE);
        }

        else if (shift && keyWasPressed(code->studio, tic_key_grave))
        {
            for(char* i = code->cursor.selection ;i != code->cursor.position; i++)
                *i = toggleCase(*i);
            history(code);
        }

        else processed = false;

        if (processed) updateEditor(code);

    } 
    else if (mode == VI_SEEK) 
    {

        if (keyWasPressed(code->studio, tic_key_escape))
            setStudioViMode(code->studio, VI_NORMAL);

        else if (clear || shift) {
            char sym = getKeyboardText(code->studio);
            if(sym)
            {
                seekForward(code, sym);
                if (code->cursor.selection == NULL)
                    setStudioViMode(code->studio, VI_NORMAL);
                else
                    setStudioViMode(code->studio, VI_SELECT);
            }
        }
    }
    else if (mode == VI_SEEK_BACK) 
    {

        if (keyWasPressed(code->studio, tic_key_escape))
            setStudioViMode(code->studio, VI_NORMAL);

        else if (clear || shift) {
            char sym = getKeyboardText(code->studio);
            if(sym)
            {
                seekBackward(code, sym);
                if (code->cursor.selection == NULL)
                    setStudioViMode(code->studio, VI_NORMAL);
                else
                    setStudioViMode(code->studio, VI_SELECT);
            }
        }
    }
}

static void processKeyboard(Code* code)
{
    tic_mem* tic = code->tic;

    enum KeybindMode keymode = getKeybindMode(code);

    if (keymode == KEYBIND_VI) 
    {
        processViKeyboard(code);
        return;
    }

    const bool emacsMode = keymode == KEYBIND_EMACS; 

    if (!emacsMode)
    {
        bool usedClipboard = true;
        switch(getClipboardEvent(code->studio))
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
    if(keyWasPressed(code->studio, tic_key_up)
        || keyWasPressed(code->studio, tic_key_down)
        || keyWasPressed(code->studio, tic_key_left)
        || keyWasPressed(code->studio, tic_key_right)
        || keyWasPressed(code->studio, tic_key_home)
        || keyWasPressed(code->studio, tic_key_end)
        || keyWasPressed(code->studio, tic_key_pageup)
        || keyWasPressed(code->studio, tic_key_pagedown))
    {
        changedSelection = true;
        if(!shift) code->cursor.selection = NULL;
        else if(code->cursor.selection == NULL) code->cursor.selection = code->cursor.position;
        else changedSelection = false;
    }

    bool usedKeybinding = true;

    // handle bookmarks
    if(keyWasPressed(code->studio, tic_key_f1))
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
    else if(ctrl || alt)
    {
        bool ctrlHandled = false;
        if(ctrl)
        {
            ctrlHandled = true;
            if(keyWasPressed(code->studio, tic_key_tab))        doTab(code, shift, ctrl);
            else if(keyWasPressed(code->studio, tic_key_a))     emacsMode ? startLine(code) : selectAll(code);
            else if(keyWasPressed(code->studio, tic_key_z))     undo(code);
            else if(keyWasPressed(code->studio, tic_key_y))     emacsMode ? copyFromClipboard(code, true) : redo(code);
            else if(keyWasPressed(code->studio, tic_key_w))     emacsMode ? cutToClipboard(code, true) : noop;
            else if(keyWasPressed(code->studio, tic_key_f))     emacsMode ? rightColumn(code) : setCodeMode(code, TEXT_FIND_MODE);
            else if(keyWasPressed(code->studio, tic_key_g))     emacsMode ? killSelection(code) : setCodeMode(code, TEXT_GOTO_MODE);
            else if(keyWasPressed(code->studio, tic_key_b))     emacsMode ? leftColumn(code) : setCodeMode(code, TEXT_BOOKMARK_MODE);
            else if(keyWasPressed(code->studio, tic_key_o))     setCodeMode(code, TEXT_OUTLINE_MODE);
            else if(keyWasPressed(code->studio, tic_key_n))     downLine(code);
            else if(keyWasPressed(code->studio, tic_key_p))     upLine(code);
            else if(keyWasPressed(code->studio, tic_key_e))     endLine(code);
            else if(keyWasPressed(code->studio, tic_key_d))     emacsMode ? deleteChar(code) : dupLine(code);
            else if(keyWasPressed(code->studio, tic_key_j))     newLine(code);
            else if(keyWasPressed(code->studio, tic_key_slash)) emacsMode ? undo(code) : commentLine(code);
            else if(keyWasPressed(code->studio, tic_key_home))  goCodeHome(code);
            else if(keyWasPressed(code->studio, tic_key_end))   goCodeEnd(code);
            else if(keyWasPressed(code->studio, tic_key_up))    extirpSExp(code);
            else if(keyWasPressed(code->studio, tic_key_down))  sexpify(code);
            else if(keyWasPressed(code->studio, tic_key_k))     deleteLine(code);
            else if(keyWasPressed(code->studio, tic_key_space)) emacsMode ? toggleMark(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_l))     recenterScroll(code, emacsMode);
            else if(keyWasPressed(code->studio, tic_key_v))     emacsMode ? pageDown(code) : noop;
            else if(shift && keyWasPressed(code->studio, tic_key_minus)) emacsMode ? undo(code) : noop;
            else ctrlHandled = false;
        }

        bool altHandled = false;
        if (alt)
        {
            char sym = getKeyboardText(code->studio);
            altHandled = true;
            if(keyWasPressed(code->studio, tic_key_p))          pageUp(code);
            else if(keyWasPressed(code->studio, tic_key_n))     pageDown(code);
            else if(keyWasPressed(code->studio, tic_key_v))     emacsMode ? pageUp(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_b))     leftWord(code);
            else if(keyWasPressed(code->studio, tic_key_f))     rightWord(code);
            else if(keyWasPressed(code->studio, tic_key_k))     emacsMode ? dupLine(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_d))     emacsMode ? deleteWord(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_r))     emacsMode ? extirpSExp(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_w))     emacsMode ? copyToClipboard(code, true) : noop;
            else if(keyWasPressed(code->studio, tic_key_g))     emacsMode ? setCodeMode(code, TEXT_GOTO_MODE) : noop;
            else if(keyWasPressed(code->studio, tic_key_s))     emacsMode ? setCodeMode(code, TEXT_FIND_MODE) : noop;
            else if(shift && sym && sym == '<')                 emacsMode ? goCodeHome(code) : noop;
            else if(shift && sym && sym == '>')                 emacsMode ? goCodeEnd(code) : noop;
            else if(shift && sym && sym == '(')                 emacsMode? sexpify(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_slash)) emacsMode ? redo(code) : noop;
            else if(keyWasPressed(code->studio, tic_key_semicolon)) emacsMode ? commentLine(code) : noop;
            else altHandled = false;
        }

        bool ctrlAltHandled = true;
        if(keyWasPressed(code->studio, tic_key_left))           leftWord(code);
        else if(keyWasPressed(code->studio, tic_key_right))     rightWord(code);
        else if(keyWasPressed(code->studio, tic_key_delete))    deleteWord(code);
        else if(keyWasPressed(code->studio, tic_key_backspace)) backspaceWord(code);
        else ctrlAltHandled = false;

        usedKeybinding = (ctrlHandled || altHandled || ctrlAltHandled);
    }
    else
    {
        if(keyWasPressed(code->studio, tic_key_up))             upLine(code);
        else if(keyWasPressed(code->studio, tic_key_down))      downLine(code);
        else if(keyWasPressed(code->studio, tic_key_left))      leftColumn(code);
        else if(keyWasPressed(code->studio, tic_key_right))     rightColumn(code);
        else if(keyWasPressed(code->studio, tic_key_home))      goHome(code);
        else if(keyWasPressed(code->studio, tic_key_end))       goEnd(code);
        else if(keyWasPressed(code->studio, tic_key_pageup))    pageUp(code);
        else if(keyWasPressed(code->studio, tic_key_pagedown))  pageDown(code);
        else if(keyWasPressed(code->studio, tic_key_delete))    deleteChar(code);
        else if(keyWasPressed(code->studio, tic_key_backspace)) backspaceChar(code);
        else if(enterWasPressed(code->studio))                          newLine(code);
        else if(keyWasPressed(code->studio, tic_key_tab))       doTab(code, shift, ctrl);
        else usedKeybinding = false;
    }

    if(!usedKeybinding)
    {
        if(shift && enterWasPressed(code->studio))
        {
            newLineAutoClose(code);
            usedKeybinding = true;
        }
    }

    if(changedSelection || usedKeybinding) updateEditor(code);
    else
    {
        char sym = getKeyboardText(code->studio);
        if(sym)
        {
            inputSymbol(code, sym);
            updateEditor(code);
        }
    }

}

static void processMouse(Code* code)
{
    tic_mem* tic = code->tic;

    tic_rect rect = {BOOKMARK_WIDTH, TOOLBAR_SIZE, CODE_EDITOR_WIDTH, CODE_EDITOR_HEIGHT};

    if(checkMousePos(code->studio, &rect))
    {
        bool useDrag = (code->mode == TEXT_DRAG_CODE && checkMouseDown(code->studio, &rect, tic_mouse_left)) 
            || checkMouseDown(code->studio, &rect, tic_mouse_right);
        setCursor(code->studio, code->mode == TEXT_DRAG_CODE || useDrag ? tic_cursor_hand : tic_cursor_ibeam);

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
                if(checkMouseDblClick(code->studio, &rect, tic_mouse_left))
                {
                    code->cursor.selection = leftWordPos(code);
                    code->cursor.position = rightWordPos(code);
                }
                else if(checkMouseDown(code->studio, &rect, tic_mouse_left))
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

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, true);   
    drawStatus(code);
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
    processMouse(code);

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, true);   
    drawStatus(code);
}

static void drawPopupBar(Code* code, const char* title)
{
    s32 pos = code->anim.pos;

    enum {TextX = BOOKMARK_WIDTH};


    tic_api_rect(code->tic, 0, TOOLBAR_SIZE + pos, TIC80_WIDTH, TIC_FONT_HEIGHT + 1, tic_color_grey);

    s32 textY = (TOOLBAR_SIZE + 1) + pos;

    if(code->shadowText)
        tic_api_print(code->tic, title, TextX+1, textY+1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, title, TextX, textY, tic_color_white, true, 1, code->altFont);

    if(code->shadowText)
        tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code) + 1, textY+1, tic_color_black, true, 1, code->altFont);

    tic_api_print(code->tic, code->popup.text, TextX + (s32)strlen(title) * getFontWidth(code), textY, tic_color_white, true, 1, code->altFont);

    drawCursor(code, TextX+(s32)(strlen(title) + strlen(code->popup.text)) * getFontWidth(code), textY, ' ');
}

static void updateFindCode(Code* code, char* pos)
{
    if(pos)
    {
        code->cursor.position = pos;
        code->cursor.selection = pos + strlen(code->popup.text);

        centerScroll(code);
        updateColumn(code);
        updateEditor(code);
    }
}

static void textFindTick(Code* code)
{


    if(enterWasPressed(code->studio)) setCodeMode(code, TEXT_EDIT_MODE);
    else if(keyWasPressed(code->studio, tic_key_up)
        || keyWasPressed(code->studio, tic_key_down)
        || keyWasPressed(code->studio, tic_key_left)
        || keyWasPressed(code->studio, tic_key_right))
    {
        if(*code->popup.text)
        {
            bool reverse = keyWasPressed(code->studio, tic_key_up) || keyWasPressed(code->studio, tic_key_left);
            char* (*func)(const char*, const char*, const char*) = reverse ? upStrStr : downStrStr;
            char* from = reverse ? MIN(code->cursor.position, code->cursor.selection) : MAX(code->cursor.position, code->cursor.selection);
            char* pos = func(code->src, from, code->popup.text);
            updateFindCode(code, pos);
        }
    }
    else if(keyWasPressed(code->studio, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    char sym = getKeyboardText(code->studio);
    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym , 0};
            strcat(code->popup.text, str);
            updateFindCode(code, strstr(code->src, code->popup.text));
        }
    }

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, false);
    drawPopupBar(code, "FIND:");
    drawStatus(code);
}

static void textReplaceTick(Code* code)
{

    if (enterWasPressed(code->studio)) {
        if (*code->popup.text && code->popup.offset == NULL) //still in "find" mode
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
            char* start = code->src;
            if (code->cursor.selection != NULL)
                start = code->cursor.selection;

            char* pos = downStrStr(start, start, code->popup.text);
            size_t src_length = strlen(code->src);
            while(pos != NULL)
            {
                deleteCode(code, pos, pos+strlen(code->popup.text));
                insertCode(code, pos, code->popup.offset);
                pos += strlen(code->popup.offset);
                if (pos - code->src > src_length) pos = code->src + src_length;

                pos = downStrStr(code->src, pos, code->popup.text);
                if (code->cursor.selection != NULL && pos > code->cursor.position)
                    break;
            } 
            history(code);
            updateEditor(code);
            parseSyntaxColor(code);
            setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    else if(keyWasPressed(code->studio, tic_key_backspace))
    {
        if(*code->popup.text && code->popup.offset == NULL)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
        } 
        else if (*code->popup.text) 
        {
            if (strlen(code->popup.offset) - strlen(" WITH:") > 0)
                code->popup.text[strlen(code->popup.text)-1] = '\0';
        }

    }

    char sym = getKeyboardText(code->studio);
    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym , 0};
            strcat(code->popup.text, str);
        }
    }

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, false);
    drawPopupBar(code, "REPL:");
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

    if(enterWasPressed(code->studio))
    {
        if(*code->popup.text)
            updateGotoCode(code);

        setCodeMode(code, TEXT_EDIT_MODE);
    }
    else if(keyWasPressed(code->studio, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            updateGotoCode(code);
        }
    }

    char sym = getKeyboardText(code->studio);

    if(sym)
    {
        if(strlen(code->popup.text)+1 < sizeof code->popup.text && sym >= '0' && sym <= '9')
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            updateGotoCode(code);
        }
    }

    tic_api_cls(tic, getConfig(code->studio)->theme.code.BG);

    if(code->jump.line >= 0)
        tic_api_rect(tic, 0, (code->jump.line - code->scroll.y) * (TIC_FONT_HEIGHT+1) + TOOLBAR_SIZE,
            TIC80_WIDTH, TIC_FONT_HEIGHT+2, getConfig(code->studio)->theme.code.select);

    drawCode(code, false);
    drawPopupBar(code, "GOTO:");
    drawStatus(code);
}

static void drawSidebarBar(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;
    tic_rect rect = {x, y, TIC80_WIDTH - x, TIC80_HEIGHT - y};

    if(checkMousePos(code->studio, &rect))
    {
        s32 mx = tic_api_mouse(tic).y - rect.y;
        mx /= STUDIO_TEXT_HEIGHT;
        mx += code->sidebar.scroll;

        if(mx >= 0 && mx < code->sidebar.size && code->sidebar.items[mx].pos)
        {
            setCursor(code->studio, tic_cursor_hand);

            if(checkMouseDown(code->studio, &rect, tic_mouse_left))
            {
                code->sidebar.index = mx;
                updateSidebarCode(code);
            }

            if(checkMouseClick(code->studio, &rect, tic_mouse_left))
                setCodeMode(code, TEXT_EDIT_MODE);
        }
    }

    tic_api_rect(code->tic, rect.x-1, rect.y, rect.w+1, rect.h, tic_color_grey);

    y -= code->sidebar.scroll * STUDIO_TEXT_HEIGHT - 1;

    char filter[STUDIO_TEXT_BUFFER_WIDTH];
    strncpy(filter, code->popup.text, sizeof(filter));

    if(code->sidebar.size)
    {
        tic_api_rect(code->tic, rect.x - 1, rect.y + (code->sidebar.index - code->sidebar.scroll) * STUDIO_TEXT_HEIGHT,
            rect.w + 1, TIC_FONT_HEIGHT + 2, tic_color_red);

        for(const tic_outline_item* ptr = code->sidebar.items, *end = ptr + code->sidebar.size; 
            ptr != end; ptr++, y += STUDIO_TEXT_HEIGHT)
            drawFilterMatch(code, x, y, ptr->pos, ptr->size, filter);
    }
    else
    {
        if(code->shadowText)
            tic_api_print(code->tic, "(empty)", x+1, y+1, tic_color_black, true, 1, code->altFont);

        tic_api_print(code->tic, "(empty)", x, y, tic_color_white, true, 1, code->altFont);
    }
}

static void normSidebarScroll(Code* code)
{
    code->sidebar.scroll = code->sidebar.size > TEXT_BUFFER_HEIGHT 
        ? CLAMP(code->sidebar.scroll, 0, code->sidebar.size - TEXT_BUFFER_HEIGHT) : 0;
}

static void updateSidebarIndex(Code* code, s32 value)
{
    if(code->sidebar.size == 0)
        return;

    code->sidebar.index = CLAMP(value, 0, code->sidebar.size - 1);

    if(code->sidebar.index - code->sidebar.scroll < 0)
        code->sidebar.scroll -= TEXT_BUFFER_HEIGHT;
    else if(code->sidebar.index - code->sidebar.scroll >= TEXT_BUFFER_HEIGHT)
        code->sidebar.scroll += TEXT_BUFFER_HEIGHT;

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
            enum{Scroll = 3};
            s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;
            code->sidebar.scroll += delta;
            normSidebarScroll(code);
        }
    }

    if(keyWasPressed(code->studio, tic_key_up))
        updateSidebarIndex(code, code->sidebar.index - 1);

    else if(keyWasPressed(code->studio, tic_key_down))
        updateSidebarIndex(code, code->sidebar.index + 1);

    else if(keyWasPressed(code->studio, tic_key_left) || keyWasPressed(code->studio, tic_key_pageup))
        updateSidebarIndex(code, code->sidebar.index - TEXT_BUFFER_HEIGHT);

    else if(keyWasPressed(code->studio, tic_key_right) || keyWasPressed(code->studio, tic_key_pagedown))
        updateSidebarIndex(code, code->sidebar.index + TEXT_BUFFER_HEIGHT);

    else if(keyWasPressed(code->studio, tic_key_home))
        updateSidebarIndex(code, 0);

    else if(keyWasPressed(code->studio, tic_key_end))
        updateSidebarIndex(code, code->sidebar.size - 1);

    else if(enterWasPressed(code->studio))
    {
        updateSidebarCode(code);        
        setCodeMode(code, TEXT_EDIT_MODE);
    }
}

static void textBookmarkTick(Code* code)
{
    processSidebar(code);

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, false);
    drawStatus(code);
    drawSidebarBar(code, (TIC80_WIDTH - SIDEBAR_WIDTH) + code->anim.sidebar, TIC_FONT_HEIGHT+1);
}

static void textOutlineTick(Code* code)
{
    processSidebar(code);

    if(keyWasPressed(code->studio, tic_key_backspace))
    {
        if(*code->popup.text)
        {
            code->popup.text[strlen(code->popup.text)-1] = '\0';
            setOutlineMode(code);
        }
    }

    char sym = getKeyboardText(code->studio);

    if(sym)
    {
        if(strlen(code->popup.text) + 1 < sizeof code->popup.text)
        {
            char str[] = {sym, 0};
            strcat(code->popup.text, str);
            setOutlineMode(code);
        }
    }

    tic_api_cls(code->tic, getConfig(code->studio)->theme.code.BG);

    drawCode(code, false);
    drawStatus(code);
    drawSidebarBar(code, (TIC80_WIDTH - SIDEBAR_WIDTH) + code->anim.sidebar, 2*(TIC_FONT_HEIGHT+1));
    drawPopupBar(code, "FUNC:");
}

static void drawFontButton(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(code->studio, &rect))
    {
        setCursor(code->studio, tic_cursor_hand);

        showTooltip(code->studio, "SWITCH FONT");

        over = true;

        if(checkMouseClick(code->studio, &rect, tic_mouse_left))
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
    if(checkMousePos(code->studio, &rect))
    {
        setCursor(code->studio, tic_cursor_hand);

        showTooltip(code->studio, "SHOW SHADOW");

        over = true;

        if(checkMouseClick(code->studio, &rect, tic_mouse_left))
        {
            code->shadowText = !code->shadowText;
        }
    }

    drawBitIcon(code->studio, tic_icon_shadow, x, y, over && !code->shadowText ? tic_color_grey : tic_color_light_grey);

    if(code->shadowText)
        drawBitIcon(code->studio, tic_icon_shadow2, x, y, tic_color_black);
}

static void drawRunButton(Code* code, s32 x, s32 y)
{
    tic_mem* tic = code->tic;

    enum {Size = TIC_FONT_WIDTH};
    tic_rect rect = {x, y, Size, Size};

    bool over = false;
    if(checkMousePos(code->studio, &rect))
    {
        setCursor(code->studio, tic_cursor_hand);
        showTooltip(code->studio, "RUN [ctrl+r]");
        over = true;

        if(checkMouseClick(code->studio, &rect, tic_mouse_left))
            runGame(code->studio);
    }

    drawBitIcon(code->studio, tic_icon_run, x, y, over ? tic_color_grey : tic_color_light_grey);
}

static void drawCodeToolbar(Code* code)
{
    tic_api_rect(code->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    static const struct Button {u8 icon; const char* tip;} Buttons[] = 
    {
        {tic_icon_hand, "DRAG [right mouse]"},
        {tic_icon_find, "FIND [ctrl+f]"},
        {tic_icon_goto, "GOTO [ctrl+g]"},
        {tic_icon_bookmark, "BOOKMARKS [ctrl+b]"},
        {tic_icon_outline, "OUTLINE [ctrl+o]"},
    };

    enum {Count = COUNT_OF(Buttons), Size = 7};

    for(s32 i = 0; i < Count; i++)
    {
        const struct Button* btn = &Buttons[i];
        tic_rect rect = {TIC80_WIDTH + (i - Count) * Size, 0, Size, Size};

        bool over = false;
        if(checkMousePos(code->studio, &rect))
        {
            setCursor(code->studio, tic_cursor_hand);

            showTooltip(code->studio, btn->tip);

            over = true;

            if(checkMouseClick(code->studio, &rect, tic_mouse_left))
            {
                if(code->mode == i) code->escape(code);
                else setCodeMode(code, i);
            }
        }

        bool active = i == code->mode && isIdle(code);
        if (active)
        {
            tic_api_rect(code->tic, rect.x, rect.y, Size, Size, tic_color_grey);
            drawBitIcon(code->studio, btn->icon, rect.x, rect.y + 1, tic_color_black);
        }            

        drawBitIcon(code->studio, btn->icon, rect.x, rect.y, active ? tic_color_white : (over ? tic_color_grey : tic_color_light_grey));
    }

    drawFontButton(code, TIC80_WIDTH - (Count+3) * Size, 1);
    drawShadowButton(code, TIC80_WIDTH - (Count+2) * Size, 0);
    drawRunButton(code, TIC80_WIDTH - (Count+1) * Size, 0);

    drawToolbar(code->studio, code->tic, false);
}

static void tick(Code* code)
{
    processAnim(code->anim.movie, code);

    if(code->cursor.delay)
        code->cursor.delay--;

    switch(code->mode)
    {
    case TEXT_DRAG_CODE:    textDragTick(code);     break;
    case TEXT_EDIT_MODE:    textEditTick(code);     break;
    case TEXT_FIND_MODE:    textFindTick(code);     break;
    case TEXT_REPLACE_MODE: textReplaceTick(code);   break;
    case TEXT_GOTO_MODE:    textGoToTick(code);     break;
    case TEXT_BOOKMARK_MODE:textBookmarkTick(code); break;
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
        setCodeMode(code, TEXT_EDIT_MODE);
        break;
    default:
        code->anim.movie = resetMovie(&code->anim.hide);

        code->cursor.position = code->popup.prevPos;
        code->cursor.selection = code->popup.prevSel;
        code->popup.prevSel = code->popup.prevPos = NULL;

        updateEditor(code);
    }
}

static void onStudioEvent(Code* code, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT: cutToClipboard(code, false); break;
    case TIC_TOOLBAR_COPY: copyToClipboard(code, false); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(code, false); break;
    case TIC_TOOLBAR_UNDO: undo(code); break;
    case TIC_TOOLBAR_REDO: redo(code); break;
    }
}

static void emptyDone(void* data) {}

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

void initCode(Code* code, Studio* studio)
{
    bool firstLoad = code->state == NULL;
    FREE(code->state);
    freeAnim(code);

    if(code->history) history_delete(code->history);

    tic_code* src = &getMemory(studio)->cart.code;

    *code = (Code)
    {
        .studio = studio,
        .tic = getMemory(studio),
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
        .sidebar =
        {
            .items = NULL,
            .size = 0,
            .index = 0,
            .scroll = 0,
        },
        .matchedDelim = NULL,
        .altFont = firstLoad ? getConfig(studio)->theme.code.altFont : code->altFont,
        .shadowText = getConfig(studio)->theme.code.shadow,
        .anim =
        {
            .idle = {.done = emptyDone,},

            .show = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
            {
                {-TOOLBAR_SIZE, 0, STUDIO_ANIM_TIME, &code->anim.pos, AnimEaseIn},
                {SIDEBAR_WIDTH, 0, STUDIO_ANIM_TIME, &code->anim.sidebar, AnimEaseIn},
            }),

            .hide = MOVIE_DEF(STUDIO_ANIM_TIME, setEditMode,
            {
                {0, -TOOLBAR_SIZE, STUDIO_ANIM_TIME, &code->anim.pos, AnimEaseIn},
                {0, SIDEBAR_WIDTH, STUDIO_ANIM_TIME, &code->anim.sidebar, AnimEaseIn},
            }),
        },
        .event = onStudioEvent,
        .update = update,
    };

    code->anim.movie = resetMovie(&code->anim.idle);

    packState(code);
    code->history = history_create(code->state, sizeof(CodeState) * TIC_CODE_SIZE);

    update(code);
}

void freeCode(Code* code)
{
    freeAnim(code);

    history_delete(code->history);
    free(code->state);
    free(code);
}
