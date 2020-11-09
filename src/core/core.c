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

#ifdef _3DS
#include <3ds.h>
#endif

STATIC_ASSERT(tic_bank_bits, TIC_BANK_BITS == 3);
STATIC_ASSERT(tic_map, sizeof(tic_map) < 1024 * 32);
STATIC_ASSERT(tic_vram, sizeof(tic_vram) == TIC_VRAM_SIZE);
STATIC_ASSERT(tic_ram, sizeof(tic_ram) == TIC_RAM_SIZE);

static inline u32* getOvrAddr(tic_mem* tic, s32 x, s32 y)
{
    enum { Top = (TIC80_FULLHEIGHT - TIC80_HEIGHT) / 2 };
    enum { Left = (TIC80_FULLWIDTH - TIC80_WIDTH) / 2 };

    return tic->screen + x + (y << TIC80_FULLWIDTH_BITS) + (Left + Top * TIC80_FULLWIDTH);
}

static void setPixelOvr(tic_mem* tic, s32 x, s32 y, u8 color)
{
    tic_core* core = (tic_core*)tic;

    *getOvrAddr(tic, x, y) = *(core->state.ovr.raw + color);
}

static u8 getPixelOvr(tic_mem* tic, s32 x, s32 y)
{
    tic_core* core = (tic_core*)tic;

    u32 color = *getOvrAddr(tic, x, y);
    u32* pal = core->state.ovr.raw;

    for (s32 i = 0; i < TIC_PALETTE_SIZE; i++, pal++)
        if (*pal == color)
            return i;

    return 0;
}

static void drawHLineOvr(tic_mem* tic, s32 x1, s32 x2, s32 y, u8 color)
{
    tic_core* core = (tic_core*)tic;
    u32 final_color = *(core->state.ovr.raw + color);
    for (s32 x = x1; x < x2; ++x) {
        *getOvrAddr(tic, x, y) = final_color;
    }
}

u8 tic_api_peek(tic_mem* memory, s32 address)
{
    if (address >= 0 && address < sizeof(tic_ram))
        return *((u8*)&memory->ram + address);

    return 0;
}

void tic_api_poke(tic_mem* memory, s32 address, u8 value)
{
    if (address >= 0 && address < sizeof(tic_ram))
        *((u8*)&memory->ram + address) = value;
}

u8 tic_api_peek4(tic_mem* memory, s32 address)
{
    if (address >= 0 && address < sizeof(tic_ram) * 2)
        return tic_tool_peek4((u8*)&memory->ram, address);

    return 0;
}

void tic_api_poke4(tic_mem* memory, s32 address, u8 value)
{
    if (address >= 0 && address < sizeof(tic_ram) * 2)
        tic_tool_poke4((u8*)&memory->ram, address, value);
}

void tic_api_memcpy(tic_mem* memory, s32 dst, s32 src, s32 size)
{
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
    }
}

void tic_api_memset(tic_mem* memory, s32 dst, u8 val, s32 size)
{
    s32 bound = sizeof(tic_ram) - size;

    if (size >= 0
        && size <= sizeof(tic_ram)
        && dst >= 0
        && dst <= bound)
    {
        u8* base = (u8*)&memory->ram;
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

    static const struct { s32 bank; s32 ram; s32 size; } Sections[] =
    {
        {offsetof(tic_bank, tiles),         offsetof(tic_ram, tiles),           sizeof(tic_tiles)   },
        {offsetof(tic_bank, sprites),       offsetof(tic_ram, sprites),         sizeof(tic_tiles)   },
        {offsetof(tic_bank, map),           offsetof(tic_ram, map),             sizeof(tic_map)     },
        {offsetof(tic_bank, sfx),           offsetof(tic_ram, sfx),             sizeof(tic_sfx)     },
        {offsetof(tic_bank, music),         offsetof(tic_ram, music),           sizeof(tic_music)   },
        {offsetof(tic_bank, palette.scn),   offsetof(tic_ram, vram.palette),    sizeof(tic_palette) },
        {offsetof(tic_bank, flags),         offsetof(tic_ram, flags),           sizeof(tic_flags)   },
    };

    enum { Count = COUNT_OF(Sections), Mask = (1 << Count) - 1 };

    if (mask == 0) mask = Mask;

    mask &= ~core->state.synced & Mask;

    assert(bank >= 0 && bank < TIC_BANKS);

    for (s32 i = 0; i < Count; i++)
        if(mask & (1 << i))
            sync((u8*)&tic->ram + Sections[i].ram, (u8*)&tic->cart.banks[bank] + Sections[i].bank, Sections[i].size, toCart);

    // copy OVR palette
    {
        enum { PaletteIndex = 5 };

        if (mask & (1 << PaletteIndex))
            sync(&core->state.ovr.palette, &tic->cart.banks[bank].palette.ovr, sizeof(tic_palette), toCart);
    }

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

static void setPixelDma(tic_mem* tic, s32 x, s32 y, u8 color)
{
    tic_tool_poke4(tic->ram.vram.screen.data, y * TIC80_WIDTH + x, color);
}

static u8 getPixelDma(tic_mem* tic, s32 x, s32 y)
{
    tic_core* core = (tic_core*)tic;

    return tic_tool_peek4(core->memory.ram.vram.screen.data, y * TIC80_WIDTH + x);
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
    u8* screen = memory->ram.vram.screen.data + ((y * TIC80_WIDTH + xl) >> 1);
    for (s32 i = 0; i < count; i++) *screen++ = color;
    if (xr & 1) {
        tic_tool_poke4(&memory->ram.vram.screen.data, y * TIC80_WIDTH + xr - 1, color);
    }
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

static const char* readMetatag(const char* code, const char* tag, const char* comment)
{
    const char* start = NULL;

    {
        static char format[] = "%s %s:";

        char* tagBuffer = malloc(strlen(format) + strlen(tag));

        if (tagBuffer)
        {
            sprintf(tagBuffer, format, comment, tag);
            if ((start = strstr(code, tagBuffer)))
                start += strlen(tagBuffer);
            free(tagBuffer);
        }
    }

    if (start)
    {
        const char* end = strstr(start, "\n");

        if (end)
        {
            while (*start <= ' ' && start < end) start++;
            while (*(end - 1) <= ' ' && end > start) end--;

            const s32 size = (s32)(end - start);

            char* value = (char*)malloc(size + 1);

            if (value)
            {
                memset(value, 0, size + 1);
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

    if (str)
    {
        result = strcmp(str, value) == 0;
        free((void*)str);
    }

    return result;
}

const tic_script_config* tic_core_script_config(tic_mem* memory)
{
    const char* code = memory->cart.code.data;

#if defined(TIC_BUILD_WITH_MOON)
    if (compareMetatag(code, "script", "moon", getMoonScriptConfig()->singleComment) ||
        compareMetatag(code, "script", "moonscript", getMoonScriptConfig()->singleComment))
        return getMoonScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_FENNEL)
    if (compareMetatag(code, "script", "fennel", getFennelConfig()->singleComment))
        return getFennelConfig();
#endif

#if defined(TIC_BUILD_WITH_JS)
    if (compareMetatag(code, "script", "js", getJsScriptConfig()->singleComment) ||
        compareMetatag(code, "script", "javascript", getJsScriptConfig()->singleComment))
        return getJsScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_WREN)
    if (compareMetatag(code, "script", "wren", getWrenScriptConfig()->singleComment))
        return getWrenScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    if (compareMetatag(code, "script", "squirrel", getSquirrelScriptConfig()->singleComment))
        return getSquirrelScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_LUA)
    return getLuaScriptConfig();
#elif defined(TIC_BUILD_WITH_JS)
    return getJsScriptConfig();
#elif defined(TIC_BUILD_WITH_WREN)
    return getWrenScriptConfig();
#elif defined(TIC_BUILD_WITH_SQUIRREL)
    return getSquirrelScriptConfig();
#endif
}

static void updateSaveid(tic_mem* memory)
{
    memset(memory->saveid, 0, sizeof memory->saveid);
    const char* saveid = readMetatag(memory->cart.code.data, "saveid", tic_core_script_config(memory)->singleComment);
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

    tic_api_music(memory, -1, 0, 0, false, false);
}

static void resetDma(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    core->state.setpix = setPixelDma;
    core->state.getpix = getPixelDma;
    core->state.drawhline = drawHLineDma;
}

void tic_api_reset(tic_mem* memory)
{
    resetPalette(memory);
    resetBlitSegment(memory);

    memset(&memory->ram.vram.vars, 0, sizeof memory->ram.vram.vars);

    tic_api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

    soundClear(memory);

    tic_core* core = (tic_core*)memory;
    core->state.initialized = false;
    core->state.scanline = NULL;
    core->state.ovr.callback = NULL;

    resetDma(memory);

    updateSaveid(memory);
}

static void initCover(tic_mem* tic)
{
    const tic_cover_image* cover = &tic->cart.cover;

    if (cover->size)
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
                    u8 color = tic_tool_find_closest_color(tic->cart.bank0.palette.scn.colors, c);
                    tic_tool_poke4(tic->ram.vram.screen.data, i, color);
                }
            }

            gif_close(image);
        }
    }
}

static void cart2ram(tic_mem* memory)
{
    static const u8 Font[] =
    {
        #include "font.inl"
    };

    memcpy(memory->ram.font.data, Font, sizeof Font);

    tic_api_sync(memory, 0, 0, false);
    initCover(memory);
}

void tic_core_tick(tic_mem* tic, tic_tick_data* data)
{
    tic_core* core = (tic_core*)tic;

    core->data = data;

    if (!core->state.initialized)
    {
        const char* code = tic->cart.code.data;

        bool done = false;
        const tic_script_config* config = NULL;

        if (strlen(code))
        {
            config = tic_core_script_config(tic);
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
            core->state.scanline = config->scanline;
            core->state.ovr.callback = config->overline;

            core->state.initialized = true;
        }
        else return;
    }

    {
        if (!tic->input.keyboard)
            ZEROMEM(tic->ram.input.keyboard);

        if (!tic->input.gamepad)
            ZEROMEM(tic->ram.input.gamepads);

        if (!tic->input.mouse)
            ZEROMEM(tic->ram.input.mouse);
    }

    core->state.tick(tic);
}

void tic_core_pause(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    memcpy(&core->pause.state, &core->state, sizeof(tic_core_state_data));
    memcpy(&core->pause.ram, &memory->ram, sizeof(tic_ram));
    core->pause.input = memory->input.data;
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
        memory->input.data = core->pause.input;
        core->data->start = core->pause.time.start + core->data->counter(core->data->data) - core->pause.time.paused;
    }
}

void tic_core_close(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    core->state.initialized = false;

#if defined(TIC_BUILD_WITH_SQUIRREL)
    getSquirrelScriptConfig()->close(memory);
#endif

#if defined(TIC_BUILD_WITH_LUA)
    getLuaScriptConfig()->close(memory);

#   if defined(TIC_BUILD_WITH_MOON)
    getMoonScriptConfig()->close(memory);
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    getFennelConfig()->close(memory);
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */


#if defined(TIC_BUILD_WITH_JS)
    getJsScriptConfig()->close(memory);
#endif

#if defined(TIC_BUILD_WITH_WREN)
    getWrenScriptConfig()->close(memory);
#endif

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
    resetDma(memory);
}

void tic_core_tick_end(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;
    tic80_input* input = &core->memory.ram.input;

    core->state.gamepads.previous.data = input->gamepads.data;
    core->state.keyboard.previous.data = input->keyboard.data;

    tic_core_sound_tick_end(memory);

    core->state.setpix = setPixelOvr;
    core->state.getpix = getPixelOvr;
    core->state.drawhline = drawHLineOvr;
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

void tic_core_blit_ex(tic_mem* tic, tic80_pixel_color_format fmt, tic_scanline scanline, tic_overline overline, void* data)
{
    // init OVR palette
    {
        tic_core* core = (tic_core*)tic;

        const tic_palette* ovr = &core->state.ovr.palette;
        bool ovrEmpty = true;
        for (s32 i = 0; i < sizeof(tic_palette); i++)
            if (ovr->data[i])
                ovrEmpty = false;

        memcpy(core->state.ovr.raw, tic_tool_palette_blit(ovrEmpty ? &tic->ram.vram.palette : ovr, fmt), sizeof core->state.ovr.raw);
    }

    if (scanline)
        scanline(tic, 0, data);

    const u32* pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);

    enum { Top = (TIC80_FULLHEIGHT - TIC80_HEIGHT) / 2, Bottom = Top };
    enum { Left = (TIC80_FULLWIDTH - TIC80_WIDTH) / 2, Right = Left };

    u32* out = tic->screen;

    memset4(&out[0 * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH * Top);

    u32* rowPtr = out + (Top * TIC80_FULLWIDTH);
    for (s32 r = 0; r < TIC80_HEIGHT; r++, rowPtr += TIC80_FULLWIDTH)
    {
        u32* colPtr = rowPtr + Left;
        memset4(rowPtr, pal[tic->ram.vram.vars.border], Left);

        s32 pos = (r + tic->ram.vram.vars.offset.y + TIC80_HEIGHT) % TIC80_HEIGHT * TIC80_WIDTH >> 1;

        u32 x = (-tic->ram.vram.vars.offset.x + TIC80_WIDTH) % TIC80_WIDTH;
        for (s32 c = 0; c < TIC80_WIDTH / 2; c++)
        {
            u8 val = ((u8*)tic->ram.vram.screen.data)[pos + c];
            *(colPtr + (x++ % TIC80_WIDTH)) = pal[val & 0xf];
            *(colPtr + (x++ % TIC80_WIDTH)) = pal[val >> 4];
        }

        memset4(rowPtr + (TIC80_FULLWIDTH - Right), pal[tic->ram.vram.vars.border], Right);

        if (scanline && (r < TIC80_HEIGHT - 1))
        {
            scanline(tic, r + 1, data);
            pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);
        }
    }

    memset4(&out[(TIC80_FULLHEIGHT - Bottom) * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH * Bottom);

    if (overline)
        overline(tic, data);

}

static inline void scanline(tic_mem* memory, s32 row, void* data)
{
    tic_core* core = (tic_core*)memory;

    if (core->state.initialized)
        core->state.scanline(memory, row, data);
}

static inline void overline(tic_mem* memory, void* data)
{
    tic_core* core = (tic_core*)memory;

    if (core->state.initialized)
        core->state.ovr.callback(memory, data);
}

void tic_core_blit(tic_mem* tic, tic80_pixel_color_format fmt)
{
    tic_core_blit_ex(tic, fmt, scanline, overline, NULL);
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
