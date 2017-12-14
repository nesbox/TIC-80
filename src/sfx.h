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

typedef struct Sfx Sfx;

struct Sfx
{
	tic_mem* tic;

	tic_sfx* src;

	u8 index:SFX_COUNT_BITS;

	struct
	{
		bool active;
		s32 note;
	} play;
	
	enum 
	{
		SFX_WAVE_TAB = 0,
		SFX_VOLUME_TAB,
		SFX_ARPEGGIO_TAB,
		SFX_PITCH_TAB,
	}canvasTab;

	struct
	{
		u8 index:4;
	} waveform;

	enum
	{
		SFX_WAVEFORM_TAB, 
		SFX_ENVELOPES_TAB,
	} tab;

	struct History* history;

	void(*tick)(Sfx*);
	void(*event)(Sfx*, StudioEvent);
};

void initSfx(Sfx*, tic_mem*, tic_sfx* src);