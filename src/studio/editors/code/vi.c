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

#include "vi.h"
#include "find.h"
#include "script.h"

#include <ctype.h>
#include <string.h>

static bool viPressed(CodeViHost* host, tic_key key)
{
    if(key == tic_key_escape && host->escaped)
        return false;

    return keyboard_pressed(host->tic, key);
}

static char viText(CodeViHost* host)
{
    return host->captured ? '\0' : keyboard_text(host->tic);
}

static void setEditorMode(CodeViHost* host, CodeViEdit mode)
{
    host->setEditorMode(host->host, mode);
}

static void viDeleteChar(Text* text, CodeViHost* host)
{
    text_delete_char(text);
    host->updateView(host->host);
}

static void viBackspaceChar(Text* text, CodeViHost* host)
{
    text_backspace_char(text);
    host->updateView(host->host);
}

static void viNewLine(Text* text, CodeViHost* host)
{
    if(text_new_line(text))
        host->updateView(host->host);
}

static void viDoTab(Text* text, CodeViHost* host, bool shift, bool ctrl)
{
    if(text_do_tab(text, shift, ctrl))
        host->updateView(host->host);
}

static void viUndo(Text* text, CodeViHost* host)
{
    text_undo(text);
    updateColumn(text);
    parseSyntaxColor(text);
    host->updateView(host->host);
}

static void viRedo(Text* text, CodeViHost* host)
{
    text_redo(text);
    updateColumn(text);
    parseSyntaxColor(text);
    host->updateView(host->host);
}

// The bookmark jump: the position comes from the search model, the view
// follows it.
static bool viBookmark(Text* text, CodeViHost* host, char* from, bool next)
{
    const char* pos = next ? code_bookmark_next(text, from) : code_bookmark_prev(text, from);

    if(pos)
    {
        text->cursor.position = (char*)pos;
        updateColumn(text);
        host->updateView(host->host);
        host->recenter(host->host, false);
        return true;
    }

    return false;
}

static void seekEmptyLineForward(Text* text, CodeViHost* host)
{
    char* pos = text->cursor.position;

    //goto state machine, one of the few ways to use goto well
start:
    if(*pos == '\0')
        goto end;
    else if(*pos == '\n')
    {
        pos++;
        goto check;
    }
    else
    {
        pos++;
        goto start;
    }

check:
    if(*pos == '\0')
        goto end;
    else if(*pos == '\t')
    {
        pos++;
        goto check;
    }
    else if(*pos == ' ')
    {
        pos++;
        goto check;
    }
    else if(*pos == '\n')
        goto end;
    else
        goto start;

end:
    text->cursor.position = pos;
    updateColumn(text);
    host->updateView(host->host);
}

static void seekEmptyLineBackward(Text* text, CodeViHost* host)
{
    char* pos = text->cursor.position;

    if(pos == text->src)
        return;
    else
        pos--; //need to start one behind so we don't match the one we are on

    //goto state machine, one of the few ways to use goto well
start:
    if(pos == text->src)
        goto end;
    else if(*pos == '\n')
    {
        pos--;
        goto check;
    }
    else
    {
        pos--;
        goto start;
    }

check:
    if(pos == text->src)
        goto end;
    else if(*pos == '\t')
    {
        pos--;
        goto check;
    }
    else if(*pos == ' ')
    {
        pos--;
        goto check;
    }
    else if(*pos == '\n')
    {
        pos++;
        goto end;
    }
    else
        goto start;

end:
    text->cursor.position = pos;
    updateColumn(text);
    host->updateView(host->host);
}

static void seekForward(Text* text, CodeViHost* host, char sought)
{
    char* start = text->cursor.position;
    text->cursor.position++;
    while(
        *text->cursor.position != sought && *text->cursor.position != '\n' && *text->cursor.position != '\0')
        text->cursor.position++;
    if(*text->cursor.position == '\n' || *text->cursor.position == '\0')
        text->cursor.position = start;

    updateColumn(text);
    host->updateView(host->host);
}

static void seekBackward(Text* text, CodeViHost* host, char sought)
{
    char* start = text->cursor.position;
    text->cursor.position--;
    while(
        text->cursor.position > text->src && *text->cursor.position != sought && *text->cursor.position != '\n')
        text->cursor.position--;

    if(text->cursor.position <= text->src || *text->cursor.position == '\n')
        text->cursor.position = start;

    updateColumn(text);
    host->updateView(host->host);
}

static void findNextPopupText(Text* text, CodeViHost* host)
{
    if(*host->search)
    {
        const char* pos = code_search_down(text->src, text->cursor.position, host->search);
        if(pos == text->cursor.position)
        {
            pos += strlen(host->search);
            pos = code_search_down(text->src, pos, host->search);
        }
        if(pos == NULL)
            pos = code_search_down(text->src, text->src, host->search);
        if(pos != NULL)
        {
            text->cursor.position = (char*)pos;
            updateColumn(text);
        }
    }
}

static bool processViPosition(Text* text, CodeViHost* host, bool ctrl, bool alt, bool shift)
{
    bool clear = !(shift || ctrl || alt);

    bool processed = true;
    if(clear && viPressed(host, tic_key_k))
        upLine(text);
    else if(clear && viPressed(host, tic_key_j))
        downLine(text);
    else if(clear && viPressed(host, tic_key_h))
        leftColumn(text);
    else if(clear && viPressed(host, tic_key_l))
        rightColumn(text);

    //no need to be elitist, arrow keys can work too
    else if(viPressed(host, tic_key_up))
        upLine(text);
    else if(viPressed(host, tic_key_down))
        downLine(text);
    else if(viPressed(host, tic_key_left))
        leftColumn(text);
    else if(viPressed(host, tic_key_right))
        rightColumn(text);

    else if(clear && viPressed(host, tic_key_g))
        goCodeHome(text);
    else if(shift && viPressed(host, tic_key_g))
        goCodeEnd(text);

    else if(viPressed(host, tic_key_home))
        goHome(text);
    else if(clear && viPressed(host, tic_key_0))
        goHome(text);
    else if(viPressed(host, tic_key_end))
        goEnd(text);
    else if(shift && viPressed(host, tic_key_4))
        goEnd(text);

    else if(viPressed(host, tic_key_pageup))
        host->page(host->host, -1, false);
    else if(ctrl && viPressed(host, tic_key_u))
        host->page(host->host, -1, false);
    else if(viPressed(host, tic_key_pagedown))
        host->page(host->host, 1, false);
    else if(ctrl && viPressed(host, tic_key_d))
        host->page(host->host, 1, false);

    else if(clear && viPressed(host, tic_key_b))
        leftWord(text);
    else if(clear && viPressed(host, tic_key_w))
        rightWord(text);

    else if(shift && viPressed(host, tic_key_6))
    {
        goHome(text);
        while(*text->cursor.position == ' ' || *text->cursor.position == '\t')
            text->cursor.position++;
    }

    else if(shift && viPressed(host, tic_key_j))
        host->page(host->host, 1, true);

    else if(shift && viPressed(host, tic_key_k))
        host->page(host->host, -1, true);

    else if(shift && viPressed(host, tic_key_leftbracket))
        seekEmptyLineBackward(text, host);

    else if(shift && viPressed(host, tic_key_rightbracket))
        seekEmptyLineForward(text, host);

    else if(shift && viPressed(host, tic_key_5))
    {
        const char* pos = findMatchedDelim(text, text->cursor.position);
        if(pos != NULL)
        {
            text->cursor.position = (char*)pos;
            updateColumn(text);
            host->updateView(host->host);
        }
    }

    else if(clear && viPressed(host, tic_key_f))
        host->setViMode(host->host, VI_SEEK);

    else if(shift && viPressed(host, tic_key_f))
        host->setViMode(host->host, VI_SEEK_BACK);

    else if(clear && viPressed(host, tic_key_semicolon))
        seekForward(text, host, *text->cursor.position);

    else if(shift && viPressed(host, tic_key_semicolon))
        seekBackward(text, host, *text->cursor.position);

    else
        processed = false;

    return processed;
}

static void processViGoto(Text* text, CodeViHost* host, s32 initial)
{
    (void)text;

    setEditorMode(host, CODE_VI_EDIT_GOTO);
    host->search[0] = (char)('0' + initial);
    host->search[1] = '\0';
    host->gotoUpdate(host->host);
}

static char* findStringStart(Text* text, char* pos)
{
    char sentinel = *pos;

    char* target = pos;            //we will scan to the given position
    pos = getLineByPos(text, pos); //from the beginning of the line
    char* start = NULL;

    //goto state machine!
start:
    if(pos == target)
        goto end;
    else if(*pos == '\\')
    {
        pos++;
        goto escape;
    }
    else if(*pos == sentinel)
    {
        start = start ? NULL : pos;
        pos++;
        goto start;
    }
    else
    {
        pos++;
        goto start;
    }
escape:
    if(pos == target)
        goto end;
    else
    {
        pos++;
        goto start;
    }
end:
    return start;
}

static char* findStringEnd(char* pos)
{
    char sentinel = *pos; //can handle single or double quotes
                          //or anything else I guess
    if(*pos == '\0')
        goto end;
    else
        pos++; //move past the sentinel so we don't immediately detect the end

    //goto state machine!
start:
    if(*pos == '\0' || *pos == '\n' || *pos == sentinel)
        goto end;
    else if(*pos == '\\')
    {
        pos++;
        goto escape;
    }
    else
    {
        pos++;
        goto start;
    }
escape:
    if(*pos == '\0' || *pos == '\n')
        goto end;
    else
    {
        pos++;
        goto start;
    }
end:
    return pos;
}

static char* findFunctionDefinition(Text* text, char* name, size_t length)
{
    char* result = NULL;

    const tic_script* config = text->options.script;

    if(config && config->getOutline)
    {
        s32 osize = 0;
        const tic_outline_item* items = config->getOutline(text->src, &osize);

        if(items)
        {
            for(s32 i = 0; i < osize; i++)
            {
                const tic_outline_item* it = items + i;

                if(text->state[it->pos - text->src].syntax == SyntaxType_COMMENT)
                    continue;
                if(strncmp(name, it->pos, length) == 0)
                {
                    result = (char*)it->pos;
                    break;
                }
            }
        }
    }

    return result;
}

static void processViChange(Text* text, CodeViHost* host)
{
    //if on a delimiter change the contents of the delimiter
    if(matchingDelim(*text->cursor.position))
    {
        const char* match = findMatchedDelim(text, text->cursor.position);
        if(match == 0)
            viDeleteChar(text, host);
        else
        {
            if(text->cursor.position > match)
            {
                char* temp = text->cursor.position;
                text->cursor.position = (char*)match;
                match = temp;
            }
            deleteCode(text, text->cursor.position + 1, (char*)match);
            text->cursor.position++;
        }
    }
    //if on a quotation seek to change within the quotation
    else if(*text->cursor.position == '"' || *text->cursor.position == '\'')
    {
        char* start = findStringStart(text, text->cursor.position);
        if(start == NULL) start = text->cursor.position;
        char* end = findStringEnd(start);
        start++; //advance one to leave the initial quote alone
        deleteCode(text, start, end);
        text->cursor.position = start;
    }
    else if(!isalnum_(text, *text->cursor.position))
        viDeleteChar(text, host);
    else //change the word under the cursor
    {
        //only call left word if we are not already on the word border
        if(
            text->cursor.position > text->src && isalnum_(text, *(text->cursor.position - 1)))
            leftWord(text);
        text_delete_word(text);
    }
}

static char toggleCase(char c)
{
    if(isupper((u8)c))
        return (char)(tolower((u8)c));
    else if(islower((u8)c))
        return (char)(toupper((u8)c));
    else
        return c;
}

void code_vi_tick(Text* text, CodeViHost* host)
{

    tic_mem* tic = host->tic;
    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool alt = tic_api_key(tic, tic_key_alt);
    bool clear = !(shift || ctrl || alt);

    ViMode mode = host->getViMode(host->host);

    //keep these the same to be consistent with the other editors
    bool usedClipboard = true;
    switch(host->clipboardEvent)
    {
        case TIC_CLIPBOARD_CUT: host->clipboard(host->host, CODE_VI_CUT); break;
        case TIC_CLIPBOARD_COPY: host->clipboard(host->host, CODE_VI_COPY); break;
        case TIC_CLIPBOARD_PASTE: host->clipboard(host->host, CODE_VI_PASTE); break;
        default: usedClipboard = false; break;
    }

    if(usedClipboard)
    {
        if(mode != VI_INSERT)
            host->setViMode(host->host, VI_NORMAL);
        host->updateView(host->host);
        return;
    }

    if(mode == VI_INSERT)
    {
        if(viPressed(host, tic_key_escape))
        {
            host->setViMode(host->host, VI_NORMAL);
            history(text); //needs to come after the mode switch or won't be honored
        }

        else if(viPressed(host, tic_key_backspace))
            viBackspaceChar(text, host);

        else if(viPressed(host, tic_key_tab))
            viDoTab(text, host, shift, ctrl);

        else if(keyboard_enter(host->tic, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            viNewLine(text, host);

        else if(clear || shift)
        {
            char sym = viText(host);
            if(sym)
            {
                inputSymbol(text, sym);
                host->updateView(host->host);
            }
        }
    }
    else if(mode == VI_NORMAL)
    {
        bool processed = true;

        text->cursor.selection = NULL;

        if(processViPosition(text, host, ctrl, alt, shift))
            ;

        else if(clear && viPressed(host, tic_key_i))
            host->setViMode(host->host, VI_INSERT);

        else if(clear && viPressed(host, tic_key_a))
        {
            host->setViMode(host->host, VI_INSERT);
            rightColumn(text);
        }
        else if(clear && viPressed(host, tic_key_o))
        {
            host->setViMode(host->host, VI_INSERT);
            goEnd(text);
            viNewLine(text, host);
        }

        else if(shift && viPressed(host, tic_key_o))
        {
            host->setViMode(host->host, VI_INSERT);
            goHome(text);
            viNewLine(text, host);
            upLine(text);
        }
        else if(clear && viPressed(host, tic_key_space))
        {
            size_t col = text->cursor.column;
            goEnd(text);
            viNewLine(text, host);
            text->cursor.column = (s32)col;
            upLine(text);
        }
        else if(shift && viPressed(host, tic_key_space))
        {
            size_t col = text->cursor.column;
            goHome(text);
            viNewLine(text, host);
            text->cursor.position += col;
            updateColumn(text);
        }

        else if(clear && viPressed(host, tic_key_v))
            host->setViMode(host->host, VI_SELECT);

        else if(clear && viPressed(host, tic_key_slash))
            setEditorMode(host, CODE_VI_EDIT_FIND);

        else if(clear && viPressed(host, tic_key_n))
            findNextPopupText(text, host);

        else if(shift && viPressed(host, tic_key_3))
        {
            char* word_start = leftPos(text, text->cursor.position, isalnum_);
            char* word_end = rightPos(text, text->cursor.position, isalnum_);
            size_t size = word_end - word_start;
            if(size + 1 < (size_t)host->searchSize)
            {
                memcpy(host->search, word_start, size);
                host->search[size] = 0;
            }
            findNextPopupText(text, host);
        }
        else if(shift && viPressed(host, tic_key_n))
        {
            if(*host->search)
            {
                const char* pos = code_search_up(text->src, text->cursor.position, host->search);
                if(pos == NULL)
                    pos = code_search_up(text->src, text->src + strlen(text->src), host->search);
                if(pos != NULL)
                {
                    text->cursor.position = (char*)pos;
                    updateColumn(text);
                }
            }
        }

        else if(clear && viPressed(host, tic_key_leftbracket))
        {
            char* word_start = leftPos(text, text->cursor.position, isalnum_);
            char* word_end = rightPos(text, text->cursor.position, isalnum_);
            size_t size = word_end - word_start;

            char* def = findFunctionDefinition(text, word_start, size);
            if(def != NULL)
            {
                text->cursor.position = def;
                updateColumn(text);
            }
        }

        else if(clear && viPressed(host, tic_key_r))
            setEditorMode(host, CODE_VI_EDIT_REPLACE);

        else if(clear && viPressed(host, tic_key_u))
            viUndo(text, host);
        else if(shift && viPressed(host, tic_key_u))
            viRedo(text, host);

        else if(clear && viPressed(host, tic_key_p))
        {
            if(host->clipboardNewline(host->host))
            {
                downLine(text);
                goHome(text);
                char* pos = text->cursor.position;
                host->clipboard(host->host, CODE_VI_PASTE);
                text->cursor.position = pos;
            }
            else
                host->clipboard(host->host, CODE_VI_PASTE);
        }

        else if(shift && viPressed(host, tic_key_p))
        {
            if(host->clipboardNewline(host->host))
            {
                goHome(text);
                char* pos = text->cursor.position;
                host->clipboard(host->host, CODE_VI_PASTE);
                text->cursor.position = pos;
            }
            else
                host->clipboard(host->host, CODE_VI_PASTE);
        }
        else if(alt && viPressed(host, tic_key_f))
            host->viewToggle(host->host, true);

        else if(alt && viPressed(host, tic_key_s))
            host->viewToggle(host->host, false);

        else if(clear && viPressed(host, tic_key_1))
            processViGoto(text, host, 1);
        else if(clear && viPressed(host, tic_key_2))
            processViGoto(text, host, 2);
        else if(clear && viPressed(host, tic_key_3))
            processViGoto(text, host, 3);
        else if(clear && viPressed(host, tic_key_4))
            processViGoto(text, host, 4);
        else if(clear && viPressed(host, tic_key_5))
            processViGoto(text, host, 5);
        else if(clear && viPressed(host, tic_key_6))
            processViGoto(text, host, 6);
        else if(clear && viPressed(host, tic_key_7))
            processViGoto(text, host, 7);
        else if(clear && viPressed(host, tic_key_8))
            processViGoto(text, host, 8);
        else if(clear && viPressed(host, tic_key_9))
            processViGoto(text, host, 9);

        else if(shift && viPressed(host, tic_key_slash))
            setEditorMode(host, CODE_VI_EDIT_OUTLINE);

        else if(shift && viPressed(host, tic_key_m))
            setEditorMode(host, CODE_VI_EDIT_BOOKMARK);
        else if(clear && viPressed(host, tic_key_m))
            toggleBookmark(text, getLineByPos(text, text->cursor.position));
        else if(clear && viPressed(host, tic_key_comma))
        {
            if(!viBookmark(text, host, getPrevLineByPos(text, text->cursor.position), false))
                viBookmark(text, host, text->src + strlen(text->src), false);
        }
        else if(clear && viPressed(host, tic_key_period))
        {
            if(!viBookmark(text, host, getNextLineByPos(text, text->cursor.position), true))
                viBookmark(text, host, text->src, true);
        }

        else if(clear && viPressed(host, tic_key_z))
            host->recenter(host->host, true); //use emacs mode because it is nice

        else if(clear && viPressed(host, tic_key_minus))
            commentLine(text);

        else if(clear && viPressed(host, tic_key_x))
            viDeleteChar(text, host);

        else if(shift && viPressed(host, tic_key_grave))
        {
            *text->cursor.position = toggleCase(*text->cursor.position);
            history(text);
        }

        else if(clear && viPressed(host, tic_key_d))
            host->clipboard(host->host, CODE_VI_CUT); //it seems like if there is not selection it will act on the current line by default, how nice
        else if(clear && viPressed(host, tic_key_y))
            host->clipboard(host->host, CODE_VI_COPY);

        else if(shift && viPressed(host, tic_key_w))
            host->saveProject(host->host);
        else if(shift && viPressed(host, tic_key_r))
            host->runGame(host->host);

        else if(clear && viPressed(host, tic_key_c))
        {
            host->setViMode(host->host, VI_INSERT);
            processViChange(text, host);
        }

        else if(shift && viPressed(host, tic_key_c))
        {
            host->setViMode(host->host, VI_INSERT);
            char* start = text->cursor.position;
            goEnd(text);
            deleteCode(text, start, text->cursor.position);
            text->cursor.position = start;
        }

        else if(shift && viPressed(host, tic_key_comma))
            viDoTab(text, host, true, false);
        else if(shift && viPressed(host, tic_key_period))
            viDoTab(text, host, false, true);

        else
            processed = false;

        if(processed) host->updateView(host->host);
    }
    else if(mode == VI_SELECT)
    {
        if(text->cursor.selection == NULL)
            text->cursor.selection = text->cursor.position;

        bool processed = true;

        if(processViPosition(text, host, ctrl, alt, shift))
            ;

        else if(viPressed(host, tic_key_escape))
            host->setViMode(host->host, VI_NORMAL);

        else if(clear && viPressed(host, tic_key_minus))
        {
            commentLine(text);
            host->setViMode(host->host, VI_NORMAL);
        }

        else if(clear && viPressed(host, tic_key_y))
        {
            host->clipboard(host->host, CODE_VI_COPY);
            host->setViMode(host->host, VI_NORMAL);
        }
        else if(clear && viPressed(host, tic_key_d))
        {
            host->clipboard(host->host, CODE_VI_CUT);
            host->setViMode(host->host, VI_NORMAL);
        }
        else if(clear && viPressed(host, tic_key_p))
        {
            host->clipboard(host->host, CODE_VI_PASTE);
            host->setViMode(host->host, VI_NORMAL);
        }
        else if(clear && viPressed(host, tic_key_c))
        {
            host->setViMode(host->host, VI_INSERT);
            if(text->cursor.selection != NULL)
                deleteCode(text, text->cursor.selection, text->cursor.position);
            text->cursor.position = text->cursor.selection;
            text->cursor.selection = NULL;
        }

        else if(shift && viPressed(host, tic_key_comma))
            viDoTab(text, host, true, false);
        else if(shift && viPressed(host, tic_key_period))
            viDoTab(text, host, false, true);

        else if(clear && viPressed(host, tic_key_slash))
        {
            host->setViMode(host->host, VI_NORMAL);
            setEditorMode(host, CODE_VI_EDIT_FIND);
        }

        else if(clear && viPressed(host, tic_key_r))
        {
            host->setViMode(host->host, VI_NORMAL);
            setEditorMode(host, CODE_VI_EDIT_REPLACE);
        }

        else if(shift && viPressed(host, tic_key_grave))
        {
            for(char* i = text->cursor.selection; i != text->cursor.position; i++)
                *i = toggleCase(*i);
            history(text);
        }

        else
            processed = false;

        if(processed) host->updateView(host->host);
    }
    else if(mode == VI_SEEK)
    {

        if(viPressed(host, tic_key_escape))
            host->setViMode(host->host, VI_NORMAL);

        else if(clear || shift)
        {
            char sym = viText(host);
            if(sym)
            {
                seekForward(text, host, sym);
                if(text->cursor.selection == NULL)
                    host->setViMode(host->host, VI_NORMAL);
                else
                    host->setViMode(host->host, VI_SELECT);
            }
        }
    }
    else if(mode == VI_SEEK_BACK)
    {

        if(viPressed(host, tic_key_escape))
            host->setViMode(host->host, VI_NORMAL);

        else if(clear || shift)
        {
            char sym = viText(host);
            if(sym)
            {
                seekBackward(text, host, sym);
                if(text->cursor.selection == NULL)
                    host->setViMode(host->host, VI_NORMAL);
                else
                    host->setViMode(host->host, VI_SELECT);
            }
        }
    }
}
