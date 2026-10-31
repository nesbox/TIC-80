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

#pragma once

#include "api.h"
#include "studio/anim.h"
#include "studio/fs.h"
#include "studio/net.h"

typedef struct Surf Surf;

// The confirm the host asks: the screen hands its answer back through it.
typedef void (*SurfConfirm)(void* host, bool yes, void* data);

// What the browser gets once: the machine, its filesystem, net and config, and
// the host actions it asks for.
typedef struct
{
    void* host;
    tic_mem* tic;
    tic_fs* fs;
    tic_net* net;
    struct Config* config;

    void (*loadCart)(void* host, const char* name);
    void (*loadByHash)(void* host, const char* name, const char* hash,
                       const char* section, fs_done_callback done, void* data);
    void (*autoSave)(void* host);
    bool (*cartChanged)(void* host);
    void (*confirmLoadCart)(void* host, SurfConfirm callback, void* data);
    void (*run)(void* host);
    void (*exit)(void* host);
    bool (*isActive)(void* host);
} SurfDeps;

struct Surf
{
    SurfDeps deps;

    tic_mem* tic;
    struct tic_fs* fs;
    struct tic_net* net;
    struct Config* config;

    bool init;
    bool loading;
    s32 ticks;

    // Cover requests the site has not answered yet, so the browser cannot
    // hold every connection slot with them, see requestCover().
    s32 coversInFlight;

    struct
    {
        s32 pos;
        s32 target;
        struct SurfItem* items;
        s32 count;
    } menu;

    struct
    {
        struct
        {
            s32 topBarY;
            s32 bottomBarY;
            s32 menuX;
            s32 menuHeight;
            s32 coverFade;
            s32 pos;
        } val;

        Movie* movie;

        Movie idle;
        Movie show;
        Movie play;
        Movie move;

        struct
        {
            Movie show;
            Movie hide;
        } gotodir;

        struct
        {
            Movie show;
            Movie hide;
        } goback;

    } anim;

    void (*tick)(Surf* surf);
    void (*resume)(Surf* surf);
    void (*scanline)(tic_mem* tic, s32 row, void* data);
};

void surf_init(Surf* surf, const SurfDeps* deps);
void surf_free(Surf* surf);
