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
#define TIC_DEFAULT_COLOR 15
#define TIC_SOUND_RINGBUF_LEN 12 // in worst case, this induces ~ 12 tick delay i.e. 200 ms

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
    bool active;
    s32 frame;
    s32 beat;
} tic_jump_command;

typedef struct
{

    struct
    {
        tic80_gamepads previous;
        tic80_gamepads now;

        u32 holds[sizeof(tic80_gamepads) * BITS_IN_BYTE];
    } gamepads;

    struct 
    {
        tic80_keyboard previous;
        tic80_keyboard now;

        u32 holds[tic_keys_count];
    } keyboard;

    struct
    {
        tic_sound_register_data left[TIC_SOUND_CHANNELS];
        tic_sound_register_data right[TIC_SOUND_CHANNELS];
    } registers;

    struct
    {
        tic_sound_register registers[TIC_SOUND_CHANNELS];
        tic_stereo_volume stereo;
    } sound_ringbuf[TIC_SOUND_RINGBUF_LEN];

    u32 sound_ringbuf_head;
    u32 sound_ringbuf_tail;

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
        s32 tempo;
        s32 speed;
    } music;

    tic_tick tick;
    tic_blit_callback callback;
    
    u32 synced;

    struct
    {
        s32 id;
        tic_vram mem;
    } vbank;

    struct ClipRect
    {
        s32 l, t, r, b;
    } clip;

    bool initialized;
} tic_core_state_data;

typedef struct
{
    tic_mem memory; // it should be first
    tic80_pixel_color_format screen_format;

    void* currentVM;
    const tic_script_config* currentScript;

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

void tic_core_tick_io(tic_mem* memory);
void tic_core_sound_tick_start(tic_mem* memory);
void tic_core_sound_tick_end(tic_mem* memory);

// mouse cursor is the same in both modes
// for backward compatibility
#define OVR_COMPAT(CORE, BANK)                                              \
    tic_api_vbank(&CORE->memory, BANK),                                     \
    CORE->memory.ram->vram.vars.cursor = CORE->state.vbank.mem.vars.cursor

#define OVR(CORE)                                   \
    s32 MACROVAR(_bank_) = CORE->state.vbank.id;    \
    OVR_COMPAT(CORE, 1);                            \
    tic_api_cls(&CORE->memory, 0);                  \
    SCOPE(OVR_COMPAT(CORE, MACROVAR(_bank_)))
