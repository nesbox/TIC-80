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
#include "api.h"
#include "cart.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/mouse.h"
#include "studio/system.h"

typedef struct Console Console;
typedef struct CommandDesc CommandDesc;
typedef struct ConsoleAnswer ConsoleAnswer;

// What the console is given: the machine it draws on and reads its keys from,
// the filesystem it loads carts with, the network it checks, the config values
// it reads, the pointer states the host ticks, and how the launch went.
typedef struct
{
    tic_mem*            tic;
    struct tic_fs*      fs;
    struct tic_net*     net;
    const StudioConfig* config;
    MouseState*         mouse;

    struct
    {
        char*       cmd;        // --cmd: the commands to run at start, split in place
        bool        cli;        // --cli: console output only
        bool        skip;       // --skip: no startup delay
        bool        keepcmd;    // --keepcmd: run the commands again after every run

    } args;

    bool boot;                  // the launch came with a cart, still to be played

    struct ConsoleView* view;   // the host's, refreshed before every tick

} ConsoleDeps;

// What the host holds and the console reads: refreshed by the host before it
// ticks the console, since the console may not call into the studio.
typedef struct ConsoleView
{
    bool inFront;       // the console is the screen the host is showing
    bool cartChanged;   // the cart differs from what the host last saved

} ConsoleView;

// What the console wants done: it reports, and the host performs it and answers
// through console_answer(). A kind without an answer is a move — the change is
// the whole effect.
typedef enum
{
    CONSOLE_ASK_NONE,

    CONSOLE_ASK_MENU,           // the pause menu over the run
    CONSOLE_ASK_SURF,           // the browser
    CONSOLE_ASK_CODE,           // the code editor
    CONSOLE_ASK_RUN,            // run the cart, as the studio asks
    CONSOLE_ASK_PLAY,           // run the cart the launch came with
    CONSOLE_ASK_RESUME,         // back to the cart that is running
    CONSOLE_ASK_EXIT,           // leave the studio
    CONSOLE_ASK_CONFIG_RESET,   // the configuration back to its defaults
    CONSOLE_ASK_CONFIRM,        // these rows: yes or no
    CONSOLE_ASK_CART_LOADED,    // take the loaded cart's identity, or refresh
    CONSOLE_ASK_CART_SAVE,      // save the cart under this name
    CONSOLE_ASK_CART_FETCH,     // find and load the cart with this hash
    CONSOLE_ASK_EXPORT_SFX,     // write sample `index` to `name`
    CONSOLE_ASK_EXPORT_MUSIC,   // write track `index` of bank `bank` to `name`

} ConsoleAskKind;

typedef struct
{
    ConsoleAskKind kind;

    const char** rows;      // CONFIRM
    s32         rowsCount;  // CONFIRM

    // CART_LOADED: the name the cart takes now, if it takes one. `clear` is for
    // a cart that has none any more, and both unset only refreshes the host's
    // idea of the cart, which is what replacing one section asks for.
    const char* name;
    bool        clear;

    const char* hash;       // CART_FETCH
    const char* section;    // CART_FETCH

    s32 index;              // EXPORT_SFX / EXPORT_MUSIC
    s32 bank;               // EXPORT_MUSIC

} ConsoleAsk;

// What the host answers with; which fields a kind sets is noted on the kind.
struct ConsoleAnswer
{
    bool yes;               // CONFIRM
    CartSaveResult save;    // CART_SAVE
    const char* name;       // the cart's name after the ask: LOADED, SAVE, FETCH
    const char* error;      // the export's text, NULL when it worked

};

struct Console
{
    // what it was given
    tic_mem*            tic;
    struct tic_fs*      fs;
    struct tic_net*     net;
    const StudioConfig* config;
    MouseState*         mouse;

    struct
    {
        const char* cmd;
        bool        cli;
        bool        skip;
        bool        keepcmd;

    } args;

    // what the host last said, and what the console last asked
    const struct ConsoleView* view;
    ConsoleAsk ask;
    bool        waiting;    // the host has taken the ask and owes an answer

    // The flow that asked, picking itself up when the answer comes; CART_FETCH
    // keeps its caller's callback and data beside it.
    void (*onAnswer)(Console*, const ConsoleAnswer*);
    fs_done_callback fetchCallback;
    void* fetchData;

    struct
    {
        void (*yes)(Console*, const void* data);
        const void* data;

    } confirm;

    struct
    {
        tic_point pos;
        s32 delay;
    } cursor;

    struct
    {
        s32 pos;
        s32 start;

        bool active;
    } scroll;

    struct
    {
        const char* start;
        const char* end;
        bool active;
    } select;

    char* text;
    u8* color;
    char exportName[TICNAME_MAX];   // the export's file, until it is answered

    struct
    {
        char* text;
        size_t pos;
    } input;

    struct
    {
        s32 index;
        s32 size;
        char** items;
    } history;

    u32 tickCounter;

    bool active;
    bool boot;

    struct
    {
        s32 count;
        s32 current;
        char** items;
    } commands;

    CommandDesc* desc;
};

Console* console_create(const ConsoleDeps* deps);
void console_free(Console* console);

// The host's way in.
void console_tick(Console* console);
void console_answer(Console* console, const ConsoleAnswer* answer);
void console_done(Console* console);
bool console_active(const Console* console);
void console_trace(Console* console, const char* text, u8 color);
void console_error(Console* console, const char* info);

// The --cmd batch, for the host that runs the cart and comes back: when the
// launch keeps its commands and they have all run, they are rewound and this
// says so, and the host shows the console again.
bool console_replay(Console* console);

// Takes what the console wants served, if anything: the host takes it once,
// does it, and answers through console_answer() — at once for a move, when the
// dialog is closed or the cart arrives for the rest. Until then the console
// asks for nothing else.
bool console_take(Console* console, ConsoleAsk* ask);
