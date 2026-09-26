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

static s32 ClipboardHits[5];

static void clipboardCut(void* app)   { (void)app; ClipboardHits[0]++; }
static void clipboardCopy(void* app)  { (void)app; ClipboardHits[1]++; }
static void clipboardPaste(void* app) { (void)app; ClipboardHits[2]++; }
static void clipboardUndo(void* app)  { (void)app; ClipboardHits[3]++; }
static void clipboardRedo(void* app)  { (void)app; ClipboardHits[4]++; }

static const ClipboardOps CodeClipboard = {clipboardCut, clipboardCopy, clipboardPaste, clipboardUndo, clipboardRedo};

static const EditorApp Apps[TIC_MODES_COUNT] =
{
    [TIC_CODE_MODE]   = {.name = "CODE EDITOR",   .tip = "CODE EDITOR [f1]",   .icon = tic_icon_code,   .clipboard = &CodeClipboard},
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
    toolbar_begin(&tb, true);
    assert(toolbar_button(&tb, &button));
    assert(!Mouse[tic_mouse_left].click);
    assert(strcmp(Tooltip, "COPY") == 0);

    // A click outside the widget is left alone.
    clickAt(tic, Centre - 2 * Button, 3);
    toolbar_begin(&tb, true);
    assert(!toolbar_button(&tb, &button));
    assert(Mouse[tic_mouse_left].click);

    // A disabled widget consumes its click but does not act on it.
    clickAt(tic, Centre, 3);
    button.enabled = false;
    toolbar_begin(&tb, true);
    assert(!toolbar_button(&tb, &button));
    assert(!Mouse[tic_mouse_left].click);

    puts("strip ok");
}

static void testTabs(tic_mem* tic)
{
    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);

    clickAt(tic, TOOLBAR_SIZE + 3, 3);
    toolbar_begin(&tb, true);
    toolbar_end(&tb);

    assert(tb.requested == TIC_SPRITE_MODE);

    // The tab of the mode that is on screen is the highlighted one.
    tb = makeStrip(tic, TIC_CODE_MODE);
    memset(Mouse, 0, sizeof Mouse);
    tic->ram->input.mouse.x = TIC80_WIDTH - 1 + TIC80_OFFSET_LEFT;
    toolbar_begin(&tb, true);
    toolbar_end(&tb);
    assert(tb.requested == TIC_MODES_COUNT);

    puts("tabs ok");
}

static u8 screenAt(tic_mem* tic, s32 x, s32 y)
{
    return tic_tool_peek4(tic->ram->vram.screen.data, y * TIC80_WIDTH + x);
}

// The clipboard row: the same five buttons in every editor, acting on the app
// the registry entry points at.
static void testClipboard(tic_mem* tic)
{
    enum {Size = TOOLBAR_SIZE, Gap = 17 * TIC_FONT_WIDTH};
    enum {Named = 2};   /* the registry below has two named entries */

    s32 app = 0;

    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);
    tb.app = &app;

    // Two named entries, so the row starts past the second tab and the gap the
    // mode's name leaves.
    s32 row = (Named + 1) * Size + Gap;

    for(s32 i = 0; i < COUNT_OF(ClipboardHits); i++)
    {
        ClipboardHits[i] = 0;

        clickAt(tic, row + i * Size + 3, 3);
        toolbar_begin(&tb, true);
        toolbar_end(&tb);

        assert(ClipboardHits[i] == 1);
        assert(!Mouse[tic_mouse_left].click);
    }

    // One more click each, all in the same frame's worth of state: the click is
    // consumed by the first button that tests it, so only that one fires.
    for(s32 i = 0; i < COUNT_OF(ClipboardHits); i++)
        ClipboardHits[i] = 0;

    clickAt(tic, row + 3, 3);
    toolbar_begin(&tb, true);
    toolbar_end(&tb);

    assert(ClipboardHits[0] == 1);
    assert(ClipboardHits[1] == 0 && ClipboardHits[2] == 0 && ClipboardHits[3] == 0 && ClipboardHits[4] == 0);

    // Hovering a clipboard button names it, and the rail carries that name in
    // the frame it was asked for — the pixels, not just the buffer, because a
    // print that happens after the clip is invisible to a strcmp.
    Tooltip[0] = '\0';
    clickAt(tic, row + Size + 3, 3);
    Mouse[tic_mouse_left].click = false;
    toolbar_begin(&tb, true);
    toolbar_end(&tb);

    assert(strcmp(Tooltip, "COPY [ctrl+c]") == 0);

    {
        enum {NameX = (Named + 1) * Size, NameRow = 1};
        bool inked = false;

        for(s32 x = NameX; x < NameX + 15 && !inked; x++)
            inked = screenAt(tic, x, NameRow) == tic_color_dark_grey;

        assert(inked);
    }

    // A pro build's bank row takes the name's space; it does not take the row.
    tb.hideName = true;
    ClipboardHits[1] = 0;

    clickAt(tic, row + Size + 3, 3);
    toolbar_begin(&tb, true);
    toolbar_end(&tb);

    assert(ClipboardHits[1] == 1);

    puts("clipboard ok");
}

static void testSlider(tic_mem* tic)
{
    enum {Width = 23, Height = 5, Stops = 4, Pitch = 6, Tick = 5};

    Toolbar tb = makeStrip(tic, TIC_SPRITE_MODE);
    s32 left = TIC80_WIDTH - Width;

    // The sprite editor's canvas zoom: four stops over 23 pixels. Each stop is
    // a hollow square with a white line through it; the thumb is a filled one
    // with a white centre.
    for(s32 stop = 0; stop < Stops; stop++)
    {
        s32 value = 0;

        clickAt(tic, left + stop * Pitch + 2, 3);
        toolbar_begin(&tb, true);
        Mouse[tic_mouse_left].down = true;

        assert(toolbar_slider(&tb, "ZOOM", &value, 0, Stops - 1));
        assert(value == stop);

        // Every stop is drawn: the corner of its square is black. The white
        // line runs under them at row 3 and does not reach row 1.
        for(s32 i = 0; i < Stops; i++)
        {
            assert(screenAt(tic, left + i * Pitch, 1 + tb.y) == tic_color_black);
            assert(screenAt(tic, left + i * Pitch + Tick - 1, 1 + tb.y) == tic_color_black);
        }

        // The thumb is the filled one, so its middle is white and a stop that
        // is not the thumb has the black bar there instead.
        for(s32 i = 0; i < Stops; i++)
        {
            u8 middle = screenAt(tic, left + i * Pitch + 2, 2 + tb.y);

            assert(middle == (i == value ? tic_color_white : tic_color_black));
        }
    }

    // A range wider than the control looks bad but renders: one pixel per stop,
    // and the click still lands on the stop it names. Reading the step size
    // from the control's height instead divides by zero here.
    {
        s32 value = 0;

        clickAt(tic, left + 10, 3);
        toolbar_begin(&tb, true);
        Mouse[tic_mouse_left].down = true;

        assert(toolbar_slider(&tb, "WIDE", &value, 0, 40));
        assert(value == 10);
    }

    // A single stop does not paint a square over the canvas below the strip:
    // the canvas is filled first, so anything the control draws out of bounds
    // shows as a change.
    {
        s32 value = 0;

        tic_api_cls(tic, tic_color_red);

        clickAt(tic, left + 10, 3);
        toolbar_begin(&tb, true);
        Mouse[tic_mouse_left].down = true;

        assert(toolbar_slider(&tb, "ONE", &value, 7, 7));
        assert(value == 7);

        for(s32 y = TOOLBAR_SIZE; y < TOOLBAR_SIZE + Height; y++)
            for(s32 x = left; x < TIC80_WIDTH; x++)
                assert(screenAt(tic, x, y) == tic_color_red);
    }

    puts("slider ok");
}

// The strip's y is recorded per button when a press begins, so the press point
// is read against the strip the user actually pressed. Zero today, and this is
// what keeps it honest when the strip can move.
static void testPressOffset(tic_mem* tic)
{
    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);
    ToolbarButton button = {.icon = tic_icon_copy, .tip = "COPY", .width = 7, .color = tic_color_light_grey, .enabled = true};

    // The strip is four pixels up when the press lands on the widget (strip row
    // 1) and back home when the release does (strip row 3). Reading the press in
    // the release's frame would put it at row -3, off the widget.
    tb.y = -4;

    clickAt(tic, TIC80_WIDTH - 3, -3);
    Mouse[tic_mouse_left].down = true;
    Mouse[tic_mouse_left].click = false;

    toolbar_step(&tb);
    assert(tb.press.y[tic_mouse_left] == -4);

    tb.y = 0;
    Mouse[tic_mouse_left].down = false;
    Mouse[tic_mouse_left].click = true;
    Mouse[tic_mouse_left].end = (tic_point){TIC80_WIDTH - 3, 3};

    toolbar_begin(&tb, true);
    assert(toolbar_button(&tb, &button));

    // The mirror case: the strip has slid away from under the pointer, so the
    // release is no longer on the widget even though the press was.
    tb = makeStrip(tic, TIC_CODE_MODE);

    clickAt(tic, TIC80_WIDTH - 3, 3);
    Mouse[tic_mouse_left].down = true;
    Mouse[tic_mouse_left].click = false;

    toolbar_step(&tb);

    tb.y = -4;
    Mouse[tic_mouse_left].down = false;
    Mouse[tic_mouse_left].click = true;

    toolbar_begin(&tb, true);
    assert(!toolbar_button(&tb, &button));

    puts("press offset ok");
}

static bool anyOf(tic_mem* tic, s32 x, s32 y0, s32 y1, u8 color)
{
    for(s32 y = y0; y <= y1; y++)
        if(screenAt(tic, x, y) == color)
            return true;

    return false;
}

// A widget draws on its own line inside the strip: the map editor's depth
// labels sit one row down, and the rail's flush packing leaves every gap the
// caller had to hold open itself.
static void testWidgetLine(tic_mem* tic)
{
    enum {Button = 7, Filled = tic_color_red};
    enum {X = TIC80_WIDTH - Button};

    Toolbar tb = makeStrip(tic, TIC_CODE_MODE);

    ToolbarButton down =
    {
        .icon = tic_icon_copy, .tip = "T", .width = Button,
        .color = tic_color_light_grey, .pressed = true,
        .pressedColor = Filled, .y = 1, .enabled = true,
    };

    clickAt(tic, TIC80_WIDTH - 3, 3);
    toolbar_begin(&tb, true);
    assert(toolbar_button(&tb, &down));

    assert(!anyOf(tic, X, 0, 0, Filled));
    assert(anyOf(tic, X, 1, TOOLBAR_SIZE - 1, Filled));

    // The same widget on the strip's first line fills from row 0.
    ToolbarButton flat = down;
    flat.y = 0;

    clickAt(tic, TIC80_WIDTH - 3, 3);
    toolbar_begin(&tb, true);
    assert(toolbar_button(&tb, &flat));

    assert(anyOf(tic, X, 0, 0, Filled));

    puts("widget line ok");
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
    testWidgetLine(tic);
    testClick(tic);
    testStrip(tic);
    testTabs(tic);
    testSlider(tic);
    testClipboard(tic);
    testPressOffset(tic);

    tic_core_close(tic);
    puts("toolbar ok");
    return 0;
}
