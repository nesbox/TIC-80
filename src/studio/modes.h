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

#pragma once

#include "studio.h"

// The run the studio may sit over: whose it is, where it belongs, and whether
// it is paused under the menu.
typedef struct
{
    bool paused;
    RunOrigin owner;
    EditorMode origin;
} RunRecord;

// What the studio's machine reads: the visible mode, where a modal screen
// returns, and the run it may sit over.
typedef struct
{
    EditorMode mode;
    EditorMode returnTo;
    RunRecord run;
} ModeState;

// The intents the drivers express; the door maps each to a target and to the
// effects the host applies. The run's own start is modesRun's.
typedef enum
{
    StudioGoScreen,
    StudioGoMenu,
    StudioGoConfirm,
    StudioGoResume,
    StudioGoLeave,
    StudioGoClose,
} StudioGo;

// What a transition means: the mode to end at and what the host does on the
// way. `enter` is the target's app entry — the run's init among them — and a
// resume leaves it out: the run comes back as it was paused.
typedef struct
{
    EditorMode target;
    bool pauseCore;
    bool resetCore;
    bool resumeCore;
    bool clearExit;
    bool enter;
} ModeAction;

ModeAction modesGo(ModeState* state, EditorMode target, StudioGo intent);
ModeAction modesRun(ModeState* state, RunOrigin origin);

// Whether the menu sits over a player's run: the run is paused under it, the
// player started it, and the menu was opened from it.
bool modesMenuOverPlayerRun(const ModeState* state);

// Where the ESC key sends the studio, or that it does nothing there. The
// menu's back and the code editor's own escape are the host's to ask first.
typedef struct
{
    bool go;
    EditorMode target;
    StudioGo intent;
} EscapeAction;

EscapeAction modesEscape(const ModeState* state, bool gameMenu, bool playerBuild);
