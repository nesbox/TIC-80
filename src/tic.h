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

#include "retro_endianness.h"
#include "tic80.h"
#include "defines.h"

#define TIC_VRAM_SIZE (16*1024) //16K
#define TIC_RAM_SIZE (TIC_VRAM_SIZE+80*1024) //16K+80K
#define TIC_WASM_PAGE_COUNT 4 // 256K
#define TIC_FONT_WIDTH 6
#define TIC_FONT_HEIGHT 6
#define TIC_ALTFONT_WIDTH 4
#define TIC_PALETTE_BPP 4
#define TIC_PALETTE_SIZE (1 << TIC_PALETTE_BPP)
#define TIC_PALETTES 2
#define TIC_SPRITESIZE 8

#define TIC_DEFAULT_BIT_DEPTH 4
#define TIC_DEFAULT_BLIT_MODE 2

#define TIC80_OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
#define TIC80_OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)

#define BITS_IN_BYTE 8
#define TIC_BANK_SPRITES (1 << BITS_IN_BYTE)
#define TIC_SPRITE_BANKS 2
#define TIC_FLAGS (TIC_BANK_SPRITES * TIC_SPRITE_BANKS)
#define TIC_SPRITES (TIC_BANK_SPRITES * TIC_SPRITE_BANKS)

#define TIC_SPRITESHEET_SIZE 128
#define TIC_SPRITESHEET_COLS (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE)

#define TIC_MAP_ROWS (TIC_SPRITESIZE)
#define TIC_MAP_COLS (TIC_SPRITESIZE)
#define TIC_MAP_SCREEN_WIDTH (TIC80_WIDTH / TIC_SPRITESIZE)
#define TIC_MAP_SCREEN_HEIGHT (TIC80_HEIGHT / TIC_SPRITESIZE)
#define TIC_MAP_WIDTH (TIC_MAP_SCREEN_WIDTH * TIC_MAP_ROWS)
#define TIC_MAP_HEIGHT (TIC_MAP_SCREEN_HEIGHT * TIC_MAP_COLS)

#define TIC_PERSISTENT_SIZE (1024/sizeof(s32)) // 1K
#define TIC_SAVEID_SIZE 64

#define TIC_SOUND_CHANNELS 4
#define SFX_TICKS 30
#define SFX_COUNT_BITS 6
#define SFX_COUNT (1 << SFX_COUNT_BITS)
#define SFX_SPEED_BITS 3
#define SFX_DEF_SPEED (1 << SFX_SPEED_BITS)

#define NOTES 12
#define OCTAVES 8
#define MAX_VOLUME 15
#define MUSIC_PATTERN_ROWS 64
#define MUSIC_PATTERNS 60
#define MUSIC_CMD_BITS 3
#define TRACK_PATTERN_BITS 6
#define TRACK_PATTERN_MASK ((1 << TRACK_PATTERN_BITS) - 1)
#define TRACK_PATTERNS_SIZE (TRACK_PATTERN_BITS * TIC_SOUND_CHANNELS / BITS_IN_BYTE)
#define MUSIC_FRAMES 16
#define MUSIC_TRACKS 8
#define DEFAULT_TEMPO 150
#define DEFAULT_SPEED 6
#define PITCH_DELTA 128
#define NOTES_PER_BEAT 4
#define PATTERN_START 1
#define MUSIC_SFXID_LOW_BITS 5
#define WAVES_COUNT 16
#define WAVE_VALUES 32
#define WAVE_VALUE_BITS 4
#define WAVE_MAX_VALUE ((1 << WAVE_VALUE_BITS) - 1)
#define WAVE_SIZE (WAVE_VALUES * WAVE_VALUE_BITS / BITS_IN_BYTE)

#define TIC_BANKSIZE_BITS 16
#define TIC_BANK_SIZE (1 << TIC_BANKSIZE_BITS) // 64K
#define TIC_BANK_BITS 3
#define TIC_BANKS (1 << TIC_BANK_BITS)

#define TIC_CODE_SIZE (TIC_BANK_SIZE * TIC_BANKS)
#define TIC_BINARY_BANKS 4
#define TIC_BINARY_SIZE (TIC_BINARY_BANKS * TIC_BANK_SIZE) // 4 * 64k = 256K


#define TIC_BUTTONS 8
#define TIC_GAMEPADS (sizeof(tic80_gamepads) / sizeof(tic80_gamepad))

#define SFX_NOTES {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"}
#define TIC_FONT_CHARS 128

#define TIC_UNUSED(x) (void)x

enum
{
    NoteNone = 0,
    NoteStop,
    NoteNone2,
    NoteNone3,
    NoteStart,
};

typedef enum
{
    tic_color_black,
    tic_color_purple,
    tic_color_red,
    tic_color_orange,
    tic_color_yellow,
    tic_color_light_green,
    tic_color_green,
    tic_color_dark_green,
    tic_color_dark_blue,
    tic_color_blue,
    tic_color_light_blue,
    tic_color_cyan,
    tic_color_white,
    tic_color_light_grey,
    tic_color_grey,
    tic_color_dark_grey,
} tic_color;

typedef enum
{
    tic_no_flip = 0,
    tic_horz_flip = 1,
    tic_vert_flip = 2,
} tic_flip;

typedef enum
{
    tic_no_rotate,
    tic_90_rotate,
    tic_180_rotate,
    tic_270_rotate,
} tic_rotate;

typedef enum
{
    tic_bpp_4 = 4,
    tic_bpp_2 = 2,
    tic_bpp_1 = 1,
} tic_bpp;

typedef struct
{
#if RETRO_IS_BIG_ENDIAN
    u8 size:4;
    u8 start:4;
#else
    u8 start:4;
    u8 size:4;
#endif
} tic_sound_loop;

typedef struct
{
    
    struct
    {
#if RETRO_IS_BIG_ENDIAN
        u8 wave:4;
        u8 volume:4;
        s8 pitch:4;
        u8 chord:4;
#else
        u8 volume:4;
        u8 wave:4;
        u8 chord:4;
        s8 pitch:4;
#endif
    } data[SFX_TICKS];

    struct
    {
#if RETRO_IS_BIG_ENDIAN
        u8 reverse:1; // chord reverse
        s8 speed:SFX_SPEED_BITS;
        u8 pitch16x:1; // pitch factor
        u8 octave:3;
        u8 temp:2;
        u8 stereo_right:1;
        u8 stereo_left:1;
        u8 note:4;
#else
        u8 octave:3;
        u8 pitch16x:1; // pitch factor
        s8 speed:SFX_SPEED_BITS;
        u8 reverse:1; // chord reverse
        u8 note:4;
        u8 stereo_left:1;
        u8 stereo_right:1;
        u8 temp:2;
#endif
    };

    union
    {
        struct
        {
            tic_sound_loop wave;
            tic_sound_loop volume;
            tic_sound_loop chord;
            tic_sound_loop pitch;
        };

        tic_sound_loop loops[4];
    };

} tic_sample;

typedef struct
{
    union
    {
        struct
        {
            s8 wave;
            s8 volume;
            s8 chord;
            s8 pitch;
        };

        s8 data[4];
    };
} tic_sfx_pos;

typedef struct
{
    u8 data[WAVE_SIZE];
}tic_waveform;

typedef struct
{
    tic_waveform items[WAVES_COUNT];
} tic_waveforms;

#define MUSIC_CMD_LIST(macro)                                               \
    macro(empty,    0, "")                                                  \
    macro(volume,   M, "master volume for LEFT=X / RIGHT=Y channel")        \
    macro(chord,    C, "play chord, X=3 Y=7 plays +0,+3,+7 notes")          \
    macro(jump,     J, "jump to FRAME=X / BEAT=Y")                          \
    macro(slide,    S, "slide to note (legato) with TICKS=XY")              \
    macro(pitch,    P, "finepitch UP/DOWN=XY-" DEF2STR(PITCH_DELTA))        \
    macro(vibrato,  V, "vibrato with PERIOD=X and DEPTH=Y")                 \
    macro(delay,    D, "delay triggering a note with TICKS=XY")

typedef enum
{
#define ENUM_ITEM(name, ...) tic_music_cmd_##name,
    MUSIC_CMD_LIST(ENUM_ITEM)
#undef ENUM_ITEM

    tic_music_cmd_count
} tic_music_command;

typedef struct
{
#if RETRO_IS_BIG_ENDIAN
    u8 param1   :4;
    u8 note     :4;
    u8 sfxhi    :1;
    u8 command  :MUSIC_CMD_BITS; // tic_music_command
    u8 param2   :4;
    u8 octave   :3;
    u8 sfxlow   :MUSIC_SFXID_LOW_BITS;
#else
    u8 note     :4;
    u8 param1   :4;
    u8 param2   :4;
    u8 command  :MUSIC_CMD_BITS; // tic_music_command
    u8 sfxhi    :1;
    u8 sfxlow   :MUSIC_SFXID_LOW_BITS;
    u8 octave   :3;
#endif
} tic_track_row;

typedef struct
{
    tic_track_row rows[MUSIC_PATTERN_ROWS];

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
    tic_sample data[SFX_COUNT];
} tic_samples;

typedef struct
{
    tic_waveforms waveforms;
    tic_samples samples;
}tic_sfx;

typedef struct
{
    tic_patterns patterns;
    tic_tracks tracks;
}tic_music;

typedef enum
{
    tic_music_stop,
    tic_music_play_frame,
    tic_music_play,
} tic_music_status;

typedef struct
{
    struct
    {
        s8 track;
        s8 frame;
        s8 row;
    } music;
    
    struct
    {
#if RETRO_IS_BIG_ENDIAN
        u8 unknown:4;
        u8 music_sustain:1;
        u8 music_status:2; // enum tic_music_status
        u8 music_loop:1;
#else
        u8 music_loop:1;
        u8 music_status:2; // enum tic_music_status
        u8 music_sustain:1;
        u8 unknown:4;
#endif
    } flag;

} tic_music_state;

typedef union
{
    struct
    {
#if RETRO_IS_BIG_ENDIAN
        u8 right4:4;
        u8 left4:4;

        u8 right3:4;
        u8 left3:4;

        u8 right2:4;
        u8 left2:4;

        u8 right1:4;
        u8 left1:4;
#else
        u8 left1:4;
        u8 right1:4;

        u8 left2:4;
        u8 right2:4;

        u8 left3:4;
        u8 right3:4;

        u8 left4:4;
        u8 right4:4;
#endif
    };

    u32 data;
} tic_stereo_volume;

typedef struct
{
    struct
    {
	u8 freq_low;
#if RETRO_IS_BIG_ENDIAN
        u8 volume:4;
        u8 freq_high:4;
#else
        u8 freq_high:4;
        u8 volume:4;
#endif
    };

    tic_waveform waveform;
} tic_sound_register;


static INLINE u16 tic_sound_register_get_freq(const tic_sound_register* reg)
{
    return (reg->freq_high << 8) | reg->freq_low;
}

static INLINE void tic_sound_register_set_freq(tic_sound_register* reg, u16 val)
{
    reg->freq_low = val;
    reg->freq_high = val >> 8;
}

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
    char data[TIC_BINARY_SIZE];
    u32 size;
} tic_binary;

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
    u32 data[TIC_PALETTE_SIZE];
} tic_blitpal;

typedef struct
{
    tic_tile data[TIC_BANK_SPRITES];
} tic_tiles, tic_sprites;

typedef struct
{
    u8 data[TIC_FLAGS];
} tic_flags;

typedef struct
{
    tic_palette vbank0;
    tic_palette vbank1;
} tic_palettes;

typedef struct
{
    u8 data[TIC80_WIDTH * TIC80_HEIGHT * TIC_PALETTE_BPP / BITS_IN_BYTE];
} tic_screen;

typedef struct
{
    tic_screen      screen;
    tic_tiles       tiles;
    tic_sprites     sprites;
    tic_map         map;
    tic_sfx         sfx;
    tic_music       music;
    tic_flags       flags;
    tic_palettes    palette;
} tic_bank;

typedef struct
{
    union
    {
        tic_bank bank0;
        tic_bank banks[TIC_BANKS];
    };

    tic_code code;
    tic_binary binary;
    u8 lang;

} tic_cartridge;

typedef struct
{
    u8 data[(TIC_FONT_CHARS - 1) * BITS_IN_BYTE];

    union
    {
        struct
        {
            u8 width;
            u8 height;
        };

        u8 params[BITS_IN_BYTE];
    };
} tic_font_data;

typedef struct
{
    tic_font_data regular;
    tic_font_data alt;
} tic_font;

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
		struct {
#if RETRO_IS_BIG_ENDIAN
		    u8 padding_border:4;
		    u8 border:TIC_PALETTE_BPP;
#else
		    u8 border:TIC_PALETTE_BPP;
		    u8 padding_border:4;
#endif
		};
                // clear color for the BANK1
		struct {
#if RETRO_IS_BIG_ENDIAN
		    u8 padding_clear:4;
		    u8 clear:TIC_PALETTE_BPP;
#else
		    u8 clear:TIC_PALETTE_BPP;
		    u8 padding_clear:4;
#endif
		};
            };

            struct
            {
                s8 x;
                s8 y;
            } offset;

            struct
            {
#if RETRO_IS_BIG_ENDIAN
                u8 system:1;
                u8 sprite:7;
#else
                u8 sprite:7;
                u8 system:1;
#endif
            } cursor;
        } vars;

        struct
        {
#if RETRO_IS_BIG_ENDIAN
            u8 reserved:4;
            u8 segment:4;
#else
            u8 segment:4;
            u8 reserved:4;
#endif
        } blit;

        u8 reserved[3];
    };
    
    u8 data[TIC_VRAM_SIZE];
} tic_vram;

typedef struct
{
    u32 data[TIC_PERSISTENT_SIZE];
} tic_persistent;

typedef struct
{
    u8 data[TIC_GAMEPADS * TIC_BUTTONS];
} tic_mapping;

typedef union
{
    struct
    {
        tic_vram            vram;
        tic_tiles           tiles;
        tic_sprites         sprites;
        tic_map             map;
        tic80_input         input;
        tic_sfx_pos         sfxpos[TIC_SOUND_CHANNELS];
        tic_sound_register  registers[TIC_SOUND_CHANNELS];
        tic_sfx             sfx;
        tic_music           music;
        tic_music_state     music_state;
        tic_stereo_volume   stereo;
        tic_persistent      persistent;
        tic_flags           flags;
        tic_font            font;
        tic_mapping         mapping;

        u8 free;
    };

    u8 data[TIC_RAM_SIZE];

} tic_ram;

typedef enum
{
    tic_key_unknown,

    tic_key_a,
    tic_key_b,
    tic_key_c,
    tic_key_d,
    tic_key_e,
    tic_key_f,
    tic_key_g,
    tic_key_h,
    tic_key_i,
    tic_key_j,
    tic_key_k,
    tic_key_l,
    tic_key_m,
    tic_key_n,
    tic_key_o,
    tic_key_p,
    tic_key_q,
    tic_key_r,
    tic_key_s,
    tic_key_t,
    tic_key_u,
    tic_key_v,
    tic_key_w,
    tic_key_x,
    tic_key_y,
    tic_key_z,

    tic_key_0,
    tic_key_1,
    tic_key_2,
    tic_key_3,
    tic_key_4,
    tic_key_5,
    tic_key_6,
    tic_key_7,
    tic_key_8,
    tic_key_9,

    tic_key_minus,
    tic_key_equals,
    tic_key_leftbracket,
    tic_key_rightbracket,
    tic_key_backslash,
    tic_key_semicolon,
    tic_key_apostrophe,
    tic_key_grave,
    tic_key_comma,
    tic_key_period,
    tic_key_slash,
    
    tic_key_space,
    tic_key_tab,

    tic_key_return,
    tic_key_backspace,
    tic_key_delete,
    tic_key_insert,

    tic_key_pageup,
    tic_key_pagedown,
    tic_key_home,
    tic_key_end,
    tic_key_up,
    tic_key_down,
    tic_key_left,
    tic_key_right,

    tic_key_capslock,
    tic_key_ctrl,
    tic_key_shift,
    tic_key_alt,

    tic_key_escape,
    tic_key_f1,
    tic_key_f2,
    tic_key_f3,
    tic_key_f4,
    tic_key_f5,
    tic_key_f6,
    tic_key_f7,
    tic_key_f8,
    tic_key_f9,
    tic_key_f10,
    tic_key_f11,
    tic_key_f12,

    tic_key_numpad0,
    tic_key_numpad1,
    tic_key_numpad2,
    tic_key_numpad3,
    tic_key_numpad4,
    tic_key_numpad5,
    tic_key_numpad6,
    tic_key_numpad7,
    tic_key_numpad8,
    tic_key_numpad9,
    tic_key_numpadplus,
    tic_key_numpadminus,
    tic_key_numpadmultiply,
    tic_key_numpaddivide,
    tic_key_numpadenter,
    tic_key_numpadperiod,

    ////////////////

    tic_keys_count
} tic_keycode;

typedef enum
{
    tic_mouse_left,
    tic_mouse_middle,
    tic_mouse_right,
} tic_mouse_btn;

typedef enum
{
    tic_cursor_arrow,
    tic_cursor_hand,
    tic_cursor_ibeam,
} tic_cursor;
