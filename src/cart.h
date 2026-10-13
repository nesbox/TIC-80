// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "tic.h"

// How saving a cart went: the studio's cart layer answers with it, and the
// console — which reports what it prints — names it too.
typedef enum
{
    CART_SAVE_OK,
    CART_SAVE_ERROR,
    CART_SAVE_MISSING_NAME,
} CartSaveResult;

// A saved cart is its own bytes plus a 4-byte header a chunk: ten chunks a
// bank, five for the binary, nine for the code, one for the language. It is
// bigger than the struct, so a buffer holding one is this, not sizeof().
#define TIC_CART_SAVE_SIZE (sizeof(tic_cartridge) + (TIC_BANKS * 10 + 15) * sizeof(u32))

// Deflate can grow what it is given by a few bytes in a thousand.
#define TIC_CART_ZIP_SIZE (TIC_CART_SAVE_SIZE + TIC_CART_SAVE_SIZE / 1024 + 64)

void tic_cart_load(tic_cartridge* rom, const u8* buffer, s32 size);
s32 tic_cart_save(const tic_cartridge* rom, u8* buffer);
