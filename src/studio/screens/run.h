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
#include "studio/fs.h"

typedef struct Run Run;

// What the runner gets once: the machine, the filesystem and the host's
// actions. A build without a console leaves the trace and the error empty; a
// player build reports the exit where an editors build reports the end.
typedef struct
{
    void* host;
    tic_mem* tic;
    tic_fs* fs;

    void (*ended)(void* host);
    void (*exited)(void* host);
    void (*error)(void* host, const char* info);
    void (*trace)(void* host, const char* text, u8 color);
    bool (*isActive)(void* host);
} RunDeps;

struct Run
{
    RunDeps deps;

    tic_mem* tic;
    struct tic_fs* fs;
    tic_tick_data tickData;

    bool exit;

    char saveid[TICNAME_MAX];
    tic_persistent pmem;

    void (*tick)(Run*);
};

void run_init(Run* run, const RunDeps* deps);
void run_free(Run* run);
