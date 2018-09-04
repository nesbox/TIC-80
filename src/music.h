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

typedef struct Music Music;

struct Music
{
	tic_mem* tic;

	tic_music* src;

	u8 track:MUSIC_TRACKS_BITS;

	struct
	{
		bool follow;
		s32 patternCol;

		s32 frame;
		s32 col;
		s32 row;
		s32 scroll;
		s32 note;

		struct
		{
			s32 octave;
			s32 sfx;
			s32 volume;
		} last;

		struct
		{
			tic_point start;
			tic_rect rect;
			bool drag;
		} select;

		bool patterns[TIC_SOUND_CHANNELS];

	} tracker;

	enum
	{
		MUSIC_TRACKER_TAB,
		MUSIC_PIANO_TAB,
	} tab;

	struct History* history;
	
	void(*tick)(Music*);
	void(*event)(Music*, StudioEvent);
};

void initMusic(Music*, tic_mem*, tic_music* src);