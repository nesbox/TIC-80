/* The code editor links without the studio: this file builds the editor with
 * its three models, the keyboard, the mouse and the ui modules, and a fake
 * host that records what the editor asks for.
 *
 * Target code-editor-test in cmake/tests.cmake; `ctest -R code-editor` runs it.
 */
#include "studio/editors/code/code.h"
#include "script.h"
#include "core/core.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Code Code_;
static CodeView View;
static CodeDeps Deps;
static tic_mem* Tic;
static tic_tiles Tiles;
static MouseState Mouse[3];

static char* Clipboard;

static bool systemTextOwned = false;
static char systemText = '\0';

static struct
{
    s32 runs;
    s32 saves;
    s32 toggles;
    CodeViewToggle toggle;
    char tooltip[64];

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

static const tic_script SlashScript =
    {
        .name = "slash",
        .singleComment = "//",
        .stdStringStartEnd = "\"",
        .lang_isalnum = isAlnum,
};

bool tic_sys_keyboard_text(char* text)
{
    if(!systemTextOwned)
        return false;

    *text = systemText;
    systemText = '\0';
    systemTextOwned = false;
    return true;
}

void tic_sys_clipboard_set(const char* text)
{
    free(Clipboard);
    Clipboard = strdup(text);
}

bool tic_sys_clipboard_has()
{
    return Clipboard != NULL;
}

char* tic_sys_clipboard_get()
{
    return Clipboard ? strdup(Clipboard) : NULL;
}

void tic_sys_clipboard_free(const char* text)
{
    free((void*)text);
}

static ViMode getViMode(void* host)
{
    (void)host;
    return VI_NORMAL;
}

static void setViMode(void* host, ViMode mode)
{
    (void)host;
    (void)mode;
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

static void viewToggle(void* host, CodeViewToggle option)
{
    (void)host;
    Calls.toggles++;
    Calls.toggle = option;
}

static void tooltip(void* host, const char* text)
{
    (void)host;
    snprintf(Calls.tooltip, sizeof Calls.tooltip, "%s", text);
}

static void tickCore(void)
{
    tic_core_tick_start(Tic);
    tic_core_tick_io(Tic);
    tic_core_tick_end(Tic);
}

static void setup(const char* initial)
{
    memset(Tic->cart.code.data, 0, TIC_CODE_SIZE);
    strncpy(Tic->cart.code.data, initial, MAX_CODE);

    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    memset(&Tic->ram->input.mouse, 0, sizeof Tic->ram->input.mouse);
    tickCore();

    memset(&Calls, 0, sizeof Calls);
    free(Clipboard);
    Clipboard = NULL;
    systemTextOwned = false;

    Deps = (CodeDeps){
        .host = NULL,
        .tic = Tic,
        .mouse = Mouse,
        .tiles = &Tiles,
        .getViMode = getViMode,
        .setViMode = setViMode,
        .runGame = runGame,
        .saveProject = saveProject,
        .viewToggle = viewToggle,
        .tooltip = tooltip,
    };

    View = (CodeView){
        .options =
            {
                .script = &Script,
                .tabSize = 4,
                .sidebar = true,
                .statusbar = true,
            },
    };

    memset(Mouse, 0, sizeof Mouse);

    initCode(&Code_, &Deps);
    code_set_view(&Code_, &View);
    code_tick(&Code_);
}

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

static void frame(void)
{
    code_set_view(&Code_, &View);
    code_tick(&Code_);
    tickCore();
}

static void keyFrame(tic_key key)
{
    press(Tic, key);
    frame();
}

static void modFrame(tic_key mod, tic_key key)
{
    press(Tic, key);
    alsoDown(Tic, mod);
    frame();
}

static void typeFrame(char c)
{
    press(Tic, tic_key_unknown);
    systemText = c;
    systemTextOwned = true;
    frame();
}

static void idleFrames(s32 count)
{
    for(s32 i = 0; i < count; i++)
    {
        press(Tic, tic_key_unknown);
        frame();
    }
}

static void testTyping(void)
{
    setup("abc\n");

    Code_.text.cursor.position = Tic->cart.code.data + 4;

    typeFrame('X');
    assert(strcmp(Tic->cart.code.data, "abc\nX") == 0);

    modFrame(tic_key_ctrl, tic_key_z);
    assert(strcmp(Tic->cart.code.data, "abc\n") == 0);

    modFrame(tic_key_ctrl, tic_key_y);
    assert(strcmp(Tic->cart.code.data, "abc\nX") == 0);

    puts("typing ok");
}

static void testClipboard(void)
{
    setup("abc\ndef\n");

    View.frame.clipboard = TIC_CLIPBOARD_COPY;
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;

    assert(Clipboard != NULL);
    assert(strcmp(Clipboard, "abc\n") == 0);

    Code_.text.cursor.position = Tic->cart.code.data + 8;

    View.frame.clipboard = TIC_CLIPBOARD_PASTE;
    frame();
    View.frame.clipboard = TIC_CLIPBOARD_NONE;

    assert(strcmp(Tic->cart.code.data, "abc\ndef\nabc\n") == 0);

    puts("clipboard ok");
}

static void testFind(void)
{
    setup("abc\nbcd\n");

    codeFind(&Code_);
    assert(Code_.mode == TEXT_FIND_MODE);

    typeFrame('b');
    assert(strcmp(Code_.popup.text, "b") == 0);
    assert(Code_.text.cursor.position == Tic->cart.code.data + 1);

    idleFrames(STUDIO_ANIM_TIME + 2);
    keyFrame(tic_key_return);
    assert(Code_.mode == TEXT_EDIT_MODE);

    puts("find ok");
}

static void testGoto(void)
{
    setup("aa\nbb\ncc\n");

    codeGoto(&Code_);
    assert(Code_.mode == TEXT_GOTO_MODE);

    typeFrame('2');
    idleFrames(STUDIO_ANIM_TIME + 2);
    keyFrame(tic_key_return);

    assert(Code_.mode == TEXT_EDIT_MODE);
    assert(Code_.text.cursor.position == Tic->cart.code.data + 3);

    puts("goto ok");
}

static void testBookmarks(void)
{
    // Ctrl+F1 inserts the marker above the line and removes it again.
    setup("aa\nbb\n");
    Code_.text.cursor.position = Tic->cart.code.data + 3;

    modFrame(tic_key_ctrl, tic_key_f1);

    assert(strcmp(Tic->cart.code.data, "aa\n-- @@@ bookmark\nbb\n") == 0);
    assert(Code_.text.cursor.position == Tic->cart.code.data + 18);
    assert(Code_.text.cursor.selection == NULL);

    idleFrames(1);

    modFrame(tic_key_ctrl, tic_key_f1);
    assert(strcmp(Tic->cart.code.data, "aa\nbb\n") == 0);

    // One undo takes the removal back.
    modFrame(tic_key_ctrl, tic_key_z);
    assert(strcmp(Tic->cart.code.data, "aa\n-- @@@ bookmark\nbb\n") == 0);

    // The list item is the note alone.
    setup("aa\n-- @@@ physics\nbb\n");
    codeBookmarks(&Code_);

    assert(Code_.sidebar.size == 1);
    assert(Code_.sidebar.items[0].pos == Tic->cart.code.data + 10);
    assert(Code_.sidebar.items[0].size == 7);

    // F1 and Shift+F1 move between the notes.
    setup("-- @@@ one\nx\n-- @@@ two\n");
    Code_.text.cursor.position = Tic->cart.code.data + 20;

    modFrame(tic_key_shift, tic_key_f1);
    assert(Code_.text.cursor.position == Tic->cart.code.data + 7);

    idleFrames(1);

    keyFrame(tic_key_f1);
    assert(Code_.text.cursor.position == Tic->cart.code.data + 20);

    // Ctrl+Shift+F1 no longer clears anything.
    setup("aa\n-- @@@ note\nbb\n");

    press(Tic, tic_key_f1);
    alsoDown(Tic, tic_key_ctrl);
    alsoDown(Tic, tic_key_shift);
    frame();

    assert(strcmp(Tic->cart.code.data, "aa\n-- @@@ note\nbb\n") == 0);

    // The marker uses the language's own comment.
    setup("aa\n");
    View.options.script = &SlashScript;
    frame();
    Code_.text.cursor.position = Tic->cart.code.data;

    modFrame(tic_key_ctrl, tic_key_f1);
    assert(strcmp(Tic->cart.code.data, "// @@@ bookmark\naa\n") == 0);

    // vi's m toggles the same marker.
    setup("aa\nbb\n");
    View.options.vi = true;
    frame();
    Code_.text.cursor.position = Tic->cart.code.data + 3;

    keyFrame(tic_key_m);
    assert(strcmp(Tic->cart.code.data, "aa\n-- @@@ bookmark\nbb\n") == 0);

    puts("bookmarks ok");
}

static void testGutterMouse(void)
{
    setup("aa\nbb\n");

    View.options.lineNumbers = true;
    frame();

    Tic->ram->input.mouse.relative = false;
    Tic->ram->input.mouse.x = TIC80_OFFSET_LEFT + 2;
    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP;

    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {2, 0}, .end = {2, 0}};

    frame();

    assert(strcmp(Calls.tooltip, "BOOKMARK LINE [ctrl+f1]") == 0);
    assert(strcmp(Tic->cart.code.data, "-- @@@ bookmark\naa\nbb\n") == 0);

    // The second click on the marker line removes it.
    Mouse[tic_mouse_left] = (MouseState){0};
    frame();

    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {2, 0}, .end = {2, 0}};
    frame();

    assert(strcmp(Tic->cart.code.data, "aa\nbb\n") == 0);

    Mouse[tic_mouse_left] = (MouseState){0};

    // The bar owns the pointer: the hover draws nothing and asks no tooltip.
    memset(Calls.tooltip, 0, sizeof Calls.tooltip);
    View.frame.pointerOwned = true;
    frame();
    assert(Calls.tooltip[0] == '\0');
    View.frame.pointerOwned = false;

    puts("gutter mouse ok");
}

static void testAsks(void)
{
    setup("aa");

    View.options.vi = true;
    frame();

    modFrame(tic_key_alt, tic_key_f);
    assert(Calls.toggles == 1 && Calls.toggle == CODE_VIEW_FONT);

    modFrame(tic_key_shift, tic_key_r);
    assert(Calls.runs == 1);

    modFrame(tic_key_shift, tic_key_w);
    assert(Calls.saves == 1);

    puts("asks ok");
}

static void testSyntax(void)
{
    setup("-- note\nx = 1\n");

    // The first real view colours the text, before any edit.
    assert(getState(&Code_.text, Tic->cart.code.data + 1)->syntax == SyntaxType_COMMENT);
    assert(getState(&Code_.text, Tic->cart.code.data + 8)->syntax == SyntaxType_FG);

    puts("syntax ok");
}

static void testJustSwitched(void)
{
    setup("aa");

    code_just_switched(&Code_);

    typeFrame('X');

    assert(strcmp(Tic->cart.code.data, "aa") == 0);
    assert(!Code_.justSwitched);

    puts("just switched ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testTyping();
    testClipboard();
    testFind();
    testGoto();
    testBookmarks();
    testGutterMouse();
    testAsks();
    testSyntax();
    testJustSwitched();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
