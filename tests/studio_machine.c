/* The studio mode machine — unit tests.
 *
 * Built by cmake with -DTIC80_BUILD_TESTS=ON, once per shipped configuration
 * (cmake/studio.cmake):
 *
 *   studio_machine_editors   BUILD_EDITORS BUILD_SURF SURF_MENU
 *   studio_machine_surf                BUILD_SURF SURF_MENU
 *   studio_machine_plain
 *
 * Each binary carries the variant it was built for as TEST_VARIANT_*, so what
 * is checked here is a literal and not a re-read of the very macros the module
 * under test reads: a build flag that stops meaning what the machine believes
 * it means fails here rather than in the field. Cases whose answer depends on
 * the configuration say so ("editors only", "a build with the browser") and are
 * skipped in the binaries where they do not apply — skipped, not re-derived.
 *
 * Without cmake:
 *   cc -Isrc -Iinclude -DBUILD_EDITORS -DBUILD_SURF -DTEST_VARIANT_EDITORS \
 *      tests/studio_machine.c src/studio/machine.c -o machine-test && ./machine-test
 */
#include "studio/machine.h"

#include <stdio.h>
#include <string.h>

static int failed;
static int checks;

static SmFeatures variant(void)
{
    return sm_build_features();
}

#if defined(TEST_VARIANT_EDITORS)
enum { WANT_EDITORS = true, WANT_SURF = true };
#elif defined(TEST_VARIANT_SURF)
enum { WANT_EDITORS = false, WANT_SURF = true };
#elif defined(TEST_VARIANT_PLAIN)
enum { WANT_EDITORS = false, WANT_SURF = false };
#else
#error "the test target has to name its variant (TEST_VARIANT_*)"
#endif

static void check(bool ok, const char* what)
{
    ++checks;

    if(!ok)
    {
        ++failed;
        printf("FAIL %s\n", what);
    }
}

static void effectsToText(const SmEffectList* effects, char* out, size_t size)
{
    out[0] = '\0';

    for(u8 i = 0; i < effects->count; ++i)
    {
        if(i)
            strncat(out, " ", size - strlen(out) - 1);

        strncat(out, sm_effect_name(effects->items[i]), size - strlen(out) - 1);
    }
}

// One dispatch, checked against what it must decide and what it must ask for.
static void expect(const char* name, SmState* state, const SmEnv* env, const SmEvent* event,
    EditorMode want_mode, const SmEffect* want_effects, u8 want_count)
{
    ++checks;

    SmResult result = sm_dispatch(state, env, event);

    bool ok = result.to == want_mode;
    const char* what = "the mode it lands in";

    if(ok && result.effects.count != want_count)
    {
        ok = false;
        what = "the number of effects";
    }

    for(u8 i = 0; ok && i < want_count; ++i)
        if(result.effects.items[i] != want_effects[i])
        {
            ok = false;
            what = "the effects, in order";
        }

    if(!ok)
    {
        char got[256];
        char wanted[256];

        effectsToText(&result.effects, got, sizeof got);

        SmEffectList list = { .count = want_count };

        if(want_count)
            memcpy(list.items, want_effects, want_count * sizeof want_effects[0]);
        effectsToText(&list, wanted, sizeof wanted);

        ++failed;
        printf("FAIL %s: %s got %s [%s], want %s [%s]\n",
            name, what, sm_mode_name(result.to), got, sm_mode_name(want_mode), wanted);
    }
}

// What a build can put on screen: without editors that is the menu, the game,
// the start screen and, where it is built, the browser.
static bool legalFor(SmFeatures features, EditorMode mode)
{
    if(features.has_editors)
        return mode >= 0 && mode < TIC_MODES_COUNT;

    return mode == TIC_START_MODE
        || mode == TIC_MENU_MODE
        || mode == TIC_RUN_MODE
        || (mode == TIC_SURF_MODE && features.has_surf);
}

// The history is "the last mode that is not START, CONSOLE or MENU", and it is
// never empty: a menu with nothing to go back to reads it.
static bool legalHistory(EditorMode mode)
{
    return mode != TIC_START_MODE
        && mode != TIC_CONSOLE_MODE
        && mode != TIC_MENU_MODE
        && (int)mode != SM_MODE_NONE;
}

static SmState stateIn(SmFeatures features, EditorMode mode)
{
    SmState state;
    sm_init(&state, features);
    state.mode = mode;
    return state;
}

// The world most cases run in: a cart is loaded, no menu is up, ESC is free.
// Cases that need another world copy this and change the one fact.
static const SmEnv DefaultEnv =
{
    .cart_loaded = true,
    .has_game_menu = false,
    .menu_has_back = false,
    .keepcmd_replay = false,
    .escape_gate = SM_ESC_GATE_OPEN,
};

static SmEvent eventOf(SmEventKind kind)
{
    return (SmEvent){ .kind = kind };
}

// --- the facts about the build and the vocabulary -------------------------

static void test_features(void)
{
    SmFeatures features = sm_build_features();

    check(features.has_editors == WANT_EDITORS, "has_editors follows the build");
    check(features.has_surf == WANT_SURF, "has_surf follows the build");
}

// The toolbar draws the tabs in this order and Ctrl+PgUp/PgDn walks it: the
// order is behaviour, not a detail of where the list is stored.
static void test_editor_ring(void)
{
    static const EditorMode Want[] =
    {
        TIC_CODE_MODE,
        TIC_SPRITE_MODE,
        TIC_MAP_MODE,
        TIC_SFX_MODE,
        TIC_MUSIC_MODE,
    };

    const EditorMode* ring = sm_editor_ring();

    check(SM_EDITOR_RING_COUNT == COUNT_OF(Want), "the count and the ring are the same length");

    for(size_t i = 0; i < COUNT_OF(Want); ++i)
        check(ring[i] == Want[i], "the ring keeps its order");
}

// Names are what the transition trace (TIC80_TRACE_MODES) and the messages
// above print, so every mode and effect needs one and no two may share it.
static void test_names(void)
{
    for(int mode = 0; mode < TIC_MODES_COUNT; ++mode)
    {
        const char* name = sm_mode_name((EditorMode)mode);

        check(name && *name && *name != '?', "every mode has a name");

        for(int other = mode + 1; other < TIC_MODES_COUNT; ++other)
            check(strcmp(name, sm_mode_name((EditorMode)other)) != 0, "no two modes share a name");
    }

    for(int effect = 0; effect < SM_EFF_COUNT; ++effect)
    {
        const char* name = sm_effect_name((SmEffect)effect);

        check(name && *name && *name != '?', "every effect has a name");

        for(int other = effect + 1; other < SM_EFF_COUNT; ++other)
            check(strcmp(name, sm_effect_name((SmEffect)other)) != 0, "no two effects share a name");
    }

    check(strcmp(sm_mode_name(TIC_MODES_COUNT), "?") == 0, "the count is not a mode");
    check(strcmp(sm_mode_name((EditorMode)-1), "?") == 0, "nothing below the first mode");
}

// Where a session begins: the start screen, with the history and ownership the
// studio has always started with.
static void test_init(void)
{
    SmFeatures features = variant();
    SmState state;

    sm_init(&state, features);

    check(state.mode == TIC_START_MODE, "a session starts on the start screen");
    check(state.prev_mode == TIC_CODE_MODE, "the history starts at the code editor");
    check(state.run_from == TIC_CODE_MODE, "a run has no origin yet");
    check((int)state.pending_mode == SM_MODE_NONE, "the mailbox starts empty");
    check(state.player_run, "a run belongs to the player until the studio asks");
    check(!state.menu_over_run, "no menu is up");
    check(!state.dialog, "no dialog is up");
    check(!state.code_focus, "the code editor has no pending focus");

    EditorMode want_dialog = (!features.has_editors && features.has_surf)
        ? TIC_RUN_MODE : TIC_CONSOLE_MODE;

    check(state.dialog_return == want_dialog, "the dialog target starts where the build can answer");
}

// --- requests -------------------------------------------------------------

static void test_request_mode(void)
{
    SmFeatures features = variant();

    if(!features.has_editors)
        return;                         // the remap test below is this variant's case
    static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
    static const SmEffect ResetMenu[] = { SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET };

    // The console is not a place the studio remembers, so leaving it records
    // nothing; the editors and the menu are, so leaving them does.
    {
        SmState state = stateIn(features, TIC_CONSOLE_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_CODE_MODE };

        expect("console -> code", &state, &DefaultEnv, &event, TIC_CODE_MODE, Reset, COUNT_OF(Reset));
        check(state.prev_mode == TIC_CODE_MODE, "the console leaves no history");
    }

    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_CONSOLE_MODE };

        expect("code -> console", &state, &DefaultEnv, &event, TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
        check(state.prev_mode == TIC_CODE_MODE, "the editor is remembered on the way out");
    }

    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_MENU_MODE };

        expect("code -> menu", &state, &DefaultEnv, &event, TIC_MENU_MODE, ResetMenu, COUNT_OF(ResetMenu));
        check(!state.dialog, "a menu asked for through req is the main menu");
    }

    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_RUN_MODE };
        static const SmEffect Want[] = { SM_EFF_INIT_RUN, SM_EFF_VI_MODE_RESET };

        // Entering RUN as a mode does not reset the core: that is runGame's
        // doing, and the difference is what makes RESUME GAME possible at all.
        expect("code -> run", &state, &DefaultEnv, &event, TIC_RUN_MODE, Want, COUNT_OF(Want));
        check(state.prev_mode == TIC_CODE_MODE, "the editor is remembered when a run starts");
    }

    // Leaving a run pauses the core, and the mode that was asked for is what
    // decides whether it is reset on the way into the new one.
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_CONSOLE_MODE };
        static const SmEffect Want[] =
        {
            SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET
        };

        expect("run -> console", &state, &DefaultEnv, &event, TIC_CONSOLE_MODE, Want, COUNT_OF(Want));
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = TIC_WORLD_MODE };
        static const SmEffect Want[] =
        {
            SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_INIT_WORLD, SM_EFF_VI_MODE_RESET
        };

        expect("run -> world", &state, &DefaultEnv, &event, TIC_WORLD_MODE, Want, COUNT_OF(Want));
    }
}

// Asking for the mode that is already on is not always nothing: the music
// editor walks its tabs, and every request drops the vi mode.
static void test_same_mode_requests(void)
{
    SmFeatures features = variant();
    static const SmEffect ViOnly[] = { SM_EFF_VI_MODE_RESET };
    static const SmEffect MusicVi[] = { SM_EFF_MUSIC_NEXT_TAB, SM_EFF_VI_MODE_RESET };

    for(int mode = 0; mode < TIC_MODES_COUNT; ++mode)
    {
        if(!legalFor(features, (EditorMode)mode))
            continue;

        SmState state = stateIn(features, (EditorMode)mode);
        SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = (EditorMode)mode };

        const SmEffect* want = ViOnly;
        u8 count = COUNT_OF(ViOnly);

        if(mode == TIC_MUSIC_MODE)
        {
            want = MusicVi;
            count = COUNT_OF(MusicVi);
        }

        expect("same mode request", &state, &DefaultEnv, &event, (EditorMode)mode, want, count);
    }

    // F1 and Alt+1 are a request for the code editor rather than for the mode:
    // arriving while already there does nothing at all, vi reset included.
    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = eventOf(SM_EV_REQUEST_CODE_FOCUS);

        expect("code focus while in code", &state, &DefaultEnv, &event, TIC_CODE_MODE, NULL, 0);
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_SPRITE_MODE);
        SmEvent event = eventOf(SM_EV_REQUEST_CODE_FOCUS);
        static const SmEffect Want[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET, SM_EFF_CODE_FOCUS };

        expect("code focus from the sprite editor", &state, &DefaultEnv, &event,
            TIC_CODE_MODE, Want, COUNT_OF(Want));
        check(state.code_focus, "the request is remembered as a focus");
    }
}

static void test_cycle_editor(void)
{
    SmFeatures features = variant();
    static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
    const EditorMode* ring = sm_editor_ring();

    if(!features.has_editors)
        return;                         // the tabs are not screens this build has

    for(size_t i = 0; i < SM_EDITOR_RING_COUNT; ++i)
    {
        SmState state = stateIn(features, ring[i]);
        SmEvent event = { .kind = SM_EV_CYCLE_EDITOR, .dir = 1 };
        EditorMode want = ring[(i + 1) % SM_EDITOR_RING_COUNT];

        expect("cycle forward", &state, &DefaultEnv, &event, want, Reset, COUNT_OF(Reset));

        state = stateIn(features, ring[i]);
        event.dir = -1;
        want = ring[(i - 1 + SM_EDITOR_RING_COUNT) % SM_EDITOR_RING_COUNT];

        expect("cycle back", &state, &DefaultEnv, &event, want, Reset, COUNT_OF(Reset));
    }

    // A screen that is not an editor tab has no next tab: the walk finds
    // nothing and the mode stays.
    {
        SmState state = stateIn(features, TIC_CONSOLE_MODE);
        SmEvent event = { .kind = SM_EV_CYCLE_EDITOR, .dir = 1 };

        expect("cycle from the console", &state, &DefaultEnv, &event, TIC_CONSOLE_MODE, NULL, 0);
    }
}

// --- runs -----------------------------------------------------------------

// CLOSE GAME goes back where the run was started from: the browser for a cart
// played from surf (#3015), the console for everything else.
static void test_close_game(void)
{
    SmFeatures features = variant();
    static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
    SmEnv env = DefaultEnv;

    if(features.has_surf)
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = eventOf(SM_EV_CLOSE_GAME);
        state.run_from = TIC_SURF_MODE;
        state.menu_over_run = true;
        state.player_run = true;

        static const SmEffect Want[] =
        {
            SM_EFF_RESET_CORE, SM_EFF_SURF_RESUME, SM_EFF_VI_MODE_RESET
        };

        expect("close a cart played in surf", &state, &env, &event,
            TIC_SURF_MODE, Want, COUNT_OF(Want));
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = eventOf(SM_EV_CLOSE_GAME);
        state.run_from = TIC_CODE_MODE;

        expect("close a studio run", &state, &env, &event, TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));

        state = stateIn(features, TIC_MENU_MODE);
        state.run_from = TIC_CONSOLE_MODE;

        expect("close a run the console asked for", &state, &env, &event,
            TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
    }
}

// ESC in a run: a player's run gets the menu, the studio's own steps back out
// to where it was asked from.
static void test_leave_run_and_run_exit(void)
{
    SmFeatures features = variant();
    static const SmEffect PauseReset[] = { SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = eventOf(SM_EV_LEAVE_RUN);
        state.run_from = TIC_CODE_MODE;

        expect("leave a studio run", &state, &DefaultEnv, &event, TIC_CODE_MODE, PauseReset, COUNT_OF(PauseReset));
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = eventOf(SM_EV_RUN_EXITED);

        expect("a cart that exits", &state, &DefaultEnv, &event, TIC_CONSOLE_MODE, PauseReset, COUNT_OF(PauseReset));
    }
    else
    {
        static const SmEffect Want[] = { SM_EFF_EXIT_STUDIO };
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = eventOf(SM_EV_RUN_EXITED);

        expect("a cart that exits, without a console", &state, &DefaultEnv, &event,
            TIC_RUN_MODE, Want, COUNT_OF(Want));
    }

    // Without editors an error has nowhere to be printed, and nothing moves.
    if(!features.has_editors)
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = eventOf(SM_EV_RUN_ERRORED);

        expect("a cart that errors, without a console", &state, &DefaultEnv, &event,
            TIC_RUN_MODE, NULL, 0);
    }
}

// resumeGame is not a transition: it wakes the paused run and nothing else.
static void test_resume_game(void)
{
    SmFeatures features = variant();
    static const SmEffect Resume[] = { SM_EFF_RESUME_CORE };
    SmState state = stateIn(features, TIC_MENU_MODE);
    SmEvent event = eventOf(SM_EV_RESUME_GAME);

    state.run_from = TIC_CODE_MODE;
    state.player_run = true;
    state.menu_over_run = true;
    state.dialog_return = TIC_CONSOLE_MODE;

    expect("resume the run under the menu", &state, &DefaultEnv, &event, TIC_RUN_MODE, Resume, COUNT_OF(Resume));

    check(state.run_from == TIC_CODE_MODE, "resuming keeps where the run came from");
    check(state.player_run, "resuming keeps who owns the run");
    check(state.menu_over_run, "resuming keeps the menu's own record");
    check(state.prev_mode == TIC_CODE_MODE, "resuming adds no history");
    check(state.dialog_return == TIC_CONSOLE_MODE, "resuming keeps the dialog target");
}

// runGame: a fresh start, told where it was asked for. Restarting the run the
// menu sits over keeps its origin and its owner (#2937).
static void test_run_game(void)
{
    SmFeatures features = variant();
    // runGame resets the core itself before it asks — one reset per run, as
    // the old code had it — so the machine answers with the run alone.
    static const SmEffect StartRun[] = { SM_EFF_INIT_RUN, SM_EFF_VI_MODE_RESET };
    // Asking for the run that is already on is a restart in place: nothing
    // else moves, the vi mode included.
    static const SmEffect RestartRun[] = { SM_EFF_INIT_RUN };
    SmEnv env = DefaultEnv;

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_STUDIO };

        expect("run from the editor", &state, &env, &event, TIC_RUN_MODE, StartRun, COUNT_OF(StartRun));
        check(state.run_from == TIC_CODE_MODE, "the run remembers the editor");
        check(!state.player_run, "a run the studio asked for is not the player's");
    }

    if(features.has_surf)
    {
        SmState state = stateIn(features, TIC_SURF_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_PLAYER };

        expect("run from the browser", &state, &env, &event, TIC_RUN_MODE, StartRun, COUNT_OF(StartRun));
        check(state.run_from == TIC_SURF_MODE, "the run remembers the browser");
        check(state.player_run, "a run from the browser is the player's");
    }

    // The run that is already on: a restart in place, and no bookkeeping.
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_STUDIO };
        state.run_from = TIC_SURF_MODE;
        state.player_run = true;
        state.prev_mode = TIC_CODE_MODE;

        expect("run while running", &state, &env, &event, TIC_RUN_MODE, RestartRun, COUNT_OF(RestartRun));
        check(state.run_from == TIC_SURF_MODE, "a restart keeps where the run came from");
        check(state.player_run, "a restart keeps who owns the run");
        check(state.prev_mode == TIC_CODE_MODE, "a restart moves no history");
    }

    // The restart the pause menu asks for: the run under it keeps its own
    // origin and owner, or a player's cart would become the studio's.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_STUDIO };
        state.run_from = TIC_SURF_MODE;
        state.player_run = true;
        state.menu_over_run = true;

        expect("restart from the pause menu", &state, &env, &event, TIC_RUN_MODE, StartRun, COUNT_OF(StartRun));
        check(state.run_from == TIC_SURF_MODE, "the pause menu's restart keeps the origin");
        check(state.player_run, "the pause menu's restart keeps the owner");
    }

    // A menu opened in the studio is not that case: the run it starts is the
    // studio's, like any other.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_STUDIO };
        state.menu_over_run = false;

        expect("run from a menu opened in the studio", &state, &env, &event,
            TIC_RUN_MODE, StartRun, COUNT_OF(StartRun));
        check(state.run_from == TIC_CODE_MODE, "the menu is not an origin to remember");
        check(!state.player_run, "and the run belongs to the studio");
    }

    // --keepcmd with commands left: no run at all, the console comes back.
    if(features.has_editors)
    {
        // No reset on this path either: runGame resets inside the branch that
        // actually starts a run, and a replay is the other branch.
        static const SmEffect Want[] = { SM_EFF_VI_MODE_RESET };
        SmState state = stateIn(features, TIC_CONSOLE_MODE);
        SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = RUN_FROM_STUDIO };
        SmEnv replay = env;
        replay.keepcmd_replay = true;
        state.run_from = TIC_CODE_MODE;
        state.player_run = false;

        expect("--keepcmd replay", &state, &replay, &event, TIC_CONSOLE_MODE, Want, COUNT_OF(Want));
        check(state.run_from == TIC_CODE_MODE, "a replay moves no origin");
        check(!state.player_run, "a replay moves no ownership");
    }
}

// --- the menu -------------------------------------------------------------

// Opening the menu: the switch builds it on the way in, and an open asked for
// while the menu is already up builds it here.
static void test_open_menu(void)
{
    SmFeatures features = variant();
    static const SmEffect PauseResetMenuBuild[] =
    {
        SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET
    };
    static const SmEffect ResetMenuBuild[] =
    {
        SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET
    };
    // Already in MENU: no mode change, so no reset either — the switch has
    // nothing to do, and the rebuild is the whole point of the open.
    static const SmEffect MenuRebuild[] =
    {
        SM_EFF_VI_MODE_RESET, SM_EFF_REBUILD_MAINMENU
    };
    SmEvent event = eventOf(SM_EV_OPEN_MENU);

    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        state.run_from = TIC_SURF_MODE;

        expect("the pause menu", &state, &DefaultEnv, &event, TIC_MENU_MODE,
            PauseResetMenuBuild, COUNT_OF(PauseResetMenuBuild));
        check(state.menu_over_run, "the menu knows it sits over a run");
        check(state.run_from == TIC_SURF_MODE, "and it leaves the run's origin alone");
    }

    // Asked for while the menu is already up — the Switch's "+" again — the
    // switch does not build it, so the open does.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);

        expect("the menu asked for again", &state, &DefaultEnv, &event, TIC_MENU_MODE,
            MenuRebuild, COUNT_OF(MenuRebuild));
    }

    {
        SmState state = stateIn(features, TIC_CONSOLE_MODE);

        expect("the menu from the console", &state, &DefaultEnv, &event, TIC_MENU_MODE,
            ResetMenuBuild, COUNT_OF(ResetMenuBuild));
        check(!state.menu_over_run, "a menu in the studio is not over a run");
        check(state.run_from == TIC_CONSOLE_MODE, "it remembers the screen it was opened from");
    }
}

// The gamepad's B and the main menu's ESC: over a player's run the back
// resumes it, in the studio it steps back out.
static void test_menu_back(void)
{
    SmFeatures features = variant();
    static const SmEffect Resume[] = { SM_EFF_RESUME_CORE };
    SmEvent event = eventOf(SM_EV_MENU_BACK);

    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        state.player_run = true;
        state.menu_over_run = true;

        expect("back over a player's run", &state, &DefaultEnv, &event, TIC_RUN_MODE, Resume, COUNT_OF(Resume));
    }

    if(features.has_editors)
    {
        static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
        SmState state = stateIn(features, TIC_MENU_MODE);
        state.player_run = false;
        state.menu_over_run = false;
        state.run_from = TIC_CONSOLE_MODE;

        expect("back from a menu in the studio", &state, &DefaultEnv, &event,
            TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
    }
}

// --- dialogs --------------------------------------------------------------

// The answer goes back where the dialog was raised, and the callback is told
// which answer it was.
static void test_dialogs(void)
{
    SmFeatures features = variant();
    static const SmEffect ResetMenu[] = { SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET };
    static const SmEffect ResetCallback[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET, SM_EFF_DIALOG_CALLBACK };
    static const SmEffect ResumeCallback[] = { SM_EFF_RESUME_CORE, SM_EFF_DIALOG_CALLBACK };
    static const SmEffect BuildCallback[] = { SM_EFF_REBUILD_MAINMENU, SM_EFF_DIALOG_CALLBACK };

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEvent event = eventOf(SM_EV_CONFIRM_OPENED);

        expect("a dialog raised in the editor", &state, &DefaultEnv, &event,
            TIC_MENU_MODE, ResetMenu, COUNT_OF(ResetMenu));
        check(state.dialog_return == TIC_CODE_MODE, "the editor is where the answer goes");
        check(state.dialog, "the menu on screen is a dialog");
    }

    // Raised in the menu: the answer returns to the menu, whose rows it
    // replaced. Under the old code the target was left stale, and answering
    // "no" to the quit dialog landed in whatever screen was named last.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = eventOf(SM_EV_CONFIRM_OPENED);
        state.dialog = false;
        state.dialog_return = TIC_CONSOLE_MODE;

        sm_dispatch(&state, &DefaultEnv, &event);

        check(state.dialog_return == TIC_MENU_MODE, "a dialog in the menu returns to the menu");
    }

    // Raised over another dialog: the outer target stands, that being the one
    // the player is still in the middle of.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = eventOf(SM_EV_CONFIRM_OPENED);
        state.dialog = true;
        state.dialog_return = TIC_CONSOLE_MODE;

        sm_dispatch(&state, &DefaultEnv, &event);

        check(state.dialog_return == TIC_CONSOLE_MODE, "a dialog over a dialog keeps the outer target");
    }

    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = { .kind = SM_EV_CONFIRM_ANSWERED, .yes = true };
        state.dialog_return = TIC_RUN_MODE;
        state.dialog = true;

        expect("answering over a run", &state, &DefaultEnv, &event,
            TIC_RUN_MODE, ResumeCallback, COUNT_OF(ResumeCallback));
        check(!state.dialog, "the dialog is gone once answered");
    }

    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = { .kind = SM_EV_CONFIRM_ANSWERED, .yes = false };
        state.dialog_return = TIC_MENU_MODE;
        state.dialog = true;

        expect("answering in the menu", &state, &DefaultEnv, &event,
            TIC_MENU_MODE, BuildCallback, COUNT_OF(BuildCallback));
        check(!state.dialog, "the dialog is gone once answered");
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEvent event = { .kind = SM_EV_CONFIRM_ANSWERED, .yes = false };
        state.dialog_return = TIC_CODE_MODE;

        expect("answering back into the editor", &state, &DefaultEnv, &event,
            TIC_CODE_MODE, ResetCallback, COUNT_OF(ResetCallback));
    }
}

// --- ESC ------------------------------------------------------------------

// The ladder, per screen. Cases whose answer depends on the build say so.
static void test_escape(void)
{
    SmFeatures features = variant();
    SmEnv env = DefaultEnv;
    SmEvent event = eventOf(SM_EV_ESCAPE);

    // Who owns the key: the editor's vi mode swallows it, its popups take it.
    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEnv gated = env;
        gated.escape_gate = SM_ESC_GATE_VI;

        expect("ESC held by vi", &state, &gated, &event, TIC_CODE_MODE, NULL, 0);
    }

    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        SmEnv gated = env;
        gated.escape_gate = SM_ESC_GATE_CODE;
        static const SmEffect Want[] = { SM_EFF_EDITOR_ESCAPE };

        expect("ESC taken by a popup", &state, &gated, &event, TIC_CODE_MODE, Want, COUNT_OF(Want));
    }

    {
        SmState state = stateIn(features, TIC_START_MODE);

        expect("ESC on the start screen", &state, &env, &event, TIC_START_MODE, NULL, 0);
    }

    if(features.has_editors)
    {
        static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };

        {
            SmState state = stateIn(features, TIC_CONSOLE_MODE);

            expect("ESC in the console", &state, &env, &event, TIC_CODE_MODE, Reset, COUNT_OF(Reset));
        }

        {
            SmState state = stateIn(features, TIC_CODE_MODE);

            expect("ESC in the editor", &state, &env, &event, TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
        }

        for(int mode = TIC_SPRITE_MODE; mode <= TIC_MUSIC_MODE; ++mode)
        {
            SmState state = stateIn(features, (EditorMode)mode);

            expect("ESC in an editor", &state, &env, &event, TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
        }
    }

    if(features.has_editors)
    {
        static const SmEffect Want[] =
        {
            SM_EFF_RESET_CORE, SM_EFF_CONSOLE_DONE, SM_EFF_VI_MODE_RESET
        };
        SmState state = stateIn(features, TIC_SURF_MODE);

        expect("ESC in the browser", &state, &env, &event, TIC_CONSOLE_MODE, Want, COUNT_OF(Want));
    }

    // A menu with somewhere to go back to plays that, and moves nothing else.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        SmEnv withBack = env;
        withBack.menu_has_back = true;
        static const SmEffect Want[] = { SM_EFF_MENU_BACK_ANIM };

        expect("ESC in a menu that has a back", &state, &withBack, &event,
            TIC_MENU_MODE, Want, COUNT_OF(Want));
    }

    // A menu without one: the editors build falls back to the last screen the
    // studio was on, an editorless build does nothing at all.
    {
        SmState state = stateIn(features, TIC_MENU_MODE);
        state.prev_mode = TIC_SPRITE_MODE;

        if(features.has_editors)
        {
            static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };

            expect("ESC in a menu with nothing to go back to", &state, &env, &event,
                TIC_SPRITE_MODE, Reset, COUNT_OF(Reset));
        }
        else
        {
            expect("ESC in a menu with nothing to go back to, without editors",
                &state, &env, &event, TIC_MENU_MODE, NULL, 0);
        }
    }

    // The fallback remaps a run to the console: a mode switch cannot un-pause
    // the core, and there is nothing to switch back into.
    if(features.has_editors)
    {
        static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
        SmState state = stateIn(features, TIC_MENU_MODE);
        state.prev_mode = TIC_RUN_MODE;

        expect("ESC in a menu whose history is a run", &state, &env, &event,
            TIC_CONSOLE_MODE, Reset, COUNT_OF(Reset));
    }

    // ESC in a run: a player's gets the menu, the studio's steps back out.
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        state.player_run = true;
        state.run_from = TIC_CODE_MODE;
        static const SmEffect Want[] =
        {
            SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET
        };

        expect("ESC in a player's run", &state, &env, &event, TIC_MENU_MODE, Want, COUNT_OF(Want));
    }

    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        state.player_run = false;
        state.run_from = TIC_CONSOLE_MODE;
        state.dialog = false;

        if(features.has_editors)
        {
            static const SmEffect Want[] =
            {
                SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET
            };

            expect("ESC in a studio run", &state, &env, &event, TIC_CONSOLE_MODE, Want, COUNT_OF(Want));
        }
        else
        {
            // Without editors the console is not a screen the build has, and
            // the remap sends the request back to RUN (a cart is loaded): the
            // run is re-entered rather than left. player_run is never false in
            // these builds — nothing but the player asks for a run — so this is
            // the rule's answer rather than a path anyone walks.
            static const SmEffect Want[] =
            {
                SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_INIT_RUN, SM_EFF_VI_MODE_RESET
            };

            expect("ESC in a studio run, without editors", &state, &env, &event,
                TIC_RUN_MODE, Want, COUNT_OF(Want));
        }
    }

    // A cart that declares a game menu wants its menu seen while iterating: a
    // studio run of one gets the pause menu like a player's.
    {
        SmState state = stateIn(features, TIC_RUN_MODE);
        SmEnv withMenu = env;
        withMenu.has_game_menu = true;
        state.player_run = false;
        state.run_from = TIC_CODE_MODE;
        static const SmEffect Want[] =
        {
            SM_EFF_PAUSE_CORE, SM_EFF_RESET_CORE, SM_EFF_REBUILD_MAINMENU, SM_EFF_VI_MODE_RESET
        };

        expect("ESC in a run with a game menu", &state, &withMenu, &event,
            TIC_MENU_MODE, Want, COUNT_OF(Want));
    }
}

// --- the frame ------------------------------------------------------------

// The toolbar's click lands one frame later: the mode it wrote is picked up
// at the top of the next frame and the mailbox is emptied.
static void test_frame(void)
{
    SmFeatures features = variant();
    static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };
    SmEvent event = eventOf(SM_EV_FRAME);

    {
        SmState state = stateIn(features, TIC_CODE_MODE);

        expect("an empty mailbox", &state, &DefaultEnv, &event, TIC_CODE_MODE, NULL, 0);
        check((int)state.pending_mode == SM_MODE_NONE, "the mailbox stays empty");
    }

    if(features.has_editors)
    {
        SmState state = stateIn(features, TIC_CODE_MODE);
        state.pending_mode = TIC_SPRITE_MODE;

        expect("a click on the sprite tab", &state, &DefaultEnv, &event,
            TIC_SPRITE_MODE, Reset, COUNT_OF(Reset));
        check((int)state.pending_mode == SM_MODE_NONE, "the mailbox is emptied");
    }
}

static void test_open_surf(void)
{
    SmFeatures features = variant();
    static const SmEffect ResetInitSurfResume[] =
    {
        SM_EFF_INIT_SURF, SM_EFF_RESET_CORE, SM_EFF_SURF_RESUME, SM_EFF_VI_MODE_RESET
    };

    if(!features.has_surf)
        return;

    SmState state = stateIn(features, TIC_CONSOLE_MODE);
    SmEvent event = eventOf(SM_EV_OPEN_SURF);

    expect("opening the browser", &state, &DefaultEnv, &event, TIC_SURF_MODE,
        ResetInitSurfResume, COUNT_OF(ResetInitSurfResume));
}

static void test_startup(void)
{
    SmFeatures features = variant();
    static const SmEffect Reset[] = { SM_EFF_RESET_CORE, SM_EFF_VI_MODE_RESET };

    // --skip goes to the console, and the start screen is not a place the
    // studio remembers, so the history keeps what it had.
    {
        static const SmEffect ResetInit[] = { SM_EFF_RESET_CORE, SM_EFF_INIT_RUN, SM_EFF_VI_MODE_RESET };
        SmState state = stateIn(features, TIC_START_MODE);
        SmEvent event = { .kind = SM_EV_STARTUP, .mode = TIC_CONSOLE_MODE };

        // --skip asks for the console: the editors build has one, and a build
        // without them asks for a screen it does not have — the cart is loaded,
        // so the run starts instead, and the switch initialises it.
        EditorMode want = features.has_editors ? TIC_CONSOLE_MODE : TIC_RUN_MODE;
        const SmEffect* effects = features.has_editors ? Reset : ResetInit;
        u8 count = features.has_editors ? COUNT_OF(Reset) : COUNT_OF(ResetInit);

        expect("--skip", &state, &DefaultEnv, &event, want, effects, count);
        check(state.prev_mode == TIC_CODE_MODE, "the start screen leaves no history");
    }

    // No mode asked for: nothing at all happens.
    {
        SmState state = stateIn(features, TIC_START_MODE);
        SmEvent event = { .kind = SM_EV_STARTUP, .mode = SM_MODE_NONE };

        expect("a start with nothing to do", &state, &DefaultEnv, &event,
            TIC_START_MODE, NULL, 0);
    }
}

// --- properties -----------------------------------------------------------

// A sweep over everything: whatever the machine decides, the mode it lands in
// has to be one the build can show — an editorless build walking into an
// editor reads a NULL screen — the history has to stay a place, and the
// effects have to fit.
// A build without editors has no screen for the editors, and the menu and the
// game are what it has instead: every request lands in RUN when a cart is
// loaded and in MENU when none is. An editors build keeps every target.
static void test_editorless_remap(void)
{
    SmFeatures features = variant();
    SmEvent event = { .kind = SM_EV_REQUEST_MODE };

    bool cart[] = { true, false };

    for(size_t c = 0; c < COUNT_OF(cart); ++c)
    {
        for(int target = 0; target < TIC_MODES_COUNT; ++target)
        {
            SmEnv env = DefaultEnv;
            env.cart_loaded = cart[c];

            SmState state = stateIn(features, TIC_MENU_MODE);
            event.mode = (EditorMode)target;

            EditorMode want = (EditorMode)target;

            if(!features.has_editors)
            {
                bool kept = target == TIC_START_MODE || target == TIC_MENU_MODE
                    || target == TIC_RUN_MODE
                    || (target == TIC_SURF_MODE && features.has_surf);

                if(!kept)
                    want = cart[c] ? TIC_RUN_MODE : TIC_MENU_MODE;
            }

            // The effects are the switch's business and are covered above;
            // what is checked here is where a request lands, per build.
            SmResult result = sm_dispatch(&state, &env, &event);

            ++checks;

            if(result.to != want)
            {
                ++failed;
                printf("FAIL remap: asking for %s with cart=%d got %s, want %s\n",
                    sm_mode_name((EditorMode)target), cart[c],
                    sm_mode_name(result.to), sm_mode_name(want));
            }
        }
    }
}

static void test_sweep(void)
{
    SmFeatures features = variant();
    static const SmEventKind Events[] =
    {
        SM_EV_FRAME, SM_EV_ESCAPE, SM_EV_OPEN_MENU, SM_EV_OPEN_SURF, SM_EV_LEAVE_RUN,
        SM_EV_CLOSE_GAME, SM_EV_RESUME_GAME, SM_EV_RUN_GAME, SM_EV_RUN_EXITED,
        SM_EV_RUN_ERRORED, SM_EV_CONFIRM_OPENED, SM_EV_CONFIRM_ANSWERED, SM_EV_MENU_BACK,
        SM_EV_CART_LOADED, SM_EV_CART_LOAD_FAILED, SM_EV_STARTUP, SM_EV_REQUEST_CODE_FOCUS,
        SM_EV_CYCLE_EDITOR,
    };

    for(int mode = 0; mode < TIC_MODES_COUNT; ++mode)
    {
        // Only the screens this build has are states it can be in: a sweep
        // that starts where the build cannot stand proves nothing.
        if(!legalFor(features, (EditorMode)mode))
            continue;

        for(size_t e = 0; e < COUNT_OF(Events); ++e)
        {
            for(int variant_env = 0; variant_env < 4; ++variant_env)
            {
                SmState state = stateIn(features, (EditorMode)mode);
                SmEvent event = { .kind = Events[e] };
                SmEnv env = DefaultEnv;

                env.cart_loaded = (variant_env & 1) != 0;
                env.has_game_menu = (variant_env & 2) != 0;
                env.escape_gate = SM_ESC_GATE_OPEN;
                event.mode = (variant_env & 1) ? TIC_SPRITE_MODE : TIC_CONSOLE_MODE;
                event.dir = 1;

                SmResult result = sm_dispatch(&state, &env, &event);

                ++checks;

                if(!legalFor(features, result.to))
                {
                    ++failed;
                    printf("FAIL sweep: %s + event %d -> %s is not a screen this build has\n",
                        sm_mode_name((EditorMode)mode), (int)Events[e], sm_mode_name(result.to));
                }

                if(!legalHistory(state.prev_mode))
                {
                    ++failed;
                    printf("FAIL sweep: %s + event %d left the history at %s\n",
                        sm_mode_name((EditorMode)mode), (int)Events[e], sm_mode_name(state.prev_mode));
                }

                if(result.effects.count > SM_EFFECTS_MAX)
                {
                    ++failed;
                    printf("FAIL sweep: %s + event %d overflowed the effect list\n",
                        sm_mode_name((EditorMode)mode), (int)Events[e]);
                }
            }
        }
    }
}

int main(void)
{
    test_features();
    test_editor_ring();
    test_names();
    test_init();
    test_request_mode();
    test_same_mode_requests();
    test_cycle_editor();
    test_close_game();
    test_leave_run_and_run_exit();
    test_resume_game();
    test_run_game();
    test_open_menu();
    test_menu_back();
    test_dialogs();
    test_escape();
    test_frame();
    test_open_surf();
    test_startup();
    test_editorless_remap();
    test_sweep();

    printf("%s: %d checks, %d failed\n", failed ? "FAILED" : "ok", checks, failed);

    return failed ? 1 : 0;
}
