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

typedef struct Map Map;

struct Map
{
	tic_mem* tic;

	tic_map* src;
	
	s32 tickCounter;

	enum
	{
		MAP_DRAW_MODE = 0,
		MAP_DRAG_MODE,
		MAP_SELECT_MODE,
		MAP_FILL_MODE,
	} mode;

	struct
	{
		bool grid;
		bool draw;
		tic_point start;
	} canvas;

	struct
	{
		bool show;
		tic_rect rect;
		tic_point start;
		bool drag;
	} sheet;

	struct
	{
		s32 x;
		s32 y;

		tic_point start;

		bool active;
		bool gesture;

	} scroll;

	struct
	{
		tic_rect rect;
		tic_point start;
		bool drag;
	} select;

	u8* paste;

	struct History* history;

	void (*tick)(Map*);
	void (*event)(Map*, StudioEvent);
	void (*scanline)(tic_mem* tic, s32 row, void* data);
	void (*overline)(tic_mem* tic, void* data);
};

void initMap(Map*, tic_mem*, tic_map* src);
