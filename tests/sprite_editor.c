/* The sprite editor links without the studio: this file builds the editor
 * with the animation, keyboard, mouse, ui, clipboard and history modules and a
 * fake host that records the tooltip.
 *
 * Target sprite-editor-test in cmake/tests.cmake; `ctest -R sprite-editor`
 * runs it.
 */
#include "studio/editors/sprite/sprite.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tic_mem* Tic;
static Sprite Sprite_;
static SpriteDeps Deps;
static SpriteView View;
static MouseState Mouse[3];
static char* Clipboard;

static char Tooltip[64];
static char SystemText;
static bool SystemTextOwned;

bool tic_sys_keyboard_text(char* text)
{
    if(!SystemTextOwned)
        return false;

    *text = SystemText;
    SystemText = '\0';
    SystemTextOwned = false;
    return true;
}

void tic_sys_clipboard_set(const char* text)
{
    free(Clipboard);
    Clipboard = strdup(text);
}

bool tic_sys_clipboard_has(void)
{
    return Clipboard != NULL;
}

char* tic_sys_clipboard_get(void)
{
    return Clipboard ? strdup(Clipboard) : NULL;
}

void tic_sys_clipboard_free(const char* text)
{
    free((void*)text);
}

static void tooltip(void* host, const char* text)
{
    (void)host;
    snprintf(Tooltip, sizeof Tooltip, "%s", text);
}

static void tickCore(void)
{
    tic_core_tick_start(Tic);
    tic_core_tick_io(Tic);
    tic_core_tick_end(Tic);
}

static void setup(void)
{
    memset(&Tic->cart.banks[0].tiles, 0, sizeof(tic_tiles));
    memset(&Tic->cart.banks[0].palette, 0, sizeof(tic_palettes));
    memset(&Tic->cart.banks[0].flags, 0, sizeof(tic_flags));
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    tickCore();

    memset(Mouse, 0, sizeof Mouse);
    memset(Tooltip, 0, sizeof Tooltip);
    SystemTextOwned = false;

    free(Clipboard);
    Clipboard = NULL;

    Deps = (SpriteDeps){
        .host = NULL,
        .tic = Tic,
        .mouse = Mouse,
        .tooltip = tooltip,
    };

    View = (SpriteView){
        .cartTiles = &Tic->cart.banks[0].tiles,
        .cartPalette = &Tic->cart.banks[0].palette.vbank0,
        .vbank0 = &Tic->cart.banks[0].palette.vbank0,
        .vbank1 = &Tic->cart.banks[0].palette.vbank1,
        .flags = &Tic->cart.banks[0].flags,
    };

    initSprite(&Sprite_, &Deps, &Tic->cart.banks[0].tiles);
    sprite_set_view(&Sprite_, &View);
}

static void frame(void)
{
    sprite_set_view(&Sprite_, &View);
    sprite_tick(&Sprite_);
    tickCore();
}

static void press(tic_key key)
{
    memset(&Tic->ram->input.keyboard, 0, sizeof Tic->ram->input.keyboard);
    Tic->ram->input.keyboard.keys[0] = key;
}

static void alsoDown(tic_key key)
{
    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        if(!Tic->ram->input.keyboard.keys[i])
        {
            Tic->ram->input.keyboard.keys[i] = key;
            return;
        }
}

static void pointer(s32 x, s32 y)
{
    Tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    Tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
    Tic->ram->input.mouse.relative = false;
}

static void hold(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.down = true, .start = {x, y}, .end = {x, y}};
}

static void click(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.click = true, .start = {x, y}, .end = {x, y}};
}

static void noMouse(void)
{
    memset(Mouse, 0, sizeof Mouse);
    Tic->ram->input.mouse.scrolly = 0;
}

static void typeText(char c)
{
    SystemText = c;
    SystemTextOwned = true;
}

static u8 tilePixel(s32 tile, s32 x, s32 y)
{
    return tic_tool_peek4(Tic->cart.banks[0].tiles.data[tile].data, x + y * TIC_SPRITESIZE);
}

static void testDrawUndo(void)
{
    setup();

    noMouse();
    pointer(28, 24);
    hold(28, 24);
    frame();

    assert(tilePixel(1, 0, 0) == 2);

    noMouse();
    frame();

    SpriteClipboard.undo(&Sprite_);
    assert(tilePixel(1, 0, 0) == 0);

    SpriteClipboard.redo(&Sprite_);
    assert(tilePixel(1, 0, 0) == 2);

    puts("draw undo ok");
}

static void testFill(void)
{
    setup();

    spriteSetMode(&Sprite_, SPRITE_FILL_MODE);

    noMouse();
    pointer(28, 24);
    click(28, 24);
    frame();

    assert(tilePixel(1, 0, 0) == 2);
    assert(tilePixel(1, 7, 7) == 2);

    puts("fill ok");
}

static void testTools(void)
{
    setup();

    // Paint pixel (0,0) through the editor so the history commits it.
    Sprite_.color = 5;
    noMouse();
    pointer(28, 24);
    hold(28, 24);
    frame();
    noMouse();
    frame();
    assert(tilePixel(1, 0, 0) == 5);

    spriteToolAction(&Sprite_, SPRITE_TOOL_FLIP_HORZ);
    assert(tilePixel(1, 7, 0) == 5);
    assert(tilePixel(1, 0, 0) == 0);

    SpriteClipboard.undo(&Sprite_);
    assert(tilePixel(1, 0, 0) == 5);

    puts("tools ok");
}

static void testPaletteHex(void)
{
    setup();

    Sprite_.palette.edit = true;
    Sprite_.palette.focus = 0;
    Sprite_.color = 0;

    noMouse();
    press(tic_key_unknown);
    typeText('F');
    frame();
    assert(View.vbank0->data[0] == 0xF0);

    View.frame.captured = true;
    press(tic_key_unknown);
    typeText('A');
    frame();
    assert(View.vbank0->data[0] == 0xF0);

    puts("palette hex ok");
}

static void testFlagsHex(void)
{
    setup();

    Sprite_.flags.edit = true;
    Sprite_.flags.pos = 0;
    View.flags->data[1] = 0xAB;

    noMouse();
    press(tic_key_unknown);
    typeText('0');
    frame();
    assert(View.flags->data[1] == 0x0B);

    View.frame.captured = true;
    press(tic_key_unknown);
    typeText('A');
    frame();
    assert(View.flags->data[1] == 0x0B);

    puts("flags hex ok");
}

static void testGesture(void)
{
    setup();

    tic_tool_poke4(Tic->cart.banks[0].tiles.data[1].data, 0, 5);
    View.flags->data[1] = 0xA;

    noMouse();

    // The host says copy while the keys say cut: only the view may decide.
    View.frame.clipboard = TIC_CLIPBOARD_COPY;
    press(tic_key_x);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(Clipboard != NULL);
    assert(tilePixel(1, 0, 0) == 5);

    // The host says cut while the keys say copy.
    View.frame.clipboard = TIC_CLIPBOARD_CUT;
    press(tic_key_c);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(tilePixel(1, 0, 0) == 0);
    assert(View.flags->data[1] == 0);

    View.frame.clipboard = TIC_CLIPBOARD_PASTE;
    press(tic_key_v);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(tilePixel(1, 0, 0) == 5);
    assert(View.flags->data[1] == 0xA);

    puts("gesture ok");
}

static void testPointerOwned(void)
{
    setup();

    View.frame.pointerOwned = true;

    noMouse();
    pointer(28, 24);
    hold(28, 24);
    frame();

    assert(tilePixel(1, 0, 0) == 0);

    puts("pointer owned ok");
}

static void testTooltip(void)
{
    setup();

    noMouse();
    pointer(28, 24);
    frame();
    assert(strcmp(Tooltip, "[x=00 y=00]") == 0);

    Tooltip[0] = '\0';
    View.frame.pointerOwned = true;
    pointer(28, 24);
    frame();
    assert(Tooltip[0] == '\0');

    puts("tooltip ok");
}

static void testScanline(void)
{
    setup();

    View.vbank0->data[0] = 9;
    View.vbank1->data[0] = 7;

    Sprite_.palette.vbank1 = true;
    sprite_scanline(Tic, 0, &Sprite_);
    assert(Tic->ram->vram.palette.data[0] == 7);

    Sprite_.palette.vbank1 = false;
    sprite_scanline(Tic, 0, &Sprite_);
    assert(Tic->ram->vram.palette.data[0] == 9);

    puts("scanline ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testDrawUndo();
    testFill();
    testTools();
    testPaletteHex();
    testFlagsHex();
    testGesture();
    testPointerOwned();
    testTooltip();
    testScanline();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
