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

#include "run.h"
#include "tools.h"
#include "studio/paths.h"
#include "studio/system.h"
#include "ext/md5.h"
#include <time.h>

static void onTrace(void* data, const char* text, u8 color)
{
    Run* run = data;

    if(run->deps.trace)
        run->deps.trace(run->deps.host, text, color);
}

static void onError(void* data, const char* info)
{
    Run* run = data;

    // The console is the host's: it goes there and prints the line.
    if(run->deps.ended)
        run->deps.ended(run->deps.host);

    if(run->deps.error)
        run->deps.error(run->deps.host, info);
}

static void onExit(void* data)
{
    Run* run = (Run*)data;

    run->exit = true;
}

static const char* data2md5(const void* data, s32 length)
{
    const char* str = data;
    MD5_CTX c;

    static char out[33];

    MD5_Init(&c);

    while(length > 0)
    {
        MD5_Update(&c, str, length > 512 ? 512 : length);

        length -= 512;
        str += 512;
    }

    {
        u8 digest[16];
        MD5_Final(digest, &c);

        tic_tool_buf2str(digest, sizeof digest, out, false);
    }

    return out;
}

static void initPMemName(Run* run)
{
    tic_mem* tic = run->tic;

    const void* data = &tic->cart.bank0;
    s32 dataSize = sizeof(tic_bank);

    if(strlen(tic->saveid))
    {
        data = tic->saveid;
        dataSize = (s32)strlen(data);
    }

    const char* md5 = data2md5(data, dataSize);

    strcpy(run->saveid, TIC_LOCAL);
    strcat(run->saveid, md5);
}

static void tick(Run* run)
{
    if(!run->deps.isActive(run->deps.host))
        return;

    tic_mem* tic = run->tic;

    tic_core_tick(tic, &run->tickData);

    enum
    {
        Size = sizeof(tic_persistent)
    };

    if(memcmp(run->pmem.data, tic->ram->persistent.data, Size))
    {
        tic_fs_saveroot(run->fs, run->saveid, &tic->ram->persistent, Size, true);
        memcpy(run->pmem.data, tic->ram->persistent.data, Size);
    }

    if(run->exit)
    {
        if(run->deps.ended)
            run->deps.ended(run->deps.host);
        else if(run->deps.exited)
            run->deps.exited(run->deps.host);
    }
}

static u64 getFreq(void* data)
{
    (void)data;
    return tic_sys_freq_get();
}

static u64 getCounter(void* data)
{
    (void)data;
    return tic_sys_counter_get();
}

void run_init(Run* run, const RunDeps* deps)
{
    *run = (Run){
        .deps = *deps,
        .tic = deps->tic,
        .fs = deps->fs,
        .tick = tick,
        .exit = false,
        .tickData = (tic_tick_data){
            .error = onError,
            .trace = onTrace,
            .exit = onExit,
            .data = run,
            .counter = getCounter,
            .freq = getFreq},
    };

    {
        enum
        {
            Size = sizeof(tic_persistent)
        };
        memset(&run->tic->ram->persistent, 0, Size);

        initPMemName(run);

        s32 size = 0;
        void* data = tic_fs_loadroot(run->fs, run->saveid, &size);

        if(data) SCOPE(free(data))
            {
                memset(&run->tic->ram->persistent, 0, Size);
                memcpy(&run->tic->ram->persistent, data, MIN(size, Size));
            }

        memcpy(run->pmem.data, run->tic->ram->persistent.data, Size);
    }

    tic_sys_preseed();
}

void run_free(Run* run)
{
    free(run);
}