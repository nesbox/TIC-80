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

typedef struct { u8 index; tic_flip flip; tic_rotate rotate; } RemapResult;
typedef void(*RemapFunc)(void*, s32 x, s32 y, RemapResult* result);
typedef struct
{
	union
	{
		struct
		{
			s8 wave;
			s8 volume;
			s8 arpeggio;
			s8 pitch;
		};

		s8 data[4];
	};
} tic_sfx_pos;

typedef void(*TraceOutput)(void*, const char*, u8 color);
typedef void(*ErrorOutput)(void*, const char*);
typedef void(*ExitCallback)(void*);
typedef bool(*CheckForceExit)(void*);

typedef struct
{
	TraceOutput trace;
	ErrorOutput error;
	ExitCallback exit;
	CheckForceExit forceExit;
	
	u64 (*counter)();
	u64 (*freq)();
	u64 start;

	void (*preprocessor)(void* data, char* dst);

	void* data;
} tic_tick_data;

typedef struct tic_mem tic_mem;
typedef void(*tic_scanline)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_overlap)(tic_mem* memory, void* data);

typedef struct
{
	s32  (*draw_char)			(tic_mem* memory, u8 symbol, s32 x, s32 y, u8 color);
	s32  (*text)				(tic_mem* memory, const char* text, s32 x, s32 y, u8 color);
	s32  (*fixed_text)			(tic_mem* memory, const char* text, s32 x, s32 y, u8 color);
	s32  (*text_ex)				(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale);
	void (*clear)				(tic_mem* memory, u8 color);
	void (*pixel)				(tic_mem* memory, s32 x, s32 y, u8 color);
	u8   (*get_pixel)			(tic_mem* memory, s32 x, s32 y);
	void (*line)				(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, u8 color);
	void (*rect)				(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color);
	void (*rect_border)			(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color);
	void (*sprite)				(tic_mem* memory, const tic_tiles* src, s32 index, s32 x, s32 y, u8* colors, s32 count);
	void (*sprite_ex)			(tic_mem* memory, const tic_tiles* src, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate);
	void (*map)					(tic_mem* memory, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale);
	void (*remap)				(tic_mem* memory, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data);
	void (*map_set)				(tic_mem* memory, tic_map* src, s32 x, s32 y, u8 value);
	u8   (*map_get)				(tic_mem* memory, const tic_map* src, s32 x, s32 y);
	void (*circle)				(tic_mem* memory, s32 x, s32 y, u32 radius, u8 color);
	void (*circle_border)		(tic_mem* memory, s32 x, s32 y, u32 radius, u8 color);
	void (*tri)					(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color);
	void(*textri)				(tic_mem* memory, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8 chroma);
	void (*clip)				(tic_mem* memory, s32 x, s32 y, s32 width, s32 height);
	void (*sfx)					(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel);
	void (*sfx_stop)			(tic_mem* memory, s32 channel);
	void (*sfx_ex)				(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 volume, s32 speed);
	tic_sfx_pos (*sfx_pos)		(tic_mem* memory, s32 channel);
	void (*music)				(tic_mem* memory, s32 track, s32 frame, s32 row, bool loop);
	void (*music_frame)			(tic_mem* memory, s32 track, s32 frame, s32 row, bool loop);
	double (*time)				(tic_mem* memory);
	void (*tick)				(tic_mem* memory, tic_tick_data* data);
	void (*scanline)			(tic_mem* memory, s32 row, void* data);
	void (*overlap)				(tic_mem* memory, void* data);
	void (*reset)				(tic_mem* memory);
	void (*pause)				(tic_mem* memory);
	void (*resume)				(tic_mem* memory);
	void (*sync)				(tic_mem* memory, const char* section, s32 bank, bool toCart);
	u32 (*btnp)					(tic_mem* memory, s32 id, s32 hold, s32 period);

	void (*load)				(tic_cartridge* rom, const u8* buffer, s32 size, bool palette);
	s32  (*save)				(const tic_cartridge* rom, u8* buffer);

	void (*tick_start)			(tic_mem* memory, const tic_sfx* sfx, const tic_music* music);
	void (*tick_end)			(tic_mem* memory);
	void (*blit)				(tic_mem* tic, tic_scanline scanline, tic_overlap overlap, void* data);

	tic_script_lang (*get_script)(tic_mem* memory);
} tic_api;

struct tic_mem
{
	tic_ram 			ram;
	tic_cartridge 		cart;
	tic_cartridge		config;
	tic_input_method 	input;
	tic_script_lang 	script;
	tic_font 			font;
	tic_api 			api;

	char saveid[TIC_SAVEID_SIZE];

	struct
	{
		s16* buffer;
		s32 size;
	} samples;

	u32 screen[TIC80_FULLWIDTH * TIC80_FULLHEIGHT];
};

tic_mem* tic_create(s32 samplerate);
void tic_close(tic_mem* memory);

typedef struct
{
	tic80 tic;
	tic_mem* memory;
	tic_tick_data tickData;
} tic80_local;
