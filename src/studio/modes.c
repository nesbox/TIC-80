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

#include "modes.h"

// The switch every intent that is not the run's own goes through: the state
// follows, the host applies the effects it returns.
ModeAction modesGo(ModeState* state, EditorMode target, StudioGo intent)
{
    ModeAction action = {.target = target};

    switch(intent)
    {
        case StudioGoMenu:
            // Where the menu returns, and where the run it may start belongs:
            // opening it is not coming back to a screen.
            if(state->mode != TIC_MENU_MODE)
                state->returnTo = state->mode;

            // The menu is not a place a run belongs either; asking for it
            // again while it is open leaves the record alone.
            if(state->mode != TIC_RUN_MODE && state->mode != TIC_MENU_MODE)
                state->run.origin = state->mode;

            action.target = TIC_MENU_MODE;
            break;

        case StudioGoConfirm:
            // A confirm over the menu keeps the target the menu was opened
            // from; anywhere else it takes the mode it was asked in.
            if(state->mode != TIC_MENU_MODE)
                state->returnTo = state->mode;

            action.target = TIC_MENU_MODE;
            break;

        case StudioGoResume:
            // Resuming keeps the machine as it was paused: no reset, no run
            // init — only the pause lifts.
            if(!state->run.paused)
                return (ModeAction){.target = state->mode};

            state->run.paused = false;
            state->mode = TIC_RUN_MODE;

            // Resuming leaves the entry out: the run must not be made again.
            return (ModeAction){
                .target = TIC_RUN_MODE,
                .resumeCore = true,
                .clearExit = true,
            };

        case StudioGoLeave:
            action.target = state->run.origin;
            break;

        case StudioGoClose:
            // CLOSE GAME returns where the run belongs (#3015), and only the
            // browser is a home of its own; anything else is the build's home.
#if defined(BUILD_SURF)
            action.target = state->run.origin == TIC_SURF_MODE ? TIC_SURF_MODE : TIC_HOME_MODE;
#else
            action.target = TIC_HOME_MODE;
#endif
            break;

        case StudioGoScreen:
            break;
    }

    if(action.target == state->mode)
        return action;

    if(state->mode == TIC_RUN_MODE)
    {
        action.pauseCore = true;
        state->run.paused = true;
    }

    if(action.target != TIC_RUN_MODE)
        action.resetCore = true;

    // The target's entry runs on every switch but the resume, which is not
    // entering the run but lifting its pause.
    action.enter = intent != StudioGoResume;

    state->mode = action.target;

    return action;
}

// A run begins or restarts: the record keeps who started it and where it
// belongs when it is the same run restarted under the menu.
ModeAction modesRun(ModeState* state, RunOrigin origin)
{
    ModeAction action = {.target = TIC_RUN_MODE, .resetCore = true, .enter = true};

    if(state->mode == TIC_RUN_MODE)
        return action;

    if(state->mode != TIC_MENU_MODE || state->returnTo != TIC_RUN_MODE)
        state->run.owner = origin;

    // The startup screen is not an origin: that run belongs to the home
    // screen it never left.
    if(state->mode != TIC_MENU_MODE && state->mode != TIC_START_MODE)
        state->run.origin = state->mode;

    state->run.paused = false;
    state->mode = TIC_RUN_MODE;

    return action;
}

bool modesMenuOverPlayerRun(const ModeState* state)
{
    return state->mode == TIC_MENU_MODE && state->returnTo == TIC_RUN_MODE && state->run.paused && state->run.owner == RUN_FROM_PLAYER;
}

// The ESC key's rule for the mode it is pressed in. A player build's menu
// leaves only by its own back, and its runs always get the menu: there is no
// editor behind them to return to.
EscapeAction modesEscape(const ModeState* state, bool gameMenu, bool playerBuild)
{
    EscapeAction action = {0};

    switch(state->mode)
    {
        case TIC_START_MODE:
            break;

        case TIC_MENU_MODE:
            if(playerBuild)
                break;

            action.go = true;
            action.target = state->returnTo;
            action.intent = state->returnTo == TIC_RUN_MODE ? StudioGoResume : StudioGoScreen;
            break;

        case TIC_RUN_MODE:
            // A player's run, and a cart that declares a game menu, get the
            // pause menu; a dev's run leaves for where it was started (#2937).
            if(playerBuild || gameMenu || state->run.owner == RUN_FROM_PLAYER)
            {
                action.go = true;
                action.target = TIC_MENU_MODE;
                action.intent = StudioGoMenu;
            }
            else
            {
                action.go = true;
                action.target = state->run.origin;
            }
            break;

        case TIC_CONSOLE_MODE:
            action.go = true;
            action.target = TIC_CODE_MODE;
            break;

        case TIC_CODE_MODE:
            // The code editor's own escape is asked first; a text-mode editor
            // lands in the console.
            action.go = true;
            action.target = TIC_CONSOLE_MODE;
            break;

#if defined(BUILD_SURF)
        case TIC_SURF_MODE:
            action.go = true;
            action.target = TIC_HOME_MODE;
            break;
#endif

        default:
            action.go = true;
            action.target = TIC_CONSOLE_MODE;
            break;
    }

    return action;
}
