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

#define TIC_FN "TIC"
#define SCN_FN "SCN"
#define OVR_FN "OVR"

#define API_KEYWORDS {TIC_FN, SCN_FN, OVR_FN, "print", "cls", "pix", "line", "rect", "rectb", \
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

	struct
	{
		tic_sound_register_data left[TIC_SOUND_CHANNELS];
		tic_sound_register_data right[TIC_SOUND_CHANNELS];
	} registers;

	Channel channels[TIC_SOUND_CHANNELS];
	struct
	{
		s32 ticks;
		Channel channels[TIC_SOUND_CHANNELS];
	} music;

	tic_tick tick;
	tic_scanline scanline;

	struct
	{
		tic_overline callback;
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
#if defined(TIC_BUILD_WITH_LUA) || defined(TIC_BUILD_WITH_MOON) || defined(TIC_BUILD_WITH_FENNEL)
		struct lua_State* lua;
#endif

#if defined(TIC_BUILD_WITH_JS)
		struct duk_hthread* js;
#endif

#if defined(TIC_BUILD_WITH_WREN)
		struct WrenVM* wren;
#endif	
	};

	struct
	{
		blip_buffer_t* left;
		blip_buffer_t* right;
	} blip;
	
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

typedef s32(DrawCharFunc)(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, bool alt);
s32 drawText(tic_mem* memory, const char* text, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, DrawCharFunc* func, bool alt);
s32 drawSpriteFont(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale, bool alt);
s32 drawFixedSpriteFont(tic_mem* memory, u8 index, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale, bool alt);
void parseCode(const tic_script_config* config, const char* start, u8* color, const tic_code_theme* theme);

#if defined(TIC_BUILD_WITH_LUA)
const tic_script_config* getLuaScriptConfig();

#	if defined(TIC_BUILD_WITH_MOON)
const tic_script_config* getMoonScriptConfig();
#	endif

#	if defined(TIC_BUILD_WITH_FENNEL)
const tic_script_config* getFennelConfig();
#	endif

#endif /* defined(TIC_BUILD_WITH_LUA) */


#if defined(TIC_BUILD_WITH_JS)
const tic_script_config* getJsScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_WREN)
const tic_script_config* getWrenScriptConfig();
#endif
