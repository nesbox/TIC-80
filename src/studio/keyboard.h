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

// What the machine's keyboard says: a key being pressed, what it types, and the
// clipboard gesture the keys ask for. The studio's wrappers add its own hold and
// period, so both faces answer the same questions.

bool keyboard_pressed(tic_mem* tic, tic_key key);

// Pressed this frame rather than held: the no-hold, no-period check that four
// files used to spell out for themselves.
bool keyboard_pressed_once(tic_mem* tic, s32 key);

bool keyboard_enter(tic_mem* tic, s32 hold, s32 period);
char keyboard_text(tic_mem* tic);
ClipboardEvent keyboard_clipboard(tic_mem* tic);
