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

#if defined(BUILD_EDITORS)

#include "editors/code.h"
#include "editors/sprite.h"
#include "editors/map.h"
#include "editors/world.h"
#include "editors/sfx.h"
#include "editors/music.h"
#include "screens/console.h"
#include "screens/surf.h"
#include "net.h"

#endif

#include "screens/start.h"
#include "screens/run.h"
#include "screens/menu.h"
#include "config.h"

#include "fs.h"

#define MD5_HASHSIZE 16

#if defined(TIC80_PRO)
#define TIC_EDITOR_BANKS (TIC_BANKS)
#else
#define TIC_EDITOR_BANKS 1
#endif

#ifdef BUILD_EDITORS
typedef struct
{
    u8 data[MD5_HASHSIZE];
} CartHash;

static const EditorMode Modes[] =
{
    TIC_CODE_MODE,
    TIC_SPRITE_MODE,
    TIC_MAP_MODE,
    TIC_SFX_MODE,
    TIC_MUSIC_MODE,
};

static const EditorMode BankModes[] =
{
    TIC_SPRITE_MODE,
    TIC_MAP_MODE,
    TIC_SFX_MODE,
    TIC_MUSIC_MODE,
};

#endif

typedef struct
{
    bool down;
    bool click;

    tic_point start;
    tic_point end;

} MouseState;

typedef struct
{
    tic_mapping mapping;
    s32 index;
    s32 key;
} Gamepads;

struct Studio
{
    tic_mem* tic;

    bool alive;

    EditorMode mode;
    EditorMode prevMode;
    EditorMode toolbarMode;

    struct
    {
        MouseState state[3];
    } mouse;

#if defined(BUILD_EDITORS)
    EditorMode menuMode;

    struct
    {
        CartHash hash;
        u64 mdate;
    }cart;

    struct
    {
        bool show;
        bool chained;

        union
        {
            struct
            {
                s8 sprites;
                s8 map;
                s8 sfx;
                s8 music;
            } index;

            s8 indexes[COUNT_OF(BankModes)];
        };

    } bank;

    struct
    {
        struct
        {
            s32 popup;
        } pos;

        Movie* movie;

        Movie idle;
        Movie show;
        Movie wait;
        Movie hide;

    } anim;

    struct
    {
        char message[STUDIO_TEXT_BUFFER_WIDTH];
    } popup;

    struct
    {
        char text[STUDIO_TEXT_BUFFER_WIDTH];
    } tooltip;

    struct
    {
        bool record;

        u32* buffer;
        s32 frames;
        s32 frame;

    } video;

    Code*       code;

    struct
    {
        Sprite* sprite[TIC_EDITOR_BANKS];
        Map*    map[TIC_EDITOR_BANKS];
        Sfx*    sfx[TIC_EDITOR_BANKS];
        Music*  music[TIC_EDITOR_BANKS];
    } banks;

    Console*    console;
    World*      world;
    Surf*       surf;

    tic_net* net;
#endif

    Start*      start;
    Run*        run;
    Menu*       menu;
    Config*     config;

    struct StudioMainMenu* mainmenu;

    tic_fs* fs;

    s32 samplerate;

    tic_font systemFont;

};
