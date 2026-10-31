/* The map editor links without the studio: this file builds the map and the
 * world with the keyboard, the mouse, the ui, the clipboard and the history
 * modules and a fake host that records the two asks.
 *
 * Target map-editor-test in cmake/tests.cmake; `ctest -R map-editor` runs it.
 */
#include "studio/editors/map/map.h"
#include "studio/editors/map/world.h"
#include "studio/layout.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tic_mem* Tic;
static Map Map_;
static World World_;
static MapDeps Deps;
static MapView View;
static MouseState Mouse[3];
static tic_palette Palette;
static tic_palette CartPalette;
static char* Clipboard;

static struct
{
    s32 openWorld;
    s32 openMap;

} Calls;

bool tic_sys_keyboard_text(char* text)
{
    (void)text;
    return false;
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

static void openWorld(void* host)
{
    (void)host;
    Calls.openWorld++;
}

static void openMap(void* host)
{
    (void)host;
    Calls.openMap++;
}

static void tickCore(void)
{
    tic_core_tick_start(Tic);
    tic_core_tick_io(Tic);
    tic_core_tick_end(Tic);
}

static void setup(void)
{
    memset(Tic->cart.banks[0].map.data, 0, sizeof(tic_map));
    memset(Tic->cart.banks[0].tiles.data, 0, sizeof(tic_tiles));
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    tickCore();

    memset(&Calls, 0, sizeof Calls);
    memset(Mouse, 0, sizeof Mouse);

    free(Clipboard);
    Clipboard = NULL;

    Palette = (tic_palette){0};
    CartPalette = (tic_palette){0};

    Deps = (MapDeps){
        .host = NULL,
        .tic = Tic,
        .mouse = Mouse,
        .openWorld = openWorld,
        .openMap = openMap,
    };

    View = (MapView){
        .editorTop = STUDIO_BAR_HEIGHT,
        .tiles = &Tic->cart.banks[0].tiles,
        .palette = &Palette,
        .cartPalette = &CartPalette,
    };

    initMap(&Map_, &Deps, &Tic->cart.banks[0].map);
    map_set_view(&Map_, &View);
}

static void frame(void)
{
    map_set_view(&Map_, &View);
    map_tick(&Map_);
    tickCore();
}

static void worldFrame(void)
{
    map_set_view(&Map_, &View);
    world_tick(&World_);
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

static void keyFrame(tic_key key)
{
    press(key);
    frame();
}

static void pointer(s32 x, s32 y)
{
    Tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    Tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
    Tic->ram->input.mouse.relative = false;
}

static void noMouse(void)
{
    memset(Mouse, 0, sizeof Mouse);
    Tic->ram->input.mouse.scrolly = 0;
}

static void leftDown(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.down = true, .start = {x, y}, .end = {x, y}};
}

static void leftClick(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.click = true, .start = {x, y}, .end = {x, y}};
}

static void noKeys(void)
{
    memset(&Tic->ram->input.keyboard, 0, sizeof Tic->ram->input.keyboard);
}

static u8 mapAt(s32 x, s32 y)
{
    return Map_.src->data[x + y * TIC_MAP_WIDTH];
}

static void testDrawUndoRedo(void)
{
    setup();

    Map_.sheet.rect = (tic_rect){1, 0, 1, 1};

    noKeys();
    pointer(8, 8);
    leftDown(8, 8);
    frame();

    // The first held frame places the brush; the next one paints the cell.
    assert(mapAt(1, 1) == 0);

    pointer(8, 8);
    leftDown(8, 8);
    frame();

    assert(mapAt(1, 1) == 1);

    noMouse();
    noKeys();
    frame();

    MapClipboard.undo(&Map_);
    assert(mapAt(1, 1) == 0);

    MapClipboard.redo(&Map_);
    assert(mapAt(1, 1) == 1);

    puts("draw, undo, redo ok");
}

static void testFill(void)
{
    setup();

    Map_.sheet.rect = (tic_rect){1, 0, 1, 1};

    noMouse();
    keyFrame(tic_key_4);
    assert(Map_.mode == MAP_FILL_MODE);

    press(tic_key_unknown);
    noMouse();
    pointer(8, 8);
    leftClick(8, 8);
    frame();

    assert(mapAt(0, 0) == 1);
    assert(mapAt(TIC_MAP_WIDTH - 1, TIC_MAP_HEIGHT - 1) == 1);

    puts("fill ok");
}

static void testSelectionClipboard(void)
{
    setup();

    Map_.src->data[0] = 1;
    Map_.src->data[1] = 2;
    Map_.src->data[TIC_MAP_WIDTH] = 3;
    Map_.src->data[TIC_MAP_WIDTH + 1] = 4;
    Map_.select.rect = (tic_rect){0, 0, 2, 2};

    MapClipboard.copy(&Map_);
    assert(Clipboard != NULL);

    memset(Map_.src->data, 0, sizeof(tic_map));

    MapClipboard.paste(&Map_);
    assert(Map_.paste != NULL);
    assert(Map_.mode == MAP_SELECT_MODE);

    noKeys();
    pointer(36, 36);
    leftClick(36, 36);
    frame();

    assert(mapAt(4, 4) == 1);
    assert(mapAt(5, 4) == 2);
    assert(mapAt(4, 5) == 3);
    assert(mapAt(5, 5) == 4);

    puts("selection clipboard ok");
}

static void testClipboardGesture(void)
{
    setup();

    Map_.src->data[0] = 1;
    Map_.src->data[1] = 2;
    Map_.src->data[TIC_MAP_WIDTH] = 3;
    Map_.src->data[TIC_MAP_WIDTH + 1] = 4;
    Map_.select.rect = (tic_rect){0, 0, 2, 2};

    noMouse();
    View.frame.clipboard = TIC_CLIPBOARD_COPY;
    // The host says copy while the keys say cut: only the view may decide.
    press(tic_key_x);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(Clipboard != NULL);
    assert(mapAt(0, 0) == 1);

    View.frame.clipboard = TIC_CLIPBOARD_CUT;
    Map_.select.rect = (tic_rect){0, 0, 2, 2};
    // The host says cut while the keys say copy.
    press(tic_key_c);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(mapAt(0, 0) == 0);
    assert(mapAt(1, 1) == 0);

    View.frame.clipboard = TIC_CLIPBOARD_PASTE;
    press(tic_key_v);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(Map_.paste != NULL);

    puts("clipboard gesture ok");
}

static void testPointerOwned(void)
{
    setup();

    Map_.sheet.rect = (tic_rect){1, 0, 1, 1};

    View.frame.pointerOwned = true;

    noKeys();
    pointer(8, 8);
    leftDown(8, 8);
    frame();

    assert(mapAt(1, 1) == 0);

    puts("pointer owned ok");
}

static void testAsks(void)
{
    setup();

    noMouse();
    keyFrame(tic_key_tab);
    assert(Calls.openWorld == 1);

    setup();

    noKeys();
    Tic->ram->input.mouse.scrolly = -1;
    frame();
    assert(Calls.openWorld == 1);

    puts("asks ok");
}

static void testWorld(void)
{
    setup();

    tic_tile* tile = &Tic->cart.banks[0].tiles.data[1];
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(tile->data, i, 5);

    Map_.src->data[0] = 1;
    Map_.src->data[1] = 1;

    initWorld(&World_, &Map_);
    map_set_view(&Map_, &View);
    worldRefresh(&World_);

    assert(tic_tool_peek4(World_.preview, 0) == 5);
    assert(tic_tool_peek4(World_.preview, 1) == 5);

    // A press moves the map's scroll; the click asks to leave the world.
    noKeys();
    pointer(120, 68);
    Mouse[tic_mouse_left] = (MouseState){.down = true, .click = true, .start = {120, 68}, .end = {120, 68}};
    worldFrame();

    assert(Map_.scroll.x == (120 - TIC_MAP_SCREEN_WIDTH / 2) * TIC_SPRITESIZE);
    assert(Map_.scroll.y == (68 - TIC_MAP_SCREEN_HEIGHT / 2) * TIC_SPRITESIZE);
    assert(Calls.openMap == 1);

    // Tab and a scroll down leave too.
    setup();
    initWorld(&World_, &Map_);
    map_set_view(&Map_, &View);
    worldRefresh(&World_);

    noMouse();
    press(tic_key_tab);
    worldFrame();
    assert(Calls.openMap == 1);

    setup();
    initWorld(&World_, &Map_);
    map_set_view(&Map_, &View);
    worldRefresh(&World_);

    noKeys();
    Tic->ram->input.mouse.scrolly = 1;
    worldFrame();
    assert(Calls.openMap == 1);

    // The scanline takes the editor bank's palette.
    Palette.data[0] = 7;
    world_scanline(Tic, 0, &World_);
    assert(Tic->ram->vram.palette.data[0] == 7);

    puts("world ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testDrawUndoRedo();
    testFill();
    testSelectionClipboard();
    testClipboardGesture();
    testPointerOwned();
    testAsks();
    testWorld();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
