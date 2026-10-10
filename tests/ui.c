/* What every screen draws and points with, on a machine of its own: the module
 * links with the core and nothing else, which is what building it here says. */
#include "studio/ui.h"
#include <assert.h>
#include <stdio.h>

static tic_tiles Tiles;

static void fillTile(tic_tile* tile, u8 value)
{
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(tile->data, i, value);
}

static void testIcon(tic_mem* tic)
{
    enum {Id = 5, X = 10, Y = 20};

    fillTile(&Tiles.data[Id], 1);
    ui_icon(tic, &Tiles, Id, X, Y, tic_color_red);

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (Y + y) * TIC80_WIDTH + X + x) == tic_color_red);

    // A zero pixel leaves the screen alone.
    fillTile(&Tiles.data[Id], 0);
    ui_icon(tic, &Tiles, Id, X, Y, tic_color_white);

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (Y + y) * TIC80_WIDTH + X + x) == tic_color_red);

    puts("icon ok");
}

static void testCursor(tic_mem* tic)
{
    ui_cursor(tic, tic_cursor_hand);
    assert(tic->ram->vram.vars.cursor.sprite == tic_cursor_hand);

    ui_cursor(tic, tic_cursor_ibeam);
    assert(tic->ram->vram.vars.cursor.sprite == tic_cursor_ibeam);

    puts("cursor ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testIcon(tic);
    testCursor(tic);

    tic_core_close(tic);
    puts("ui ok");
    return 0;
}
