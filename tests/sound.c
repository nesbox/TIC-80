/* The sound the studio plays, on a machine of its own: the module links with the
 * core and nothing else, which is what building it here says. */
#include "studio/sound.h"
#include <assert.h>
#include <stdio.h>

static tic_sfx Sfx;

// The sample channel 0 is playing, or -1: what playing and silencing differ in.
static s32 channelSample(tic_mem* tic)
{
    return ((tic_core*)tic)->state.sfx.channels[0].index;
}

static void testPlay(tic_mem* tic)
{
    enum {Id = 3};
    tic_sample* sample = &Sfx.samples.data[Id];

    sample->note = NoteStart;
    sample->octave = 4;

    sound_play(tic, &Sfx, Id);

    // The channel plays the sample it was handed.
    assert(channelSample(tic) == Id);

    puts("play ok");
}

static void testStop(tic_mem* tic)
{
    sound_stop(tic, 0);

    // Silenced: no sample left on the channel to play.
    assert(channelSample(tic) < 0);

    puts("stop ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testPlay(tic);
    testStop(tic);

    tic_core_close(tic);
    puts("sound ok");
    return 0;
}
