/* The studio's mode machine links without the studio: this file builds the
 * decisions with the core's headers alone, so a call into studio.c fails to
 * link here rather than appearing quietly in the app build. No machine, no
 * window and no screen: the state goes in, the action comes out.
 *
 * Built as the editors-and-browser flavour, the one the app ships, and again
 * without the browser for the close rule's other branch.
 *
 * Target modes-test / modes-test-nosurf in cmake/tests.cmake;
 * `ctest -R modes-test` runs both.
 */
#include "studio/modes.h"

#include <assert.h>
#include <stdio.h>

// An origin that is not the home: the browser when it is built.
enum
{
    Elsewhere =
#if defined(BUILD_SURF)
        TIC_SURF_MODE,
#else
        TIC_MAP_MODE,
#endif
};

static ModeState state(EditorMode mode, EditorMode returnTo, RunOrigin owner, EditorMode origin, bool paused)
{
    return (ModeState){
        .mode = mode,
        .returnTo = returnTo,
        .run =
            {
                .paused = paused,
                .owner = owner,
                .origin = origin,
            },
    };
}

static ModeState plain(EditorMode mode)
{
    return state(mode, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_CONSOLE_MODE, false);
}

static void testScreen(void)
{
    // A plain switch: the API resets, the machine does not pause, the target's
    // entry runs.
    ModeState s = plain(TIC_CONSOLE_MODE);
    ModeAction a = modesGo(&s, TIC_CODE_MODE, StudioGoScreen);

    assert(a.target == TIC_CODE_MODE);
    assert(a.resetCore && !a.pauseCore && !a.resumeCore && !a.clearExit && a.enter);
    assert(s.mode == TIC_CODE_MODE);

    // Entering the run is not a reset: the run's entry inits it.
    s = plain(TIC_CODE_MODE);
    a = modesGo(&s, TIC_RUN_MODE, StudioGoScreen);

    assert(a.target == TIC_RUN_MODE);
    assert(!a.resetCore && !a.pauseCore && a.enter);
    assert(s.mode == TIC_RUN_MODE);

    // The same mode asked for is no transition at all.
    s = plain(TIC_CODE_MODE);
    a = modesGo(&s, TIC_CODE_MODE, StudioGoScreen);

    assert(a.target == TIC_CODE_MODE);
    assert(!a.resetCore && !a.pauseCore && !a.enter);
    assert(s.mode == TIC_CODE_MODE);

    puts("screen ok");
}

static void testRunLeave(void)
{
    // Leaving the run pauses the machine and marks the run paused.
    ModeState s = plain(TIC_RUN_MODE);
    ModeAction a = modesGo(&s, TIC_CODE_MODE, StudioGoScreen);

    assert(a.pauseCore && a.resetCore && a.enter);
    assert(s.run.paused);

    // Leaving for the run's origin is the same switch.
    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    a = modesGo(&s, s.run.origin, StudioGoLeave);

    assert(a.target == TIC_MAP_MODE);
    assert(a.pauseCore && a.resetCore && a.enter);
    assert(s.run.paused);

    // From a screen it is a plain switch to the origin.
    s = state(TIC_CODE_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    a = modesGo(&s, TIC_MAP_MODE, StudioGoLeave);

    assert(a.target == TIC_MAP_MODE);
    assert(!a.pauseCore && a.resetCore && a.enter);

    puts("run leave ok");
}

static void testMenu(void)
{
    // The menu returns to the screen it was opened from, and a run started
    // from it belongs there too.
    ModeState s = plain(TIC_CODE_MODE);
    ModeAction a = modesGo(&s, TIC_MENU_MODE, StudioGoMenu);

    assert(a.target == TIC_MENU_MODE);
    assert(a.resetCore && !a.pauseCore && a.enter);
    assert(s.returnTo == TIC_CODE_MODE);
    assert(s.run.origin == TIC_CODE_MODE);
    assert(s.mode == TIC_MENU_MODE);

    // Over a run the target is the run, and the run's own origin stays: the
    // menu is not where it belongs (#3015).
    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_PLAYER, Elsewhere, false);
    a = modesGo(&s, TIC_MENU_MODE, StudioGoMenu);

    assert(s.returnTo == TIC_RUN_MODE);
    assert(s.run.origin == Elsewhere);
    assert(a.pauseCore && a.resetCore);
    assert(s.run.paused);

    // A menu asked for while it is open keeps the target and the record.
    s = state(TIC_MENU_MODE, TIC_CODE_MODE, RUN_FROM_STUDIO, TIC_CODE_MODE, false);
    a = modesGo(&s, TIC_MENU_MODE, StudioGoMenu);

    assert(s.returnTo == TIC_CODE_MODE);
    assert(s.run.origin == TIC_CODE_MODE);
    assert(!a.resetCore && !a.pauseCore && !a.enter);

    puts("menu ok");
}

static void testMenuFromScreens(void)
{
    // The menu opened from the start screen returns there.
    ModeState s = plain(TIC_START_MODE);
    modesGo(&s, TIC_MENU_MODE, StudioGoMenu);

    assert(s.returnTo == TIC_START_MODE);
    assert(s.run.origin == TIC_START_MODE);

    // A confirm takes the mode it was asked in and leaves the run's origin.
    s = plain(TIC_CONSOLE_MODE);
    ModeAction a = modesGo(&s, TIC_MENU_MODE, StudioGoConfirm);

    assert(s.returnTo == TIC_CONSOLE_MODE);
    assert(s.run.origin == TIC_CONSOLE_MODE);
    assert(a.enter);

    puts("menu from screens ok");
}

static void testMenuOverRun(void)
{
    // Over a player's run, opened from it: the menu's back resumes.
    ModeState s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    assert(modesMenuOverPlayerRun(&s));

    // Opened in the studio after the run ended: the pause is no game to
    // return to.
    s = state(TIC_MENU_MODE, TIC_CONSOLE_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    assert(!modesMenuOverPlayerRun(&s));

    // A dev's run under the menu is not the player's.
    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_STUDIO, TIC_CONSOLE_MODE, true);
    assert(!modesMenuOverPlayerRun(&s));

    // A run that is not paused is not under anything.
    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, false);
    assert(!modesMenuOverPlayerRun(&s));

    puts("menu over run ok");
}

static void testConfirm(void)
{
    // A confirm takes the mode it was asked in.
    ModeState s = plain(TIC_RUN_MODE);
    ModeAction a = modesGo(&s, TIC_MENU_MODE, StudioGoConfirm);

    assert(s.returnTo == TIC_RUN_MODE);
    assert(a.pauseCore && a.resetCore && a.enter);

    // Over the menu it keeps the target the menu was opened from, and it does
    // not move the run's origin the way opening the menu does.
    s = state(TIC_MENU_MODE, TIC_CODE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    a = modesGo(&s, TIC_MENU_MODE, StudioGoConfirm);

    assert(s.returnTo == TIC_CODE_MODE);
    assert(s.run.origin == TIC_MAP_MODE);
    assert(!a.resetCore && !a.pauseCore && !a.enter);

    puts("confirm ok");
}

static void testResume(void)
{
    // Nothing is paused: no transition.
    ModeState s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, false);
    ModeAction a = modesGo(&s, TIC_RUN_MODE, StudioGoResume);

    assert(a.target == TIC_MENU_MODE);
    assert(!a.resumeCore && !a.resetCore && !a.clearExit && !a.enter);

    // A paused run resumes as it was: no reset, no entry, the exit clears.
    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    a = modesGo(&s, TIC_RUN_MODE, StudioGoResume);

    assert(a.target == TIC_RUN_MODE);
    assert(a.resumeCore && a.clearExit);
    assert(!a.resetCore && !a.pauseCore && !a.enter);
    assert(s.mode == TIC_RUN_MODE);
    assert(!s.run.paused);
    assert(s.run.owner == RUN_FROM_PLAYER);
    assert(s.run.origin == Elsewhere);

    puts("resume ok");
}

static void testClose(void)
{
    // CLOSE GAME goes where the run belongs, and only the browser is a home
    // of its own.
#if defined(BUILD_SURF)
    ModeState s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, TIC_SURF_MODE, true);
    ModeAction a = modesGo(&s, TIC_SURF_MODE, StudioGoClose);

    assert(a.target == TIC_SURF_MODE);
    assert(a.resetCore && a.enter);
#else
    ModeState s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    ModeAction a = modesGo(&s, Elsewhere, StudioGoClose);

    // Without the browser every origin is the build's home.
    assert(a.target == TIC_HOME_MODE);
    assert(a.resetCore && a.enter);
#endif

    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, true);
    a = modesGo(&s, TIC_MAP_MODE, StudioGoClose);

    assert(a.target == TIC_HOME_MODE);
    assert(a.enter);

    puts("close ok");
}

static void testRun(void)
{
    // A run restarted in place is the machine's reset and the run's entry.
    ModeState s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_PLAYER, Elsewhere, false);
    ModeAction a = modesRun(&s, RUN_FROM_STUDIO);

    assert(a.target == TIC_RUN_MODE);
    assert(a.resetCore && a.enter);
    assert(s.mode == TIC_RUN_MODE);
    assert(s.run.owner == RUN_FROM_PLAYER);
    assert(s.run.origin == Elsewhere);

    // From a screen: the run takes both from the caller and the screen.
    s = plain(TIC_CONSOLE_MODE);
    a = modesRun(&s, RUN_FROM_STUDIO);

    assert(s.mode == TIC_RUN_MODE);
    assert(s.run.owner == RUN_FROM_STUDIO);
    assert(s.run.origin == TIC_CONSOLE_MODE);
    assert(!s.run.paused);
    assert(a.resetCore && a.enter);

    // Under the menu over the run it restarts the same run: #2937/#3015.
    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    modesRun(&s, RUN_FROM_STUDIO);

    assert(s.run.owner == RUN_FROM_PLAYER);
    assert(s.run.origin == Elsewhere);
    assert(!s.run.paused);

    // Under a menu opened in the studio nothing changes for the record: the
    // origin the menu set stays.
    s = state(TIC_MENU_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_CODE_MODE, false);
    modesRun(&s, RUN_FROM_PLAYER);

    assert(s.run.owner == RUN_FROM_PLAYER);
    assert(s.run.origin == TIC_CODE_MODE);

    // The startup screen is not an origin.
    s = state(TIC_START_MODE, TIC_HOME_MODE, RUN_FROM_PLAYER, TIC_HOME_MODE, false);
    modesRun(&s, RUN_FROM_PLAYER);

    assert(s.run.origin == TIC_HOME_MODE);

    puts("run ok");
}

static void testEscape(void)
{
    // The console opens the code editor.
    ModeState s = plain(TIC_CONSOLE_MODE);
    EscapeAction e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_CODE_MODE && e.intent == StudioGoScreen);

    // A text-mode code editor lands in the console.
    s = plain(TIC_CODE_MODE);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_CONSOLE_MODE);

    // A dev's run leaves for its origin; a game menu and a player's run get
    // the menu.
    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_MAP_MODE && e.intent == StudioGoScreen);

    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    e = modesEscape(&s, true, false);
    assert(e.go && e.target == TIC_MENU_MODE && e.intent == StudioGoMenu);

    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_PLAYER, Elsewhere, false);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_MENU_MODE && e.intent == StudioGoMenu);

    // A player build's run always gets the menu.
    s = state(TIC_RUN_MODE, TIC_CONSOLE_MODE, RUN_FROM_STUDIO, TIC_MAP_MODE, false);
    e = modesEscape(&s, false, true);
    assert(e.go && e.target == TIC_MENU_MODE && e.intent == StudioGoMenu);

    // The menu returns to where it was opened; a run under it resumes.
    s = state(TIC_MENU_MODE, TIC_CODE_MODE, RUN_FROM_STUDIO, TIC_CODE_MODE, false);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_CODE_MODE && e.intent == StudioGoScreen);

    s = state(TIC_MENU_MODE, TIC_RUN_MODE, RUN_FROM_PLAYER, Elsewhere, true);
    e = modesEscape(&s, true, false);
    assert(e.go && e.target == TIC_RUN_MODE && e.intent == StudioGoResume);

    // A player build's menu leaves only by its own back.
    s = state(TIC_MENU_MODE, TIC_CODE_MODE, RUN_FROM_STUDIO, TIC_CODE_MODE, false);
    e = modesEscape(&s, false, true);
    assert(!e.go);

    // The browser goes home; the splash ignores the key.
#if defined(BUILD_SURF)
    s = plain(TIC_SURF_MODE);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_HOME_MODE);
#endif

    s = plain(TIC_START_MODE);
    e = modesEscape(&s, false, false);
    assert(!e.go);

    // The other editors land in the console.
    s = plain(TIC_SPRITE_MODE);
    e = modesEscape(&s, false, false);
    assert(e.go && e.target == TIC_CONSOLE_MODE);

    puts("escape ok");
}

int main(void)
{
    testScreen();
    testRunLeave();
    testMenu();
    testMenuFromScreens();
    testMenuOverRun();
    testConfirm();
    testResume();
    testClose();
    testRun();
    testEscape();

    puts("modes ok");
    return 0;
}
