// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

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

#include <stdlib.h>

#include <3ds.h>
#include <citro3d.h>
#include <png.h>

#include "utils.h"

static int npot(int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bool ctr_load_png(C3D_Tex* tex, const char* name, texture_location loc)
{
    png_image img;
    u32 *data;

    memset(&img, 0, sizeof(png_image));
    img.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&img, name)) {
        return false;
    }
    
    img.format = PNG_FORMAT_ABGR;
    
    if (loc == TEXTURE_TARGET_VRAM) {
        C3D_TexInitVRAM(tex, npot(img.width), npot(img.height), GPU_RGBA8);
    } else {
        C3D_TexInit(tex, npot(img.width), npot(img.height), GPU_RGBA8);
    }
    data = linearAlloc(tex->width * tex->height * sizeof(u32));
    
    if (!png_image_finish_read(&img, NULL, data, tex->width * sizeof(u32), NULL)) {
        linearFree(data);
        C3D_TexDelete(tex);
        return false;
    }

    GSPGPU_FlushDataCache(data, tex->width * tex->height * sizeof(u32));

    C3D_SyncDisplayTransfer(data, GX_BUFFER_DIM(tex->width, tex->height),
        tex->data, GX_BUFFER_DIM(tex->width, tex->height),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GPU_RGBA8) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA8)
        | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    linearFree(data);
    png_image_free(&img);
    return true;
}