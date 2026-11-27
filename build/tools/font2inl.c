// MIT License

// Copyright (c) 2026 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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
#include "studio/ui.h"

static bool writeInl(const char* path, const u8* data, s32 size)
{
    FILE* file = fopen(path, "wb");
    if(!file)
        return false;

    for(s32 i = 0; i < size; i++)
        fprintf(file, "0x%02x, ", data[i]);

    fclose(file);
    return true;
}

int main(int argc, char** argv)
{
    int res = -1;

    if(argc == 4)
    {
        FILE* cart_file = fopen(argv[1], "rb");

        if(cart_file)
        {
            fseek(cart_file, 0, SEEK_END);
            s32 size = (s32)ftell(cart_file);
            fseek(cart_file, 0, SEEK_SET);

            unsigned char* buffer = (unsigned char*)malloc(size);

            if(buffer)
            {
                fread(buffer, size, 1, cart_file);
                fclose(cart_file);

                tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));

                if(cart)
                {
                    tic_cart_load(cart, buffer, size);

                    tic_font font;
                    ui_font_cells(&font, &cart->bank0.sprites);

                    if(writeInl(argv[2], font.regular.data, sizeof font.regular.data) &&
                       writeInl(argv[3], font.alt.data, sizeof font.alt.data))
                        res = 0;

                    free(cart);
                }

                free(buffer);
            }
        }
    }
    else
        printf("usage: font2inl <cart> <font.inl> <altfont.inl>\n");

    return res;
}
