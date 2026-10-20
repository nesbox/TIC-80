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
};

static MenuBarCategory Categories[] =
    {
        // First, so a click on the bar would land on it if it were not skipped.
        {"HIDDEN", HiddenItems, COUNT_OF(HiddenItems), false, 0, -1, 0, true},
        {"APP", MainItems, COUNT_OF(MainItems), false, 0, -1, 0, false},
        {"RIGHT", RightItems, COUNT_OF(RightItems), true, 0, Two, 0, false},
};

static Menubar makeMenubar(void)
{
    Menubar bar;

    menubar_init(&bar, Categories, COUNT_OF(Categories), Mouse, Hits, CheckIcon);

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

static u8 screenAt(tic_mem* tic, s32 x, s32 y)
{
    return tic_tool_peek4(tic->ram->vram.screen.data, y * TIC80_WIDTH + x);
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

    // Away from the owner and the submenu, it closes at once.
    moveTo(tic, ItemX + 5, itemY(3) + 3);
    step(&bar, tic);
    assert(bar.sub == NULL);
    assert(bar.open == AppMain);

    puts("submenu follow ok");
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
    testSubmenuSwitch(tic);
    testOwnerClick(tic);
    testSubmenuToggleStaysOpen(tic);
    testSubmenuDispatch(tic);

    tic_core_close(tic);
    puts("menubar ok");
    return 0;
}
