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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "ticapi.h"
#include "tools.h"
#include "machine.h"
#include "ext/gif.h"

#define CLOCKRATE (TIC_FRAMERATE*30000)
#define MIN_PERIOD_VALUE 10
#define MAX_PERIOD_VALUE 4096
#define BASE_NOTE_FREQ 440.0
#define BASE_NOTE_POS 49
#define ENVELOPE_FREQ_SCALE 2
#define NOTES_PER_MUNUTE (TIC_FRAMERATE / NOTES_PER_BEET * 60)
#define min(a,b) (a < b ? a : b)

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
	CHUNK_SOUND,	// 9
	CHUNK_WAVEFORM,	// 10
	CHUNK_OLDMUSIC,	// 11
	CHUNK_PALETTE, 	// 12
	CHUNK_PATTERNS, // 13
	CHUNK_MUSIC,	// 14

} ChunkType;

typedef struct
{
	ChunkType type:8;
	u32 size:24;
} Chunk;

STATIC_ASSERT(rom_chunk_size, sizeof(Chunk) == 4);
STATIC_ASSERT(tic_map, sizeof(tic_map) < 1024*32);
STATIC_ASSERT(tic_sound_effect, sizeof(tic_sound_effect) == 66);
STATIC_ASSERT(tic_track_pattern, sizeof(tic_track_pattern) == 3*MUSIC_PATTERN_ROWS);
STATIC_ASSERT(tic_track, sizeof(tic_track) == 3*MUSIC_FRAMES+3);
STATIC_ASSERT(tic_vram, sizeof(tic_vram) == TIC_VRAM_SIZE);
STATIC_ASSERT(tic_ram, sizeof(tic_ram) == TIC_RAM_SIZE);
STATIC_ASSERT(tic_sound_register, sizeof(tic_sound_register) == 16+2);
STATIC_ASSERT(tic80_input, sizeof(tic80_input) == 2);

static void update_amp(blip_buffer_t* blip, tic_sound_register_data* data, s32 new_amp )
{
	s32 delta = new_amp - data->amp;
	data->amp += delta;
	blip_add_delta( blip, data->time, delta );
}

inline s32 freq2note(double freq)
{
	return (s32)round((double)NOTES * log2(freq / BASE_NOTE_FREQ)) + BASE_NOTE_POS;
}

inline double note2freq(s32 note)
{
	return pow(2, (note - BASE_NOTE_POS) / (double)NOTES) * BASE_NOTE_FREQ;
}

inline s32 freq2period(double freq)
{
	if(freq == 0.0) return MAX_PERIOD_VALUE;

	enum {Rate = CLOCKRATE * ENVELOPE_FREQ_SCALE / ENVELOPE_VALUES};
	s32 period = (s32)round(Rate / freq - 1.0);

	if(period < MIN_PERIOD_VALUE) return MIN_PERIOD_VALUE;
	if(period > MAX_PERIOD_VALUE) return MAX_PERIOD_VALUE;

	return period;
}

inline s32 getAmp(const tic_sound_register* reg, s32 amp)
{
	enum {MaxAmp = (u16)-1 / (MAX_VOLUME * TIC_SOUND_CHANNELS)};

	return amp * MaxAmp * reg->volume / MAX_VOLUME;
}

static void runEnvelope(blip_buffer_t* blip, tic_sound_register* reg, tic_sound_register_data* data, s32 end_time )
{
	s32 period = freq2period(reg->freq * ENVELOPE_FREQ_SCALE);

	for ( ; data->time < end_time; data->time += period )
	{
		data->phase = (data->phase + 1) % ENVELOPE_VALUES;

		update_amp(blip, data, getAmp(reg, tic_tool_peek4(reg->waveform.data, data->phase)));
	}
}

static void runNoise(blip_buffer_t* blip, tic_sound_register* reg, tic_sound_register_data* data, s32 end_time )
{
	// phase is noise LFSR, which must never be zero 
	if ( data->phase == 0 )
		data->phase = 1;
	
	s32 period = freq2period(reg->freq);

	for ( ; data->time < end_time; data->time += period )
	{
		data->phase = ((data->phase & 1) * (0b11 << 13)) ^ (data->phase >> 1);
		update_amp(blip, data, getAmp(reg, (data->phase & 1) ? MAX_VOLUME : 0));
	}
}

static void resetPalette(tic_mem* memory)
{
	static const u8 DefaultMapping[] = {16, 50, 84, 118, 152, 186, 220, 254};
	memcpy(memory->ram.vram.palette.data, memory->cart.palette.data, sizeof(tic_palette));
	memcpy(memory->ram.vram.mapping, DefaultMapping, sizeof DefaultMapping);
	memset(&memory->ram.vram.vars, 0, sizeof memory->ram.vram.vars);
	memory->ram.vram.vars.mask.data = TIC_GAMEPAD_MASK;
}

static void setPixel(tic_machine* machine, s32 x, s32 y, u8 color)
{
	if(x < machine->state.clip.l || y < machine->state.clip.t || x >= machine->state.clip.r || y >= machine->state.clip.b) return;

	tic_tool_poke4(machine->memory.ram.vram.screen.data, y * TIC80_WIDTH + x, tic_tool_peek4(machine->memory.ram.vram.mapping, color));
}

static u8 getPixel(tic_machine* machine, s32 x, s32 y)
{
	if(x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT) return 0;

	return tic_tool_peek4(machine->memory.ram.vram.mapping, tic_tool_peek4(machine->memory.ram.vram.screen.data, y * TIC80_WIDTH + x));
}

static void drawHLine(tic_machine* machine, s32 x, s32 y, s32 width, u8 color)
{
	if(y < 0 || y >= TIC80_HEIGHT) return;

	s32 xl = x < 0 ? 0 : x;
	s32 xr = x + width >= TIC80_WIDTH ? TIC80_WIDTH : x + width;

	for(s32 i = xl; i < xr; ++i)
		setPixel(machine, i, y, color);
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

static void drawRectRev(tic_machine* machine, s32 y, s32 x, s32 height, s32 width, u8 color)
{
	drawRect(machine, x, y, width, height, color);
}

static void drawTile(tic_machine* machine, const tic_tile* buffer, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
	flip &= 0b11;
	rotate &= 0b11;

	s32 a = flip & tic_horz_flip ? -scale : scale;
	s32 b = flip & tic_vert_flip ? -scale : scale;

	void(*drawRectFunc)(tic_machine*, s32, s32, s32, s32, u8) = drawRect;

	if(rotate == tic_90_rotate) a = -a;
	else if(rotate == tic_180_rotate) a = -a, b = -b;
	else if(rotate == tic_270_rotate) b = -b;

	x += (scale - a) * (TIC_SPRITESIZE - 1) >> 1;
	y += (scale - b) * (TIC_SPRITESIZE - 1) >> 1;

	if(rotate == tic_90_rotate || rotate == tic_270_rotate)
	{
		s32 t = a; a = b; b = t;
		t = x; x = y; y = t;
		drawRectFunc = drawRectRev;
	}

	for(s32 i = 0, px = 0, xx = x; i < TIC_SPRITESIZE * TIC_SPRITESIZE; px++, xx += a, i++)
	{
		if(px == TIC_SPRITESIZE) px = 0, xx = x, y += b;

		u8 color = tic_tool_peek4(buffer, i);

		if(count == 1)
		{
			if(*colors != color)
				drawRectFunc(machine, xx, y, scale, scale, color);
		}
		else
		{
			bool draw = true;
			for(s32 i = 0; i < count; i++)
			{
				if(color == colors[i])
				{
					draw = false;
					break;
				}
			}

			if(draw)
				drawRectFunc(machine, xx, y, scale, scale, color);
		}
	}		
}

static void drawMap(tic_machine* machine, const tic_gfx* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data)
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
			RemapResult tile = { *(src->map.data + index), tic_no_flip, tic_no_rotate };

			if (remap)
				remap(data, mi, mj, &tile);

			drawTile(machine, src->tiles + tile.index, ii, jj, &chromakey, 1, scale, tile.flip, tile.rotate);
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

    {
        struct {s8 speed:SFX_SPEED_BITS;} temp = {speed};
        c->speed = speed == temp.speed ? speed : machine->soundSrc->sfx.data[index].speed;
    }

	// start index of idealized piano
	enum {PianoStart = -8};

	s32 freq = (s32)note2freq(note + octave * NOTES + PianoStart);

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

	memory->ram.music_pos.track = index;

	if(index < 0)
	{
		machine->state.music.play = MusicStop;
		resetMusic(memory);
	}
	else
	{
		memory->ram.music_pos.row = row;
		memory->ram.music_pos.frame = frame < 0 ? 0 : frame;
		memory->ram.music_pos.flag.loop = loop;
		machine->state.music.play = MusicPlay;

		const tic_track* track = &machine->soundSrc->music.tracks.data[index];
		machine->state.music.ticks = row >= 0 ? row * (track->speed + DEFAULT_SPEED) * NOTES_PER_MUNUTE / (track->tempo + DEFAULT_TEMPO) / DEFAULT_SPEED : 0;
	}
}

static void api_music(tic_mem* memory, s32 index, s32 frame, s32 row, bool loop)
{
	tic_machine* machine = (tic_machine*)memory;

	setMusic(machine, index, frame, row, loop);

	if(index >= 0)
		machine->state.music.play = MusicPlay;
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
				tic_sound_register_data* data = &machine->state.registers[i];
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
	api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

	soundClear(memory);

	tic_machine* machine = (tic_machine*)memory;
	machine->state.initialized = false;
	machine->state.scanline = NULL;

	updateSaveid(memory);
}

static void api_pause(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;
	memcpy(&machine->pause.state, &machine->state, sizeof(MachineState));
	memcpy(&machine->pause.registers, &memory->ram.registers, sizeof memory->ram.registers);
	memcpy(&machine->pause.music_pos, &memory->ram.music_pos, sizeof memory->ram.music_pos);
	memcpy(&machine->pause.vram, &memory->ram.vram, sizeof memory->ram.vram);

	api_reset(memory);
	memcpy(memory->ram.vram.palette.data, memory->config.palette.data, sizeof(tic_palette));
}

static void api_resume(tic_mem* memory)
{
	api_reset(memory);

	tic_machine* machine = (tic_machine*)memory;
	memcpy(&machine->state, &machine->pause.state, sizeof(MachineState));
	memcpy(&memory->ram.registers, &machine->pause.registers, sizeof memory->ram.registers);
	memcpy(&memory->ram.music_pos, &machine->pause.music_pos, sizeof memory->ram.music_pos);
	memcpy(&memory->ram.vram, &machine->pause.vram, sizeof memory->ram.vram);
}

void tic_close(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->state.initialized = false;

	closeJavascript(machine);
	closeLua(machine);
	blip_delete(machine->blip);

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

static s32 drawChar(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale)
{
	const u8* ptr = memory->font.data + symbol*BITS_IN_BYTE;
	x += (BITS_IN_BYTE - 1)*scale;

	for(s32 i = 0, ys = y; i < TIC_FONT_HEIGHT; i++, ptr++, ys += scale)
		for(s32 col = BITS_IN_BYTE - TIC_FONT_WIDTH, xs = x - col; col < BITS_IN_BYTE; col++, xs -= scale)
			if(*ptr & 1 << col)
				api_rect(memory, xs, ys, scale, scale, color);

	return TIC_FONT_WIDTH*scale;
}

static s32 api_draw_char(tic_mem* memory, u8 symbol, s32 x, s32 y, u8 color)
{
	return drawChar(memory, symbol, x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1);
}

s32 drawText(tic_mem* memory, const char* text, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale, DrawCharFunc* func)
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
		else pos += func(memory, sym, pos, y, width, height, color, scale);
	}

	return pos > max ? pos - x : max - x;
}

static s32 api_fixed_text(tic_mem* memory, const char* text, s32 x, s32 y, u8 color)
{
	return drawText(memory, text, x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1, drawChar);
}

static s32 drawNonFixedChar(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 color, s32 scale)
{
	const u8* ptr = memory->font.data + (symbol)*BITS_IN_BYTE;

	s32 start = 0;
	s32 end = TIC_FONT_WIDTH;
	s32 i = 0;

	for(s32 col = 0; col < TIC_FONT_WIDTH; col++)
	{
		for(i = 0; i < TIC_FONT_HEIGHT; i++)
			if(*(ptr + i) & 0b10000000 >> col) break;

		if(i < TIC_FONT_HEIGHT)	break; else start++;
	}

	x -= start * scale;
	
	for(s32 col = TIC_FONT_WIDTH - 1; col >= start; col--)
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
	return (size ? size + 1 : TIC_FONT_WIDTH - 2) * scale;
}

static s32 api_text(tic_mem* memory, const char* text, s32 x, s32 y, u8 color)
{
	return drawText(memory, text, x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, 1, drawNonFixedChar);
}

static s32 api_text_ex(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale)
{
	return drawText(memory, text, x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT, color, scale, fixed ? drawChar : drawNonFixedChar);
}

static void drawSprite(tic_mem* memory, const tic_gfx* src, s32 index, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
	if(index < TIC_SPRITES)
		drawTile((tic_machine*)memory, src->tiles + index, x, y, colors, count, scale, flip, rotate);
}

static void api_sprite_ex(tic_mem* memory, const tic_gfx* src, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
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

s32 drawSpriteFont(tic_mem* memory, u8 symbol, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale)
{
	api_sprite_ex(memory, &memory->ram.gfx, symbol + TIC_BANK_SPRITES, x, y, 1, 1, &chromakey, 1, scale, tic_no_flip, tic_no_rotate);

	return width * scale;
}

s32 drawFixedSpriteFont(tic_mem* memory, u8 index, s32 x, s32 y, s32 width, s32 height, u8 chromakey, s32 scale)
{
	const u8* ptr = memory->ram.gfx.sprites[index].data;

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
	float ULeft[TIC80_HEIGHT];
	float VLeft[TIC80_HEIGHT];
	float URight[TIC80_HEIGHT];
	float VRight[TIC80_HEIGHT];
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
	int yy = (int)y;
	if (yy >= 0 && yy < TIC80_HEIGHT)
	{
		if (x < SidesBuffer.Left[yy])
		{
			SidesBuffer.Left[yy] = x;
			SidesBuffer.ULeft[yy] = u;
			SidesBuffer.VLeft[yy] = v;
		}
		if (x > SidesBuffer.Right[yy])
		{
			SidesBuffer.Right[yy] = x;
			SidesBuffer.URight[yy] = u;
			SidesBuffer.VRight[yy] = v;
		}
	}
}

static void api_circle(tic_mem* memory, s32 xm, s32 ym, u32 radius, u8 color)
{
	tic_machine* machine = (tic_machine*)memory;

	initSidesBuffer();

	s32 r = radius;
	int x = -r, y = 0, err = 2-2*r;
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

	for(s32 y = 0; y < TIC80_HEIGHT; y++)
		for(s32 x = SidesBuffer.Left[y]; x <= SidesBuffer.Right[y]; ++x)
			setPixel(machine, x, y, color);
}

static void api_circle_border(tic_mem* memory, s32 xm, s32 ym, u32 radius, u8 color)
{
	s32 r = radius;
	int x = -r, y = 0, err = 2-2*r;
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

	for(s32 y = 0; y < TIC80_HEIGHT; y++)
		for(s32 x = SidesBuffer.Left[y]; x <= SidesBuffer.Right[y]; ++x)
			setPixel(machine, x, y, color);
}


typedef struct
{
	float x, y, u, v;
} TexVert;


static void ticTexLine(tic_mem* memory, TexVert *v0, TexVert *v1)
{
	int iy;
	TexVert *top = v0;
	TexVert *bot = v1;

	if (bot->y < top->y)
	{
		top = v1;
		bot = v0;
	}

	float dy = bot->y - top->y;
	if ((int)dy == 0)	return;

	float step_x = (bot->x - top->x) / dy;
	float step_u = (bot->u - top->u) / dy;
	float step_v = (bot->v - top->v) / dy;

	float x = top->x;
	float y = top->y;
	float u = top->u;
	float v = top->v;
	for (; y < (int)bot->y; y++)
	{
		setSideTexPixel(x, y, u, v);
		x += step_x;
		u += step_u;
		v += step_v;
	}
}

static void api_textri(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 u1, s32 v1, s32 u2, s32 v2, s32 u3, s32 v3,bool use_map,u8 chroma)
{
	tic_machine* machine = (tic_machine*)memory;
	TexVert V0, V1, V2;
	const u8* ptr = memory->ram.gfx.tiles[0].data;
	const u8* map = memory->ram.gfx.map.data;

	V0.x = (float)x1; 	V0.y = (float)y1; 	V0.u = (float)u1; 	V0.v = (float)v1;
	V1.x = (float)x2; 	V1.y = (float)y2; 	V1.u = (float)u2; 	V1.v = (float)v2;
	V2.x = (float)x3; 	V2.y = (float)y3; 	V2.u = (float)u3; 	V2.v = (float)v3;
	initSidesBuffer();

	ticTexLine(memory, &V0, &V1);
	ticTexLine(memory, &V1, &V2);
	ticTexLine(memory, &V2, &V0);

	{
		for (s32 y = 0; y < TIC80_HEIGHT; y++)
		{
			float width = SidesBuffer.Right[y] - SidesBuffer.Left[y];
			if (width > 0)
			{
				float du = (SidesBuffer.URight[y] - SidesBuffer.ULeft[y]) / width;
				float dv = (SidesBuffer.VRight[y] - SidesBuffer.VLeft[y]) / width;
				float u = SidesBuffer.ULeft[y];
				float v = SidesBuffer.VLeft[y];

				for (s32 x = (int)SidesBuffer.Left[y]; x <= (int)SidesBuffer.Right[y]; ++x)
				{
					if ((x >= 0) && (x < TIC80_WIDTH))
					{
						if (use_map == true)
						{
							int iu = (int)u % 1920;
							int iv = (int)v % 1088;
							u8 tile = map[(iv>>3) * TIC_MAP_WIDTH + (iu>>3)];
							u8 *buffer = &ptr[tile << 5];
							u8 color = tic_tool_peek4(buffer, (iu & 7) + ((iv & 7) << 3));
							if (color != chroma)
								setPixel(machine, x, y, color);
						}
						else
						{
							int iu = (int)(u) & 127;
							int iv = (int)(v) & 255;
							u8 *buffer = &ptr[((iu >> 3) + ((iv >> 3) << 4)) << 5];
							u8 color = tic_tool_peek4(buffer, (iu & 7) + ((iv & 7) << 3));
							if (color != chroma)
								setPixel(machine, x, y, color);
						}
					}
					u += du;
					v += dv;
				}
			}
		}
	}
}


static void api_sprite(tic_mem* memory, const tic_gfx* src, s32 index, s32 x, s32 y, u8* colors, s32 count)
{
	drawSprite(memory, src, index, x, y, colors, count, 1, tic_no_flip, tic_no_rotate);
}

static void api_map(tic_mem* memory, const tic_gfx* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale)
{
	drawMap((tic_machine*)memory, src, x, y, width, height, sx, sy, chromakey, scale, NULL, NULL);
}

static void api_remap(tic_mem* memory, const tic_gfx* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8 chromakey, s32 scale, RemapFunc remap, void* data)
{
	drawMap((tic_machine*)memory, src, x, y, width, height, sx, sy, chromakey, scale, remap, data);
}

static void api_map_set(tic_mem* memory, tic_gfx* src, s32 x, s32 y, u8 value)
{
	if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

	*(src->map.data + y * TIC_MAP_WIDTH + x) = value;
}

static u8 api_map_get(tic_mem* memory, const tic_gfx* src, s32 x, s32 y)
{
	if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;
	
	return *(src->map.data + y * TIC_MAP_WIDTH + x);
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

static void sfx(tic_mem* memory, s32 index, s32 freq, Channel* channel, tic_sound_register* reg)
{
	tic_machine* machine = (tic_machine*)memory;

	if(channel->duration > 0)
		channel->duration--;

	if(index < 0 || channel->duration == 0)
	{
		resetSfx(channel);
		return;
	}

	const tic_sound_effect* effect = &machine->soundSrc->sfx.data[index];
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
	 
		if(arp) freq = (s32)note2freq(freq2note(freq)+arp);

		freq += effect->data[channel->pos.pitch].pitch * (effect->pitch16x ? 16 : 1);

		reg->freq = freq;
		reg->volume = volume;

		u8 wave = effect->data[channel->pos.wave].wave;
		const tic_waveform* waveform = &machine->soundSrc->sfx.waveform.envelopes[wave];
		memcpy(reg->waveform.data, waveform->data, sizeof(tic_waveform));
	}
}

static void processMusic(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	if(machine->state.music.play == MusicStop) return;

	const tic_track* track = &machine->soundSrc->music.tracks.data[memory->ram.music_pos.track];
	s32 row = machine->state.music.ticks++ * (track->tempo + DEFAULT_TEMPO) * DEFAULT_SPEED / (track->speed + DEFAULT_SPEED) / NOTES_PER_MUNUTE;

	s32 rows = MUSIC_PATTERN_ROWS - track->rows;
	if (row >= rows)
	{
		row = 0;
		machine->state.music.ticks = 0;
		resetMusic(memory);

		if(machine->state.music.play == MusicPlay)
		{
			memory->ram.music_pos.frame++;

			if(memory->ram.music_pos.frame >= MUSIC_FRAMES)
			{
				if(memory->ram.music_pos.flag.loop)
					memory->ram.music_pos.frame = 0;
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
					val += tic_tool_get_pattern_id(track, memory->ram.music_pos.frame, c);

				if(!val)
				{
					if(memory->ram.music_pos.flag.loop)
						memory->ram.music_pos.frame = 0;
					else
					{
						api_music(memory, -1, 0, 0, false);
						return;
					}					
				}
			}
		}
		else if(machine->state.music.play == MusicPlayFrame)
		{
			if(!memory->ram.music_pos.flag.loop)
			{
				api_music(memory, -1, 0, 0, false);
				return;
			}
		}
	}

	if (row != memory->ram.music_pos.row && row < rows)
	{
		memory->ram.music_pos.row = row;

		for (s32 channel = 0; channel < TIC_SOUND_CHANNELS; channel++)
		{
			s32 patternId = tic_tool_get_pattern_id(track, memory->ram.music_pos.frame, channel);
			if (!patternId) continue;

			const tic_track_pattern* pattern = &machine->soundSrc->music.patterns.data[patternId - PATTERN_START];

			s32 note = pattern->rows[memory->ram.music_pos.row].note;

			if (note > NoteNone)
			{
				musicSfx(memory, -1, 0, 0, 0, channel);

				if (note >= NoteStart)
				{
					s32 octave = pattern->rows[memory->ram.music_pos.row].octave;
					s32 sfx = (pattern->rows[row].sfxhi << MUSIC_SFXID_LOW_BITS) | pattern->rows[row].sfxlow;
					s32 volume = pattern->rows[memory->ram.music_pos.row].volume;
					musicSfx(memory, sfx, note - NoteStart, octave, volume, channel);
				}
			}
		}
	}

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		Channel* c = &machine->state.music.channels[i];
		
		if(c->index >= 0)
			sfx(memory, c->index, c->freq, c, &memory->ram.registers[i]);
	}
}

static bool isNoiseWaveform(const tic_waveform* wave)
{
	static const tic_waveform NoiseWave = {.data = {0}};

	return memcmp(&NoiseWave.data, &wave->data, sizeof(tic_waveform)) == 0;
}

static void api_tick_start(tic_mem* memory, const tic_sound* src)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->soundSrc = src;

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
		memset(&memory->ram.registers[i], 0, sizeof(tic_sound_register));

	processMusic(memory);

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		Channel* c = &machine->state.channels[i];
		
		if(c->index >= 0)
			sfx(memory, c->index, c->freq, c, &memory->ram.registers[i]);
	}

	// process gamepad
	for(s32 i = 0; i < COUNT_OF(machine->state.gamepad.holds); i++)
	{
		u32 mask = 1 << i;
		u32 prevDown = machine->state.gamepad.previous.data & mask;
		u32 down = memory->ram.vram.input.gamepad.data & mask;

		u32* hold = &machine->state.gamepad.holds[i];
		if(prevDown && prevDown == down) (*hold)++;
		else *hold = 0;
	}
}

static void api_tick_end(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->state.gamepad.previous.data = machine->memory.ram.vram.input.gamepad.data;

	enum {EndTime = CLOCKRATE / TIC_FRAMERATE};
	for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
	{
		tic_sound_register* reg = &memory->ram.registers[i];
		tic_sound_register_data* data = &machine->state.registers[i];

		isNoiseWaveform(&reg->waveform)
			? runNoise(machine->blip, reg, data, EndTime)
			: runEnvelope(machine->blip, reg, data, EndTime);

		data->time -= EndTime;
	}
	
	blip_end_frame(machine->blip, EndTime);
	blip_read_samples(machine->blip, machine->memory.samples.buffer, machine->samplerate / TIC_FRAMERATE);
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
		machine->state.music.play = MusicPlayFrame;
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
					u8 color = tic_tool_find_closest_color(tic->cart.palette.colors, &rgb);
					tic_tool_poke4(tic->ram.vram.screen.data, i, color);
				}
			}

			gif_close(image);
		}		
	}
}

static void cart2ram(tic_mem* memory)
{
	memcpy(&memory->ram.gfx, &memory->cart.gfx, sizeof memory->ram.gfx);
	memcpy(&memory->ram.sound, &memory->cart.sound, sizeof memory->ram.sound);

	initCover(memory);
}

static const char* readMetatag(const char* code, const char* tag, const char* format)
{
	const char* start = NULL;

	{
		char* tagBuffer = malloc(strlen(format) + strlen(tag));

		if(tagBuffer)
		{
			sprintf(tagBuffer, format, tag);
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

static const char* TagFormatLua = "-- %s:";
static const char* TagFormatJS = "// %s:";

static bool compareMetatag(const char* code, const char* tag, const char* value)
{
	bool result = false;

	// LUA comments
	const char* str = readMetatag(code, tag, TagFormatLua);

	if(str)
	{
		result = strcmp(str, value) == 0;
		free((void*)str);
	}
	else
	{
		// JS comments
		str = readMetatag(code, tag, TagFormatJS);

		if(str)
		{
			result = strcmp(str, value) == 0;
			free((void*)str);
		}
	}

	return result;
}

static bool isMoonscript(const char* code)
{
	return compareMetatag(code, "script", "moon") || compareMetatag(code, "script", "moonscript");
}

static bool isJavascript(const char* code)
{
	return compareMetatag(code, "script", "js") || compareMetatag(code, "script", "javascript");
}

static tic_script_lang api_get_script(tic_mem* memory)
{
	if(isMoonscript(memory->cart.code.data)) return tic_script_moon;
	if(isJavascript(memory->cart.code.data)) return tic_script_js;
	return tic_script_lua;
}

static void updateSaveid(tic_mem* memory)
{
	memset(memory->saveid, 0, sizeof memory->saveid);
	const char* saveid = readMetatag(memory->cart.code.data, "saveid", TagFormatLua);
	if(saveid)
	{
		strcpy(memory->saveid, saveid);
		free((void*)saveid);
	}
	else
	{
		const char* saveid = readMetatag(memory->cart.code.data, "saveid", TagFormatJS);
		if(saveid)
		{
			strcpy(memory->saveid, saveid);
			free((void*)saveid);
		}		
	}
}

static void api_tick(tic_mem* memory, tic_tick_data* data)
{
	tic_machine* machine = (tic_machine*)memory;

	machine->data = data;
	
	if(!machine->state.initialized)
	{
		cart2ram(memory);

		char* code = machine->memory.cart.code.data;
		if(code && strlen(code))
		{
			static const char DoFileTag[] = "dofile(";
			enum {Size = sizeof DoFileTag - 1};

			if (memcmp(code, DoFileTag, Size) == 0)
			{
				const char* start = code + Size;
				const char* end = strchr(start, ')');

				if(end && *start == *(end-1) && (*start == '"' || *start == '\''))
				{
					char filename[FILENAME_MAX] = {0};
					memcpy(filename, start + 1, end - start - 2);

					FILE* file = fopen(filename, "rb");

					if(file)
					{
						fseek(file, 0, SEEK_END);
						s32 size = ftell(file);
						fseek(file, 0, SEEK_SET);

						if(size > 0)
						{
							if(size > TIC_CODE_SIZE)
							{
								char buffer[256];
								sprintf(buffer, "code is larger than %i symbols", TIC_CODE_SIZE);
								machine->data->error(machine->data->data, buffer);

								fclose(file);
								
								return;
							}
							else
							{
								void* buffer = malloc(size+1);

								if(buffer)
								{
									memset(buffer, 0, size+1);

									if(fread(buffer, size, 1, file)) {}

									code = buffer;
								}								
							}							
						}

						fclose(file);
					}
					else
					{
						char buffer[256];
						sprintf(buffer, "dofile: file '%s' not found", filename);
						machine->data->error(machine->data->data, buffer);
						return;
					}
				}
			}

			memory->input = compareMetatag(code, "input", "mouse") ? tic_mouse_input : tic_gamepad_input;


			if(memory->input == tic_mouse_input)
				memory->ram.vram.vars.mask.data = 0;

			//////////////////////////

			memory->script = tic_script_lua;

			if (isMoonscript(code))
			{
				if(!initMoonscript(machine, code))
					return;

				memory->script = tic_script_moon;
			}
			else if(isJavascript(code))
			{
				if(!initJavascript(machine, code))
					return;

				memory->script = tic_script_js;
			}
			else if(!initLua(machine, code))
				return;
		}

		// TODO: possible memory leak if script not initialozed
		if(code != machine->memory.cart.code.data)
			free(code);

		machine->state.scanline = memory->script == tic_script_js ? callJavascriptScanline : callLuaScanline;

		machine->state.initialized = true;
	}

	memory->script == tic_script_js
		? callJavascriptTick(machine)
		: callLuaTick(machine);
}

static void api_scanline(tic_mem* memory, s32 row)
{
	tic_machine* machine = (tic_machine*)memory;

	if(machine->state.initialized)
		machine->state.scanline(memory, row);
}

static double api_time(tic_mem* memory)
{
	tic_machine* machine = (tic_machine*)memory;
	return (double)((machine->data->counter() - machine->data->start)*1000)/machine->data->freq();
}

static void api_sync(tic_mem* tic, bool toCart)
{
	if(toCart)
	{
		memcpy(&tic->cart.gfx, &tic->ram.gfx, sizeof tic->cart.gfx);
		memcpy(&tic->cart.sound, &tic->ram.sound, sizeof tic->cart.sound);		
	}
	else
	{
		memcpy(&tic->ram.gfx, &tic->cart.gfx, sizeof tic->cart.gfx);
		memcpy(&tic->ram.sound, &tic->cart.sound, sizeof tic->cart.sound);
	}
}

static u32 api_btnp(tic_mem* tic, s32 index, s32 hold, s32 period)
{
	tic_machine* machine = (tic_machine*)tic;

	if(index < 0)
	{
		return (~machine->state.gamepad.previous.data) & machine->memory.ram.vram.input.gamepad.data;
	}
	else if(hold < 0 || period < 0)
	{
		return ((~machine->state.gamepad.previous.data) & machine->memory.ram.vram.input.gamepad.data) & (1 << index);
	}

	tic80_input previous;
	
	previous.data = machine->state.gamepad.holds[index] >= hold 
		? period && machine->state.gamepad.holds[index] % period ? machine->state.gamepad.previous.data : 0
		: machine->state.gamepad.previous.data;

	return ((~previous.data) & machine->memory.ram.vram.input.gamepad.data) & (1 << index);
}

static void api_load(tic_cartridge* cart, const u8* buffer, s32 size, bool palette)
{
	const u8* end = buffer + size;
	memset(cart, 0, sizeof(tic_cartridge));

	if(palette)
	{
		static const u8 DB16[] = {0x14, 0x0c, 0x1c, 0x44, 0x24, 0x34, 0x30, 0x34, 0x6d, 0x4e, 0x4a, 0x4e, 0x85, 0x4c, 0x30, 0x34, 0x65, 0x24, 0xd0, 0x46, 0x48, 0x75, 0x71, 0x61, 0x59, 0x7d, 0xce, 0xd2, 0x7d, 0x2c, 0x85, 0x95, 0xa1, 0x6d, 0xaa, 0x2c, 0xd2, 0xaa, 0x99, 0x6d, 0xc2, 0xca, 0xda, 0xd4, 0x5e, 0xde, 0xee, 0xd6};
		memcpy(cart->palette.data, DB16, sizeof(tic_palette));				
	}

	#define LOAD_CHUNK(to) memcpy(&to, buffer, min(sizeof(to), chunk.size))

	while(buffer < end)
	{
		Chunk chunk;
		memcpy(&chunk, buffer, sizeof(Chunk));
		buffer += sizeof(Chunk);

		switch(chunk.type)
		{
		case CHUNK_TILES: 		LOAD_CHUNK(cart->gfx.tiles); 					break;
		case CHUNK_SPRITES: 	LOAD_CHUNK(cart->gfx.sprites); 					break;
		case CHUNK_MAP: 		LOAD_CHUNK(cart->gfx.map); 						break;
		case CHUNK_CODE: 		LOAD_CHUNK(cart->code); 						break;
		case CHUNK_SOUND: 		LOAD_CHUNK(cart->sound.sfx.data); 				break;
		case CHUNK_WAVEFORM:	LOAD_CHUNK(cart->sound.sfx.waveform);			break;
		case CHUNK_MUSIC:		LOAD_CHUNK(cart->sound.music.tracks.data); 		break;
		case CHUNK_PATTERNS:	LOAD_CHUNK(cart->sound.music.patterns.data); 	break;
		case CHUNK_PALETTE:		
			if(palette)
				LOAD_CHUNK(cart->palette); 					
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

static u8* saveFixedChunk(u8* buffer, ChunkType type, const void* from, s32 size)
{
	if(size)
	{
		Chunk chunk = {type, size};
		memcpy(buffer, &chunk, sizeof(Chunk));
		buffer += sizeof(Chunk);
		memcpy(buffer, from, size);
		buffer += size;
	}

	return buffer;
}

static u8* saveChunk(u8* buffer, ChunkType type, const void* from, s32 size)
{
	s32 chunkSize = calcBufferSize(from, size);

	return saveFixedChunk(buffer, type, from, chunkSize);
}

static s32 api_save(const tic_cartridge* cart, u8* buffer)
{
	u8* start = buffer;

	#define SAVE_CHUNK(id, from) saveChunk(buffer, id, &from, sizeof(from))

	buffer = SAVE_CHUNK(CHUNK_TILES, 	cart->gfx.tiles);
	buffer = SAVE_CHUNK(CHUNK_SPRITES, 	cart->gfx.sprites);
	buffer = SAVE_CHUNK(CHUNK_MAP, 		cart->gfx.map);
	buffer = SAVE_CHUNK(CHUNK_CODE, 	cart->code);
	buffer = SAVE_CHUNK(CHUNK_SOUND, 	cart->sound.sfx.data);
	buffer = SAVE_CHUNK(CHUNK_WAVEFORM, cart->sound.sfx.waveform);
	buffer = SAVE_CHUNK(CHUNK_PATTERNS, cart->sound.music.patterns.data);
	buffer = SAVE_CHUNK(CHUNK_MUSIC, 	cart->sound.music.tracks.data);
	buffer = SAVE_CHUNK(CHUNK_PALETTE, 	cart->palette);

	buffer = saveFixedChunk(buffer, CHUNK_COVER, cart->cover.data, cart->cover.size);

	#undef SAVE_CHUNK

	return (s32)(buffer - start);
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
	INIT_API(reset);
	INIT_API(pause);
	INIT_API(resume);
	INIT_API(get_script);
	INIT_API(sync);
	INIT_API(btnp);
	INIT_API(load);
	INIT_API(save);
	INIT_API(tick_start);
	INIT_API(tick_end);

#undef INIT_API
}

tic_mem* tic_create(s32 samplerate)
{
	tic_machine* machine = (tic_machine*)malloc(sizeof(tic_machine));
	memset(machine, 0, sizeof(tic_machine));

	if(machine != (tic_machine*)&machine->memory)
		return NULL;

	machine->soundSrc = &machine->memory.ram.sound;

	initApi(&machine->memory.api);

	machine->samplerate = samplerate;
	machine->memory.samples.size = samplerate / TIC_FRAMERATE * sizeof(s16);
	machine->memory.samples.buffer = malloc(machine->memory.samples.size);

	machine->blip = blip_new(samplerate / 10);

	blip_set_rates(machine->blip, CLOCKRATE, samplerate);

	machine->memory.api.reset(&machine->memory);

	return &machine->memory;
}
