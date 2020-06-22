// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

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
#include "ticapi.h"

#include <3ds.h>
#include <citro3d.h>

typedef struct
{
	C3D_Tex tex;
	u8 kd[8];
	u32 kd_count;
	bool render_dirty;

	u64 scroll_debounce;
} tic_n3ds_keyboard;

void n3ds_keyboard_init(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_free(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_draw(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_update(tic_n3ds_keyboard *kbd, tic_mem *tic, char *chcode);
void n3ds_gamepad_update(tic_n3ds_keyboard *kbd, tic_mem *tic);