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
    "break",
    "def",
    "defn",
    "do",
    "fn",
    "if",
    "quasiquote",
    "quote",
    "set",
    "splice",
    "unquote",
    "upscope",
    "var",
    "while",
};

static tic_core* CurrentMachine = NULL;

static inline tic_core* getJanetMachine(void)
{
    return CurrentMachine;
}

/* ***************** */

static Janet janet_print(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, 7);
    int32_t x = 0;
    int32_t y = 0;
    int32_t color = 15;
    bool fixed = false;
    int32_t scale = 1;
    bool alt = false;

    const char *text = janet_getcstring(argv, 0);
    if (argc >= 2) x = janet_getinteger(argv, 1);
    if (argc >= 3) y = janet_getinteger(argv, 2);
    if (argc >= 4) color = janet_getinteger(argv, 3);
    if (argc >= 5) fixed = janet_getboolean(argv, 4);
    if (argc >= 6) scale = janet_getinteger(argv, 5);
    if (argc >= 7) alt = janet_getboolean(argv, 6);

    tic_mem* memory = (tic_mem*)getJanetMachine();
    int32_t width = tic_api_print(memory, text, x, y, color, fixed, scale, alt);
    return janet_wrap_integer(width);
}

static Janet janet_cls(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 1);

    int32_t color = 0;

    if (argc > 0) {
        color = janet_getinteger(argv, 0);
    }

    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_cls(memory, color);
    return janet_wrap_nil();
}

static Janet janet_pix(int32_t argc, Janet* argv)
{
    janet_arity(argc, 2, 3);
    bool get;
    int32_t color = 0;

    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);

    if (argc == 2) {
        get = true;
    } else {
        color = janet_getinteger(argv, 2);
        get = false;
    }

    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_pix(memory, x, y, color, get));
}

static Janet janet_line(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    double x1 = janet_getnumber(argv, 0);
    double y1 = janet_getnumber(argv, 1);
    double x2 = janet_getnumber(argv, 2);
    double y2 = janet_getnumber(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_line(memory, x1, y1, x2, y2, color);
    return janet_wrap_nil();
}

static Janet janet_rect(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t width = janet_getinteger(argv, 2);
    int32_t height = janet_getinteger(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_rect(memory, x, y, width, height, color);
    return janet_wrap_nil();
}

static Janet janet_rectb(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t width = janet_getinteger(argv, 2);
    int32_t height = janet_getinteger(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_rectb(memory, x, y, width, height, color);
    return janet_wrap_nil();
}

static Janet janet_spr(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_btn(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_btnp(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_sfx(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_map(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_mget(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_mget(memory, x, y));
}

static Janet janet_mset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t value = janet_getinteger(argv, 2);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_mset(memory, x, y, value);
    return janet_wrap_nil();
}

static Janet janet_peek(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_poke(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_peek1(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 1);
    int32_t address = janet_getinteger(argv, 0);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    return janet_wrap_integer(tic_api_peek1(memory, address));
}

static Janet janet_poke1(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 2);
    int32_t address = janet_getinteger(argv, 0);
    int32_t value = janet_getinteger(argv, 1);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_poke1(memory, address, value);
    return janet_wrap_nil();
}

static Janet janet_peek2(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_poke2(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_peek4(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_poke4(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_memcpy(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);
    int32_t dst = janet_getinteger(argv, 0);
    int32_t src = janet_getinteger(argv, 1);
    int32_t size = janet_getinteger(argv, 2);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_memcpy(memory, dst, src, size);
    return janet_wrap_nil();
}

static Janet janet_memset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);
    int32_t dst = janet_getinteger(argv, 0);
    int32_t val = janet_getinteger(argv, 1);
    int32_t size = janet_getinteger(argv, 2);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_memset(memory, dst, val, size);
    return janet_wrap_nil();
}

static Janet janet_trace(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_pmem(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_time(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_tstamp(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_exit(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_font(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_mouse(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_circ(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 4);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t radius = janet_getinteger(argv, 2);
    int32_t color = janet_getinteger(argv, 3);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_circ(memory, x, y, radius, color);
    return janet_wrap_nil();
}

static Janet janet_circb(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 4);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t radius = janet_getinteger(argv, 2);
    int32_t color = janet_getinteger(argv, 3);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_circb(memory, x, y, radius, color);
    return janet_wrap_nil();
}

static Janet janet_elli(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t a = janet_getinteger(argv, 2);
    int32_t b = janet_getinteger(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_elli(memory, x, y, a, b, color);
    return janet_wrap_nil();
}

static Janet janet_ellib(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t a = janet_getinteger(argv, 2);
    int32_t b = janet_getinteger(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_ellib(memory, x, y, a, b, color);
    return janet_wrap_nil();
}

static Janet janet_tri(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 7);
    double x1 = janet_getnumber(argv, 0);
    double y1 = janet_getnumber(argv, 1);
    double x2 = janet_getnumber(argv, 2);
    double y2 = janet_getnumber(argv, 3);
    double x3 = janet_getnumber(argv, 4);
    double y3 = janet_getnumber(argv, 5);
    int32_t color = janet_getnumber(argv, 6);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_tri(memory, x1, y1, x2, y2, x3, y3, color);
    return janet_wrap_nil();
}

static Janet janet_trib(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 7);
    double x1 = janet_getnumber(argv, 0);
    double y1 = janet_getnumber(argv, 1);
    double x2 = janet_getnumber(argv, 2);
    double y2 = janet_getnumber(argv, 3);
    double x3 = janet_getnumber(argv, 4);
    double y3 = janet_getnumber(argv, 5);
    int32_t color = janet_getnumber(argv, 6);
    tic_mem* memory = (tic_mem*)getJanetMachine();
    tic_api_trib(memory, x1, y1, x2, y2, x3, y3, color);
    return janet_wrap_nil();
}

static Janet janet_ttri(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_clip(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_music(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_sync(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_vbank(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_reset(int32_t argc, Janet* argv)
{
    tic_core* machine = getJanetMachine();
    machine->state.initialized = false;
    return janet_wrap_nil();
}

static Janet janet_key(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_keyp(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_fget(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_fset(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

/* ***************** */

static void closeJanet(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if (core->currentVM) {
        janet_deinit();
        core->currentVM = NULL;
        CurrentMachine = NULL;
    }
}

static bool initJanet(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeJanet(tic);
    janet_init();
    CurrentMachine = core;
    core->currentVM = (JanetTable*)janet_core_env(NULL);
    janet_cfuns(core->currentVM, "tic", janet_c_functions);
    janet_dostring(core->currentVM, code, __func__, NULL);
    return true; // XXX should return whether dostring() has issues or not
}

static void evalJanet(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    JanetTable* env = core->currentVM;

    if (!env) return;

    Janet result;
    janet_dostring(env, code, __func__, &result); // XXX handle error
}

/*
 * Find a function called TIC_FN and execute it. If we can't find it, then
 * it is a problem.
 */
static void callJanetTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    Janet pre_fn;
    if (janet_dostring(core->currentVM, TIC_FN, __func__, &pre_fn)) {
        core->data->error(core->data->data, "(TIC) isn't found :(");
        return;
    }
    JanetFunction *fn = janet_unwrap_function(pre_fn);

    Janet result;
    JanetSignal status = janet_pcall(fn, 0, NULL, &result, NULL);

    if (status != JANET_SIGNAL_OK) {
        core->data->error(core->data->data, "XXX"); // XXX get error text
    }
}

/*
 * Find a function called BOOT_FN and execute it. If we can't find it, then
 * it's not a problem.
 */
static void callJanetBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    Janet pre_fn;
    if (janet_dostring(core->currentVM, BOOT_FN, __func__, &pre_fn)) {
        return;
    }
    JanetFunction *fn = janet_unwrap_function(pre_fn);

    Janet result;
    JanetSignal status = janet_pcall(fn, 0, NULL, &result, NULL);

    if (status != JANET_SIGNAL_OK) {
        core->data->error(core->data->data, "XXX"); // XXX get error text
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
    if (janet_dostring(core->currentVM, name, __func__, &pre_fn)) {
        return;
    }
    JanetFunction *fn = janet_unwrap_function(pre_fn);

    Janet result;
    Janet argv[] = {
        janet_wrap_integer(value),
    };
    JanetSignal status = janet_pcall(fn, 1, argv, &result, NULL);

    if (status != JANET_SIGNAL_OK) {
        core->data->error(core->data->data, "XXX"); // XXX get error text
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
    static tic_outline_item* items = NULL;
    if (items) {
        free(items);
        items = NULL;
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
