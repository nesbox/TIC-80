/* The code editor links without the studio: this file builds the editor with
 * its three models, the keyboard, the mouse and the ui modules, and a fake
 * host that records what the editor asks for.
 *
 * Target code-editor-test in cmake/tests.cmake; `ctest -R code-editor` runs it.
 */
#include "studio/editors/code/code.h"
#include "studio/editors/code/minimap.h"
#include "script.h"
#include "core/core.h"
#include "tools.h"

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

static u8 pixelAt(s32 x, s32 y)
{
    return tic_tool_peek4(Tic->ram->vram.screen.data, y * TIC80_WIDTH + x);
}

static void setMinimapTheme(void)
{
    View.theme = (CodeTheme){
        .BG = tic_color_dark_grey,
        .FG = tic_color_white,
        .STRING = tic_color_yellow,
        .NUMBER = tic_color_cyan,
        .KEYWORD = tic_color_orange,
        .API = tic_color_light_green,
        .COMMENT = tic_color_grey,
        .SIGN = tic_color_light_grey,
        .select = tic_color_light_blue,
        .cursor = tic_color_red,
    };
}

static void buildDrawSource(char* out, size_t size)
{
    size_t len = 0;
    out[0] = '\0';

    for(s32 line = 0; line < 20; line++)
    {
        if(line == 5)
            len += snprintf(out + len, size - len, "-- c\n");
        else if(line == 6)
            len += snprintf(out + len, size - len, "\n");
        else if(line == 7)
        {
            len += snprintf(out + len, size - len, "-- ");

            for(s32 i = 0; i < 97; i++)
                out[len++] = '-';

            out[len++] = '\n';
            out[len] = '\0';
        }
        else
            len += snprintf(out + len, size - len, line + 1 < 20 ? "x\n" : "x");
    }
}

static void buildMarkedSource(char* out, size_t size)
{
    size_t len = 0;
    out[0] = '\0';

    for(s32 line = 0; line < 20; line++)
    {
        if(line == 7)
        {
            // Long enough to check the caret's horizontal place.
            for(s32 i = 0; i < 40; i++)
                out[len++] = 'x';
        }
        else
        {
            const char* text = line == 3 ? "-- @@@ note" : "x";
            len += snprintf(out + len, size - len, "%s", text);
        }

        if(line + 1 < 20)
            out[len++] = '\n';

        out[len] = '\0';
    }
}

static void buildManyLines(char* out, size_t size, s32 count, bool bookmarkFirst)
{
    size_t len = 0;
    out[0] = '\0';

    for(s32 line = 0; line < count; line++)
    {
        const char* text = bookmarkFirst && line == 0 ? "-- @@@ n" : "x";
        len += snprintf(out + len, size - len, "%s\n", text);
    }
}

static void showMinimap(void)
{
    View.options.minimap = true;
    frame();
    idleFrames(STUDIO_ANIM_TIME);

    assert(Code_.anim.minimap == MINIMAP_WIDTH);
}

static void testMinimapGeometry(void)
{
    assert(minimap_map_lines(129) == 64);
    assert(minimap_map_lines(122) == 61);
    assert(minimap_map_lines(1) == 0);

    // A fitting file shows from its first line.
    assert(minimap_top(20, 129, 0, 18) == 0);
    assert(minimap_top(20, 129, 10, 18) == 0);

    // A longer file scrolls the frame across the whole map.
    assert(minimap_top(201, 129, 0, 18) == 0);
    assert(minimap_top(201, 129, 100, 18) == 75);
    assert(minimap_top(201, 129, 183, 18) == 137);
    assert(minimap_top(201, 129, 200, 18) == 137);

    MinimapFrame frame = minimap_viewport(201, 129, 100, 18);
    assert(frame.y == 50 && frame.height == 36);

    frame = minimap_viewport(201, 129, 183, 18);
    assert(frame.y == 92 && frame.height == 36);

    frame = minimap_viewport(20, 129, 0, 18);
    assert(frame.y == 0 && frame.height == 36);

    assert(minimap_line_at(0, 201, 129, 100, 18) == 75);
    assert(minimap_line_at(127, 201, 129, 100, 18) == 138);

    assert(minimap_drag_scroll(0, 0, 201, 129, 18) == 0);
    assert(minimap_drag_scroll(0, 90, 201, 129, 18) == 179);
    assert(minimap_drag_scroll(0, 118, 201, 129, 18) == 183);
    assert(minimap_drag_scroll(100, 90, 201, 129, 18) == 183);
    assert(minimap_drag_scroll(20, 0, 31, 129, 18) == 20);
    assert(minimap_drag_scroll(20, 10, 31, 129, 18) == 25);
    assert(minimap_drag_scroll(20, 118, 31, 129, 18) == 30);

    puts("minimap geometry ok");
}

static void testMinimapDraw(void)
{
    char src[1024];
    buildDrawSource(src, sizeof src);

    setup(src);
    setMinimapTheme();
    showMinimap();

    const s32 x = TIC80_WIDTH - MINIMAP_WIDTH;

    // 20 lines over the 129-pixel column: two pixels each.
    assert(pixelAt(x + 1, 10) == tic_color_grey);                  // line 5, a comment
    assert(pixelAt(x + 1, 12) == tic_color_dark_grey);             // line 6, the frame's background
    assert(pixelAt(x + MINIMAP_WIDTH - 2, 14) == tic_color_grey);  // line 7, clipped
    assert(pixelAt(x + MINIMAP_WIDTH - 1, 14) == tic_color_white); // its white frame border

    // Outside the frame the column is darker than the editor's background.
    assert(pixelAt(x, 100) == tic_color_black);

    // The long text line at screen row 49-55 does not paint into the column.
    assert(pixelAt(x, 50) == tic_color_black);
    assert(pixelAt(x + 4, 50) == tic_color_black);

    // The 18 visible lines frame the first 36 map pixels.
    assert(pixelAt(x, 0) == tic_color_white);
    assert(pixelAt(x + MINIMAP_WIDTH - 1, 0) == tic_color_white);
    assert(pixelAt(x, 35) == tic_color_white);

    View.options.minimap = false;
    frame();
    idleFrames(STUDIO_ANIM_TIME);

    assert(Code_.anim.minimap == 0);
    assert(pixelAt(x, 100) == tic_color_dark_grey);

    puts("minimap draw ok");
}

static void testMinimapMarks(void)
{
    char src[1024];
    buildMarkedSource(src, sizeof src);

    setup(src);
    setMinimapTheme();
    showMinimap();

    const s32 x = TIC80_WIDTH - MINIMAP_WIDTH;

    // The caret is solid right after an edit; keep it so while it is checked.
    Code_.cursor.delay = TIC80_FRAMERATE;

    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 7);
    frame();

    // The bookmark line is one full-width yellow row.
    assert(pixelAt(x, 6) == tic_color_yellow);
    assert(pixelAt(x + 15, 6) == tic_color_yellow);
    assert(pixelAt(x + MINIMAP_WIDTH - 1, 6) == tic_color_yellow);
    assert(pixelAt(x + 1, 5) == tic_color_dark_grey);
    assert(pixelAt(x + 1, 7) == tic_color_dark_grey);

    assert(pixelAt(x + 1, 14) == tic_color_red);

    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 9);
    frame();

    assert(pixelAt(x + 1, 18) == tic_color_red);
    assert(pixelAt(x + 1, 14) == tic_color_white);

    // The caret's tick follows its column, not just its line.
    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 7) + 20;
    frame();

    assert(pixelAt(x + 11, 14) == tic_color_red);
    assert(pixelAt(x + 1, 14) == tic_color_white);

    // A text selection shows as a select-coloured segment on its line.
    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 7) + 25;
    Code_.text.cursor.selection = getPosByLine(Tic->cart.code.data, 7) + 5;
    frame();

    assert(pixelAt(x + 2, 14) == tic_color_light_blue);
    assert(pixelAt(x + 12, 14) == tic_color_light_blue);
    assert(pixelAt(x + 1, 14) != tic_color_light_blue);

    // The caret on the map blinks with the editor's.
    Code_.text.cursor.selection = NULL;
    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 7);
    Code_.cursor.delay = 0;
    Code_.tickCounter = 0;
    frame();

    assert(pixelAt(x + 1, 14) == tic_color_red);

    Code_.tickCounter = TIC80_FRAMERATE / 2;
    frame();

    assert(pixelAt(x + 1, 14) != tic_color_red);

    Code_.cursor.delay = TIC80_FRAMERATE;

    // The map follows the viewport when the file does not fit.
    char many[2048];
    buildManyLines(many, sizeof many, 200, true);

    setup(many);
    setMinimapTheme();
    showMinimap();

    Code_.scroll.y = 100;
    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 100);
    frame();

    const s32 height = TIC80_HEIGHT - STUDIO_TEXT_HEIGHT;
    const s32 visible = height / STUDIO_TEXT_HEIGHT;
    const s32 lines = getLinesCount(&Code_.text) + 1;
    const s32 top = minimap_top(lines, height, Code_.scroll.y, visible);
    const s32 row = (100 - top) * MINIMAP_LINE_HEIGHT;

    assert(top == 75);
    assert(row == 50);

    // The bookmark at line 0 is out of the window.
    assert(pixelAt(x + 1, 0) != tic_color_yellow);

    // The caret's tick is at its row in the window.
    assert(pixelAt(x + 1, row) == tic_color_red);

    puts("minimap marks ok");
}

static void testMinimapSlide(void)
{
    setup("a\n");
    setMinimapTheme();

    assert(Code_.anim.minimap == 0);
    assert(!Code_.showMinimap);

    View.options.minimap = true;
    frame();

    assert(Code_.showMinimap);
    assert(Code_.anim.minimap == 0);

    idleFrames(STUDIO_ANIM_TIME);
    assert(Code_.anim.minimap == MINIMAP_WIDTH);

    View.options.minimap = false;
    frame();

    assert(!Code_.showMinimap);
    assert(Code_.anim.minimap == MINIMAP_WIDTH);

    idleFrames(STUDIO_ANIM_TIME);
    assert(Code_.anim.minimap == 0);

    // A reversal starts from the current width.
    View.options.minimap = true;
    frame();
    idleFrames(3);

    s32 width = Code_.anim.minimap;
    assert(width > 0 && width < MINIMAP_WIDTH);

    View.options.minimap = false;
    frame();
    assert(Code_.anim.minimap == width);

    idleFrames(STUDIO_ANIM_TIME);
    assert(Code_.anim.minimap == 0);

    // The saved state opens without a slide.
    View.options.minimap = true;
    initCode(&Code_, &Deps);
    code_set_view(&Code_, &View);
    code_tick(&Code_);

    assert(Code_.showMinimap);
    assert(Code_.anim.minimap == MINIMAP_WIDTH);

    puts("minimap slide ok");
}

static void testMinimapMouse(void)
{
    char src[2048];
    buildManyLines(src, sizeof src, 200, false);

    setup(src);
    setMinimapTheme();
    showMinimap();

    const s32 x = TIC80_WIDTH - MINIMAP_WIDTH;
    const s32 height = TIC80_HEIGHT - STUDIO_TEXT_HEIGHT;
    const s32 visible = height / STUDIO_TEXT_HEIGHT;
    const s32 lines = getLinesCount(&Code_.text) + 1;
    const s32 clickY = 60;
    const s32 pressLine = minimap_line_at(clickY, lines, height, Code_.scroll.y, visible);

    assert(pressLine == 30);

    Tic->ram->input.mouse.relative = false;
    Tic->ram->input.mouse.x = TIC80_OFFSET_LEFT + x + 4;
    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + clickY;

    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {x + 4, clickY}, .end = {x + 4, clickY}};

    char* caret = Code_.text.cursor.position;
    frame();

    assert(Code_.scroll.y == pressLine - visible / 2);
    assert(Code_.text.cursor.position == caret);

    const s32 dragY = 80;
    const s32 dragLine = minimap_line_at(dragY, lines, height, Code_.scroll.y, visible);

    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + dragY;
    frame();

    assert(Code_.scroll.y == dragLine - visible / 2);

    Mouse[tic_mouse_left] = (MouseState){0};
    frame();

    // A press inside the frame drags it as the map's scrollbar: the scroll is
    // even over the whole file, and the screen's end is the file's end.
    Code_.scroll.y = 0;

    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + 10;
    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {x + 4, 10}, .end = {x + 4, 10}};
    frame();

    assert(Code_.scroll.y == 0);

    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + 100;
    frame();

    assert(Code_.scroll.y == 179);

    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + height - 1;
    frame();

    assert(Code_.scroll.y == lines - visible);
    assert(minimap_top(lines, height, Code_.scroll.y, visible) == lines - minimap_map_lines(height));
    assert(minimap_viewport(lines, height, Code_.scroll.y, visible).y == 92);

    Mouse[tic_mouse_left] = (MouseState){0};
    frame();

    // The wheel still scrolls over the column.
    Code_.scroll.y = 50;
    Tic->ram->input.mouse.scrolly = -1;
    frame();
    Tic->ram->input.mouse.scrolly = 0;

    assert(Code_.scroll.y == 53);

    // Hidden, the column gives the press back to the text.
    View.options.minimap = false;
    frame();
    idleFrames(STUDIO_ANIM_TIME);
    assert(Code_.anim.minimap == 0);

    Tic->ram->input.mouse.x = TIC80_OFFSET_LEFT + 220;
    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + 14;
    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {220, 14}, .end = {220, 14}};

    caret = Code_.text.cursor.position;
    frame();

    assert(Code_.text.cursor.position != caret);

    Mouse[tic_mouse_left] = (MouseState){0};

    // Hiding the column does not snap the view back to the caret.
    View.options.minimap = true;
    frame();
    idleFrames(STUDIO_ANIM_TIME);
    assert(Code_.anim.minimap == MINIMAP_WIDTH);

    Code_.text.cursor.position = getPosByLine(Tic->cart.code.data, 150);
    Code_.scroll.y = 10;

    View.options.minimap = false;
    frame();
    idleFrames(STUDIO_ANIM_TIME);

    assert(Code_.anim.minimap == 0);
    assert(Code_.scroll.y == 10);

    puts("minimap mouse ok");
}

static void testMinimapFitDrag(void)
{
    char src[512];
    buildManyLines(src, sizeof src, 30, false);

    setup(src);
    setMinimapTheme();
    showMinimap();

    const s32 x = TIC80_WIDTH - MINIMAP_WIDTH;
    const s32 height = TIC80_HEIGHT - STUDIO_TEXT_HEIGHT;
    const s32 visible = height / STUDIO_TEXT_HEIGHT;
    const s32 lines = getLinesCount(&Code_.text) + 1;

    // Scrolled past the last full page; the frame is drawn lower.
    Code_.scroll.y = 20;
    frame();

    assert(lines == 31);
    assert(minimap_viewport(lines, height, 20, visible).y == 40);

    // Grabbing the frame does not jump: the press keeps the scroll.
    Tic->ram->input.mouse.relative = false;
    Tic->ram->input.mouse.x = TIC80_OFFSET_LEFT + x + 4;
    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + 50;
    Mouse[tic_mouse_left] = (MouseState){
        .down = true, .click = true, .start = {x + 4, 50}, .end = {x + 4, 50}};
    frame();

    assert(Code_.scroll.y == 20);

    // The drag is line by line and stops at the editor's end.
    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + 60;
    frame();

    assert(Code_.scroll.y == 25);

    Tic->ram->input.mouse.y = TIC80_OFFSET_TOP + height - 1;
    frame();

    assert(Code_.scroll.y == lines - 1);

    Mouse[tic_mouse_left] = (MouseState){0};
    frame();

    puts("minimap fit drag ok");
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
    testMinimapGeometry();
    testMinimapDraw();
    testMinimapMarks();
    testMinimapSlide();
    testMinimapMouse();
    testMinimapFitDrag();

    free(Clipboard);
    tic_core_close(Tic);

    return 0;
}
