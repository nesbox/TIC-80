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

#include "start.h"
#include "studio/fs.h"
#include "cart.h"

#if defined(__TIC_WINDOWS__)
#include <windows.h>
#else
#include <unistd.h>
#endif

static void reset(Start* start)
{
    u8* tile = (u8*)start->tic->ram.tiles.data;

    tic_api_cls(start->tic, tic_color_black);

    static const u8 Reset[] = {0x0, 0x2, 0x42, 0x00};
    u8 val = Reset[sizeof(Reset) * (start->ticks % TIC80_FRAMERATE) / TIC80_FRAMERATE];

    for(s32 i = 0; i < sizeof(tic_tile); i++) tile[i] = val;

    tic_api_map(start->tic, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT + (TIC80_HEIGHT % TIC_SPRITESIZE ? 1 : 0), 0, 0, 0, 0, 1, NULL, NULL);
}

static void drawHeader(Start* start)
{
    for(s32 i = 0; i < STUDIO_TEXT_BUFFER_SIZE; i++)
        tic_api_print(start->tic, (char[]){start->text[i], '\0'}, 
            (i % STUDIO_TEXT_BUFFER_WIDTH) * STUDIO_TEXT_WIDTH, 
            (i / STUDIO_TEXT_BUFFER_WIDTH) * STUDIO_TEXT_HEIGHT, 
            start->color[i], true, 1, false);
}

static void header(Start* start)
{
    if(!start->play)
    {
        playSystemSfx(1);

        start->play = true;
    }

    drawHeader(start);
}

static void end(Start* start)
{
    if(start->play)
    {   
        sfx_stop(start->tic, 0);

        start->play = false;
    }

    drawHeader(start);

    setStudioMode(TIC_CONSOLE_MODE);
}

static void tick(Start* start)
{
    if(!start->initialized)
    {
        start->phase = 1;
        start->ticks = 0;

        start->initialized = true;
    }

    tic_api_cls(start->tic, TIC_COLOR_BG);

    static void(*const steps[])(Start*) = {reset, header, end};

    steps[CLAMP(start->ticks / TIC80_FRAMERATE, 0, COUNT_OF(steps) - 1)](start);

    start->ticks++;
}

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

void initStart(Start* start, tic_mem* tic, const char* cart)
{
    *start = (Start)
    {
        .tic = tic,
        .initialized = false,
        .phase = 1,
        .ticks = 0,
        .tick = tick,
        .play = false,
        .embed = false,
    };

    start->text = calloc(1, STUDIO_TEXT_BUFFER_SIZE);
    start->color = calloc(1, STUDIO_TEXT_BUFFER_SIZE);

    static const char* Header[] = 
    {
        "",
        " " TIC_NAME_FULL,
        " version " TIC_VERSION,
        " " TIC_COPYRIGHT,
    };

    for(s32 i = 0; i < COUNT_OF(Header); i++)
        strcpy(start->text + i * STUDIO_TEXT_BUFFER_WIDTH, Header[i]);

    for(s32 i = 0; i < STUDIO_TEXT_BUFFER_SIZE; i++)
        start->color[i] = CLAMP(((i % STUDIO_TEXT_BUFFER_WIDTH) + (i / STUDIO_TEXT_BUFFER_WIDTH)) / 2, 
            tic_color_black, tic_color_dark_grey);

#if defined(__EMSCRIPTEN__)

    if (cart)
    {
        s32 size = 0;
        void* data = fs_read(cart, &size);

        if(data) SCOPE(free(data))
        {
            tic_cart_load(&tic->cart, data, size);
            start->embed = true;
        }
    }

#else

    {
        char appPath[TICNAME_MAX];
    
#   if defined(__TIC_WINDOWS__)
        {
            wchar_t wideAppPath[TICNAME_MAX];
            GetModuleFileNameW(NULL, wideAppPath, sizeof wideAppPath);
            WideCharToMultiByte(CP_UTF8, 0, wideAppPath, COUNT_OF(wideAppPath), appPath, COUNT_OF(appPath), 0, 0);
        }
#   elif defined(__TIC_LINUX__)
        s32 size = readlink("/proc/self/exe", appPath, sizeof appPath);
        appPath[size] = '\0';
#   elif defined(__TIC_MACOSX__)
        s32 size = sizeof appPath;
        _NSGetExecutablePath(appPath, &size);
#   endif
    
        s32 appSize = 0;
        u8* app = fs_read(appPath, &appSize);
    
        if(app) SCOPE(free(app))
        {
            s32 size = appSize;
            const u8* ptr = app;
    
            while(true)
            {
                const EmbedHeader* header = (const EmbedHeader*)_memmem(ptr, size, CART_SIG, STRLEN(CART_SIG));
    
                if(header)
                {
                    if(appSize == header->appSize + sizeof(EmbedHeader) + header->cartSize)
                    {
                        u8* data = calloc(1, sizeof(tic_cartridge));
    
                        if(data)
                        {
                            s32 dataSize = tic_tool_unzip(data, sizeof(tic_cartridge), app + header->appSize + sizeof(EmbedHeader), header->cartSize);
    
                            if(dataSize)
                            {
                                tic_cart_load(&tic->cart, data, dataSize);
                                start->embed = true;
                            }
                                
                            free(data);
                        }
    
                        break;
                    }
                    else
                    {
                        ptr = (const u8*)header + STRLEN(CART_SIG);
                        size = appSize - (s32)(ptr - app);
                    }
                }
                else break;
            }
        }
    }

#endif
}

void freeStart(Start* start)
{
    free(start->text);
    free(start->color);
    free(start);
}
