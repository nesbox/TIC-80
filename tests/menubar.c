/* The menu bar links without the studio: this file builds menubar.c with the
 * core and the module it draws through, ui.c, so a call to anything studio.c
 * provides fails to link here rather than appearing quietly in the app build.
 *
 * Target menubar-test in cmake/tests.cmake; `ctest -R menubar` runs it.
 */
#include "studio/menubar.h"
#include "tools.h"
#include <assert.h>
#include <stdio.h>

enum
{
    Mode = 0,
    CheckIcon = 9,
};

enum
{
    AppHidden,
    AppMain,
    AppRight,
};

enum
{
    HiddenAction = 1,
    One,
    Two,
    Disabled,
    SubAItem0,
    SubAItem1,
    SubAItem2,
    SubBItem0,
    ToggleOn,
    ToggleOff,
};

static MouseState Mouse[3];
static s32 Hits[32];
static tic_tiles Tiles;

static void hit(void* data, s32 id)
{
    (void)data;
    Hits[id]++;
}

// A submenu and a second one, so the pointer can move from one owner to another.
static const MenuBarItem SubAItems[] =
    {
        {"SUB A ONE", NULL, hit, SubAItem0, true, false, 0, 0, NULL, -1},
        {"SUB A TWO", NULL, hit, SubAItem1, true, false, 0, 0, NULL, -1},
        {"SUB A ON", NULL, hit, SubAItem2, true, false, 0, 0, NULL, 0},
};

static const MenuBarCategory SubAMenu = {"SUB A", SubAItems, COUNT_OF(SubAItems), false, 0, -1, 0, false};

static const MenuBarItem SubBItems[] =
    {
        {"SUB B ONE", NULL, hit, SubBItem0, true, false, 0, 0, NULL, -1},
};

static const MenuBarCategory SubBMenu = {"SUB B", SubBItems, COUNT_OF(SubBItems), false, 0, -1, 0, false};

static const MenuBarItem HiddenItems[] =
    {
        {"HIDDEN ITEM", NULL, hit, HiddenAction, true, false, 0, 0, NULL, -1},
};

// A disabled item, two submenu owners, and two toggles with no icons: the
// circle is the host's check icon, drawn dark for on and light for off.
static const MenuBarItem MainItems[] =
    {
        {"ONE", "F1", hit, One, true, false, 0, 0, NULL, -1},
        {"TWO", NULL, hit, Two, true, false, 0, 0, NULL, -1},
        {"", NULL, NULL, 0, true, true, 0, 0, NULL, -1},
        {"DISABLED", NULL, hit, Disabled, false, false, 0, 0, NULL, -1},
        {"SUB A", NULL, NULL, 0, true, false, 0, 0, &SubAMenu, -1},
        {"SUB B", NULL, NULL, 0, true, false, 0, 0, &SubBMenu, -1},
        {"ON", NULL, hit, ToggleOn, true, false, 0, 0, NULL, 1},
        {"OFF", NULL, hit, ToggleOff, true, false, 0, 0, NULL, 0},
};

static const MenuBarItem RightItems[] =
    {
        {"RIGHT ITEM", NULL, hit, Two, true, false, 0, 0, NULL, -1},
        {"RIGHT MORE", NULL, hit, One, true, false, 0, 0, NULL, -1},
};

static MenuBarCategory Categories[] =
    {
        // First, so a click on the bar would land on it if it were not skipped.
        {"HIDDEN", HiddenItems, COUNT_OF(HiddenItems), false, 0, -1, 0, true},
        {"APP", MainItems, COUNT_OF(MainItems), false, 0, -1, 0, false},
        // A selection group marked on its second row, for the keyboard's
        // opening row.
        {"RIGHT", RightItems, COUNT_OF(RightItems), true, 0, One, 0, false},
};

// Eight selectable rows: a five-row page lands inside the list. The first row
// owns a submenu of the same length for the submenu cases.
static const MenuBarItem LongSubItems[] =
    {
        {"S0", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S1", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S2", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S3", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S4", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S5", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S6", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"S7", NULL, hit, One, true, false, 0, 0, NULL, -1},
};

static const MenuBarCategory LongSubMenu = {"SUB", LongSubItems, COUNT_OF(LongSubItems), false, 0, -1, 0, false};

static const MenuBarItem LongItems[] =
    {
        {"SUB", NULL, NULL, 0, true, false, 0, 0, &LongSubMenu, -1},
        {"L1", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L2", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L3", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L4", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L5", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L6", NULL, hit, One, true, false, 0, 0, NULL, -1},
        {"L7", NULL, hit, One, true, false, 0, 0, NULL, -1},
};

// Compact mode: two icon items become tabs, the third (no icon) does not; pins
// point at menu items, so their handlers are the test's counters.
static const MenuBarItem TabItems[] =
    {
        {"TAB ONE", NULL, hit, One, true, false, 1, 0, NULL, -1},
        {"TAB TWO", NULL, hit, Two, true, false, 2, 0, NULL, -1},
        {"NO ICON", NULL, hit, Disabled, true, false, 0, 0, NULL, -1},
};

static const MenuBarItem* const CompactPins[] =
    {
        &MainItems[0], // ONE
        &MainItems[6], // ON
};

#define MANY_PIN &MainItems[0],
static const MenuBarItem* const ManyPins[] =
    {
        MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN
            MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN
                MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN
                    MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN
                        MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN MANY_PIN};
#undef MANY_PIN

static Menubar makeMenubar(void)
{
    Menubar bar;

    menubar_init(&bar, Categories, COUNT_OF(Categories), Mouse, Hits, CheckIcon);

    // F10 opens the host's home menu; APP here, as the studio's TIC-80 one.
    bar.home = AppMain;

    return bar;
}

// A press and a release on the same point, as processMouseStates would leave
// them for the frame the button comes up.
static void clickAt(tic_mem* tic, s32 x, s32 y)
{
    memset(Mouse, 0, sizeof Mouse);

    Mouse[tic_mouse_left].start = (tic_point){x, y};
    Mouse[tic_mouse_left].end = (tic_point){x, y};
    Mouse[tic_mouse_left].click = true;

    // The raw input is in the full window; tic_api_mouse subtracts the margins.
    tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
}

static void moveTo(tic_mem* tic, s32 x, s32 y)
{
    memset(Mouse, 0, sizeof Mouse);

    tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
}

static void step(Menubar* bar, tic_mem* tic)
{
    menubar_step(bar, tic, Mode);
}

static void draw(Menubar* bar, tic_mem* tic)
{
    menubar_draw(bar, tic, Mode, &Tiles);
}

// One key press for one frame: set it, step the module, clear it again. The
// core's previous state stays empty, so every press reads as a fresh one.
static void keyPress(Menubar* bar, tic_mem* tic, tic_key key)
{
    tic->ram->input.keyboard.keys[0] = key;
    step(bar, tic);
    memset(&tic->ram->input.keyboard, 0, sizeof tic->ram->input.keyboard);
}

// A key press whose handler the draw runs, as the studio does.
static void keyDraw(Menubar* bar, tic_mem* tic, tic_key key)
{
    keyPress(bar, tic, key);
    draw(bar, tic);
}

// The keyboard cases start from a still pointer out of the bar's way.
static void still(Menubar* bar, tic_mem* tic)
{
    moveTo(tic, 200, 130);
    step(bar, tic);
}

static u8 screenAt(tic_mem* tic, s32 x, s32 y)
{
    return tic_tool_peek4(tic->ram->vram.screen.data, y * TIC80_WIDTH + x);
}

static bool anyInk(tic_mem* tic, s32 x, s32 y, s32 w, s32 h)
{
    for(s32 j = y; j < y + h; j++)
        for(s32 i = x; i < x + w; i++)
            if(screenAt(tic, i, j) != tic_color_white)
                return true;

    return false;
}

static bool hasColor(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, u8 color)
{
    for(s32 j = y; j < y + h; j++)
        for(s32 i = x; i < x + w; i++)
            if(screenAt(tic, i, j) == color)
                return true;

    return false;
}

// The APP popup's rows: its title is three characters wide, so the popup
// starts at x 2, and every item is seven pixels tall under a one-pixel border
// and a two-pixel pad.
enum
{
    PopupX = 2,
    PopupY = MENUBAR_HEIGHT,
    ItemX = PopupX + 1,
    FirstItemY = PopupY + 1 + 2,
};

static s32 itemY(s32 index)
{
    return FirstItemY + index * MENUBAR_HEIGHT;
}

static void testOpenAndDispatch(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    moveTo(tic, 200, 130);
    step(&bar, tic);
    assert(bar.open == -1);

    // The title click opens the popup and is consumed.
    clickAt(tic, 10, 3);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(!Mouse[tic_mouse_left].click);

    // An item click closes the menu, consumes the click, and queues the
    // handler; the bar runs it on the draw.
    clickAt(tic, ItemX + 5, itemY(0) + 3);
    step(&bar, tic);

    assert(bar.open == -1);
    assert(!Mouse[tic_mouse_left].click);
    assert(Hits[One] == 0);

    draw(&bar, tic);
    assert(Hits[One] == 1);

    puts("open and dispatch ok");
}

static void testToggleStaysOpen(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // A checked item runs its handler but leaves the popup open.
    clickAt(tic, ItemX + 5, itemY(6) + 3);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(!Mouse[tic_mouse_left].click);
    assert(Hits[ToggleOn] == 0);

    draw(&bar, tic);
    assert(Hits[ToggleOn] == 1);
    assert(bar.open == AppMain);

    puts("toggle stays open ok");
}

static void testDisabled(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // A disabled item takes its click and does nothing; the menu stays up.
    clickAt(tic, ItemX + 5, itemY(3) + 3);
    step(&bar, tic);

    assert(!Mouse[tic_mouse_left].click);
    assert(bar.open == AppMain);
    assert(Hits[Disabled] == 0);

    puts("disabled ok");
}

static void testClickAway(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // Outside closes the menu and never reaches the editor.
    clickAt(tic, 200, 130);
    step(&bar, tic);

    assert(bar.open == -1);
    assert(!Mouse[tic_mouse_left].click);

    puts("click away ok");
}

static void testHidden(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    // HIDDEN sits first in the list; the bar packs APP at the left edge.
    clickAt(tic, 3, 3);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(bar.open != AppHidden);

    draw(&bar, tic);
    assert(Hits[HiddenAction] == 0);

    puts("hidden ok");
}

static void testTitleSwitch(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // An open menu follows the pointer to another title, with no click.
    moveTo(tic, 210, 3);
    step(&bar, tic);
    assert(bar.open == AppRight);

    moveTo(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // A click on another title switches and is consumed.
    clickAt(tic, 210, 3);
    step(&bar, tic);
    assert(bar.open == AppRight);
    assert(!Mouse[tic_mouse_left].click);

    puts("title switch ok");
}

static void testClickConsumption(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    // The click that opens the bar also clears a double-click window.
    clickAt(tic, 10, 3);
    Mouse[tic_mouse_left].dbl.click = true;
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(!Mouse[tic_mouse_left].click);
    assert(!Mouse[tic_mouse_left].dbl.click);

    // The click that closes it outside does the same.
    clickAt(tic, 200, 130);
    Mouse[tic_mouse_left].dbl.click = true;
    step(&bar, tic);

    assert(bar.open == -1);
    assert(!Mouse[tic_mouse_left].click);
    assert(!Mouse[tic_mouse_left].dbl.click);

    puts("click consumption ok");
}

static void testGroupStaysOpen(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 210, 3);
    step(&bar, tic);
    assert(bar.open == AppRight);

    // The category marks a selection group, so its item keeps it open.
    clickAt(tic, 180, 13);
    step(&bar, tic);

    assert(bar.open == AppRight);
    assert(!Mouse[tic_mouse_left].click);
    assert(Hits[Two] == 0);

    draw(&bar, tic);
    assert(Hits[Two] == 1);

    puts("group stays open ok");
}

static void testAutohide(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    bar.autohide = true;

    // Away from the top, the bar slides out.
    moveTo(tic, 200, 100);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == -MENUBAR_HEIGHT);

    // Back in the top strip, it slides in.
    moveTo(tic, 10, 0);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == 0);

    // An open menu keeps it shown.
    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    moveTo(tic, 200, 100);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == 0);

    puts("autohide ok");
}

static void testPartialBar(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    bar.y = -3;

    // The title rows follow the bar: what is still on screen can be clicked.
    clickAt(tic, 10, 0);
    step(&bar, tic);

    assert(bar.y == -1);
    assert(bar.open == AppMain);

    draw(&bar, tic);
    assert(screenAt(tic, 0, 0) == tic_color_white);

    puts("partial bar ok");
}

static void testPointerOwnership(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    // No bar on screen: the pointer is the editor's.
    bar.mode = MENUBAR_NO_MODE;
    moveTo(tic, 10, 3);
    assert(!menubar_owns_pointer(&bar, tic));

    // Over the bar and below it.
    moveTo(tic, 10, 3);
    step(&bar, tic);
    assert(menubar_owns_pointer(&bar, tic));

    moveTo(tic, 10, 20);
    step(&bar, tic);
    assert(!menubar_owns_pointer(&bar, tic));

    // An open popup owns the pointer everywhere.
    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    moveTo(tic, 200, 130);
    step(&bar, tic);
    assert(menubar_owns_pointer(&bar, tic));

    puts("pointer ownership ok");
}

static void testChecked(tic_mem* tic)
{
    Menubar bar = makeMenubar();

    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);

    // Away from the popup, so neither toggle is hovered.
    moveTo(tic, 200, 130);
    draw(&bar, tic);

    // The ON and OFF rows have no icon, so the host's check icon is drawn in
    // the icon column: dark when on, light when off.
    assert(screenAt(tic, ItemX + 3, itemY(6)) == tic_color_dark_grey);
    assert(screenAt(tic, ItemX + 3, itemY(7)) == tic_color_light_grey);

    // The checked circle carries the 3x3 green LED and its lit corner; the
    // off one stays a plain circle.
    assert(screenAt(tic, ItemX + 5, itemY(6) + 2) == tic_color_green);
    assert(screenAt(tic, ItemX + 7, itemY(6) + 2) == tic_color_white);
    assert(screenAt(tic, ItemX + 7, itemY(7) + 2) == tic_color_light_grey);

    puts("checked ok");
}

static void openSubmenu(tic_mem* tic, Menubar* bar)
{
    clickAt(tic, 10, 3);
    step(bar, tic);
    assert(bar->open == AppMain);

    moveTo(tic, ItemX + 5, itemY(4) + 3);
    step(bar, tic);

    assert(bar->sub == &SubAMenu);
    assert(bar->subItem == 4);
}

static void testSubmenuHover(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // The submenu opens on hover, with no click and no delay.
    assert(!Mouse[tic_mouse_left].click);

    // The row under the pointer is the submenu's selected row, so a host can
    // read the hover from the bar; the keyboard case starts from the same field.
    moveTo(tic, 89, itemY(4) + 3);
    step(&bar, tic);

    assert(bar.sub == &SubAMenu);
    assert(bar.subSelect == 0);

    puts("submenu hover ok");
}

static void testSubmenuFollow(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // The submenu overlaps the parent's edge, so the border column is inside
    // it: crossing slowly does not close it.
    moveTo(tic, PopupX + 77, itemY(4) + 3);
    step(&bar, tic);
    assert(bar.sub == &SubAMenu);

    // Into the submenu: it stays open.
    moveTo(tic, 89, itemY(4) + 3);
    step(&bar, tic);
    assert(bar.sub == &SubAMenu);

    // In the popup, away from the owner and the submenu, it closes at once.
    moveTo(tic, ItemX + 5, itemY(3) + 3);
    step(&bar, tic);
    assert(bar.sub == NULL);
    assert(bar.open == AppMain);

    puts("submenu follow ok");
}

static void testSubmenuSticky(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // Out of the popup into the void, the submenu stays: only a hover inside
    // the popup takes it away.
    moveTo(tic, 200, 130);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(bar.sub == &SubAMenu);
    assert(bar.subItem == 4);

    // A click there still closes the menu, submenu and all.
    clickAt(tic, 200, 130);
    step(&bar, tic);

    assert(bar.open == -1);
    assert(bar.sub == NULL);
    assert(!Mouse[tic_mouse_left].click);

    puts("submenu sticky ok");
}

static void testSubmenuSwitch(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // Another owner with a submenu takes over at once.
    moveTo(tic, ItemX + 5, itemY(5) + 3);
    step(&bar, tic);

    assert(bar.sub == &SubBMenu);
    assert(bar.subItem == 5);

    puts("submenu switch ok");
}

static void testOwnerClick(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // Clicking the item that opened the submenu keeps it open.
    clickAt(tic, ItemX + 5, itemY(4) + 3);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(bar.sub == &SubAMenu);
    assert(!Mouse[tic_mouse_left].click);

    puts("owner click ok");
}

static void testSubmenuToggleStaysOpen(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // The third submenu item is a toggle: it keeps the submenu open.
    clickAt(tic, 89, itemY(4) + 17);
    step(&bar, tic);

    assert(bar.open == AppMain);
    assert(bar.sub == &SubAMenu);
    assert(!Mouse[tic_mouse_left].click);
    assert(Hits[SubAItem2] == 0);

    draw(&bar, tic);
    assert(Hits[SubAItem2] == 1);
    assert(bar.sub == &SubAMenu);

    puts("submenu toggle stays open ok");
}

static void testSubmenuDispatch(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    openSubmenu(tic, &bar);

    // The first submenu item: its row starts at the owner's y.
    clickAt(tic, 89, itemY(4) + 3);
    step(&bar, tic);

    assert(bar.open == -1);
    assert(bar.sub == NULL);
    assert(!Mouse[tic_mouse_left].click);
    assert(Hits[SubAItem0] == 0);

    draw(&bar, tic);
    assert(Hits[SubAItem0] == 1);

    puts("submenu dispatch ok");
}

static void testKeyboardFocus(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);

    assert(!bar.keyboard);
    assert(bar.focus == -1);
    assert(!bar.captured);

    // Ctrl+F10 is the bytebattle toggle, not the bar's.
    tic->ram->input.keyboard.keys[0] = tic_key_ctrl;
    tic->ram->input.keyboard.keys[1] = tic_key_f10;
    step(&bar, tic);
    memset(&tic->ram->input.keyboard, 0, sizeof tic->ram->input.keyboard);

    assert(!bar.keyboard);
    assert(!bar.captured);

    // F10 opens the home menu on its first selectable row.
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.keyboard);
    assert(bar.open == AppMain);
    assert(bar.focus == AppMain);
    assert(bar.select == 0);
    assert(bar.captured);

    // A second F10 closes it and leaves the bar; that frame is still the
    // bar's, so the editor does not see the F10 that closed the menu.
    keyPress(&bar, tic, tic_key_f10);
    assert(!bar.keyboard);
    assert(bar.open == -1);
    assert(bar.focus == -1);
    assert(bar.captured);

    step(&bar, tic);
    assert(!bar.captured);

    puts("keyboard focus ok");
}

static void testKeyboardTitles(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);

    // ESC closes the popup and leaves the title row focused, as the second
    // ESC level.
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.open == -1);
    assert(bar.keyboard);
    assert(bar.focus == AppMain);

    keyPress(&bar, tic, tic_key_right);
    assert(bar.focus == AppRight);

    // The ends wrap: Right on the last title reaches the first one, HIDDEN
    // is never a stop.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.focus == AppMain);

    keyPress(&bar, tic, tic_key_left);
    assert(bar.focus == AppRight);

    keyPress(&bar, tic, tic_key_left);
    assert(bar.focus == AppMain);

    // Home and End reach the visible ends.
    keyPress(&bar, tic, tic_key_end);
    assert(bar.focus == AppRight);
    keyPress(&bar, tic, tic_key_home);
    assert(bar.focus == AppMain);

    puts("keyboard titles ok");
}

static void testKeyboardOpen(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);

    // F10 opens on the first selectable row.
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.open == AppMain);
    assert(bar.select == 0);

    // ESC leaves the bar focused; Up opens on the last selectable row.
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.open == -1);
    assert(bar.keyboard);

    keyPress(&bar, tic, tic_key_up);
    assert(bar.open == AppMain);
    assert(bar.select == 7);

    // Focus again, then a group opens on its marked row, not the first.
    keyPress(&bar, tic, tic_key_escape);
    keyPress(&bar, tic, tic_key_escape);
    assert(!bar.keyboard);

    keyPress(&bar, tic, tic_key_f10);
    keyPress(&bar, tic, tic_key_escape);
    keyPress(&bar, tic, tic_key_right);
    assert(bar.focus == AppRight);

    keyPress(&bar, tic, tic_key_down);
    assert(bar.open == AppRight);
    assert(bar.select == 1);

    puts("keyboard open ok");
}

static void testKeyboardRows(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.select == 0);

    // The separator (2) and the disabled row (3) are skipped.
    keyPress(&bar, tic, tic_key_down);
    assert(bar.select == 1);
    keyPress(&bar, tic, tic_key_down);
    assert(bar.select == 4);
    keyPress(&bar, tic, tic_key_down);
    assert(bar.select == 5);

    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 4);
    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 1);
    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 0);

    // Wrapping lands on OFF, past the disabled rows again.
    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 7);

    // Home and End reach the ends, skipping the separator and disabled row.
    keyPress(&bar, tic, tic_key_home);
    assert(bar.select == 0);
    keyPress(&bar, tic, tic_key_end);
    assert(bar.select == 7);

    puts("keyboard rows ok");
}

static void testKeyboardDispatch(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.open == AppMain && bar.select == 0);

    // Enter runs the row, closes the menu and drops focus; the frame still
    // belongs to the bar, so the editor will not see that Enter.
    s32 before = Hits[One];
    keyPress(&bar, tic, tic_key_return);

    assert(bar.open == -1);
    assert(!bar.keyboard);
    assert(bar.captured);
    assert(Hits[One] == before);

    draw(&bar, tic);
    assert(Hits[One] == before + 1);

    // The next frame is the editor's again.
    step(&bar, tic);
    assert(!bar.captured);

    puts("keyboard dispatch ok");
}

static void testKeyboardToggle(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 7);
    keyPress(&bar, tic, tic_key_up);
    assert(bar.select == 6);

    // A toggle runs but leaves the menu focused and open.
    s32 before = Hits[ToggleOn];
    keyDraw(&bar, tic, tic_key_return);
    assert(Hits[ToggleOn] == before + 1);
    assert(bar.open == AppMain);
    assert(bar.keyboard);
    assert(bar.captured);

    // Space does the same.
    keyDraw(&bar, tic, tic_key_space);
    assert(Hits[ToggleOn] == before + 2);

    puts("keyboard toggle ok");
}

static void testKeyboardGroup(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    keyPress(&bar, tic, tic_key_right);
    assert(bar.open == AppRight && bar.select == 1);

    // A selection group runs its item and stays open.
    s32 before = Hits[One];
    keyDraw(&bar, tic, tic_key_return);
    assert(Hits[One] == before + 1);
    assert(bar.open == AppRight);
    assert(bar.keyboard);

    puts("keyboard group ok");
}

static void testKeyboardAdjacent(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.open == AppMain);

    // The popup stays open while the keyboard switches menus. RIGHT opens on
    // its marked row.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.open == AppRight);
    assert(bar.select == 1);
    assert(bar.keyboard);

    // Right on the last menu wraps to the first.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.open == AppMain);
    assert(bar.select == 0);

    // Left on the first menu wraps to the last.
    keyPress(&bar, tic, tic_key_left);
    assert(bar.open == AppRight);
    assert(bar.select == 1);
    assert(bar.keyboard);

    keyPress(&bar, tic, tic_key_left);
    assert(bar.open == AppMain);
    assert(bar.select == 0);

    // From a row, Left still wraps to the last menu.
    keyPress(&bar, tic, tic_key_down);
    assert(bar.select == 1);
    keyPress(&bar, tic, tic_key_left);
    assert(bar.open == AppRight);
    assert(bar.select == 1);

    puts("keyboard adjacent ok");
}

static void testKeyboardSubmenu(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    keyPress(&bar, tic, tic_key_down);
    keyPress(&bar, tic, tic_key_down);
    assert(bar.select == 4);

    // Right opens the submenu on its first selectable row.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.sub == &SubAMenu);
    assert(bar.subItem == 4);
    assert(bar.subSelect == 0);
    assert(bar.open == AppMain);

    keyPress(&bar, tic, tic_key_down);
    assert(bar.subSelect == 1);

    // Left returns to the parent row, with the popup still open.
    keyPress(&bar, tic, tic_key_left);
    assert(bar.sub == NULL);
    assert(bar.select == 4);
    assert(bar.open == AppMain);
    assert(bar.keyboard);

    // Running an item from the submenu closes the menu.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.sub == &SubAMenu);

    s32 before = Hits[SubAItem0];
    keyPress(&bar, tic, tic_key_return);
    assert(bar.open == -1);
    assert(bar.sub == NULL);
    assert(!bar.keyboard);

    draw(&bar, tic);
    assert(Hits[SubAItem0] == before + 1);

    puts("keyboard submenu ok");
}

static void testKeyboardEscape(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    keyPress(&bar, tic, tic_key_down);
    keyPress(&bar, tic, tic_key_down);
    keyPress(&bar, tic, tic_key_right);
    assert(bar.sub == &SubAMenu);

    // Submenu -> popup, keeping the focus and the parent row.
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.sub == NULL);
    assert(bar.open == AppMain);
    assert(bar.keyboard);
    assert(bar.select == 4);
    assert(bar.escaped);

    // Popup -> bar focus.
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.open == -1);
    assert(bar.keyboard);
    assert(bar.focus == AppMain);
    assert(bar.captured);

    // Bar focus -> editor. The ESC frame is still the bar's, so the editor
    // does not see it either.
    keyPress(&bar, tic, tic_key_escape);
    assert(!bar.keyboard);
    assert(bar.focus == -1);
    assert(bar.captured);

    step(&bar, tic);
    assert(!bar.captured);

    puts("keyboard escape ok");
}

static void testKeyboardCapture(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    assert(!bar.captured);

    // A mouse-opened popup captures keys too, without keyboard focus.
    clickAt(tic, 10, 3);
    step(&bar, tic);
    assert(bar.open == AppMain);
    assert(!bar.keyboard);
    assert(bar.captured);

    // The first navigation key takes the menu over from outside the rows; a
    // page key starts on the first row.
    keyPress(&bar, tic, tic_key_pagedown);
    assert(bar.keyboard);
    assert(bar.focus == AppMain);
    assert(bar.select == 0);

    puts("keyboard capture ok");
}

static void testKeyboardAutohide(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    bar.autohide = true;

    moveTo(tic, 200, 100);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == -MENUBAR_HEIGHT);

    // F10 snaps the hidden bar in and keeps it there.
    tic->ram->input.keyboard.keys[0] = tic_key_f10;
    step(&bar, tic);
    memset(&tic->ram->input.keyboard, 0, sizeof tic->ram->input.keyboard);

    assert(bar.keyboard);
    assert(bar.open == AppMain);
    assert(bar.y == 0);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == 0);

    // Popup -> bar focus -> editor; then it slides away again.
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.keyboard);
    keyPress(&bar, tic, tic_key_escape);
    assert(!bar.keyboard);

    for(s32 i = 0; i < MENUBAR_HEIGHT + 1; i++)
        step(&bar, tic);

    assert(bar.y == -MENUBAR_HEIGHT);

    puts("keyboard autohide ok");
}

static void testKeyboardPointer(tic_mem* tic)
{
    Menubar bar = makeMenubar();
    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.keyboard && bar.open == AppMain);

    // A mouse move hands the bar back to the pointer; its menu stays open.
    moveTo(tic, 100, 100);
    step(&bar, tic);

    assert(!bar.keyboard);
    assert(bar.focus == -1);
    assert(bar.open == AppMain);

    // Pointer control behaves as before: a click on the open title closes it.
    clickAt(tic, 10, 3);
    step(&bar, tic);

    assert(bar.open == -1);
    assert(!bar.keyboard);

    puts("keyboard pointer handoff ok");
}

static void testKeyboardJumpKeys(tic_mem* tic)
{
    MenuBarCategory cats[] =
        {
            {"LONG", LongItems, COUNT_OF(LongItems), false, 0, -1, 0, false},
        };

    Menubar bar;
    menubar_init(&bar, cats, COUNT_OF(cats), Mouse, Hits, CheckIcon);
    bar.home = 0;

    still(&bar, tic);
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.open == 0 && bar.select == 0);

    // A page is five selectable rows; the second one clamps at the end.
    keyPress(&bar, tic, tic_key_pagedown);
    assert(bar.select == 5);
    keyPress(&bar, tic, tic_key_pagedown);
    assert(bar.select == 7);

    keyPress(&bar, tic, tic_key_pageup);
    assert(bar.select == 2);
    keyPress(&bar, tic, tic_key_pageup);
    assert(bar.select == 0);

    // Home and End.
    keyPress(&bar, tic, tic_key_end);
    assert(bar.select == 7);
    keyPress(&bar, tic, tic_key_home);
    assert(bar.select == 0);

    // The same keys inside the submenu.
    keyPress(&bar, tic, tic_key_right);
    assert(bar.sub == &LongSubMenu);
    assert(bar.subSelect == 0);

    keyPress(&bar, tic, tic_key_pagedown);
    assert(bar.subSelect == 5);
    keyPress(&bar, tic, tic_key_end);
    assert(bar.subSelect == 7);
    keyPress(&bar, tic, tic_key_pageup);
    assert(bar.subSelect == 2);
    keyPress(&bar, tic, tic_key_home);
    assert(bar.subSelect == 0);

    // Left returns to the owner row.
    keyPress(&bar, tic, tic_key_left);
    assert(bar.sub == NULL);
    assert(bar.select == 0);

    puts("keyboard jump keys ok");
}

static void testCompact(tic_mem* tic)
{
    MenuBarCategory cats[] =
        {
            {"APPS", TabItems, COUNT_OF(TabItems), false, 0, One, 0, false},
            {"MENU", MainItems, COUNT_OF(MainItems), false, CheckIcon, -1, 0, false},
            {"TIC", RightItems, COUNT_OF(RightItems), true, 0, -1, 0, false},
        };

    Menubar bar;
    menubar_init(&bar, cats, COUNT_OF(cats), Mouse, Hits, CheckIcon);
    bar.compact = true;
    bar.tabs = &cats[0];
    bar.more = &cats[1];
    bar.tabIcon = CheckIcon;
    bar.caption = "TEST EDITOR";
    bar.home = 2;
    bar.buttons = CompactPins;
    bar.buttonCount = COUNT_OF(CompactPins);
    bar.buttonSplit = 1;

    // One inked pixel in the first tab's icon: the white glyph, its black
    // shadow and the grey tab behind it are each visible.
    tic_tool_poke4(Tiles.data[TabItems[0].icon].data, 0, 1);

    still(&bar, tic);

    // A tab click runs its app item; the tabs lead on the left.
    s32 before = Hits[One];
    clickAt(tic, 2 + 3, 3);
    step(&bar, tic);
    draw(&bar, tic);
    assert(Hits[One] == before + 1);

    // The active tab: the app icon in white, its black shadow a row down, and
    // the grey tab glyph behind them.
    assert(screenAt(tic, 2, 0) == tic_color_white);
    assert(screenAt(tic, 2, 1) == tic_color_black);
    assert(screenAt(tic, 3, 1) == tic_color_grey);

    // A pin click runs its menu item, after the tabs and the hint.
    before = Hits[One];
    clickAt(tic, 186, 3);
    step(&bar, tic);
    draw(&bar, tic);
    assert(Hits[One] == before + 1);

    // The mode group has its own items.
    before = Hits[ToggleOn];
    clickAt(tic, 197, 3);
    step(&bar, tic);
    draw(&bar, tic);
    assert(Hits[ToggleOn] == before + 1);

    // The idle hint is the full app name, right after the tabs, in grey.
    moveTo(tic, 120, 130);
    step(&bar, tic);
    draw(&bar, tic);
    assert(anyInk(tic, 20, 1, 70, 6));
    assert(hasColor(tic, 20, 1, 66, 6, tic_color_grey));
    assert(!hasColor(tic, 20, 1, 66, 6, tic_color_dark_grey));

    // The editor's own hover text fills the hint when no bar control is
    // hovered, and it is a tooltip: darker than the app's name.
    bar.tooltip = "LEFT STEREO";
    moveTo(tic, 120, 130);
    step(&bar, tic);
    draw(&bar, tic);
    assert(hasColor(tic, 20, 1, 66, 6, tic_color_dark_grey));
    bar.tooltip = NULL;

    // Hovering a pin replaces it with the label and the accelerator, darker.
    moveTo(tic, 186, 3);
    step(&bar, tic);
    draw(&bar, tic);
    assert(hasColor(tic, 20, 1, 48, 6, tic_color_dark_grey));
    assert(!anyInk(tic, 70, 1, 16, 6));

    // Hovering the MENU icon names it too.
    moveTo(tic, 208, 3);
    step(&bar, tic);
    draw(&bar, tic);
    assert(hasColor(tic, 20, 1, 24, 6, tic_color_dark_grey));
    assert(!anyInk(tic, 46, 1, 40, 6));

    // Keyboard: the tabs category is not a focus stop.
    keyPress(&bar, tic, tic_key_f10);
    assert(bar.open == 2);
    keyPress(&bar, tic, tic_key_escape);
    assert(bar.keyboard);
    assert(bar.focus == 2);
    keyPress(&bar, tic, tic_key_left);
    assert(bar.focus == 1);
    keyPress(&bar, tic, tic_key_home);
    assert(bar.focus == 1);
    keyPress(&bar, tic, tic_key_right);
    assert(bar.focus == 2);
    keyPress(&bar, tic, tic_key_escape);
    assert(!bar.keyboard);

    // An open menu does not follow the pointer to another title here; a click
    // on it still switches, and is consumed.
    clickAt(tic, 208, 3);
    step(&bar, tic);
    assert(bar.open == 1);

    moveTo(tic, 230, 3);
    step(&bar, tic);
    assert(bar.open == 1);

    clickAt(tic, 230, 3);
    step(&bar, tic);
    assert(bar.open == 2);
    assert(!Mouse[tic_mouse_left].click);

    keyPress(&bar, tic, tic_key_escape);
    assert(bar.open == -1);

    // Too many pins: the visible ones stop before the MENU title, and a click
    // where the next pin would be opens MENU instead of running it.
    MenuBarCategory manyCats[] =
        {
            {"APPS", TabItems, COUNT_OF(TabItems), false, 0, One, 0, false},
            {"MENU", MainItems, COUNT_OF(MainItems), false, CheckIcon, -1, 0, false},
            {"TIC", RightItems, COUNT_OF(RightItems), true, 0, -1, 0, false},
        };

    Menubar many;
    menubar_init(&many, manyCats, COUNT_OF(manyCats), Mouse, Hits, CheckIcon);
    many.compact = true;
    many.tabs = &manyCats[0];
    many.more = &manyCats[1];
    many.buttons = ManyPins;
    many.buttonCount = COUNT_OF(ManyPins);
    still(&many, tic);

    before = Hits[One];
    clickAt(tic, 208, 3);
    step(&many, tic);
    assert(many.open == 1);
    assert(Hits[One] == before);

    // A narrow hint drops the accelerator instead of cutting it: seven
    // characters fit, and the pin's label is three.
    Menubar medium;
    menubar_init(&medium, manyCats, COUNT_OF(manyCats), Mouse, Hits, CheckIcon);
    medium.compact = true;
    medium.tabs = &manyCats[0];
    medium.more = &manyCats[1];
    medium.buttons = ManyPins;
    medium.buttonCount = 19;
    still(&medium, tic);
    moveTo(tic, 71, 3);
    step(&medium, tic);
    draw(&medium, tic);
    assert(anyInk(tic, 20, 1, 18, 6));
    assert(!anyInk(tic, 38, 1, 26, 6));

    // The order is the menu's: titles leading (BANK), the pins, then MENU.
    static const MenuBarItem BankItems[] =
        {
            {"BANK 0", NULL, hit, One, true, false, 0, 0, NULL, -1},
            {"BANK 1", NULL, hit, Two, true, false, 0, 0, NULL, -1},
        };

    MenuBarCategory orderCats[] =
        {
            {"APPS", TabItems, COUNT_OF(TabItems), false, 0, One, 0, false},
            {"BANK", BankItems, COUNT_OF(BankItems), false, 0, 1, 0, false},
            {"MENU", MainItems, COUNT_OF(MainItems), false, CheckIcon, -1, 0, false},
            {"TIC", RightItems, COUNT_OF(RightItems), true, 0, -1, 0, false},
        };

    Menubar orderBar;
    menubar_init(&orderBar, orderCats, COUNT_OF(orderCats), Mouse, Hits, CheckIcon);
    orderBar.compact = true;
    orderBar.tabs = &orderCats[0];
    orderBar.more = &orderCats[2];
    orderBar.buttons = CompactPins;
    orderBar.buttonCount = COUNT_OF(CompactPins);
    orderBar.buttonSplit = 1;
    still(&orderBar, tic);

    clickAt(tic, 155, 3);
    step(&orderBar, tic);
    assert(orderBar.open == 1);

    before = Hits[One];
    clickAt(tic, 186, 3);
    step(&orderBar, tic);
    draw(&orderBar, tic);
    assert(Hits[One] == before + 1);

    clickAt(tic, 208, 3);
    step(&orderBar, tic);
    assert(orderBar.open == 2);

    // The BANK stop names itself by its selected item.
    moveTo(tic, 155, 3);
    step(&orderBar, tic);
    draw(&orderBar, tic);
    assert(anyInk(tic, 50, 1, 6, 6));

    // An icon-only title is a pin-sized cell: the hover fill spans seven
    // pixels, so its last column is eight and the ninth stays white.
    enum
    {
        IconOnly = 10
    };

    tic_tool_poke4(Tiles.data[IconOnly].data, 0, 1);

    MenuBarCategory iconCats[] =
        {
            {"", NULL, 0, false, IconOnly, -1, 0, false},
            {"TIC", RightItems, COUNT_OF(RightItems), true, 0, -1, 0, false},
        };

    Menubar iconBar;
    menubar_init(&iconBar, iconCats, COUNT_OF(iconCats), Mouse, Hits, CheckIcon);
    still(&iconBar, tic);
    moveTo(tic, 3, 3);
    step(&iconBar, tic);
    draw(&iconBar, tic);
    assert(screenAt(tic, 2, 0) == tic_color_dark_grey);
    assert(screenAt(tic, 8, 1) == tic_color_light_grey);
    assert(screenAt(tic, 9, 1) == tic_color_white);

    // The home stop becomes a pin-sized icon in compact mode; hovering it names
    // it in the hint, accelerator included.
    MenuBarCategory homeCats[] =
        {
            {"APPS", TabItems, COUNT_OF(TabItems), false, 0, One, 0, false},
            {"MENU", MainItems, COUNT_OF(MainItems), false, 0, -1, 0, false},
            {"TIC-80", RightItems, COUNT_OF(RightItems), true, CheckIcon, -1, 0, false},
        };

    Menubar homeBar;
    menubar_init(&homeBar, homeCats, COUNT_OF(homeCats), Mouse, Hits, CheckIcon);
    homeBar.compact = true;
    homeBar.tabs = &homeCats[0];
    homeBar.caption = "TIP";
    homeBar.home = 2;
    still(&homeBar, tic);
    draw(&homeBar, tic);
    assert(!anyInk(tic, 40, 1, 60, 6));
    assert(screenAt(tic, 239, 0) == tic_color_white);

    moveTo(tic, 234, 3);
    step(&homeBar, tic);
    draw(&homeBar, tic);
    assert(anyInk(tic, 40, 1, 60, 6));

    // A separator ends the tabs: the icon after it is a menu row, and its
    // cell runs nothing.
    static const MenuBarItem SplitTabItems[] =
        {
            {"TAB ONE", NULL, hit, One, true, false, 1, 0, NULL, -1},
            {"", NULL, NULL, 0, true, true, 0, 0, NULL, -1},
            {"TAB TWO", NULL, hit, Two, true, false, 2, 0, NULL, -1},
        };

    MenuBarCategory splitCats[] =
        {
            {"APPS", SplitTabItems, COUNT_OF(SplitTabItems), false, 0, One, 0, false},
            {"MENU", MainItems, COUNT_OF(MainItems), false, CheckIcon, -1, 0, false},
            {"TIC", RightItems, COUNT_OF(RightItems), true, 0, -1, 0, false},
        };

    Menubar splitBar;
    menubar_init(&splitBar, splitCats, COUNT_OF(splitCats), Mouse, Hits, CheckIcon);
    splitBar.compact = true;
    splitBar.tabs = &splitCats[0];
    splitBar.more = &splitCats[1];
    splitBar.caption = "TEST";
    still(&splitBar, tic);

    before = Hits[One];
    clickAt(tic, 3, 3);
    step(&splitBar, tic);
    draw(&splitBar, tic);
    assert(Hits[One] == before + 1);

    before = Hits[Two];
    clickAt(tic, 10, 3);
    step(&splitBar, tic);
    draw(&splitBar, tic);
    assert(Hits[Two] == before);

    puts("compact separator ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    // The check circle's tile is the only one the test needs inked.
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(Tiles.data[CheckIcon].data, i, 1);

    testOpenAndDispatch(tic);
    testToggleStaysOpen(tic);
    testDisabled(tic);
    testClickAway(tic);
    testHidden(tic);
    testTitleSwitch(tic);
    testClickConsumption(tic);
    testGroupStaysOpen(tic);
    testAutohide(tic);
    testPartialBar(tic);
    testPointerOwnership(tic);
    testChecked(tic);
    testSubmenuHover(tic);
    testSubmenuFollow(tic);
    testSubmenuSticky(tic);
    testSubmenuSwitch(tic);
    testOwnerClick(tic);
    testSubmenuToggleStaysOpen(tic);
    testSubmenuDispatch(tic);
    testKeyboardFocus(tic);
    testKeyboardTitles(tic);
    testKeyboardOpen(tic);
    testKeyboardRows(tic);
    testKeyboardJumpKeys(tic);
    testKeyboardDispatch(tic);
    testKeyboardToggle(tic);
    testKeyboardGroup(tic);
    testKeyboardAdjacent(tic);
    testKeyboardSubmenu(tic);
    testKeyboardEscape(tic);
    testKeyboardCapture(tic);
    testKeyboardAutohide(tic);
    testKeyboardPointer(tic);
    testCompact(tic);

    tic_core_close(tic);
    puts("menubar ok");
    return 0;
}
