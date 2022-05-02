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
    u8* tile = (u8*)start->tic->ram->tiles.data;

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

static void chime(Start* start) 
{
    playSystemSfx(start->studio, 1);
}

static void stop_chime(Start* start) 
{
    sfx_stop(start->tic, 0);
}

static void header(Start* start)
{
    drawHeader(start);
}

static void start_console(Start* start)
{
    drawHeader(start);
    setStudioMode(start->studio, TIC_CONSOLE_MODE);
}

static void tick(Start* start)
{
    // stages that have a tick count of 0 run in zero time  
    // (typically this is only used to start/stop audio)
    while (start->stages[start->stage].ticks == 0) {
        start->stages[start->stage].fn(start);
        start->stage++;
    }

    tic_api_cls(start->tic, TIC_COLOR_BG);

    Stage *stage = &start->stages[start->stage];
    stage->fn(start);
    if (stage->ticks > 0) stage->ticks--;
    if (stage->ticks == 0) start->stage++;

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

void initStart(Start* start, Studio* studio, const char* cart)
{
    enum duration {
        immediate = 0,
        one_second = TIC80_FRAMERATE,
        forever = -1
    };

    *start = (Start)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .initialized = true,
        .tick = tick,
        .embed = false,
        .ticks = 0,
        .stage = 0,
        .stages = 
        {
            { reset, .ticks = one_second },
            { chime, .ticks = immediate },
            { header, .ticks = one_second },
            { stop_chime, .ticks = immediate },
            { start_console, .ticks = forever },
        }
    };

    static const char* Header[] = 
    {
        "",
        " " TIC_NAME_FULL,
        " version " TIC_VERSION,
        " " TIC_COPYRIGHT,
    };

    for(s32 i = 0; i < COUNT_OF(Header); i++)
        strcpy(&start->text[i * STUDIO_TEXT_BUFFER_WIDTH], Header[i]);

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
            tic_cart_load(&start->tic->cart, data, size);
            tic_api_reset(start->tic);
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
                                tic_cart_load(&start->tic->cart, data, dataSize);
                                tic_api_reset(start->tic);
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
    free(start);
}
