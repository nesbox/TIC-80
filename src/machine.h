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
#include "blip_buf.h"

#define SFX_DEF_SPEED (1 << SFX_SPEED_BITS)

#define API_KEYWORDS {"TIC", "scanline", "OVR", "print", "cls", "pix", "line", "rect", "rectb", \
	"spr", "btn", "btnp", "sfx", "map", "mget", "mset", "peek", "poke", "peek4", "poke4", \
	"memcpy", "memset", "trace", "pmem", "time", "exit", "font", "mouse", "circ", "circb", "tri", "textri", \
	"clip", "music", "sync", "reset", "key", "keyp"}
	
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
		tic80_gamepads previous;

		u32 holds[sizeof(tic80_gamepads) * BITS_IN_BYTE];
	} gamepads;

	struct 
	{
		tic80_keyboard previous;

		u32 holds[tic_keys_count];
	} keyboard;

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

	tic_tick tick;
	tic_scanline scanline;

	struct
	{
		tic_overlap callback;
		u32 palette[TIC_PALETTE_SIZE];
	} ovr;

	void (*setpix)(tic_mem* memory, s32 x, s32 y, u8 color);
	u8 (*getpix)(tic_mem* memory, s32 x, s32 y);
	void (*drawhline)(tic_mem* memory, s32 xl, s32 xr, s32 y, u8 color);

	u32 synced;

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
	};

	blip_buffer_t* blip;
	s32 samplerate;

	struct
	{
		const tic_sfx* sfx;
		const tic_music* music;
	} sound;

	tic_tick_data* data;

	MachineState state;

	struct
	{
		MachineState state;	
		tic_ram ram;

		struct
		{
			u64 start;
			u64 paused;			
		} time;
	} pause;

} tic_machine;

typedef s32(DrawCharFunc)(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale);
s32 drawText(tic_mem* memory, const char* text, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, DrawCharFunc* func);
s32 drawSpriteFont(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale);
s32 drawFixedSpriteFont(tic_mem* memory, u8 index, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale);
void parseCode(const tic_script_config* config, const char* start, u8* color, const tic_code_theme* theme);

const tic_script_config* getLuaScriptConfig();
const tic_script_config* getMoonScriptConfig();
const tic_script_config* getJsScriptConfig();
