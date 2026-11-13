/* What every screen draws and points with, on a machine of its own: the module
 * links with the core and nothing else, which is what building it here says. */
#include "studio/ui.h"
#include "tools.h"
#include <assert.h>
#include <stdio.h>

static tic_tiles Tiles;

// The config cart packs four 1bpp cells to a sprite in page 1 order; this
// fills every row of cell `id` with `bits`.
static void fillCell(s32 id, u8 bits)
{
    enum
    {
        CellsPerSprite = 4
    };

    tic_tile* cell = &Tiles.data[(id / TIC_SPRITESHEET_COLS) * TIC_SPRITESHEET_COLS + (id % TIC_SPRITESHEET_COLS) / CellsPerSprite];

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        cell->data[y * CellsPerSprite + id % CellsPerSprite] = bits;
}

static void testIcon(tic_mem* tic)
{
    enum
    {
        Id = 5,
        X = 10,
        Y = 20
    };

    fillCell(Id, 0xff);
    ui_icon(tic, &Tiles, Id, X, Y, tic_color_red);

    for(s32 y = 0; y < TIC_SPRITESIZE; y++)
        for(s32 x = 0; x < TIC_SPRITESIZE; x++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (Y + y) * TIC80_WIDTH + X + x) == tic_color_red);

    // A zero pixel leaves the screen alone.
    fillCell(Id, 0);
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
