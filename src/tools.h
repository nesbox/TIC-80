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

inline void tic_tool_poke4(void* addr, u32 index, u8 value)
{
	u8* val = (u8*)addr + (index >> 1);

	if(index & 1)
	{
		*val &= 0x0f;
		*val |= (value << 4);
	}
	else
	{
		*val &= 0xf0;
		*val |= value & 0x0f;
	}
}

inline u8 tic_tool_peek4(const void* addr, u32 index)
{
	u8 val = ((u8*)addr)[index >> 1];

	return index & 1 ? val >> 4 : val & 0xf;
}

bool tic_tool_parse_note(const char* noteStr, s32* note, s32* octave);
s32 tic_tool_get_pattern_id(const tic_track* track, s32 frame, s32 channel);
void tic_tool_set_pattern_id(tic_track* track, s32 frame, s32 channel, s32 id);
u32 tic_tool_find_closest_color(const tic_rgb* palette, const tic_rgb* color);
u32* tic_palette_blit(const tic_palette* src);
