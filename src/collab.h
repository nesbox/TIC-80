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

#if defined(TIC_BUILD_WITH_COLLAB)
#define IF_COLLAB( ... ) __VA_ARGS__
#else
#define IF_COLLAB( ... )
#endif

#if defined(TIC_BUILD_WITH_COLLAB)

#include <tic80_types.h>

typedef struct tic_mem tic_mem;

typedef struct Collab Collab;

struct Collab* collab_create(s32 offset, s32 size, s32 count);
void collab_delete(Collab* collab);
void collab_diff(Collab *collab, tic_mem *tic);
void* collab_data(Collab *collab, tic_mem *tic, s32 index);
bool collab_isChanged(Collab *collab, s32 index);
void collab_setChanged(Collab* collab, s32 index, u8 value);
bool collab_anyChanged(Collab *collab);
void collab_put(Collab* collab, tic_mem* tic);
void collab_get(Collab* collab, tic_mem* tic);
void collab_putRange(Collab* collab, tic_mem* tic, s32 first, s32 count);
void collab_getRange(Collab* collab, tic_mem* tic, s32 first, s32 count);
void collab_putInitialData(tic_mem *tic);
void collab_startChangesStream(tic_mem *tic);
void collab_stopChangesStream();

#endif
