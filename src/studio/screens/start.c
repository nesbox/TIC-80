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
#include "studio/sound.h"

typedef struct
{
    void (*fn)(Start*);
    s32 ticks;

} Stage;

struct Start
{
    tic_mem* tic;
    const StudioConfig* config;

    Stage stages[4];
    s32 stage;
    s32 ticks;

    char text[STUDIO_TEXT_BUFFER_SIZE];
    u8 color[STUDIO_TEXT_BUFFER_SIZE];

    bool done;
};

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
    sound_play(start->tic, &start->config->cart->bank0.sfx, 1);
}

static void stop_chime(Start* start)
{
    sound_stop(start->tic, 0);
}

static void header(Start* start)
{
    drawHeader(start);
}

void start_banner(char* text, u8* color)
{
    static const char* Header[] =
    {
        "",
        " " TIC_NAME_FULL,
        " version " TIC_VERSION,
        " " TIC_COPYRIGHT,
    };

    memset(text, 0, STUDIO_TEXT_BUFFER_SIZE);

    for(s32 i = 0; i < COUNT_OF(Header); i++)
        strcpy(&text[i * STUDIO_TEXT_BUFFER_WIDTH], Header[i]);

    for(s32 i = 0; i < STUDIO_TEXT_BUFFER_SIZE; i++)
        color[i] = CLAMP(((i % STUDIO_TEXT_BUFFER_WIDTH) + (i / STUDIO_TEXT_BUFFER_WIDTH)) / 2,
            tic_color_black, tic_color_dark_grey);
}

Start* start_create(const StartDeps* deps)
{
    Start* start = calloc(1, sizeof(Start));

    if(start)
    {
        *start = (Start)
        {
            .tic = deps->tic,
            .config = deps->config,
            .stages =
            {
                { reset, .ticks = TIC80_FRAMERATE },
                { chime },
                { header, .ticks = TIC80_FRAMERATE },
                { stop_chime },
            },
        };

        start_banner(start->text, start->color);
    }

    return start;
}

void start_tick(Start* start)
{
    // A stage with no ticks runs in zero time — the two that start and stop the
    // chime — and the intro is over when there are none left.
    while(start->stage < COUNT_OF(start->stages) && start->stages[start->stage].ticks == 0)
    {
        start->stages[start->stage].fn(start);
        start->stage++;
    }

    if(start->stage >= COUNT_OF(start->stages))
    {
        start->done = true;
        return;
    }

    tic_api_cls(start->tic, TIC_COLOR_BG);

    Stage* stage = &start->stages[start->stage];
    stage->fn(start);

    if(stage->ticks > 0 && --stage->ticks == 0)
        start->stage++;

    start->ticks++;
}

bool start_done(const Start* start)
{
    return start->done;
}

void start_free(Start* start)
{
    free(start);
}
