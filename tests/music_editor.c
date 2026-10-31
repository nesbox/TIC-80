/* The music editor links without the studio: this file builds the editor with
 * the keyboard, mouse, ui, clipboard, sound and history modules and a fake
 * host that records the tooltip.
 *
 * Target music-editor-test in cmake/tests.cmake; `ctest -R music-editor` runs
 * it.
 */
#include "studio/editors/music/music.h"
#include "tools.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tic_mem* Tic;
static Music Music_;
static MusicDeps Deps;
static MusicView View;
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

static void setup(bool tracker)
{
    memset(&Tic->cart.banks[0].music, 0, sizeof(tic_music));
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    tickCore();

    memset(Mouse, 0, sizeof Mouse);
    memset(Tooltip, 0, sizeof Tooltip);
    SystemTextOwned = false;

    free(Clipboard);
    Clipboard = NULL;

    Deps = (MusicDeps){
        .host = NULL,
        .tic = Tic,
        .mouse = Mouse,
        .tracker = tracker,
        .tooltip = tooltip,
    };

    View = (MusicView){
        .cartTiles = &Tic->cart.banks[0].tiles,
    };

    initMusic(&Music_, &Deps, &Tic->cart.banks[0].music);
    music_set_view(&Music_, &View);
}

static void frame(void)
{
    music_set_view(&Music_, &View);
    music_tick(&Music_);
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

static void typeText(char c)
{
    SystemText = c;
    SystemTextOwned = true;
}

static tic_track* track(void)
{
    return &Music_.src->tracks.data[0];
}

static void addPattern(void)
{
    tic_tool_set_pattern_id(track(), 0, 0, PATTERN_START);
}

static tic_track_pattern* pattern(void)
{
    return &Music_.src->patterns.data[0];
}

static void testInitialTab(void)
{
    setup(true);
    assert(Music_.tab == MUSIC_TRACKER_TAB);

    setup(false);
    assert(Music_.tab == MUSIC_PIANO_TAB);

    puts("initial tab ok");
}

static void testTrackerNote(void)
{
    setup(true);
    addPattern();

    Music_.tracker.edit = (tic_point){0, 0};

    press(tic_key_z);
    frame();

    assert(pattern()->rows[0].note == NoteStart);
    assert(pattern()->rows[0].octave == 3);
    assert(Music_.tracker.edit.y == 1);

    puts("tracker note ok");
}

static void testTrackerCommand(void)
{
    setup(true);
    addPattern();

    Music_.tracker.edit = (tic_point){5, 0};

    press(tic_key_unknown);
    typeText('M');
    frame();

    assert(pattern()->rows[0].command == tic_music_cmd_volume);
    assert(pattern()->rows[0].param1 == MAX_VOLUME);
    assert(pattern()->rows[0].param2 == MAX_VOLUME);

    View.frame.captured = true;
    press(tic_key_unknown);
    typeText('S');
    frame();

    assert(pattern()->rows[0].command == tic_music_cmd_volume);

    puts("tracker command ok");
}

static void testPianoDelete(void)
{
    setup(false);
    addPattern();

    Music_.piano.edit = (tic_point){0, 0};

    press(tic_key_delete);
    frame();

    assert(tic_tool_get_pattern_id(track(), 0, 0) == 0);

    puts("piano delete ok");
}

static void testClipboardGesture(void)
{
    setup(true);
    addPattern();

    Music_.tracker.edit = (tic_point){0, 0};

    press(tic_key_z);
    frame();
    assert(pattern()->rows[0].note == NoteStart);

    // The note entry moved the cursor down; copy the row it was written to.
    Music_.tracker.edit.y = 0;

    // The host says copy while the keys say cut: only the view may decide.
    View.frame.clipboard = TIC_CLIPBOARD_COPY;
    press(tic_key_x);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(Clipboard != NULL);
    assert(pattern()->rows[0].note == NoteStart);

    Music_.tracker.edit.y = 1;
    View.frame.clipboard = TIC_CLIPBOARD_PASTE;
    press(tic_key_v);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(pattern()->rows[1].note == NoteStart);

    MusicClipboard.undo(&Music_);
    assert(pattern()->rows[1].note == 0);

    MusicClipboard.redo(&Music_);
    assert(pattern()->rows[1].note == NoteStart);

    // The host says cut while the keys say copy.
    View.frame.clipboard = TIC_CLIPBOARD_CUT;
    press(tic_key_c);
    alsoDown(tic_key_ctrl);
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;
    assert(pattern()->rows[1].note == 0);

    puts("clipboard gesture ok");
}

static void testPlayback(void)
{
    setup(true);

    musicPlayTrack(&Music_, false);
    assert(Tic->ram->music_state.flag.music_status != tic_music_stop);

    musicStop(&Music_);
    assert(Tic->ram->music_state.flag.music_status == tic_music_stop);

    puts("playback ok");
}

static void testTooltipAndPointer(void)
{
    setup(true);

    // The channel 0 tumbler in the tracker layout.
    memset(Mouse, 0, sizeof Mouse);
    pointer(68, 25);
    frame();
    assert(strcmp(Tooltip, "on/off channel") == 0);

    Tooltip[0] = '\0';
    View.frame.pointerOwned = true;
    memset(Mouse, 0, sizeof Mouse);
    pointer(68, 25);
    click(68, 25);
    frame();
    assert(Tooltip[0] == '\0');
    assert(Music_.on[0]);

    View.frame.pointerOwned = false;
    memset(Mouse, 0, sizeof Mouse);
    pointer(68, 25);
    click(68, 25);
    frame();
    assert(!Music_.on[0]);

    puts("tooltip and pointer ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testInitialTab();
    testTrackerNote();
    testTrackerCommand();
    testPianoDelete();
    testClipboardGesture();
    testPlayback();
    testTooltipAndPointer();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
