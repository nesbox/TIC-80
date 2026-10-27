// MIT License

// Copyright (c) 2026 TIC-80 contributors

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

// Forth through Lugforth, sandboxed.
//
// pForth (src/api/forth.c) runs a cartridge as native code: its @ and ! take
// the player's own addresses. Here the Forth system is a WebAssembly module,
// built in the Lugforth repository and translated to C by wasm2c
// (vendor/lugforth/): it runs natively, but every address a cartridge makes is
// an offset in the module's memory, each access is checked against its bounds
// and traps past them, and the only ways out of the module are the imports
// below, which check the addresses they are handed. The words, their names and
// their stack effects are those of the pForth binding, so its cartridges keep
// working; the two builds are exclusive (the same script id).

#include "core/core.h"

// The core types of wasm2c are TIC-80's own, but for the floats.
#define WASM_RT_CORE_TYPES_DEFINED 1
typedef float f32;
typedef double f64;

#include "lugforth_w2c.h"
#include "wasm-rt-exceptions.h"
#include "wasm-rt-impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tic80_api.h"

#define LUGFORTH_OUT_SIZE 256
#define LUGFORTH_STR_SIZE 1024

// The instance of the imports: wasm2c hands it to each of them.
typedef struct w2c_tic80
{
    tic_core* core;
    w2c_lugforth module;
    bool instantiated;

    // Execution tokens of the callbacks, looked up after each evaluation.
    s32 tic, boot, scn, bdr, menu;

    // A trap leaves the module half way through a word: nothing more runs.
    bool broken;

    char out[LUGFORTH_OUT_SIZE];
    s32 outLen;

    // The output of the command running, for its error: the line that says
    // what went wrong has usually gone to the console already.
    char recent[LUGFORTH_OUT_SIZE];
    s32 recentLen;
} LugforthVM;

typedef enum
{
    Export_init,
    Export_alloc,
    Export_free,
    Export_eval,
    Export_find,
    Export_call,
    Export_call1,
    Export_depth,
} Export;

static LugforthVM* getVM(tic_core* core)
{
    return (LugforthVM*)core->currentVM;
}

// =============================================================================
// Output: TYPE goes to the console, a line at a time, and is kept for errors.
// =============================================================================

static void flushOut(LugforthVM* vm)
{
    if(vm->outLen > 0 && vm->core->data && vm->core->data->trace)
    {
        vm->out[vm->outLen] = '\0';
        vm->core->data->trace(vm->core->data->data, vm->out, 15);
    }
    vm->outLen = 0;
}

static void keepRecent(LugforthVM* vm, char c)
{
    if(vm->recentLen >= LUGFORTH_OUT_SIZE - 1)
    {
        s32 half = vm->recentLen / 2;
        memmove(vm->recent, vm->recent + half, vm->recentLen - half);
        vm->recentLen -= half;
    }

    // Lines are kept as one, without the empty ones.
    if(c == '\n')
    {
        if(vm->recentLen == 0 || vm->recent[vm->recentLen - 1] == ' ')
            return;
        c = ' ';
    }

    vm->recent[vm->recentLen++] = c;
}

static void putOut(LugforthVM* vm, char c)
{
    if(c == '\r')
        return;

    keepRecent(vm, c);

    if(c == '\n')
    {
        flushOut(vm);
        return;
    }

    vm->out[vm->outLen++] = c;
    if(vm->outLen >= LUGFORTH_OUT_SIZE - 1)
        flushOut(vm);
}

static void reportError(LugforthVM* vm, const char* what, const char* detail)
{
    tic_core* core = vm->core;
    if(!core->data)
        return;

    char buf[1024];
    while(vm->recentLen && vm->recent[vm->recentLen - 1] == ' ')
        vm->recentLen--;
    vm->recent[vm->recentLen] = '\0';
    snprintf(buf, sizeof buf, "Forth: %.200s%s%.200s%s%.255s", what,
             detail ? ": " : "", detail ? detail : "",
             vm->recentLen ? "\n" : "", vm->recent);
    vm->outLen = 0;
    vm->recentLen = 0;
    core->data->error(core->data->data, buf);
}

// =============================================================================
// Imports of the module "tic80"
// =============================================================================

static const struct
{
    const char* name;
    s32 args;
    s32 results;
} ApiWords[] =
    {
#define API(id, name, op, nargs, nres) [id] = {name, nargs, nres},
        TIC80_API_LIST
#undef API
};

enum
{
#define API(id, name, op, nargs, nres) Api_##op = id,
    TIC80_API_LIST
#undef API
        ApiCount
};

// The memory of the module, little endian as WebAssembly's on the hosts TIC-80
// runs on. An address and a length from the module are checked against it.
static u8* moduleBytes(LugforthVM* vm, u32 addr, u32 len)
{
    wasm_rt_memory_t* mem = w2c_lugforth_memory(&vm->module);
    if(addr > mem->size || len > mem->size - addr)
        wasm_rt_trap(WASM_RT_TRAP_OOB);
    return mem->data + addr;
}

// Copies a string of the module ( a n ) out, bounded: the API takes C strings.
static const char* moduleString(LugforthVM* vm, u32 addr, s32 len, char* buf)
{
    if(len < 0)
        wasm_rt_trap(WASM_RT_TRAP_OOB);
    const u8* text = moduleBytes(vm, addr, len);
    if(len >= LUGFORTH_STR_SIZE)
        len = LUGFORTH_STR_SIZE - 1;
    memcpy(buf, text, len);
    buf[len] = '\0';
    return buf;
}

static s32 transparent(s32 colorkey, u8* trans)
{
    if(colorkey < 0)
        return 0;
    trans[0] = (u8)colorkey;
    return 1;
}

// api ( id base n -- results ): runs API word id on the n cells at base, the
// last one the top of the Forth stack, and writes its results from base.
u32 w2c_tic80_api(LugforthVM* vm, u32 id, u32 base, u32 n)
{
    tic_core* core = vm->core;
    tic_mem* tic = (tic_mem*)core;

    if(id >= ApiCount || n != (u32)ApiWords[id].args || (base & 3))
        wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);

    s32 cells = MAX(ApiWords[id].args, ApiWords[id].results);
    u8* cell = moduleBytes(vm, base, cells * 4);
    s32 a[18] = {0};
    memcpy(a, cell, n * 4);

    s32 r[7] = {0};
    s32 results = 0;
    char str[LUGFORTH_STR_SIZE];
    u8 trans[1];

#define RESULT(v) (r[results++] = (s32)(v))

    switch(id)
    {
        case Api_CLS:
            core->api.cls(tic, a[0]);
            break;
        case Api_PRINT:
        {
            const char* text = moduleString(vm, a[0], a[1], str);
            RESULT(core->api.print(tic, text, a[2], a[3], a[4], a[5] != 0, a[6], a[7] != 0));
            break;
        }
        case Api_PIX:
            RESULT(core->api.pix(tic, a[0], a[1], 0, true));
            break;
        case Api_PIX_STORE:
            core->api.pix(tic, a[0], a[1], a[2], false);
            break;
        case Api_LINE:
            core->api.line(tic, a[0], a[1], a[2], a[3], a[4]);
            break;
        case Api_RECT:
            core->api.rect(tic, a[0], a[1], a[2], a[3], a[4]);
            break;
        case Api_RECTB:
            core->api.rectb(tic, a[0], a[1], a[2], a[3], a[4]);
            break;
        case Api_SPR:
        {
            s32 count = transparent(a[3], trans);
            core->api.spr(tic, a[0], a[1], a[2], a[7], a[8], trans, count, a[4], a[5], a[6]);
            break;
        }
        case Api_BTN:
            RESULT(core->api.btn(tic, a[0]));
            break;
        case Api_BTNP:
            RESULT(core->api.btnp(tic, a[0], a[1], a[2]));
            break;
        case Api_SFX:
            core->api.sfx(tic, a[0], a[1], a[2], a[3], a[4], a[5], a[5], a[6]);
            break;
        case Api_MAP:
        {
            s32 count = transparent(a[6], trans);
            core->api.map(tic, a[0], a[1], a[2], a[3], a[4], a[5], trans, count, a[7], NULL, NULL);
            break;
        }
        case Api_MGET:
            RESULT(core->api.mget(tic, a[0], a[1]));
            break;
        case Api_MSET:
            core->api.mset(tic, a[0], a[1], a[2]);
            break;
        case Api_PEEK:
            RESULT(core->api.peek(tic, a[0], a[1]));
            break;
        case Api_POKE:
            core->api.poke(tic, a[0], a[1], a[2]);
            break;
        case Api_PEEK1:
            RESULT(core->api.peek1(tic, a[0]));
            break;
        case Api_POKE1:
            core->api.poke1(tic, a[0], a[1]);
            break;
        case Api_PEEK2:
            RESULT(core->api.peek2(tic, a[0]));
            break;
        case Api_POKE2:
            core->api.poke2(tic, a[0], a[1]);
            break;
        case Api_PEEK4:
            RESULT(core->api.peek4(tic, a[0]));
            break;
        case Api_POKE4:
            core->api.poke4(tic, a[0], a[1]);
            break;
        case Api_MEMCPY:
            core->api.memcpy(tic, a[0], a[1], a[2]);
            break;
        case Api_MEMSET:
            core->api.memset(tic, a[0], a[1], a[2]);
            break;
        case Api_TRACE:
        {
            const char* text = moduleString(vm, a[0], a[1], str);
            core->api.trace(tic, text, a[2]);
            break;
        }
        case Api_PMEM:
            RESULT(core->api.pmem(tic, a[0], 0, false));
            break;
        case Api_PMEM_STORE:
            core->api.pmem(tic, a[1], (u32)a[0], true);
            break;
        case Api_TIME:
            RESULT(core->api.time(tic));
            break;
        case Api_TSTAMP:
            RESULT(core->api.tstamp(tic));
            break;
        case Api_EXITGAME:
            core->api.exit(tic);
            break;
        case Api_FONT:
        {
            const char* text = moduleString(vm, a[0], a[1], str);
            trans[0] = (u8)a[4];
            RESULT(core->api.font(tic, text, a[2], a[3], trans, 1, a[5], a[6], a[7] != 0, a[8], a[9] != 0));
            break;
        }
        case Api_MOUSE:
        {
            tic_point pos = core->api.mouse(tic);
            const tic80_mouse* m = &core->memory.ram->input.mouse;
            RESULT(pos.x);
            RESULT(pos.y);
            RESULT(m->left);
            RESULT(m->middle);
            RESULT(m->right);
            RESULT(m->scrollx);
            RESULT(m->scrolly);
            break;
        }
        case Api_CIRC:
            core->api.circ(tic, a[0], a[1], a[2], a[3]);
            break;
        case Api_CIRCB:
            core->api.circb(tic, a[0], a[1], a[2], a[3]);
            break;
        case Api_ELLI:
            core->api.elli(tic, a[0], a[1], a[2], a[3], a[4]);
            break;
        case Api_ELLIB:
            core->api.ellib(tic, a[0], a[1], a[2], a[3], a[4]);
            break;
        case Api_PAINT:
            core->api.paint(tic, a[0], a[1], a[2], a[3]);
            break;
        case Api_TRI:
            core->api.tri(tic, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            break;
        case Api_TRIB:
            core->api.trib(tic, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            break;
        case Api_TTRI:
        {
            s32 count = transparent(a[13], trans);
            core->api.ttri(tic, a[0], a[1], a[2], a[3], a[4], a[5],
                           a[6], a[7], a[8], a[9], a[10], a[11],
                           (tic_texture_src)a[12], trans, count, a[14], a[15], a[16], a[17] != 0);
            break;
        }
        case Api_CLIP:
            core->api.clip(tic, a[0], a[1], a[2], a[3]);
            break;
        case Api_CLIP0:
            core->api.clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
            break;
        case Api_MUSIC:
            core->api.music(tic, a[0], a[1], a[2], a[3] != 0, a[4] != 0, a[5], a[6]);
            break;
        case Api_SYNC:
            core->api.sync(tic, (u32)a[0], a[1], a[2] != 0);
            break;
        case Api_VBANK:
            RESULT(core->api.vbank(tic, a[0]));
            break;
        case Api_RESET:
            // As every other binding: the cart restarts at the next tick.
            core->state.initialized = false;
            break;
        case Api_KEYPRESSED:
            RESULT(core->api.key(tic, (tic_key)a[0]));
            break;
        case Api_KEYP:
            RESULT(core->api.keyp(tic, (tic_key)a[0], a[1], a[2]));
            break;
        case Api_FGET:
            RESULT(core->api.fget(tic, a[0], a[1]));
            break;
        case Api_FSET:
            core->api.fset(tic, a[0], a[1], a[2] != 0);
            break;
        case Api_FFT:
            RESULT(core->api.fft(tic, a[0], a[1]) * 65535.0);
            break;
        case Api_FFTS:
            RESULT(core->api.ffts(tic, a[0], a[1]) * 65535.0);
            break;
        case Api_RRECT:
            core->api.rrect(tic, a[0], a[1], a[2], a[3], a[4], a[5], (float)a[6]);
            break;
        case Api_RRECTB:
            core->api.rrectb(tic, a[0], a[1], a[2], a[3], a[4], a[5], (float)a[6]);
            break;
        case Api_BTNR:
            RESULT(core->api.btnr(tic, a[0]));
            break;
        case Api_KEYR:
            RESULT(core->api.keyr(tic, (tic_key)a[0]));
            break;
    }

#undef RESULT

    // An API word never grows the memory of the module, but the bounds are
    // taken again anyway.
    memcpy(moduleBytes(vm, base, cells * 4), r, results * 4);
    return results;
}

// type ( a n -- ): the text of TYPE.
void w2c_tic80_type(LugforthVM* vm, u32 addr, u32 len)
{
    const char* text = (const char*)moduleBytes(vm, addr, len);
    for(u32 i = 0; i < len; i++)
        putOut(vm, text[i]);
}

u32 w2c_tic80_ticks(LugforthVM* vm)
{
    return (u32)(s32)vm->core->api.time((tic_mem*)vm->core);
}

#define MATH1(name)                             \
    f64 w2c_tic80_##name(LugforthVM* vm, f64 x) \
    {                                           \
        (void)vm;                               \
        return name(x);                         \
    }

#define MATH2(name)                                    \
    f64 w2c_tic80_##name(LugforthVM* vm, f64 x, f64 y) \
    {                                                  \
        (void)vm;                                      \
        return name(x, y);                             \
    }

MATH1(sin)
MATH1(cos)
MATH1(exp)
MATH1(log)
MATH2(atan2)
MATH2(pow)

// =============================================================================
// Calls into the module
// =============================================================================

// Runs an export; on a trap, says so and stops the module.
static bool callModule(LugforthVM* vm, Export fn, s32* ret, const char* what, s32 a, s32 b)
{
    if(vm->broken)
        return false;

    wasm_rt_trap_t trap = wasm_rt_impl_try();
    if(trap)
    {
        vm->broken = true;
        reportError(vm, what, wasm_rt_strerror(trap));
        return false;
    }

    w2c_lugforth* m = &vm->module;
    u32 r = 0;
    switch(fn)
    {
        case Export_init: r = w2c_lugforth_lf_init(m); break;
        case Export_alloc: r = w2c_lugforth_lf_alloc(m, a); break;
        case Export_free: w2c_lugforth_lf_free(m, a); break;
        case Export_eval: r = w2c_lugforth_lf_eval(m, a, b); break;
        case Export_find: r = w2c_lugforth_lf_find(m, a, b); break;
        case Export_call: r = w2c_lugforth_lf_call(m, a); break;
        case Export_call1: r = w2c_lugforth_lf_call1(m, a, b); break;
        case Export_depth: r = w2c_lugforth_lf_depth(m); break;
    }

    if(ret)
        *ret = (s32)r;

    return true;
}

// Copies a text into the module: its address there, or 0.
static s32 toModule(LugforthVM* vm, const char* text, s32 len)
{
    s32 addr = 0;
    if(!callModule(vm, Export_alloc, &addr, "out of memory", len ? len : 1, 0) || !addr)
        return 0;

    wasm_rt_memory_t* mem = w2c_lugforth_memory(&vm->module);
    if((u32)addr > mem->size || (u32)len > mem->size - (u32)addr)
        return 0;

    memcpy(mem->data + addr, text, len);
    return addr;
}

static s32 findWord(LugforthVM* vm, const char* name)
{
    s32 len = (s32)strlen(name);
    s32 addr = toModule(vm, name, len);
    if(!addr)
        return 0;

    s32 xt = 0;
    callModule(vm, Export_find, &xt, "find", addr, len);
    callModule(vm, Export_free, NULL, "free", addr, 0);
    return xt;
}

static void findCallbacks(LugforthVM* vm)
{
    vm->tic = findWord(vm, TIC_FN);
    vm->boot = findWord(vm, BOOT_FN);
    vm->scn = findWord(vm, SCN_FN);
    vm->bdr = findWord(vm, BDR_FN);
    vm->menu = findWord(vm, MENU_FN);

#if defined(BUILD_RENDER_CACHE)
    vm->core->state.has_scn = vm->scn != 0;
    vm->core->state.has_bdr = vm->bdr != 0;
#endif
}

static bool evaluate(LugforthVM* vm, const char* code)
{
    s32 len = (s32)strlen(code);
    s32 addr = toModule(vm, code, len);
    if(!addr)
    {
        if(!vm->broken)
            reportError(vm, "out of memory", NULL);
        return false;
    }

    s32 thrown = 0;
    vm->recentLen = 0;
    bool ok = callModule(vm, Export_eval, &thrown, "evaluate", addr, len);
    if(ok)
        callModule(vm, Export_free, NULL, "free", addr, 0);

    if(ok && thrown)
    {
        char code[32];
        snprintf(code, sizeof code, "THROW %d", (int)thrown);
        reportError(vm, code, NULL);
        return false;
    }

    flushOut(vm);
    return ok;
}

// Runs a callback; a word that leaves or takes cells is an error, as in the
// pForth binding (the module empties the stack before each call anyway).
static void callWord(tic_mem* tic, s32 xt, const char* name, s32 param, bool hasParam)
{
    tic_core* core = (tic_core*)tic;
    LugforthVM* vm = getVM(core);
    if(!vm || !xt || vm->broken)
        return;

    s32 thrown = 0;
    vm->recentLen = 0;
    bool ok = hasParam
                  ? callModule(vm, Export_call1, &thrown, name, xt, param)
                  : callModule(vm, Export_call, &thrown, name, xt, 0);

    if(!ok)
        return;

    if(thrown)
    {
        char what[64];
        snprintf(what, sizeof what, "THROW %d in %s", (int)thrown, name);
        reportError(vm, what, NULL);
        return;
    }

    s32 depth = 0;
    if(callModule(vm, Export_depth, &depth, "depth", 0, 0) && depth != 0)
    {
        char what[128];
        snprintf(what, sizeof what, "stack %s in %s (%d item%s %s)",
                 depth > 0 ? "overflow" : "underflow", name, (int)abs(depth),
                 abs(depth) == 1 ? "" : "s", depth > 0 ? "extra left" : "missing");
        reportError(vm, what, NULL);
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

static void closeLugforth(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    LugforthVM* vm = getVM(core);

    if(vm)
    {
        flushOut(vm);
        if(vm->instantiated)
            wasm2c_lugforth_free(&vm->module);
        free(vm);
        core->currentVM = NULL;
    }
}

static bool initLugforth(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeLugforth(tic);

    LugforthVM* vm = calloc(1, sizeof(LugforthVM));
    if(!vm)
        return false;

    vm->core = core;
    core->currentVM = vm;

    static bool runtimeReady;
    if(!runtimeReady)
    {
        wasm_rt_init();
        runtimeReady = true;
    }

    wasm_rt_trap_t trap = wasm_rt_impl_try();
    if(trap)
    {
        reportError(vm, "cannot load the Lugforth module", wasm_rt_strerror(trap));
        closeLugforth(tic);
        return false;
    }

    wasm2c_lugforth_instantiate(&vm->module, vm);
    vm->instantiated = true;

    s32 ready = -1;
    if(!callModule(vm, Export_init, &ready, "boot", 0, 0) || ready != 0)
    {
        if(!vm->broken)
            reportError(vm, "the boot of Lugforth failed", NULL);
        closeLugforth(tic);
        return false;
    }

    if(!evaluate(vm, code))
    {
        closeLugforth(tic);
        return false;
    }

    findCallbacks(vm);
    return true;
}

static void callLugforthTick(tic_mem* tic)
{
    LugforthVM* vm = getVM((tic_core*)tic);
    if(!vm)
        return;

    callWord(tic, vm->tic, TIC_FN, 0, false);
    // '.' output that has no line end yet.
    flushOut(vm);
}

static void callLugforthBoot(tic_mem* tic)
{
    LugforthVM* vm = getVM((tic_core*)tic);
    if(vm)
        callWord(tic, vm->boot, BOOT_FN, 0, false);
}

static void callLugforthScanline(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    LugforthVM* vm = getVM((tic_core*)tic);
    if(vm)
        callWord(tic, vm->scn, SCN_FN, row, true);
}

static void callLugforthBorder(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    LugforthVM* vm = getVM((tic_core*)tic);
    if(vm)
        callWord(tic, vm->bdr, BDR_FN, row, true);
}

static void callLugforthMenu(tic_mem* tic, s32 index, void* data)
{
    (void)data;
    LugforthVM* vm = getVM((tic_core*)tic);
    if(vm)
        callWord(tic, vm->menu, MENU_FN, index, true);
}

static void evalLugforth(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    if(!getVM(core) && !initLugforth(tic, ""))
        return;

    LugforthVM* vm = getVM(core);
    if(evaluate(vm, code))
        findCallbacks(vm);
}

// =============================================================================
// Editor support
// =============================================================================

// Lines that start with ': NAME' are the outline.
static const tic_outline_item* getLugforthOutline(const char* code, s32* size)
{
    static tic_outline_item items[128];
    *size = 0;

    for(const char* p = code; *p;)
    {
        if(*p == ':' && (p == code || p[-1] == '\n'))
        {
            p++;
            while(*p == ' ' || *p == '\t')
                p++;

            const char* start = p;
            while(*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                p++;

            if(p > start && *size < COUNT_OF(items))
            {
                items[*size].pos = start;
                items[*size].size = (s32)(p - start);
                (*size)++;
            }
        }

        while(*p && *p != '\n')
            p++;
        if(*p)
            p++;
    }

    return *size ? items : NULL;
}

static const char* const LugforthKeywords[] =
    {
        "IF",
        "THEN",
        "ELSE",
        "BEGIN",
        "UNTIL",
        "WHILE",
        "REPEAT",
        "AGAIN",
        "DO",
        "?DO",
        "LOOP",
        "+LOOP",
        "LEAVE",
        "UNLOOP",
        "I",
        "J",
        "EXIT",
        "RECURSE",
        "CASE",
        "OF",
        "ENDOF",
        "ENDCASE",
        ":",
        ";",
        ":NONAME",
        "VARIABLE",
        "CONSTANT",
        "VALUE",
        "TO",
        "CREATE",
        "DOES>",
        "DEFER",
        "IS",
        "IMMEDIATE",
        "POSTPONE",
        "LITERAL",
        "[",
        "]",
        "DUP",
        "DROP",
        "SWAP",
        "OVER",
        "ROT",
        "-ROT",
        "NIP",
        "TUCK",
        "PICK",
        "ROLL",
        "2DUP",
        "2DROP",
        "2SWAP",
        "2OVER",
        ">R",
        "R>",
        "R@",
        "+",
        "-",
        "*",
        "/",
        "MOD",
        "/MOD",
        "*/",
        "*/MOD",
        "MAX",
        "MIN",
        "ABS",
        "NEGATE",
        "1+",
        "1-",
        "2*",
        "2/",
        "AND",
        "OR",
        "XOR",
        "INVERT",
        "LSHIFT",
        "RSHIFT",
        "=",
        "<>",
        "<",
        ">",
        "<=",
        ">=",
        "0=",
        "0<",
        "0>",
        "@",
        "!",
        "C@",
        "C!",
        "+!",
        "MOVE",
        "FILL",
        ".",
        ".\"",
        "EMIT",
        "CR",
        "SPACE",
        "SPACES",
        "TYPE",
        "S\"",
        "C\"",
        "CHAR",
        "[CHAR]",
        "TRUE",
        "FALSE",
        "CELL",
        "CELLS",
        "HERE",
        "ALLOT",
        ",",
        "CATCH",
        "THROW",
        "{:",
        ":}",
};

static const char* LugforthApiKeywords[] =
    {
#define TIC_CALLBACK_DEF(name, ...) #name,
        TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef TIC_CALLBACK_DEF
#define API(id, name, op, nargs, nres) name,
            TIC80_API_LIST
#undef API
};

static const u8 DemoRom[] =
    {
#include "../build/assets/forthdemo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/forthmark.tic.dat"
};

// The same id, name and extension as the pForth binding: a Forth cartridge
// runs on either build.
TIC_EXPORT const tic_script EXPORT_SCRIPT(Forth) =
    {
        .id = 22,
        .name = "forth",
        .fileExtension = ".fth",
        .projectComment = "\\",
        {
            .init = initLugforth,
            .close = closeLugforth,
            .tick = callLugforthTick,
            .boot = callLugforthBoot,
            .callback =
                {
                    .scanline = callLugforthScanline,
                    .border = callLugforthBorder,
                    .menu = callLugforthMenu,
                },
        },
        .getOutline = getLugforthOutline,
        .eval = evalLugforth,

        .blockCommentStart = NULL,
        .blockCommentEnd = NULL,
        .blockCommentStart2 = NULL,
        .blockCommentEnd2 = NULL,
        .singleComment = "\\",
        .blockStringStart = NULL,
        .blockStringEnd = NULL,
        .stdStringStartEnd = NULL,
        .blockEnd = NULL,

        .keywords = LugforthKeywords,
        .keywordsCount = COUNT_OF(LugforthKeywords),

        .api_keywords = LugforthApiKeywords,
        .api_keywordsCount = COUNT_OF(LugforthApiKeywords),

        .demo = {DemoRom, sizeof DemoRom, "forthdemo.tic"},
        .mark = {MarkRom, sizeof MarkRom, "forthmark.tic"},
};
