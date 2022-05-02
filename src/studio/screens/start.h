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

#define CART_SIG "TIC.CART"

typedef struct
{
    u8 sig[STRLEN(CART_SIG)];
    s32 appSize;
    s32 cartSize;
} EmbedHeader;

typedef struct Start Start;

typedef struct stage {
    void(*fn)(Start*);
    u8 ticks;
} Stage;

struct Start
{
    Studio* studio;
    tic_mem* tic;

    bool initialized;
    Stage stages[5];

    u32 stage;
    s32 ticks;
    bool play;

    char text[STUDIO_TEXT_BUFFER_SIZE];
    u8 color[STUDIO_TEXT_BUFFER_SIZE];

    bool embed;

    void (*tick)(Start*);
};

void initStart(Start* start, Studio* studio, const char* cart);
void freeStart(Start* start);
