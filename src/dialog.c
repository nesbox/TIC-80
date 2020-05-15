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

#include "dialog.h"

static void drawButton(Dialog* dlg, const char* label, s32 x, s32 y, u8 color, void(*callback)(Dialog* dlg), s32 id)
{
    tic_mem* tic = dlg->tic;

    enum {BtnWidth = 20, BtnHeight = 9};

    tic_rect rect = {x, y, BtnWidth, BtnHeight};
    bool down = false;
    bool over = false;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);
        over = true;

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            down = true;
            dlg->focus = id;
        }

        if(checkMouseClick(&rect, tic_mouse_left))
            callback(dlg);
    }
    
    if(down)
    {
        tic_api_rect(tic, rect.x, rect.y+1, rect.w, rect.h, tic_color_12);
    }
    else
    {
        tic_api_rect(tic, rect.x, rect.y+1, rect.w, rect.h, tic_color_0);
        tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_12);
    }

    s32 size = tic_api_print(tic, label, 0, -TIC_FONT_HEIGHT, 0, true, 1, false);
    tic_api_print(tic, label, rect.x + (BtnWidth - size+1)/2, rect.y + (down?3:2), color, true, 1, false);

    if(dlg->focus == id)
    {
        static const u8 Icon[] =
        {
            0b10000000,
            0b11000000,
            0b11100000,
            0b11000000,
            0b10000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        drawBitIcon(rect.x-5, rect.y+3, Icon, tic_color_0);
        drawBitIcon(rect.x-5, rect.y+2, Icon, tic_color_12);
    }
}

static void onYes(Dialog* dlg)
{
    dlg->callback(true, dlg->data);
    hideDialog();
}

static void onNo(Dialog* dlg)
{
    dlg->callback(false, dlg->data);
    hideDialog();
}

static void processKeyboard(Dialog* dlg)
{
    tic_mem* tic = dlg->tic;
    
    if(tic->ram.input.keyboard.data == 0) return;

    if(keyWasPressed(tic_key_left))
    {
        dlg->focus = (dlg->focus-1) % 2;
        playSystemSfx(2);
    }
    else if(keyWasPressed(tic_key_right) || keyWasPressed(tic_key_tab))
    {
        dlg->focus = (dlg->focus+1) % 2;
        playSystemSfx(2);
    }
    else if(keyWasPressed(tic_key_return) || keyWasPressed(tic_key_space))
    {
        dlg->focus == 0 ? onYes(dlg) : onNo(dlg);
    }
}

static void drawDialog(Dialog* dlg)
{
    enum {Width = TIC80_WIDTH/2, Height = TIC80_HEIGHT/2-TOOLBAR_SIZE};

    tic_mem* tic = dlg->tic;

    tic_rect rect = {(TIC80_WIDTH - Width)/2, (TIC80_HEIGHT - Height)/2, Width, Height};

    rect.x -= dlg->pos.x;
    rect.y -= dlg->pos.y;

    tic_rect header = {rect.x, rect.y-(TOOLBAR_SIZE-1), rect.w, TOOLBAR_SIZE};

    if(checkMousePos(&header))
    {
        setCursor(tic_cursor_hand);

        if(checkMouseDown(&header, tic_mouse_left))
        {
            if(!dlg->drag.active)
            {
                dlg->drag.start.x = getMouseX() + dlg->pos.x;
                dlg->drag.start.y = getMouseY() + dlg->pos.y;

                dlg->drag.active = true;
            }
        }
    }

    if(dlg->drag.active)
    {
        setCursor(tic_cursor_hand);

        dlg->pos.x = dlg->drag.start.x - getMouseX();
        dlg->pos.y = dlg->drag.start.y - getMouseY();

        tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};
        if(!checkMouseDown(&rect, tic_mouse_left))
            dlg->drag.active = false;
    }

    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_15);
    tic_api_rectb(tic, rect.x, rect.y, rect.w, rect.h, tic_color_12);
    tic_api_line(tic, rect.x, rect.y+Height, rect.x+Width-1, rect.y+Height, tic_color_0);
    tic_api_rect(tic, rect.x, rect.y-(TOOLBAR_SIZE-2), rect.w, TOOLBAR_SIZE-2, tic_color_12);
    tic_api_line(tic, rect.x+1, rect.y-(TOOLBAR_SIZE-1), rect.x+Width-2, rect.y-(TOOLBAR_SIZE-1), tic_color_12);

    {
        static const char Label[] = "WARNING!";
        s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, 0, true, 1, false);
        tic_api_print(tic, Label, rect.x + (Width - size)/2, rect.y-(TOOLBAR_SIZE-2), tic_color_15, true, 1, false);
    }

    {
        u8 chromakey = 14;
        tic_api_spr(tic, &getConfig()->cart->bank0.tiles, 2, rect.x+6, rect.y-4, 2, 2, &chromakey, 1, 1, tic_no_flip, tic_no_rotate);
    }

    {
        for(s32 i = 0; i < dlg->rows; i++)
        {
            s32 size = tic_api_print(tic, dlg->text[i], 0, -TIC_FONT_HEIGHT, 0, true, 1, false);

            s32 x = rect.x + (Width - size)/2;
            s32 y = rect.y + (TIC_FONT_HEIGHT+1)*(i+1);
            tic_api_print(tic, dlg->text[i], x, y+1, tic_color_0, true, 1, false);
            tic_api_print(tic, dlg->text[i], x, y, tic_color_12, true, 1, false);
        }
    }

    drawButton(dlg, "YES", rect.x + (Width/2 - 26), rect.y + 45, tic_color_2, onYes, 0);
    drawButton(dlg, "NO", rect.x + (Width/2 + 6), rect.y + 45, tic_color_6, onNo, 1);
}

static void tick(Dialog* dlg)
{
    dlg->ticks++;

    processKeyboard(dlg);

    if(!dlg->init)
    {
        playSystemSfx(0);

        dlg->init = true;
    }

    tic_mem* tic = dlg->tic;
    drawBGAnimation(tic, dlg->ticks);

    drawDialog(dlg);
}

static void escape(Dialog* dlg)
{
    dlg->callback(false, dlg->data);
    hideDialog();
}

void initDialog(Dialog* dlg, tic_mem* tic, const char** text, s32 rows, DialogCallback callback, void* data)
{
    *dlg = (Dialog)
    {
        .init = false,
        .tic = tic,
        .tick = tick,
        .escape = escape,
        .ticks = 0,
        .callback = callback,
        .data = data,
        .text = text,
        .rows = rows,
        .focus = 0,
        .pos = {0, 0},
        .drag = 
        {
            .start = {0, 0},
            .active = 0,
        },
    };
}

void freeDialog(Dialog* dialog)
{
    free(dialog);
}