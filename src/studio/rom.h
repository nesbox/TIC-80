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

#include "system.h"
#include "fs.h"

typedef enum
{
    CART_SAVE_OK,
    CART_SAVE_ERROR,
    CART_SAVE_MISSING_NAME,
} CartSaveResult;

typedef struct
{
    char name[TICNAME_MAX];
    char path[TICNAME_MAX];
} CartName;

CartName* studioCart(Studio* studio);

const char* getCartName(const char* name);
void studioSetCartName(Studio* studio, const char* name, const char* path);
void loadCartSection(Studio* studio, const tic_cartridge* cart, const char* section);

bool studioLoadCart(Studio* studio, const char* path);
void studioLoadByHash(Studio* studio, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data);
CartSaveResult studioSaveCart(Studio* studio, const char* name);
CartSaveResult studioAutoSave(Studio* studio);
