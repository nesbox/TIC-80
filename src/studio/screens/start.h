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

#include "studio/studio.h"

typedef struct Start Start;

// What the intro runs on: the machine it draws with, and the cart its chime
// comes from. No studio, and nothing to call back into.
typedef struct
{
    tic_mem*            tic;
    const StudioConfig* config;

} StartDeps;

Start* start_create(const StartDeps* deps);
void   start_tick(Start* start);
void   start_free(Start* start);

// The intro is over; where that leads is the host's to decide.
bool   start_done(const Start* start);

// The studio's name, version and copyright: what the splash draws, and what the
// console prints in builds that reach it without drawing the splash at all.
void   start_banner(char* text, u8* color);
