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
#include "studio/anim.h"
#include "studio/clipboard.h"
#include "studio/keyboard.h"
#include "studio/layout.h"
#include "studio/mouse.h"
#include "studio/vimode.h"
#include "text.h"

typedef struct Code Code;
struct tic_script;

// The editor's own view toggles: what the bar's items and the Alt+F/S keys
// switch, kept in the config by the host.
typedef enum
{
    CODE_VIEW_FONT,
    CODE_VIEW_SHADOW,
    CODE_VIEW_LINE_NUMBERS,
    CODE_VIEW_SIDEBAR,
    CODE_VIEW_STATUSBAR,

} CodeViewToggle;

// The colours the editor draws with, in the theme's order.
typedef struct
{
#define CODE_COLOR_DEF(VAR) u8 VAR;
    CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef CODE_COLOR_DEF

    u8 select;
    u8 cursor;
    bool altCaret;

} CodeTheme;

// The language and the editing options the editor is reading this frame.
typedef struct
{
    const struct tic_script* script;
    s32 tabSize;
    bool spacesForTab;
    bool structuredEdit;
    bool autoDelimiters;
    bool matchDelimiters;
    bool vi;
    bool emacs;

    // The view toggles the menubar's EDITOR menu and the editor's shortcuts
    // share; the host persists them in the config.
    bool altFont;
    bool shadowText;
    bool lineNumbers;
    bool sidebar;
    bool statusbar;
    bool minimap;

} CodeOptions;

// PRO's bytebattle, as the status bar and the caret colour see it.
typedef struct
{
    struct
    {
        s32 lower;
        s32 current;

    } limit;

    struct
    {
        bool hidetime;
        s32 left;

    } battle;

} CodeBattle;

// What the host gives the editor once: the machine, the pointer states, the
// tiles the icons come from, the shared vi mode and the actions only the
// studio can do.
typedef struct
{
    void* host;
    tic_mem* tic;
    MouseState* mouse;
    const tic_tiles* tiles;

    ViMode (*getViMode)(void* host);
    void (*setViMode)(void* host, ViMode mode);
    void (*runGame)(void* host);
    void (*saveProject)(void* host);
    void (*viewToggle)(void* host, CodeViewToggle option);
    void (*tooltip)(void* host, const char* text);

} CodeDeps;

// The frame: the bar's flags, the clipboard gesture, and the values the menubar
// may change while the editor is open. The host refreshes it every tick.
typedef struct
{
    struct
    {
        bool captured;
        bool escaped;
        bool pointerOwned;
        ClipboardEvent clipboard;

    } frame;

    CodeOptions options;
    CodeTheme theme;
    s32 editorTop;
    const CodeBattle* battle;

} CodeView;

struct Code
{
    CodeDeps deps;
    tic_mem* tic;
    const CodeView* view;

    Text text;

    bool justSwitched;

    struct
    {
        char* mouseDownPosition;
        s32 delay;
    } cursor;

    struct
    {
        s32 x;
        s32 y;

        tic_point start;

        bool active;

    } scroll;

    struct
    {
        char line[STUDIO_TEXT_BUFFER_WIDTH + sizeof "-2147483648"];
        char size[STUDIO_TEXT_BUFFER_WIDTH];
        tic_color color;
        int lineCount;
    } status;

    u32 tickCounter;

    enum
    {
        TEXT_FIND_MODE,
        TEXT_GOTO_MODE,
        TEXT_BOOKMARK_MODE,
        TEXT_OUTLINE_MODE,
        TEXT_REPLACE_MODE,
        TEXT_EDIT_MODE,
    } mode;

    struct
    {
        char text[STUDIO_TEXT_BUFFER_WIDTH - sizeof "FIND:"];
        char* offset;

        char* prevPos;
        char* prevSel;
    } popup;

    struct
    {
        s32 line;
    } jump;

    struct
    {
        tic_outline_item* items;

        s32 size;
        s32 index;
        s32 scroll;
    } sidebar;

    const char* matchedDelim;
    bool altFont;
    bool shadowText;
    bool lineNumbers;
    bool showGutter;
    bool showStatusbar;
    bool showMinimap;
    bool minimapSynced;

    struct
    {
        s32 pressScroll;
        bool dragging;

    } minimap;

    struct
    {
        s32 pos;
        s32 sidebar;
        s32 minimap;

        Movie* movie;
        Movie* minimapMovie;

        Movie idle;
        Movie show;
        Movie hide;
        Movie minimapShow;
        Movie minimapHide;

    } anim;

    void (*escape)(Code*);
    void (*update)(Code*);
};

extern const ClipboardOps CodeClipboard;

void initCode(Code*, const CodeDeps* deps);
void code_set_view(Code*, const CodeView* view);
void code_tick(Code* code);
void code_just_switched(Code* code);
void freeCode(Code*);
void codeGetPos(Code*, s32* x, s32* y);
void codeSetPos(Code*, s32 x, s32 y);

void codeFind(Code*);
void codeGoto(Code*);
void codeBookmarks(Code*);
void codeOutline(Code*);

void trimWhitespace(Code*);
