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
#if defined(TIC_BUILD_WITH_WASM)

#define dbg(...) printf(__VA_ARGS__)
//define dbg(...)



#include "tools.h"

#include <ctype.h>

#include "src/wat2wasm.h"
#include "wasm3.h"
#include "m3_api_defs.h"
#include "m3_exception.h"
#include "m3_env.h"

static const char TicCore[] = "_TIC80";

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
    return (tic_core*)ctx -> userPointer;
}

void deinitWasmRuntime( IM3Runtime runtime )
{
  dbg("Denitializing wasm runtime\n");
  if (runtime == NULL)
 {
   printf("WARNING deinitWasm of null");
 }
 else
 {
  IM3Environment env = runtime -> environment;
  printf("deiniting env %d\n", env);
  m3_FreeRuntime (runtime);
  m3_FreeEnvironment (env);
 }
}

m3ApiRawFunction(wasmtic_line)
{
    m3ApiGetArg      (int32_t, x0)
    m3ApiGetArg      (int32_t, y0)
    m3ApiGetArg      (int32_t, x1)
    m3ApiGetArg      (int32_t, y1)
    m3ApiGetArg      (int8_t, color)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_line(tic, x0,y0,x1, y1, color);

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
      tic_mem* tic = (tic_mem*)getWasmCore(runtime);
      tic_api_circ(tic, x,y,radius, color);
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
      tic_mem* tic = (tic_mem*)getWasmCore(runtime);
      tic_api_circb(tic, x,y,radius, color);
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

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_rect(tic, x,y,w, h, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_rectb)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
    m3ApiGetArg      (int8_t, color)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_rectb(tic, x,y,w, h, color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_cls)
{
    m3ApiGetArg      (int8_t, color)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_cls(tic ,color);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_btn)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArg      (int32_t, id)

    tic_core* core = getWasmCore(runtime);

    m3ApiReturn(core->memory.ram.input.gamepads.data & (1 << id))

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_peek)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    m3ApiReturn(tic_api_peek(tic, address));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_peek4)
{
    m3ApiReturnType  (int8_t)

    m3ApiGetArg      (int32_t, address)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    m3ApiReturn(tic_api_peek4(tic, address));

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke)
{

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_poke(tic, address, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_poke4)
{

    m3ApiGetArg      (int32_t, address)
    m3ApiGetArg      (int8_t, value)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_poke4(tic, address, value);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_pix)
{

    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int8_t, color)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_pix(tic, x,y,color, false);

    m3ApiSuccess();
}



m3ApiRawFunction(wasmtic_spr)
{
    m3ApiGetArg      (int32_t, index)
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int8_t, colorKey) // note: only support single color
    m3ApiGetArg      (int32_t, scale)
    m3ApiGetArg      (int32_t, flip)
    m3ApiGetArg      (int32_t, rotate)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
//    printf("Called SPR %d %d", colorKey, scale);
    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    tic_api_spr(tic, index, x, y, w, h, &colorKey, 1, scale, flip, rotate) ;

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_map)
{
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int32_t, w)
    m3ApiGetArg      (int32_t, h)
    m3ApiGetArg      (int32_t, sx)
    m3ApiGetArg      (int32_t, sy)
    m3ApiGetArg      (int8_t, colorKey) // note: only support single color
    m3ApiGetArg      (int32_t, scale)
    m3ApiGetArg      (int32_t, remap)

    tic_mem* tic = (tic_mem*)getWasmCore(runtime);
    tic_api_map(tic, x, y, w, h, sx, sy, &colorKey, 1, scale, NULL, NULL);

    m3ApiSuccess();
}

m3ApiRawFunction(wasmtic_print)
{
    m3ApiReturnType  (int32_t)

    m3ApiGetArgMem   (const char *, text)
    m3ApiGetArg      (int32_t, x)
    m3ApiGetArg      (int32_t, y)
    m3ApiGetArg      (int8_t, color)
    m3ApiGetArg      (int32_t, fixed)
    m3ApiGetArg      (int32_t, scale)
    m3ApiGetArg      (int32_t, alt)


    tic_mem* tic = (tic_mem*)getWasmCore(runtime);

    m3ApiReturn( tic_api_print(tic, text, x,y,color,fixed, scale, alt) );

    m3ApiSuccess();
}

M3Result linkTic80(IM3Module module)
{
  M3Result result = m3Err_none;
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "spr",     "v(iiiiiiiii)",  &wasmtic_spr)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "map",     "v(iiiiiiiii)",  &wasmtic_map)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "btn",     "i(i)",          &wasmtic_btn)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek",    "i(i)",          &wasmtic_peek)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "peek4",   "i(i)",          &wasmtic_peek4)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke",    "v(ii)",         &wasmtic_poke)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "poke4",   "v(ii)",         &wasmtic_poke4)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "pix",     "v(iii)",        &wasmtic_pix)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "line",    "v(iiiii)",      &wasmtic_line)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "circ",    "v(iiii)",       &wasmtic_rect)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "circb",   "v(iiii)",       &wasmtic_rect)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "rect",    "v(iiiii)",      &wasmtic_rect)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "rectb",   "v(iiiii)",      &wasmtic_rectb)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "cls",     "v(i)",          &wasmtic_cls)));
  _   (SuppressLookupFailure (m3_LinkRawFunction (module, "env", "print",   "i(*iiiiii)",    &wasmtic_print)));

_catch:
  return result;
}


static void closeWasm(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->currentVM)
    {
        // FIX duk_destroy_heap(core->js);
        deinitWasmRuntime(core->currentVM);
	core->currentVM = NULL;
    }
}

/*

static duk_ret_t duk_btn(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_uint(duk, core->memory.ram.input.gamepads.data);
    }
    else
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;
        duk_push_boolean(duk, core->memory.ram.input.gamepads.data & (1 << index));
    }

    return 1;
}

static duk_ret_t duk_btnp(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = (tic_mem*)core;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_uint(duk, tic_api_btnp(tic, -1, -1, -1));
    }
    else if(duk_is_null_or_undefined(duk, 1) && duk_is_null_or_undefined(duk, 2))
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;

        duk_push_boolean(duk, tic_api_btnp(tic, index, -1, -1));
    }
    else
    {
        s32 index = duk_to_int(duk, 0) & 0x1f;
        u32 hold = duk_to_int(duk, 1);
        u32 period = duk_to_int(duk, 2);

        duk_push_boolean(duk, tic_api_btnp(tic, index, hold, period));
    }

    return 1;
}

static s32 duk_key(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = &core->memory;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_boolean(duk, tic_api_key(tic, tic_key_unknown));
    }
    else
    {
        tic_key key = duk_to_int(duk, 0);

        if(key < tic_keys_count)
            duk_push_boolean(duk, tic_api_key(tic, key));
        else return duk_error(duk, DUK_ERR_ERROR, "unknown keyboard code\n");
    }

    return 1;
}

static s32 duk_keyp(duk_context* duk)
{
    tic_core* core = getDukCore(duk);
    tic_mem* tic = &core->memory;

    if (duk_is_null_or_undefined(duk, 0))
    {
        duk_push_boolean(duk, tic_api_keyp(tic, tic_key_unknown, -1, -1));
    }
    else
    {
        tic_key key = duk_to_int(duk, 0);

        if(key >= tic_keys_count)
        {
            return duk_error(duk, DUK_ERR_ERROR, "unknown keyboard code\n");
        }
        else
        {
            if(duk_is_null_or_undefined(duk, 1) && duk_is_null_or_undefined(duk, 2))
            {
                duk_push_boolean(duk, tic_api_keyp(tic, key, -1, -1));
            }
            else
            {
                u32 hold = duk_to_int(duk, 1);
                u32 period = duk_to_int(duk, 2);

                duk_push_boolean(duk, tic_api_keyp(tic, key, hold, period));
            }
        }
    }

    return 1;
}

static duk_ret_t duk_sfx(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    s32 index = duk_opt_int(duk, 0, -1);

    s32 note = -1;
    s32 octave = -1;
    s32 speed = SFX_DEF_SPEED;

    if (index < SFX_COUNT)
    {
        if(index >= 0)
        {
            tic_sample* effect = tic->ram.sfx.samples.data + index;

            note = effect->note;
            octave = effect->octave;
            speed = effect->speed;

            if(!duk_is_null_or_undefined(duk, 1))
            {
                if(duk_is_string(duk, 1))
                {
                    const char* noteStr = duk_to_string(duk, 1);

                    if(!tic_tool_parse_note(noteStr, &note, &octave))
                    {
                        return duk_error(duk, DUK_ERR_ERROR, "invalid note, should be like C#4\n");
                    }
                }
                else
                {
                    s32 id = duk_to_int(duk, 1);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
            }
        }
    }
    else
    {
        return duk_error(duk, DUK_ERR_ERROR, "unknown sfx index\n");
    }

    s32 duration = duk_opt_int(duk, 2, -1);
    s32 channel = duk_opt_int(duk, 3, 0);
    s32 volumes[TIC_STEREO_CHANNELS];

    if(duk_is_array(duk, 4))
    {
        for(s32 i = 0; i < COUNT_OF(volumes); i++)
        {
            duk_get_prop_index(duk, 4, i);
            if(!duk_is_null_or_undefined(duk, -1))
                volumes[i] = duk_to_int(duk, -1);
            duk_pop(duk);
        }
    }
    else volumes[0] = volumes[1] = duk_opt_int(duk, 4, MAX_VOLUME);

    speed = duk_opt_int(duk, 5, speed);

    if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
    {
        tic_api_sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xf, volumes[1] & 0xf, speed);
    }
    else return duk_error(duk, DUK_ERR_ERROR, "unknown channel\n");

    return 0;
}

typedef struct
{
    duk_context* duk;
    void* remap;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{

    RemapData* remap = (RemapData*)data;
    duk_context* duk = remap->duk;

    duk_push_heapptr(duk, remap->remap);
    duk_push_int(duk, result->index);
    duk_push_int(duk, x);
    duk_push_int(duk, y);
    duk_pcall(duk, 3);

    if(duk_is_array(duk, -1))
    {
        duk_get_prop_index(duk, -1, 0);
        result->index = duk_to_int(duk, -1);
        duk_pop(duk);

        duk_get_prop_index(duk, -1, 1);
        result->flip = duk_to_int(duk, -1);
        duk_pop(duk);

        duk_get_prop_index(duk, -1, 2);
        result->rotate = duk_to_int(duk, -1);
        duk_pop(duk);
    }
    else
    {
        result->index = duk_to_int(duk, -1);
    }

    duk_pop(duk);
}

static duk_ret_t duk_map(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);
    s32 w = duk_opt_int(duk, 2, TIC_MAP_SCREEN_WIDTH);
    s32 h = duk_opt_int(duk, 3, TIC_MAP_SCREEN_HEIGHT);
    s32 sx = duk_opt_int(duk, 4, 0);
    s32 sy = duk_opt_int(duk, 5, 0);
    s32 scale = duk_opt_int(duk, 7, 1);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    {
        if(!duk_is_null_or_undefined(duk, 6))
        {
            if(duk_is_array(duk, 6))
            {
                for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                {
                    duk_get_prop_index(duk, 6, i);
                    if(duk_is_null_or_undefined(duk, -1))
                    {
                        duk_pop(duk);
                        break;
                    }
                    else
                    {
                        colors[i] = duk_to_int(duk, -1);
                        count++;
                        duk_pop(duk);
                    }
                }
            }
            else
            {
                colors[0] = duk_to_int(duk, 6);
                count = 1;
            }
        }
    }

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    if (duk_is_null_or_undefined(duk, 8))
        tic_api_map(tic, x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);
    else
    {
        void* remap = duk_get_heapptr(duk, 8);

        RemapData data = {duk, remap};

        tic_api_map((tic_mem*)getDukCore(duk), x, y, w, h, sx, sy, colors, count, scale, remapCallback, &data);
    }

    return 0;
}

static duk_ret_t duk_mget(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u8 value = tic_api_mget(tic, x, y);
    duk_push_uint(duk, value);
    return 1;
}

static duk_ret_t duk_mset(duk_context* duk)
{
    s32 x = duk_opt_int(duk, 0, 0);
    s32 y = duk_opt_int(duk, 1, 0);
    u8 value = duk_opt_int(duk, 2, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_mset(tic, x, y, value);

    return 1;
}

static duk_ret_t duk_peek(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    duk_push_uint(duk, tic_api_peek(tic, address));
    return 1;
}

static duk_ret_t duk_poke(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_poke(tic, address, value);

    return 0;
}

static duk_ret_t duk_peek4(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    duk_push_uint(duk, tic_api_peek4(tic, address));
    return 1;
}

static duk_ret_t duk_poke4(duk_context* duk)
{
    s32 address = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_poke4(tic, address, value);

    return 0;
}

static duk_ret_t duk_memcpy(duk_context* duk)
{
    s32 dest = duk_to_int(duk, 0);
    s32 src = duk_to_int(duk, 1);
    s32 size = duk_to_int(duk, 2);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_memcpy(tic, dest, src, size);

    return 0;
}

static duk_ret_t duk_memset(duk_context* duk)
{
    s32 dest = duk_to_int(duk, 0);
    u8 value = duk_to_int(duk, 1);
    s32 size = duk_to_int(duk, 2);

    tic_mem* tic = (tic_mem*)getDukCore(duk);
    tic_api_memset(tic, dest, value, size);

    return 0;
}

static duk_ret_t duk_trace(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    const char* text = duk_opt_string(duk, 0, "");
    u8 color = duk_opt_int(duk, 1, TIC_DEFAULT_COLOR);

    tic_api_trace(tic, text, color);

    return 0;
}

static duk_ret_t duk_pmem(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);
    u32 index = duk_to_int(duk, 0);

    if(index < TIC_PERSISTENT_SIZE)
    {
        u32 val = tic_api_pmem(tic, index, 0, false);

        if(!duk_is_null_or_undefined(duk, 1))
        {
            tic_api_pmem(tic, index, duk_to_uint(duk, 1), true);
        }

        duk_push_int(duk, val);

        return 1;
    }
    else return duk_error(duk, DUK_ERR_ERROR, "invalid persistent tic index\n");

    return 0;
}

static duk_ret_t duk_time(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    duk_push_number(duk, tic_api_time(tic));

    return 1;
}

static duk_ret_t duk_tstamp(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    duk_push_number(duk, tic_api_tstamp(tic));

    return 1;
}

static duk_ret_t duk_exit(duk_context* duk)
{
    tic_api_exit((tic_mem*)getDukCore(duk));

    return 0;
}

static duk_ret_t duk_font(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    const char* text = duk_to_string(duk, 0);
    s32 x = duk_to_int(duk, 1);
    s32 y = duk_to_int(duk, 2);
    u8 chromakey = duk_to_int(duk, 3);
    s32 width =  duk_opt_int(duk, 4, TIC_SPRITESIZE);
    s32 height =  duk_opt_int(duk, 5, TIC_SPRITESIZE);
    bool fixed = duk_opt_boolean(duk, 6, false);
    s32 scale =  duk_opt_int(duk, 7, 1);
    bool alt = duk_opt_boolean(duk, 8, false);
    if(scale == 0)
    {
        duk_push_int(duk, 0);
        return 1;
    }

    s32 size = tic_api_font(tic, text, x, y, chromakey, width, height, fixed, scale, alt);

    duk_push_int(duk, size);

    return 1;
}

static duk_ret_t duk_mouse(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    const tic80_mouse* mouse = &core->memory.ram.input.mouse;

    duk_idx_t idx = duk_push_array(duk);

    {
        tic_point pos = tic_api_mouse((tic_mem*)core);

        duk_push_int(duk, pos.x);
        duk_put_prop_index(duk, idx, 0);
        duk_push_int(duk, pos.y);
        duk_put_prop_index(duk, idx, 1);
    }

    duk_push_boolean(duk, mouse->left);
    duk_put_prop_index(duk, idx, 2);
    duk_push_boolean(duk, mouse->middle);
    duk_put_prop_index(duk, idx, 3);
    duk_push_boolean(duk, mouse->right);
    duk_put_prop_index(duk, idx, 4);
    duk_push_int(duk, mouse->scrollx);
    duk_put_prop_index(duk, idx, 5);
    duk_push_int(duk, mouse->scrolly);
    duk_put_prop_index(duk, idx, 6);

    return 1;
}

static duk_ret_t duk_circ(duk_context* duk)
{
    s32 radius = duk_to_int(duk, 2);
    if(radius < 0) return 0;

    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 color = duk_to_int(duk, 3);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_circ(tic, x, y, radius, color);

    return 0;
}

static duk_ret_t duk_circb(duk_context* duk)
{
    s32 radius = duk_to_int(duk, 2);
    if(radius < 0) return 0;

    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 color = duk_to_int(duk, 3);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_circb(tic, x, y, radius, color);

    return 0;
}

static duk_ret_t duk_tri(duk_context* duk)
{
    s32 pt[6];

    for(s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = duk_to_int(duk, i);

    s32 color = duk_to_int(duk, 6);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

    return 0;
}

static duk_ret_t duk_textri(duk_context* duk)
{
    float pt[12];

    for (s32 i = 0; i < COUNT_OF(pt); i++)
        pt[i] = (float)duk_to_number(duk, i);
    tic_mem* tic = (tic_mem*)getDukCore(duk);
    bool use_map = duk_opt_boolean(duk, 12, false);

    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    {
        if(!duk_is_null_or_undefined(duk, 13))
        {
            if(duk_is_array(duk, 13))
            {
                for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                {
                    duk_get_prop_index(duk, 13, i);
                    if(duk_is_null_or_undefined(duk, -1))
                    {
                        duk_pop(duk);
                        break;
                    }
                    else
                    {
                        colors[i] = duk_to_int(duk, -1);
                        count++;
                        duk_pop(duk);
                    }
                }
            }
            else
            {
                colors[0] = duk_to_int(duk, 13);
                count = 1;
            }
        }
    }

    tic_api_textri(tic, pt[0], pt[1],   //  xy 1
                        pt[2], pt[3],   //  xy 2
                        pt[4], pt[5],   //  xy 3
                        pt[6], pt[7],   //  uv 1
                        pt[8], pt[9],   //  uv 2
                        pt[10], pt[11],//  uv 3
                        use_map, // usemap
                        colors, count);    //  chroma

    return 0;
}


static duk_ret_t duk_clip(duk_context* duk)
{
    s32 x = duk_to_int(duk, 0);
    s32 y = duk_to_int(duk, 1);
    s32 w = duk_opt_int(duk, 2, TIC80_WIDTH);
    s32 h = duk_opt_int(duk, 3, TIC80_HEIGHT);

    tic_mem* tic = (tic_mem*)getDukCore(duk);

    tic_api_clip(tic, x, y, w, h);

    return 0;
}

static duk_ret_t duk_music(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    s32 track = duk_opt_int(duk, 0, -1);
    tic_api_music(tic, -1, 0, 0, false, false);

    if(track >= 0)
    {
        s32 frame = duk_opt_int(duk, 1, -1);
        s32 row = duk_opt_int(duk, 2, -1);
        bool loop = duk_opt_boolean(duk, 3, true);
        bool sustain = duk_opt_boolean(duk, 4, false);

        tic_api_music(tic, track, frame, row, loop, sustain);
    }

    return 0;
}

static duk_ret_t duk_sync(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 mask = duk_opt_int(duk, 0, 0);
    s32 bank = duk_opt_int(duk, 1, 0);
    bool toCart = duk_opt_boolean(duk, 2, false);

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else
        return duk_error(duk, DUK_ERR_ERROR, "sync() error, invalid bank");

    return 0;
}

static duk_ret_t duk_reset(duk_context* duk)
{
    tic_core* core = getDukCore(duk);

    core->state.initialized = false;

    return 0;
}

static duk_ret_t duk_fget(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 index = duk_opt_int(duk, 0, 0);
    u32 flag = duk_opt_int(duk, 1, 0);

    bool value = tic_api_fget(tic, index, flag);

    duk_push_boolean(duk, value);

    return 1;
}

static duk_ret_t duk_fset(duk_context* duk)
{
    tic_mem* tic = (tic_mem*)getDukCore(duk);

    u32 index = duk_opt_int(duk, 0, 0);
    u32 flag = duk_opt_int(duk, 1, 0);
    bool value = duk_opt_boolean(duk, 2, false);

    tic_api_fset(tic, index, flag, value);

    return 0;
}
*/



static u64 ForceExitCounter = 0;

s32 wasm_timeout_check(void* udata)
{
    tic_core* core = (tic_core*)udata;
    tic_tick_data* tick = core->data;

    return ForceExitCounter++ > 1000 ? tick->forceExit && tick->forceExit(tick->data) : false;
}


static bool initWasm(tic_mem* tic, const char* code)
{
 // TODO for errors: core->data->error(core->data->data, lua_tostring(lua, -1));
    closeWasm(tic);
  tic_core* core = (tic_core*)tic;
  dbg("Initializing wasm runtime %d\n", core);

    IM3Environment env = m3_NewEnvironment ();
    if(!env)
    {
        core->data->error(core->data->data, "Unable to init env");
        return false;
    }
    IM3Runtime ctx = m3_NewRuntime (env, WASM_STACK_SIZE, core);
    if(!ctx)
    {
        core->data->error(core->data->data, "Unable to init runtime");
        return false;
    }

    ( (tic_core*)tic)->wasm = ctx;

    long unsigned int srcSize = strlen(code);
    long unsigned int fsize;
    char* error;
    void* wasmcode = wat2wasm(code ,srcSize, &fsize, &error);
    if (!wasmcode){
        core->data->error(core->data->data, error);
	free(error);
	 return false;
    }

    IM3Module module;
    M3Result result = m3_ParseModule (ctx->environment, &module, wasmcode, fsize);
    free(wasmcode);
    if (result){
        core->data->error(core->data->data, result);
	return false;
    }

    result = m3_LoadModule (ctx, module);
    if (result){
        core->data->error(core->data->data, result);
	return false;
    }

    result = linkTic80(ctx->modules);
    if (result)
    {
        core->data->error(core->data->data, result);
        return false;
    }

    return true;

}

static void callWasmTick(tic_mem* tic)
{
    ForceExitCounter = 0;

    tic_core* core = (tic_core*)tic;

    IM3Runtime ctx = core->currentVM;

    if(ctx)
    {
	M3Result res;

        IM3Function func;
        res = m3_FindFunction (&func, ctx, TIC_FN);
        if (res)
        {
            core->data->error(core->data->data, res);
            return;
        }

        res = m3_CallWithArgs (func, 0, NULL);
	if(res)
	{
        	core->data->error(core->data->data, res);
	}
    }
}

static void callWasmScanline(tic_mem* tic, s32 row, void* data)
{
    ForceExitCounter = 0;

    tic_core* core = (tic_core*)tic;

    IM3Runtime ctx = core->currentVM;

    if(ctx)
    {
	M3Result res;

        IM3Function func;
        res = m3_FindFunction (&func, ctx, SCN_FN);
        if (res == m3Err_functionLookupFailed)
        {
            return;
        }
        if (res)
        {
            core->data->error(core->data->data, res);
            return;
        }

        static const char buf[100];
        //itoa(row, buf, 10);
        res = m3_CallWithArgs (func, 1, &buf);
	if(res)
	{
        	core->data->error(core->data->data, res);
	}
    }

}

static void callWasmOverline(tic_mem* tic, void* data)
{
    ForceExitCounter = 0;

    tic_core* core = (tic_core*)tic;

    IM3Runtime ctx = core->currentVM;

    if(ctx)
    {
	M3Result res;

        IM3Function func;
        res = m3_FindFunction (&func, ctx, OVR_FN);
        if (res == m3Err_functionLookupFailed)
        {
            return;
        }
        if (res)
        {
            core->data->error(core->data->data, res);
            return;
        }

        res = m3_CallWithArgs (func, 0, NULL);
	if(res)
	{
        	core->data->error(core->data->data, res);
	}
    }
}

static const char* const WasmKeywords [] =
{
    "module", "func", "param", "result", "export", "import", "memory", "data", "table", "elem",
    "global", "call", "end", "type"
};

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

static const tic_script_config WasmSyntaxConfig =
{
    .name               = "wasm",
    .fileExtension      = ".wasmp",
    .projectComment     = "--",
    .init               = initWasm,
    .close              = closeWasm,
    .tick               = callWasmTick,
    .scanline           = callWasmScanline,
    .overline           = callWasmOverline,

    .getOutline         = getWasmOutline,
    .eval               = evalWasm,

    .blockCommentStart  = "(;",
    .blockCommentEnd    = ";)",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = ";;",

    .keywords           = WasmKeywords,
    .keywordsCount      = COUNT_OF(WasmKeywords),
};

const tic_script_config* get_wasm_script_config()
{
    return &WasmSyntaxConfig;
}

#else

// ??
s32 wasm_timeout_check(void* udata){return 0;}

#endif /* defined(TIC_BUILD_WITH_WASM) */
