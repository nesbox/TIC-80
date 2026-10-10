/* The sound the studio plays, on a machine of its own: the module links with the
 * core and nothing else, which is what building it here says. */
#include "studio/sound.h"
#include <assert.h>
#include <stdio.h>

static tic_sfx Sfx;

static void testPlay(tic_mem* tic)
{
    enum {Id = 3};
    tic_sample* sample = &Sfx.samples.data[Id];

    sample->note = NoteStart;
    sample->octave = 4;

    tic->ram->sfxpos[0] = (tic_sfx_pos){0};
    sound_play(tic, &Sfx, Id);

    // The sample starts on channel 0 and moves the channel off its start.
    assert(memcmp(&tic->ram->sfxpos[0], &(tic_sfx_pos){0}, sizeof(tic_sfx_pos)) != 0);

    puts("play ok");
}

static void testStop(tic_mem* tic)
{
    tic->ram->sfxpos[0] = (tic_sfx_pos){0};
    sound_stop(tic, 0);

    // Silencing a channel resets it, so it is nowhere the sample left it.
    assert(memcmp(&tic->ram->sfxpos[0], &(tic_sfx_pos){0}, sizeof(tic_sfx_pos)) != 0);

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
