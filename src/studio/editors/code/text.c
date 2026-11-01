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

#include "text.h"
#include "defines.h"
#include "ext/history.h"
#include "script.h"

#include "tic_assert.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static_assert(sizeof(CodeState) == 2, "CodeStateSize");

static inline bool islineend(char c)
{
    return c == '\n' || c == '\0';
}

static inline bool isalpha_(char c)
{
    return isalpha((u8)c) || c == '_';
}

static inline bool isdoublequote(char c)
{
    return c == '"';
}

static inline bool isopenparen_(Text* text, char c)
{
    (void)text;
    return c == '(' || c == '{' || c == '[';
}

static inline bool iscloseparen_(Text* text, char c)
{
    (void)text;
    return c == ')' || c == '}' || c == ']';
}

void text_init(Text* text, char* buffer, TextOptions options, bool (*historyEnabled)(void* host), void* host)
{
    *text = (Text){
        .src = buffer,
        .state = calloc(TIC_CODE_SIZE, sizeof(CodeState)),
        .cursor = {.position = buffer, .selection = NULL, .column = 0},
        .options = options,
        .historyEnabled = historyEnabled,
        .host = host,
    };

    // The snapshot the first commit is diffed against is the text as it
    // arrives, not the zeroed state array.
    packState(text);

    text->history = text->state ? history_create(text->state, sizeof(CodeState) * TIC_CODE_SIZE) : NULL;
}

void text_free(Text* text)
{
    if(text->history)
        history_delete(text->history);

    free(text->state);

    *text = (Text){0};
}

void packState(Text* text)
{
    const char* src = text->src;
    for(CodeState *s = text->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
    {
        s->cursor = src == text->cursor.position;
        s->sym = *src++;
    }
}

static void unpackState(Text* text, bool pos_undo)
{
    char* src = text->src;

    char* first_change = NULL;
    char* stored_pos = NULL;

    for(CodeState *s = text->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
    {

        if(first_change == NULL && *src != s->sym)
            first_change = src;

        if(s->cursor)
            stored_pos = src;

        *src++ = s->sym;
    }

    if(first_change != NULL)
    {
        //the first differing character is exactly where the change was made,
        //so the cursor goes there, as if we were about to make the change
        if(pos_undo)
            text->cursor.position = first_change;

        else
            //a state that stored no cursor leaves the position at the start
            //rather than null: the editor reads it as a pointer into the text
            text->cursor.position = stored_pos ? stored_pos : text->src;
    }
}

void text_undo(Text* text)
{
    history_undo(text->history);
    unpackState(text, true);
}

void text_redo(Text* text)
{
    history_redo(text->history);
    unpackState(text, false);
}

void history(Text* text)
{
    //if we are in insert mode we want want all changes we make to be reflected
    //in the undo/redo history only when we leave it
    if(text->historyEnabled && !text->historyEnabled(text->host))
        return;
    packState(text);
    history_add(text->history);
}

CodeState* getState(Text* text, const char* pos)
{
    return text->state + (pos - text->src);
}

char matchingDelim(const char current)
{
    char match = 0;
    switch(current)
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

const char* findMatchedDelim(Text* text, const char* current)
{
    const char* start = text->src;
    // delimiters inside comments and strings don't get to be matched!
    if(text->state[current - start].syntax == SyntaxType_COMMENT ||
       text->state[current - start].syntax == SyntaxType_STRING) return 0;

    char initial = *current;
    char seeking = matchingDelim(initial);
    if(seeking == 0) return NULL;

    s8 dir = (initial == '(' || initial == '[' || initial == '{') ? 1 : -1;

    while(*current && (start < current))
    {
        current += dir;
        // skip over anything inside a comment or string
        if(text->state[current - start].syntax == SyntaxType_COMMENT ||
           text->state[current - start].syntax == SyntaxType_STRING) continue;
        if(*current == seeking) return current;
        if(*current == initial) current = findMatchedDelim(text, current);
        if(!current) break;
    }

    return NULL;
}

static inline bool config_isalnum_(const tic_script* config, char c)
{
    if(config && config->lang_isalnum)
        return config->lang_isalnum((u8)c);
    else
        return isalnum((u8)c) || c == '_';
}

bool isalnum_(Text* text, char c)
{
    return config_isalnum_(text->options.script, c);
}

static void setCodeState(CodeState* state, u8 color, s32 start, s32 size)
{
    for(CodeState *s = state + start, *end = s + size; s != end; ++s)
        s->syntax = color;
}

static void parseCode(const tic_script* config, const char* start, CodeState* state)
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
            const char* blockStart = blockStdStringStart + 1;

            while(true)
            {
                const char* pos = strchr(blockStart, *blockStdStringStart);

                if(pos)
                {
                    if(*(pos - 1) == '\\' && *(pos - 2) != '\\')
                        blockStart = pos + 1;
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
            while(!islineend(*ptr))
                ptr++;

            setCodeState(state, SyntaxType_COMMENT, (s32)(singleCommentStart - start), (s32)(ptr - singleCommentStart));
            singleCommentStart = NULL;
            continue;
        }
        else if(wordStart)
        {
            while(!islineend(*ptr) && config_isalnum_(config, *ptr))
                ptr++;

            s32 len = (s32)(ptr - wordStart);
            bool keyword = false;
            {
                for(s32 i = 0; i < config->keywordsCount; i++)
                    if(len == (s32)strlen(config->keywords[i]) && memcmp(wordStart, config->keywords[i], len) == 0)
                    {
                        setCodeState(state, SyntaxType_KEYWORD, (s32)(wordStart - start), len);
                        keyword = true;
                        break;
                    }
            }

            if(!keyword)
            {
                static const char* const ApiKeywords[] = {
#define TIC_CALLBACK_DEF(name, ...) #name,
                    TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef TIC_CALLBACK_DEF

#define API_KEYWORD_DEF(name, ...) #name,
                        TIC_API_LIST(API_KEYWORD_DEF)
#undef API_KEYWORD_DEF
                };
                const char* const* keywords = config->api_keywordsCount > 0 ? config->api_keywords : ApiKeywords;
                const s32 apiCount = config->api_keywordsCount > 0 ? config->api_keywordsCount : COUNT_OF(ApiKeywords);

                for(s32 i = 0; i < apiCount; i++)
                    if(len == (s32)strlen(keywords[i]) && memcmp(wordStart, keywords[i], len) == 0)
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
                char ch = *ptr;

                if(isdigit((u8)ch))
                    ptr++;
                else if(numberStart[0] == '0' && (numberStart[1] == 'x' || numberStart[1] == 'X') && isxdigit((u8)numberStart[2]))
                {
                    if((ptr - numberStart < 2) || isxdigit((u8)ch))
                        ptr++;
                    else
                        break;
                }
                else if(ch == '.' || ch == 'e' || ch == 'E')
                {
                    if(isdigit((u8)ptr[1]))
                        ptr++;
                    else
                        break;
                }
                else
                    break;
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
            else if((config->stdStringStartEnd == NULL && (c == '"' || c == '\'')) || (config->stdStringStartEnd != NULL && c != 0 && strchr(config->stdStringStartEnd, c)))
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
            else if(isdigit((u8)c) || (c == '.' && isdigit((u8)ptr[1])))
            {
                numberStart = ptr;
                ptr++;
                continue;
            }
            else if(ispunct((u8)c))
                state[ptr - start].syntax = SyntaxType_SIGN;
        }

        if(!c) break;

        ptr++;
    }
}

void parseSyntaxColor(Text* text)
{
    for(CodeState *s = text->state, *end = s + TIC_CODE_SIZE; s != end; ++s)
        s->syntax = SyntaxType_FG;

    const tic_script* config = text->options.script;

    if(config)
    {
        parseCode(config, text->src, text->state);
    }
}

char* getLineByPos(Text* text, char* pos)
{
    char* buf = text->src;
    char* line = buf;

    while(buf < pos)
        if(*buf++ == '\n')
            line = buf;

    return line;
}

char* getLine(Text* text)
{
    return getLineByPos(text, text->cursor.position);
}

char* getPrevLineByPos(Text* text, char* pos)
{
    char* buf = text->src;
    char* prevLine = buf;
    char* line = buf;

    while(buf < pos)
        if(*buf++ == '\n')
        {
            prevLine = line;
            line = buf;
        }

    return prevLine;
}

char* getPrevLine(Text* text)
{
    return getPrevLineByPos(text, text->cursor.position);
}

char* getNextLineByPos(Text* text, char* pos)
{
    (void)text;
    while(*pos && *pos++ != '\n')
        ;
    return pos;
}

char* getNextLine(Text* text)
{
    return getNextLineByPos(text, text->cursor.position);
}

s32 getLineSize(const char* line)
{
    s32 size = 0;
    while(*line != '\n' && *line++)
        size++;

    return size;
}

char* getVisualPosByLine(Text* text, char* line, s32 column)
{
    if(column <= 0) return line;

    s32 visual = 0;
    char* ptr = line;

    while(*ptr && *ptr != '\n')
    {
        if(*ptr == '\t')
        {
            s32 width = getTabColumnWidth(text, visual);

            if(column < visual + width)
            {
                s32 left = column - visual;
                s32 right = visual + width - column;

                return right <= left ? ptr + 1 : ptr;
            }

            visual += width;
            ptr++;

            continue;
        }

        if(column == visual)
            return ptr;

        visual++;
        ptr++;
    }

    return ptr;
}

s32 getTabColumnWidth(Text* text, s32 column)
{
    s32 tabSize = text->options.tabSize;
    if(tabSize <= 0) tabSize = 1;

    return tabSize - column % tabSize;
}

void updateColumn(Text* text)
{
    text->cursor.column = (s32)(text->cursor.position - getLine(text));
}

s32 getLinesCount(Text* text)
{
    const char* ptr = text->src;
    s32 count = 0;

    while(*ptr)
        if(*ptr++ == '\n')
            count++;

    return count;
}

char* getPosByLine(char* ptr, s32 line)
{
    s32 y = 0;
    while(*ptr)
    {
        if(y == line) break;
        if(*ptr++ == '\n') y++;
    }

    return ptr;
}

void startLine(Text* text)
{
    char* start = text->src;
    while(text->cursor.position > start + 1)
    {
        if(islineend(*(text->cursor.position - 1)))
            break;
        --text->cursor.position;
    }
    updateColumn(text);
}

void endLine(Text* text)
{
    while(*text->cursor.position)
    {
        if(islineend(*text->cursor.position))
            break;
        text->cursor.position++;
    }
    updateColumn(text);
}

void upLine(Text* text)
{
    char* prevLine = getPrevLine(text);
    size_t prevSize = getLineSize(prevLine);
    size_t size = text->cursor.column;

    text->cursor.position = prevLine + (prevSize > size ? size : prevSize);
}

void downLine(Text* text)
{
    char* nextLine = getNextLine(text);
    size_t nextSize = getLineSize(nextLine);
    size_t size = text->cursor.column;

    text->cursor.position = nextLine + (nextSize > size ? size : nextSize);
}

void leftColumn(Text* text)
{
    char* start = text->src;

    if(text->cursor.position > start)
    {
        text->cursor.position--;
        updateColumn(text);
    }
}

void rightColumn(Text* text)
{
    if(*text->cursor.position)
    {
        text->cursor.position++;
        updateColumn(text);
    }
}

char* leftPos(Text* text, char* pos, tic_code_predicate pred)
{
    const char* start = text->src;
    if(pos > start)
    {
        if(pred(text, *pos))
            while(pos > start && pred(text, *(pos - 1)))
                pos--;
        else
            while(pos > start && !pred(text, *(pos - 1)))
                pos--;
        return pos;
    }

    return text->cursor.position;
}

char* leftWordPos(Text* text)
{
    return leftPos(text, text->cursor.position - 1, isalnum_);
}

void leftWord(Text* text)
{
    text->cursor.position = leftWordPos(text);
    updateColumn(text);
}

char* leftQuote(Text* text, char* pos)
{
    char* start = text->src;
    while(pos > start)
    {
        if(isdoublequote(*pos) && *(pos - 1) != '\\')
            return pos;
        --pos;
    }
    return start;
}

char* rightQuote(Text* text, char* pos)
{
    char* end = text->src + strlen(text->src);
    while(pos < end)
    {
        if(isdoublequote(*pos) && *(pos - 1) != '\\')
            return pos;
        ++pos;
    }
    return end;
}

char* leftSexp(Text* text, char* pos)
{
    char* start = text->src;
    int nesting = 0;

    while(pos >= start)
    {
        if(isopenparen_(text, *pos))
        {
            if(nesting == 0)
                return pos;
            else
                --nesting;
        }
        else if(iscloseparen_(text, *pos))
        {
            ++nesting;
        }
        --pos;
    }
    return start; // fallback
}

char* rightPos(Text* text, char* pos, tic_code_predicate pred)
{
    const char* end = text->src + strlen(text->src);

    if(pos < end)
    {
        if(pred(text, *pos))
            while(pos < end && pred(text, *pos))
                pos++;
        else
            while(pos < end && !pred(text, *pos))
                pos++;
    }

    return pos;
}

char* rightWordPos(Text* text)
{
    return rightPos(text, text->cursor.position, isalnum_);
}

void rightWord(Text* text)
{
    text->cursor.position = rightWordPos(text);
    updateColumn(text);
}

char* rightSexp(Text* text, char* pos)
{
    char* end = text->src + strlen(text->src);
    int nesting = 0;

    while(pos < end)
    {
        if(iscloseparen_(text, *pos))
        {
            if(nesting == 0)
                return pos;
            else
                --nesting;
        }
        else if(isopenparen_(text, *pos))
        {
            ++nesting;
        }
        ++pos;
    }
    return end; // fallback
}

void goHome(Text* text)
{
    text->cursor.position = getLine(text);

    updateColumn(text);
}

void goEnd(Text* text)
{
    char* line = getLine(text);
    text->cursor.position = line + getLineSize(line);

    updateColumn(text);
}

void goCodeHome(Text* text)
{
    text->cursor.position = text->src;

    updateColumn(text);
}

void goCodeEnd(Text* text)
{
    text->cursor.position = text->src + strlen(text->src);

    updateColumn(text);
}

void deleteCode(Text* text, char* start, char* end)
{
    s32 size = (s32)strlen(end) + 1;
    memmove(start, end, size);

    // delete code state
    memmove(getState(text, start), getState(text, end), size * sizeof(CodeState));
}

void insertCodeSize(Text* text, char* dst, const char* src, s32 size)
{
    // The cap holds here, wherever the caller's own check sits.
    if(size <= 0 || strlen(text->src) + (size_t)size > (size_t)MAX_CODE)
        return;

    s32 restSize = (s32)strlen(dst) + 1;
    memmove(dst + size, dst, restSize);
    memcpy(dst, src, size);

    // insert code state
    {
        CodeState* pos = getState(text, dst);
        memmove(pos + size, pos, restSize * sizeof(CodeState));
        memset(pos, 0, size * sizeof(CodeState));
    }
}

void insertCode(Text* text, char* dst, const char* src)
{
    insertCodeSize(text, dst, src, (s32)(strlen(src)));
}

bool replaceSelection(Text* text)
{
    char* pos = text->cursor.position;
    char* sel = text->cursor.selection;

    if(sel && sel != pos)
    {
        char* start = MIN(sel, pos);
        char* end = MAX(sel, pos);

        deleteCode(text, start, end);

        text->cursor.position = start;
        text->cursor.selection = NULL;

        history(text);

        parseSyntaxColor(text);

        return true;
    }

    return false;
}

bool text_structured(Text* text)
{
    return text->options.structuredEdit;
}

static bool structuredDeleteOverride(Text* text, char* pos)
{
    if(!text_structured(text))
        return false;

    const char* end = text->src + strlen(text->src);
    if(pos >= end - 1)
        return false;

    const bool isopen = isopenparen_(text, *pos);
    const bool isclose = iscloseparen_(text, *pos);
    if(isopen || isclose)
    {
        if((isopen && iscloseparen_(text, *(pos + 1))) || (isclose && isopenparen_(text, *(pos + 1))))
        {
            deleteCode(text, pos, pos + 2);
            history(text);
            parseSyntaxColor(text);
        }
        // if not, disallow deletion until it's empty
        return true;
    }

    if(isdoublequote(*pos))
    {
        const bool canRemoveDblQuote = isdoublequote(*(pos + 1));
        if(canRemoveDblQuote)
        {
            deleteCode(text, pos, pos + 2);
            history(text);
            parseSyntaxColor(text);
        }
        // only allow deletion when string is empty
        return true;
    }

    return false;
}

void text_delete_char(Text* text)
{
    if(!replaceSelection(text))
    {
        if(structuredDeleteOverride(text, text->cursor.position))
            return;

        deleteCode(text, text->cursor.position, text->cursor.position + 1);
        history(text);
        parseSyntaxColor(text);
    }
}

void text_backspace_char(Text* text)
{
    if(!replaceSelection(text) && text->cursor.position > text->src)
    {
        char* pos = --text->cursor.position;

        if(structuredDeleteOverride(text, pos))
            return;

        deleteCode(text, pos, pos + 1);
        history(text);
        parseSyntaxColor(text);
    }
}

void text_delete_word(Text* text)
{
    const char* end = text->src + strlen(text->src);
    char* pos = text->cursor.position;

    if(pos < end)
    {
        if(isalnum_(text, *pos))
            while(pos < end && isalnum_(text, *pos))
                pos++;
        else
            while(pos < end && !isalnum_(text, *pos))
                pos++;

        deleteCode(text, text->cursor.position, pos);

        history(text);
        parseSyntaxColor(text);
    }
}

void text_backspace_word(Text* text)
{
    const char* start = text->src;
    char* pos = text->cursor.position - 1;

    if(pos > start)
    {
        if(isalnum_(text, *pos))
            while(pos > start && isalnum_(text, *(pos - 1)))
                pos--;
        else
            while(pos > start && !isalnum_(text, *(pos - 1)))
                pos--;

        deleteCode(text, pos, text->cursor.position);

        text->cursor.position = pos;
        history(text);
        parseSyntaxColor(text);
    }
}

char* findLineEnd(Text* text, char* pos)
{
    char* lineend = pos + 1;
    const char* end = text->src + strlen(text->src);

    while(lineend < end)
    {
        if(islineend(*lineend)) break;
        if(text_structured(text) && iscloseparen_(text, *lineend)) break;
        ++lineend;
    }
    return lineend;
}

static void inputSymbolBase(Text* text, char sym)
{
    const size_t codeSize = strlen(text->src);
    const bool useStructuredEdit = text_structured(text);
    const bool isOpeningDelimiter = sym == '(' || sym == '[' || sym == '{';
    const bool pairDelimiter = (useStructuredEdit || text->options.autoDelimiters) && isOpeningDelimiter;
    const bool pairQuote = useStructuredEdit && isdoublequote(sym);

    if(codeSize + ((pairDelimiter || pairQuote) ? 2 : 1) > MAX_CODE)
        return;

    if(pairDelimiter)
    {
        insertCode(text, text->cursor.position++, (const char[]){sym, matchingDelim(sym), '\0'});
    }
    else if(pairQuote)
    {
        insertCode(text, text->cursor.position++, (const char[]){sym, sym, '\0'});
    }
    else
    {
        insertCode(text, text->cursor.position++, (const char[]){sym, '\0'});
    }

    history(text);
    updateColumn(text);
    parseSyntaxColor(text);
}

void inputSymbol(Text* text, char sym)
{
    replaceSelection(text);

    inputSymbolBase(text, sym);
}

bool text_new_line(Text* text)
{
    if(!replaceSelection(text))
    {
        char* ptr = getLine(text);
        size_t size = 0;
        char firstChar = *ptr;

        while(*ptr == '\t' || *ptr == ' ')
            ptr++, size++;

        if(ptr > text->cursor.position)
            size -= ptr - text->cursor.position;

        inputSymbol(text, '\n');

        for(size_t i = 0; i < size; i++)
            inputSymbol(text, firstChar);

        return true;
    }

    return false;
}

void selectAll(Text* text)
{
    text->cursor.selection = text->src;
    text->cursor.position = text->cursor.selection + strlen(text->cursor.selection);
}

void killSelection(Text* text)
{
    text->cursor.selection = NULL;
}

static bool useSpacesForTab(Text* text)
{
    return text->options.spacesForTab;
}

s32 insertTab(Text* text, char* line_start, char* pos)
{
    const size_t codeSize = strlen(text->src);

    // A cart may carry more code than this build's cap allows.
    if(codeSize >= MAX_CODE)
        return 0;

    const s32 room = (s32)(MAX_CODE - codeSize);

    if(useSpacesForTab(text))
    {
        s32 tab_size = text->options.tabSize;
        s32 count = 0;

        while(count < tab_size && count < room)
        {
            insertCode(text, pos++, " ");
            count++;
            if((pos - line_start) % tab_size == 0)
                break;
        }

        return count;
    }
    else
    {
        insertCode(text, pos, "\t");
        return 1;
    }
}

s32 removeTab(Text* text, char* line_start, char* pos)
{
    (void)line_start;
    if(useSpacesForTab(text))
    {
        s32 tab_size = text->options.tabSize;
        s32 count = 0;

        while(count < tab_size)
        {
            if(*pos != ' ')
                break;
            deleteCode(text, pos, pos + 1);
            count++;
        }

        return count;
    }
    else if(*pos == '\t' || *pos == ' ')
    {
        deleteCode(text, pos, pos + 1);
        return 1;
    }
    return 0;
}

bool text_do_tab(Text* text, bool shift, bool crtl)
{
    char* cursor_position = text->cursor.position;
    char* cursor_selection = text->cursor.selection;

    bool has_selection = cursor_selection && cursor_selection != cursor_position;
    bool modifier_key_pressed = shift || crtl;

    if(has_selection || modifier_key_pressed)
    {
        char* start;
        char* end;

        bool changed = false;

        if(cursor_selection)
        {
            start = MIN(cursor_selection, cursor_position);
            end = MAX(cursor_selection, cursor_position);
        }
        else
        {
            start = end = cursor_position;
        }

        char* line = start = getLineByPos(text, start);

        while(line)
        {
            if(shift)
            {
                if(*line == '\t' || *line == ' ')
                {
                    end -= removeTab(text, line, line);
                    changed = true;
                }
            }
            else
            {
                end += insertTab(text, line, line);
                changed = true;
            }

            line = getNextLineByPos(text, line);
            if(line >= end) break;
        }

        if(changed)
        {

            if(has_selection)
            {
                text->cursor.position = start;
                text->cursor.selection = end;
            }
            else if(start <= end)
                text->cursor.position = end;

            history(text);
            text_update(text);
        }

        return changed;
    }
    else
    {
        char* line = getLineByPos(text, text->cursor.position);
        text->cursor.position += insertTab(text, line, text->cursor.position);
        history(text);
        text_update(text);

        return true;
    }
}

void text_new_line_auto_close(Text* text)
{
    const char* blockEnd = text->options.script->blockEnd;
    if(blockEnd != NULL)
    {
        text_new_line(text);

        while(*blockEnd)
            inputSymbol(text, *blockEnd++);

        upLine(text);
        goEnd(text);
        text_do_tab(text, false, true);
    }
}

static char* skipIndent(char* line)
{
    while(*line == ' ' || *line == '\t')
        line++;

    return line;
}

static inline bool isBlankLine(char* line)
{
    char* text = skipIndent(line);
    return *text == '\n' || *text == '\0';
}

static inline bool isLineCommented(const char* comment, const char* line)
{
    // strncmp stops at the end of the text, where memcmp could read past it
    return strncmp(line, comment, strlen(comment)) == 0;
}

static void shiftCursorEnd(char** end, char* at, s32 delta)
{
    if(*end && *end > at)
        *end = delta < 0 && *end - at < -delta ? at : *end + delta;
}

bool text_sexpify(Text* text)
{
    if(!text_structured(text))
        return false;

    char* pos = text->cursor.position;

    // a cursor that is not in the text has nothing to s-expify
    if(!pos)
        return false;

    char* start = NULL;
    char* end = NULL;
    char* sel = text->cursor.selection;
    if(sel)
    {
        start = sel < pos ? sel : pos;
        end = sel < pos ? pos : sel;
    }
    else if(isopenparen_(text, *pos))
    {
        start = pos;
        end = rightSexp(text, pos + 1);
    }
    else if(isalnum_(text, *pos))
    {
        start = leftPos(text, pos, isalnum_);
        end = rightPos(text, pos, isalnum_);
    }

    if(start == NULL || end == NULL)
        return false;

    if(strlen(text->src) + 2 > MAX_CODE)
        return false;

    insertCode(text, start, (const char[]){'(', '\0'});
    insertCode(text, end + 1, (const char[]){')', '\0'});

    ++text->cursor.position;
    history(text);
    parseSyntaxColor(text);

    return true;
}

bool text_extirp_sexp(Text* text)
{
    if(!text_structured(text))
        return false;

    bool changed = false;

    const char* start = text->src;
    char* pos = text->cursor.position;
    const char* end = text->src + strlen(text->src);

    char* sexp_start = NULL;
    char* sexp_end = NULL;

    if(pos > start)
    {
        if(isopenparen_(text, *pos))
        {
            sexp_start = pos;
            sexp_end = rightSexp(text, pos + 1);
        }
        if(iscloseparen_(text, *pos))
        {
            sexp_start = leftSexp(text, pos - 1);
            sexp_end = pos;
        }
        if(isalnum_(text, *pos))
        {
            sexp_start = leftPos(text, pos, isalnum_);
            sexp_end = rightPos(text, pos, isalnum_) - 1;
        }
        if(sexp_start != NULL && sexp_end != NULL)
        {
            char* sexp_outer_start = leftSexp(text, sexp_start - 1);
            char* sexp_outer_end = rightSexp(text, sexp_end + 1);
            if(sexp_start > start && sexp_outer_start > start && sexp_end < end && sexp_outer_end < end)
            {
                deleteCode(text, sexp_end + 1, sexp_outer_end + 1);
                deleteCode(text, sexp_outer_start, sexp_start);
                text->cursor.position = sexp_outer_start;
                history(text);
                parseSyntaxColor(text);
                changed = true;
            }
        }
    }

    return changed;
}

void text_toggle_mark(Text* text)
{
    if(text->cursor.selection)
        text->cursor.selection = NULL;
    else
        text->cursor.selection = text->cursor.position;

    parseSyntaxColor(text);
}

void commentLine(Text* text)
{
    const char* comment = text->options.script->singleComment;
    s32 size = (s32)strlen(comment);

    char* pos = text->cursor.position;
    char* sel = text->cursor.selection;

    // a cursor that is not in the text has no line to comment
    if(!pos)
        return;

    bool selected = sel && sel != pos;

    char* start = selected ? MIN(pos, sel) : pos;
    char* end = selected ? MAX(pos, sel) : pos;

    // a selection that ends at the start of a line covers none of that line
    if(selected && end[-1] == '\n')
        end--;

    char* first = getLineByPos(text, start);
    char* last = getLineByPos(text, end);

    s32 lines = 0;
    bool allCommented = true;

    // the first and last non-blank lines, or -1 when there are none
    s32 firstText = -1;
    s32 lastText = -1;

    // comments all go in at the smallest indent of the non-blank lines, so
    // they line up; no indent is longer than the code
    s32 minIndent = TIC_CODE_SIZE;

    for(char* line = first; line <= last; line += getLineSize(line) + 1, lines++)
    {
        if(!isBlankLine(line))
        {
            char* at = skipIndent(line);

            if(firstText < 0)
                firstText = lines;

            lastText = lines;
            allCommented &= isLineCommented(comment, at);
            minIndent = MIN(minIndent, (s32)(at - line));
        }
    }

    // every non-blank line already commented makes this an uncomment
    bool commentAll = !(firstText >= 0 && allCommented);

    if(commentAll && strlen(text->src) + lines * size >= MAX_CODE)
        return;

    // each edit is made before the next line is found, so the walk follows
    // the text as it moves
    char* line = first;
    for(s32 i = 0; i < lines; i++, line += getLineSize(line) + 1)
    {
        // blank lines before the first or after the last non-blank line of a
        // selection are left alone, so an all-blank selection does nothing;
        // those between are commented along with the rest; with no selection
        // the cursor line is always toggled
        bool outside = selected && (i < firstText || i > lastText);

        if(isBlankLine(line) && (outside || !commentAll))
            continue;

        char* at = skipIndent(line);

        if(commentAll)
        {
            // a blank line shorter than the smallest indent takes the comment
            // at its end
            at = line + MIN(minIndent, (s32)(at - line));

            insertCode(text, at, comment);
            shiftCursorEnd(&text->cursor.position, at, size);
            shiftCursorEnd(&text->cursor.selection, at, size);
        }
        else
        {
            deleteCode(text, at, at + size);
            shiftCursorEnd(&text->cursor.position, at, -size);
            shiftCursorEnd(&text->cursor.selection, at, -size);
        }
    }

    parseSyntaxColor(text);
    history(text);
}

bool text_duplicate_line(Text* text)
{
    char* start = getLine(text);
    if(*start)
    {
        s32 size = getLineSize(start) + 1;

        if(strlen(text->src) + size > MAX_CODE)
            return false;

        insertCodeSize(text, start, start, size);

        text->cursor.position += size;

        history(text);
        parseSyntaxColor(text);

        return true;
    }

    return false;
}

// The tag of a bookmark comment line, and the note the toggle writes.
static const char BookmarkTag[] = "@@@";
static const char BookmarkNote[] = "bookmark";

const char* text_bookmark_note(Text* text, const char* line, s32* size)
{
    const tic_script* script = text->options.script;
    const char* comment = script ? script->singleComment : NULL;

    if(!comment || !*comment || strncmp(line, comment, strlen(comment)) != 0)
        return NULL;

    const char* tag = line + strlen(comment);

    while(*tag == ' ' || *tag == '\t')
        tag++;

    if(strncmp(tag, BookmarkTag, sizeof BookmarkTag - 1) != 0 || tag[sizeof BookmarkTag - 1] == '@')
        return NULL;

    if(getState(text, tag)->syntax != SyntaxType_COMMENT)
        return NULL;

    const char* note = tag + sizeof BookmarkTag - 1;

    while(*note == ' ' || *note == '\t')
        note++;

    const char* end = note;
    while(*end && *end != '\n')
        end++;

    while(end > note && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        end--;

    if(size)
        *size = (s32)(end - note);

    return note;
}

void toggleBookmark(Text* text, char* pos)
{
    char* line = getLineByPos(text, pos);

    if(text_bookmark_note(text, line, NULL))
    {
        // The whole line goes, line end included; a last line without one
        // takes the line end before it, so no blank line is left.
        char* next = getNextLineByPos(text, line);
        const bool hasLineEnd = next > line && next[-1] == '\n';
        char* start = line;

        if(!hasLineEnd && line > text->src && line[-1] == '\n')
        {
            start = line - 1;

            if(start > text->src && start[-1] == '\r')
                start--;
        }

        deleteCode(text, start, next);
        text->cursor.position = start;
    }
    else
    {
        const char* comment = text->options.script ? text->options.script->singleComment : NULL;

        if(!comment || !*comment)
            return;

        char prefix[64];
        const size_t commentSize = strlen(comment);
        const size_t prefixSize = commentSize + STRLEN(" @@@ ") + STRLEN(BookmarkNote) + 1;

        if(prefixSize > sizeof prefix || strlen(text->src) + prefixSize > (size_t)MAX_CODE)
            return;

        memcpy(prefix, comment, commentSize);
        memcpy(prefix + commentSize, " @@@ ", STRLEN(" @@@ "));
        memcpy(prefix + commentSize + STRLEN(" @@@ "), BookmarkNote, STRLEN(BookmarkNote));
        prefix[prefixSize - 1] = '\n';

        insertCodeSize(text, line, prefix, (s32)prefixSize);
        text->cursor.position = line + (s32)commentSize + (s32)STRLEN(" @@@ ") + (s32)STRLEN(BookmarkNote);
    }

    text->cursor.selection = NULL;

    updateColumn(text);
    history(text);
    parseSyntaxColor(text);
}

void text_update(Text* text)
{
    updateColumn(text);
    parseSyntaxColor(text);
}

void text_trim(Text* text)
{
    char* data = text->src;
    char* limit = data + MAX_CODE;

    char* src = data;
    char* dst = data;

    char* cursor = text->cursor.position;
    char* select = text->cursor.selection;

    while(src < limit && *src != '\0')
    {
        // Find the end of the line or the end of the file.
        char* end = src;
        while(end < limit && *end != '\0' && *end != '\n')
            ++end;
        bool done = end >= limit || *end == '\0';

        // Find the first trimmed space or the end of the line.
        char* trim = end;
        while(trim > src && isspace((unsigned char)trim[-1]))
            --trim;

        if(trim > src)
        {
            s32 len = (s32)(trim - src);
            memmove(dst, src, len);
            dst += len;
        }

        if(dst < limit)
        {
            *dst = '\n';
            ++dst;
        }

        // Move the cursor back by the amount of trimmed spaces before it.
        if(cursor > trim) text->cursor.position -= (cursor < end ? cursor : end) - trim;
        if(select > trim) text->cursor.selection -= (select < end ? select : end) - trim;

        // Go to the next line or exit the loop.
        src = end;
        if(done) break;
        ++src;
    }

    // Terminate the string after the trailing newline.
    while(dst[-1] == '\n')
        --dst;
    if(dst < limit) ++dst;
    // limit is inside the array: MAX_CODE leaves a byte for the terminator.
    *dst = '\0';

    // Move the cursor back if it was on one of the trimmed newlines.
    if(text->cursor.position > dst) text->cursor.position = dst;
    if(text->cursor.selection > dst) text->cursor.selection = dst;

    history(text);
    text_update(text);
}
