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
#include "studio/fs.h"

typedef enum
{
    CART_SAVE_OK,
    CART_SAVE_ERROR,
    CART_SAVE_MISSING_NAME,
} CartSaveResult;

typedef struct Console Console;
typedef struct CommandDesc CommandDesc;

struct Console
{
    struct Config* config;

    struct
    {
        tic_point pos;
        s32 delay;
    } cursor;

    struct
    {
        s32 pos;
        s32 start;

        bool active;
    } scroll;

    struct
    {
        const char* start;
        const char* end;
        bool active;
    } select;

    char* text;
    u8* color;

    struct
    {
        char* text;
        size_t pos;
    } input;

    Studio* studio;
    tic_mem* tic;

    struct tic_fs* fs;
    struct tic_net* net;

    struct
    {
        char name[TICNAME_MAX];
        char path[TICNAME_MAX];
    } rom;

    struct
    {
        s32 index;
        s32 size;
        char** items;
    } history;

    u32 tickCounter;

    bool active;
    StartArgs args;
    
    struct
    {
        s32 count;
        s32 current;
        char** items;
    } commands;

    CommandDesc* desc;

    void(*load)(Console*, const char* path);
    bool(*loadCart)(Console*, const char* path);
    void(*loadByHash)(Console*, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data);
    void(*updateProject)(Console*);
    void(*error)(Console*, const char*);
    void(*trace)(Console*, const char*, u8 color);
    void(*tick)(Console*);
    void(*done)(Console*);

    CartSaveResult(*save)(Console*);
};

void initConsole(Console*, Studio* studio, struct tic_fs* fs, struct tic_net* net, struct Config* config, StartArgs args);
void freeConsole(Console* console);
