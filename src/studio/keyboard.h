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

// How long a key is held before it repeats, and how fast it repeats after that:
// the one place the studio's own feel is spelled.
#define KEYBOARD_HOLD   20
#define KEYBOARD_PERIOD 3

// The gesture the keys ask for, which `keyboard_clipboard` answers with.
typedef enum
{
    TIC_CLIPBOARD_NONE,
    TIC_CLIPBOARD_CUT,
    TIC_CLIPBOARD_COPY,
    TIC_CLIPBOARD_PASTE,
} ClipboardEvent;

// What the machine's keyboard says: a key pressed — and still repeating while it
// is held, which is the studio's own feel, KEYBOARD_HOLD and KEYBOARD_PERIOD —
// what it types, and the clipboard gesture the keys ask for. The enter check
// takes its hold and period because its callers have their own.

bool keyboard_pressed(tic_mem* tic, tic_key key);

// Pressed this frame rather than held: the no-hold, no-period check that four
// files used to spell out for themselves.
bool keyboard_pressed_once(tic_mem* tic, s32 key);

bool keyboard_enter(tic_mem* tic, s32 hold, s32 period);
char keyboard_text(tic_mem* tic);
ClipboardEvent keyboard_clipboard(tic_mem* tic);
