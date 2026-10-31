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

#include "defines.h"
#include "fs.h"
#include "cart.h"
#include "config.h"

typedef struct
{
    char name[TICNAME_MAX];
    char path[TICNAME_MAX];
} CartName;

// The cart service's inputs: the machine, the filesystem, the identity kept
// for the loaded cart, the config and the host's notifications.
typedef struct
{
    tic_mem* tic;
    tic_fs* fs;
    CartName* cart;
    Config* config;

    void* host;
    void (*loaded)(void* host);
    void (*saved)(void* host);
} RomDeps;

void rom_set_cart_name(const RomDeps* deps, const char* name, const char* path);

bool rom_load_cart(const RomDeps* deps, const char* path);
void rom_reload_cart(const RomDeps* deps);
void rom_load_by_hash(const RomDeps* deps, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data);
CartSaveResult rom_save_cart(const RomDeps* deps, const char* name);
CartSaveResult rom_autosave(const RomDeps* deps);
