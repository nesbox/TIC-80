/* The splash links without the studio: this file builds start.c with the core
 * and the module it plays through, sound.c, so a call to anything studio.c
 * provides fails to link here rather than appearing quietly in the app build.
 *
 * Target start-test in cmake/tests.cmake; `ctest -R start` runs it.
 */
#include "studio/screens/start.h"
#include <assert.h>
#include <stdio.h>

static tic_cartridge Cart;
static StudioConfig Cfg;

// The sample channel 0 is playing, or -1: what the chime's stages differ in.
static s32 channelSample(tic_mem* tic)
{
    return ((tic_core*)tic)->state.sfx.channels[0].index;
}

static Start* makeSplash(tic_mem* tic)
{
    Cfg.cart = &Cart;

    return start_create(&(StartDeps){.tic = tic, .config = &Cfg});
}

static void testBanner(void)
{
    char text[STUDIO_TEXT_BUFFER_SIZE];
    u8 color[STUDIO_TEXT_BUFFER_SIZE];

    start_banner(text, color);

    // The buffer is a screen, not a string: each row carries one line, and the
    // row the name and the version land on is the row the splash draws them on.
    assert(strstr(text + STUDIO_TEXT_BUFFER_WIDTH, TIC_NAME_FULL));
    assert(strstr(text + 2 * STUDIO_TEXT_BUFFER_WIDTH, TIC_VERSION));

    // Nothing is left behind in the rows it does not write to.
    assert(text[0] == '\0');
    assert(text[STUDIO_TEXT_BUFFER_SIZE - 1] == '\0');

    assert(color[0] == tic_color_black);
    assert(color[STUDIO_TEXT_BUFFER_SIZE - 1] == tic_color_dark_grey);

    puts("banner ok");
}

static void testIntro(tic_mem* tic)
{
    Start* start = makeSplash(tic);
    assert(start);

    s32 ticks = 0;

    while(!start_done(start) && ticks < 4 * TIC80_FRAMERATE)
    {
        start_tick(start);
        ticks++;

        // The chime is the cart's own first sample, and it plays on the frame
        // after the first second of pulsing.
        if(ticks == TIC80_FRAMERATE + 1)
            assert(channelSample(tic) == 1);
    }

    // Two seconds of pulsing and banner, and the frame that stops the chime.
    assert(start_done(start));
    assert(ticks == 2 * TIC80_FRAMERATE + 1);
    assert(channelSample(tic) < 0);

    start_free(start);
    puts("intro ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testBanner();
    testIntro(tic);

    tic_core_close(tic);
    puts("start ok");
    return 0;
}
