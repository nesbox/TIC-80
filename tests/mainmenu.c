/* The pause/options screen links without the studio: this file builds it with
 * the menu widget and the modules they link, a fake host that records the
 * callbacks, and stubs for the system calls it reaches, so a call into
 * studio.c fails to link here rather than appearing quietly in the app build.
 *
 * The target is built as the editors flavour (BUILD_EDITORS), so CLOSE GAME
 * and its exit are covered as they ship.
 *
 * Target mainmenu-test in cmake/tests.cmake; `ctest -R mainmenu` runs it.
 */
#include "studio/screens/mainmenu.h"
#include "studio/system.h"
#include "script.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static tic_mem* Tic;
static Menu* Menu_;
static MouseState Mouse[3];
static MenuView View;
static StudioMainMenu* Main;
static MainMenuDeps Deps;

// The config cart the widget is given and the options the screen writes.
static tic_cartridge Cart;
static struct StudioOptions Options;

static s32 RunCalls;
static s32 ResumeCalls;
static s32 LeaveCalls;
static s32 ExitStudioCalls;
static s32 ExitGameCalls;
static s32 SetRunModeCalls;
static s32 SaveOptionsCalls;

// The cart's game menu callback, which the language runtime owns in the app.
static s32 MenuCalls;
static s32 MenuIndex;

static bool Paused;
static bool OverPlayerRun;
static bool CartLoaded;

static bool Fullscreen;
static s32 FullscreenSets;
static s32 DefaultMappingCalls;

static void fakeSaveOptions(void* host)
{
    (void)host;
    SaveOptionsCalls++;
}

static bool fakeRunPaused(void* host)
{
    (void)host;
    return Paused;
}

static void fakeResume(void* host)
{
    (void)host;
    ResumeCalls++;
}

static void fakeRun(void* host)
{
    (void)host;
    RunCalls++;
}

static void fakeLeave(void* host)
{
    (void)host;
    LeaveCalls++;
}

static void fakeExitStudio(void* host)
{
    (void)host;
    ExitStudioCalls++;
}

static void fakeExitGame(void* host)
{
    (void)host;
    ExitGameCalls++;
}

static void fakeSetRunMode(void* host)
{
    (void)host;
    SetRunModeCalls++;
}

static bool fakeMenuOverPlayerRun(void* host)
{
    (void)host;
    return OverPlayerRun;
}

static bool fakeIsCartLoaded(void* host)
{
    (void)host;
    return CartLoaded;
}

// The script the screen reads its metatags through: the test's own, so the
// game menu's callback is recorded rather than run.
static void scriptMenu(tic_mem* tic, s32 index, void* data)
{
    (void)tic;
    (void)data;
    MenuCalls++;
    MenuIndex = index;
}

static tic_script TestScript =
    {
        .id = 200,
        .name = "test",
        .singleComment = "--",
        .callback = {.menu = scriptMenu},
};

// The platform's own text input, which the keyboard module asks before its
// fallback: the screen never types, so this answers nothing.
bool tic_sys_keyboard_text(char* text)
{
    (void)text;
    return false;
}

bool tic_sys_fullscreen_get(void)
{
    return Fullscreen;
}

void tic_sys_fullscreen_set(bool value)
{
    Fullscreen = value;
    FullscreenSets++;
}

void tic_sys_default_mapping(tic_mapping* mapping)
{
    DefaultMappingCalls++;
    *mapping = (tic_mapping){0};
}

// The screen's own frame, in the studio's order: the machine starts the tick,
// the menu widget runs the screen's handlers, the machine closes it.
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

// One frame with the key down, then released: the screen takes it as a fresh
// press on that frame and as nothing after it.
static void press(tic_key key)
{
    keys(key);
    frame();
    keys(tic_key_unknown);
}

// Enough frames for two movies in a row — a close that opens a submenu — to
// finish and the widget to be idle.
static void settle(void)
{
    keys(tic_key_unknown);

    for(s32 i = 0; i < 3 * 10; i++)
        frame();
}

static void down(s32 count)
{
    for(s32 i = 0; i < count; i++)
    {
        press(tic_key_down);
        settle();
    }
}

static void initScreen(void);

static void setup(const char* code)
{
    RunCalls = ResumeCalls = LeaveCalls = 0;
    ExitStudioCalls = ExitGameCalls = SetRunModeCalls = SaveOptionsCalls = 0;
    MenuCalls = MenuIndex = 0;
    Paused = OverPlayerRun = false;
    CartLoaded = true;
    Fullscreen = false;
    FullscreenSets = DefaultMappingCalls = 0;

    memset(&Cart, 0, sizeof Cart);
    memset(&Options, 0, sizeof Options);
    memset(Mouse, 0, sizeof Mouse);
    memset(&Tic->ram->input, 0, sizeof Tic->ram->input);

    memset(Tic->cart.code.data, 0, sizeof Tic->cart.code.data);

    if(code)
        strcpy(Tic->cart.code.data, code);

    // The machine's own cart is what the screen reads its metatags from; the
    // language it names is the test's script.
    Tic->cart.lang = TestScript.id;
    tic_add_script(&TestScript);

    View = (MenuView){.active = true};
    Menu_ = menu_create(&(MenuDeps){
        .tic = Tic,
        .cart = &Cart,
        .mouse = Mouse,
    });
    menu_set_view(Menu_, &View);

    Deps = (MainMenuDeps){
        .host = NULL,
        .tic = Tic,
        .menu = Menu_,
        .options = &Options,
        .saveOptions = fakeSaveOptions,

        .runPaused = fakeRunPaused,
        .resume = fakeResume,
        .run = fakeRun,
        .leave = fakeLeave,
        .exitStudio = fakeExitStudio,
        .exitGame = fakeExitGame,
        .setRunMode = fakeSetRunMode,
        .menuOverPlayerRun = fakeMenuOverPlayerRun,
        .isCartLoaded = fakeIsCartLoaded,
    };

    initScreen();
}

// The screen is rebuilt wherever the studio opens the menu, so a host flag
// changed after setup takes effect through this.
static void initScreen(void)
{
    mainmenu_free(Main);
    Main = mainmenu_init(&Deps);
    settle();
}

static void teardown(void)
{
    mainmenu_free(Main);
    Main = NULL;

    menu_free(Menu_);
    Menu_ = NULL;
}

// The main menu's OPTIONS row (RESUME, RESET, CLOSE GAME, OPTIONS), then the
// options menu, which opens on VOLUME.
static void openOptions(void)
{
    down(3);
    press(tic_key_return);
    settle();
}

static void testGameMenu(void)
{
    setup("-- menu: ONE TWO\n");

    // The cart's game menu is the first item; the enter key opens it.
    press(tic_key_return);
    settle();

    // Its first item starts the run and calls the script with its own index.
    press(tic_key_return);
    settle();
    assert(MenuCalls == 1);
    assert(MenuIndex == 0);
    assert(RunCalls == 1);

    down(1);
    press(tic_key_return);
    settle();
    assert(MenuCalls == 2);
    assert(MenuIndex == 1);
    assert(RunCalls == 2);

    // Down from the last item skips the separator onto BACK, which returns to
    // the main menu: the next enter opens the game menu again.
    down(1);
    press(tic_key_return);
    settle();

    press(tic_key_return);
    settle();
    press(tic_key_return);
    settle();
    assert(MenuCalls == 3);
    assert(MenuIndex == 0);

    teardown();
    puts("game menu ok");
}

static void testResume(void)
{
    setup(NULL);

    // No game menu: the top item is RESUME GAME, and no run is paused.
    press(tic_key_return);
    settle();
    assert(RunCalls == 1);
    assert(ResumeCalls == 0);

    Paused = true;
    press(tic_key_return);
    settle();
    assert(ResumeCalls == 1);
    assert(RunCalls == 1);

    teardown();
    puts("resume ok");
}

static void testReset(void)
{
    setup(NULL);

    down(1);

    // RESET GAME resets the machine and asks the host for the run mode.
    Tic->ram->vram.vars.cursor.sprite = tic_cursor_hand;
    press(tic_key_return);
    settle();

    assert(SetRunModeCalls == 1);
    assert(Tic->ram->vram.vars.cursor.sprite == tic_cursor_arrow);

    teardown();
    puts("reset ok");
}

static void testBack(void)
{
    setup(NULL);

    // The back over a player's run resumes it; anywhere else it leaves. A
    // player's run is paused under the menu, so the resume is the paused one.
    OverPlayerRun = true;
    Paused = true;
    assert(menu_back(Menu_));
    settle();
    assert(ResumeCalls == 1);
    assert(LeaveCalls == 0);

    OverPlayerRun = false;
    assert(menu_back(Menu_));
    settle();
    assert(LeaveCalls == 1);

    teardown();
    puts("back ok");
}

static void testNoCartBack(void)
{
    setup(NULL);

    // Without a cart under the menu there is no back to take: the screen is
    // rebuilt with the flag off, as the studio does.
    CartLoaded = false;
    initScreen();

    assert(!menu_back(Menu_));

    press(tic_key_backspace);
    settle();

    assert(LeaveCalls == 0);
    assert(ResumeCalls == 0);

    teardown();
    puts("no cart back ok");
}

static void testOptions(void)
{
    setup(NULL);
    openOptions();

    // Volume is the row the options open on; left and right go through the
    // option's own set.
    press(tic_key_right);
    settle();
    assert(Options.volume == 1);

    press(tic_key_left);
    settle();
    assert(Options.volume == 0);

    // Leaving the options saves them before the main menu returns.
    press(tic_key_backspace);
    settle();
    assert(SaveOptionsCalls == 1);

    teardown();
    puts("options ok");
}

static void testFullscreen(void)
{
    setup(NULL);
    openOptions();

    // VOLUME, then up: INTEGER SCALE, DRAW CACHE when it is built in, and
    // FULLSCREEN.
    press(tic_key_up);
    settle();
#if defined(BUILD_RENDER_CACHE)
    press(tic_key_up);
    settle();
#endif
    press(tic_key_up);
    settle();

    press(tic_key_right);
    settle();
    assert(Options.fullscreen);
    assert(Fullscreen);

    press(tic_key_left);
    settle();
    assert(!Options.fullscreen);
    assert(!Fullscreen);

    // One setter a change, and the system was told each time.
    assert(FullscreenSets == 2);

    teardown();
    puts("fullscreen ok");
}

static void testGamepad(void)
{
    setup(NULL);
    openOptions();

    // VOLUME, then down: SETUP GAMEPAD, which opens on the first button.
    down(1);
    press(tic_key_return);
    settle();

    // Picking the button asks for a key; the screen takes it from the machine
    // once, then the gamepad menu is back.
    press(tic_key_return);
    settle();

    Tic->ram->input.keyboard.keys[0] = tic_key_z;
    assert(mainmenu_keyboard(Main));
    assert(!mainmenu_keyboard(Main));
    Tic->ram->input.keyboard.keys[0] = 0;
    settle();

    // Eight buttons later is SAVE MAPPING: the assignment reaches the options.
    down(8);
    press(tic_key_return);
    settle();
    assert(Options.mapping.data[0 * TIC_BUTTONS + 0] == tic_key_z);

    // Back on the SETUP GAMEPAD row: the gamepad menu again; the separator is
    // skipped on the way, and RESET TO DEFAULTS asks the system for the
    // defaults.
    press(tic_key_return);
    settle();
    down(10);
    press(tic_key_return);
    settle();
    assert(DefaultMappingCalls == 1);

    teardown();
    puts("gamepad ok");
}

static void testExits(void)
{
    setup(NULL);

    // RESUME, RESET, CLOSE GAME, OPTIONS, a separator, QUIT.
    down(4);
    press(tic_key_return);
    settle();
    assert(ExitStudioCalls == 1);

    teardown();

    setup(NULL);
    down(2);
    press(tic_key_return);
    settle();
    assert(ExitGameCalls == 1);

    teardown();
    puts("exits ok");
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testGameMenu();
    testResume();
    testReset();
    testBack();
    testNoCartBack();
    testOptions();
    testFullscreen();
    testGamepad();
    testExits();

    tic_core_close(Tic);
    puts("mainmenu ok");
    return 0;
}
