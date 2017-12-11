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

#include "tic80.h"

#include "defines.h"

#define TIC_VERSION_MAJOR 0
#define TIC_VERSION_MINOR 50
#define TIC_VERSION_PATCH 1
#define TIC_VERSION_STATUS ""

#if defined(TIC80_PRO)
#define TIC_VERSION_POST " Pro"
#else
#define TIC_VERSION_POST ""
#endif

#define TIC_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define TIC_VERSION TIC_MAKE_VERSION(MYPROJ_VERSION_MAJOR, MYPROJ_VERSION_MINOR, MYPROJ_VERSION_PATCH)

#define DEF2STR2(x) #x
#define DEF2STR(x) DEF2STR2(x)

#define TIC_VERSION_LABEL DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) "." DEF2STR(TIC_VERSION_PATCH) TIC_VERSION_STATUS TIC_VERSION_POST
#define TIC_PACKAGE "com.nesbox.tic"
#define TIC_NAME "TIC-80"
#define TIC_NAME_FULL TIC_NAME " tiny computer"
#define TIC_TITLE TIC_NAME_FULL " " TIC_VERSION_LABEL
#define TIC_HOST "tic.computer"
#define TIC_COPYRIGHT "http://" TIC_HOST " (C) 2017"

#define TIC_VRAM_SIZE (16*1024) //16K
#define TIC_RAM_SIZE (80*1024) //80K
#define TIC_FONT_WIDTH 6
#define TIC_FONT_HEIGHT 6
#define TIC_PALETTE_BPP 4
#define TIC_PALETTE_SIZE (1 << TIC_PALETTE_BPP)
#define TIC_FRAMERATE 60
#define TIC_SPRITESIZE 8
#define TIC_GAMEPAD_MASK 0xff

#define BITS_IN_BYTE 8
#define TIC_BANK_SPRITES (1 << BITS_IN_BYTE)
#define TIC_SPRITE_BANKS 2
#define TIC_SPRITES (TIC_BANK_SPRITES * TIC_SPRITE_BANKS)

#define TIC_SPRITESHEET_SIZE 128

#define TIC_MAP_ROWS (TIC_SPRITESIZE)
#define TIC_MAP_COLS (TIC_SPRITESIZE)
#define TIC_MAP_SCREEN_WIDTH (TIC80_WIDTH / TIC_SPRITESIZE)
#define TIC_MAP_SCREEN_HEIGHT (TIC80_HEIGHT / TIC_SPRITESIZE)
#define TIC_MAP_WIDTH (TIC_MAP_SCREEN_WIDTH * TIC_MAP_ROWS)
#define TIC_MAP_HEIGHT (TIC_MAP_SCREEN_HEIGHT * TIC_MAP_COLS)

#define TIC_PERSISTENT_SIZE ((56-25)/sizeof(s32))
#define TIC_SAVEID_SIZE 64

#define TIC_SOUND_CHANNELS 4
#define SFX_TICKS 30
#define SFX_COUNT_BITS 6
#define SFX_COUNT (1 << SFX_COUNT_BITS)
#define SFX_SPEED_BITS 3

#define NOTES 12
#define OCTAVES 8
#define MAX_VOLUME 15
#define MUSIC_PATTERN_ROWS 64
#define MUSIC_PATTERNS 60
#define TRACK_PATTERN_BITS 6
#define TRACK_PATTERN_MASK ((1 << TRACK_PATTERN_BITS) - 1)
#define TRACK_PATTERNS_SIZE (TRACK_PATTERN_BITS * TIC_SOUND_CHANNELS / BITS_IN_BYTE)
#define MUSIC_FRAMES 16
#define MUSIC_TRACKS_BITS 3
#define MUSIC_TRACKS (1 << MUSIC_TRACKS_BITS)
#define DEFAULT_TEMPO 150
#define DEFAULT_SPEED 6
#define NOTES_PER_BEET 4
#define PATTERN_START 1
#define MUSIC_SFXID_LOW_BITS 5
#define ENVELOPES_COUNT 16
#define ENVELOPE_VALUES 32
#define ENVELOPE_VALUE_BITS 4
#define ENVELOPE_SIZE (ENVELOPE_VALUES * ENVELOPE_VALUE_BITS / BITS_IN_BYTE)

#define TIC_CODE_SIZE (0x10000)

#define TIC_BANKS 4

#define SFX_NOTES {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"}

#define API_KEYWORDS {"TIC", "scanline", "OVR", "print", "cls", "pix", "line", "rect", "rectb", \
	"spr", "btn", "btnp", "sfx", "map", "mget", "mset", "peek", "poke", "peek4", "poke4", \
	"memcpy", "memset", "trace", "pmem", "time", "exit", "font", "mouse", "circ", "circb", "tri", "textri", \
	"clip", "music", "sync"}

#define TIC_FONT_CHARS 128

enum
{
	NoteNone = 0,
	NoteStop,
	NoteNone2,
	NoteNone3,
	NoteStart,
};

enum
{
	tic_color_black,		// 0
	tic_color_dark_red,		// 1
	tic_color_dark_blue,	// 2
	tic_color_dark_gray,	// 3
	tic_color_brown,		// 4
	tic_color_green,		// 5
	tic_color_red,			// 6
	tic_color_gray,			// 7
	tic_color_blue,			// 8
	tic_color_orange,		// 9
	tic_color_light_blue,	// 10
	tic_color_light_green,	// 11
	tic_color_peach,		// 12
	tic_color_cyan,			// 13
	tic_color_yellow,		// 14
	tic_color_white,		// 15
} tic_color;

typedef enum
{
	tic_no_flip = 0b00,
	tic_horz_flip = 0b01,
	tic_vert_flip = 0b10,
} tic_flip;

typedef enum
{
	tic_no_rotate,
	tic_90_rotate,
	tic_180_rotate,
	tic_270_rotate,
} tic_rotate;

typedef struct
{
	u8 start:4;
	u8 size:4;
} tic_sound_loop;

typedef struct
{
	
	struct
	{
		u8 volume:4;
		u8 wave:4;
		u8 arpeggio:4;
		s8 pitch:4;
	} data[SFX_TICKS];

	struct
	{
		u8 octave:3;
		u8 pitch16x:1; // pitch factor
		s8 speed:3;
		u8 reverse:1; // arpeggio reverse
		u8 note:4;
		u8 chain:1;
		u8 temp:3;
	};

	union
	{
		struct
		{
			tic_sound_loop wave;
			tic_sound_loop volume;
			tic_sound_loop arpeggio;
			tic_sound_loop pitch;
		};

		tic_sound_loop loops[4];
	};

} tic_sound_effect;

typedef struct
{
	u8 data[ENVELOPE_SIZE];
}tic_waveform;

typedef struct
{
	tic_waveform envelopes[ENVELOPES_COUNT];
} tic_waveforms;

typedef struct
{
	struct
	{
		u8 note:4;
		u8 volume:4;
		u8 param:4;
		u8 effect:3;
		u8 sfxhi:1;
		u8 sfxlow:MUSIC_SFXID_LOW_BITS;
		u8 octave:3;
	} rows[MUSIC_PATTERN_ROWS];

} tic_track_pattern;

typedef struct
{
	u8 data[MUSIC_FRAMES * TRACK_PATTERNS_SIZE]; // sfx - 6bits per channel = 24 bit

	s8 tempo; // delta value, rel to 120 bpm * 10 [32-255]
	u8 rows; // delta value, rel to 64 rows, can be [1-64]
	s8 speed; // delta value, rel to 6 [1-31]

} tic_track;

typedef struct
{
	tic_track_pattern data[MUSIC_PATTERNS];
} tic_patterns;

typedef struct
{
	tic_track data[MUSIC_TRACKS];
} tic_tracks;

typedef struct
{
	tic_waveforms waveform;
	tic_sound_effect data[SFX_COUNT];
}tic_sfx;

typedef struct
{
	tic_patterns patterns;
	tic_tracks tracks;
}tic_music;

typedef struct
{
	s8 track;
	s8 frame;
	s8 row;
	
	struct
	{
		bool loop:1;
	} flag;

} tic_music_pos;

typedef struct
{
	struct
	{
		u16 freq:12;
		u16 volume:4;
	};

	tic_waveform waveform;
} tic_sound_register;

typedef struct
{
	u8 data[TIC_MAP_WIDTH * TIC_MAP_HEIGHT];
} tic_map;

typedef struct
{
	u8 data[TIC_SPRITESIZE * TIC_SPRITESIZE * TIC_PALETTE_BPP / BITS_IN_BYTE];
} tic_tile;

typedef struct
{
	char data[TIC_CODE_SIZE];
} tic_code;

typedef struct 
{
	s32 size;
	u8 data [TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32)];
} tic_cover_image;

typedef struct
{
	u8 r;
	u8 g;
	u8 b;
} tic_rgb;

typedef union
{
	tic_rgb colors[TIC_PALETTE_SIZE];

	u8 data[TIC_PALETTE_SIZE * sizeof(tic_rgb)];
} tic_palette;

typedef struct
{
	tic_tile data[TIC_BANK_SPRITES];
} tic_tiles;

typedef struct
{
	tic_tiles tiles;
	tic_tiles sprites;
	tic_map map;
	tic_sfx sfx;
	tic_music music;
} tic_bank;

typedef struct
{
	union
	{
		tic_bank bank;
		tic_bank banks[TIC_BANKS];
	};
	
	tic_code code;
	tic_cover_image cover;
	tic_palette palette;
} tic_cartridge;

typedef struct
{
	u8 data[TIC_FONT_CHARS * BITS_IN_BYTE];
} tic_font;

typedef struct
{
	u8 data[TIC80_WIDTH * TIC80_HEIGHT * TIC_PALETTE_BPP / BITS_IN_BYTE];
} tic_screen;

typedef union
{
	struct
	{
		tic_screen screen;
		tic_palette palette;
		u8 mapping[TIC_PALETTE_SIZE * TIC_PALETTE_BPP / BITS_IN_BYTE];

		struct
		{
			union
			{
				u8 colors;
							
				struct
				{
					u8 border:TIC_PALETTE_BPP;
					u8 tmp:TIC_PALETTE_BPP;
				};
			};

			struct
			{
				s8 x;
				s8 y;
			} offset;

			union
			{
				u8 cursor;
				tic80_gamepad mask;
			};
		} vars;

		struct
		{
			tic80_input gamepad;
			u8 reserved[2];
		} input;
	};
	
	u8 data[TIC_VRAM_SIZE];
} tic_vram;

typedef struct
{
	s32 data[TIC_PERSISTENT_SIZE];
} tic_persistent;

typedef union
{
	struct
	{
		tic_vram vram;
		tic_tiles tiles;
		tic_tiles sprites;
		tic_map map;
		tic_persistent persistent;
		tic_sound_register registers[TIC_SOUND_CHANNELS];
		tic_sfx sfx;
		tic_music music;
		tic_music_pos music_pos;
	};

	u8 data[TIC_RAM_SIZE];
} tic_ram;

typedef enum
{
	tic_gamepad_input,
	tic_mouse_input,
} tic_input_method;

typedef enum
{
	tic_script_lua,	
	tic_script_moon,
	tic_script_js,
} tic_script_lang;
