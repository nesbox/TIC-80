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

typedef struct Code Code;

struct Code
{
    tic_mem* tic;

    char* src;

    struct
    {
        struct
        {
            char* position;
            char* selection;
            s32 column;
        };

        char* mouseDownPosition;
        s32 delay;
    } cursor;

    struct
    {
        s32 x;
        s32 y;

        tic_point start;

        bool active;

    } scroll;

    struct CodeState
    {
        u8 syntax:3;
        u8 bookmark:1;
        u8 temp:4;
    }* state;

    char statusLine[STUDIO_TEXT_BUFFER_WIDTH];
    char statusSize[STUDIO_TEXT_BUFFER_WIDTH];

    u32 tickCounter;

    struct
    {
        struct History* code;
        struct History* cursor;
        struct History* state;
    } history;

    enum
    {
        TEXT_RUN_CODE,
        TEXT_EDIT_MODE,
        TEXT_DRAG_CODE,
        TEXT_FIND_MODE,
        TEXT_GOTO_MODE,
        TEXT_OUTLINE_MODE,
    } mode;

    struct
    {
        char text[STUDIO_TEXT_BUFFER_WIDTH - sizeof "FIND:"];

        char* prevPos;
        char* prevSel;
    } popup;

    struct
    {
        s32 line;
    } jump;

    struct
    {
        tic_outline_item* items;

        s32 size;
        s32 index;
    } outline;

    const char* matchedDelim;
    bool altFont;
    bool shadowText;

    void(*tick)(Code*);
    void(*escape)(Code*);
    void(*event)(Code*, StudioEvent);
    void(*update)(Code*);
};

void initCode(Code*, tic_mem*, tic_code* src);
void freeCode(Code*);
