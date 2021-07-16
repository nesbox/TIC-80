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

#include <stdlib.h>
#include <string.h>

#include <tic80.h>
#include "api.h"
#include "tools.h"
#include "cart.h"

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

static u64 getFreq(void* data)
{
    return TIC80_FRAMERATE;
}

static u64 getCounter(void* data)
{
    tic80_local* tic80 = (tic80_local*)data;
    return tic80->tick_counter;
}

tic80* tic80_create(s32 samplerate)
{
    tic80_local* tic80 = malloc(sizeof(tic80_local));

    if(tic80)
    {
        memset(tic80, 0, sizeof(tic80_local));

        tic80->memory = tic_core_create(samplerate);
        tic80->tic.screen_format = tic80->memory->screen_format;

        return &tic80->tic;
    }

    return NULL;
}

TIC80_API void tic80_load(tic80* tic, void* cart, s32 size)
{
    tic80_local* tic80 = (tic80_local*)tic;

    tic80->tic.sound.count = tic80->memory->samples.size/sizeof(s16);
    tic80->tic.sound.samples = tic80->memory->samples.buffer;

    tic80->tic.screen = tic80->memory->screen;

    {
        tic80->tickData.error = onError;
        tic80->tickData.trace = onTrace;
        tic80->tickData.exit = onExit;
        tic80->tickData.data = tic80;

        tic80->tickData.start = 0;
        tic80->tickData.freq = getFreq;
        tic80->tickData.counter = getCounter;
        tic80->tick_counter = 0;
    }

    {
        tic_cart_load(&tic80->memory->cart, cart, size);
        tic_api_reset(tic80->memory);
    }
}

TIC80_API void tic80_tick(tic80* tic, const tic80_input* input)
{
    tic80_local* tic80 = (tic80_local*)tic;

    tic80->memory->screen_format = tic80->tic.screen_format;
    tic80->memory->ram.input = *input;
    
    tic_core_tick_start(tic80->memory);
    tic_core_tick(tic80->memory, &tic80->tickData);
    tic_core_tick_end(tic80->memory);

    tic_core_blit(tic80->memory, tic80->memory->screen_format);

    tic80->tick_counter++;
}

TIC80_API void tic80_delete(tic80* tic)
{
    tic80_local* tic80 = (tic80_local*)tic;

    tic_core_close(tic80->memory);

    free(tic80);
}
