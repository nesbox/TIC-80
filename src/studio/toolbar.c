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
// Each endpoint is read against the strip as it stood at its own moment: the
// press against where the strip was then, the release against where it is now.
// So a press that lands on a widget keeps the widget even if the strip moves
// under the pointer, and a release below a strip that has slid away misses it.
static bool stripClick(Toolbar* tb, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &tb->mouse[button];

    if(!state->click)
        return false;

    tic_point start = {state->start.x, state->start.y - tb->press.y[button]};
    tic_point end   = {state->end.x,   state->end.y   - tb->y};

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
        tic_api_print(tb->tic, button->label, x, y + tb->y, color, true, 1, button->altFont);
    else
        toolbar_icon(tb->tic, &tb->config->cart->bank0.tiles, button->icon, x, y + tb->y, color);
}

static void stripButton(Toolbar* tb, const ToolbarButton* button, const tic_rect* rect, bool over)
{
    u8 color = button->pressed ? tic_color_white
        : over ? (button->over ? button->over : tic_color_grey)
        : button->color;

    if(button->pressed)
        tic_api_rect(tb->tic, rect->x, rect->y + tb->y, rect->w, rect->h, button->pressedColor);

    stripGlyph(tb, button, rect->x, rect->y, color);
}

// The rail is reset here whether or not the background is painted: an editor
// that painted its own passes bg = false, and a stale cursor would pack the
// right rail from wherever the last frame left it.
void toolbar_begin(Toolbar* tb, bool bg)
{
    if(bg)
        tic_api_rect(tb->tic, 0, tb->y, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    tb->railX = TIC80_WIDTH;
}

bool toolbar_button(Toolbar* tb, const ToolbarButton* button)
{
    tic_rect rect = {tb->railX - button->width, 0, button->width, TOOLBAR_SIZE};

    tb->railX -= button->width;

    bool over = stripHover(tb, &rect, button->tip);
    bool hit = stripClick(tb, &rect, tic_mouse_left);

    if(button->draw)
        button->draw(tb, &rect, over, button->ctx);
    else
        stripButton(tb, button, &rect, over);

    return hit && button->enabled;
}

// A discrete control: one stop per value, drawn the way the sprite editor's
// canvas zoom has always been drawn — hollow stops with the thumb's white
// centre on top. The range is the caller's, and a range wider than the control
// still renders instead of dividing by zero or painting outside itself.
bool toolbar_slider(Toolbar* tb, const char* tip, s32* value, s32 min, s32 max)
{
    enum {Width = 23, Height = 5};

    tic_rect rect = {tb->railX - Width, 1, Width, Height};

    tb->railX -= Width + 1;

    stripHover(tb, &rect, tip);

    s32 stops = MAX(1, max - min + 1);
    s32 pitch = MAX(1, (Width + 1) / stops);
    s32 tick  = MIN(MAX(1, pitch - 1), Height);

    bool changed = false;

    if(stripDown(tb, &rect, tic_mouse_left))
    {
        s32 stop = CLAMP((tic_api_mouse(tb->tic).x - rect.x) / pitch, 0, stops - 1);
        *value = min + stop;
        changed = true;
    }

    if(stripClick(tb, &rect, tic_mouse_left))
        changed = true;

    for(s32 i = 0; i < stops; i++)
    {
        if(i * pitch + tick > Width)
            break;

        tic_api_rect(tb->tic, rect.x + i * pitch, rect.y + tb->y, tick, tick, tic_color_black);
    }

    // After the drag, not before: the thumb has to show the value this frame
    // produced, not the one it replaced.
    s32 at = CLAMP(*value - min, 0, stops - 1);
    s32 thumbX = rect.x + MIN(at * pitch, Width - tick);

    tic_api_rect(tb->tic, rect.x, rect.y + tb->y + 1, Width, Height - 2, tic_color_black);
    tic_api_rect(tb->tic, rect.x + 1, rect.y + tb->y + 2, Width - 2, Height - 4, tic_color_white);

    tic_api_rect(tb->tic, thumbX, rect.y + tb->y, tick, tick, tic_color_black);
    tic_api_rect(tb->tic, thumbX + 1, rect.y + tb->y + 1, tick - 2, tick - 2, tic_color_white);

    return changed;
}

// The left rail: one tab per editor, then the mode's name or the tooltip of
// whatever is under the pointer, then the clipboard row. The five clipboard
// operations are identical in every editor; only what they act on differs, and
// that comes from the registry.
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
            toolbar_icon(tb->tic, tiles, tic_icon_tab, rect.x, tb->y, tic_color_grey);
            toolbar_icon(tb->tic, tiles, app->icon, rect.x, tb->y + 1, tic_color_black);
        }

        toolbar_icon(tb->tic, tiles, app->icon, rect.x, tb->y,
            current == tab ? tic_color_white : over ? tic_color_grey : tic_color_light_grey);

        tab++;
    }

    if(current < 0)
        return;

    s32 x0 = (tab + 1) * Size;
    s32 x = x0;

    // A pro build's bank row sits between the tabs and the row that follows.
#if defined (TIC80_PRO) && defined(BUILD_EDITORS)
    x0 += Size - 2;
#endif

    const ClipboardOps* ops = tb->apps[tb->mode].clipboard;

    enum {Gap = 17 * TIC_FONT_WIDTH, Count = 5};

    static const struct { u8 icon; const char* tip; u8 color; } Buttons[Count] =
    {
        {tic_icon_cut,   "CUT [ctrl+x]",    tic_color_red},
        {tic_icon_copy,  "COPY [ctrl+c]",   tic_color_orange},
        {tic_icon_paste, "PASTE [ctrl+v]",  tic_color_yellow},
        {tic_icon_undo,  "UNDO [ctrl+z]",   tic_color_light_green},
        {tic_icon_redo,  "REDO [ctrl+y]",   tic_color_green},
    };

    void (*const handlers[Count])(void*) =
    {
        ops->cut, ops->copy, ops->paste, ops->undo, ops->redo,
    };

    if(!ops)
        return;

    x += Gap;

    for(s32 i = 0; i < Count; i++)
    {
        ToolbarButton button =
        {
            .icon = Buttons[i].icon,
            .tip = Buttons[i].tip,
            .width = Size,
            .color = tic_color_light_grey,
            .over = Buttons[i].color,
            .enabled = true,
        };

        tic_rect rect = {x + i * Size, 0, Size, Size};

        bool over = stripHover(tb, &rect, button.tip);
        bool held = stripDown(tb, &rect, tic_mouse_left);

        if(stripClick(tb, &rect, tic_mouse_left))
            handlers[i](tb->app);

        if(held)
            tic_api_rect(tb->tic, rect.x, rect.y + tb->y, rect.w, rect.h, button.over);

        stripGlyph(tb, &button, rect.x, rect.y, held ? tic_color_white : over ? button.over : button.color);
    }

    // The name and tooltip yield to a pro build's bank row, but nothing else
    // does: that row ends at x=83 and the clipboard row starts at x=144.
    if(tb->hideName)
        return;

    // Last, so a clipboard button's hover has already set the tooltip this
    // frame draws — which is the order the old drawExtrabar was called in.
    tic_api_print(tb->tic, tb->tooltip[0] ? tb->tooltip : tb->apps[tb->mode].name, x0, 1 + tb->y,
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
