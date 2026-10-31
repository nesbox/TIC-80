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

#include "studio/screens/menu.h"

typedef struct StudioMainMenu StudioMainMenu;

// What the screen gets once: the host, the machine, the menu widget it fills,
// the options and their save, and the run and mode actions as callbacks.
typedef struct
{
    void* host;
    tic_mem* tic;
    Menu* menu;
    struct StudioOptions* options;
    void (*saveOptions)(void* host);

    bool (*runPaused)(void* host);
    void (*resume)(void* host);
    void (*run)(void* host);
    void (*leave)(void* host);
    void (*exitStudio)(void* host);
    void (*exitGame)(void* host);
    void (*setRunMode)(void* host);
    bool (*menuOverPlayerRun)(void* host);
    bool (*isCartLoaded)(void* host);
} MainMenuDeps;

StudioMainMenu* mainmenu_init(const MainMenuDeps* deps);
bool mainmenu_keyboard(StudioMainMenu* menu);
void mainmenu_free(StudioMainMenu* menu);
