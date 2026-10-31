/* The sfx editor links without the studio: this file builds the editor with
 * the keyboard, the mouse, the ui, the clipboard and the sound modules and a
 * fake host that records the tooltip.
 *
 * Target sfx-editor-test in cmake/tests.cmake; `ctest -R sfx-editor` runs it.
 */
#include "studio/editors/sfx/sfx.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tic_mem* Tic;
static Sfx Sfx_;
static SfxDeps Deps;
static SfxView View;
static MouseState Mouse[3];
static char* Clipboard;

static char Tooltip[64];

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
    memset(&Tic->cart.banks[0].sfx, 0, sizeof(tic_sfx));
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    tickCore();

    memset(Mouse, 0, sizeof Mouse);
    memset(Tooltip, 0, sizeof Tooltip);

    free(Clipboard);
    Clipboard = NULL;

    Deps = (SfxDeps){
        .host = NULL,
        .tic = Tic,
        .mouse = Mouse,
        .tooltip = tooltip,
    };

    View = (SfxView){
        .cartTiles = &Tic->cart.banks[0].tiles,
    };

    initSfx(&Sfx_, &Deps, &Tic->cart.banks[0].sfx);
    sfx_set_view(&Sfx_, &View);
}

static void frame(void)
{
    sfx_set_view(&Sfx_, &View);
    sfx_tick(&Sfx_);
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

static void click(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.click = true, .start = {x, y}, .end = {x, y}};
}

static void hold(s32 x, s32 y)
{
    Mouse[tic_mouse_left] = (MouseState){.down = true, .start = {x, y}, .end = {x, y}};
}

static tic_sample* effect(void)
{
    return &Sfx_.src->samples.data[Sfx_.index];
}

static tic_waveform* wave(void)
{
    return &Sfx_.src->waveforms.items[effect()->data[0].wave];
}

static void testNote(void)
{
    setup();

    press(tic_key_z);
    frame();

    assert(effect()->note == 0);
    assert(Sfx_.play.active);

    puts("note ok");
}

static void testCanvasUndo(void)
{
    setup();

    // The bottom row of the volume LEDs is volume 15.
    pointer(115, 43);
    hold(115, 43);
    frame();
    assert(effect()->data[0].volume == MAX_VOLUME);

    memset(Mouse, 0, sizeof Mouse);
    frame();

    SfxClipboard.undo(&Sfx_);
    assert(effect()->data[0].volume == 0);

    SfxClipboard.redo(&Sfx_);
    assert(effect()->data[0].volume == MAX_VOLUME);

    puts("canvas undo ok");
}

static void testEffectClipboard(void)
{
    setup();

    effect()->data[0].volume = 7;

    SfxClipboard.copy(&Sfx_);
    assert(Clipboard != NULL);

    Sfx_.index = 1;
    SfxClipboard.paste(&Sfx_);
    assert(effect()->data[0].volume == 7);

    SfxClipboard.undo(&Sfx_);
    assert(effect()->data[0].volume == 0);

    SfxClipboard.redo(&Sfx_);
    assert(effect()->data[0].volume == 7);

    Sfx_.index = 0;
    SfxClipboard.cut(&Sfx_);
    assert(effect()->data[0].volume == 0);

    puts("effect clipboard ok");
}

static void testGesture(void)
{
    setup();

    effect()->data[0].volume = 7;

    // The host says copy while the keys say cut: only the view may decide.
    View.frame.clipboard = TIC_CLIPBOARD_COPY;
    press(tic_key_x);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(Clipboard != NULL);
    assert(effect()->data[0].volume == 7);

    // The host says cut while the keys say copy.
    View.frame.clipboard = TIC_CLIPBOARD_CUT;
    press(tic_key_c);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(effect()->data[0].volume == 0);

    View.frame.clipboard = TIC_CLIPBOARD_PASTE;
    press(tic_key_v);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(effect()->data[0].volume == 7);

    press(tic_key_z);
    alsoDown(tic_key_ctrl);
    frame();
    assert(effect()->data[0].volume == 0);

    press(tic_key_y);
    alsoDown(tic_key_ctrl);
    frame();
    assert(effect()->data[0].volume == 7);

    puts("gesture ok");
}

static void testWaves(void)
{
    setup();

    // The wave bar, top to bottom at x = 72: cut (84), copy (91),
    // paste (98), undo (105) and redo (112).
    wave()->data[0] = 5;

    pointer(75, 94);
    click(75, 94);
    frame();
    assert(Clipboard != NULL);

    wave()->data[0] = 9;

    pointer(75, 101);
    click(75, 101);
    frame();
    assert(wave()->data[0] == 5);

    pointer(75, 108);
    click(75, 108);
    frame();
    assert(wave()->data[0] == 0);

    pointer(75, 115);
    click(75, 115);
    frame();
    assert(wave()->data[0] == 5);

    // Cut copies and clears, unlike copy, so its reset pins the button.
    pointer(75, 87);
    click(75, 87);
    frame();
    assert(wave()->data[0] == 0);
    assert(Clipboard != NULL);

    pointer(75, 108);
    click(75, 108);
    frame();
    assert(wave()->data[0] == 5);

    pointer(75, 115);
    click(75, 115);
    frame();
    assert(wave()->data[0] == 0);

    puts("waves ok");
}

static void testTooltip(void)
{
    setup();

    // The loop start arrow of the volume canvas.
    pointer(91, 41);
    frame();
    assert(strcmp(Tooltip, "set loop start") == 0);

    Tooltip[0] = '\0';
    View.frame.pointerOwned = true;
    pointer(91, 41);
    frame();
    assert(Tooltip[0] == '\0');

    puts("tooltip ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testNote();
    testCanvasUndo();
    testEffectClipboard();
    testGesture();
    testWaves();
    testTooltip();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
