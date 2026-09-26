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

#include "toolbar.h"

// The strip links against the core alone: no Studio, and nothing that only
// studio.c provides. tests/toolbar.c builds this file without it.

// Runs before the mode's tick, so the strip's frame is settled before anything
// draws into it. The auto-hide state machine lands here too.
void toolbar_step(Toolbar* tb)
{
    tb->tooltip[0] = '\0';

    for(s32 i = 0; i < COUNT_OF(tb->press.y); i++)
    {
        if(tb->mouse[i].down && !tb->press.wasDown[i])
        {
            tb->press.wasDown[i] = true;
            tb->press.y[i] = tb->y;
        }
        else if(!tb->mouse[i].down)
            tb->press.wasDown[i] = false;
    }
}

static inline bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
    return (pt->x >= rect->x)
        && (pt->x < (rect->x + rect->w))
        && (pt->y >= rect->y)
        && (pt->y < (rect->y + rect->h));
}

// The widget's rectangle is in strip coordinates; the mouse is in screen ones.
// The press point is read against the strip as it was when the press began, so
// a strip that moves under the pointer still receives the click.
static bool stripClick(Toolbar* tb, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &tb->mouse[button];

    if(!state->click)
        return false;

    tic_point start = {state->start.x, state->start.y - tb->press.y[button]};
    tic_point end   = {state->end.x,   state->end.y   - tb->press.y[button]};

    if(!pointInRect(&start, rect) || !pointInRect(&end, rect))
        return false;

    state->click = false;
    return true;
}

static bool stripOver(Toolbar* tb, const tic_rect* rect)
{
    tic_point pos = tic_api_mouse(tb->tic);
    pos.y -= tb->y;

    return pointInRect(&pos, rect);
}

static bool stripDown(Toolbar* tb, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &tb->mouse[button];
    tic_point start = {state->start.x, state->start.y - tb->press.y[button]};

    return state->down && pointInRect(&start, rect);
}

// Hover feedback is the strip's, not the widget's: the hand cursor and the
// tooltip in the left rail are the same for every widget.
static bool stripHover(Toolbar* tb, const tic_rect* rect, const char* tip)
{
    if(!stripOver(tb, rect))
        return false;

    toolbar_cursor(tb->tic, tic_cursor_hand);

    if(tip)
        strncpy(tb->tooltip, tip, STUDIO_TEXT_BUFFER_WIDTH - 1);

    return true;
}

static void stripGlyph(Toolbar* tb, const ToolbarButton* button, s32 x, s32 y, u8 color)
{
    if(button->label)
        tic_api_print(tb->tic, button->label, x, y, color, true, 1, true);
    else
        toolbar_icon(tb->tic, &tb->config->cart->bank0.tiles, button->icon, x, y, color);
}

static void stripButton(Toolbar* tb, const ToolbarButton* button, const tic_rect* rect, bool over)
{
    u8 color = button->pressed ? tic_color_white : over ? tic_color_grey : button->color;

    if(button->pressed)
        tic_api_rect(tb->tic, rect->x, rect->y, rect->w, rect->h, tic_color_black);

    stripGlyph(tb, button, rect->x, rect->y, color);
}

void toolbar_begin(Toolbar* tb)
{
    tic_api_rect(tb->tic, 0, tb->y, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    tb->railX = TIC80_WIDTH;
}

bool toolbar_button(Toolbar* tb, const ToolbarButton* button)
{
    tic_rect rect = {tb->railX - button->width, 0, button->width, TOOLBAR_SIZE};

    tb->railX -= button->width;

    bool over = stripHover(tb, &rect, button->tip);
    bool hit = stripClick(tb, &rect, tic_mouse_left);

    stripButton(tb, button, &rect, over);

    return hit && button->enabled;
}

bool toolbar_slider(Toolbar* tb, s32 id, const char* tip, s32* value, s32 min, s32 max)
{
    enum {Width = 23, Height = 5};

    tic_rect rect = {tb->railX - Width, 1, Width, Height};

    tb->railX -= Width + 1;

    stripHover(tb, &rect, tip);

    bool changed = false;

    if(stripDown(tb, &rect, tic_mouse_left))
    {
        s32 step = (tic_api_mouse(tb->tic).x - rect.x) / (Width / (max - min + 1));
        *value = CLAMP(min + step, min, max);
        changed = true;
    }

    if(stripClick(tb, &rect, tic_mouse_left))
        changed = true;

    for(s32 i = min; i <= max; i++)
        tic_api_rect(tb->tic, rect.x + (i - min) * (Width / (max - min + 1)), rect.y, Height / (max - min + 1) - 1, Height, tic_color_black);

    tic_api_rect(tb->tic, rect.x, rect.y + 1, Width, Height - 2, tic_color_black);
    tic_api_rect(tb->tic, rect.x + 1, rect.y + 2, Width - 2, Height - 4, tic_color_white);

    tic_api_rect(tb->tic, rect.x + (*value - min) * (Width / (max - min + 1)), rect.y, Height / (max - min + 1), Height, tic_color_black);
    tic_api_rect(tb->tic, rect.x + 1 + (*value - min) * (Width / (max - min + 1)), rect.y + 1, Height / (max - min + 1) - 2, Height - 2, tic_color_white);

    return changed;
}

// The left rail: one tab per editor, then the mode's name or the tooltip of
// whatever is under the pointer. The clipboard row joins it in group 5.
void toolbar_end(Toolbar* tb)
{
    enum {Size = TOOLBAR_SIZE};

    s32 tab = 0;
    s32 current = -1;

    for(s32 mode = 0; mode < tb->appCount; mode++)
    {
        const EditorApp* app = &tb->apps[mode];

        if(!app->name)
            continue;

        tic_rect rect = {tab * Size, 0, Size, Size};

        bool over = stripHover(tb, &rect, app->tip);
        const tic_tiles* tiles = &tb->config->cart->bank0.tiles;

        if(stripClick(tb, &rect, tic_mouse_left))
            tb->requested = mode;

        if(tb->mode == mode)
            current = tab;

        if(current == tab)
        {
            toolbar_icon(tb->tic, tiles, tic_icon_tab, rect.x, 0, tic_color_grey);
            toolbar_icon(tb->tic, tiles, app->icon, rect.x, 1, tic_color_black);
        }

        toolbar_icon(tb->tic, tiles, app->icon, rect.x, 0,
            current == tab ? tic_color_white : over ? tic_color_grey : tic_color_light_grey);

        tab++;
    }

    // A pro build's bank row takes this space, name and tooltip alike.
    if(current < 0 || tb->hideName)
        return;

    s32 x = (tab + 1) * Size;

    tic_api_print(tb->tic, tb->tooltip[0] ? tb->tooltip : tb->apps[tb->mode].name, x, 1,
        tb->tooltip[0] ? tic_color_dark_grey : tic_color_grey, false, 1, false);
}

void toolbar_icon(tic_mem* tic, const tic_tiles* tiles, s32 id, s32 x, s32 y, u8 color)
{
    const tic_tile* tile = &tiles->data[id];

    for(s32 i = 0, sx = x, ex = sx + TIC_SPRITESIZE; i != TIC_SPRITESIZE * TIC_SPRITESIZE; ++i, ++x)
    {
        if(x == ex)
        {
            x = sx;
            y++;
        }

        if(tic_tool_peek4(tile, i))
            tic_api_pix(tic, x, y, color, false);
    }
}

void toolbar_cursor(tic_mem* tic, tic_cursor id)
{
    VBANK(tic, 0)
    {
        tic->ram->vram.vars.cursor.sprite = id;
    }
}

void toolbar_playClick(tic_mem* tic, const tic_sfx* sfx, s32 id)
{
    const tic_sample* effect = &sfx->samples.data[id];

    tic_api_sfx(tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, MAX_VOLUME, effect->speed);
}
