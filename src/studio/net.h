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

#include "tic80_types.h"

typedef struct tic_net tic_net;

typedef struct
{
    enum
    {
        net_get_progress,
        net_get_done,
        net_get_error,
    } type;

    union
    {
        struct
        {
            s32 size;
            s32 total;
        } progress;

        struct
        {
            s32 size;
            u8* data;
        } done;

        struct
        {
            s32 code;
        } error;
    };

    void* calldata;
    const char* url;

} net_get_data;

typedef void(*net_get_callback)(const net_get_data*);

tic_net* tic_net_create(const char* host);

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata);
void tic_net_close(tic_net* net);
void tic_net_start(tic_net *net);
void tic_net_end(tic_net *net);
