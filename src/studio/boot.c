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

#include "boot.h"
#include "cart.h"

static void* _memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen)
{
    const u8* p = haystack;
    size_t plen = hlen;

    if (!nlen) return NULL;

    s32 needle_first = *(u8*)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void*)p;

        p++;
        plen = hlen - (p - (const u8*)haystack);
    }

    return NULL;
}

bool boot_findCart(const u8* app, s32 size, void* cart, s32 capacity, s32* cartSize)
{
    const u8* ptr = app;
    s32 left = size;

    while(true)
    {
        const EmbedHeader* header = (const EmbedHeader*)_memmem(ptr, left, CART_SIG, STRLEN(CART_SIG));

        if(header)
        {
            // The image is exactly the app, the header and the cart: any other
            // match is a signature that merely occurs in the bytes.
            if(size == header->appSize + sizeof(EmbedHeader) + header->cartSize)
            {
                s32 done = tic_tool_unzip(cart, capacity, app + header->appSize + sizeof(EmbedHeader), header->cartSize);

                if(done)
                {
                    *cartSize = done;
                    return true;
                }

                return false;
            }

            ptr = (const u8*)header + STRLEN(CART_SIG);
            left = size - (s32)(ptr - app);
        }
        else break;
    }

    return false;
}

void* boot_embedCart(tic_mem* tic, const u8* app, s32* size)
{
    u8* data = NULL;
    void* cart = malloc(sizeof(tic_cartridge));

    SCOPE(free(cart))
    {
        s32 cartSize = tic_cart_save(&tic->cart, cart);

        s32 zipSize = sizeof(tic_cartridge);
        u8* zipData = (u8*)malloc(zipSize);

        SCOPE(free(zipData))
        {
            if((zipSize = tic_tool_zip(zipData, zipSize, cart, cartSize)))
            {
                s32 appSize = *size;

                EmbedHeader header =
                {
                    .appSize = appSize,
                    .cartSize = zipSize,
                };

                memcpy(header.sig, CART_SIG, STRLEN(CART_SIG));

                s32 finalSize = appSize + sizeof header + header.cartSize;
                data = malloc(finalSize);

                if (data)
                {
                    memcpy(data, app, appSize);
                    memcpy(data + appSize, &header, sizeof header);
                    memcpy(data + appSize + sizeof header, zipData, header.cartSize);

                    *size = finalSize;
                }
            }
        }
    }

    return data;
}
