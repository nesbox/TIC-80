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

void initStart(Start* start, Studio* studio)
{
    enum duration 
    {
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
}

void freeStart(Start* start)
{
    free(start);
}
