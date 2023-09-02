/*
 * vim: ts=4 sts=4 sw=4 et
 */

// MIT License

// Copyright (c) 2022 Charlotte Koch @dressupgeekout <dressupgeekout@gmail.com>

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

#include "core/core.h"

#if defined(TIC_BUILD_WITH_JANET)

#include <janet.h>

static inline tic_core* getJanetMachine(void);

static Janet janet_print(int32_t argc, Janet* argv);
static Janet janet_cls(int32_t argc, Janet* argv);
static Janet janet_pix(int32_t argc, Janet* argv);
static Janet janet_line(int32_t argc, Janet* argv);
static Janet janet_rect(int32_t argc, Janet* argv);
static Janet janet_rectb(int32_t argc, Janet* argv);
static Janet janet_spr(int32_t argc, Janet* argv);
static Janet janet_btn(int32_t argc, Janet* argv);
static Janet janet_btnp(int32_t argc, Janet* argv);
static Janet janet_sfx(int32_t argc, Janet* argv);
static Janet janet_map(int32_t argc, Janet* argv);
static Janet janet_mget(int32_t argc, Janet* argv);
static Janet janet_mset(int32_t argc, Janet* argv);
static Janet janet_peek(int32_t argc, Janet* argv);
static Janet janet_poke(int32_t argc, Janet* argv);
static Janet janet_peek1(int32_t argc, Janet* argv);
static Janet janet_poke1(int32_t argc, Janet* argv);
static Janet janet_peek2(int32_t argc, Janet* argv);
static Janet janet_poke2(int32_t argc, Janet* argv);
static Janet janet_peek4(int32_t argc, Janet* argv);
static Janet janet_poke4(int32_t argc, Janet* argv);
static Janet janet_memcpy(int32_t argc, Janet* argv);
static Janet janet_memset(int32_t argc, Janet* argv);
static Janet janet_trace(int32_t argc, Janet* argv);
static Janet janet_pmem(int32_t argc, Janet* argv);
static Janet janet_time(int32_t argc, Janet* argv);
static Janet janet_tstamp(int32_t argc, Janet* argv);
static Janet janet_exit(int32_t argc, Janet* argv);
static Janet janet_font(int32_t argc, Janet* argv);
static Janet janet_mouse(int32_t argc, Janet* argv);
static Janet janet_circ(int32_t argc, Janet* argv);
static Janet janet_circb(int32_t argc, Janet* argv);
static Janet janet_elli(int32_t argc, Janet* argv);
static Janet janet_ellib(int32_t argc, Janet* argv);
static Janet janet_tri(int32_t argc, Janet* argv);
static Janet janet_trib(int32_t argc, Janet* argv);
static Janet janet_ttri(int32_t argc, Janet* argv);
static Janet janet_clip(int32_t argc, Janet* argv);
static Janet janet_music(int32_t argc, Janet* argv);
static Janet janet_sync(int32_t argc, Janet* argv);
static Janet janet_vbank(int32_t argc, Janet* argv);
static Janet janet_reset(int32_t argc, Janet* argv);
static Janet janet_key(int32_t argc, Janet* argv);
static Janet janet_keyp(int32_t argc, Janet* argv);
static Janet janet_fget(int32_t argc, Janet* argv);
static Janet janet_fset(int32_t argc, Janet* argv);

static void closeJanet(tic_mem* tic);
static bool initJanet(tic_mem* tic, const char* code);
static void evalJanet(tic_mem* tic, const char* code);
static void callJanetTick(tic_mem* tic);
static void callJanetBoot(tic_mem* tic);
static void callJanetIntCallback(tic_mem* memory, s32 value, void* data, const char* name);
static void callJanetScanline(tic_mem* memory, s32 row, void* data);
static void callJanetBorder(tic_mem* memory, s32 row, void* data);
static void callJanetMenu(tic_mem* memory, s32 index, void* data);
static const tic_outline_item* getJanetOutline(const char* code, s32* size);

/* ***************** */

static const JanetReg janet_c_functions[] =
{
    {"print", janet_print, NULL},
    {"cls", janet_cls, NULL},
    {"pix", janet_pix, NULL},
    {"line", janet_line, NULL},
    {"rect", janet_rect, NULL},
    {"rectb", janet_rectb, NULL},
    {"spr", janet_spr, NULL},
    {"btn", janet_btn, NULL},
    {"btnp", janet_btnp, NULL},
    {"sfx", janet_sfx, NULL},
    {"map", janet_map, NULL},
    {"mget", janet_mget, NULL},
    {"mset", janet_mset, NULL},
    {"peek", janet_peek, NULL},
    {"poke", janet_poke, NULL},
    {"peek1", janet_peek1, NULL},
    {"poke1", janet_poke1, NULL},
    {"peek2", janet_peek2, NULL},
    {"poke2", janet_poke2, NULL},
    {"peek4", janet_peek4, NULL},
    {"poke4", janet_poke4, NULL},
    {"memcpy", janet_memcpy, NULL},
    {"memset", janet_memset, NULL},
    {"trace", janet_trace, NULL},
    {"pmem", janet_pmem, NULL},
    {"time", janet_time, NULL},
    {"tstamp", janet_tstamp, NULL},
    {"exit", janet_exit, NULL},
    {"font", janet_font, NULL},
    {"mouse", janet_mouse, NULL},
    {"circ", janet_circ, NULL},
    {"circb", janet_circb, NULL},
    {"elli", janet_elli, NULL},
    {"ellib", janet_ellib, NULL},
    {"tri", janet_tri, NULL},
    {"trib", janet_trib, NULL},
    {"ttri", janet_ttri, NULL},
    {"clip", janet_clip, NULL},
    {"music", janet_music, NULL},
    {"sync", janet_sync, NULL},
    {"vbank", janet_vbank, NULL},
    {"reset", janet_reset, NULL},
    {"key", janet_key, NULL},
    {"keyp", janet_keyp, NULL},
    {"fget", janet_fget, NULL},
    {"fset", janet_fset, NULL},
    {NULL, NULL, NULL}
};

static const char* const JanetKeywords[] =
{
    "defmacro", "with-syms", "macex1", "macex",
    "do", "values", "break"
    "if", "when", "cond", "match",
    "each", "for", "loop", "while",
    "fn", "defn", "partial",
    "def", "var", "set", "defglobal", "let", "set", "put",
    "if-let", "when-let", "if-not",
    "or", "and", "true", "false", "nil", "not", "not=", "length",
    "brshift", "blshift", "bor", "band", "bnot", "bxor",
    "#", ":", "->", "->>", "-?>", "-?>>", "$",
    "quasiquote", "quote", "unquote", "upscope",
    "splice", ";"
};

static JanetFiber* GameFiber = NULL;
static JanetBuffer *errBuffer;
static tic_core* CurrentMachine = NULL;


static inline tic_core* getJanetMachine(void)
{
    return CurrentMachine;
}

/* ***************** */

typedef struct
{
    s32 note;
    s32 octave;
} SFXNote;

static SFXNote tic_optsfxnote(Janet *argv, int32_t argc, int32_t n, SFXNote sfxNote)
{
    if (argc <= n)
    {
        return sfxNote;
    }
    else if (janet_checktype(argv[n], JANET_STRING))
    {
        const char *noteStr = janet_getcstring(argv, n);

        if (!tic_tool_parse_note(noteStr, &sfxNote.note, &sfxNote.octave))
        {
            janet_panicf("invalid note, should be like C#4, got %s\n", noteStr);
        }

        return sfxNote;
    }
    else
    {
        s32 id = janet_getinteger(argv, n);
        sfxNote.note = id % NOTES;
        sfxNote.octave = id / NOTES;
        return sfxNote;
    }
}

typedef struct
{
    u8 colors[TIC_PALETTE_SIZE];
    int32_t count;
} ColorKey;

static ColorKey tic_optcolorkey(Janet *argv, int32_t argc, int32_t n)
{
    ColorKey colorkey = {0};
    colorkey.count = 0;

    if (argc > n)
    {
        if (janet_checktypes(argv[n], JANET_TFLAG_INDEXED))
        {
            JanetView keys = janet_getindexed(argv, n);
            s32 list_count = keys.len;

            for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
            {
                if (i < list_count)
                {
                    colorkey.colors[i] = (s32)janet_getinteger(keys.items, i);;
                    colorkey.count++;
                }
                else
                {
                    break;
                }
            }
        }
        else if (janet_checkint(argv[n]))
        {
            colorkey.colors[0] = (s32)janet_getnumber(argv, n);
            colorkey.count = 1;
        }
        else
        {
            janet_panic("Error: colorkeys must be either int or list of int");
        }
    }

    return colorkey;
}

typedef struct
{
    float z[3];
    bool on;
} TriDepth;

static TriDepth tic_opttridepth(Janet* argv, int32_t argc, int32_t n)
{
    TriDepth depth = {0};
    depth.on = false;

    if (argc > n)
    {
        depth.on = true;

        depth.z[0] = janet_getnumber(argv, n);
        depth.z[1] = janet_getnumber(argv, n+1);
        depth.z[2] = janet_getnumber(argv, n+2);
    }

    return depth;
}

/* ***************** */

static Janet janet_print(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 7);

    s32 x = 0;
    s32 y = 0;
    u8 color = 15;
    bool fixed = false;
    s32 scale = 1;
    bool alt = false;

    const char *text = janet_getcstring(argv, 0);
    if (argc >= 2) x = (s32)janet_getinteger(argv, 1);
    if (argc >= 3) y = (s32)janet_getinteger(argv, 2);
    if (argc >= 4) color = (u8)janet_getinteger(argv, 3);
    if (argc >= 5) fixed = janet_getboolean(argv, 4);
    if (argc >= 6) scale = (s32)janet_getinteger(argv, 5);
    if (argc >= 7) alt = janet_getboolean(argv, 6);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    int32_t width = tic_api_print(memory, text, x, y, color, fixed, scale, alt);
    return janet_wrap_integer(width);
}

static Janet janet_cls(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 1);
    u32 color = (u32)janet_optinteger(argv, argc, 0, 0);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_cls(memory, color);
    return janet_wrap_nil();
}

static Janet janet_pix(int32_t argc, Janet* argv)
{
    janet_arity(argc, 2, 3);

    bool get;
    u8 color = 0;

    s32 x = (s32)janet_getinteger(argv, 0);
    s32 y = (s32)janet_getinteger(argv, 1);

    if (argc == 2) {
        get = true;
    } else {
        color = (u8)janet_getinteger(argv, 2);
        get = false;
    }

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_pix(memory, x, y, color, get));
}

static Janet janet_line(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);

    float x1 = (float)janet_getnumber(argv, 0);
    float y1 = (float)janet_getnumber(argv, 1);
    float x2 = (float)janet_getnumber(argv, 2);
    float y2 = (float)janet_getnumber(argv, 3);
    u8 color = (u8)janet_getinteger(argv, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_line(memory, x1, y1, x2, y2, color);
    return janet_wrap_nil();
}

static Janet janet_rect(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);

    s32 x = (s32)janet_getinteger(argv, 0);
    s32 y = (s32)janet_getinteger(argv, 1);
    s32 width = (s32)janet_getinteger(argv, 2);
    s32 height = (s32)janet_getinteger(argv, 3);
    u8 color = (u8)janet_getinteger(argv, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_rect(memory, x, y, width, height, color);
    return janet_wrap_nil();
}

static Janet janet_rectb(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);

    s32 x = (s32)janet_getinteger(argv, 0);
    s32 y = (s32)janet_getinteger(argv, 1);
    s32 width = (s32)janet_getinteger(argv, 2);
    s32 height = (s32)janet_getinteger(argv, 3);
    u8 color = (u8)janet_getinteger(argv, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_rectb(memory, x, y, width, height, color);
    return janet_wrap_nil();
}

static Janet janet_spr(int32_t argc, Janet* argv)
{
    janet_arity(argc, 3, 9);

    s32 index = (s32)janet_getinteger(argv, 0);
    s32 x = (s32)janet_getinteger(argv, 1);
    s32 y = (s32)janet_getinteger(argv, 2);

    ColorKey colorkey = tic_optcolorkey(argv, argc, 3);

    s32 scale = (s32)janet_optnumber(argv, argc, 4, 1);
    tic_flip flip = (s32)janet_optnumber(argv, argc, 5, tic_no_flip);
    tic_rotate rotate = (s32)janet_optnumber(argv, argc, 6, tic_no_rotate);
    s32 w = (s32)janet_optnumber(argv, argc, 7, 1);
    s32 h = (s32)janet_optnumber(argv, argc, 8, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_spr(memory, index, x, y, w, h,
                colorkey.colors, colorkey.count,
                scale, flip, rotate);

    return janet_wrap_nil();
}

static Janet janet_btn(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);

    s32 id = (s32)janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_boolean(tic_api_btn(memory, id));
}

static Janet janet_btnp(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 3);

    s32 id = (s32)janet_getinteger(argv, 0);
    s32 hold = (s32)janet_optinteger(argv, argc, 1, -1);
    s32 period = (s32)janet_optinteger(argv, argc, 2, -1);

    tic_mem* memory = (tic_mem*)getJanetMachine();

    return janet_wrap_boolean(tic_api_btnp(memory, id, hold, period));
}

static Janet janet_sfx(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 6);
    tic_mem* memory = (tic_mem*)getJanetMachine();

    s32 index = (s32)janet_getinteger(argv, 0);
    if (index >= SFX_COUNT) {
        janet_panicf("unknown sfx index, got %s\n", index);
    }

    // possibly get default values from sfx
    tic_sample* effect = memory->ram->sfx.samples.data + index;
    SFXNote defaultSfxNote = {-1, -1};
    s32 defaultSpeed = SFX_DEF_SPEED;
    if (index >= 0)
    {
        defaultSfxNote.note = effect->note;
        defaultSfxNote.octave = effect->octave;
        defaultSpeed = effect->speed;
    }

    SFXNote sfxNote = tic_optsfxnote(argv, argc, 1, defaultSfxNote);
    s32 duration = (s32)janet_optinteger(argv, argc, 2, -1);
    s32 channel = (s32)janet_optinteger(argv, argc, 3, 0);
    if (channel < 0 || channel > TIC_SOUND_CHANNELS) {
        janet_panicf("unknown channel, got %s\n", channel);
    }

    s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
    volumes[0] = (s32)janet_optinteger(argv, argc, 4, MAX_VOLUME);
    volumes[1] = volumes[0];

    s32 speed = (s32)janet_optinteger(argv, argc, 5, defaultSpeed);

    tic_api_sfx(memory, index,
                sfxNote.note, sfxNote.octave,
                duration, channel,
                volumes[0] & 0xf, volumes[1] & 0xf,
                speed);
    return janet_wrap_nil();
}

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    JanetFunction* remap_fn = (JanetFunction*)data;

    Janet *argv = janet_tuple_begin(3);
    argv[0] = janet_wrap_integer(result->index);
    argv[1] = janet_wrap_integer(x);
    argv[2] = janet_wrap_integer(y);
    janet_tuple_end(argv);

    Janet jresult = janet_call(remap_fn, 3, argv);

    if (janet_checktypes(jresult, JANET_TFLAG_INDEXED))
    {
        const Janet *jresult_tuple = janet_unwrap_tuple(jresult);
        u8 retc = janet_tuple_length(jresult_tuple);
        result->index = janet_getinteger(jresult_tuple, 0);
        result->flip = janet_optinteger(jresult_tuple, retc, 1,  0);
        result->rotate = janet_optinteger(jresult_tuple, retc, 2,  0);
    }
    else if (janet_checkint(jresult))
    {
        result->index = janet_unwrap_integer(jresult);
    }
}

static Janet janet_map(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 9);

    s32 x = (s32)janet_optinteger(argv, argc, 0, 0);
    s32 y = (s32)janet_optinteger(argv, argc, 1, 0);

    s32 w = (s32)janet_optinteger(argv, argc, 2, 30);
    s32 h = (s32)janet_optinteger(argv, argc, 3, 17);

    s32 sx = (s32)janet_optinteger(argv, argc, 4, 0);
    s32 sy = (s32)janet_optinteger(argv, argc, 5, 0);

    ColorKey colorkey = tic_optcolorkey(argv, argc, 6);

    s32 scale = (s32)janet_optnumber(argv, argc, 7, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();

    if (argc < 9)
    {
        tic_api_map(memory, x, y, w, h, sx, sy,
                    colorkey.colors, colorkey.count,
                    scale, NULL, NULL);
    }
    else
    {
        JanetFunction *remap = janet_getfunction(argv, 8);
        tic_api_map(memory, x, y, w, h, sx, sy,
                    colorkey.colors, colorkey.count,
                    scale,
                    remapCallback, remap);
    }

    return janet_wrap_nil();
}

static Janet janet_mget(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);

    s32 x = (s32)janet_getinteger(argv, 0);
    s32 y = (s32)janet_getinteger(argv, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_mget(memory, x, y));
}

static Janet janet_mset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);

    s32 x = (s32)janet_getinteger(argv, 0);
    s32 y = (s32)janet_getinteger(argv, 1);
    u8 value = (u8)janet_getinteger(argv, 2);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_mset(memory, x, y, value);
    return janet_wrap_nil();
}

static Janet janet_peek(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 2);
    s32 address = (s32)janet_getinteger(argv, 0);
    s32 bits = (s32)janet_optinteger(argv, argc, 1, BITS_IN_BYTE);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_peek(memory, address, bits));
}

static Janet janet_poke(int32_t argc, Janet* argv)
{
    janet_arity(argc, 2, 3);
    s32 address = (s32)janet_getinteger(argv, 0);
    u8 value = (s32)janet_getinteger(argv, 1);
    s32 bits = (s32)janet_optinteger(argv, argc, 2, BITS_IN_BYTE);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_poke(memory, address, value, bits);

    return janet_wrap_nil();
}

static Janet janet_peek1(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);

    u8 address = (u8)janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_peek1(memory, address));
}

static Janet janet_poke1(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);

    s32 address = (s32)janet_getinteger(argv, 0);
    u8 value = (u8)janet_getinteger(argv, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_poke1(memory, address, value);
    return janet_wrap_nil();
}

static Janet janet_peek2(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);

    s32 address = janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_peek2(memory, address));
}

static Janet janet_poke2(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);

    s32 address = janet_getinteger(argv, 0);
    u8 value = janet_getinteger(argv, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_poke2(memory, address, value);
    return janet_wrap_nil();
}

static Janet janet_peek4(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);

    s32 address = janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_peek4(memory, address));
}

static Janet janet_poke4(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);

    s32 address = janet_getinteger(argv, 0);
    u8 value = janet_getinteger(argv, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_poke4(memory, address, value);
    return janet_wrap_nil();
}

static Janet janet_memcpy(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);

    s32 dst = janet_getinteger(argv, 0);
    s32 src = janet_getinteger(argv, 1);
    s32 size = janet_getinteger(argv, 2);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_memcpy(memory, dst, src, size);
    return janet_wrap_nil();
}

static Janet janet_memset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);

    s32 dst = janet_getinteger(argv, 0);
    u8 val = janet_getinteger(argv, 1);
    s32 size = janet_getinteger(argv, 2);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_memset(memory, dst, val, size);
    return janet_wrap_nil();
}

static Janet janet_trace(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 2);
    u8 color = 15;

    const char *message = janet_getcstring(argv, 0);

    if (argc > 1) {
        color = janet_getinteger(argv, 1);
    }

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_trace(memory, message, color);
    return janet_wrap_nil();
}

static Janet janet_pmem(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 2);
    s32 index = janet_getinteger(argv, 0);

    if(index < TIC_PERSISTENT_SIZE)
    {
        tic_mem* memory = (tic_mem*)getJanetMachine();
        u32 val = tic_api_pmem(memory, index, 0, false);

        if (argc >= 2)
        {
            u32 value = janet_getinteger(argv, 1);
            tic_api_pmem(memory, index, value, true);
        }

        return janet_wrap_integer(val);
    }
    else
    {
        janet_panic("Error: invalid persistent tic index");
    }
}

static Janet janet_time(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 0);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_number(tic_api_time(memory));
}

static Janet janet_tstamp(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 0);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_tstamp(memory));
}

static Janet janet_exit(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 0);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_exit(memory);
    return janet_wrap_nil();
}

static Janet janet_font(int32_t argc, Janet* argv)
{
    janet_arity(argc, 3, 9);

    const char* text = janet_getcstring(argv, 0);
    s32 x = (s32)janet_getinteger(argv, 1);
    s32 y = (s32)janet_getinteger(argv, 2);

    u8 chromakey = (u8)janet_optinteger(argv, argc, 3, 0);
    s32 w= (s32)janet_optinteger(argv, argc, 4, 0);
    s32 h = (s32)janet_optinteger(argv, argc, 5, 0);
    bool fixed = janet_optboolean(argv, argc, 6, false);
    s32 scale = (s32)janet_optinteger(argv, argc, 7, 1);
    bool alt = janet_optboolean(argv, argc, 8, false);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    int32_t width = tic_api_font(memory,
                                 text, x, y, &chromakey, 1,
                                 w, h, fixed, scale, alt);
    return janet_wrap_integer(width);
}

static Janet janet_mouse(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    Janet result[7];

    {
        tic_point point = tic_api_mouse(memory);
        result[0] = janet_wrap_integer(point.x);
        result[1] = janet_wrap_integer(point.y);
    }

    tic_core* core = getJanetMachine();
    const tic80_mouse* mouse = &core->memory.ram->input.mouse;
    result[2] = janet_wrap_boolean(mouse->left);
    result[3] = janet_wrap_boolean(mouse->middle);
    result[4] = janet_wrap_boolean(mouse->right);
    result[5] = janet_wrap_number(mouse->scrollx);
    result[6] = janet_wrap_number(mouse->scrolly);

    return janet_wrap_tuple(janet_tuple_n(result, 7));
}

static Janet janet_circ(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 4);

    s32 x = janet_getinteger(argv, 0);
    s32 y = janet_getinteger(argv, 1);
    s32 radius = janet_getinteger(argv, 2);
    u8 color = janet_getinteger(argv, 3);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_circ(memory, x, y, radius, color);
    return janet_wrap_nil();
}

static Janet janet_circb(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 4);

    s32 x = janet_getinteger(argv, 0);
    s32 y = janet_getinteger(argv, 1);
    s32 radius = janet_getinteger(argv, 2);
    u8 color = janet_getinteger(argv, 3);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_circb(memory, x, y, radius, color);
    return janet_wrap_nil();
}

static Janet janet_elli(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);

    s32 x = janet_getinteger(argv, 0);
    s32 y = janet_getinteger(argv, 1);
    s32 a = janet_getinteger(argv, 2);
    s32 b = janet_getinteger(argv, 3);
    u8 color = janet_getinteger(argv, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_elli(memory, x, y, a, b, color);
    return janet_wrap_nil();
}

static Janet janet_ellib(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);

    s32 x = janet_getinteger(argv, 0);
    s32 y = janet_getinteger(argv, 1);
    s32 a = janet_getinteger(argv, 2);
    s32 b = janet_getinteger(argv, 3);
    u8 color = janet_getinteger(argv, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_ellib(memory, x, y, a, b, color);
    return janet_wrap_nil();
}

static Janet janet_tri(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 7);

    float x1 = janet_getnumber(argv, 0);
    float y1 = janet_getnumber(argv, 1);
    float x2 = janet_getnumber(argv, 2);
    float y2 = janet_getnumber(argv, 3);
    float x3 = janet_getnumber(argv, 4);
    float y3 = janet_getnumber(argv, 5);
    u8 color = janet_getnumber(argv, 6);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_tri(memory, x1, y1, x2, y2, x3, y3, color);
    return janet_wrap_nil();
}

static Janet janet_trib(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 7);

    float x1 = janet_getnumber(argv, 0);
    float y1 = janet_getnumber(argv, 1);
    float x2 = janet_getnumber(argv, 2);
    float y2 = janet_getnumber(argv, 3);
    float x3 = janet_getnumber(argv, 4);
    float y3 = janet_getnumber(argv, 5);
    u8 color = janet_getnumber(argv, 6);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_trib(memory, x1, y1, x2, y2, x3, y3, color);
    return janet_wrap_nil();
}

static Janet janet_ttri(int32_t argc, Janet* argv)
{
    janet_arity(argc, 12, 17);

    float x1 = janet_getnumber(argv, 0);
    float y1 = janet_getnumber(argv, 1);
    float x2 = janet_getnumber(argv, 2);
    float y2 = janet_getnumber(argv, 3);
    float x3 = janet_getnumber(argv, 4);
    float y3 = janet_getnumber(argv, 5);

    float u1 = janet_getnumber(argv, 6);
    float v1 = janet_getnumber(argv, 7);
    float u2 = janet_getnumber(argv, 8);
    float v2 = janet_getnumber(argv, 9);
    float u3 = janet_getnumber(argv, 10);
    float v3 = janet_getnumber(argv, 11);

    tic_texture_src src = tic_tiles_texture;
    if (janet_optboolean(argv, argc, 12, false)) {
        src = tic_map_texture;
    }

    ColorKey trans = tic_optcolorkey(argv, argc, 13);
    TriDepth depth = tic_opttridepth(argv, argc, 14);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_ttri(memory,
                 x1, y1,
                 x2, y2,
                 x3, y3,
                 u1, v1,
                 u2, v2,
                 u3, v3,
                 src,
                 trans.colors, trans.count,
                 depth.z[0], depth.z[1], depth.z[2], depth.on);

    return janet_wrap_nil();
}

static Janet janet_clip(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 4);

    tic_mem* memory = (tic_mem*)getJanetMachine();

    if (argc == 0) {
        tic_api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    } else if (argc == 4) {
        s32 x = janet_getinteger(argv, 0);
        s32 y = janet_getinteger(argv, 1);
        s32 w = janet_getinteger(argv, 2);
        s32 h = janet_getinteger(argv, 3);
        tic_api_clip(memory, x, y, w, h);
    } else {
        janet_panic("Error: must provide exactly 0 or 4 args.");
    }

    return janet_wrap_nil();
}

static Janet janet_music(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 7);

    s32 track = janet_optinteger(argv, argc, 0, -1);
    s32 frame = janet_optinteger(argv, argc, 1, -1);
    s32 row = janet_optinteger(argv, argc, 2, -1);
    bool loop = janet_optboolean(argv, argc, 3, true);
    bool sustain = janet_optboolean(argv, argc, 4, true);
    s32 tempo = janet_optinteger(argv, argc, 5, -1);
    s32 speed = janet_optinteger(argv, argc, 6, -1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_music(memory, track, frame, row, loop, sustain, tempo, speed);

    return janet_wrap_nil();
}

static Janet janet_sync(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 3);

    u32 mask = janet_optinteger(argv, argc, 0, 0);
    s32 bank = janet_optinteger(argv, argc, 1, 0);
    bool toCart = janet_optboolean(argv, argc, 2, false);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_sync(memory, mask, bank, toCart);
    return janet_wrap_nil();
}

static Janet janet_vbank(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);

    s32 bank = janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_vbank(memory, bank));
}

static Janet janet_reset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 0);
    tic_core* machine = getJanetMachine();
    machine->state.initialized = false;
    return janet_wrap_nil();
}

static Janet janet_key(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 1);
    tic_key key = -1;

    if (argc >= 1) key = (tic_key)janet_getinteger(argv, 0);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_boolean(tic_api_key(memory, key));
}

static Janet janet_keyp(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 3);
    tic_key key = -1;
    s32 hold = -1;
    s32 period = -1;

    if (argc >= 1) key = (tic_key)janet_getinteger(argv, 0);
    if (argc >= 2) hold = janet_getinteger(argv, 1);
    if (argc >= 3) period = janet_getinteger(argv, 2);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_boolean(tic_api_keyp(memory, key, hold, period));
}

static Janet janet_fget(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);

    s32 index = janet_getinteger(argv, 0);
    u8 flag = janet_getinteger(argv, 1);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_boolean(tic_api_fget(memory, index, flag));
}


static Janet janet_fset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);

    s32 index = janet_getinteger(argv, 0);
    u8 flag = janet_getinteger(argv, 1);
    bool value = janet_getboolean(argv, 2);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_fset(memory, index, flag, value);
    return janet_wrap_nil();
}

/* ***************** */
static void reportError(tic_core* core, Janet result)
{
    janet_stacktrace(GameFiber, result);
    janet_buffer_push_u8(errBuffer, 0);
    core->data->error(core->data->data, errBuffer->data);
}


static void closeJanet(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if (core->currentVM) {
        janet_deinit();
        core->currentVM = NULL;
        CurrentMachine = NULL;
        errBuffer = NULL;
        GameFiber = NULL;
    }
}

static bool initJanet(tic_mem* tic, const char* code)
{
    closeJanet(tic);
    janet_init();
    janet_sandbox(JANET_SANDBOX_ALL);

    JanetTable *env  = janet_core_env(NULL);
    JanetTable *sub_env = janet_table(0);
    janet_cfuns(sub_env, "tic80", janet_c_functions);

    // Provide the tic80 api as a module
    Janet module_cache = janet_resolve_core("module/cache");
    janet_table_put(janet_unwrap_table(module_cache), janet_cstringv("tic80"), janet_wrap_table(sub_env));

    tic_core* core = (tic_core*)tic;
    CurrentMachine = core;
    core->currentVM = (JanetTable*)janet_core_env(NULL);

    // override the dynamic err to a buffer, so that we can get errors later
    errBuffer = janet_buffer(1028);
    janet_setdyn("err", janet_wrap_buffer(errBuffer));

    GameFiber = janet_current_fiber();
    Janet result;

    // Load the game source code
    if (janet_dostring(core->currentVM, code, "main", &result)) {
        reportError(core, result);
        return false;
    }

    return true;
}


static void evalJanet(tic_mem* tic, const char* code)
{
  tic_core* core = (tic_core*)tic;

  Janet result = janet_wrap_nil();
  if (janet_dostring(core->currentVM, code, "main", &result)) {
      reportError(core, result);
  }
}


/*
 * Find a function called TIC_FN and execute it. If we can't find it, then
 * it is a problem.
 */
static void callJanetTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    // Load the TIC function
    Janet pre_fn;
    (void)janet_resolve(core->currentVM, janet_csymbol(TIC_FN), &pre_fn);

    if (janet_type(pre_fn) != JANET_FUNCTION) {
        core->data->error(core->data->data, "(TIC) isn't found :(");
        return;
    }

    JanetFunction *tic_fn = janet_unwrap_function(pre_fn);
    Janet result = janet_wrap_nil();
    JanetSignal status = janet_pcall(tic_fn, 0, NULL, &result, &GameFiber);

    if (status != JANET_SIGNAL_OK) {
        reportError(core, result);
    }

#if defined(BUILD_DEPRECATED)
    // call OVR() callback for backward compatibility
    (void)janet_resolve(core->currentVM, janet_csymbol(OVR_FN), &pre_fn);
    if (janet_type(pre_fn) == JANET_FUNCTION) {
        JanetFunction *ovr_fn = janet_unwrap_function(pre_fn);
        JanetSignal status = janet_pcall(ovr_fn, 0, NULL, &result, &GameFiber);

        if (status != JANET_SIGNAL_OK) {
            reportError(core, result);
        }
    }
#endif
}

/*
 * Find a function called BOOT_FN and execute it. If we can't find it, then
 * it's not a problem.
 */
static void callJanetBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    Janet pre_fn;
    (void)janet_resolve(core->currentVM, janet_csymbol(BOOT_FN), &pre_fn);

    if (janet_type(pre_fn) != JANET_FUNCTION) {
        return;
    }

    Janet result = janet_wrap_nil();
    JanetFunction *boot_fn = janet_unwrap_function(pre_fn);
    JanetSignal status = janet_pcall(boot_fn, 0, NULL, &result, &GameFiber);

    if (status != JANET_SIGNAL_OK) {
        reportError(core, result);
    }
}

/*
 * Find a function with the given name and execute it with the given value.
 * If we can't find it, then it's not a problem.
 */
static void callJanetIntCallback(tic_mem* tic, s32 value, void* data, const char* name)
{
    tic_core* core = (tic_core*)tic;

    Janet pre_fn;
    (void)janet_resolve(core->currentVM, janet_csymbol(name), &pre_fn);

    if (janet_type(pre_fn) != JANET_FUNCTION) {
        return;
    }

    Janet result = janet_wrap_nil();
    Janet argv[] = { janet_wrap_integer(value), };
    JanetFunction *fn = janet_unwrap_function(pre_fn);
    JanetSignal status = janet_pcall(fn, 1, argv, &result, &GameFiber);

    if (status != JANET_SIGNAL_OK) {
        reportError(core, result);
    }
}

static void callJanetScanline(tic_mem* tic, s32 row, void* data)
{
    callJanetIntCallback(tic, row, data, SCN_FN);
    callJanetIntCallback(tic, row, data, "scanline");
}

static void callJanetBorder(tic_mem* tic, s32 row, void* data)
{
    callJanetIntCallback(tic, row, data, BDR_FN);
}

static void callJanetMenu(tic_mem* tic, s32 index, void* data)
{
    callJanetIntCallback(tic, index, data, MENU_FN);
}

static const tic_outline_item* getJanetOutline(const char* code, s32* size)
{
    enum{Size = sizeof(tic_outline_item)};

    *size = 0;

    static tic_outline_item* items = NULL;

    if(items)
    {
        free(items);
        items = NULL;
    }

    const char* ptr = code;

    while(true)
    {
        static const char FuncString[] = "(defn ";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            ptr += sizeof FuncString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(c == ' ' || c == '\t' || c == '\n' || c == '[')
                {
                    end = ptr;
                    break;
                }

                ptr++;
            }

            if(end > start)
            {
                items = realloc(items, (*size + 1) * Size);

                items[*size].pos = start;
                items[*size].size = (s32)(end - start);

                (*size)++;
            }
        }
        else break;
    }

    return items;
}

/* ***************** */

const tic_script_config JanetSyntaxConfig =
{
    .id                 = 18,
    .name               = "janet",
    .fileExtension      = ".janet",
    .projectComment     = "#",
    .init               = initJanet,
    .close              = closeJanet,
    .tick               = callJanetTick,
    .boot               = callJanetBoot,

    .callback           =
    {
        .scanline       = callJanetScanline,
        .border         = callJanetBorder,
        .menu           = callJanetMenu,
    },

    .getOutline         = getJanetOutline,
    .eval               = evalJanet,

    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .singleComment      = "#",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .blockEnd           = NULL,

    .keywords           = JanetKeywords,
    .keywordsCount      = COUNT_OF(JanetKeywords),
};

#endif /* defined(TIC_BUILD_WITH_JANET) */
