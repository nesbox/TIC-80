// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "machine.h"

#include "tic_assert.h"

SmFeatures sm_build_features(void)
{
    return (SmFeatures)
    {
        .has_editors = SM_HAS_EDITORS,
        .has_surf    = SM_HAS_SURF,
    };
}

// An editors build always carries the browser (cmake/studio.cmake defines
// BUILD_SURF for BUILD_EDITORS too), so "editors without surf" is not a
// configuration that exists — and the rules below are written for the three
// that do.
static_assert(!SM_HAS_EDITORS || SM_HAS_SURF, "an editors build always carries the browser");

static const EditorMode Ring[] =
{
    TIC_CODE_MODE,
    TIC_SPRITE_MODE,
    TIC_MAP_MODE,
    TIC_SFX_MODE,
    TIC_MUSIC_MODE,
};

static_assert(COUNT_OF(Ring) == SM_EDITOR_RING_COUNT, "the ring and its count are one thing");

const EditorMode* sm_editor_ring(void)
{
    return Ring;
}

void sm_init(SmState* state, SmFeatures features)
{
    // The dialog target is what confirmHandler fell back to when a dialog was
    // answered before any dialog had been raised (studio.c's initialiser).
    EditorMode dialog_return = TIC_CONSOLE_MODE;

    if(!features.has_editors && features.has_surf)
        dialog_return = TIC_RUN_MODE;

    *state = (SmState)
    {
        .mode = TIC_START_MODE,
        .prev_mode = TIC_CODE_MODE,
        .run_from = TIC_CODE_MODE,
        .pending_mode = SM_MODE_NONE,
        .dialog_return = dialog_return,
        .player_run = true,      // an editorless build starts its cart without runGame
        .menu_over_run = false,
        .dialog = false,
        .code_focus = false,
    };
}

// SM_MODE_NONE is -1 while EditorMode is an unsigned enum, so the question
// "is there a mode here" is a comparison rather than a truth test.
static bool hasMode(EditorMode mode)
{
    return (int)mode != SM_MODE_NONE;
}

static void pushEffect(SmEffectList* effects, SmEffect effect)
{
    assert(effects->count < SM_EFFECTS_MAX);

    effects->items[effects->count++] = effect;
}

// A target an editorless build cannot show becomes the menu or the game, the
// same remap setStudioMode does in place (studio.c, "no editors" switch): the
// build has no screen for the editors, and what it has instead depends on
// whether a cart is loaded.
static EditorMode editorlessTarget(SmFeatures features, const SmEnv* env, EditorMode target)
{
    switch(target)
    {
    case TIC_START_MODE:
    case TIC_MENU_MODE:
    case TIC_RUN_MODE:
        return target;

    case TIC_SURF_MODE:
        // A build without the browser has no use for the mode either.
        if(features.has_surf)
            return target;
        break;

    case TIC_CONSOLE_MODE:
    case TIC_CODE_MODE:
    case TIC_SPRITE_MODE:
    case TIC_MAP_MODE:
    case TIC_WORLD_MODE:
    case TIC_SFX_MODE:
    case TIC_MUSIC_MODE:
        break;

    case TIC_MODES_COUNT:
        assert(false);                              // not a mode
        break;
    }

    return env->cart_loaded ? TIC_RUN_MODE : TIC_MENU_MODE;
}

// The single way a mode is entered — setStudioMode, as a function of the state.
static void req(SmState* state, SmFeatures features, const SmEnv* env, SmResult* result, EditorMode target)
{
    if(target != state->mode)
    {
        if(state->mode == TIC_RUN_MODE)
            pushEffect(&result->effects, SM_EFF_PAUSE_CORE);

        // The answer to "does the core get reset" is the mode that was asked
        // for, not the one the remap below settles on: a build without editors
        // asked for an editor still resets, and lands in RUN. That order is
        // today's, and the two differ exactly there.
        if(target != TIC_RUN_MODE)
            pushEffect(&result->effects, SM_EFF_RESET_CORE);

        // Every mode is remembered as "where the studio was", except the three
        // that are not places: the start screen, the console and the menu.
        switch(state->mode)
        {
        case TIC_START_MODE:
        case TIC_CONSOLE_MODE:
        case TIC_MENU_MODE:
            break;

        case TIC_RUN_MODE:
        case TIC_CODE_MODE:
        case TIC_SPRITE_MODE:
        case TIC_MAP_MODE:
        case TIC_WORLD_MODE:
        case TIC_SFX_MODE:
        case TIC_MUSIC_MODE:
        case TIC_SURF_MODE:
            state->prev_mode = state->mode;
            break;

        case TIC_MODES_COUNT:
            assert(false);                          // not a mode
            break;
        }

        if(!features.has_editors)
            target = editorlessTarget(features, env, target);

        switch(target)
        {
        case TIC_RUN_MODE:      pushEffect(&result->effects, SM_EFF_INIT_RUN); break;
        case TIC_MENU_MODE:     pushEffect(&result->effects, SM_EFF_REBUILD_MAINMENU); break;
        case TIC_CONSOLE_MODE:
            // The console gets told a browser session is over, so it can print
            // a fresh prompt over whatever surf left in its buffer.
            if(state->mode == TIC_SURF_MODE)
                pushEffect(&result->effects, SM_EFF_CONSOLE_DONE);
            break;
        case TIC_WORLD_MODE:    pushEffect(&result->effects, SM_EFF_INIT_WORLD); break;
        case TIC_SURF_MODE:     pushEffect(&result->effects, SM_EFF_SURF_RESUME); break;
        case TIC_START_MODE:
        case TIC_CODE_MODE:
        case TIC_SPRITE_MODE:
        case TIC_MAP_MODE:
        case TIC_SFX_MODE:
        case TIC_MUSIC_MODE:
        case TIC_MODES_COUNT:
            break;                                  // no screen needs waking up
        }

        state->mode = target;

        // A menu entered through req is the main menu: a dialog sets the flag
        // again right after (SM_EV_CONFIRM_OPENED).
        if(target == TIC_MENU_MODE)
            state->dialog = false;
    }
    else if(target == TIC_MUSIC_MODE)
    {
        // Asking for MUSIC while in MUSIC walks its tabs. It reads like a
        // no-op of a mode request, which is why it is spelled out here rather
        // than left to a caller to remember.
        pushEffect(&result->effects, SM_EFF_MUSIC_NEXT_TAB);
    }

    // Unconditional, even when nothing moved: setStudioMode clears the vi mode
    // on every call, and the F1 press that lands in CODE relies on it.
    pushEffect(&result->effects, SM_EFF_VI_MODE_RESET);
}

// The menu, opened from wherever the player is — ESC in a game, the `menu`
// command, the Switch's "+" button. Over a run it is the pause menu and knows
// where the run came from; opened in the studio it remembers the screen it was
// opened from instead.
static void openMenu(SmState* state, SmFeatures features, const SmEnv* env, SmResult* result)
{
    state->menu_over_run = state->mode == TIC_RUN_MODE;

    if(!state->menu_over_run)
        state->run_from = state->mode;

    req(state, features, env, result, TIC_MENU_MODE);

    // gotoMenu builds the menu itself as well, and the switch above has just
    // built it: two builds per open, which is today's behaviour and stays
    // until the callers are migrated.
    pushEffect(&result->effects, SM_EFF_REBUILD_MAINMENU);
}

// resumeGame: the core was paused by the run that the menu is sitting over, and
// entering RUN through a mode switch would initialise a fresh one instead of
// un-pausing that one. No bookkeeping comes with it.
static void resumeRun(SmState* state, SmResult* result)
{
    pushEffect(&result->effects, SM_EFF_RESUME_CORE);
    state->mode = TIC_RUN_MODE;
}

static void escape(SmState* state, SmFeatures features, const SmEnv* env, SmResult* result)
{
    switch(env->escape_gate)
    {
    case SM_ESC_GATE_VI:
        return;                                     // the editor's vi mode owns ESC

    case SM_ESC_GATE_CODE:
        pushEffect(&result->effects, SM_EFF_EDITOR_ESCAPE);
        return;                                     // a popup in the editor, not a screen change

    case SM_ESC_GATE_OPEN:
        break;
    }

    switch(state->mode)
    {
    case TIC_START_MODE:
        break;                                      // the start screen takes no shortcuts

    case TIC_MENU_MODE:
        if(env->menu_has_back)
            pushEffect(&result->effects, SM_EFF_MENU_BACK_ANIM);
        else if(features.has_editors)
            req(state, features, env, result,
                state->prev_mode == TIC_RUN_MODE ? TIC_CONSOLE_MODE : state->prev_mode);
        // An editorless build drops the answer of studio_menu_back and does
        // nothing here at all — kept, it is that variant's own behaviour.
        break;

    case TIC_RUN_MODE:
        if(state->player_run || env->has_game_menu)
            openMenu(state, features, env, result);
        else
            req(state, features, env, result, state->run_from);
        break;

    case TIC_CONSOLE_MODE:
        req(state, features, env, result, TIC_CODE_MODE);
        break;

    case TIC_CODE_MODE:
        // The gate above already answered for a code editor with a popup open.
        req(state, features, env, result, TIC_CONSOLE_MODE);
        break;

    case TIC_SURF_MODE:
        if(features.has_editors)
            req(state, features, env, result, TIC_CONSOLE_MODE);
        else
            openMenu(state, features, env, result);
        break;

    case TIC_SPRITE_MODE:
    case TIC_MAP_MODE:
    case TIC_WORLD_MODE:
    case TIC_SFX_MODE:
    case TIC_MUSIC_MODE:
        req(state, features, env, result, TIC_CONSOLE_MODE);
        break;

    case TIC_MODES_COUNT:
        assert(false);                              // not a mode
        break;
    }
}

SmResult sm_dispatch(SmState* state, const SmEnv* env, const SmEvent* event)
{
    // The build's own answers are constants of the compilation, not facts about
    // the frame: they are read here and passed down, so no rule below carries
    // an #if.
    const SmFeatures features = sm_build_features();

    SmResult result =
    {
        .from = state->mode,
        .to = state->mode,
        .moved = false,
        .yes = false,
        .effects = { .count = 0 },
    };

    switch(event->kind)
    {
    case SM_EV_FRAME:
        if(hasMode(state->pending_mode))
        {
            EditorMode target = state->pending_mode;
            state->pending_mode = SM_MODE_NONE;
            req(state, features, env, &result, target);
        }
        break;

    case SM_EV_REQUEST_MODE:
        req(state, features, env, &result, event->mode);
        break;

    case SM_EV_REQUEST_CODE_FOCUS:
        // F1 and Alt+1 ask for the code editor rather than for the mode: the
        // press that reaches them while already in CODE does nothing at all,
        // not even the vi reset every other request emits.
        if(state->mode != TIC_CODE_MODE)
        {
            req(state, features, env, &result, TIC_CODE_MODE);
            state->code_focus = true;
            pushEffect(&result.effects, SM_EFF_CODE_FOCUS);
        }
        break;

    case SM_EV_CYCLE_EDITOR:
        for(size_t i = 0; i < SM_EDITOR_RING_COUNT; ++i)
        {
            if(state->mode == Ring[i])
            {
                req(state, features, env, &result,
                    Ring[(i + event->dir + SM_EDITOR_RING_COUNT) % SM_EDITOR_RING_COUNT]);
                break;
            }
        }
        break;

    case SM_EV_ESCAPE:
        escape(state, features, env, &result);
        break;

    case SM_EV_OPEN_MENU:
        openMenu(state, features, env, &result);
        break;

    case SM_EV_OPEN_SURF:
        // gotoSurf builds the browser and then switches: the init is not the
        // switch's, and the switch is what tells a resume from a first entry.
        pushEffect(&result.effects, SM_EFF_INIT_SURF);
        req(state, features, env, &result, TIC_SURF_MODE);
        break;

    case SM_EV_LEAVE_RUN:
        req(state, features, env, &result, state->run_from);
        break;

    case SM_EV_CLOSE_GAME:
        // A cart played in the browser goes back to the browser (#3015); any
        // other run lands in the console, the editor it came from included.
        req(state, features, env, &result,
            state->run_from == TIC_SURF_MODE ? TIC_SURF_MODE : TIC_CONSOLE_MODE);
        break;

    case SM_EV_RESUME_GAME:
        resumeRun(state, &result);
        break;

    case SM_EV_RUN_GAME:
        if(env->keepcmd_replay)
        {
            // --keepcmd with commands still queued: no run at all, the
            // commands are replayed and the console comes back.
            req(state, features, env, &result, TIC_CONSOLE_MODE);
            break;
        }

        pushEffect(&result.effects, SM_EFF_RESET_CORE);

        if(state->mode == TIC_RUN_MODE)
        {
            // Asking for the run that is already on is a restart: the cart is
            // re-initialised in place, and nothing else moves.
            pushEffect(&result.effects, SM_EFF_INIT_RUN);
            break;
        }

        // A run asked for from the menu is the restart of the run the menu
        // sits over: it keeps that run's origin and owner, or a player's cart
        // would become the studio's for the rest of the session.
        if(state->mode != TIC_MENU_MODE || !state->menu_over_run)
            state->player_run = event->origin == RUN_FROM_PLAYER;

        if(state->mode != TIC_MENU_MODE)
            state->run_from = state->mode;

        req(state, features, env, &result, TIC_RUN_MODE);
        break;

    case SM_EV_RUN_EXITED:
        if(features.has_editors)
            req(state, features, env, &result, TIC_CONSOLE_MODE);
        else
            pushEffect(&result.effects, SM_EFF_EXIT_STUDIO);
        break;

    case SM_EV_RUN_ERRORED:
        if(features.has_editors)
            req(state, features, env, &result, TIC_CONSOLE_MODE);
        break;

    case SM_EV_CONFIRM_OPENED:
        // The answer goes back where the dialog was raised. Raised in the
        // menu it names the menu, whose rows it is about to replace; raised
        // over another dialog it keeps the outer target, which is the one the
        // player is still in the middle of.
        if(state->mode != TIC_MENU_MODE || !state->dialog)
            state->dialog_return = state->mode;

        req(state, features, env, &result, TIC_MENU_MODE);
        state->dialog = true;
        break;

    case SM_EV_CONFIRM_ANSWERED:
        result.yes = event->yes;

        switch(state->dialog_return)
        {
        case TIC_RUN_MODE:
            resumeRun(state, &result);
            state->dialog = false;
            break;

        case TIC_MENU_MODE:
            // Raised in the menu: the rows come back, the mode does not move.
            pushEffect(&result.effects, SM_EFF_REBUILD_MAINMENU);
            state->dialog = false;
            break;

        case TIC_START_MODE:
        case TIC_CONSOLE_MODE:
        case TIC_CODE_MODE:
        case TIC_SPRITE_MODE:
        case TIC_MAP_MODE:
        case TIC_WORLD_MODE:
        case TIC_SFX_MODE:
        case TIC_MUSIC_MODE:
        case TIC_SURF_MODE:
        case TIC_MODES_COUNT:
            req(state, features, env, &result, state->dialog_return);
            break;
        }

        pushEffect(&result.effects, SM_EFF_DIALOG_CALLBACK);
        break;

    case SM_EV_MENU_BACK:
        // The back of the main menu: over a player's run it resumes it, in the
        // studio it steps back to where the run was asked from — the same step
        // ESC takes in RUN (#2937).
        if(state->player_run && state->menu_over_run)
            resumeRun(state, &result);
        else
            req(state, features, env, &result, state->run_from);
        break;

    case SM_EV_CART_LOADED:
    case SM_EV_CART_LOAD_FAILED:
        // Loading a cart is not a transition: what changes is the next env's
        // cart_loaded. The events exist so a call site speaks one vocabulary.
        break;

    case SM_EV_STARTUP:
        if(hasMode(event->mode))
            req(state, features, env, &result, event->mode);
        break;

    case SM_EV_COUNT:
        break;                                      // not an event
    }

    result.to = state->mode;
    result.moved = result.from != result.to;

    return result;
}

SmModePolicy sm_mode_policy(EditorMode mode)
{
    switch(mode)
    {
    case TIC_RUN_MODE:
        // The cart is playing: its own sound, its own input, and no studio UI
        // drawn over it.
        return (SmModePolicy)
        {
            .sound = SM_SOUND_RAM,
            .input = SM_INPUT_KEEP,
            .clear_vbank1 = false,
            .config_palette = false,
        };

    case TIC_START_MODE:
    case TIC_MENU_MODE:
    case TIC_SURF_MODE:
        // Screens that are the studio's own and are not editors: the sound
        // comes from the config cart the menu plays its sfx with.
        return (SmModePolicy)
        {
            .sound = SM_SOUND_CONFIG,
            .input = SM_INPUT_CLEAR,
            .clear_vbank1 = true,
            .config_palette = true,
        };

    case TIC_CONSOLE_MODE:
    case TIC_CODE_MODE:
    case TIC_SPRITE_MODE:
    case TIC_MAP_MODE:
    case TIC_WORLD_MODE:
    case TIC_SFX_MODE:
    case TIC_MUSIC_MODE:
        // The editors: the studio's UI over a bank of the cart being edited,
        // and its sound comes from that bank rather than from the config cart.
        return (SmModePolicy)
        {
            .sound = SM_SOUND_BANKS,
            .input = SM_INPUT_CLEAR_PAD,
            .clear_vbank1 = true,
            .config_palette = true,
        };

    case TIC_MODES_COUNT:
        break;                                      // not a mode
    }

    return (SmModePolicy)
    {
        .sound = SM_SOUND_BANKS,
        .input = SM_INPUT_CLEAR_PAD,
        .clear_vbank1 = true,
        .config_palette = true,
    };
}

const char* sm_mode_name(EditorMode mode)
{
    switch(mode)
    {
    case TIC_START_MODE:    return "START";
    case TIC_CONSOLE_MODE:  return "CONSOLE";
    case TIC_RUN_MODE:      return "RUN";
    case TIC_CODE_MODE:     return "CODE";
    case TIC_SPRITE_MODE:   return "SPRITE";
    case TIC_MAP_MODE:      return "MAP";
    case TIC_WORLD_MODE:    return "WORLD";
    case TIC_SFX_MODE:      return "SFX";
    case TIC_MUSIC_MODE:    return "MUSIC";
    case TIC_MENU_MODE:     return "MENU";
    case TIC_SURF_MODE:     return "SURF";
    case TIC_MODES_COUNT:   break;
    }

    return "?";
}

const char* sm_effect_name(SmEffect effect)
{
    switch(effect)
    {
    case SM_EFF_PAUSE_CORE:       return "PAUSE_CORE";
    case SM_EFF_RESET_CORE:       return "RESET_CORE";
    case SM_EFF_RESUME_CORE:      return "RESUME_CORE";
    case SM_EFF_INIT_RUN:         return "INIT_RUN";
    case SM_EFF_INIT_SURF:        return "INIT_SURF";
    case SM_EFF_SURF_RESUME:      return "SURF_RESUME";
    case SM_EFF_INIT_WORLD:       return "INIT_WORLD";
    case SM_EFF_REBUILD_MAINMENU: return "REBUILD_MAINMENU";
    case SM_EFF_CONSOLE_DONE:     return "CONSOLE_DONE";
    case SM_EFF_MUSIC_NEXT_TAB:   return "MUSIC_NEXT_TAB";
    case SM_EFF_VI_MODE_RESET:    return "VI_MODE_RESET";
    case SM_EFF_CODE_FOCUS:       return "CODE_FOCUS";
    case SM_EFF_EDITOR_ESCAPE:    return "EDITOR_ESCAPE";
    case SM_EFF_MENU_BACK_ANIM:   return "MENU_BACK_ANIM";
    case SM_EFF_DIALOG_CALLBACK:  return "DIALOG_CALLBACK";
    case SM_EFF_EXIT_STUDIO:      return "EXIT_STUDIO";
    case SM_EFF_COUNT:            break;
    }

    return "?";
}
