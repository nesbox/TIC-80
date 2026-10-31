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

#include "tic80_types.h"
#include "tic.h"
#include "studio/mouse.h"

typedef struct Menu Menu;

// What the widget gets once: the machine, the config cart it draws and plays
// from, and the pointer's three button states.
typedef struct
{
    tic_mem* tic;
    const tic_cartridge* cart;
    MouseState* mouse;
} MenuDeps;

// The frame: the studio is in the menu, and the bar owns the pointer.
typedef struct
{
    bool active;
    bool pointerOwned;
} MenuView;

Menu* menu_create(const MenuDeps* deps);
void menu_set_view(Menu* menu, const MenuView* view);
void menu_tick(Menu* menu);

typedef s32 (*MenuOptionGetHandler)(void*);
typedef void (*MenuOptionSetHandler)(void*, s32);

typedef struct
{
    const char** values;
    s32 count;
    MenuOptionGetHandler get;
    MenuOptionSetHandler set;
    s32 width;
    s32 pos;
} MenuOption;

typedef void (*MenuItemHandler)(void*, s32);

typedef struct
{
    const char* label;
    MenuItemHandler handler;

    MenuOption* option;
    const char* help;
    bool back;
    s32 width;
    tic_keycode hotkey;
} MenuItem;

void menu_init(Menu* menu, const MenuItem* items, s32 rows, s32 pos, s32 backPos, MenuItemHandler handler, void* data);
bool menu_back(Menu* menu);
void menu_free(Menu* menu);

void menu_anim(struct tic_mem* tic, s32 ticks);
void menu_anim_scanline(struct tic_mem* tic, s32 row, void* data);
