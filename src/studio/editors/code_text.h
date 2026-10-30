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

#include "tic.h"

#include <stdbool.h>
#include <stddef.h>

struct tic_script;
struct History;

// The editor's text model: the cart's code buffer, the per-byte syntax and
// bookmark state, the caret and the undo history. It links and is tested with
// the history module alone; the studio, the keyboard, the mouse and the screen
// stay in the editor that embeds it.

typedef struct CodeState
{
    u8 syntax : 3;
    u8 bookmark : 1;
    u8 cursor : 1;
    u8 temp : 3;
    char sym;

} CodeState;

typedef struct Text Text;

typedef bool (*tic_code_predicate)(Text* text, char c);

// The syntax states, in the theme's order: one list feeds the state and the
// config's colours.
enum
{
#define CODE_COLOR_DEF(VAR) SyntaxType_##VAR,
    CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef CODE_COLOR_DEF
};

// The language and the editing options, as the editor reads them from the
// config: values, so the module never reaches for the studio.
typedef struct
{
    const struct tic_script* script;
    s32 tabSize;
    bool spacesForTab;   // TAB_SPACE, or TAB_AUTO on the languages that use them
    bool structuredEdit; // KEYBIND_EMACS on a language with structured editing
    bool autoDelimiters;

} TextOptions;

struct Text
{
    char* src;
    CodeState* state;
    struct History* history;

    struct
    {
        char* position;
        char* selection;
        s32 column;

    } cursor;

    TextOptions options;

    // Asked before a commit: a vi insert session holds its edits back until it
    // ends. The host is the editor.
    bool (*historyEnabled)(void* host);
    void* host;
};

// The terminator needs a byte of the same array.
#if defined(TIC80_PRO)
#define MAX_CODE (sizeof(tic_code) - 1)
#else
#define MAX_CODE TIC_BANK_SIZE
#endif

void text_init(Text* text, char* buffer, TextOptions options, bool (*historyEnabled)(void* host), void* host);
void text_free(Text* text);

// The buffer and its state.
void packState(Text* text);
CodeState* getState(Text* text, const char* pos);
void history(Text* text);
void text_undo(Text* text);
void text_redo(Text* text);
void text_update(Text* text);
void toggleBookmark(Text* text, char* pos);

void deleteCode(Text* text, char* start, char* end);
void insertCodeSize(Text* text, char* dst, const char* src, s32 size);
void insertCode(Text* text, char* dst, const char* src);
bool replaceSelection(Text* text);
void text_trim(Text* text);

void parseSyntaxColor(Text* text);
char matchingDelim(char current);
const char* findMatchedDelim(Text* text, const char* current);
bool isalnum_(Text* text, char c);

// Navigation.
char* getLineByPos(Text* text, char* pos);
char* getLine(Text* text);
char* getPrevLineByPos(Text* text, char* pos);
char* getPrevLine(Text* text);
char* getNextLineByPos(Text* text, char* pos);
char* getNextLine(Text* text);
s32 getLineSize(const char* line);
s32 getLinesCount(Text* text);
char* getPosByLine(char* ptr, s32 line);
s32 getTabColumnWidth(Text* text, s32 column);
char* getVisualPosByLine(Text* text, char* line, s32 column);
void updateColumn(Text* text);

void startLine(Text* text);
void endLine(Text* text);
void upLine(Text* text);
void downLine(Text* text);
void leftColumn(Text* text);
void rightColumn(Text* text);
char* leftPos(Text* text, char* pos, tic_code_predicate pred);
char* leftWordPos(Text* text);
void leftWord(Text* text);
char* leftQuote(Text* text, char* pos);
char* rightQuote(Text* text, char* pos);
char* leftSexp(Text* text, char* pos);
char* rightPos(Text* text, char* pos, tic_code_predicate pred);
char* rightWordPos(Text* text);
void rightWord(Text* text);
char* rightSexp(Text* text, char* pos);
void goHome(Text* text);
void goEnd(Text* text);
void goCodeHome(Text* text);
void goCodeEnd(Text* text);

// Editing.
void text_delete_char(Text* text);
void text_backspace_char(Text* text);
void text_delete_word(Text* text);
void text_backspace_word(Text* text);
char* findLineEnd(Text* text, char* pos);
bool text_structured(Text* text);

void inputSymbol(Text* text, char sym);
bool text_new_line(Text* text);
void selectAll(Text* text);
void killSelection(Text* text);

s32 insertTab(Text* text, char* line_start, char* pos);
s32 removeTab(Text* text, char* line_start, char* pos);
bool text_do_tab(Text* text, bool shift, bool ctrl);
void text_new_line_auto_close(Text* text);

void commentLine(Text* text);
bool text_duplicate_line(Text* text);
bool text_sexpify(Text* text);
bool text_extirp_sexp(Text* text);
void text_toggle_mark(Text* text);
