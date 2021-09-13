// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "api.h"
#include "core.h"
#include "tilesheet.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>

#ifdef _3DS
#include <3ds.h>
#endif

static_assert(TIC_BANK_BITS == 3,                   "tic_bank_bits");
static_assert(sizeof(tic_map) < 1024 * 32,          "tic_map");
static_assert(sizeof(tic_vram) == TIC_VRAM_SIZE,    "tic_vram");
static_assert(sizeof(tic_ram) == TIC_RAM_SIZE,      "tic_ram");

u8 tic_api_peek(tic_mem* memory, s32 address, s32 res)
{
    if (address < 0)
        return 0;

    const u8* ram = (u8*)&memory->ram;
    enum{RamBits = sizeof(tic_ram) * BITS_IN_BYTE};

    switch(res)
    {
    case 1: if(address < RamBits / 1) return tic_tool_peek1(ram, address);
    case 2: if(address < RamBits / 2) return tic_tool_peek2(ram, address);
    case 4: if(address < RamBits / 4) return tic_tool_peek4(ram, address);
    case 8: if(address < RamBits / 8) return ram[address];
    }

    return 0;
}

void tic_api_poke(tic_mem* memory, s32 address, u8 value, s32 res)
{
    if (address < 0)
        return;

    tic_core* core = (tic_core*)memory;
    u8* ram = (u8*)&memory->ram;
    enum{RamBits = sizeof(tic_ram) * BITS_IN_BYTE};
    
    switch(res)
    {
    case 1: if(address < RamBits / 1) {tic_tool_poke1(ram, address, value); core->state.memmask[address >> 2] = 1;} break;
    case 2: if(address < RamBits / 2) {tic_tool_poke2(ram, address, value); core->state.memmask[address >> 1] = 1;} break;
    case 4: if(address < RamBits / 4) {tic_tool_poke4(ram, address, value); core->state.memmask[address >> 0] = 1;} break;
    case 8: if(address < RamBits / 8) {ram[address] = value; *(u16*)&core->state.memmask[address << 1] = 0x0101;} break;
    }
}

u8 tic_api_peek4(tic_mem* memory, s32 address)
{
    return tic_api_peek(memory, address, 4);
}

void tic_api_poke4(tic_mem* memory, s32 address, u8 value)
{
    tic_api_poke(memory, address, value, 4);
}

void tic_api_memcpy(tic_mem* memory, s32 dst, s32 src, s32 size)
{
    tic_core* core = (tic_core*)memory;
    s32 bound = sizeof(tic_ram) - size;

    if (size >= 0
        && size <= sizeof(tic_ram)
        && dst >= 0
        && src >= 0
        && dst <= bound
        && src <= bound)
    {
        u8* base = (u8*)&memory->ram;
        memcpy(base + dst, base + src, size);
        memcpy(core->state.memmask + (dst << 1), core->state.memmask + (src << 1), size << 1);
    }
}

void tic_api_memset(tic_mem* memory, s32 dst, u8 val, s32 size)
{
    tic_core* core = (tic_core*)memory;
    s32 bound = sizeof(tic_ram) - size;

    if (size >= 0
        && size <= sizeof(tic_ram)
        && dst >= 0
        && dst <= bound)
    {
        u8* base = (u8*)&memory->ram;
        memset(base + dst, val, size);
        memset(core->state.memmask + (dst << 1), 1, size << 1);
    }
}

void tic_api_trace(tic_mem* memory, const char* text, u8 color)
{
    tic_core* core = (tic_core*)memory;
    core->data->trace(core->data->data, text ? text : "nil", color);
}

u32 tic_api_pmem(tic_mem* tic, s32 index, u32 value, bool set)
{
    u32 old = tic->ram.persistent.data[index];

    if (set)
        tic->ram.persistent.data[index] = value;

    return old;
}

void tic_api_exit(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    core->data->exit(core->data->data);
}

static inline void sync(void* dst, void* src, s32 size, bool rev)
{
    if(rev)
        SWAP(dst, src, void*);

    memcpy(dst, src, size);
}

void tic_api_sync(tic_mem* tic, u32 mask, s32 bank, bool toCart)
{
    tic_core* core = (tic_core*)tic;

    static const struct { s32 bank; s32 ram; s32 size; u8 mask; } Sections[] = 
    { 
#define TIC_SYNC_DEF(CART, RAM, ...) { offsetof(tic_bank, CART), offsetof(tic_ram, RAM), sizeof(tic_##CART), tic_sync_##CART },
        TIC_SYNC_LIST(TIC_SYNC_DEF) 
#undef  TIC_SYNC_DEF
    };

    enum { Count = COUNT_OF(Sections), Mask = (1 << Count) - 1 };

    if (mask == 0) mask = Mask;

    mask &= ~core->state.synced & Mask;

    assert(bank >= 0 && bank < TIC_BANKS);

    for (s32 i = 0; i < Count; i++)
        if(mask & Sections[i].mask)
            sync((u8*)&tic->ram + Sections[i].ram, (u8*)&tic->cart.banks[bank] + Sections[i].bank, Sections[i].size, toCart);

    // copy OVR palette
    if (mask & tic_sync_palette)
        sync(&core->state.ovr.palette, &tic->cart.banks[bank].palette.ovr, sizeof(tic_palette), toCart);

    core->state.synced |= mask;
}

double tic_api_time(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    return (double)((core->data->counter(core->data->data) - core->data->start) * 1000) / core->data->freq(core->data->data);
}

s32 tic_api_tstamp(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    return (s32)time(NULL);
}

static void resetPalette(tic_mem* memory)
{
    static const u8 DefaultMapping[] = { 16, 50, 84, 118, 152, 186, 220, 254 };
    memcpy(memory->ram.vram.palette.data, memory->cart.bank0.palette.scn.data, sizeof(tic_palette));
    memcpy(memory->ram.vram.mapping, DefaultMapping, sizeof DefaultMapping);
}

static void resetBlitSegment(tic_mem* memory)
{
    memory->ram.vram.blit.segment = TIC_DEFAULT_BLIT_MODE;
}

static bool compareMetatag(const char* code, const char* tag, const char* value, const char* comment)
{
    bool result = false;

    const char* str = tic_tool_metatag(code, tag, comment);

    if (str)
    {
        result = strcmp(str, value) == 0;
        free((void*)str);
    }

    return result;
}

#define SCRIPT_DEF(name, _, __, vm) const tic_script_config* get_## name ##_script_config();
    SCRIPT_LIST(SCRIPT_DEF)
#undef SCRIPT_DEF

const tic_script_config* tic_core_script_config(tic_mem* memory)
{
    static const struct Config
    {
        const char* name;
        const tic_script_config*(*func)();
    } Configs[] = 
    {
#define SCRIPT_DEF(name, ...) {#name, get_## name ##_script_config},
        SCRIPT_LIST(SCRIPT_DEF)
#undef  SCRIPT_DEF
    };

    FOR(const struct Config*, it, Configs)
        if(compareMetatag(memory->cart.code.data, "script", it->name, it->func()->singleComment))
            return it->func();

    return Configs->func();
}

static void updateSaveid(tic_mem* memory)
{
    memset(memory->saveid, 0, sizeof memory->saveid);
    const char* saveid = tic_tool_metatag(memory->cart.code.data, "saveid", tic_core_script_config(memory)->singleComment);
    if (saveid)
    {
        strncpy(memory->saveid, saveid, TIC_SAVEID_SIZE - 1);
        free((void*)saveid);
    }
}

static void soundClear(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
    {
        static const tic_channel_data EmptyChannel =
        {
            .tick = -1,
            .pos = NULL,
            .index = -1,
            .note = 0,
            .volume = {0, 0},
            .speed = 0,
            .duration = -1,
        };

        memcpy(&core->state.music.channels[i], &EmptyChannel, sizeof EmptyChannel);
        memcpy(&core->state.sfx.channels[i], &EmptyChannel, sizeof EmptyChannel);

        memset(core->state.sfx.channels[i].pos = &memory->ram.sfxpos[i], -1, sizeof(tic_sfx_pos));
        memset(core->state.music.channels[i].pos = &core->state.music.sfxpos[i], -1, sizeof(tic_sfx_pos));
    }

    memset(&memory->ram.registers, 0, sizeof memory->ram.registers);
    memset(memory->samples.buffer, 0, memory->samples.size);

    tic_api_music(memory, -1, 0, 0, false, false, -1, -1);
}

void tic_api_reset(tic_mem* memory)
{
    resetPalette(memory);
    resetBlitSegment(memory);

    memset(&memory->ram.vram.vars, 0, sizeof memory->ram.vram.vars);
    memory->ram.input.mouse.relative = 0;

    tic_api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

    soundClear(memory);

    tic_core* core = (tic_core*)memory;
    core->state.initialized = false;
    ZEROMEM(core->state.callback);

    updateSaveid(memory);
}

static void cart2ram(tic_mem* memory)
{
    static const u8 Font[] =
    {
        #include "font.inl"
    };

    memcpy(memory->ram.font.data, Font, sizeof Font);

    enum
    {
#define     TIC_SYNC_DEF(NAME, _, INDEX) sync_##NAME = INDEX,
            TIC_SYNC_LIST(TIC_SYNC_DEF)
#undef      TIC_SYNC_DEF
            count,
            all = (1 << count) - 1,
            noscreen = BITCLEAR(all, sync_screen)
    };

    // don't sync empty screen
    tic_api_sync(memory, EMPTY(memory->cart.bank0.screen.data) ? noscreen : all, 0, false);
}

void tic_core_tick(tic_mem* tic, tic_tick_data* data)
{
    tic_core* core = (tic_core*)tic;

    core->data = data;

    if (!core->state.initialized)
    {
        const char* code = tic->cart.code.data;

        bool done = false;
        const tic_script_config* config = tic_core_script_config(tic);

        if (strlen(code))
        {
            cart2ram(tic);

            core->state.synced = 0;
            tic->input.data = 0;

            if (compareMetatag(code, "input", "mouse", config->singleComment))
                tic->input.mouse = 1;
            else if (compareMetatag(code, "input", "gamepad", config->singleComment))
                tic->input.gamepad = 1;
            else if (compareMetatag(code, "input", "keyboard", config->singleComment))
                tic->input.keyboard = 1;
            else tic->input.data = -1;  // default is all enabled

            data->start = data->counter(core->data->data);

            done = config->init(tic, code);
        }
        else
        {
            core->data->error(core->data->data, "the code is empty");
        }

        if (done)
        {
            core->state.tick = config->tick;
            core->state.callback = config->callback;
            core->state.initialized = true;
        }
        else return;
    }

    core->state.tick(tic);
}

void tic_core_pause(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    memcpy(&core->pause.state, &core->state, sizeof(tic_core_state_data));
    memcpy(&core->pause.ram, &memory->ram, sizeof(tic_ram));
    memset(&core->state.ovr, 0, sizeof core->state.ovr);

    if (core->data)
    {
        core->pause.time.start = core->data->start;
        core->pause.time.paused = core->data->counter(core->data->data);
    }
}

void tic_core_resume(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    if (core->data)
    {
        memcpy(&core->state, &core->pause.state, sizeof(tic_core_state_data));
        memcpy(&memory->ram, &core->pause.ram, sizeof(tic_ram));
        core->data->start = core->pause.time.start + core->data->counter(core->data->data) - core->pause.time.paused;
    }
}

void tic_core_close(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    core->state.initialized = false;

#define SCRIPT_DEF(name, ...) get_## name ##_script_config()->close(memory);
    SCRIPT_LIST(SCRIPT_DEF)
#undef  SCRIPT_DEF

    blip_delete(core->blip.left);
    blip_delete(core->blip.right);

    free(memory->samples.buffer);
    free(core);
}

void tic_core_tick_start(tic_mem* memory)
{
    tic_core_sound_tick_start(memory);
    tic_core_tick_io(memory);

    tic_core* core = (tic_core*)memory;
    core->state.synced = 0;
}

void tic_core_tick_end(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    tic80_input* input = &core->memory.ram.input;

    core->state.gamepads.previous.data = input->gamepads.data;
    core->state.keyboard.previous.data = input->keyboard.data;

    tic_core_sound_tick_end(memory);
}

// copied from SDL2
static inline void memset4(void* dst, u32 val, u32 dwords)
{
#if defined(__GNUC__) && defined(i386)
    s32 u0, u1, u2;
    __asm__ __volatile__(
        "cld \n\t"
        "rep ; stosl \n\t"
        : "=&D" (u0), "=&a" (u1), "=&c" (u2)
        : "0" (dst), "1" (val), "2" (dwords)
        : "memory"
    );
#else
    u32 _n = (dwords + 3) / 4;
    u32* _p = (u32*)dst;
    u32 _val = (val);
    if (dwords == 0)
        return;
    switch (dwords % 4)
    {
    case 0: do {
        *_p++ = _val;
    case 3:         *_p++ = _val;
    case 2:         *_p++ = _val;
    case 1:         *_p++ = _val;
    } while (--_n);
    }
#endif
}

void tic_core_blit_ex(tic_mem* tic, tic_blit_callback clb)
{
    tic_core* core = (tic_core*)tic;

    tic_palette ovrpal;
    tic80_pixel_color_format fmt = tic->screen_format;

    if(clb.overline)
    {
        memcpy(&ovrpal, EMPTY(core->state.ovr.palette.data) 
            ? &tic->ram.vram.palette 
            : &core->state.ovr.palette
            , sizeof ovrpal);
    }

    {
#define UPDBRD()                                                        \
        if(clb.border)                                                  \
        {                                                               \
            clb.border(tic, row, clb.data);                             \
            pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);   \
        }

        if (clb.scanline)
            clb.scanline(tic, 0, clb.data);

        const u32* pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);

        s32 row = 0;
        u32* rowPtr = tic->screen;

        for(; row < TIC80_MARGIN_TOP; ++row, rowPtr += TIC80_FULLWIDTH)
        {
            UPDBRD();
            memset4(rowPtr, pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH);
        }

        for (; row < TIC80_FULLHEIGHT - TIC80_MARGIN_BOTTOM; ++row, rowPtr += TIC80_FULLWIDTH)
        {
            UPDBRD();

            memset4(rowPtr, pal[tic->ram.vram.vars.border], TIC80_MARGIN_LEFT);
            memset4(rowPtr + (TIC80_FULLWIDTH - TIC80_MARGIN_RIGHT), pal[tic->ram.vram.vars.border], TIC80_MARGIN_RIGHT);

            for (s32 i = (row + tic->ram.vram.vars.offset.y + (TIC80_HEIGHT - TIC80_MARGIN_TOP)) % TIC80_HEIGHT * TIC80_WIDTH, 
                end = i + TIC80_WIDTH, x = (TIC80_WIDTH - tic->ram.vram.vars.offset.x) % TIC80_WIDTH; i < end; ++i, ++x)
                rowPtr[TIC80_MARGIN_LEFT + x % TIC80_WIDTH] = pal[tic_tool_peek4(tic->ram.vram.screen.data, i)];

            if (clb.scanline && (row < TIC80_HEIGHT + TIC80_MARGIN_TOP - 1))
            {
                clb.scanline(tic, row - (TIC80_MARGIN_TOP - 1), clb.data);
                pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);
            }
        }

        for(; row < TIC80_FULLHEIGHT; ++row, rowPtr += TIC80_FULLWIDTH)
        {
            UPDBRD();
            memset4(rowPtr, pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH);
        }

#undef  UPDBRD
    }

    if (clb.overline)
    {
        ZEROMEM(core->state.memmask);

        tic_palette scnpal;
        memcpy(&scnpal, &tic->ram.vram.palette, sizeof scnpal);
        memcpy(&tic->ram.vram.palette, &ovrpal, sizeof ovrpal);

        {
            clb.overline(tic, clb.data);

            const u32* pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);

            u32* out = tic->screen + TIC80_MARGIN_TOP * TIC80_FULLWIDTH + TIC80_MARGIN_LEFT;
            u8* memmask = core->state.memmask;
            for(s32 pos = 0; pos != TIC80_WIDTH * TIC80_HEIGHT; pos += TIC80_WIDTH, out += TIC80_MARGIN_LEFT + TIC80_MARGIN_RIGHT)
                for (s32 i = pos, end = pos + TIC80_WIDTH; i != end; ++i, ++out)
                    if(*memmask++)
                        *out = pal[tic_tool_peek4(tic->ram.vram.screen.data, i)];
        }

        memcpy(&core->state.ovr.palette, &tic->ram.vram.palette, sizeof(tic_palette));
        memcpy(&tic->ram.vram.palette, &scnpal, sizeof scnpal);
    }
}

static inline void scanline(tic_mem* memory, s32 row, void* data)
{
    tic_core* core = (tic_core*)memory;

    if (core->state.initialized)
        core->state.callback.scanline(memory, row, data);
}

static inline void border(tic_mem* memory, s32 row, void* data)
{
    tic_core* core = (tic_core*)memory;

    if (core->state.initialized)
        core->state.callback.border(memory, row, data);
}

static inline void overline(tic_mem* memory, void* data)
{
    tic_core* core = (tic_core*)memory;

    if (core->state.initialized)
        core->state.callback.overline(memory, data);
}

void tic_core_blit(tic_mem* tic)
{
    tic_core_blit_ex(tic, (tic_blit_callback){scanline, overline, border, NULL});
}

tic_mem* tic_core_create(s32 samplerate)
{
    tic_core* core = (tic_core*)malloc(sizeof(tic_core));
    memset(core, 0, sizeof(tic_core));

    if (core != (tic_core*)&core->memory)
    {
        free(core);
        return NULL;
    }

    core->memory.screen_format = TIC80_PIXEL_COLOR_RGBA8888;
    core->samplerate = samplerate;
#ifdef _3DS
    // To feed texture data directly to the 3DS GPU, linearly allocated memory is required, which is
    // not guaranteed by malloc.
    // Additionally, allocate TIC80_FULLHEIGHT + 1 lines to minimize glitches in linear scaling mode.
    core->memory.screen = linearAlloc(TIC80_FULLWIDTH * (TIC80_FULLHEIGHT + 1) * sizeof(u32));
#endif
    core->memory.samples.size = samplerate * TIC_STEREO_CHANNELS / TIC80_FRAMERATE * sizeof(s16);
    core->memory.samples.buffer = malloc(core->memory.samples.size);

    core->blip.left = blip_new(samplerate / 10);
    core->blip.right = blip_new(samplerate / 10);

    blip_set_rates(core->blip.left, CLOCKRATE, samplerate);
    blip_set_rates(core->blip.right, CLOCKRATE, samplerate);

    tic_api_reset(&core->memory);

    return &core->memory;
}
