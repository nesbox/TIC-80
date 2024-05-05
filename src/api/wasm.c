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

#include "core/core.h"

#define dbg(...) printf(__VA_ARGS__)
//define dbg(...)

#include "tools.h"

#include <ctype.h>

// Avoid redefining u* and s*
#define d_m3ShortTypesDefined 1
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef double          f64;
typedef float           f32;

#include "wasm3.h"
#include "m3_exec_defs.h"
#include "m3_exception.h"
#include "m3_env.h"
#include "m3_core.h"

static const char TicCore[] = "_TIC80";

IM3Function BDR_function;
IM3Function SCN_function;
IM3Function TIC_function;
IM3Function BOOT_function;
IM3Function MENU_function;

#define FATAL(msg, ...) { printf("Error: [Fatal] " msg "\n", ##__VA_ARGS__); goto _onfatal; }
#define WASM_STACK_SIZE 64*1024

static
M3Result SuppressLookupFailure (M3Result i_result)
{
    if (i_result == m3Err_functionLookupFailed)
        return m3Err_none;
    else
        return i_result;
}

M3Result wasm_load  (IM3Runtime runtime, const char* fn)
{
    M3Result result = m3Err_none;

    u8* wasm = NULL;
    u32 fsize = 0;

    FILE* f = fopen (fn, "rb");
    if (!f) {
        return "cannot open file";
    }
    fseek (f, 0, SEEK_END);
    fsize = ftell(f);
    fseek (f, 0, SEEK_SET);

    if (fsize < 8) {
        return "file is too small";
    } else if (fsize > 10*1024*1024) {
        return "file too big";
    }

    wasm = (u8*) malloc(fsize);
    if (!wasm) {
        return "cannot allocate memory for wasm binary";
    }

    if (fread (wasm, 1, fsize, f) != fsize) {
        return "cannot read file";
    }
    fclose (f);

    IM3Module module;
    result = m3_ParseModule (runtime->environment, &module, wasm, fsize);
    if (result) return result;

    result = m3_LoadModule (runtime, module);
    if (result) return result;

    return result;
}

/*
M3Result wasm_call  (IM3Runtime runtime, const char* name, int argc, void* argv)
{
    M3Result result = m3Err_none;

    IM3Function func;
    result = m3_FindFunction (&func, runtime, name);
    if (result) return result;

    result = m3_CallWithArgs (func, argc, argv);
    if (result) return result;

    return result;
}
*/
M3Result wasm_dump(IM3Runtime runtime)
{
    uint32_t len;
    uint8_t* mem = m3_GetMemory(runtime, &len, 0);
    if (mem) {
        FILE* f = fopen ("wasm3_dump.bin", "wb");
        if (!f) {
            return "cannot open file";
        }
        if (fwrite (mem, 1, len, f) != len) {
            return "cannot write file";
        }
        fclose (f);
    }
    return m3Err_none;
}

static tic_core* getWasmCore(IM3Runtime ctx)
{
    return (tic_core*)ctx->userdata;
}

m3ApiRawFunction(wasmtic_line)
{
    m3ApiGetArg      (float, x0)
    m3ApiGetArg      (float, y0)
    m3ApiGetArg      (float, x1)
    m3ApiGetArg      (float, y1)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.line(tic, x0, y0, x1, y1, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_circ)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, radius)
    m3ApiGetArg      (int8_t, color)

    if (radius >= 0)
    {
      tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
      core->api.circ(tic, x, y, radius, color);
    }

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_circb)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, radius)
    m3ApiGetArg      (int8_t, color)

    if (radius >= 0)
    {
      tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
      core->api.circb(tic, x, y, radius, color);
    }

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_elli)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, a)
    m3ApiGetArg      (int32_t, b)
    m3ApiGetArg      (int8_t, color)

    if (a >= 0 && b >= 0)
    {
      tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
      core->api.elli(tic, x, y, a, b, color);
    }

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_ellib)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, a)
    m3ApiGetArg      (int32_t, b)
    m3ApiGetArg      (int8_t, color)

    if (a >= 0 && b >= 0)
    {
      tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
      core->api.ellib(tic, x, y, a, b, color);
    }

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_rect)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.rect(tic, x, y, w, h, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_rectb)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.rectb(tic, x, y, w, h, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_tri)
{
    m3ApiGetArg      (float, x1)
    m3ApiGetArg      (float, y1)
    m3ApiGetArg      (float, x2)
    m3ApiGetArg      (float, y2)
    m3ApiGetArg      (float, x3)
    m3ApiGetArg      (float, y3)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.tri(tic, x1, y1, x2, y2, x3, y3, color);

    m3ApiSuccess();
}

// ttri x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 [texsrc=0] [trans=-1] [z1 z2 z3 depth]
m3ApiRawFunction(wasmtic_ttri)
{
    m3ApiGetArg      (float, x1)
    m3ApiGetArg      (float, y1)
    m3ApiGetArg      (float, x2)
    m3ApiGetArg      (float, y2)
    m3ApiGetArg      (float, x3)
    m3ApiGetArg      (float, y3)
    m3ApiGetArg      (float, u1)
    m3ApiGetArg      (float, v1)
    m3ApiGetArg      (float, u2)
    m3ApiGetArg      (float, v2)
    m3ApiGetArg      (float, u3)
    m3ApiGetArg      (float, v3)
    m3ApiGetArg      (int32_t, texsrc)
    m3ApiGetArgMem   (u8*, trans_colors)
    m3ApiGetArg      (int8_t, colorCount)
    m3ApiGetArg      (float, z1)
    m3ApiGetArg      (float, z2)
    m3ApiGetArg      (float, z3)
    m3ApiGetArg      (bool, depth)    
    if (trans_colors == NULL) {
        colorCount = 0;
    }

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
    
    core->api.ttri(tic, x1, y1, x2, y2, x3, y3, 
        u1, v1, u2, v2, u3, v3, texsrc, trans_colors, colorCount, z1, z2, z3, depth);

    m3ApiSuccess();
}


m3ApiRawFunction(wasmtic_trib)
{
    m3ApiGetArg      (float, x1)
    m3ApiGetArg      (float, y1)
    m3ApiGetArg      (float, x2)
    m3ApiGetArg      (float, y2)
    m3ApiGetArg      (float, x3)
    m3ApiGetArg      (float, y3)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.trib(tic, x1, y1, x2, y2, x3, y3, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_cls)
{
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    color = (color == -1) ? 0 : color;

    core->api.cls(tic, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_btn)
{
    // TODO: should this be boolean, how to deal with
    // this being multi-typed
    m3ApiReturnType  (int32_t)

    m3ApiGetArg      (int32_t, index)

    tic_core* core = getWasmCore(runtime);

    // -1 is a default placeholder here for `id`, but one that `core->api.btn` already understands, so
    // it just gets passed straight thru

    m3ApiReturn(core->api.btn((tic_mem *)core, index));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_btnp)
{
    m3ApiReturnType  (bool)

    m3ApiGetArg      (int32_t, index)
    m3ApiGetArg      (int32_t, hold)
    m3ApiGetArg      (int32_t, period)

    tic_core* core = getWasmCore(runtime);

    // -1 is the "default" placeholder for index, hold, and period but the TIC side API
    // knows this so we don't need to do any transaction, we can just pass the -1 values
    // straight thru 

    m3ApiReturn(core->api.btnp((tic_mem *)core, index, hold, period));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_key)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArg      (int32_t, index)

    if (index == -1) {
        index = tic_key_unknown;
    }

    tic_core* core = getWasmCore(runtime);

    m3ApiReturn(core->api.key((tic_mem *)core, index));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_keyp)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArg      (int8_t, index)
    m3ApiGetArg      (int32_t, hold)
    m3ApiGetArg      (int32_t, period)

    if (index == -1) {
        index = tic_key_unknown;
    }

    tic_core* core = getWasmCore(runtime);

    m3ApiReturn(core->api.keyp((tic_mem *)core, index, hold, period));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_fget)
{
    m3ApiReturnType  (bool)

    m3ApiGetArg      (int32_t, sprite_index);
    m3ApiGetArg      (int8_t, flag);
    
    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.fget(tic, sprite_index, flag));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_fset)
{
    m3ApiGetArg      (int32_t, sprite_index);
    m3ApiGetArg      (int8_t, flag);
    m3ApiGetArg      (bool, value);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.fset(tic, sprite_index, flag, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_mget)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArg      (int32_t, x);
    m3ApiGetArg      (int32_t, y);
    
    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.mget(tic, x, y));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_mset)
{
    m3ApiGetArg      (int32_t, x);
    m3ApiGetArg      (int32_t, y);
    m3ApiGetArg      (int32_t, value);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.mset(tic, x, y, value);

    m3ApiSuccess();
}


m3ApiRawFunction(wasmtic_peek)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, bits)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.peek(tic, address, bits));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_peek4)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.peek4(tic, address));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_peek2)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.peek2(tic, address));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_peek1)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.peek1(tic, address));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke)
{
    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)
    m3ApiGetArg      (int8_t, bits)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.poke(tic, address, value, bits);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke4)
{

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.poke4(tic, address, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke2)
{

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.poke2(tic, address, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke1)
{

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.poke1(tic, address, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_pmem)
{
    m3ApiReturnType  (uint32_t)

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int64_t, value)
    bool writeToStorage = true;

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    // read value
    if (value == -1) { 
        writeToStorage = false;
    };

    // TODO: this should move into core->api.pmem, should it not?
    if (address >= TIC_PERSISTENT_SIZE) {
        m3ApiReturn(0);
    }  else {
        u32 val = core->api.pmem(tic, address, value, writeToStorage);
        m3ApiReturn(val);
    }
    
    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_pix)
{
    m3ApiReturnType  (u8)

    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int8_t, color)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    bool getPixel = color < 0;
    u8 pix = core->api.pix(tic, x, y, color, getPixel);
    m3ApiReturn(pix);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_spr)
{
    m3ApiGetArg      (int32_t, index)
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArgMem   (u8*, trans_colors)
    m3ApiGetArg      (int8_t, colorCount)
    if (trans_colors == NULL) {
        colorCount = 0;
    }
    m3ApiGetArg      (int32_t, scale)
    m3ApiGetArg      (int32_t, flip)
    m3ApiGetArg      (int32_t, rotate)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;


    // defaults
    if (scale == -1) { scale = 1; }
    if (flip == -1) { flip = 0; }
    if (rotate == -1) { rotate = 0; }
    if (w == -1) { w = 1; }
    if (h == -1) { h = 1; }

    core->api.spr(tic, index, x, y, w, h, trans_colors, colorCount, scale, flip, rotate) ;

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_clip)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    // defaults
    if (x == -1) { x = 0; }
    if (y == -1) { y = 0; }
    if (w == -1) { w = TIC80_WIDTH; }
    if (h == -1) { h = TIC80_HEIGHT; }

    core->api.clip(tic, x, y, w, h);

    m3ApiSuccess();
}

struct MapData {
    int32_t func_index;
    uint32_t user_data;
    uint32_t res_ptr;
};
struct RemapInfo {
    uint32_t data;
    IM3Function fun;
    tic_core* core;
    uint8_t* mem;
    RemapResult* wasm_result;
};
static void wasm_remap(void* data, s32 x, s32 y, RemapResult* result) {
    if (data == NULL) return;
    struct RemapInfo* rdata = (struct RemapInfo*) data;
    IM3Function fun = rdata->fun;
    // : )
    uint8_t* _mem = rdata->mem;

    *rdata->wasm_result = *result;
    M3Result res = m3_CallV(fun, rdata->data, x, y, m3ApiPtrToOffset(rdata->wasm_result));
    if (res) { 
        rdata->core->data->error(rdata->core->data->data, res);
        return;
    }

    *result = *rdata->wasm_result;
}
m3ApiRawFunction(wasmtic_map)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
    m3ApiGetArg      (int32_t, sx)
    m3ApiGetArg      (int32_t, sy)
    m3ApiGetArgMem   (u8*, trans_colors)
    m3ApiGetArg      (int8_t, colorCount)
    if (trans_colors == NULL) {
        colorCount = 0;
    }    m3ApiGetArg      (int8_t, scale)
    m3ApiGetArg      (i32, map_data_ptr)
    

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;
    // depends on their only being 1 module, which SHOULD:tm: be true
    struct RemapInfo info = { .mem = _mem, .core = (tic_core*)tic };
    if (map_data_ptr > 0) {
        struct MapData* map_data = (struct MapData*)m3ApiOffsetToPtr(map_data_ptr);
        m3_GetTableFunction(&info.fun, runtime->modules, map_data->func_index);
        info.data = map_data->user_data;
        info.wasm_result = (RemapResult*)m3ApiOffsetToPtr(map_data->res_ptr);
    }
    // defaults
    if (x == -1) { x = 0; }    
    if (y == -1) { y = 0; }
    if (w == -1) { w = 30; }
    if (h == -1) { h = 17; }
    if (scale == -1) { scale = 1; }

    core->api.map(tic, x, y, w, h, sx, sy, trans_colors, colorCount, scale, map_data_ptr > 0 ? &wasm_remap : NULL, map_data_ptr > 0 ? &info : NULL);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_print)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem   (const char *, text)
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int8_t, color)
    // optional arguments, but we'll force people to pass them all
    // let the pre-language APIs deal with simplifying
    m3ApiGetArg      (int8_t, fixed)
    m3ApiGetArg      (int8_t, scale)
    m3ApiGetArg      (int8_t, alt)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    // TODO: how to deal with defaults when we can't pass -1 as a bool?

    int32_t text_width;
    if (scale==0) {
        text_width = 0;
    } else {
        text_width = core->api.print(tic, text, x, y, color, fixed, scale, alt);
    }
    m3ApiReturn(text_width);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_font)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem   (const char *, text)
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArgMem   (u8*, trans_colors)
    m3ApiGetArg      (int8_t, trans_count)
    if (trans_colors == NULL) {
        trans_count = 0;
    }
    m3ApiGetArg      (int8_t, char_width)
    m3ApiGetArg      (int8_t, char_height)
    m3ApiGetArg      (bool, fixed)
    m3ApiGetArg      (int8_t, scale)
    m3ApiGetArg      (bool, alt)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    if (scale == -1) { scale = 1; }
    if (char_width == -1 ) { char_width = TIC_SPRITESIZE; }
    if (char_height == -1 ) { char_height = TIC_SPRITESIZE; }

    int32_t text_width;
    if (scale==0) {
        text_width = 0;
    } else {
        text_width = core->api.font(tic, text, x, y, trans_colors, trans_count, char_width, char_height, fixed, scale, alt);
    }
    m3ApiReturn(text_width);

    m3ApiSuccess();
}

// audio 

// id [note] [duration=-1] [channel=0] [volume=15] [speed=0]
m3ApiRawFunction(wasmtic_sfx)
{
    m3ApiGetArg      (int32_t, sfx_id);
    m3ApiGetArg      (int32_t, note);
    m3ApiGetArg      (int32_t, octave);
    m3ApiGetArg      (int32_t, duration);
    m3ApiGetArg      (int32_t, channel);
    m3ApiGetArg      (int32_t, volumeLeft);
    m3ApiGetArg      (int32_t, volumeRight);
    m3ApiGetArg      (int32_t, speed);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
    {
        core->api.sfx(tic, sfx_id, note, octave, duration, channel, volumeLeft & 0xf, volumeRight & 0xf, speed);
    }    

    m3ApiSuccess();
}


m3ApiRawFunction(wasmtic_music)
{
    m3ApiGetArg      (int32_t, track);
    m3ApiGetArg      (int32_t, frame);
    m3ApiGetArg      (int32_t, row);
    m3ApiGetArg      (bool, loop);
    m3ApiGetArg      (bool, sustain);
    m3ApiGetArg      (int32_t, tempo);
    m3ApiGetArg      (int32_t, speed);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    // TODO: defaults... how with bool vs int

    if(track > MUSIC_TRACKS - 1)
        m3ApiTrap("invalid music track index");

    core->api.music(tic, track, frame, row, loop, sustain, tempo, speed);

    m3ApiSuccess();
}

// memory

m3ApiRawFunction(wasmtic_memset)
{
    m3ApiGetArg      (int32_t, address);
    m3ApiGetArg      (int32_t, value);
    m3ApiGetArg      (int32_t, length);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.memset(tic, address, value, length);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_memcpy)
{
    m3ApiGetArg      (int32_t, dest);
    m3ApiGetArg      (int32_t, src);
    m3ApiGetArg      (int32_t, length);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.memcpy(tic, dest, src, length);

    m3ApiSuccess();
}


m3ApiRawFunction(wasmtic_exit)
{
    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    core->api.exit(tic);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_sync)
{
    m3ApiGetArg      (int32_t, mask);
    m3ApiGetArg      (int8_t, bank);
    m3ApiGetArg      (int8_t, tocart);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    bool toCart;

    if (mask == -1) {
        mask = 0;
    }
    if (bank == -1) {
        bank = 0;
    }
    toCart = (tocart == -1 || tocart == 0) ? false : true;

    // TODO: how to throw error if bank out of bounds?
    if (bank >=0 && bank < TIC_BANKS)
        core->api.sync(tic, mask, bank, toCart);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_time)
{
    m3ApiReturnType  (float) // 32 bit float

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.time(tic));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_tstamp)
{
    m3ApiReturnType  (uint32_t)

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    m3ApiReturn(core->api.tstamp(tic));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_trace)
{
    m3ApiGetArgMem(const char*, text);
    m3ApiGetArg(int8_t, color);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    if (color == -1) {
        color = 15;
    }

    core->api.trace(tic, text, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_vbank)
{
    m3ApiReturnType(int8_t)
    m3ApiGetArg(int8_t, bank);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    uint8_t previous_bank = core->api.vbank(tic, bank);
    m3ApiReturn(previous_bank);

    m3ApiSuccess();
}


// input

m3ApiRawFunction(wasmtic_mouse)
{
    struct Mouse {
        int16_t x;
        int16_t y;
        int8_t scrollx;
        int8_t scrolly;
        bool left;
        bool middle;
        bool right;
    };

    m3ApiGetArgMem(struct Mouse*, mouse_ptr_addy);

    tic_core* core = getWasmCore(runtime); tic_mem* tic = (tic_mem*)core;

    struct Mouse* mouse_data = mouse_ptr_addy;
    const tic80_mouse* mouse = &tic->ram->input.mouse;

    tic_point pos = core->api.mouse(tic);

    mouse_data->x = pos.x;
    mouse_data->y = pos.y;
    mouse_data->left = mouse->left;
    mouse_data->middle = mouse->middle;
    mouse_data->right = mouse->right;
    mouse_data->scrollx = mouse->scrollx;
    mouse_data->scrolly = mouse->scrolly;

    m3ApiSuccess();
}



M3Result linkTicAPI(IM3Module module)
{
    M3Result result = m3Err_none;
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "btn",     "i(i)",          &wasmtic_btn)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "btnp",    "i(iii)",        &wasmtic_btnp)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "clip",    "v(iiii)",       &wasmtic_clip)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "cls",     "v(i)",          &wasmtic_cls)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "circ",    "v(iiii)",       &wasmtic_circ)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "circb",   "v(iiii)",       &wasmtic_circb)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "elli",    "v(iiiii)",      &wasmtic_elli)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "ellib",   "v(iiiii)",      &wasmtic_ellib)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "exit",    "v()",           &wasmtic_exit)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "fget",    "i(ii)",         &wasmtic_fget)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "fset",    "v(iii)",        &wasmtic_fset)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "font",    "i(*iiiiiiiii)", &wasmtic_font)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "key",     "i(i)",          &wasmtic_key)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "keyp",    "i(iii)",        &wasmtic_keyp)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "line",    "v(ffffi)",      &wasmtic_line)));
    // TODO: needs a lot of help for all the optional arguments
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "map",     "v(iiiiiiiiii)", &wasmtic_map)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "memcpy",  "v(iii)",        &wasmtic_memcpy)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "memset",  "v(iii)",        &wasmtic_memset)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "mget",    "i(ii)",         &wasmtic_mget)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "mset",    "v(iii)",        &wasmtic_mset)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "mouse",   "v(*)",          &wasmtic_mouse)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "music",   "v(iiiiiii)",    &wasmtic_music)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "pix",     "i(iii)",        &wasmtic_pix)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek",    "i(ii)",         &wasmtic_peek)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek4",   "i(i)",          &wasmtic_peek4)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek2",   "i(i)",          &wasmtic_peek2)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek1",   "i(i)",          &wasmtic_peek1)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "pmem",    "i(iI)",         &wasmtic_pmem)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke",    "v(iii)",        &wasmtic_poke)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke4",   "v(ii)",         &wasmtic_poke4)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke2",   "v(ii)",         &wasmtic_poke2)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke1",   "v(ii)",         &wasmtic_poke1)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "print",   "i(*iiiiii)",    &wasmtic_print)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "rect",    "v(iiiii)",      &wasmtic_rect)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "rectb",   "v(iiiii)",      &wasmtic_rectb)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "sfx",     "v(iiiiiiii)",   &wasmtic_sfx)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "spr",     "v(iiiiiiiiii)", &wasmtic_spr)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "sync",    "v(iii)",        &wasmtic_sync)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "time",    "f()",           &wasmtic_time)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "tstamp",  "i()",           &wasmtic_tstamp)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "trace",   "v(*i)",         &wasmtic_trace)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "tri",     "v(ffffffi)",    &wasmtic_tri)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "trib",    "v(ffffffi)",    &wasmtic_trib)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "ttri",  "v(ffffffffffffiiifffi)",    &wasmtic_ttri)));
    _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "vbank",   "i(i)",          &wasmtic_vbank)));

_catch:
  return result;
}

void deinitWasmRuntime( IM3Runtime runtime )
{
    dbg("Denitializing wasm runtime\n");
    if (runtime == NULL)
    {
        printf("WARNING deinitWasm of null");
        return;
    }
    IM3Environment env = runtime -> environment;
    printf("deiniting env %p\n", env);

    tic_core* tic = getWasmCore(runtime);
    m3_FreeRuntime (runtime);
    m3_FreeEnvironment (env);
}

static void closeWasm(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->currentVM)
    {
        // this is necessary because when TIC-80 calls tic_reset_api, we
        // may not even have a VM.  Because the [non WASM] VM isn't
        // initialized yet when loading a new cartridge the "init" steps
        // are being applied to the WASM RAM, not the base RAM, so as we're
        // on our way out we need to copy the our lower 96kb back into base
        // RAM to close this loophole

        // TODO: This could also be fixed by correcting the sequencing such
        // that prior VMs were released before starting the init process
        // for an entirely different VM.  This sequencing matters a lot
        // less if one assumes (like before) that all the VMs share a
        // common memory area.
        u8* low_ram =  (u8*)core->memory.base_ram;
        u8* wasm_ram = m3_GetMemory(core->currentVM, NULL, 0);
        memcpy(low_ram, wasm_ram, TIC_RAM_SIZE);
        deinitWasmRuntime(core->currentVM);
        core->currentVM = NULL;
        core->memory.ram = NULL;
    }
}

// TODO: restore functionality
// static u64 ForceExitCounter = 0;

// s32 wasm_timeout_check(void* udata)
// {
//     tic_core* core = (tic_core*)udata;
//     tic_tick_data* tick = core->data;

//     return ForceExitCounter++ > 1000 ? tick->forceExit && tick->forceExit(tick->data) : false;
// }


static bool initWasm(tic_mem* tic, const char* code)
{
    // closeWasm(tic);
    tic_core* core = (tic_core*)tic;
    dbg("Initializing WASM3 runtime %p\n", core);

    IM3Environment env = m3_NewEnvironment ();
    if(!env)
    {
        core->data->error(core->data->data, "Unable to init WASM env");
        return false;
    }
    IM3Runtime runtime = m3_NewRuntime (env, WASM_STACK_SIZE, core);
    if(!runtime)
    {
        core->data->error(core->data->data, "Unable to init WASM runtime");
        return false;
    }

    runtime->memory.maxPages = TIC_WASM_PAGE_COUNT;
    ResizeMemory(runtime, TIC_WASM_PAGE_COUNT);

    u8* low_ram =  (u8*)core->memory.ram;
    u8* wasm_ram = m3_GetMemory(runtime, NULL, 0);
    memcpy(wasm_ram, low_ram, TIC_RAM_SIZE);
    core->memory.ram = (tic_ram*)wasm_ram;

    core->currentVM = runtime;

    // TODO: if compiling from WAT is an option where should this
    // code go?

    // long unsigned int srcSize = strlen(code);
    // long unsigned int fsize;
    // char* error;
    // void* wasmcode = wat2wasm(code ,srcSize, &fsize, &error);
    // if (!wasmcode){
    //     core->data->error(core->data->data, error);
    // free(error);
    //  return false;
    // }

    void* wasmcode = tic->cart.binary.data;
    // TODO: will this blow up or have bad effects if we are zero-padded?
    // if so we'll need to find a way to pass in size here
    // int fsize = TIC_BINARY_SIZE;
    int fsize = tic->cart.binary.size;

    IM3Module module;
    M3Result result = m3_ParseModule (runtime->environment, &module, wasmcode, fsize);

    if (result){
        core->data->error(core->data->data, result);
        return false;
    }

    result = m3_LoadModule (runtime, module);
    if (result){
        core->data->error(core->data->data, result);
        return false;
    }

    result = linkTicAPI(runtime->modules);
    if (result)
    {
        core->data->error(core->data->data, result);
        return false;
    }

    m3_FindFunction (&BDR_function, runtime, BDR_FN);
    m3_FindFunction (&SCN_function, runtime, SCN_FN);
    m3_FindFunction (&BOOT_function, runtime, BOOT_FN);
    m3_FindFunction (&MENU_function, runtime, MENU_FN);
    result = m3_FindFunction (&TIC_function, runtime, TIC_FN);

    if (result)
    {
        core->data->error(core->data->data, "Error: WASM must export a TIC function.");
        return false;
    }

    return true;
}

static void callWasmTick(tic_mem* tic)
{
    // ForceExitCounter = 0;

    tic_core* core = (tic_core*)tic;

    IM3Runtime runtime = core->currentVM;

    if(!runtime) { return; }

    M3Result res = m3_CallV(TIC_function);
    if(res)
    {
        core->data->error(core->data->data, res);
    }
}

static void callWasmBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    IM3Runtime runtime = core->currentVM;

    if(!runtime) { return; }
    if (BOOT_function == NULL) { return; }

    M3Result res = m3_CallV(BOOT_function);
    if(res)
    {
        core->data->error(core->data->data, res);
    }
}

static void callWasmIntFunc(tic_mem* tic, IM3Function func, s32 value, void* data)
{
    tic_core* core = (tic_core*)tic;

    IM3Runtime runtime = core->currentVM;

    if(!runtime) { return; }
    if (func == NULL) { return; }

    M3Result res = m3_CallV(func, value);
    if(res)
    {
        core->data->error(core->data->data, res);
    }
}

static void callWasmScanline(tic_mem* tic, s32 row, void* data)
{
    callWasmIntFunc(tic, SCN_function, row, data);
}

static void callWasmBorder(tic_mem* tic, s32 row, void* data)
{
    callWasmIntFunc(tic, BDR_function, row, data);
}

static void callWasmMenu(tic_mem* tic, s32 index, void* data)
{
    callWasmIntFunc(tic, MENU_function, index, data);
}

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getWasmOutline(const char* code, s32* size)
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
        static const char FuncString[] = "function ";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            ptr += sizeof FuncString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(isalnum_(c));
                else if(c == '(')
                {
                    end = ptr;
                    break;
                }
                else break;

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

void evalWasm(tic_mem* tic, const char* code) {
    printf("TODO: Wasm eval not yet implemented\n.");
}

static const u8 DemoRom[] =
{
    #include "../build/assets/wasmdemo.tic.dat"
};

static const u8 MarkRom[] =
{
    #include "../build/assets/wasmmark.tic.dat"
};

const tic_script EXPORT_SCRIPT(Wasm) =
{
    .id                 = 17,
    .name               = "wasm",
    .fileExtension      = ".wasmp",
    .projectComment     = "--",
    {
      .init               = initWasm,
      .close              = closeWasm,
      .tick               = callWasmTick,
      .boot               = callWasmBoot,

      .callback           =
      {
        .scanline       = callWasmScanline,
        .border         = callWasmBorder,
        .menu           = callWasmMenu,
      },
    },

    .getOutline         = getWasmOutline,
    .eval               = evalWasm,

    .blockCommentStart  = "(;",
    .blockCommentEnd    = ";)",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = "**",
    .blockStringEnd     = "**",
    .singleComment      = "--",

    .keywords           = NULL,
    .keywordsCount      = 0,

    .demo = {DemoRom, sizeof DemoRom},
    .mark = {MarkRom, sizeof MarkRom, "wasmmark.tic"},
};
