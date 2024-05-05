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

#include "tic80.h"
#include "script.h"
#include "tools.h"
#include "cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(TIC_MODULE_EXT)
#include <dlfcn.h>
#endif

static void onTrace(void* data, const char* text, u8 color)
{
    tic80* tic = (tic80*)data;

    if(tic->callback.trace)
        tic->callback.trace(text, color);
}

static void onError(void* data, const char* info)
{
    tic80* tic = (tic80*)data;

    if(tic->callback.error)
        tic->callback.error(info);
}

static void onExit(void* data)
{
    tic80* tic = (tic80*)data;

    if(tic->callback.exit)
        tic->callback.exit();
}

tic80* tic80_create(s32 samplerate, tic80_pixel_color_format format)
{
    return &tic_core_create(samplerate, format)->product;
}

TIC80_API void tic80_load(tic80* tic, void* cart, s32 size)
{
    tic_mem* mem = (tic_mem*)tic;

    tic_cart_load(&mem->cart, cart, size);

    const tic_script* script = tic_get_script(mem);
    if(script)
    {
        tic_api_reset(mem);
    }

#if defined(TIC_MODULE_EXT)
    else
    {
        const char* tag = tic_tool_metatag(mem->cart.code.data, "script", NULL);
        char name[128];
        sprintf(name, "%s" TIC_MODULE_EXT, tag);

        void* module = dlopen(name, RTLD_NOW | RTLD_LOCAL);

        if(module)
        {
            const tic_script* config = dlsym(module, DEF2STR(SCRIPT_CONFIG));

            if(config)
            {
                tic_add_script(config);
                tic_api_reset(mem);
            }
            else
            {
                dlclose(module);
            }
        }
    }
#endif
}

TIC80_API void tic80_tick(tic80* tic, tic80_input input, CounterCallback counter, FreqCallback freq)
{
    tic_mem* mem = (tic_mem*)tic;

    mem->ram->input = input;

    tic_tick_data tickData = (tic_tick_data)
    {
        .error = onError,
        .trace = onTrace,
        .exit = onExit,
        .data = tic,
        .start = 0,
        .counter = counter,
        .freq = freq
    };

    tic_core_tick_start(mem);
    tic_core_tick(mem, &tickData);
    tic_core_tick_end(mem);
    tic_core_blit(mem);
}

TIC80_API void tic80_sound(tic80* tic)
{
    tic_mem* mem = (tic_mem*)tic;
    tic_core_synth_sound(mem);
}

TIC80_API void tic80_delete(tic80* tic)
{
    tic_mem* mem = (tic_mem*)tic;
    tic_core_close(mem);
}
