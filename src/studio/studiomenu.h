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
#include "menubar.h"
#include "apps.h"

// One per studio, filled by studiomenu_init and handed to the bar as its data:
// a handler reaches both the studio and the bar it belongs to.
typedef struct
{
    Studio* studio;
    Menubar* bar;

    const EditorMode* modes;
    s32 modeCount;
    const EditorApp* apps;
} StudioMenuContext;

// The module's side.
void* studiomenu_init(Studio*, Menubar*, const EditorMode* modes, s32 modeCount, const EditorApp* apps);
MenuBarCategory* studiomenu_categories(s32* count);
void studiomenu_sync(Studio*, Menubar*, EditorMode, const EditorApp*);
void studiomenu_sync_editor(Studio*, Menubar*, EditorMode, void* instance);

// The host's side, implemented in studio.c.
void* studiomenu_editor(Studio*, EditorMode);
s32 studiomenu_editor_bank(Studio*, EditorMode);
void studiomenu_command(Studio*, const char* command);

void takeScreenshot(Studio*);
void startVideoRecord(Studio*);
void gotoFullscreen(Studio*);

#if defined(TIC80_PRO)
void studiomenu_bank_switch(Studio*, s32 bank);
void studiomenu_bank_chain(Studio*);
bool studiomenu_bank_state(Studio*, s32* index, bool* chained, bool* banked);
#endif
