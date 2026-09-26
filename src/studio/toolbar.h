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

#include "studio.h"
#include "apps.h"
#include "mouse.h"

// The strip's state: one per studio, not one per editor, because its position
// and its animation are one thing on one screen. The host fills the second
// half at init — the strip itself holds no Studio.
// Tagged, because apps.h forward-declares `struct Toolbar` for the band slot.
typedef struct Toolbar
{
    tic_mem*            tic;
    const StudioConfig* config;
    MouseState*         mouse;      // [3], the studio's
    char*               tooltip;    // the studio's buffer, drawn by the strip
    const EditorApp*    apps;
    s32                 appCount;

    s32        y;                   // vertical offset; 0 until the strip can hide
    EditorMode mode;                // whose band is on screen
    EditorMode requested;           // a tab click, for the host to pick up

    s32  railX;                     // the right rail's packing cursor
    bool hideName;                  // a pro build's bank row takes the name's space

    // The strip's y when each button's press began, so a widget that moves
    // under the pointer still reads the click it was given.
    struct
    {
        bool wasDown[3];
        s32  y[3];
    } press;

} Toolbar;

// One widget on the right rail: an icon or a short label, never both. `pressed`
// is the active look (dark background, inverted glyph); `color` is the glyph's
// colour otherwise, which is how the music editor's toggles show green.
typedef struct
{
    u8          icon;
    const char* label;
    const char* tip;
    s32         width;
    u8          color;
    bool        pressed;
    bool        enabled;            // a click is consumed either way

} ToolbarButton;

void toolbar_step(Toolbar*);
void toolbar_begin(Toolbar*);
bool toolbar_button(Toolbar*, const ToolbarButton*);
bool toolbar_slider(Toolbar*, s32 id, const char* tip, s32* value, s32 min, s32 max);
void toolbar_end(Toolbar*);

// The strip's own drawing and sound. These are the pieces the editors' panels
// below the strip need too, so studio.c keeps a Studio*-taking wrapper over
// each and no call site outside changes.
void toolbar_icon(tic_mem* tic, const tic_tiles* tiles, s32 id, s32 x, s32 y, u8 color);
void toolbar_cursor(tic_mem* tic, tic_cursor id);
void toolbar_playClick(tic_mem* tic, const tic_sfx* sfx, s32 id);
