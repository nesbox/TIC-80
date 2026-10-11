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

#include "mouse.h"
#include "core/core.h"

static inline bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
    return (pt->x >= rect->x)
        && (pt->x < (rect->x + rect->w))
        && (pt->y >= rect->y)
        && (pt->y < (rect->y + rect->h));
}

bool mouse_over(tic_mem* tic, const tic_rect* rect)
{
    tic_point pos = tic_api_mouse(tic);
    return pointInRect(&pos, rect);
}

bool mouse_click(MouseState* state, const tic_rect* rect)
{
    bool value = state->click
        && pointInRect(&state->start, rect)
        && pointInRect(&state->end, rect);

    if(value) state->click = false;

    return value;
}

bool mouse_dblclick(MouseState* state, const tic_rect* rect)
{
    bool value = state->dbl.click
        && pointInRect(&state->start, rect)
        && pointInRect(&state->end, rect);

    if(value) state->dbl.click = false;

    return value;
}

bool mouse_down(const MouseState* state, const tic_rect* rect)
{
    return state->down && pointInRect(&state->start, rect);
}
