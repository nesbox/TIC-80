/* The list menu links without the studio: this file builds the widget with the
 * animation, the keyboard, the mouse, the ui and the sound modules and a fake
 * host that fills its view, so a call into studio.c fails to link here rather
 * than appearing quietly in the app build.
 *
 * Target menu-test in cmake/tests.cmake; `ctest -R menu` runs it.
 */
#include "studio/screens/menu.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static tic_mem* Tic;

// The config cart the widget is given: its own buffer, as in the studio, where
// it is not the loaded cart the machine holds.
static tic_cartridge Cart;

static Menu* Menu_;
static MouseState Mouse[3];
static MenuView View;

// The option behind one item, written and read through its own handlers.
static s32 Value;
static s32 Gets;
static s32 Sets;

static s32 HandlerRuns;
static s32 HandlerPos;
static s32 HotkeyRuns;
static s32 BackRuns;

// The platform's own text input, which the keyboard module asks before its
// fallback: the menu never types, so this answers nothing.
bool tic_sys_keyboard_text(char* text)
{
    (void)text;
    return false;
}

static s32 optionGet(void* data)
{
    (void)data;
    Gets++;
    return Value;
}

static void optionSet(void* data, s32 value)
{
    (void)data;
    Sets++;
    Value = value;
}

enum
{
    TextMargin = 2,
    ItemHeight = TIC_FONT_HEIGHT + TextMargin * 2,
};

static void handler(void* data, s32 pos)
{
    (void)data;
    HandlerRuns++;
    HandlerPos = pos;
}

static void hotkeyHandler(void* data, s32 pos)
{
    (void)data;
    HotkeyRuns++;
    HandlerPos = pos;
}

static void backHandler(void* data, s32 pos)
{
    (void)data;
    (void)pos;
    BackRuns++;
}

// A host handler that leaves the menu, as RESUME GAME does: the mode change
// and the view's own update run inside the widget's tick.
static void leaveHandler(void* data, s32 pos)
{
    (void)data;
    (void)pos;
    HandlerRuns++;
    View.active = false;
}

static const char* OptionValues[] = {"OFF", "ON", "AUTO"};

static MenuOption Option = {
    .values = OptionValues,
    .count = COUNT_OF(OptionValues),
    .get = optionGet,
    .set = optionSet,
};

static const MenuItem Items[] =
    {
        {.label = "ONE", .handler = handler},
        {.label = "TWO", .handler = handler},
        {.label = "THREE", .handler = handler},
};

static const MenuItem OptionItems[] =
    {
        {.label = "OPTION", .option = &Option},
};

static const MenuItem HotkeyItems[] =
    {
        {.label = "ONE", .handler = handler},
        {.label = "HOT", .handler = hotkeyHandler, .hotkey = tic_key_q},
};

static const MenuItem LeaveItems[] =
    {
        {.label = "LEAVE", .handler = leaveHandler},
};

// The menu's own frame, in the studio's order: the machine starts the tick, the
// widget runs, the machine closes it.
static void frame(void)
{
    tic_core_tick_start(Tic);
    menu_tick(Menu_);
    tic_core_tick_end(Tic);
}

static void keys(tic_key key)
{
    memset(&Tic->ram->input.keyboard, 0, sizeof Tic->ram->input.keyboard);

    if(key != tic_key_unknown)
        Tic->ram->input.keyboard.keys[0] = key;
}

// One frame with the key down, then released: the menu takes it as a fresh
// press on that frame and as nothing after it.
static void press(tic_key key)
{
    keys(key);
    frame();
    keys(tic_key_unknown);
}

// Enough frames for the longest movie to finish and the widget to be idle.
static void settle(void)
{
    keys(tic_key_unknown);

    for(s32 i = 0; i < 20; i++)
        frame();
}

static void setup(const MenuItem* items, s32 count, MenuItemHandler back)
{
    Value = 0;
    Gets = Sets = 0;
    HandlerRuns = HandlerPos = HotkeyRuns = BackRuns = 0;
    Option.pos = 0;
    Option.width = 0;

    memset(Mouse, 0, sizeof Mouse);
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);
    memset(&Cart, 0, sizeof Cart);

    View = (MenuView){.active = true};

    Menu_ = menu_create(&(MenuDeps){
        .tic = Tic,
        .cart = &Cart,
        .mouse = Mouse,
    });

    menu_set_view(Menu_, &View);
    menu_init(Menu_, items, count, 0, 0, back, NULL);
}

// The screen's own coordinates: the studio hands the machine an absolute
// position, and the relative one shrinks it to a signed byte.
static void pointer(s32 x, s32 y)
{
    Tic->ram->input.mouse.x = x + TIC80_OFFSET_LEFT;
    Tic->ram->input.mouse.y = y + TIC80_OFFSET_TOP;
    Tic->ram->input.mouse.relative = false;
}

static void click(s32 x, s32 y)
{
    pointer(x, y);
    Mouse[tic_mouse_left] = (MouseState){.click = true, .start = {x, y}, .end = {x, y}};
}

// The label rect of item `i` with the widget idle at `pos`, as drawMenu lays it
// out: a plain item is as wide as its label.
static tic_rect itemRect(const char* label, s32 i, s32 pos)
{
    s32 width = (s32)(strlen(label) * TIC_FONT_WIDTH);

    return (tic_rect){
        .x = (TIC80_WIDTH - width) / 2,
        .y = (TIC80_HEIGHT - ItemHeight) / 2 + TextMargin + ItemHeight * (i - pos),
        .w = width,
        .h = TIC_FONT_HEIGHT,
    };
}

// The second bank the widget draws into: the core keeps it out of the machine's
// vram while bank 0 is in front.
static u8* menuBank(void)
{
    return ((tic_core*)Tic)->state.vbank.mem.screen.data;
}

static void fillBank(u8 value)
{
    memset(menuBank(), value, sizeof(tic_screen));
}

static bool bankIs(u8 value)
{
    const u8* bank = menuBank();

    for(s32 i = 0; i < (s32)sizeof(tic_screen); i++)
        if(bank[i] != value)
            return false;

    return true;
}

// The first sound channel's sample: 2 is the system sfx the widget plays.
static s32 channelSample(void)
{
    return ((tic_core*)Tic)->state.sfx.channels[0].index;
}

// The note the channel was started with: sound_play hands on the sample's own
// note and octave, so this tells the deps cart's sample from the machine's.
static s32 channelNote(void)
{
    return ((tic_core*)Tic)->state.sfx.channels[0].note;
}

static void testMovies(void)
{
    setup(Items, COUNT_OF(Items), NULL);

    // The start movie runs first: a direction pressed while it plays moves
    // nothing, so the enter key still runs the first item.
    press(tic_key_down);
    settle();

    // The system sound is the deps cart's sample 2, at the sample's own note
    // and octave: three, plus twelve semitones for the octave.
    Cart.banks[0].sfx.samples.data[2].note = 3;
    Cart.banks[0].sfx.samples.data[2].octave = 1;

    press(tic_key_return);
    assert(channelSample() == 2);
    assert(channelNote() == 3 + 12);
    settle();

    assert(HandlerRuns == 1);
    assert(HandlerPos == 0);

    // The menu drew into its bank, and reset the palette from the deps' cart,
    // which is not the cart the machine holds.
    memset(Cart.banks[0].palette.vbank0.data, 0x3C, sizeof(tic_palette));
    fillBank(0xAA);
    frame();

    assert(!bankIs(0xAA));
    assert(memcmp(((tic_core*)Tic)->state.vbank.mem.palette.data,
                  Cart.banks[0].palette.vbank0.data, sizeof(tic_palette)) == 0);
    assert(memcmp(Cart.banks[0].palette.vbank0.data,
                  Tic->cart.banks[0].palette.vbank0.data, sizeof(tic_palette)) != 0);

    menu_free(Menu_);
    puts("movies ok");
}

static void testNavigation(void)
{
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    press(tic_key_down);
    settle();
    press(tic_key_return);
    settle();

    assert(HandlerRuns == 1);
    assert(HandlerPos == 1);

    // Up twice from the second item: the first, then the last — the list wraps.
    press(tic_key_up);
    settle();
    press(tic_key_up);
    settle();
    press(tic_key_return);
    settle();

    assert(HandlerRuns == 2);
    assert(HandlerPos == 2);

    menu_free(Menu_);
    puts("navigation ok");
}

static void testOption(void)
{
    setup(OptionItems, COUNT_OF(OptionItems), NULL);
    settle();

    // menu_init reads the option once, and the item has no handler.
    assert(Gets == 1);
    assert(Value == 0);

    press(tic_key_right);
    settle();
    assert(Value == 1 && Sets == 1);

    press(tic_key_left);
    settle();
    assert(Value == 0 && Sets == 2);

    // Enter on an option item advances it rather than running a handler.
    press(tic_key_return);
    settle();
    assert(Value == 1 && Sets == 3);

    // Every write is followed by a read through the option's own get.
    assert(Gets == 4);
    assert(HandlerRuns == 0);

    // The right arrow is a click target of its own, past the widest value.
    {
        const s32 label = (s32)strlen("OPTION") * TIC_FONT_WIDTH;
        const s32 value = (s32)strlen("AUTO") * TIC_FONT_WIDTH;
        const s32 width = label + value + 3 * TIC_FONT_WIDTH;

        click((TIC80_WIDTH - width) / 2 + label + value + 2 * TIC_FONT_WIDTH,
              (TIC80_HEIGHT - ItemHeight) / 2 + TextMargin + 2);
        settle();
    }

    assert(Value == 2 && Sets == 4);

    menu_free(Menu_);
    puts("option ok");
}

static void testHotkey(void)
{
    setup(HotkeyItems, COUNT_OF(HotkeyItems), NULL);
    settle();

    press(tic_key_q);
    settle();

    assert(HotkeyRuns == 1);
    assert(HandlerPos == 1);
    assert(HandlerRuns == 0);

    menu_free(Menu_);
    puts("hotkey ok");
}

static void testPointer(void)
{
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    tic_rect rect = itemRect("TWO", 1, 0);

    click(rect.x + rect.w / 2, rect.y + rect.h / 2);
    settle();

    assert(HandlerRuns == 1);
    assert(HandlerPos == 1);

    menu_free(Menu_);
    puts("pointer ok");
}

static void testOwned(void)
{
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    tic_rect rect = itemRect("TWO", 1, 0);
    s32 x = rect.x + rect.w / 2;
    s32 y = rect.y + rect.h / 2;

    // The bar owns the pointer: no hover, and the click is neither the item's
    // nor spent by it.
    View.pointerOwned = true;
    Tic->ram->vram.vars.cursor.sprite = tic_cursor_arrow;
    click(x, y);
    frame();

    assert(HandlerRuns == 0);
    assert(Mouse[tic_mouse_left].click);
    assert(Tic->ram->vram.vars.cursor.sprite == tic_cursor_arrow);

    // The same click lands once the host hands the pointer back.
    View.pointerOwned = false;
    settle();

    assert(HandlerRuns == 1);
    assert(HandlerPos == 1);

    menu_free(Menu_);
    puts("owned ok");
}

static void testActive(void)
{
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    // Another mode ticks the widget: it draws nothing and takes nothing.
    View.active = false;
    fillBank(0xAA);

    tic_rect rect = itemRect("TWO", 1, 0);
    click(rect.x + rect.w / 2, rect.y + rect.h / 2);

    press(tic_key_down);
    press(tic_key_return);
    settle();

    assert(bankIs(0xAA));
    assert(HandlerRuns == 0);
    assert(Mouse[tic_mouse_left].click);

    menu_free(Menu_);
    puts("active ok");
}

static void testBack(void)
{
    setup(Items, COUNT_OF(Items), backHandler);
    settle();

    // The host asks for the back, as the studio does on ESC.
    assert(menu_back(Menu_));
    settle();
    assert(BackRuns == 1);

    // The backspace key asks for it too.
    press(tic_key_backspace);
    settle();
    assert(BackRuns == 2);

    menu_free(Menu_);

    // Without a back handler the widget has nowhere to go.
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    assert(!menu_back(Menu_));

    press(tic_key_backspace);
    settle();
    assert(BackRuns == 0);

    menu_free(Menu_);
    puts("back ok");
}

static void testReinit(void)
{
    setup(Items, COUNT_OF(Items), NULL);
    settle();

    // Opening a submenu calls menu_init again on the same widget: the deps and
    // the view have to survive it.
    menu_init(Menu_, OptionItems, COUNT_OF(OptionItems), 0, 0, NULL, NULL);
    settle();

    press(tic_key_right);
    settle();

    assert(Value == 1 && Sets == 1);

    menu_free(Menu_);
    puts("reinit ok");
}

static void testLeave(void)
{
    setup(LeaveItems, COUNT_OF(LeaveItems), NULL);
    settle();

    tic_rect rect = itemRect("LEAVE", 0, 0);
    s32 x = rect.x + rect.w / 2;
    s32 y = rect.y + rect.h / 2;

    // A click every frame ends on the item when the close movie finishes, and
    // its handler leaves the menu right there: neither the draw nor that click
    // may run after it.
    for(s32 i = 0; i < 30 && HandlerRuns == 0; i++)
    {
        fillBank(0xAA);
        click(x, y);
        frame();
    }

    assert(HandlerRuns == 1);
    assert(Mouse[tic_mouse_left].click);
    assert(bankIs(0xAA));

    menu_free(Menu_);
    puts("leave ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testMovies();
    testNavigation();
    testOption();
    testHotkey();
    testPointer();
    testOwned();
    testActive();
    testBack();
    testReinit();
    testLeave();

    tic_core_close(Tic);
    puts("menu ok");
    return 0;
}
