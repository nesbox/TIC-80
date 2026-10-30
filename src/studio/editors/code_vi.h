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

#include "code_text.h"
#include "studio/keyboard.h"
#include "studio/vimode.h"

// The code editor's vi machine: the modes, the motions, the operators and the
// seeks. It reads keys through the keyboard module on the machine it is given,
// and reaches the editor's own state and screen through the host below; it
// links without the studio.

typedef enum
{
    CODE_VI_EDIT_FIND,
    CODE_VI_EDIT_REPLACE,
    CODE_VI_EDIT_GOTO,
    CODE_VI_EDIT_BOOKMARK,
    CODE_VI_EDIT_OUTLINE,

} CodeViEdit;

typedef enum
{
    CODE_VI_COPY,
    CODE_VI_CUT,
    CODE_VI_PASTE,

} CodeViClipboard;

typedef struct
{
    void* host;
    tic_mem* tic;

    // The menubar's frame: nothing typed reaches the editor while it owns the
    // frame, and Escape is not seen on the frame it closed on.
    bool captured;
    bool escaped;

    ClipboardEvent clipboardEvent; // the frame's gesture
    char* search;                  // the popup's pattern buffer
    s32 searchSize;

    ViMode (*getViMode)(void* host);
    void (*setViMode)(void* host, ViMode mode);

    void (*setEditorMode)(void* host, CodeViEdit mode);
    void (*updateView)(void* host);
    void (*recenter)(void* host, bool emacs);
    void (*page)(void* host, s32 dir, bool half);
    void (*gotoUpdate)(void* host);

    void (*runGame)(void* host);
    void (*saveProject)(void* host);
    void (*viewToggle)(void* host, bool font);
    void (*clipboard)(void* host, CodeViClipboard op);
    bool (*clipboardNewline)(void* host);

} CodeViHost;

void code_vi_tick(Text* text, CodeViHost* host);
