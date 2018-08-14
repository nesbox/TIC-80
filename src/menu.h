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

typedef struct Menu Menu;

struct Menu
{
	tic_mem* tic;
	struct FileSystem* fs;

	bool init;
	void* bg;
	s32 ticks;

	struct
	{
		s32 focus;
	} main;

	struct
	{
		u32 tab;
		s32 selected;
	} gamepad;

	tic_point pos;

	struct
	{
		tic_point start;
		bool active;
	} drag;

	enum
	{
		MAIN_MENU_MODE,
		GAMEPAD_MENU_MODE,
	} mode;
	
	void(*tick)(Menu* Menu);
};

void initMenu(Menu* menu, tic_mem* tic, struct FileSystem* fs);
