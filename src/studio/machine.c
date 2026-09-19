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
// configuration that exists — and the machine's rules are written for the
// three that do.
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
