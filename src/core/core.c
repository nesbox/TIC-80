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

#include "tic_assert.h"

#ifdef _3DS
#include <3ds.h>
#endif

static_assert(TIC_BANK_BITS == 3,                   "tic_bank_bits");
static_assert(sizeof(tic_map) < 1024 * 32,          "tic_map");
static_assert(sizeof(tic_rgb) == 3,    "tic_rgb");
static_assert(sizeof(tic_palette) == 48,    "tic_palette");
static_assert(sizeof(((tic_vram *)0)->vars) == 4, "tic_vram vars");
static_assert(sizeof(tic_vram) == TIC_VRAM_SIZE,    "tic_vram");
static_assert(sizeof(tic_ram) == TIC_RAM_SIZE,      "tic_ram");

u8 tic_api_peek(tic_mem* memory, s32 address, s32 bits)
{
    if (address < 0)
        return 0;

    const u8* ram = (u8*)memory->ram;
    enum{RamBits = sizeof(tic_ram) * BITS_IN_BYTE};

    switch(bits)
    {
    case 1: if(address < RamBits / 1) return tic_tool_peek1(ram, address);
    case 2: if(address < RamBits / 2) return tic_tool_peek2(ram, address);
    case 4: if(address < RamBits / 4) return tic_tool_peek4(ram, address);
    case 8: if(address < RamBits / 8) return ram[address];
    }

    return 0;
}

void tic_api_poke(tic_mem* memory, s32 address, u8 value, s32 bits)
{
    if (address < 0)
        return;

    tic_core* core = (tic_core*)memory;
    u8* ram = (u8*)memory->ram;
    enum{RamBits = sizeof(tic_ram) * BITS_IN_BYTE};
    
    switch(bits)
    {
    case 1: if(address < RamBits / 1) tic_tool_poke1(ram, address, value); break;
    case 2: if(address < RamBits / 2) tic_tool_poke2(ram, address, value); break;
    case 4: if(address < RamBits / 4) tic_tool_poke4(ram, address, value); break;
    case 8: if(address < RamBits / 8) ram[address] = value; break;
    }
}

u8 tic_api_peek4(tic_mem* memory, s32 address)
{
    return tic_api_peek(memory, address, 4);
}

u8 tic_api_peek1(tic_mem* memory, s32 address)
{
    return tic_api_peek(memory, address, 1);
}

void tic_api_poke1(tic_mem* memory, s32 address, u8 value)
{
    tic_api_poke(memory, address, value, 1);
}

u8 tic_api_peek2(tic_mem* memory, s32 address)
{
    return tic_api_peek(memory, address, 2);
}

void tic_api_poke2(tic_mem* memory, s32 address, u8 value)
{
    tic_api_poke(memory, address, value, 2);
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
        u8* base = (u8*)memory->ram;
        memcpy(base + dst, base + src, size);
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
        u8* base = (u8*)memory->ram;
        memset(base + dst, val, size);
    }
}

void tic_api_trace(tic_mem* memory, const char* text, u8 color)
{
    tic_core* core = (tic_core*)memory;
    core->data->trace(core->data->data, text ? text : "nil", color);
}

u32 tic_api_pmem(tic_mem* tic, s32 index, u32 value, bool set)
{
    u32 old = tic->ram->persistent.data[index];

    if (set)
        tic->ram->persistent.data[index] = value;

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
            sync((u8*)tic->ram + Sections[i].ram, (u8*)&tic->cart.banks[bank] + Sections[i].bank, Sections[i].size, toCart);

    core->state.synced |= mask;
}

double tic_api_time(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    return (double)(core->data->counter(core->data->data) - core->data->start) * 1000.0 / core->data->freq(core->data->data);
}

s32 tic_api_tstamp(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    return (s32)time(NULL);
}

static bool compareMetatag(const char* code, const char* tag, const char* value, const char* comment)
{
    bool result = false;

    char* str = tic_tool_metatag(code, tag, comment);

    if (str)
    {
        result = strcmp(str, value) == 0;
        free(str);
    }

    return result;
}

const tic_script_config* tic_core_script_config(tic_mem* memory)
{
    FOR_EACH_LANG(it)
    {
        if(it->id == memory->cart.lang || compareMetatag(memory->cart.code.data, "script", it->name, it->singleComment))
            return it;
    }
    FOR_EACH_LANG_END

    return Languages[0];
}

static void updateSaveid(tic_mem* memory)
{
    memset(memory->saveid, 0, sizeof memory->saveid);
    char* saveid = tic_tool_metatag(memory->cart.code.data, "saveid", tic_core_script_config(memory)->singleComment);
    if (saveid)
    {
        strncpy(memory->saveid, saveid, TIC_SAVEID_SIZE - 1);
        free(saveid);
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

        memset(core->state.sfx.channels[i].pos = &memory->ram->sfxpos[i], -1, sizeof(tic_sfx_pos));
        memset(core->state.music.channels[i].pos = &core->state.music.sfxpos[i], -1, sizeof(tic_sfx_pos));
    }

    memset(&memory->ram->registers, 0, sizeof memory->ram->registers);
    memset(memory->product.samples.buffer, 0, memory->product.samples.count * TIC80_SAMPLESIZE);

    tic_api_music(memory, -1, 0, 0, false, false, -1, -1);
}

static void resetVbank(tic_mem* memory)
{
    ZEROMEM(memory->ram->vram.vars);

    static const u8 DefaultMapping[] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
    memcpy(memory->ram->vram.mapping, DefaultMapping, sizeof DefaultMapping);
    memory->ram->vram.palette = memory->cart.bank0.palette.vbank0;
    memory->ram->vram.blit.segment = TIC_DEFAULT_BLIT_MODE;
}

static void font2ram(tic_mem* memory)
{
  memory->ram->font = (tic_font) {
        .regular =     
        {
            .data = 
            {
                #include "font.inl"
            },
	    {
	      {
		.width = TIC_FONT_WIDTH, 
		.height = TIC_FONT_HEIGHT,
	      }
	    } 
        },

        .alt = 
        {
            .data = 
            {
                #include "altfont.inl"
            },
	    {
	      {
		.width = TIC_ALTFONT_WIDTH, 
		.height = TIC_FONT_HEIGHT, 
	      }
	    }
        },
  };
}

void tic_api_reset(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    // keyboard state is critical and must be preserved across API resets.
    // Often `tic_api_reset` is called to effect transitions between modes
    // yet we still need to know when the key WAS pressed after the
    // transition - to prevent it from counting as a second keypress.
    //
    // So why presev `now` not `previous`?  this is most often called in
    // the middle of a tick... so we preserve now, which during `tick_end`
    // is copied to previous. This duplicates the prior behavior of
    // `ram.input.keyboard` (which existing outside `state`).
    u32 kb_now = core->state.keyboard.now.data;
    ZEROMEM(core->state);
    core->state.keyboard.now.data = kb_now;
    tic_api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

    resetVbank(memory);

    VBANK(memory, 1)
    {
        resetVbank(memory);

        // init VBANK1 palette with VBANK0 palette if it's empty
        // for backward compatibility
        if(!EMPTY(memory->cart.bank0.palette.vbank1.data))
            memcpy(&memory->ram->vram.palette, &memory->cart.bank0.palette.vbank1, sizeof(tic_palette));
    }

    memory->ram->input.mouse.relative = 0;

    soundClear(memory);
    updateSaveid(memory);
    font2ram(memory);
}

static void cart2ram(tic_mem* memory)
{
    font2ram(memory);

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

static void tic_close_current_vm(tic_core* core)
{
    // close previous VM if any
    if(core->currentVM)
    {
        // printf("Closing VM of %s, %d\n", core->currentScript->name, core->currentVM);
        core->currentScript->close( (tic_mem*)core );
        core->currentVM = NULL;
    }
    if (core->memory.ram == NULL) {
        core->memory.ram = core->memory.base_ram;
    }
}

static bool tic_init_vm(tic_core* core, const char* code, const tic_script_config* config)
{
    tic_close_current_vm(core);
    // set current script config and init
    core->currentScript = config;
    bool done = config->init( (tic_mem*) core , code);
    if(!done)
    {
        // if it couldn't init, make sure the VM is not left dirty by the implementation
        core->currentVM = NULL;
    }
    else
    {
        //printf("Initialized VM of %s, %d\n", core->currentScript->name, core->currentVM);
    }
    return done;
}

s32 tic_api_vbank(tic_mem* tic, s32 bank)
{
    tic_core* core = (tic_core*)tic;

    s32 prev = core->state.vbank.id;

    switch(bank)
    {
    case 0:
    case 1:
        if(core->state.vbank.id != bank)
        {
            SWAP(tic->ram->vram, core->state.vbank.mem, tic_vram);
            core->state.vbank.id = bank;
        }
    }

    return prev;
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

            // TODO: does where to fetch code from need to be a config option so this isn't hard
            // coded for just a single langage? perhaps change it later when we have a second script
            // engine that uses BINARY?
            if (strcmp(config->name,"wasm")==0) {
                code = tic->cart.binary.data;
            }

            done = tic_init_vm(core, code, config);
        }
        else
        {
            core->data->error(core->data->data, "the code is empty");
        }

        if (done)
        {
            config->boot(tic);
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
    memcpy(&core->pause.ram, memory->ram, sizeof(tic_ram));
    core->pause.input = memory->input.data;

    if (core->data)
    {
        core->pause.time.start = core->data->start;
        core->pause.time.paused = core->data->counter(core->data->data);;
    }
}

void tic_core_resume(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    if (core->data)
    {
        memcpy(&core->state, &core->pause.state, sizeof(tic_core_state_data));
        memcpy(memory->ram, &core->pause.ram, sizeof(tic_ram));
        core->data->start = core->pause.time.start + core->data->counter(core->data->data) - core->pause.time.paused;
        memory->input.data = core->pause.input;
    }
}

void tic_core_close(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    core->state.initialized = false;

    tic_close_current_vm(core);

    blip_delete(core->blip.left);
    blip_delete(core->blip.right);

    free(memory->product.screen);
    free(memory->product.samples.buffer);
    free(core);
}

void tic_core_tick_start(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    tic_core_sound_tick_start(memory);
    tic_core_tick_io(memory);

    // SECURITY: preserve the system keyboard/game controller input state
    // (and restore it post-tick, see below) to prevent user cartridges
    // from being able to corrupt and take control of the inputs in
    // nefarious ways.
    //
    // Related: https://github.com/nesbox/TIC-80/issues/1785
    core->state.keyboard.now.data = core->memory.ram->input.keyboard.data;
    core->state.gamepads.now.data = core->memory.ram->input.gamepads.data;

    core->state.synced = 0;
}

void tic_core_tick_end(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    tic80_input* input = &core->memory.ram->input;

    core->state.gamepads.previous.data = input->gamepads.data;
    // SECURITY: we do not use `memory.ram.input` here because it is
    // untrustworthy since the cartridge could have modified it to 
    // inject artificial keyboard/gamepad events.
    core->state.keyboard.previous.data = core->state.keyboard.now.data;
    core->state.gamepads.previous.data = core->state.gamepads.now.data;

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

static inline tic_vram* vbank0(tic_core* core)
{
    return core->state.vbank.id ? &core->state.vbank.mem : &core->memory.ram->vram;
}

static inline tic_vram* vbank1(tic_core* core)
{
    return core->state.vbank.id ? &core->memory.ram->vram : &core->state.vbank.mem;
}

static inline void updpal(tic_mem* tic, tic_blitpal* pal0, tic_blitpal* pal1)
{
    tic_core* core = (tic_core*)tic;
    *pal0 = tic_tool_palette_blit(&vbank0(core)->palette, core->screen_format);
    *pal1 = tic_tool_palette_blit(&vbank1(core)->palette, core->screen_format);
}

static inline void updbdr(tic_mem* tic, s32 row, u32* ptr, tic_blit_callback clb, tic_blitpal* pal0, tic_blitpal* pal1)
{
    tic_core* core = (tic_core*)tic;

    if(clb.border) clb.border(tic, row, clb.data);

    if(clb.scanline)
    {
        if(row == 0) clb.scanline(tic, 0, clb.data);
        else if(row > TIC80_MARGIN_TOP && row < (TIC80_HEIGHT + TIC80_MARGIN_TOP))
            clb.scanline(tic, row - TIC80_MARGIN_TOP, clb.data);
    }

    if(clb.border || clb.scanline)
        updpal(tic, pal0, pal1);

    memset4(ptr, pal0->data[vbank0(core)->vars.border], TIC80_FULLWIDTH);
}

static inline u32 blitpix(tic_mem* tic, s32 offset0, s32 offset1, const tic_blitpal* pal0, const tic_blitpal* pal1)
{
    tic_core* core = (tic_core*)tic;
    u32 pix = tic_tool_peek4(vbank1(core)->screen.data, offset1);

    return pix != vbank1(core)->vars.clear
        ? pal1->data[pix]
        : pal0->data[tic_tool_peek4(vbank0(core)->screen.data, offset0)];
}

void tic_core_blit_ex(tic_mem* tic, tic_blit_callback clb)
{
    tic_core* core = (tic_core*)tic;

    tic_blitpal pal0, pal1;
    updpal(tic, &pal0, &pal1);

    s32 row = 0;
    u32* rowPtr = tic->product.screen;

#define UPDBDR() updbdr(tic, row, rowPtr, clb, &pal0, &pal1)

    for(; row != TIC80_MARGIN_TOP; ++row, rowPtr += TIC80_FULLWIDTH)
        UPDBDR();

    for(; row != TIC80_FULLHEIGHT - TIC80_MARGIN_BOTTOM; ++row)
    {
        UPDBDR();
        rowPtr += TIC80_MARGIN_LEFT;

        if(*(u16*)&vbank0(core)->vars.offset == 0 && *(u16*)&vbank1(core)->vars.offset == 0)
        {
            // render line without XY offsets
            for(s32 x = (row - TIC80_MARGIN_TOP) * TIC80_WIDTH, end = x + TIC80_WIDTH; x != end; ++x)
                *rowPtr++ = blitpix(tic, x, x, &pal0, &pal1);
        }
        else
        {
            // render line with XY offsets
            enum{OffsetY = TIC80_HEIGHT - TIC80_MARGIN_TOP};
            s32 start0 = (row - vbank0(core)->vars.offset.y + OffsetY) % TIC80_HEIGHT * TIC80_WIDTH;
            s32 start1 = (row - vbank1(core)->vars.offset.y + OffsetY) % TIC80_HEIGHT * TIC80_WIDTH;
            s32 offsetX0 = vbank0(core)->vars.offset.x;
            s32 offsetX1 = vbank1(core)->vars.offset.x;

            for(s32 x = TIC80_WIDTH; x != 2 * TIC80_WIDTH; ++x)
                *rowPtr++ = blitpix(tic, (x - offsetX0) % TIC80_WIDTH + start0, 
                    (x - offsetX1) % TIC80_WIDTH + start1, &pal0, &pal1);
        }

        rowPtr += TIC80_MARGIN_RIGHT;
    }

    for(; row != TIC80_FULLHEIGHT; ++row, rowPtr += TIC80_FULLWIDTH)
        UPDBDR();

#undef  UPDBDR
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

void tic_core_blit(tic_mem* tic)
{
    tic_core_blit_ex(tic, (tic_blit_callback){scanline, border, NULL});
}

tic_mem* tic_core_create(s32 samplerate, tic80_pixel_color_format format)
{
    tic_core* core = (tic_core*)malloc(sizeof(tic_core));
    memset(core, 0, sizeof(tic_core));

    tic80* product = &core->memory.product;

    core->screen_format = format;
    core->memory.ram = (tic_ram*)malloc(TIC_RAM_SIZE);
    core->memory.base_ram = core->memory.ram;
    core->samplerate = samplerate;

    memset(core->memory.ram, 0, sizeof(tic_ram));
#ifdef _3DS
    // To feed texture data directly to the 3DS GPU, linearly allocated memory is required, which is
    // not guaranteed by malloc.
    // Additionally, allocate TIC80_FULLHEIGHT + 1 lines to minimize glitches in linear scaling mode.
    product->screen = linearAlloc(TIC80_FULLWIDTH * (TIC80_FULLHEIGHT + 1) * sizeof(u32));
#else
    product->screen = malloc(TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof product->screen[0]);
#endif
    product->samples.count = samplerate * TIC80_SAMPLE_CHANNELS / TIC80_FRAMERATE;
    product->samples.buffer = malloc(product->samples.count * TIC80_SAMPLESIZE);

    core->blip.left = blip_new(samplerate / 10);
    core->blip.right = blip_new(samplerate / 10);

    blip_set_rates(core->blip.left, CLOCKRATE, samplerate);
    blip_set_rates(core->blip.right, CLOCKRATE, samplerate);

    tic_api_reset(&core->memory);

    return &core->memory;
}
