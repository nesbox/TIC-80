/* What every screen draws and points with, on a machine of its own: the module
 * links with the core and nothing else, which is what building it here says. */
#include "studio/ui.h"
#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static tic_tiles Tiles;

// The cell rows at literal addresses: the cart packs four 1bpp cells to a
// sprite, a byte a row. Cell 5 is the second cell of sprite 1, cell 200 the
// third of sprite 194, cell 261 the second of sprite 5 in page 1 and cell 456
// the third of sprite 198 there. A wrong reader formula fails here; one
// copied into the test would not.
static const u8 Cell5[] = {1, 5, 9, 13, 17, 21, 25, 29};
static const u8 Cell200[] = {0, 4, 8, 12, 16, 20, 24, 28};
static const u8 Cell261[] = {1, 5, 9, 13, 17, 21, 25, 29};
static const u8 Cell456[] = {0, 4, 8, 12, 16, 20, 24, 28};

static void inkCell(tic_tile* sprite, const u8* rows, u8 bits)
{
    for(s32 row = 0; row < TIC_SPRITESIZE; row++)
        sprite->data[rows[row]] = bits;
}

static void assertIcon(tic_mem* tic, s32 x, s32 y, u8 color)
{
    for(s32 row = 0; row < TIC_SPRITESIZE; row++)
        for(s32 col = 0; col < TIC_SPRITESIZE; col++)
            assert(tic_tool_peek4(tic->ram->vram.screen.data, (y + row) * TIC80_WIDTH + x + col) == color);
}

static void testIcon(tic_mem* tic)
{
    enum
    {
        First = 5,
        Second = 200,
        X = 10,
        Y = 20,
        Gap = TIC_SPRITESIZE
    };

    inkCell(&Tiles.data[1], Cell5, 0xff);
    inkCell(&Tiles.data[194], Cell200, 0xff);

    ui_icon(tic, &Tiles, First, X, Y, tic_color_red);
    ui_icon(tic, &Tiles, Second, X + Gap, Y, tic_color_red);

    assertIcon(tic, X, Y, tic_color_red);
    assertIcon(tic, X + Gap, Y, tic_color_red);

    // A zero pixel leaves the screen alone.
    inkCell(&Tiles.data[1], Cell5, 0);
    inkCell(&Tiles.data[194], Cell200, 0);

    ui_icon(tic, &Tiles, First, X, Y, tic_color_white);
    ui_icon(tic, &Tiles, Second, X + Gap, Y, tic_color_white);

    assertIcon(tic, X, Y, tic_color_red);
    assertIcon(tic, X + Gap, Y, tic_color_red);

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

static void testCellPages(void)
{
    enum
    {
        Second = 5,
        Third = 200,
        PageOne = TIC_FONT_CHARS,
    };

    memset(&Tiles, 0, sizeof Tiles);
    inkCell(&Tiles.data[5], Cell261, 0xff);
    inkCell(&Tiles.data[198], Cell456, 0xff);

    for(s32 row = 0; row < TIC_SPRITESIZE; row++)
    {
        assert(ui_cell_row(&Tiles, PageOne + Second, row) == 0xff);
        assert(ui_cell_row(&Tiles, PageOne + Third, row) == 0xff);
        assert(ui_cell_row(&Tiles, Second, row) == 0x00);
        assert(ui_cell_row(&Tiles, Third, row) == 0x00);
    }

    puts("cell pages ok");
}

// The 512 config-cart cells land in the two face structs cell for cell, and
// each face's width/height cell is the pair the copy sets, not cart bytes.
static void testFontCells(void)
{
    tic_font font;

    for(s32 s = 0; s < TIC_BANK_SPRITES; s++)
        for(s32 b = 0; b < (s32)sizeof(tic_tile); b++)
            Tiles.data[s].data[b] = (u8)(s * 7 + b);

    ui_font_cells(&font, &Tiles);

    assert(font.regular.width == TIC_FONT_WIDTH && font.regular.height == TIC_FONT_HEIGHT);
    assert(font.alt.width == TIC_ALTFONT_WIDTH && font.alt.height == TIC_FONT_HEIGHT);

    for(s32 c = 0; c < TIC_FONT_CHARS - 1; c++)
        for(s32 row = 0; row < TIC_SPRITESIZE; row++)
        {
            assert(font.regular.data[c * BITS_IN_BYTE + row] == ui_cell_row(&Tiles, c, row));
            assert(font.alt.data[c * BITS_IN_BYTE + row] == ui_cell_row(&Tiles, TIC_FONT_CHARS + c, row));
        }

    puts("font cells ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testIcon(tic);
    testCursor(tic);
    testCellPages();
    testFontCells();

    tic_core_close(tic);
    puts("ui ok");
    return 0;
}
