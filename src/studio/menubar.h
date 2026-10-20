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
#include "mouse.h"
#include "ui.h"

// The studio's shell: a title row and its dropdowns. The host supplies the
// categories and runs the items; nothing here knows an editor.

enum
{
    MENUBAR_HEIGHT = 7,
    MENUBAR_NO_MODE = -1,
};

typedef void (*MenuBarHandler)(void* data, s32 id);

typedef struct MenuBarCategory MenuBarCategory;

typedef struct
{
    const char* label;
    const char* accel; // right-aligned, NULL for none
    MenuBarHandler handler;
    s32 id;
    bool enabled;
    bool separator;                 // a line in place of an item
    u8 icon;                        // left of the label; 0 for none
    u8 color;                       // icon colour when enabled; 0 for the default
    const MenuBarCategory* submenu; // opens instead of running the handler
    s8 checked;                     // -1: not a toggle; 0: off; 1: on
} MenuBarItem;

struct MenuBarCategory
{
    const char* title;
    const MenuBarItem* items;
    s32 count;
    bool right;   // packed from the right edge
    u8 icon;      // before the title in the bar
    s32 selected; // the marked item's id, -1 for none
    u8 iconColor; // the bar icon's colour; 0 for the default
    bool hidden;  // left out of the bar and the pointer's way
};

typedef struct
{
    MenuBarCategory* categories;
    s32 count;
    MouseState* mouse;
    void* data; // the host's, handed back to a handler

    s32 mode;
    s32 requested; // MENUBAR_NO_MODE when nothing is asked
    u8 checkIcon;  // the host's atlas icon for a toggle without one; 0 for none

    bool autohide; // the host's toggle: slide the bar away when the pointer leaves
    s32 y;         // bar offset: 0 shown, -MENUBAR_HEIGHT hidden

    s32 open;
    const MenuBarCategory* sub; // the open submenu
    s32 subItem;                // the parent row it hangs from
    MenuBarHandler pending;
    s32 pendingId;
    bool escaped; // ESC closed the menu this frame
} Menubar;

void menubar_init(Menubar*, MenuBarCategory*, s32 count, MouseState*, void* data, u8 checkIcon);
void menubar_step(Menubar*, tic_mem*, s32 mode);
void menubar_draw(Menubar*, tic_mem*, s32 mode, const tic_tiles* icons);
