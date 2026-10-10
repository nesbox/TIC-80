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

#include "studio.h"

// The cart this launch came with: one embedded in the app image, or the one the
// player named. Not a user cart — it has no name of its own, and nothing saves it.
typedef struct
{
    bool cart;      // arrived with the launch, still to be played

} Boot;

// Reads what the launch brought, loads it and sets the flag. Called once, after
// the filesystem and the machine exist and before anything reads the flag.
void boot_init(Boot*, Studio*, const char* cart);

// The studio is opaque outside studio.c, so its boot state is handed out the
// way the config and the start screen are.
Boot* getBoot(Studio*);

#define CART_SIG "TIC.CART"

// The cart an app image carries: the app's bytes, then this header, then the
// zipped cart. `export game` writes it, the launch reads it back.
typedef struct
{
    u8 sig[STRLEN(CART_SIG)];
    s32 appSize;
    s32 cartSize;
} EmbedHeader;

// The image's cart, decompressed into a fresh allocation the caller frees, or
// NULL when there is none — the signature alone is not proof.
void* boot_findCart(const u8* app, s32 size, s32* cartSize);

// The format's other end: `app`'s bytes with `tic`'s cart appended, zipped. The
// caller owns the result.
void* boot_embedCart(tic_mem* tic, const u8* app, s32* size);
