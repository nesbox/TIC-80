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

#include "tic.h"

// The studio's layout grid: the bar, the text cells and the animation time.
// Shared by the shell, the screens and the editors, with no studio in it.

#define STUDIO_BAR_HEIGHT         7
#define STUDIO_TEXT_WIDTH         (TIC_FONT_WIDTH)
#define STUDIO_TEXT_HEIGHT        (TIC_FONT_HEIGHT + 1)
#define STUDIO_TEXT_BUFFER_WIDTH  (TIC80_WIDTH / STUDIO_TEXT_WIDTH)
#define STUDIO_TEXT_BUFFER_HEIGHT (TIC80_HEIGHT / STUDIO_TEXT_HEIGHT)
#define STUDIO_TEXT_BUFFER_SIZE   (STUDIO_TEXT_BUFFER_WIDTH * STUDIO_TEXT_BUFFER_HEIGHT)
#define STUDIO_ANIM_TIME          8

#define TIC_COLOR_BG tic_color_black
