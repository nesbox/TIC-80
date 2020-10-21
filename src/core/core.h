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

#include "api.h"
#include "tools.h"
#include "blip_buf.h"

#define CLOCKRATE (255<<13)
#define TIC_DEFAULT_COLOR tic_color_white

typedef struct
{
    s32 time;       /* clock time of next delta */
    s32 phase;      /* position within waveform */
    s32 amp;        /* current amplitude in delta buffer */
}tic_sound_register_data;

typedef struct
{
    s32 tick;
    tic_sfx_pos* pos;
    s32 index;
    s32 note;
    struct
    {
        u8 left:4;
        u8 right:4;
    } volume;
    s8 speed:SFX_SPEED_BITS;
    s32 duration;
} tic_channel_data;

typedef struct
{
    struct
    {
        s32 tick;
        u8 note1:4;
        u8 note2:4;
    } chord;

    struct
    {
        s32 tick;
        u8 period:4;
        u8 depth:4;
    } vibrato;

    struct
    {
        s32 tick;
        u8 note;
        s32 duration;
    } slide;

    struct
    {
        s32 value;
    } finepitch;

    struct
    {
        const tic_track_row* row;
        s32 ticks;
    } delay;

} tic_command_data;

typedef struct
{
    s32 l;
    s32 t;
    s32 r;
    s32 b;
} tic_clip_data;

typedef struct
{
    bool active;
    s32 frame;
    s32 beat;
} tic_jump_command;

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

    tic_clip_data clip;

    struct
    {
        tic_sound_register_data left[TIC_SOUND_CHANNELS];
        tic_sound_register_data right[TIC_SOUND_CHANNELS];
    } registers;

    struct
    {
        tic_channel_data channels[TIC_SOUND_CHANNELS];
    } sfx;

    struct
    {
        s32 ticks;
        tic_channel_data channels[TIC_SOUND_CHANNELS];
        tic_command_data commands[TIC_SOUND_CHANNELS];
        tic_sfx_pos sfxpos[TIC_SOUND_CHANNELS];
        tic_jump_command jump;

    } music;

    tic_tick tick;
    tic_scanline scanline;

    struct
    {
        tic_overline callback;
        u32 raw[TIC_PALETTE_SIZE];
        tic_palette palette;
    } ovr;

    void (*setpix)(tic_mem* memory, s32 x, s32 y, u8 color);
    u8 (*getpix)(tic_mem* memory, s32 x, s32 y);
    void (*drawhline)(tic_mem* memory, s32 xl, s32 xr, s32 y, u8 color);

    u32 synced;

    bool initialized;
} tic_core_state_data;

typedef struct
{
    tic_mem memory; // it should be first

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

#if defined(TIC_BUILD_WITH_SQUIRREL)
        struct SQVM* squirrel;
#endif

    };

    struct
    {
        blip_buffer_t* left;
        blip_buffer_t* right;
    } blip;
    
    s32 samplerate;

    tic_tick_data* data;

    tic_core_state_data state;

    struct
    {
        tic_core_state_data state;   
        tic_ram ram;
        u8 input;

        struct
        {
            u64 start;
            u64 paused;
        } time;
    } pause;

} tic_core;

#if defined(TIC_BUILD_WITH_SQUIRREL)
const tic_script_config* getSquirrelScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_LUA)
const tic_script_config* getLuaScriptConfig();

#   if defined(TIC_BUILD_WITH_MOON)
const tic_script_config* getMoonScriptConfig();
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
const tic_script_config* getFennelConfig();
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */

#if defined(TIC_BUILD_WITH_JS)
const tic_script_config* getJsScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_WREN)
const tic_script_config* getWrenScriptConfig();
#endif

void tic_core_tick_io(tic_mem* memory);
void tic_core_sound_tick_start(tic_mem* memory);
void tic_core_sound_tick_end(tic_mem* memory);
