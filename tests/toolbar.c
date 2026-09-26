/* The strip links without the studio: this file builds toolbar.c with the core
 * and nothing else, so a call to anything studio.c provides fails to link here
 * rather than appearing quietly in the app build.
 * cc -Isrc -Iinclude -Ibuild -Ivendor/blip-buf tests/toolbar.c
 *    src/studio/toolbar.c src/core/core.c src/core/draw.c src/core/io.c
 *    src/core/sound.c src/tic.c src/tools.c src/tilesheet.c src/cart.c
 *    src/script.c src/vqtdata.c src/fftdata.c src/ext/fft.c src/ext/kiss_fft.c
 *    src/ext/kiss_fftr.c src/ext/vqt.c src/ext/vqt_kernel.c
 *    vendor/blip-buf/blip_buf.c -lm -o toolbar-test
 */
#include "studio/toolbar.h"
#include <assert.h>
#include <stdio.h>

static tic_tiles Tiles;
static tic_sfx Sfx;

// cart.c brings the png and zip readers with it; nothing here loads a cart.
png_buffer png_create(s32 size) { (void)size; return (png_buffer){0}; }
png_buffer png_decode(png_buffer cover) { (void)cover; return (png_buffer){0}; }
u32 tic_tool_unzip(void* dest, s32 bufSize, const void* source, s32 size)
{
    (void)dest; (void)bufSize; (void)source; (void)size;
    return 0;
}

static void fillTile(tic_tile* tile, u8 value)
{
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(tile->data, i, value);
}

static void testIcon(tic_mem* tic)
{
    enum {Id = 5, X = 10, Y = 20};

    fillTile(&Tiles.data[Id], 1);
    toolbar_icon(tic, &Tiles, Id, X, Y, tic_color_red);

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (Y + y) * TIC80_WIDTH + X + x) == tic_color_red);

    // A zero pixel leaves the screen alone.
    fillTile(&Tiles.data[Id], 0);
    toolbar_icon(tic, &Tiles, Id, X, Y, tic_color_white);

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (Y + y) * TIC80_WIDTH + X + x) == tic_color_red);

    puts("icon ok");
}

static void testCursor(tic_mem* tic)
{
    toolbar_cursor(tic, tic_cursor_hand);
    assert(tic->ram->vram.vars.cursor.sprite == tic_cursor_hand);

    toolbar_cursor(tic, tic_cursor_ibeam);
    assert(tic->ram->vram.vars.cursor.sprite == tic_cursor_ibeam);

    puts("cursor ok");
}

static void testClick(tic_mem* tic)
{
    enum {Id = 3};
    tic_sample* sample = &Sfx.samples.data[Id];

    sample->note = NoteStart;
    sample->octave = 4;

    tic->ram->sfxpos[0] = (tic_sfx_pos){0};
    toolbar_playClick(tic, &Sfx, Id);

    // The click starts the effect on channel 0 and moves it off its start.
    assert(memcmp(&tic->ram->sfxpos[0], &(tic_sfx_pos){0}, sizeof(tic_sfx_pos)) != 0);

    puts("click ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testIcon(tic);
    testCursor(tic);
    testClick(tic);

    tic_core_close(tic);
    puts("toolbar ok");
    return 0;
}
