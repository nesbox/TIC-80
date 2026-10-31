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
#include "studio/clipboard.h"
#include "studio/keyboard.h"
#include "studio/mouse.h"

typedef struct Sfx Sfx;

// What the host gives the editor once: the machine, the pointer states and the
// tooltip ask.
typedef struct
{
    void* host;
    tic_mem* tic;
    MouseState* mouse;

    void (*tooltip)(void* host, const char* text);

} SfxDeps;

// The frame: the bar's pointer flag, the clipboard gesture and the cart's
// bank0 tiles the icons are drawn from.
typedef struct
{
    struct
    {
        bool pointerOwned;
        ClipboardEvent clipboard;

    } frame;

    const tic_tiles* cartTiles;

} SfxView;

struct Sfx
{
    SfxDeps deps;
    const SfxView* view;

    tic_mem* tic;
    tic_sfx* src;

    u8 index : SFX_COUNT_BITS;
    s32 volwave;
    s32 hoverWave;
    s32 holdValue;

    struct
    {
        bool active;
        s32 note;
        u32 tick;
    } play;

    struct History* history;
    struct History* waveHistory;
    bool needsHistoryUpdate;
    bool needsWaveHistoryUpdate;
    s32 lastEditCanvasTab;
};

extern const ClipboardOps SfxClipboard;

void initSfx(Sfx*, const SfxDeps*, tic_sfx* src);
void sfx_set_view(Sfx*, const SfxView*);
void sfx_tick(Sfx*);
void freeSfx(Sfx* sfx);
