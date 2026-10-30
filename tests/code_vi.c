/* The vi machine links without the studio: this file builds code_vi.c with the
 * text model, the search model, the keyboard module and the core, and a fake
 * host that records what the machine asks for. Keys are pressed the way
 * tests/keyboard.c presses them: into the machine's input buffers, then a core
 * tick.
 *
 * Target code-vi-test in cmake/tests.cmake; `ctest -R code-vi` runs it.
 */
#include "studio/editors/code/vi.h"
#include "studio/editors/code/find.h"
#include "script.h"
#include "core/core.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char Buffer[TIC_CODE_SIZE];
static Text Text_;
static CodeViHost Host;
static tic_mem* Tic;
static ViMode Mode;
static char Search[64];

static bool systemTextOwned = false;
static char systemText = '\0';

static struct
{
    s32 editorMode;
    s32 editorModes;
    s32 updates;
    s32 recenters;
    s32 pages;
    s32 lastPageDir;
    bool lastPageHalf;
    s32 gotos;
    s32 runs;
    s32 saves;
    s32 fonts;
    s32 shadows;
    s32 copies;
    s32 cuts;
    s32 pastes;

} Calls;

static bool isAlnum(char c)
{
    return isalnum((u8)c) || c == '_';
}

static const tic_script Script =
    {
        .name = "test",
        .singleComment = "--",
        .stdStringStartEnd = "\"",
        .lang_isalnum = isAlnum,
};

// The platform's text input, which the keyboard module asks before its
// fallback; the test answers it, as tests/keyboard.c does.
bool tic_sys_keyboard_text(char* text)
{
    if(!systemTextOwned)
        return false;

    *text = systemText;
    systemText = '\0';
    systemTextOwned = false;
    return true;
}

static bool historyEnabled(void* host)
{
    (void)host;
    return Mode != VI_INSERT;
}

static ViMode getViMode(void* host)
{
    (void)host;
    return Mode;
}

static void setViMode(void* host, ViMode mode)
{
    (void)host;
    Mode = mode;
}

static void setEditorMode(void* host, CodeViEdit mode)
{
    (void)host;
    Calls.editorMode = mode;
    Calls.editorModes++;
}

static void updateView(void* host)
{
    (void)host;
    Calls.updates++;
}

static void recenter(void* host, bool emacs)
{
    (void)host;
    (void)emacs;
    Calls.recenters++;
}

static void page(void* host, s32 dir, bool half)
{
    (void)host;
    Calls.pages++;
    Calls.lastPageDir = dir;
    Calls.lastPageHalf = half;
}

static void gotoUpdate(void* host)
{
    (void)host;
    Calls.gotos++;
}

static void runGame(void* host)
{
    (void)host;
    Calls.runs++;
}

static void saveProject(void* host)
{
    (void)host;
    Calls.saves++;
}

static void viewToggle(void* host, bool font)
{
    (void)host;
    if(font)
        Calls.fonts++;
    else
        Calls.shadows++;
}

static void clipboard(void* host, CodeViClipboard op)
{
    (void)host;
    switch(op)
    {
        case CODE_VI_COPY: Calls.copies++; break;
        case CODE_VI_CUT: Calls.cuts++; break;
        case CODE_VI_PASTE: Calls.pastes++; break;
    }
}

static bool clipboardNewline(void* host)
{
    (void)host;
    return false;
}

static void setup(const char* initial)
{
    text_free(&Text_);
    memset(Buffer, 0, sizeof Buffer);
    strncpy(Buffer, initial, MAX_CODE);

    Mode = VI_NORMAL;
    memset(&Calls, 0, sizeof Calls);
    memset(Search, 0, sizeof Search);
    systemTextOwned = false;

    text_init(&Text_, Buffer, (TextOptions){.script = &Script, .tabSize = 4}, historyEnabled, NULL);

    // A frame with no keys clears the core's key history, so the first press
    // of the next case is a fresh press whatever the previous case ended on.
    memset(&Tic->ram->input.keyboard, 0, sizeof Tic->ram->input.keyboard);
    tic_core_tick_start(Tic);
    tic_core_tick_io(Tic);
    tic_core_tick_end(Tic);

    Host = (CodeViHost){
        .host = NULL,
        .tic = Tic,
        .search = Search,
        .searchSize = sizeof Search,

        .getViMode = getViMode,
        .setViMode = setViMode,
        .setEditorMode = setEditorMode,
        .updateView = updateView,
        .recenter = recenter,
        .page = page,
        .gotoUpdate = gotoUpdate,
        .runGame = runGame,
        .saveProject = saveProject,
        .viewToggle = viewToggle,
        .clipboard = clipboard,
        .clipboardNewline = clipboardNewline,
    };
}

// One frame with the key in the buffer; the core tick after it is what makes
// the next frame's press a fresh press rather than a held key.
static void press(tic_mem* tic, tic_key key)
{
    memset(&tic->ram->input.keyboard, 0, sizeof tic->ram->input.keyboard);
    tic->ram->input.keyboard.keys[0] = key;
}

static void alsoDown(tic_mem* tic, tic_key key)
{
    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        if(!tic->ram->input.keyboard.keys[i])
        {
            tic->ram->input.keyboard.keys[i] = key;
            return;
        }
}

static void frame(tic_mem* tic)
{
    code_vi_tick(&Text_, &Host);

    tic_core_tick_start(tic);
    tic_core_tick_io(tic);
    tic_core_tick_end(tic);
}

static void keyFrame(tic_mem* tic, tic_key key)
{
    press(tic, key);
    frame(tic);
}

// A frame with the key released: the next press of the same key is a fresh
// press, not a held one.
static void releaseFrame(tic_mem* tic)
{
    press(tic, tic_key_unknown);
    frame(tic);
}

static void modFrame(tic_mem* tic, tic_key mod, tic_key key)
{
    press(tic, key);
    alsoDown(tic, mod);
    frame(tic);
}

static void typeFrame(tic_mem* tic, char c)
{
    press(tic, tic_key_unknown);
    systemText = c;
    systemTextOwned = true;
    frame(tic);
}

static void testInsertUndo(tic_mem* tic)
{
    setup("");

    Mode = VI_INSERT;
    typeFrame(tic, 'a');
    typeFrame(tic, 'b');

    assert(strcmp(Buffer, "ab") == 0);

    // Escape ends the session; one undo takes the whole session back.
    keyFrame(tic, tic_key_escape);
    assert(Mode == VI_NORMAL);

    text_undo(&Text_);
    assert(strcmp(Buffer, "") == 0);

    puts("insert undo ok");
}

static void testGates(tic_mem* tic)
{
    setup("");
    Mode = VI_INSERT;

    // The bar owns the frame: typed text does not reach the editor.
    Host.captured = true;
    typeFrame(tic, 'a');
    assert(strcmp(Buffer, "") == 0);
    Host.captured = false;

    // The bar closed on Escape: the editor does not see it.
    Host.escaped = true;
    keyFrame(tic, tic_key_escape);
    assert(Mode == VI_INSERT);
    Host.escaped = false;

    puts("gates ok");
}

static void testMotions(tic_mem* tic)
{
    setup("one two\nthree\n");

    keyFrame(tic, tic_key_l);
    assert(Text_.cursor.position == Buffer + 1);

    keyFrame(tic, tic_key_h);
    assert(Text_.cursor.position == Buffer);

    keyFrame(tic, tic_key_w);
    assert(Text_.cursor.position == Buffer + 3);

    keyFrame(tic, tic_key_b);
    assert(Text_.cursor.position == Buffer);

    keyFrame(tic, tic_key_end);
    assert(Text_.cursor.position == Buffer + 7);

    keyFrame(tic, tic_key_0);
    assert(Text_.cursor.position == Buffer);

    keyFrame(tic, tic_key_down);
    assert(Text_.cursor.position == Buffer + 8);

    keyFrame(tic, tic_key_up);
    assert(Text_.cursor.position == Buffer);

    puts("motions ok");
}

static void testChangeInDelimiters(tic_mem* tic)
{
    setup(" (abc)");

    Text_.cursor.position = Buffer + 1;
    keyFrame(tic, tic_key_c);

    assert(Mode == VI_INSERT);
    assert(strcmp(Buffer, " ()") == 0);
    assert(Text_.cursor.position == Buffer + 2);

    puts("change delimiters ok");
}

static void testLineOperations(tic_mem* tic)
{
    setup("foo\nbar\n");

    keyFrame(tic, tic_key_d);
    assert(Calls.cuts == 1);

    keyFrame(tic, tic_key_y);
    assert(Calls.copies == 1);

    keyFrame(tic, tic_key_p);
    assert(Calls.pastes == 1);

    puts("line operations ok");
}

static void testGoto(tic_mem* tic)
{
    setup("a\nb\nc\n");

    keyFrame(tic, tic_key_3);

    assert(Calls.editorMode == CODE_VI_EDIT_GOTO);
    assert(strcmp(Search, "3") == 0);
    assert(Calls.gotos == 1);

    puts("goto ok");
}

static void testSearch(tic_mem* tic)
{
    setup("foo bar foo");

    // '*' takes the word under the caret and finds the next one.
    modFrame(tic, tic_key_shift, tic_key_3);

    assert(strcmp(Search, "foo") == 0);
    assert(Text_.cursor.position == Buffer + 8);

    // 'n' repeats the search.
    keyFrame(tic, tic_key_n);
    assert(Calls.updates > 0);

    // '/' opens the find UI and stops the machine's own search.
    keyFrame(tic, tic_key_slash);
    assert(Calls.editorMode == CODE_VI_EDIT_FIND);

    puts("search ok");
}

static void testMatchingDelimiter(tic_mem* tic)
{
    setup(" (a) ");

    Text_.cursor.position = Buffer + 1;
    modFrame(tic, tic_key_shift, tic_key_5);

    assert(Text_.cursor.position == Buffer + 3);

    puts("delimiter ok");
}

static void testSeekEmptyLine(tic_mem* tic)
{
    setup("a\n\nb\n");

    modFrame(tic, tic_key_shift, tic_key_rightbracket);
    assert(Text_.cursor.position == Buffer + 2);

    puts("seek ok");
}

static void testUndoRedo(tic_mem* tic)
{
    setup("abc");

    keyFrame(tic, tic_key_x);
    assert(strcmp(Buffer, "bc") == 0);

    keyFrame(tic, tic_key_u);
    assert(strcmp(Buffer, "abc") == 0);

    releaseFrame(tic);
    modFrame(tic, tic_key_shift, tic_key_u);
    assert(strcmp(Buffer, "bc") == 0);

    puts("undo/redo ok");
}

static void testAsks(tic_mem* tic)
{
    setup("abc");

    modFrame(tic, tic_key_shift, tic_key_w);
    assert(Calls.saves == 1);

    modFrame(tic, tic_key_shift, tic_key_r);
    assert(Calls.runs == 1);

    modFrame(tic, tic_key_alt, tic_key_f);
    assert(Calls.fonts == 1);

    modFrame(tic, tic_key_alt, tic_key_s);
    assert(Calls.shadows == 1);

    keyFrame(tic, tic_key_pageup);
    assert(Calls.pages == 1 && Calls.lastPageDir < 0 && !Calls.lastPageHalf);

    modFrame(tic, tic_key_shift, tic_key_k);
    assert(Calls.pages == 2 && Calls.lastPageDir < 0 && Calls.lastPageHalf);

    puts("asks ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    Tic = tic;

    testInsertUndo(tic);
    testGates(tic);
    testMotions(tic);
    testChangeInDelimiters(tic);
    testLineOperations(tic);
    testGoto(tic);
    testSearch(tic);
    testMatchingDelimiter(tic);
    testSeekEmptyLine(tic);
    testUndoRedo(tic);
    testAsks(tic);

    tic_core_close(tic);

    return 0;
}
