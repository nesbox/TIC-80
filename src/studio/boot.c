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
#include "embed.h"
#include "cart.h"
#include "fs.h"

void boot_init(Boot* boot, Studio* studio, const char* cart)
{
    tic_mem* tic = getMemory(studio);

#if defined(__EMSCRIPTEN__)

    // The player handed the path in rather than an image carrying the cart.
    if(cart)
    {
        s32 size = 0;
        void* data = fs_read(cart, &size);

        if(data) SCOPE(free(data))
            {
                tic_cart_load(&tic->cart, data, size);
                tic_api_reset(tic);
                boot->cart = true;
                studioRomLoaded(studio);
            }
    }

#else

    TIC_UNUSED(cart);

    {
        const char* appPath = fs_apppath();

        s32 appSize = 0;
        u8* app = fs_read(appPath, &appSize);

        if(app) SCOPE(free(app))
            {
                s32 dataSize = 0;
                u8* data = embed_find_cart(app, appSize, &dataSize);

                if(data) SCOPE(free(data))
                    {
                        tic_cart_load(&tic->cart, data, dataSize);
                        tic_api_reset(tic);
                        boot->cart = true;
                        studioRomLoaded(studio);
                    }
            }
    }

#endif
}
