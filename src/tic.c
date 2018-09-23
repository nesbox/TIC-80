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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>
#include <stddef.h>

#include "ticapi.h"
#include "tools.h"
#include "machine.h"
#include "ext/gif.h"

#define CLOCKRATE (TIC_FRAMERATE*30000)
#define MIN_PERIOD_VALUE 10
#define MAX_PERIOD_VALUE 4096
#define BASE_NOTE_FREQ 440.0
#define BASE_NOTE_POS 49.0
#define ENVELOPE_FREQ_SCALE 2
#define NOTES_PER_MUNUTE (TIC_FRAMERATE / NOTES_PER_BEET * 60)
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

typedef enum
{
	CHUNK_DUMMY, 	// 0
	CHUNK_TILES, 	// 1
	CHUNK_SPRITES, 	// 2
	CHUNK_COVER, 	// 3
	CHUNK_MAP, 		// 4
	CHUNK_CODE, 	// 5
	CHUNK_TEMP,		// 6
	CHUNK_TEMP2, 	// 7
	CHUNK_TEMP3,	// 8
	CHUNK_SAMPLES,	// 9
	CHUNK_WAVEFORM,	// 10
	CHUNK_TEMP4,	// 11
	CHUNK_PALETTE, 	// 12
	CHUNK_PATTERNS, // 13
	CHUNK_MUSIC,	// 14

} ChunkType;

typedef struct
{
	ChunkType type:5;
	u32 bank:TIC_BANK_BITS;
	u32 size:16; // max chunk size is 64K
	u32 temp:8;
} Chunk;

STATIC_ASSERT(tic_bank_bits, TIC_BANK_BITS == 3);
STATIC_ASSERT(tic_chunk_size, sizeof(Chunk) == 4);
STATIC_ASSERT(tic_map, sizeof(tic_map) < 1024*32);
STATIC_ASSERT(tic_sample, sizeof(tic_sample) == 66);
STATIC_ASSERT(tic_track_pattern, sizeof(tic_track_pattern) == 3*MUSIC_PATTERN_ROWS);
STATIC_ASSERT(tic_track, sizeof(tic_track) == 3*MUSIC_FRAMES+3);
STATIC_ASSERT(tic_vram, sizeof(tic_vram) == TIC_VRAM_SIZE);
STATIC_ASSERT(tic_ram, sizeof(tic_ram) == TIC_RAM_SIZE);
STATIC_ASSERT(tic_sound_register, sizeof(tic_sound_register) == 16+2);
STATIC_ASSERT(tic80_input, sizeof(tic80_input) == 12);

static void update_amp(blip_buffer_t* blip, tic_sound_register_data* data, s32 new_amp )
{
	s32 delta = new_amp - data->amp;
	data->amp += delta;
	blip_add_delta( blip, data->time, delta );
}

static inline double freq2note(double freq)
{
	return (double)NOTES * log2(freq / BASE_NOTE_FREQ) + BASE_NOTE_POS;
}

static inline s32 note2freq(double note)
{
	return round(pow(2, (note - BASE_NOTE_POS) / (double)NOTES) * BASE_NOTE_FREQ);
}

static inline s32 freq2period(double freq)
{
	if(freq == 0.0) return MAX_PERIOD_VALUE;

	enum {Rate = CLOCKRATE * ENVELOPE_FREQ_SCALE / ENVELOPE_VALUES};
	s32 period = round((double)Rate / freq - 1.0);

	if(period < MIN_PERIOD_VALUE) return MIN_PERIOD_VALUE;
	if(period > MAX_PERIOD_VALUE) return MAX_PERIOD_VALUE;

	return period;
}

static inline s32 getAmp(const tic_sound_register* reg, s32 amp)
{
	enum {MaxAmp = (u16)-1 / (MAX_VOLUME * TIC_SOUND_CHANNELS)};

	return amp * MaxAmp * reg->volume / MAX_VOLUME;
}

static void runEnvelope(blip_buffer_t* blip, tic_sound_register* reg, tic_sound_register_data* data, s32 end_time, u8 masterVolume)
{
	s32 period = freq2period(reg->freq * ENVELOPE_FREQ_SCALE);

	for ( ; data->time < end_time; data->time += period )
	{
		data->phase = (data->phase + 1) % ENVELOPE_VALUES;

		update_amp(blip, data, getAmp(reg, tic_tool_peek4(reg->waveform.data, data->phase) * masterVolume / MAX_VOLUME));
	}
}

static void runNoise(blip_buffer_t* blip, tic_sound_register* reg, tic_sound_register_data* data, s32 end_time, u8 masterVolume)
{
	// phase is noise LFSR, which must never be zero 
	if ( data->phase == 0 )
		data->phase = 1;
	
	s32 period = freq2period(reg->freq);

	for ( ; data->time < end_time; data->time += period )
	{
		data->phase = ((data->phase & 1) * (0b11 << 13)) ^ (data->phase >> 1);
		update_amp(blip, data, getAmp(reg, (data->phase & 1) ? masterVolume : 0));
	}
}

static void resetPalette(tic_mem* memory)
{
	static const u8 DefaultMapping[] = {16, 50, 84, 118, 152, 186, 220, 254};
	memcpy(memory->ram.vram.palette.data, memory->cart.bank0.palette.data, sizeof(tic_palette));
	memcpy(memory->ram.vram.mapping, DefaultMapping, sizeof DefaultMapping);
}

static inline u8 mapColor(tic_mem* tic, u8 color)
{
	return tic_tool_peek4(tic->ram.vram.mapping, color & 0xf);
}

static void setPixelDma(tic_mem* tic, s32 x, s32 y, u8 color)
{
	tic_tool_poke4(tic->ram.vram.screen.data, y * TIC80_WIDTH + x, color);
}

static inline u32* getOvrAddr(tic_mem* tic, s32 x, s32 y)
{
	enum {Top = (TIC80_FULLHEIGHT-TIC80_HEIGHT)/2};
	enum {Left = (TIC80_FULLWIDTH-TIC80_WIDTH)/2};

	return tic->screen + x + (y << TIC80_FULLWIDTH_BITS) + (Left + Top * TIC80_FULLWIDTH);
}

static void setPixelOvr(tic_mem* tic, s32 x, s32 y, u8 color)
{
	tic_machine* machine = (tic_machine*)tic;
	
	*getOvrAddr(tic, x, y) = *(machine->state.ovr.palette + color);
}

static u8 getPixelOvr(tic_mem* tic, s32 x, s32 y)
{
	tic_machine* machine = (tic_machine*)tic;
	
	u32 color = *getOvrAddr(tic, x, y);
	u32* pal = machine->state.ovr.palette;

	for(s32 i = 0; i < TIC_PALETTE_SIZE; i++, pal++)
		if(*pal == color)
			return i;

	return 0;
}

static u8 getPixelDma(tic_mem* tic, s32 x, s32 y)
{
	tic_machine* machine = (tic_machine*)tic;

	return tic_tool_peek4(machine->memory.ram.vram.screen.data, y * TIC80_WIDTH + x);
}

static void setPixel(tic_machine* machine, s32 x, s32 y, u8 color)
{
	if(x < machine->state.clip.l || y < machine->state.clip.t || x >= machine->state.clip.r || y >= machine->state.clip.b) return;

	machine->state.setpix(&machine->memory, x, y, mapColor(&machine->memory, color));
}

static u8 getPixel(tic_machine* machine, s32 x, s32 y)
{
	if(x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT) return 0;

	return machine->state.getpix(&machine->memory, x, y);
}

static void drawHLineDma(tic_mem* memory, s32 xl, s32 xr, s32 y, u8 color)
{
	color = color << 4 | color;
	if (xl >= xr) return;
	if (xl & 1) {
		tic_tool_poke4(&memory->ram.vram.screen.data, y * TIC80_WIDTH + xl, color);
		xl++;
	}
	s32 count = (xr - xl) >> 1;
	u8 *screen = memory->ram.vram.screen.data + ((y * TIC80_WIDTH + xl) >> 1);
	for(s32 i = 0; i < count; i++) *screen++ = color;
	if (xr & 1) {
		tic_tool_poke4(&memory->ram.vram.screen.data, y * TIC80_WIDTH + xr - 1, color);
	}
}

static void drawHLineOvr(tic_mem* tic, s32 x1, s32 x2, s32 y, u8 color)
{
	tic_machine* machine = (tic_machine*)tic;
	u32 final_color = *(machine->state.ovr.palette + color);
	for(s32 x = x1; x < x2; ++x) {
		*getOvrAddr(tic, x, y) = final_color;
	}
}


static void drawHLine(tic_machine* machine, s32 x, s32 y, s32 width, u8 color)
{
	if(y < machine->state.clip.t || machine->state.clip.b <= y) return;
	u8 final_color = mapColor(&machine->memory, color);
	s32 xl = max(x, machine->state.clip.l);
	s32 xr = min(x + width, machine->state.clip.r);
	machine->state.drawhline(&machine->memory, xl, xr, y, final_color);
}

static void drawVLine(tic_machine* machine, s32 x, s32 y, s32 height, u8 color)
{
	if(x < 0 || x >= TIC80_WIDTH) return;

	s32 yl = y < 0 ? 0 : y;
	s32 yr = y + height >= TIC80_HEIGHT ? TIC80_HEIGHT : y + height;

	for(s32 i = yl; i < yr; ++i)
		setPixel(machine, x, i, color);
}

static void drawRect(tic_machine* machine, s32 x, s32 y, s32 width, s32 height, u8 color)
{
	for(s32 i = y; i < y + height; ++i)
		drawHLine(machine, x, i, width, color);
}

static void drawRectBorder(tic_machine* machine, s32 x, s32 y, s32 width, s32 height, u8 color)
{
	drawHLine(machine, x, y, width, color);
	drawHLine(machine, x, y + height - 1, width, color);

	drawVLine(machine, x, y, height, color);
	drawVLine(machine, x + width - 1, y, height, color);
}

#define DRAW_TILE_BODY(INDEX_EXPR) do {\
	for(s32 py=sy; py < ey; py++, y++) \
	{ \
		s32 xx = x; \
		for(s32 px=sx; px < ex; px++, xx++) \
		{ \
			u8 color = mapping[tic_tool_peek4(buffer, INDEX_EXPR)]; \
			if(color != 255) machine->state.setpix(&machine->memory, xx, y, color); \
		} \
	} \
	} while(0)

#define REVERT(X) (TIC_SPRITESIZE - 1 - (X))
#define INDEX_XY(X, Y) ((Y) * TIC_SPRITESIZE + (X))

static void drawTile(tic_machine* machine, const tic_tile* buffer, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
	static u8 mapping[TIC_PALETTE_SIZE];
	for (s32 i = 0; i < TIC_PALETTE_SIZE; i++)
	{
		u8 mapped = tic_tool_peek4(machine->memory.ram.vram.mapping, i);
		for (s32 j = 0; j < count; j++)
		{
			if (mapped == colors[j]) {
				mapped = 255;
				break;
			}
		}
		mapping[i] = mapped;
	}

	rotate &= 0b11;
	u32 orientation = flip & 0b11;

	if(rotate == tic_90_rotate) orientation ^= 0b001;
	else if(rotate == tic_180_rotate) orientation ^= 0b011;
	else if(rotate == tic_270_rotate) orientation ^= 0b010;
	if (rotate == tic_90_rotate || rotate == tic_270_rotate) orientation |= 0b100;

	if (scale == 1) {
		// the most common path
		s32 sx, sy, ex, ey;
		sx = machine->state.clip.l - x; if (sx < 0) sx = 0;
		sy = machine->state.clip.t - y; if (sy < 0) sy = 0;
		ex = machine->state.clip.r - x; if (ex > TIC_SPRITESIZE) ex = TIC_SPRITESIZE;
		ey = machine->state.clip.b - y; if (ey > TIC_SPRITESIZE) ey = TIC_SPRITESIZE;
		y += sy;
		x += sx;
		switch (orientation) {
			case 0b100: DRAW_TILE_BODY(INDEX_XY(py, px)); break;
			case 0b110: DRAW_TILE_BODY(INDEX_XY(REVERT(py), px)); break;
			case 0b101: DRAW_TILE_BODY(INDEX_XY(py, REVERT(px))); break;
			case 0b111: DRAW_TILE_BODY(INDEX_XY(REVERT(py), REVERT(px))); break;
			case 0b000: DRAW_TILE_BODY(INDEX_XY(px, py)); break;
			case 0b010: DRAW_TILE_BODY(INDEX_XY(px, REVERT(py))); break;
			case 0b001: DRAW_TILE_BODY(INDEX_XY(REVERT(px), py)); break;
			case 0b011: DRAW_TILE_BODY(INDEX_XY(REVERT(px), REVERT(py))); break;
			default: assert(!"Unknown value of orientation in drawTile");
		}
		return;
	}

	for(s32 py=0; py < TIC_SPRITESIZE; py++, y+=scale)
	{
		s32 xx = x;
		for(s32 px=0; px < TIC_SPRITESIZE; px++, xx+=scale)
		{
			s32 i;
			s32 ix = orientation & 0b001 ? TIC_SPRITESIZE - px - 1: px;
			s32 iy = orientation & 0b010 ? TIC_SPRITESIZE - py - 1: py;
			if(orientation & 0b100) {
				i = ix * TIC_SPRITESIZE + iy;
			} else {
				i = iy * TIC_SPRITESIZE + ix;
			}
			u8 color = tic_tool_peek4(buffer, i);
			if(mapping[color] != 255) drawRect(machine, xx, y, scale, scale, color);
		}
	}
}

static void drawMap(tic_machine* machine, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data)
{
	const s32 size = TIC_SPRITESIZE * scale;

	for(s32 j = y, jj = sy; j < y + height; j++, jj += size)
		for(s32 i = x, ii = sx; i < x + width; i++, ii += size)
		{
			s32 mi = i;
			s32 mj = j;

			while(mi < 0) mi += TIC_MAP_WIDTH;
			while(mj < 0) mj += TIC_MAP_HEIGHT;
			while(mi >= TIC_MAP_WIDTH) mi -= TIC_MAP_WIDTH;
			while(mj >= TIC_MAP_HEIGHT) mj -= TIC_MAP_HEIGHT;
			
			s32 index = mi + mj * TIC_MAP_WIDTH;
			RemapResult tile = { *(src->data + index), tic_no_flip, tic_no_rotate };

			if (remap)
				remap(data, mi, mj, &tile);

			drawTile(machine, tiles->data + tile.index, ii, jj, &chromakey, 1, scale, tile.flip, tile.rotate);
		}
}

static void resetSfx(Channel* channel)
{
	memset(channel->pos.data, -1, sizeof(tic_sfx_pos));
	channel->tick = -1;
}

static void channelSfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, Channel* c, s32 volume, s32 speed)
{
	tic_machine* machine = (tic_machine*)memory;

	c->volume = volume;

	if(index >= 0)
	{
		struct {s8 speed:SFX_SPEED_BITS;} temp = {speed};
		c->speed = speed == temp.speed ? speed : machine->sound.sfx->samples.data[index].speed;
	}

	// start index of idealized piano
	enum {PianoStart = -8};

	s32 freq = note2freq(note + octave * NOTES + PianoStart);

	c->duration = duration;
	c->freq = freq;
	c->index = index;

	resetSfx(c);
}

static void musicSfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 volume, s32 channel)
{
	tic_machine* machine = (tic_machine*)memory;
	channelSfx(memory, index, note, octave, -1, &machine->state.music.channels[channel], MAX_VOLUME - volume, SFX_DEF_SPEED);
}

static void resetMusic(tic_mem* memory)
{
	for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
		musicSfx(memory, -1, 0, 0, 0, c);
}

static void setMusic(tic_machine* machine, s32 index, s32 frame, s32 row, bool loop)
{
	tic_mem* memory = (tic_mem*)machine;

	memory->ram.sound_state.music.track = index;

	if(index < 0)
	{
		memory->ram.sound_state.flag.music_state = tic_music_stop;
		resetMusic(memory);
	}
	else
	{
		memory->ram.sound_state.music.row = row;
		memory->ram.sound_state.music.frame = frame < 0 ? 0 : frame;
		memory->ram.sound_state.flag.music_loop = loop;
		memory->ram.sound_state.flag.music_state = tic_music_play;

		const tic_track* track = &machine->sound.music->tracks.data[index];
		machine->state.music.ticks = row >= 0 ? row * (track->speed + DEFAULT_SPEED) * NOTES_PER_MUNUTE / (track->tempo + DEFAULT_TEMPO) / DEFAULT_SPEED : 0;
	}
}

static void api_music(tic_mem* memory, s32 index, s32 frame, s32 row, bool loop)
{
	tic_machine* machine = (tic_machine*)memory;

	setMusic(machine, index, frame, row, loop);

	if(index >= 0)
		memory->ram.sound_state.flag.music_state = tic_music_play;
}

static void soundClear(tic_mem* memory)
{
	{
		tic_machine* machine = (tic_machine*)memory;

		Channel channel =
		{ 
			.tick = -1,
			.pos = 
			{
				.volume = -1,
				.wave = -1, 
				.arpeggio = -1, 
				.pitch = -1,				
			},
			.index = -1,
			.freq = 0,
			.volume = MAX_VOLUME,
			.speed = 0,
			.duration = -1,
		};

		for(s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
		{
			memcpy(machine->state.channels+i, &channel, sizeof channel);
			memcpy(machine->state.music.channels+i, &channel, sizeof channel);
			
			{
				tic_sound_register* reg = &memory->ram.registers[i];
				memset(reg, 0, sizeof(tic_sound_register));
			}
			
			{
				tic_sound_register_data* data = &machine->state.registers.left[i];
				memset(data, 0, sizeof(tic_sound_register_data));				
			}

			{
				tic_sound_register_data* data = &machine->state.registers.right[i];
				memset(data, 0, sizeof(tic_sound_register_data));				
			}
		}

		api_music(memory, -1, 0, 0, false);
	}

	memset(memory->samples.buffer, 0, memory->samples.size);
}

static void updateSaveid(tic_mem* memory);

static void api_clip(tic_mem* memory, s32 x, s32 y, s32 width, s32 height)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->state.clip.l = x;
	machine->state.clip.t = y;
	machine->state.clip.r = x + width;
	machine->state.clip.b = y + height;

	if(machine->state.clip.l < 0) machine->state.clip.l = 0;
	if(machine->state.clip.t < 0) machine->state.clip.t = 0;
	if(machine->state.clip.r > TIC80_WIDTH) machine->state.clip.r = TIC80_WIDTH;
	if(machine->state.clip.b > TIC80_HEIGHT) machine->state.clip.b = TIC80_HEIGHT;
}

static void api_reset(tic_mem* memory)
{
	resetPalette(memory);

	memset(&memory->ram.vram.vars, 0, sizeof memory->ram.vram.vars);
	
	api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

	soundClear(memory);

	tic_machine* machine = (tic_machine*)memory;
	machine->state.initialized = false;
	machine->state.scanline = NULL;
	machine->state.ovr.callback = NULL;

	machine->state.setpix = setPixelDma;
	machine->state.getpix = getPixelDma;
	machine->state.drawhline = drawHLineDma;

	updateSaveid(memory);
}

static void api_pause(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	memcpy(&machine->pause.state, &machine->state, sizeof(MachineState));
	memcpy(&machine->pause.ram, &memory->ram, sizeof(tic_ram));

	machine->pause.time.start = machine->data->start;
	machine->pause.time.paused = machine->data->counter();
}

static void api_resume(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	if (machine->data)
	{
		memcpy(&machine->state, &machine->pause.state, sizeof(MachineState));
		memcpy(&memory->ram, &machine->pause.ram, sizeof(tic_ram));

		machine->data->start = machine->pause.time.start + machine->data->counter() - machine->pause.time.paused;
	}
}

void tic_close(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->state.initialized = false;

#if defined(TIC_BUILD_WITH_LUA)
	getLuaScriptConfig()->close(memory);

#	if defined(TIC_BUILD_WITH_MOON)
	getMoonScriptConfig()->close(memory);
#	endif

#	if defined(TIC_BUILD_WITH_FENNEL)
	getFennelConfig()->close(memory);
#	endif

#endif /* defined(TIC_BUILD_WITH_LUA) */


#if defined(TIC_BUILD_WITH_JS)
	getJsScriptConfig()->close(memory);
#endif

#if defined(TIC_BUILD_WITH_WREN)
	getWrenScriptConfig()->close(memory);
#endif

	blip_delete(machine->blip.left);
	blip_delete(machine->blip.right);

	free(memory->samples.buffer);
	free(machine);
}

///////////////////////////////////////////////////////////////////////////////
// API ////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void api_rect(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	drawRect(machine, x, y, width, height, color);
}

static void api_clear(tic_mem* memory, u8 color)
{
	static const Clip EmptyClip = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

	tic_machine* machine = (tic_machine*)memory;

	if(memcmp(&machine->state.clip, &EmptyClip, sizeof(Clip)) == 0)
	{
		color &= 0b00001111;
		memset(memory->ram.vram.screen.data, color | (color << TIC_PALETTE_BPP), sizeof(memory->ram.vram.screen.data));		
	}
	else
	{
		api_rect(memory, machine->state.clip.l, machine->state.clip.t, machine->state.clip.r - machine->state.clip.l, machine->state.clip.b - machine->state.clip.t, color);
	}
}

static s32 drawChar(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, bool alt)
{
	const u8* ptr = memory->font.data + (symbol + (alt ? TIC_FONT_CHARS / 2 : 0))*BITS_IN_BYTE;
	x += (BITS_IN_BYTE - 1)*scale;

	for(s32 i = 0, ys = y; i < TIC_FONT_HEIGHT; i++, ptr++, ys += scale)
		for(s32 col = BITS_IN_BYTE - (alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH), xs = x - col; col < BITS_IN_BYTE; col++, xs -= scale)
			if(*ptr & 1 << col)
				api_rect(memory, xs, ys, scale, scale, color);

	return (alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH)*scale;
}

static s32 api_draw_char(tic_mem* memory, u8 symbol, s32 x, s32 y, u8 color, bool alt)
{
	return drawChar(memory, symbol, x, y, alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1, alt);
}

s32 drawText(tic_mem* memory, const char* text, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, DrawCharFunc* func, bool alt)
{
	s32 pos = x;
	s32 max = x;
	char sym = 0;

	while((sym = *text++))
	{
		if(sym == '\n')
		{
			if(pos > max)
				max = pos;

			pos = x;
			y += height * scale;
		}
		else pos += func(memory, sym, pos, y, width, height, color, scale, alt);
	}

	return pos > max ? pos - x : max - x;
}

static s32 api_fixed_text(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool alt)
{
	return drawText(memory, text, x, y, alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1, drawChar, alt);
}

static s32 drawNonFixedChar(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, bool alt)
{
	const u8* ptr = memory->font.data + (symbol + (alt ? TIC_FONT_CHARS / 2 : 0))*BITS_IN_BYTE;

	const s32 FontWidth = alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH;

	s32 start = 0;
	s32 end = FontWidth;
	s32 i = 0;

	for(s32 col = 0; col < FontWidth; col++)
	{
		for(i = 0; i < TIC_FONT_HEIGHT; i++)
			if(*(ptr + i) & 0b10000000 >> col) break;

		if(i < TIC_FONT_HEIGHT)	break; else start++;
	}

	x -= start * scale;
	
	for(s32 col = FontWidth - 1; col >= start; col--)
	{
		for(i = 0; i < TIC_FONT_HEIGHT; i++)
			if(*(ptr + i) & 0b10000000 >> col) break;

		if(i < TIC_FONT_HEIGHT)	break; else end--;
	}

	for(s32 ys = y, i = 0; i < TIC_FONT_HEIGHT; i++, ptr++, ys += scale)
		for(s32 col = start, xs = x + start*scale; col < end; col++, xs += scale)
			if(*ptr & 0b10000000 >> col)
				api_rect(memory, xs, ys, scale, scale, color);

	s32 size = end - start;
	return (size ? size + 1 : FontWidth - 2) * scale;
}

static s32 api_text(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool alt)
{
	return drawText(memory, text, x, y, alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1, drawNonFixedChar, alt);
}

static s32 api_text_ex(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
	return drawText(memory, text, x, y, alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, scale, fixed ? drawChar : drawNonFixedChar, alt);
}

static void drawSprite(tic_mem* memory, const tic_tiles* src, s32 index, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
	if(index < TIC_SPRITES)
		drawTile((tic_machine*)memory, src->data + index, x, y, colors, count, scale, flip, rotate);
}

static void api_sprite_ex(tic_mem* memory, const tic_tiles* src, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
	s32 step = TIC_SPRITESIZE * scale;

	const tic_flip vert_horz_flip = tic_horz_flip | tic_vert_flip;

	for(s32 i = 0; i < w; i++)
	{
		for(s32 j = 0; j < h; j++)
		{
			s32 mx = i;
			s32 my = j;

			if(flip == tic_horz_flip || flip == vert_horz_flip) mx = w-1-i;
			if(flip == tic_vert_flip || flip == vert_horz_flip) my = h-1-j;
			
			if (rotate == tic_180_rotate)
			{
				mx = w-1-mx;
				my = h-1-my;			
			}
			else if(rotate == tic_90_rotate)
			{
				if(flip == tic_no_flip || flip == vert_horz_flip) my = h-1-my;
				else mx = w-1-mx;
			}
			else if(rotate == tic_270_rotate)
			{
				if(flip == tic_no_flip || flip == vert_horz_flip) mx = w-1-mx;
				else my = h-1-my;
			}

			enum {Cols = TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE};

			if(rotate==0 || rotate==2)
				drawSprite(memory, src, index + mx+my*Cols, x+i*step, y+j*step, colors, count, scale, flip, rotate);
			else
				drawSprite(memory, src, index + mx+my*Cols, x+j*step, y+i*step, colors, count, scale, flip, rotate);
		}
	}
}

s32 drawSpriteFont(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale, bool alt)
{
	api_sprite_ex(memory, &memory->ram.sprites, symbol, x, y, 1, 1, &chromakey, 1, scale, tic_no_flip, tic_no_rotate);

	return width * scale;
}

s32 drawFixedSpriteFont(tic_mem* memory, u8 index, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale, bool alt)
{
	const u8* ptr = memory->ram.sprites.data[index].data;

	enum {Size = TIC_SPRITESIZE};

	s32 start = 0;
	s32 end = Size;
	s32 i = 0;

	for(s32 col = 0; col < Size; col++)
	{
		for(i = 0; i < Size*Size; i += Size)
			if(tic_tool_peek4(ptr, col + i) != chromakey) break;

		if(i < Size*Size) break; else start++;
	}

	x -= start * scale;
	
	for(s32 col = Size - 1; col >= start; col--)
	{
		for(i = 0; i < Size*Size; i += Size)
			if(tic_tool_peek4(ptr, col + i) != chromakey) break;

		if(i < Size*Size) break; else end--;
	}

	for(s32 row = 0, ys = y; row < Size; row++, ys += scale)
	{
		for(s32 col = start, xs = x + start*scale; col < end; col++, xs += scale)
		{
			u8 color = tic_tool_peek4(ptr, col + row * Size);

			if(color != chromakey)
				api_rect(memory, xs, ys, scale, scale, color);
		}
	}

	s32 size = end - start;
	return (size ? size + 1 : width) * scale;
}

static void api_pixel(tic_mem* memory, s32 x, s32 y, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	setPixel(machine, x, y, color);
}

static u8 api_get_pixel(tic_mem* memory, s32 x, s32 y)
{
	tic_machine* machine = (tic_machine*)memory;

	return getPixel(machine, x, y);
}

static void api_rect_border(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	drawRectBorder(machine, x, y, width, height, color);
}

static struct
{
	s16 Left[TIC80_HEIGHT];
	s16 Right[TIC80_HEIGHT];	
	s32 ULeft[TIC80_HEIGHT];
	s32 VLeft[TIC80_HEIGHT];
} SidesBuffer;

static void initSidesBuffer()
{
	for(s32 i = 0; i < COUNT_OF(SidesBuffer.Left); i++)
		SidesBuffer.Left[i] = TIC80_WIDTH, SidesBuffer.Right[i] = -1;	
}

static void setSidePixel(s32 x, s32 y)
{
	if(y >= 0 && y < TIC80_HEIGHT)
	{
		if(x < SidesBuffer.Left[y]) SidesBuffer.Left[y] = x;
		if(x > SidesBuffer.Right[y]) SidesBuffer.Right[y] = x;
	}
}

static void setSideTexPixel(s32 x, s32 y, float u, float v)
{
	s32 yy = y;
	if (yy >= 0 && yy < TIC80_HEIGHT)
	{
		if (x < SidesBuffer.Left[yy])
		{
			SidesBuffer.Left[yy] = x;
			SidesBuffer.ULeft[yy] = u*65536.0f;
			SidesBuffer.VLeft[yy] = v*65536.0f;
		}
		if (x > SidesBuffer.Right[yy])
		{
			SidesBuffer.Right[yy] = x;
		}
	}
}

static void api_circle(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	initSidesBuffer();

	s32 r = radius;
	s32 x = -r, y = 0, err = 2-2*r;
	do 
	{
		setSidePixel(xm-x, ym+y);
		setSidePixel(xm-y, ym-x);
		setSidePixel(xm+x, ym-y);
		setSidePixel(xm+y, ym+x);

		r = err;
		if (r <= y) err += ++y*2+1;
		if (r > x || err > y) err += ++x*2+1;
	} while (x < 0);

	s32 yt = max(machine->state.clip.t, ym-radius);
	s32 yb = min(machine->state.clip.b, ym+radius+1);
	u8 final_color = mapColor(&machine->memory, color);
	for(s32 y = yt; y < yb; y++) {
		s32 xl = max(SidesBuffer.Left[y], machine->state.clip.l);
		s32 xr = min(SidesBuffer.Right[y]+1, machine->state.clip.r);
		machine->state.drawhline(&machine->memory, xl, xr, y, final_color);
	}
}

static void api_circle_border(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
	s32 r = radius;
	s32 x = -r, y = 0, err = 2-2*r;
	do {
		api_pixel(memory, xm-x, ym+y, color);
		api_pixel(memory, xm-y, ym-x, color);
		api_pixel(memory, xm+x, ym-y, color);
		api_pixel(memory, xm+y, ym+x, color);
		r = err;
		if (r <= y) err += ++y*2+1;
		if (r > x || err > y) err += ++x*2+1;
	} while (x < 0);
}

typedef void(*linePixelFunc)(tic_mem* memory, s32 x, s32 y, u8 color);
static void ticLine(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color, linePixelFunc func)
{
	s32 dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	s32 dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1; 
	s32 err = (dx > dy ? dx : -dy) / 2, e2;

	for(;;)
	{
		func(memory, x0, y0, color);
		if (x0 == x1 && y0 == y1) break;
		e2 = err;
		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

static void triPixelFunc(tic_mem* memory, s32 x, s32 y, u8 color)
{
	setSidePixel(x, y);
}

static void api_tri(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	initSidesBuffer();

	ticLine(memory, x1, y1, x2, y2, color, triPixelFunc);
	ticLine(memory, x2, y2, x3, y3, color, triPixelFunc);
	ticLine(memory, x3, y3, x1, y1, color, triPixelFunc);

	u8 final_color = mapColor(&machine->memory, color);
	s32 yt = max(machine->state.clip.t, min(y1, min(y2, y3)));
	s32 yb = min(machine->state.clip.b, max(y1, max(y2, y3)) + 1);

	for(s32 y = yt; y < yb; y++) {
		s32 xl = max(SidesBuffer.Left[y], machine->state.clip.l);
		s32 xr = min(SidesBuffer.Right[y]+1, machine->state.clip.r);
		machine->state.drawhline(&machine->memory, xl, xr, y, final_color);
	}
}


typedef struct
{
	float x, y, u, v;
} TexVert;


static void ticTexLine(tic_mem* memory, TexVert *v0, TexVert *v1)
{
	TexVert *top = v0;
	TexVert *bot = v1;

	if (bot->y < top->y)
	{
		top = v1;
		bot = v0;
	}

	float dy = bot->y - top->y;
	float step_x = (bot->x - top->x);
	float step_u = (bot->u - top->u);
	float step_v = (bot->v - top->v);

	if ((s32)dy != 0)
	{
		step_x /= dy;
		step_u /= dy;
		step_v /= dy;
	}

	float x = top->x;
	float y = top->y;
	float u = top->u;
	float v = top->v;

	if(y < .0f)
	{
		y = .0f - y;

		x += step_x * y;
		u += step_u * y;
		v += step_v * y;

		y = .0f;
	}

	s32 botY = bot->y;
	if(botY > TIC80_HEIGHT)
		botY = TIC80_HEIGHT;

	for (; y <botY; ++y)
	{
		setSideTexPixel(x, y, u, v);
		x += step_x;
		u += step_u;
		v += step_v;
	}
}

static void api_textri(tic_mem* memory, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8 chroma)
{
	tic_machine* machine = (tic_machine*)memory;
	TexVert V0, V1, V2;
	const u8* ptr = memory->ram.tiles.data[0].data;
	const u8* map = memory->ram.map.data;

	V0.x = x1; 	V0.y = y1; 	V0.u = u1; 	V0.v = v1;
	V1.x = x2; 	V1.y = y2; 	V1.u = u2; 	V1.v = v2;
	V2.x = x3; 	V2.y = y3; 	V2.u = u3; 	V2.v = v3;

	//	calculate the slope of the surface 
	//	use floats here 
	double denom = (V0.x - V2.x) * (V1.y - V2.y) - (V1.x - V2.x) * (V0.y - V2.y);
	if (denom == 0.0)
	{
		return;
	}
	double id = 1.0 / denom;
	float dudx, dvdx;
	//	this is the UV slope across the surface
	dudx = ((V0.u - V2.u) * (V1.y - V2.y) - (V1.u - V2.u) * (V0.y - V2.y)) * id;
	dvdx = ((V0.v - V2.v) * (V1.y - V2.y) - (V1.v - V2.v) * (V0.y - V2.y)) * id;
	//	convert to fixed
	s32 dudxs = dudx * 65536.0f;
	s32 dvdxs = dvdx * 65536.0f;
	//	fill the buffer 
	initSidesBuffer();
	//	parse each line and decide where in the buffer to store them ( left or right ) 
	ticTexLine(memory, &V0, &V1);
	ticTexLine(memory, &V1, &V2);
	ticTexLine(memory, &V2, &V0);

	for (s32 y = 0; y < TIC80_HEIGHT; y++)
	{
		//	if it's backwards skip it
		s32 width = SidesBuffer.Right[y] - SidesBuffer.Left[y];
		//	if it's off top or bottom , skip this line
		if ((y < machine->state.clip.t) || (y > machine->state.clip.b))
			width = 0;
		if (width > 0)
		{
			s32 u = SidesBuffer.ULeft[y];
			s32 v = SidesBuffer.VLeft[y];
			s32 left = SidesBuffer.Left[y];
			s32 right = SidesBuffer.Right[y];
			//	check right edge, and clamp it
			if (right > machine->state.clip.r)
				right = machine->state.clip.r;
			//	check left edge and offset UV's if we are off the left 
			if (left < machine->state.clip.l)
			{
				s32 dist = machine->state.clip.l - SidesBuffer.Left[y];
				u += dudxs * dist;
				v += dvdxs * dist;
				left = machine->state.clip.l;
			}
			//	are we drawing from the map . ok then at least check before the inner loop
			if (use_map == true)
			{
				for (s32 x = left; x < right; ++x)
				{
					enum { MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE, MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE };
					s32 iu = (u >> 16) % MapWidth;
					s32 iv = (v >> 16) % MapHeight;

					while (iu < 0) iu += MapWidth;
					while (iv < 0) iv += MapHeight;
					u8 tile = map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];
					const u8 *buffer = &ptr[tile << 5];
					u8 color = tic_tool_peek4(buffer, (iu & 7) + ((iv & 7) << 3));
					if (color != chroma)
						setPixel(machine, x, y, color);
					u += dudxs;
					v += dvdxs;
				}
			}
			else
			{
				//	direct from tile ram 
				for (s32 x = left; x < right; ++x)
				{
					enum{SheetWidth = TIC_SPRITESHEET_SIZE, SheetHeight = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS};
					s32 iu = (u>>16) & (SheetWidth - 1);
					s32 iv = (v>>16) & (SheetHeight - 1);
					const u8 *buffer = &ptr[((iu >> 3) + ((iv >> 3) << 4)) << 5];
					u8 color = tic_tool_peek4(buffer, (iu & 7) + ((iv & 7) << 3));
					if (color != chroma)
						setPixel(machine, x, y, color);
					u += dudxs;
					v += dvdxs;
				}
			}
		}
	}
}


static void api_sprite(tic_mem* memory, const tic_tiles* src, s32 index, s32 x, s32 y, u8* colors, s32 count)
{
	drawSprite(memory, src, index, x, y, colors, count, 1, tic_no_flip, tic_no_rotate);
}

static void api_map(tic_mem* memory, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale)
{
	drawMap((tic_machine*)memory, src, tiles, x, y, width, height, sx, sy, chromakey, scale, NULL, NULL);
}

static void api_remap(tic_mem* memory, const tic_map* src, const tic_tiles* tiles, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data)
{
	drawMap((tic_machine*)memory, src, tiles, x, y, width, height, sx, sy, chromakey, scale, remap, data);
}

static void api_map_set(tic_mem* memory, tic_map* src, s32 x, s32 y, u8 value)
{
	if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

	*(src->data + y * TIC_MAP_WIDTH + x) = value;
}

static u8 api_map_get(tic_mem* memory, const tic_map* src, s32 x, s32 y)
{
	if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;
	
	return *(src->data + y * TIC_MAP_WIDTH + x);
}

static void api_line(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color)
{
	ticLine(memory, x0, y0, x1, y1, color, api_pixel);
}

static s32 calcLoopPos(const tic_sound_loop* loop, s32 pos)
{
	s32 offset = 0;

	if(loop->size > 0)
	{
		for(s32 i = 0; i < pos; i++)
		{
			if(offset < (loop->start + loop->size-1))
				offset++;
			else offset = loop->start;
		}
	}
	else offset = pos >= SFX_TICKS ? SFX_TICKS - 1 : pos;

	return offset;
}

static void sfx(tic_mem* memory, s32 index, s32 freq, Channel* channel, tic_sound_register* reg, s32 channelIndex)
{
	tic_machine* machine = (tic_machine*)memory;

	if(channel->duration > 0)
		channel->duration--;

	if(index < 0 || channel->duration == 0)
	{
		resetSfx(channel);
		return;
	}

	const tic_sample* effect = &machine->sound.sfx->samples.data[index];
	s32 pos = ++channel->tick;

	s8 speed = channel->speed;

	if(speed)
	{
		if(speed > 0) pos *= 1 + speed;
		else pos /= 1 - speed;
	}

	for(s32 i = 0; i < sizeof(tic_sfx_pos); i++)
		*(channel->pos.data+i) = calcLoopPos(effect->loops + i, pos);

	u8 volume = (MAX_VOLUME - effect->data[channel->pos.volume].volume) * channel->volume / MAX_VOLUME;

	if(volume > 0)
	{
		s8 arp = effect->data[channel->pos.arpeggio].arpeggio * (effect->reverse ? -1 : 1);
	 
		if(arp) freq = note2freq(freq2note(freq)+arp);

		freq += effect->data[channel->pos.pitch].pitch * (effect->pitch16x ? 16 : 1);

		reg->freq = freq;
		reg->volume = volume;

		u8 wave = effect->data[channel->pos.wave].wave;
		const tic_waveform* waveform = &machine->sound.sfx->waveform.envelopes[wave];
		memcpy(reg->waveform.data, waveform->data, sizeof(tic_waveform));

		tic_tool_poke4(&memory->ram.stereo.data, channelIndex*2, MAX_VOLUME * effect->stereo_left);
		tic_tool_poke4(&memory->ram.stereo.data, channelIndex*2+1, MAX_VOLUME * effect->stereo_right);
	}
}

static void processMusic(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	if(memory->ram.sound_state.flag.music_state == tic_music_stop) return;

	const tic_track* track = &machine->sound.music->tracks.data[memory->ram.sound_state.music.track];
	s32 row = machine->state.music.ticks * (track->tempo + DEFAULT_TEMPO) * DEFAULT_SPEED / (track->speed + DEFAULT_SPEED) / NOTES_PER_MUNUTE;

	s32 rows = MUSIC_PATTERN_ROWS - track->rows;
	if (row >= rows)
	{
		row = 0;
		machine->state.music.ticks = 0;
		resetMusic(memory);

		if(memory->ram.sound_state.flag.music_state == tic_music_play)
		{
			memory->ram.sound_state.music.frame++;

			if(memory->ram.sound_state.music.frame >= MUSIC_FRAMES)
			{
				if(memory->ram.sound_state.flag.music_loop)
					memory->ram.sound_state.music.frame = 0;
				else
				{
					api_music(memory, -1, 0, 0, false);
					return;
				}
			}
			else
			{
				s32 val = 0;
				for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
					val += tic_tool_get_pattern_id(track, memory->ram.sound_state.music.frame, c);

				if(!val)
				{
					if(memory->ram.sound_state.flag.music_loop)
						memory->ram.sound_state.music.frame = 0;
					else
					{
						api_music(memory, -1, 0, 0, false);
						return;
					}					
				}
			}
		}
		else if(memory->ram.sound_state.flag.music_state == tic_music_play_frame)
		{
			if(!memory->ram.sound_state.flag.music_loop)
			{
				api_music(memory, -1, 0, 0, false);
				return;
			}
		}
	}

	if (row != memory->ram.sound_state.music.row)
	{
		memory->ram.sound_state.music.row = row;

		for (s32 channel = 0; channel < TIC_SOUND_CHANNELS; channel++)
		{
			s32 patternId = tic_tool_get_pattern_id(track, memory->ram.sound_state.music.frame, channel);
			if (!patternId) continue;

			const tic_track_pattern* pattern = &machine->sound.music->patterns.data[patternId - PATTERN_START];

			s32 note = pattern->rows[memory->ram.sound_state.music.row].note;

			if (note > NoteNone)
			{
				musicSfx(memory, -1, 0, 0, 0, channel);

				if (note >= NoteStart)
				{
					s32 octave = pattern->rows[memory->ram.sound_state.music.row].octave;
					s32 sfx = (pattern->rows[row].sfxhi << MUSIC_SFXID_LOW_BITS) | pattern->rows[row].sfxlow;
					s32 volume = pattern->rows[memory->ram.sound_state.music.row].volume;
					musicSfx(memory, sfx, note - NoteStart, octave, volume, channel);
				}
			}
		}
	}

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		Channel* c = &machine->state.music.channels[i];
		
		if(c->index >= 0)
			sfx(memory, c->index, c->freq, c, &memory->ram.registers[i], i);
	}

	machine->state.music.ticks++;
}

static bool isNoiseWaveform(const tic_waveform* wave)
{
	static const tic_waveform NoiseWave = {.data = {0}};

	return memcmp(&NoiseWave.data, &wave->data, sizeof(tic_waveform)) == 0;
}

static bool isKeyPressed(const tic80_keyboard* input, tic_key key)
{
	for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
		if(input->keys[i] == key)
			return true;

	return false;
}

static void api_tick_start(tic_mem* memory, const tic_sfx* sfxsrc, const tic_music* music)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->sound.sfx = sfxsrc;
	machine->sound.music = music;

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
		memset(&memory->ram.registers[i], 0, sizeof(tic_sound_register));

	memory->ram.stereo.data = 0;

	processMusic(memory);

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		Channel* c = &machine->state.channels[i];
		
		if(c->index >= 0)
			sfx(memory, c->index, c->freq, c, &memory->ram.registers[i], i);
	}

	// process gamepad
	for(s32 i = 0; i < COUNT_OF(machine->state.gamepads.holds); i++)
	{
		u32 mask = 1 << i;
		u32 prevDown = machine->state.gamepads.previous.data & mask;
		u32 down = memory->ram.input.gamepads.data & mask;

		u32* hold = &machine->state.gamepads.holds[i];
		if(prevDown && prevDown == down) (*hold)++;
		else *hold = 0;
	}

	// process keyboard
	for(s32 i = 0; i < tic_keys_count; i++)
	{
		bool prevDown = isKeyPressed(&machine->state.keyboard.previous, i);
		bool down = isKeyPressed(&memory->ram.input.keyboard, i);

		u32* hold = &machine->state.keyboard.holds[i];

		if(prevDown && down) (*hold)++;
		else *hold = 0;
	}

	machine->state.setpix = setPixelDma;
	machine->state.getpix = getPixelDma;
	machine->state.synced = 0;
	machine->state.drawhline = drawHLineDma;
}

static void stereo_tick_end(tic_mem* memory, tic_sound_register_data* registers, blip_buffer_t* blip, u8 stereoRight)
{
	enum {EndTime = CLOCKRATE / TIC_FRAMERATE};
	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		u8 masterVol = MAX_VOLUME - tic_tool_peek4(&memory->ram.stereo.data, stereoRight + i*2);

		tic_sound_register* reg = &memory->ram.registers[i];
		tic_sound_register_data* data = registers + i;

		isNoiseWaveform(&reg->waveform)
			? runNoise(blip, reg, data, EndTime, masterVol)
			: runEnvelope(blip, reg, data, EndTime, masterVol);

		data->time -= EndTime;
	}
	
	blip_end_frame(blip, EndTime);
}

static void api_tick_end(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->state.gamepads.previous.data = machine->memory.ram.input.gamepads.data;
	machine->state.keyboard.previous.data = machine->memory.ram.input.keyboard.data;

	stereo_tick_end(memory, machine->state.registers.left, machine->blip.left, 0);
	stereo_tick_end(memory, machine->state.registers.right, machine->blip.right, 1);

	blip_read_samples(machine->blip.left, machine->memory.samples.buffer, machine->samplerate / TIC_FRAMERATE, TIC_STEREO_CHANNLES);
	blip_read_samples(machine->blip.right, machine->memory.samples.buffer + 1, machine->samplerate / TIC_FRAMERATE, TIC_STEREO_CHANNLES);

	machine->state.setpix = setPixelOvr;
	machine->state.getpix = getPixelOvr;
	machine->state.drawhline = drawHLineOvr;
}


static tic_sfx_pos api_sfx_pos(tic_mem* memory, s32 channel)
{
	tic_machine* machine = (tic_machine*)memory;

	Channel* c = &machine->state.channels[channel];

	return c->pos;
}

static void api_sfx_ex(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 volume, s32 speed)
{
	tic_machine* machine = (tic_machine*)memory;
	channelSfx(memory, index, note, octave, duration, &machine->state.channels[channel], volume, speed);
}

static void api_sfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel)
{
	api_sfx_ex(memory, index, note, octave, duration, channel, MAX_VOLUME, SFX_DEF_SPEED);
}

static void api_sfx_stop(tic_mem* memory, s32 channel)
{
	api_sfx(memory, -1, 0, 0, -1, channel);
}

static void api_music_frame(tic_mem* memory, s32 index, s32 frame, s32 row, bool loop)
{
	tic_machine* machine = (tic_machine*)memory;

	setMusic(machine, index, frame, row, loop);

	if(index >= 0)
		memory->ram.sound_state.flag.music_state = tic_music_play_frame;
}

static void initCover(tic_mem* tic)
{
	const tic_cover_image* cover = &tic->cart.cover;

	if(cover->size)
	{
		gif_image* image = gif_read_data(cover->data, cover->size);

		if (image)
		{
			if (image->width == TIC80_WIDTH && image->height == TIC80_HEIGHT)
			{
				enum { Size = TIC80_WIDTH * TIC80_HEIGHT };

				for (s32 i = 0; i < Size; i++)
				{
					const gif_color* c = &image->palette[image->buffer[i]];
					tic_rgb rgb = { c->r, c->g, c->b };
					u8 color = tic_tool_find_closest_color(tic->cart.bank0.palette.colors, &rgb);
					tic_tool_poke4(tic->ram.vram.screen.data, i, color);
				}
			}

			gif_close(image);
		}
	}
}

static void api_sync(tic_mem* tic, u32 mask, s32 bank, bool toCart)
{
	tic_machine* machine = (tic_machine*)tic;

	static const struct {s32 bank; s32 ram; s32 size;} Sections[] = 
	{
		{offsetof(tic_bank, tiles), 	offsetof(tic_ram, tiles), 			sizeof(tic_tiles)	},
		{offsetof(tic_bank, sprites),	offsetof(tic_ram, sprites), 		sizeof(tic_tiles)	},
		{offsetof(tic_bank, map), 		offsetof(tic_ram, map), 			sizeof(tic_map)		},
		{offsetof(tic_bank, sfx), 		offsetof(tic_ram, sfx), 			sizeof(tic_sfx)		},
		{offsetof(tic_bank, music), 	offsetof(tic_ram, music), 			sizeof(tic_music)	},
		{offsetof(tic_bank, palette), 	offsetof(tic_ram, vram.palette), 	sizeof(tic_palette)	},
	};

	enum{Count = COUNT_OF(Sections), Mask = (1 << Count) - 1};

	if(mask == 0) mask = Mask;
	
	mask &= ~machine->state.synced & Mask;

	assert(bank >= 0 && bank < TIC_BANKS);

	for(s32 i = 0; i < Count; i++)
	{
		if(mask & (1 << i))
			toCart
				? memcpy((u8*)&tic->cart.banks[bank] + Sections[i].bank, (u8*)&tic->ram + Sections[i].ram, Sections[i].size)
				: memcpy((u8*)&tic->ram + Sections[i].ram, (u8*)&tic->cart.banks[bank] + Sections[i].bank, Sections[i].size);
	}

	machine->state.synced |= mask;
}

static void cart2ram(tic_mem* memory)
{
	api_sync(memory, 0, 0, false);

	initCover(memory);
}

static const char* readMetatag(const char* code, const char* tag, const char* comment)
{
	const char* start = NULL;

	{
		static char format[] = "%s %s:";

		char* tagBuffer = malloc(strlen(format) + strlen(tag));

		if(tagBuffer)
		{
			sprintf(tagBuffer, format, comment, tag);
			if((start = strstr(code, tagBuffer)))
				start += strlen(tagBuffer);
			free(tagBuffer);			
		}
	}

	if(start)
	{
		const char* end = strstr(start, "\n");

		if(end)
		{
			while(*start <= ' ' && start < end) start++;
			while(*(end-1) <= ' ' && end > start) end--;

			const s32 size = (s32)(end-start);

			char* value = (char*)malloc(size+1);

			if(value)
			{
				memset(value, 0, size+1);
				memcpy(value, start, size);

				return value;				
			}
		}
	}

	return NULL;
}

static bool compareMetatag(const char* code, const char* tag, const char* value, const char* comment)
{
	bool result = false;

	const char* str = readMetatag(code, tag, comment);

	if(str)
	{
		result = strcmp(str, value) == 0;
		free((void*)str);
	}

	return result;
}

static const tic_script_config* getScriptConfig(const char* code)
{
#if defined(TIC_BUILD_WITH_MOON)
	if(compareMetatag(code, "script", "moon", getMoonScriptConfig()->singleComment) ||
		compareMetatag(code, "script", "moonscript", getMoonScriptConfig()->singleComment)) 
		return getMoonScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_FENNEL)
	if(compareMetatag(code, "script", "fennel", getFennelConfig()->singleComment))
		return getFennelConfig();
#endif

#if defined(TIC_BUILD_WITH_JS)
	if(compareMetatag(code, "script", "js", getJsScriptConfig()->singleComment) ||
		compareMetatag(code, "script", "javascript", getJsScriptConfig()->singleComment)) 
		return getJsScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_WREN)
	if(compareMetatag(code, "script", "wren", getWrenScriptConfig()->singleComment)) 
		return getWrenScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_LUA)
	return getLuaScriptConfig();
#elif defined(TIC_BUILD_WITH_JS)
	return getJsScriptConfig();
#elif defined(TIC_BUILD_WITH_WREN)
	return getWrenScriptConfig();
#endif
}

static const tic_script_config* api_get_script_config(tic_mem* memory)
{
	return getScriptConfig(memory->cart.code.data);
}

static void updateSaveid(tic_mem* memory)
{
	memset(memory->saveid, 0, sizeof memory->saveid);
	const char* saveid = readMetatag(memory->cart.code.data, "saveid", api_get_script_config(memory)->singleComment);
	if(saveid)
	{
		strncpy(memory->saveid, saveid, TIC_SAVEID_SIZE-1);
		free((void*)saveid);
	}
}

static void api_tick(tic_mem* tic, tic_tick_data* data)
{
	tic_machine* machine = (tic_machine*)tic;

	machine->data = data;
	
	if(!machine->state.initialized)
	{
		enum{CodeSize = sizeof(tic_code)};
		char* code = malloc(CodeSize);

		if(code)
		{
			memset(code, 0, CodeSize);
			strcpy(code, tic->cart.code.data);

			if(data->preprocessor)
				data->preprocessor(data->data, code);

			bool done = false;
			const tic_script_config* config = NULL;

			if(strlen(code))
			{
				config = getScriptConfig(code);
				cart2ram(tic);

				machine->state.synced = 0;
				tic->input.data = 0;
				
				if(compareMetatag(code, "input", "mouse", config->singleComment))
					tic->input.mouse = 1;
				else if(compareMetatag(code, "input", "gamepad", config->singleComment))
					tic->input.gamepad = 1;
				else if(compareMetatag(code, "input", "keyboard", config->singleComment))
					tic->input.keyboard = 1;
				else tic->input.data = -1;  // default is all enabled

				data->start = data->counter();
				
				done = config->init(tic, code);
			}
			else
			{
				machine->data->error(machine->data->data, "the code is empty");
			}

			free(code);

			if(done)
			{
				machine->state.tick = config->tick;
				machine->state.scanline = config->scanline;
				machine->state.ovr.callback = config->overline;

				machine->state.initialized = true;
			}
			else return;
		}
	}

	machine->state.tick(tic);
}

static void api_scanline(tic_mem* memory, s32 row, void* data)
{
	tic_machine* machine = (tic_machine*)memory;

	if(machine->state.initialized)
		machine->state.scanline(memory, row, data);
}

static void api_overline(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;

	if(machine->state.initialized)
		machine->state.ovr.callback(memory, data);
}

static double api_time(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;
	return (double)((machine->data->counter() - machine->data->start)*1000)/machine->data->freq();
}

static u32 api_btnp(tic_mem* tic, s32 index, s32 hold, s32 period)
{
	tic_machine* machine = (tic_machine*)tic;

	if(index < 0)
	{
		return (~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data;
	}
	else if(hold < 0 || period < 0)
	{
		return ((~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data) & (1 << index);
	}

	tic80_gamepads previous;
	
	previous.data = machine->state.gamepads.holds[index] >= hold 
		? period && machine->state.gamepads.holds[index] % period ? machine->state.gamepads.previous.data : 0
		: machine->state.gamepads.previous.data;

	return ((~previous.data) & machine->memory.ram.input.gamepads.data) & (1 << index);
}

static bool api_key(tic_mem* tic, tic_key key)
{
	return key > tic_key_unknown 
		? isKeyPressed(&tic->ram.input.keyboard, key) 
		: tic->ram.input.keyboard.data;
}

static bool api_keyp(tic_mem* tic, tic_key key, s32 hold, s32 period)
{
	tic_machine* machine = (tic_machine*)tic;

	if(key > tic_key_unknown)
	{			
		bool prevDown = hold >= 0 && period >= 0 && machine->state.keyboard.holds[key] >= hold
			? period && machine->state.keyboard.holds[key] % period 
				? isKeyPressed(&machine->state.keyboard.previous, key) 
				: false
			: isKeyPressed(&machine->state.keyboard.previous, key);

		bool down = isKeyPressed(&tic->ram.input.keyboard, key);

		return !prevDown && down;
	}

	for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
	{
		tic_key key = tic->ram.input.keyboard.keys[i];

		if(key)
		{
			bool wasPressed = false;

			for(s32 p = 0; p < TIC80_KEY_BUFFER; p++)
			{
				if(machine->state.keyboard.previous.keys[p] == key)
				{
					wasPressed = true;
					break;
				}
			}

			if(!wasPressed)
				return true;
		}
	}

	return false;
}

static void api_load(tic_cartridge* cart, const u8* buffer, s32 size, bool palette)
{
	const u8* end = buffer + size;
	memset(cart, 0, sizeof(tic_cartridge));

	if(palette)
	{
		static const u8 DB16[] = {0x14, 0x0c, 0x1c, 0x44, 0x24, 0x34, 0x30, 0x34, 0x6d, 0x4e, 0x4a, 0x4e, 0x85, 0x4c, 0x30, 0x34, 0x65, 0x24, 0xd0, 0x46, 0x48, 0x75, 0x71, 0x61, 0x59, 0x7d, 0xce, 0xd2, 0x7d, 0x2c, 0x85, 0x95, 0xa1, 0x6d, 0xaa, 0x2c, 0xd2, 0xaa, 0x99, 0x6d, 0xc2, 0xca, 0xda, 0xd4, 0x5e, 0xde, 0xee, 0xd6};

		memcpy(cart->bank0.palette.data, DB16, sizeof(tic_palette));
	}

	#define LOAD_CHUNK(to) memcpy(&to, buffer, min(sizeof(to), chunk.size))

	while(buffer < end)
	{
		Chunk chunk;
		memcpy(&chunk, buffer, sizeof(Chunk));
		buffer += sizeof(Chunk);

		switch(chunk.type)
		{
		case CHUNK_TILES: 		LOAD_CHUNK(cart->banks[chunk.bank].tiles); 			break;
		case CHUNK_SPRITES: 	LOAD_CHUNK(cart->banks[chunk.bank].sprites); 		break;
		case CHUNK_MAP: 		LOAD_CHUNK(cart->banks[chunk.bank].map); 			break;
		case CHUNK_SAMPLES: 	LOAD_CHUNK(cart->banks[chunk.bank].sfx.samples); 	break;
		case CHUNK_WAVEFORM:	LOAD_CHUNK(cart->banks[chunk.bank].sfx.waveform); 	break;
		case CHUNK_MUSIC:		LOAD_CHUNK(cart->banks[chunk.bank].music.tracks); 	break;
		case CHUNK_PATTERNS:	LOAD_CHUNK(cart->banks[chunk.bank].music.patterns);	break;
		case CHUNK_PALETTE:		LOAD_CHUNK(cart->banks[chunk.bank].palette);		break;
		case CHUNK_CODE: 		
			if(chunk.bank == 0)
				LOAD_CHUNK(cart->code);
			break;
		case CHUNK_COVER: 	
			LOAD_CHUNK(cart->cover.data);
			cart->cover.size = chunk.size;
			break;
		default: break;
		}

		buffer += chunk.size;
	}

	#undef LOAD_CHUNK
}


static s32 calcBufferSize(const void* buffer, s32 size)
{
	const u8* ptr = (u8*)buffer + size - 1;
	const u8* end = (u8*)buffer;

	while(ptr >= end)
	{
		if(*ptr) break;

		ptr--;
		size--;
	}

	return size;
}

static u8* saveFixedChunk(u8* buffer, ChunkType type, const void* from, s32 size, s32 bank)
{
	if(size)
	{
		Chunk chunk = {.type = type, .bank = bank, .size = size, .temp = 0};
		memcpy(buffer, &chunk, sizeof(Chunk));
		buffer += sizeof(Chunk);
		memcpy(buffer, from, size);
		buffer += size;
	}

	return buffer;
}

static u8* saveChunk(u8* buffer, ChunkType type, const void* from, s32 size, s32 bank)
{
	s32 chunkSize = calcBufferSize(from, size);

	return saveFixedChunk(buffer, type, from, chunkSize, bank);
}

static s32 api_save(const tic_cartridge* cart, u8* buffer)
{
	u8* start = buffer;

	#define SAVE_CHUNK(ID, FROM, BANK) saveChunk(buffer, ID, &FROM, sizeof(FROM), BANK)

	for(s32 i = 0; i < TIC_BANKS; i++)
	{
		buffer = SAVE_CHUNK(CHUNK_TILES, 	cart->banks[i].tiles, 			i);
		buffer = SAVE_CHUNK(CHUNK_SPRITES, 	cart->banks[i].sprites, 		i);
		buffer = SAVE_CHUNK(CHUNK_MAP, 		cart->banks[i].map, 			i);
		buffer = SAVE_CHUNK(CHUNK_SAMPLES, 	cart->banks[i].sfx.samples, 	i);
		buffer = SAVE_CHUNK(CHUNK_WAVEFORM, cart->banks[i].sfx.waveform, 	i);
		buffer = SAVE_CHUNK(CHUNK_PATTERNS, cart->banks[i].music.patterns, 	i);
		buffer = SAVE_CHUNK(CHUNK_MUSIC, 	cart->banks[i].music.tracks, 	i);
		buffer = SAVE_CHUNK(CHUNK_PALETTE, 	cart->banks[i].palette, 		i);
	}

	buffer = SAVE_CHUNK(CHUNK_CODE, cart->code, 0);
	buffer = saveFixedChunk(buffer, CHUNK_COVER, cart->cover.data, cart->cover.size, 0);

	#undef SAVE_CHUNK

	return (s32)(buffer - start);
}

// copied from SDL2
static inline void memset4(void *dst, u32 val, u32 dwords)
{
#if defined(__GNUC__) && defined(i386)
	s32 u0, u1, u2;
	__asm__ __volatile__ (
		"cld \n\t"
		"rep ; stosl \n\t"
		: "=&D" (u0), "=&a" (u1), "=&c" (u2)
		: "0" (dst), "1" (val), "2" (dwords)
		: "memory"
	);
#else
	u32 _n = (dwords + 3) / 4;
	u32 *_p = (u32*)dst;
	u32 _val = (val);
	if (dwords == 0)
		return;
	switch (dwords % 4)
	{
		case 0: do {    *_p++ = _val;
		case 3:         *_p++ = _val;
		case 2:         *_p++ = _val;
		case 1:         *_p++ = _val;
		} while ( --_n );
	}
#endif
}

static void api_blit(tic_mem* tic, tic_scanline scanline, tic_overline overline, void* data)
{
	const u32* pal = tic_palette_blit(&tic->ram.vram.palette);

	{
		tic_machine* machine = (tic_machine*)tic;
		memcpy(machine->state.ovr.palette, pal, sizeof machine->state.ovr.palette);
	}

	if(scanline)
	{
		scanline(tic, 0, data);
		pal = tic_palette_blit(&tic->ram.vram.palette);
	}

	enum {Top = (TIC80_FULLHEIGHT-TIC80_HEIGHT)/2, Bottom = Top};
	enum {Left = (TIC80_FULLWIDTH-TIC80_WIDTH)/2, Right = Left};

	u32* out = tic->screen;

	memset4(&out[0 * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH*Top);

	u32* rowPtr = out + (Top*TIC80_FULLWIDTH);
	for(s32 r = 0; r < TIC80_HEIGHT; r++, rowPtr += TIC80_FULLWIDTH)
	{
		u32 *colPtr = rowPtr + Left;
		memset4(rowPtr, pal[tic->ram.vram.vars.border], Left);

		s32 pos = (r + tic->ram.vram.vars.offset.y + TIC80_HEIGHT) % TIC80_HEIGHT * TIC80_WIDTH >> 1;

		u32 x = (-tic->ram.vram.vars.offset.x + TIC80_WIDTH) % TIC80_WIDTH;
		for(s32 c = 0; c < TIC80_WIDTH / 2; c++)
		{
			u8 val = ((u8*)tic->ram.vram.screen.data)[pos + c];
			*(colPtr + (x++ % TIC80_WIDTH)) = pal[val & 0xf];
			*(colPtr + (x++ % TIC80_WIDTH)) = pal[val >> 4];
		}

		memset4(rowPtr + (TIC80_FULLWIDTH-Right), pal[tic->ram.vram.vars.border], Right);
			
		if(scanline && (r < TIC80_HEIGHT-1))
		{
			scanline(tic, r+1, data);
			pal = tic_palette_blit(&tic->ram.vram.palette);
		}
	}

	memset4(&out[(TIC80_FULLHEIGHT-Bottom) * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH*Bottom);

	if(overline)
		overline(tic, data);
}

static void initApi(tic_api* api)
{
#define INIT_API(func) api->func = api_##func

	INIT_API(draw_char);
	INIT_API(text);
	INIT_API(fixed_text);
	INIT_API(text_ex);
	INIT_API(clear);
	INIT_API(pixel);
	INIT_API(get_pixel);
	INIT_API(line);
	INIT_API(rect);
	INIT_API(rect_border);
	INIT_API(sprite);
	INIT_API(sprite_ex);
	INIT_API(map);
	INIT_API(remap);
	INIT_API(map_set);
	INIT_API(map_get);
	INIT_API(circle);
	INIT_API(circle_border);
	INIT_API(tri);
	INIT_API(textri);
	INIT_API(clip);
	INIT_API(sfx);
	INIT_API(sfx_stop);
	INIT_API(sfx_ex);
	INIT_API(sfx_pos);
	INIT_API(music);
	INIT_API(music_frame);
	INIT_API(time);
	INIT_API(tick);
	INIT_API(scanline);
	INIT_API(overline);
	INIT_API(reset);
	INIT_API(pause);
	INIT_API(resume);
	INIT_API(sync);
	INIT_API(btnp);
	INIT_API(key);
	INIT_API(keyp);
	INIT_API(load);
	INIT_API(save);
	INIT_API(tick_start);
	INIT_API(tick_end);
	INIT_API(blit);

	INIT_API(get_script_config);

#undef INIT_API
}

tic_mem* tic_create(s32 samplerate)
{
	tic_machine* machine = (tic_machine*)malloc(sizeof(tic_machine));
	memset(machine, 0, sizeof(tic_machine));

	if(machine != (tic_machine*)&machine->memory)
		return NULL;

	machine->sound.sfx = &machine->memory.ram.sfx;
	machine->sound.music = &machine->memory.ram.music;

	initApi(&machine->memory.api);

	machine->samplerate = samplerate;
	machine->memory.samples.size = samplerate * TIC_STEREO_CHANNLES / TIC_FRAMERATE * sizeof(s16);
	machine->memory.samples.buffer = malloc(machine->memory.samples.size);

	machine->blip.left = blip_new(samplerate / 10);
	machine->blip.right = blip_new(samplerate / 10);

	blip_set_rates(machine->blip.left, CLOCKRATE, samplerate);
	blip_set_rates(machine->blip.right, CLOCKRATE, samplerate);

	machine->memory.api.reset(&machine->memory);

	return &machine->memory;
}

static inline bool islineend(char c) {return c == '\n' || c == '\0';}
static inline bool isalpha_(char c) {return isalpha(c) || c == '_';}
static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

void parseCode(const tic_script_config* config, const char* start, u8* color, const tic_code_theme* theme)
{
	const char* ptr = start;

	const char* blockCommentStart = NULL;
	const char* blockStringStart = NULL;
	const char* blockStdStringStart = NULL;
	const char* singleCommentStart = NULL;
	const char* wordStart = NULL;
	const char* numberStart = NULL;

	while(true)
	{
		char c = ptr[0];

		if(blockCommentStart)
		{
			const char* end = strstr(ptr, config->blockCommentEnd);

			ptr = end ? end + strlen(config->blockCommentEnd) : blockCommentStart + strlen(blockCommentStart);
			memset(color + (blockCommentStart - start), theme->comment, ptr - blockCommentStart);
			blockCommentStart = NULL;
			continue;
		}
		else if(blockStringStart)
		{
			const char* end = strstr(ptr, config->blockStringEnd);

			ptr = end ? end + strlen(config->blockStringEnd) : blockStringStart + strlen(blockStringStart);
			memset(color + (blockStringStart - start), theme->string, ptr - blockStringStart);
			blockStringStart = NULL;
			continue;
		}
		else if(blockStdStringStart)
		{
			const char* blockStart = blockStdStringStart+1;

			while(true)
			{
				const char* pos = strchr(blockStart, *blockStdStringStart);
				
				if(pos)
				{
					if(*(pos-1) == '\\' && *(pos-2) != '\\') blockStart = pos + 1;
					else
					{
						ptr = pos + 1;
						break;
					}
				}
				else
				{
					ptr = blockStdStringStart + strlen(blockStdStringStart);
					break;
				}
			}

			memset(color + (blockStdStringStart - start), theme->string, ptr - blockStdStringStart);
			blockStdStringStart = NULL;
			continue;
		}
		else if(singleCommentStart)
		{
			while(!islineend(*ptr))ptr++;

			memset(color + (singleCommentStart - start), theme->comment, ptr - singleCommentStart);
			singleCommentStart = NULL;
			continue;
		}
		else if(wordStart)
		{
			while(!islineend(*ptr) && isalnum_(*ptr)) ptr++;

			s32 len = ptr - wordStart;
			bool keyword = false;
			{
				for(s32 i = 0; i < config->keywordsCount; i++)
					if(len == strlen(config->keywords[i]) && memcmp(wordStart, config->keywords[i], len) == 0)
					{
						memset(color + (wordStart - start), theme->keyword, len);
						keyword = true;
						break;
					}
			}

			if(!keyword)
			{
				for(s32 i = 0; i < config->apiCount; i++)
					if(len == strlen(config->api[i]) && memcmp(wordStart, config->api[i], len) == 0)
					{
						memset(color + (wordStart - start), theme->api, len);
						break;
					}
			}

			wordStart = NULL;
			continue;
		}
		else if(numberStart)
		{
			while(!islineend(*ptr))
			{
				char c = *ptr;

				if(isdigit(c)) ptr++;
				else if(numberStart[0] == '0' 
					&& (numberStart[1] == 'x' || numberStart[1] == 'X') 
					&& isxdigit(numberStart[2]))
				{
					if((ptr - numberStart < 2) || (ptr - numberStart >= 2 && isxdigit(c))) ptr++;
					else break;
				}
				else if(c == '.' || c == 'e' || c == 'E')
				{
					if(isdigit(ptr[1])) ptr++;
					else break;
				}
				else break;
			}

			memset(color + (numberStart - start), theme->number, ptr - numberStart);
			numberStart = NULL;
			continue;
		}
		else
		{
			if(config->blockCommentStart && memcmp(ptr, config->blockCommentStart, strlen(config->blockCommentStart)) == 0)
			{
				blockCommentStart = ptr;
				ptr += strlen(config->blockCommentStart);
				continue;
			}
			if(config->blockStringStart && memcmp(ptr, config->blockStringStart, strlen(config->blockStringStart)) == 0)
			{
				blockStringStart = ptr;
				ptr += strlen(config->blockStringStart);
				continue;
			}
			else if(c == '"' || c == '\'')
			{
				blockStdStringStart = ptr;
				ptr++;
				continue;
			}
			else if(config->singleComment && memcmp(ptr, config->singleComment, strlen(config->singleComment)) == 0)
			{
				singleCommentStart = ptr;
				ptr += strlen(config->singleComment);
				continue;
			}
			else if(isalpha_(c))
			{
				wordStart = ptr;
				ptr++;
				continue;
			}
			else if(isdigit(c) || (c == '.' && isdigit(ptr[1])))
			{
				numberStart = ptr;
				ptr++;
				continue;
			}
			else if(ispunct(c)) color[ptr - start] = theme->sign;
			else if(iscntrl(c)) color[ptr - start] = theme->other;
		}

		if(!c) break;

		ptr++;
	}
}
