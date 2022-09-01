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

#include <janet.h>

#if defined(TIC_BUILD_WITH_JANET)

static tic_core* CurrentMachine = NULL;
static inline tic_core* getJanetMachine(JanetTable* env)
{
    return CurrentMachine;
}

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


static Janet janet_print(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_cls(int32_t argc, Janet* argv)
{
    janet_arity(argc, 0, 1);

    int32_t color = 0;

    if (argc > 0) {
        color = janet_getinteger(argv, 0);
    }

    //tic_mem* memory = (tic_mem*)getJanetMachine(janet??)
    tic_api_cls(memory, color);
    return janet_wrap_nil();
}

static Janet janet_pix(int32_t argc, Janet* argv)
{
    janet_arity(argc, 2, 3);
    bool get;

    switch (argc) {
    case 2:
        get = true;
        break;
    case 3:
        get = false;
        break;
    default:
        ; // NOTREACHED
    }

    int32_t result = tic_api_pix(memory, x, y, color, get);
    return janet_wrap_integer(result);
}

static Janet janet_line(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 5);
    double x1 = janet_getnumber(argv, 0);
    double y1 = janet_getnumber(argv, 1);
    double x2 = janet_getnumber(argv, 2);
    double y2 = janet_getnumber(argv, 3);
    int32_t color = janet_getinteger(argv, 4);
    //tic_mem* memory = whatever;
    tic_api_line(memory, x1, y1, x2, y2, color);
    return janet_wrap_nil();
}

static Janet janet_rect(int32_t argc, Janet* argv)
{
    return janet_wrap_nil();
}

static Janet janet_rectb(int32_t argc, Janet* argv)
{
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
    //tic_mem* memory = XXX;
    int32_t tile_id = tic_api_mget(memory, x, y);
    return janet_wrap_integer(tile_id);
}

static Janet janet_mset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);
    int32_t x = janet_getinteger(argv, 0);
    int32_t y = janet_getinteger(argv, 1);
    int32_t value = janet_getinteger(argv, 2);
    //tic_mem* memory = XXX;
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
    return janet_wrap_nil();
}

static Janet janet_poke1(int32_t argc, Janet* argv)
{
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
    tic_api_memcpy(memory, dst, src, size);
    return janet_wrap_nil();
}

static Janet janet_memset(int32_t argc, Janet* argv)
{
    janet_fixarity(argc, 3);
    int32_t dst = janet_getinteger(argv, 0);
    int32_t val = janet_getinteger(argv, 1);
    int32_t size = janet_getinteger(argv, 2);
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
}

static bool initJanet(tic_mem* tic, const char* code)
{
    tic_core* machine = (tic_core*)tic;
    closeJanet(tic);
    CurrentMachine = machine;
    //machine->currentVM = malloc(sizeof(JanetTable));
    //JanetTable* currentVM = (JanetTable*)machine->currentVM;
}

static void evalJanet(tic_mem* tic, const char* code)
{
}

static void callJanetTick(tic_mem* tic)
{
}

static void callJanetBoot(tic_mem* tic)
{
}

static void callJanetIntCallback(tic_mem* memory, s32 value, void* data, const char* name)
{
}

static void callJanetScanline(tic_mem* memory, s32 row, void* data)
{
}

static void callJanetBorder(tic_mem* memory, s32 row, void* data)
{
}

static void callJanetMenu(tic_mem* memory, s32 index, void* data)
{
}

static const tic_outline_item* getJanetOutline(const char* code, s32* size)
{
}


/* ***************** */

const tic_script_config JanetSyntaxConfig = 
{
    .id                 = 99,
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
    .blockEnd           = "end",

    .keywords           = JanetKeywords,
    .keywordsCount      = COUNT_OF(JanetKeywords),


#endif /* defined(TIC_BUILD_WITH_JANET) */
