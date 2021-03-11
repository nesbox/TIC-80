// MIT License

// Copyright (c) 2021 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include <stdio.h>
#include <stdlib.h>
#include "cart.h"
#include "tools.h"
#include "ext/gif.h"
#include "ext/png.h"

typedef struct
{
    s32 size;
    u8* data;
} FileBuffer;

static FileBuffer readFile(const char* path)
{
    FileBuffer buffer = {0};

    FILE* file = fopen(path, "rb");
    if(file)
    {

        fseek(file, 0, SEEK_END);
        buffer.size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if((buffer.data = malloc(buffer.size)) && fread(buffer.data, buffer.size, 1, file)) {}

        fclose(file);
    }

    return buffer;
}

static bool writeFile(const char* name, FileBuffer buffer)
{
    FILE* file = fopen(name, "wb");

    if(file)
    {
        fwrite(buffer.data, 1, buffer.size, file);
        fclose(file);

        return true;
    }

    return false;
}

s32 main(s32 argc, char** argv)
{
    if(argc >= 2)
    {
        FileBuffer buffer = readFile(argv[1]);

        if(buffer.data)
        {
            tic_cartridge* cart = malloc(sizeof(tic_cartridge));

            tic_cart_load(cart, buffer.data, buffer.size);
            free(buffer.data);

            // export cover.gif
            {
                FileBuffer buffer = {TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32), malloc(buffer.size)};

                u8* data = malloc(TIC80_WIDTH * TIC80_HEIGHT);
                for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                    data[i] = tic_tool_peek4(cart->bank0.screen.data, i);

                if(gif_write_data(buffer.data, &buffer.size, TIC80_WIDTH, TIC80_HEIGHT, data, (gif_color*)cart->bank0.palette.scn.colors, TIC_PALETTE_BPP))
                {
                    writeFile("cover.gif", buffer);
                    printf("cover.gif successfully exported\n");
                }
                else printf("cannot extract gif cover\n");

                free(data);
                free(buffer.data);
            }

            // export cover.png
            {
                png_img img = {TIC80_WIDTH, TIC80_HEIGHT, malloc(TIC80_WIDTH * TIC80_HEIGHT * sizeof(u32))};

                for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                {
                    const tic_rgb* c = &cart->bank0.palette.scn.colors[tic_tool_peek4(cart->bank0.screen.data, i)];
                    ((u32*)img.data)[i] = (0xff << 24) | (c->b << 16) | (c->g << 8) | (c->r << 0);
                }

                png_buffer png = png_write(img);
                writeFile("cover.png", (FileBuffer){png.size, png.data});
                printf("cover.png successfully exported\n");

                free(png.data);
                free(img.data);
            }

            free(cart);
        }
        else printf("cannot open cart file\n");
    }
    else printf("usage: expld <cart>\n");

    return 0;
}