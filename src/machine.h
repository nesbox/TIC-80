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

#include "ticapi.h"
#include "tools.h"
#include "ext/blip_buf.h"

#define SFX_DEF_SPEED (1 << SFX_SPEED_BITS)

typedef struct
{
	s32 time;       /* clock time of next delta */
	s32 phase;      /* position within waveform */
	s32 amp;        /* current amplitude in delta buffer */
}tic_sound_register_data;

typedef struct
{
	s32 tick;
	tic_sfx_pos pos;
	s32 index;
	u16 freq;
	u8 volume:4;
	s8 speed:SFX_SPEED_BITS;
	s32 duration;
} Channel;

typedef void(ScanlineFunc)(tic_mem* memory, s32 row);

typedef struct
{
	s32 l;
	s32 t;
	s32 r;
	s32 b;
} Clip;

typedef struct
{

	struct
	{
		tic80_input previous;

		u32 holds[sizeof(tic80_input) * BITS_IN_BYTE];
	} gamepad;

	Clip clip;

	tic_sound_register_data registers[TIC_SOUND_CHANNELS];
	Channel channels[TIC_SOUND_CHANNELS];
	struct
	{
		enum
		{
			MusicStop = 0,
			MusicPlayFrame,
			MusicPlay,
		} play;

		s32 ticks;

		Channel channels[TIC_SOUND_CHANNELS];
	} music;

	ScanlineFunc* scanline;
	bool initialized;
} MachineState;

typedef struct
{
	tic_mem memory; // it should be first
	tic_api api;

	struct
	{
		struct duk_hthread* js;
		struct lua_State* lua;	
		struct bf_State* bf;	
	};

	blip_buffer_t* blip;
	s32 samplerate;
	const tic_sound* soundSrc;

	tic_tick_data* data;

	MachineState state;

	struct
	{
		MachineState state;	
		tic_sound_register registers[TIC_SOUND_CHANNELS];
		tic_music_pos music_pos;
		tic_vram vram;
	} pause;

} tic_machine;

typedef s32(DrawCharFunc)(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale);
s32 drawText(tic_mem* memory, const char* text, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, DrawCharFunc* func);
s32 drawSpriteFont(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale);
s32 drawFixedSpriteFont(tic_mem* memory, u8 index, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale);

void closeLua(tic_machine* machine);
void closeJavascript(tic_machine* machine);
void closeBrainfuck(tic_machine* machine);

bool initMoonscript(tic_machine* machine, const char* code);
bool initLua(tic_machine* machine, const char* code);
bool initJavascript(tic_machine* machine, const char* code);
bool initBrainfuck(tic_machine* machine, const char* code);

void callLuaTick(tic_machine* machine);
void callJavascriptTick(tic_machine* machine);
void callBrainfuckTick(tic_machine* machine);

void callLuaScanline(tic_mem* memory, s32 row);
void callJavascriptScanline(tic_mem* memory, s32 row);
void callBrainfuckScanline(tic_mem* memory, s32 row);
