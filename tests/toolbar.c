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

// A strip with two editors in it, driven by hand: the test is the only caller
// of toolbar_end, so the tab it clicks is the tab it declared.
static tic_cartridge Cart;
static StudioConfig Cfg;
static MouseState Mouse[3];
static char Tooltip[STUDIO_TEXT_BUFFER_WIDTH];
static EditorMode Requested;

static const EditorApp Apps[TIC_MODES_COUNT] =
{
    [TIC_CODE_MODE]   = {.name = "CODE EDITOR",   .tip = "CODE EDITOR [f1]",   .icon = tic_icon_code},
    [TIC_SPRITE_MODE] = {.name = "SPRITE EDITOR", .tip = "SPRITE EDITOR [f2]", .icon = tic_icon_sprite},
};

static Toolbar makeStrip(tic_mem* tic, EditorMode mode)
{
    Cfg.cart = &Cart;
    Requested = TIC_MODES_COUNT;

    return (Toolbar)
    {
        .tic      = tic,
        .config   = &Cfg,
        .mouse    = Mouse,
        .tooltip  = Tooltip,
        .apps     = Apps,
        .appCount = TIC_MODES_COUNT,
        .mode     = mode,
        .requested = TIC_MODES_COUNT,
    };
}

// A press and a release on the same point, as processMouseStates would leave
// them for the frame the button comes up.
static void clickAt(tic_mem* tic, s32 x, s32 y)
{
    memset(Mouse, 0, sizeof Mouse);

    Mouse[tic_mouse_left].start = (tic_point){x, y};
    Mouse[tic_mouse_left].end   = (tic_point){x, y};
    Mouse[tic_mouse_left].click = true;

    // The raw input is in the full window; tic_api_mouse subtracts the margins,
    // and that is the space the strip's rectangles live in.
    tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
}

static void testStrip(tic_mem* tic)
{
    enum {Button = 7, Centre = TIC80_WIDTH - Button / 2};

    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);
    ToolbarButton button = {.icon = tic_icon_copy, .tip = "COPY", .width = Button, .color = tic_color_light_grey, .enabled = true};

    // Hover shows the tooltip in the left rail, and the click is consumed.
    clickAt(tic, Centre, 3);
    toolbar_begin(&tb);
    assert(toolbar_button(&tb, &button));
    assert(!Mouse[tic_mouse_left].click);
    assert(strcmp(Tooltip, "COPY") == 0);

    // A click outside the widget is left alone.
    clickAt(tic, Centre - 2 * Button, 3);
    toolbar_begin(&tb);
    assert(!toolbar_button(&tb, &button));
    assert(Mouse[tic_mouse_left].click);

    // A disabled widget consumes its click but does not act on it.
    clickAt(tic, Centre, 3);
    button.enabled = false;
    toolbar_begin(&tb);
    assert(!toolbar_button(&tb, &button));
    assert(!Mouse[tic_mouse_left].click);

    puts("strip ok");
}

static void testTabs(tic_mem* tic)
{
    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);

    clickAt(tic, TOOLBAR_SIZE + 3, 3);
    toolbar_begin(&tb);
    toolbar_end(&tb);

    assert(tb.requested == TIC_SPRITE_MODE);

    // The tab of the mode that is on screen is the highlighted one.
    tb = makeStrip(tic, TIC_CODE_MODE);
    memset(Mouse, 0, sizeof Mouse);
    tic->ram->input.mouse.x = TIC80_WIDTH - 1 + TIC80_OFFSET_LEFT;
    toolbar_begin(&tb);
    toolbar_end(&tb);
    assert(tb.requested == TIC_MODES_COUNT);

    puts("tabs ok");
}

static void testSlider(tic_mem* tic)
{
    Toolbar tb = makeStrip(tic, TIC_SPRITE_MODE);

    s32 value = 1;

    clickAt(tic, TIC80_WIDTH - 20, 3);
    toolbar_begin(&tb);
    Mouse[tic_mouse_left].down = true;

    assert(toolbar_slider(&tb, 0, "ZOOM", &value, 1, 8));
    assert(value >= 1 && value <= 8);

    puts("slider ok");
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
    testStrip(tic);
    testTabs(tic);
    testSlider(tic);

    tic_core_close(tic);
    puts("toolbar ok");
    return 0;
}
