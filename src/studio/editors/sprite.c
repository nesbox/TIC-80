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

#include "sprite.h"
#include "ext/history.h"

#define CANVAS_SIZE (64)
#define PALETTE_CELL_SIZE 8
#define PALETTE_ROWS 2
#define PALETTE_COLS (TIC_PALETTE_SIZE / PALETTE_ROWS)
#define PALETTE_WIDTH (PALETTE_COLS * PALETTE_CELL_SIZE)
#define PALETTE_HEIGHT (PALETTE_ROWS * PALETTE_CELL_SIZE)

enum
{
    ToolbarH = TOOLBAR_SIZE,
    CanvasX = 24, CanvasY = 20, CanvasW = CANVAS_SIZE, CanvasH = CANVAS_SIZE,
    PaletteX = 24, PaletteY = 112, PaletteW = PALETTE_WIDTH, PaletteH = PALETTE_HEIGHT,
    SheetX = TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, SheetY = ToolbarH, SheetW = TIC_SPRITESHEET_SIZE, SheetH = TIC_SPRITESHEET_SIZE,
};

// !TODO: move it to helpers place
static void drawPanelBorder(tic_mem* tic, s32 x, s32 y, s32 w, s32 h)
{
    tic_api_rect(tic, x, y-1, w, 1, tic_color_dark_grey);
    tic_api_rect(tic, x-1, y, 1, h, tic_color_dark_grey);
    tic_api_rect(tic, x, y+h, w, 1, tic_color_light_grey);
    tic_api_rect(tic, x+w, y, 1, h, tic_color_light_grey);
}

static void clearCanvasSelection(Sprite* sprite)
{
    memset(&sprite->select.rect, 0, sizeof(tic_rect));
}

static void initTileSheet(Sprite* sprite)
{
    sprite->sheet = tic_tilesheet_get((( sprite->blit.pages + sprite->blit.page) << 1) + sprite->blit.bank, (u8*)sprite->src);
}

static void updateIndex(Sprite* sprite)
{
    sprite->blit.page %= sprite->blit.pages;
    sprite->index = sprite->y * sprite->blit.pages * TIC_SPRITESHEET_COLS + sprite->x;
    // index has changed, clear selection
    clearCanvasSelection(sprite);
}

static void leftViewport(Sprite* sprite)
{
    if (sprite->blit.page > 0) sprite->blit.page--;
    updateIndex(sprite);
    initTileSheet(sprite);
}

static void rightViewport(Sprite* sprite)
{
    if (sprite->blit.page < sprite->blit.pages-1) sprite->blit.page++;
    updateIndex(sprite);
    initTileSheet(sprite);
}

static void selectViewportPage(Sprite* sprite, u8 page)
{
    sprite->blit.page = page;
    updateIndex(sprite);
    initTileSheet(sprite);
}

static u8 getSheetPixel(Sprite* sprite, s32 x, s32 y)
{
    return tic_tilesheet_getpix(&sprite->sheet, x, y);
}

static void setSheetPixel(Sprite* sprite, s32 x, s32 y, u8 color)
{
    tic_tilesheet_setpix(&sprite->sheet, x, y, color);
}

static s32 getIndexPosX(Sprite* sprite)
{
    return (sprite->x + sprite->blit.page * TIC_SPRITESHEET_COLS) * TIC_SPRITESIZE;
}

static s32 getIndexPosY(Sprite* sprite)
{
    return (sprite->y + sprite->blit.bank * TIC_SPRITESHEET_COLS) * TIC_SPRITESIZE;
}

static void drawSelection(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
    tic_mem* tic = sprite->tic;

    enum{Step = 3};
    u8 color = tic_color_white;

    s32 index = sprite->tickCounter / 10;
    for(s32 i = x; i < (x+w); i++)      { tic_api_pix(tic, i, y, index++ % Step ? color : 0, false);} index++;
    for(s32 i = y; i < (y+h); i++)      { tic_api_pix(tic, x + w-1, i, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (x+w-1); i >= x; i--)   { tic_api_pix(tic, i, y + h-1, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (y+h-1); i >= y; i--)   { tic_api_pix(tic, x, i, index++ % Step ? color : 0, false);}
}

static tic_rect getSpriteRect(Sprite* sprite)
{
    s32 x = getIndexPosX(sprite);
    s32 y = getIndexPosY(sprite);

    return (tic_rect){x, y, sprite->size, sprite->size};
}

static void drawCursorBorder(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
    tic_mem* tic = sprite->tic;

    tic_api_rectb(tic, x, y, w, h, tic_color_black);
    tic_api_rectb(tic, x-1, y-1, w+2, h+2, tic_color_white);
}

static void processPickerCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        if(checkMouseDown(&rect, tic_mouse_left))
            sprite->color = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);

        if(checkMouseDown(&rect, tic_mouse_right))
            sprite->color2 = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);
    }
}

static void processDrawCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;

        s32 brushSize = sprite->brushSize*Size;
        s32 offset = (brushSize - Size) / 2;

        mx -= offset;
        my -= offset;
        mx -= mx % Size;
        my -= my % Size;

        if(mx < 0) mx = 0;
        if(my < 0) my = 0;
        if(mx+brushSize >= CANVAS_SIZE) mx = CANVAS_SIZE - brushSize;
        if(my+brushSize >= CANVAS_SIZE) my = CANVAS_SIZE - brushSize;

        SHOW_TOOLTIP("[x=%02i y=%02i]", mx / Size, my / Size);

        drawCursorBorder(sprite, x + mx, y + my, brushSize, brushSize);

        bool left = checkMouseDown(&rect, tic_mouse_left);
        bool right = checkMouseDown(&rect, tic_mouse_right);

        if(left || right)
        {
            sx += mx / Size;
            sy += my / Size;
            u8 color = left ? sprite->color : sprite->color2;
            s32 pixels = sprite->brushSize;

            for(s32 j = 0; j < pixels; j++)
                for(s32 i = 0; i < pixels; i++)
                    setSheetPixel(sprite, sx+i, sy+j, color);

            history_add(sprite->history);
        }
    }
}

static void pasteSelection(Sprite* sprite)
{
    s32 l = getIndexPosX(sprite);
    s32 t = getIndexPosY(sprite);
    s32 r = l + sprite->size;
    s32 b = t + sprite->size;

    for(s32 sy = t, i = 0; sy < b; sy++)
        for(s32 sx = l; sx < r; sx++)
            setSheetPixel(sprite, sx, sy, sprite->select.back[i++]);

    tic_rect* rect = &sprite->select.rect;

    l += rect->x;
    t += rect->y;
    r = l + rect->w;
    b = t + rect->h;

    for(s32 sy = t, i = 0; sy < b; sy++)
        for(s32 sx = l; sx < r; sx++)
            setSheetPixel(sprite, sx, sy, sprite->select.front[i++]);

    history_add(sprite->history);
}

static void copySelection(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    for(s32 sy = rect.y, i = 0; sy < b; sy++)
        for(s32 sx = rect.x; sx < r; sx++)
            sprite->select.back[i++] = getSheetPixel(sprite, sx, sy);

    {
        tic_rect* rect = &sprite->select.rect;
        memset(sprite->select.front, 0, CANVAS_SIZE * CANVAS_SIZE);

        for(s32 j = rect->y, index = 0; j < (rect->y + rect->h); j++)
            for(s32 i = rect->x; i < (rect->x + rect->w); i++)
            {
                u8* color = &sprite->select.back[i+j*sprite->size];
                sprite->select.front[index++] = *color;
                *color = sprite->color2;
            }
    }
}

static void processSelectCanvasMouse(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    bool endDrag = false;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            if(sprite->select.drag)
            {
                s32 x = mx / Size;
                s32 y = my / Size;

                s32 rl = MIN(x, sprite->select.start.x);
                s32 rt = MIN(y, sprite->select.start.y);
                s32 rr = MAX(x, sprite->select.start.x);
                s32 rb = MAX(y, sprite->select.start.y);

                sprite->select.rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};
            }
            else
            {
                sprite->select.drag = true;
                sprite->select.start = (tic_point){mx / Size, my / Size};
                sprite->select.rect = (tic_rect){sprite->select.start.x, sprite->select.start.y, 1, 1};
            }
        }
        else endDrag = sprite->select.drag;
    }
    else endDrag = !tic->ram.input.mouse.left && sprite->select.drag;

    if(endDrag)
    {
        copySelection(sprite);
        sprite->select.drag = false;
    }
}

static void floodFill(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
    if(getSheetPixel(sprite, x, y) == color)
    {
        setSheetPixel(sprite, x, y, fill);

        if(x > l) floodFill(sprite, l, t, r, b, x-1, y, color, fill);
        if(x < r) floodFill(sprite, l, t, r, b, x+1, y, color, fill);
        if(y > t) floodFill(sprite, l, t, r, b, x, y-1, color, fill);
        if(y < b) floodFill(sprite, l, t, r, b, x, y+1, color, fill);
    }
}

static void replaceColor(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
    for(s32 sy = t; sy <= b; sy++)
        for(s32 sx = l; sx <= r; sx++)
            if(getSheetPixel(sprite, sx, sy) == color)
                setSheetPixel(sprite, sx, sy, fill);
}

static void processFillCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 l, s32 t)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        bool left = checkMouseClick(&rect, tic_mouse_left);
        bool right = checkMouseClick(&rect, tic_mouse_right);

        if(left || right)
        {
            s32 sx = l + mx / Size;
            s32 sy = t + my / Size;

            u8 color = getSheetPixel(sprite, sx, sy);
            u8 fill = left ? sprite->color : sprite->color2;

            if(color != fill)
            {
                tic_api_key(tic, tic_key_ctrl)
                    ? replaceColor(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill)
                    : floodFill(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill);
            }

            history_add(sprite->history);
        }
    }
}

static bool hasCanvasSelection(Sprite* sprite)
{
    return sprite->mode == SPRITE_SELECT_MODE && sprite->select.rect.w && sprite->select.rect.h;
}

static void drawBrushSlider(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    enum {Count = 4, Size = 5};

    tic_rect rect = {x, y, Size, (Size+1)*Count};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("BRUSH SIZE");
        over = true;

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            s32 my = tic_api_mouse(tic).y - y;

            sprite->brushSize = Count - my / (Size+1);
        }
    }

    tic_api_rect(tic, x+1, y, Size-2, Size*Count, tic_color_black);

    for(s32 i = 0; i < Count; i++)
    {
        s32 offset = y + i*(Size+1);

        tic_api_rect(tic, x, offset, Size, Size, tic_color_black);
        tic_api_rect(tic, x + 6, offset + 2, Count - i, 1, tic_color_black);
    }

    tic_api_rect(tic, x+2, y+1, 1, Size*Count+1, (over ? tic_color_white : tic_color_grey));

    s32 offset = y + (Count - sprite->brushSize)*(Size+1);
    tic_api_rect(tic, x, offset, Size, Size, tic_color_black);
    tic_api_rect(tic, x+1, offset+1, Size-2, Size-2, (over ? tic_color_white : tic_color_grey));
}

static void drawCanvasOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    const s32 Size = CANVAS_SIZE / sprite->size;
    const tic_rect rect = getSpriteRect(sprite);

    const tic_rect canvasRect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    if(checkMouseDown(&canvasRect, tic_mouse_middle))
    {
        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;
        sprite->color = getSheetPixel(sprite, rect.x + mx / Size, rect.y + my / Size);
    }

    drawPanelBorder(tic, canvasRect.x - 1, canvasRect.y - 1, canvasRect.w + 2, canvasRect.h + 2);
    tic_api_rectb(tic, canvasRect.x - 1, canvasRect.y - 1, canvasRect.w + 2, canvasRect.h + 2, tic_color_black);

    if(!sprite->palette.edit)
    {
        switch(sprite->mode)
        {
        case SPRITE_DRAW_MODE: 
            processDrawCanvasMouse(sprite, x, y, rect.x, rect.y);
            drawBrushSlider(sprite, x - 15, y + 20);
            break;
        case SPRITE_PICK_MODE: processPickerCanvasMouse(sprite, x, y, rect.x, rect.y); break;
        case SPRITE_SELECT_MODE: processSelectCanvasMouse(sprite, x, y); break;
        case SPRITE_FILL_MODE: processFillCanvasMouse(sprite, x, y, rect.x, rect.y); break;
        }       
    }

    if(hasCanvasSelection(sprite))
        drawSelection(sprite, x + sprite->select.rect.x * Size - 1, y + sprite->select.rect.y * Size - 1, 
            sprite->select.rect.w * Size + 2, sprite->select.rect.h * Size + 2);
    else
    {
        char buf[sizeof "#9999"];
        sprintf(buf, "#%i", sprite->index + tic_blit_calc_index(&sprite->blit));

        s32 ix = x + (CANVAS_SIZE - strlen(buf) * TIC_FONT_WIDTH) / 2;
        s32 iy = TIC_SPRITESIZE + 2;
        tic_api_print(tic, buf, ix, iy+1, tic_color_black, true, 1, false);
        tic_api_print(tic, buf, ix, iy, tic_color_white, true, 1, false);
    }
}

static void drawCanvas(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    const s32 Size = CANVAS_SIZE / sprite->size;

    for(s32 sy = rect.y, j = y; sy < b; sy++, j += Size)
        for(s32 sx = rect.x, i = x; sx < r; sx++, i += Size)
            tic_api_rect(tic, i, j, Size, Size, getSheetPixel(sprite, sx, sy));
}

static void upCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->y > 0) rect->y--;
    pasteSelection(sprite);
}

static void downCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->y + rect->h < sprite->size) rect->y++;
    pasteSelection(sprite);
}

static void leftCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->x > 0) rect->x--;
    pasteSelection(sprite);
}

static void rightCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->x + rect->w < sprite->size) rect->x++;
    pasteSelection(sprite);
}

static void rotateSelectRect(Sprite* sprite)
{
    tic_rect rect = sprite->select.rect;
    
    s32 selection_center_x = rect.x + rect.w/2;
    s32 selection_center_y = rect.y + rect.h/2;
    
    // Rotate
    sprite->select.rect.w = rect.h;
    sprite->select.rect.h = rect.w;
    
    // Make the new center be at the position of the previous center
    sprite->select.rect.x -= (sprite->select.rect.x + sprite->select.rect.w/2) - selection_center_x;
    sprite->select.rect.y -= (sprite->select.rect.y + sprite->select.rect.h/2) - selection_center_y;
    
    // Check if we are not out of boundaries
    if (sprite->select.rect.x < 0) sprite->select.rect.x = 0;
    if (sprite->select.rect.y < 0) sprite->select.rect.y = 0;
    
    if (sprite->select.rect.x + sprite->select.rect.w >= sprite->size)
    {
        sprite->select.rect.x -= sprite->select.rect.x + sprite->select.rect.w - sprite->size;
    }
    
    if (sprite->select.rect.y + sprite->select.rect.h >= sprite->size)
    {
        sprite->select.rect.y -= sprite->select.rect.y + sprite->select.rect.h - sprite->size;
    }
}

static void rotateCanvas(Sprite* sprite)
{
    u8* buffer = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);

    if(buffer)
    {
        {
            tic_rect rect = sprite->select.rect;
            const s32 Size = rect.h * rect.w;
            s32 diff = 0;

            for(s32 y = 0, i = 0; y < rect.w; y++)
                for(s32 x = 0; x < rect.h; x++)
                {
                    diff = rect.w * (x + 1) -y;
                    buffer[i++] = sprite->select.front[Size - diff];
                }
            
            for (s32 i = 0; i<Size; i++)
                sprite->select.front[i] = buffer[i];
            
            rotateSelectRect(sprite);
            pasteSelection(sprite);
            history_add(sprite->history);
        }

        free(buffer);
    }
}

static void deleteCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 left = getIndexPosX(sprite) + rect->x;
    s32 top = getIndexPosY(sprite) + rect->y;
    s32 right = left + rect->w;
    s32 bottom = top + rect->h;

    for(s32 pixel_y = top; pixel_y < bottom; pixel_y++)
        for(s32 pixel_x = left; pixel_x < right; pixel_x++)
            setSheetPixel(sprite, pixel_x, pixel_y, sprite->color2);

    clearCanvasSelection(sprite);
    
    history_add(sprite->history);
}

static void flipCanvasHorz(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 sprite_x = getIndexPosX(sprite);
    s32 sprite_y = getIndexPosY(sprite);
    
    s32 right = sprite_x + rect->x + rect->w/2;
    s32 bottom = sprite_y + rect->y + rect->h;

    for(s32 y = sprite_y + rect->y; y < bottom; y++)
        for(s32 x = sprite_x + rect->x, i = sprite_x + rect->x + rect->w - 1; x < right; x++, i--)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
            setSheetPixel(sprite, i, y, color);
        }

    history_add(sprite->history);
    copySelection(sprite);
}

static void flipCanvasVert(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 sprite_x = getIndexPosX(sprite);
    s32 sprite_y = getIndexPosY(sprite);
    
    s32 right = sprite_x + rect->x + rect->w;
    s32 bottom = sprite_y + rect->y + rect->h/2;

    for(s32 y = sprite_y + rect->y, i = sprite_y + rect->y + rect->h - 1; y < bottom; y++, i--)
        for(s32 x = sprite_x + rect->x; x < right; x++)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
            setSheetPixel(sprite, x, i, color);
        }

    history_add(sprite->history);
    copySelection(sprite);
}

static s32* getSpriteIndexes(Sprite* sprite)
{
    static s32 indexes[TIC_SPRITESIZE*TIC_SPRITESIZE+1];
    memset(indexes, -1, sizeof indexes);

    u16 sheet_cols = TIC_SPRITESHEET_COLS * sprite->blit.pages;

    {
        tic_rect r = {sprite->index % sheet_cols, sprite->index / sheet_cols
            , sprite->size / TIC_SPRITESIZE, sprite->size / TIC_SPRITESIZE};

        s32 c = 0;
        for(s32 j = r.y; j < r.h + r.y; j++)
            for(s32 i = r.x; i < r.w + r.x; i++)
                indexes[c++] = (i + j * sheet_cols) + sprite->blit.bank * TIC_BANK_SPRITES;
    }

    return indexes;
}

static void drawFlags(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    if(hasCanvasSelection(sprite)) return;

    enum {Size = 5};

    u8* flags = getBankFlags()->data;
    u8 or = 0;
    u8 and = 0xff;

    const s32* indexes = getSpriteIndexes(sprite);

    {
        const s32* i = indexes;
        while(*i >= 0)
        {
            u8 mask = flags[*i++];
            or |= mask;
            and &= mask;
        }       
    }

    for(s32 i = 0; i < BITS_IN_BYTE; i++)
    {
        const u8 mask = 1 << i;
        tic_rect rect = {x, y + (Size+1)*i, Size, Size};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            SHOW_TOOLTIP("set flag [%i]", i);

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                const s32* i = indexes;

                if(or & mask)
                    while(*i >= 0)
                        flags[*i++] &= ~mask;
                else
                    while(*i >= 0) 
                        flags[*i++] |= mask;
            }
        }

        tic_api_rect(tic, rect.x, rect.y, Size, Size, tic_color_black);

        u8 flagColor = i + 2;

        if(or & mask)
            tic_api_pix(tic, rect.x + 2, rect.y + 2, flagColor, false);
        else if(over)
            tic_api_rect(tic, rect.x + 1, rect.y + 1, Size - 2, Size - 2, flagColor);
        
        if(and & mask)
        {
            tic_api_rect(tic, rect.x + 1, rect.y + 1, Size - 2, Size - 2, flagColor);
            tic_api_pix(tic, rect.x + 3, rect.y + 1, tic_color_white, false);
        }

        tic_api_print(tic, (char[]){'0' + i, '\0'}, rect.x + (Size+2), rect.y, tic_color_light_grey, false, 1, true);
    }
}

static void switchBitMode(Sprite* sprite, tic_bpp bpp)
{
    tic_blit_update_bpp(&sprite->blit, bpp);
    updateIndex(sprite);
    initTileSheet(sprite);

    sprite->color  %= 1 << sprite->blit.mode;
    sprite->color2 %= 1 << sprite->blit.mode;
}

static void drawBitMode(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
    tic_mem* tic = sprite->tic;
    s32 label_w = tic_api_print(tic, "BPP :", x+2, y, tic_color_dark_grey, false, 1, true);
    x += label_w+4;
    w -= label_w+4;

    enum {Modes = 3, SizeY = 5, SizeX = 5, OffsetX = 15};

    s32 centerX = x + w / 2;

    for(s32 i = 0; i < Modes; i++)
    {
        tic_bpp mode = 1 << (2-i);
        bool current = mode == sprite->blit.mode;

        tic_rect rect = {centerX - SizeX / 2 + (i-1) * OffsetX, y, SizeX, SizeY};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            if(mode > 1)
                SHOW_TOOLTIP("%iBITS PER PIXEL", mode);
            else
                SHOW_TOOLTIP("%iBIT PER PIXEL", mode);

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                switchBitMode(sprite, mode);
            }
        }

        u8 label_color = current ? tic_color_white : tic_color_dark_grey;

        tic_api_rect(tic, rect.x, rect.y, SizeX, SizeY, tic_color_dark_grey);
        if (current) {
            tic_api_rect(tic, rect.x+1, rect.y+1, SizeX-2, SizeY-2, tic_color_yellow-i);
            tic_api_pix(tic, rect.x+3, rect.y+1,tic_color_white, false);
        }
        else if (over)
            tic_api_rect(tic, rect.x+1, rect.y+1, SizeX-2, SizeY-2, tic_color_light_grey);

        tic_api_print(tic, (char[]){'0' + mode, '\0'}, rect.x - 4, rect.y, label_color, false, 1, true);
    }
}

static void drawMoveButtons(Sprite* sprite)
{
    if(hasCanvasSelection(sprite))
    {
        enum { x = 24 };
        enum { y = 20 };

        static const u8 Icons[] = 
        {
            0b00010000,
            0b00111000,
            0b01111100,
            0b11111110,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,

            0b11111110,
            0b01111100,
            0b00111000,
            0b00010000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,

            0b00010000,
            0b00110000,
            0b01110000,
            0b11110000,
            0b01110000,
            0b00110000,
            0b00010000,
            0b00000000,

            0b10000000,
            0b11000000,
            0b11100000,
            0b11110000,
            0b11100000,
            0b11000000,
            0b10000000,
            0b00000000,
        };

        static const tic_rect Rects[] = 
        {
            {x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y - TIC_SPRITESIZE, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
            {x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y + CANVAS_SIZE + TIC_SPRITESIZE/2, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
            {x - TIC_SPRITESIZE, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
            {x + CANVAS_SIZE + TIC_SPRITESIZE/2, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
        };

        static void(* const Func[])(Sprite*) = {upCanvas, downCanvas, leftCanvas, rightCanvas};

        bool down = false;
        for(s32 i = 0; i < sizeof Icons / 8; i++)
        {
            down = false;

            if(checkMousePos(&Rects[i]))
            {
                setCursor(tic_cursor_hand);

                if(checkMouseDown(&Rects[i], tic_mouse_left)) down = true;

                if(checkMouseClick(&Rects[i], tic_mouse_left))
                    Func[i](sprite);
            }

            drawBitIcon(Rects[i].x, Rects[i].y+1, Icons + i*8, down ? tic_color_white : tic_color_black);

            if(!down) drawBitIcon(Rects[i].x, Rects[i].y, Icons + i*8, tic_color_white);
        }
    }
}

static void drawRGBSlider(Sprite* sprite, s32 x, s32 y, u8* value)
{
    tic_mem* tic = sprite->tic;

    enum {Size = CANVAS_SIZE, Max = 255};

    {
        static const u8 Icon[] =
        {
            0b11100000,
            0b11100000,
            0b11100000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x, y-2, Size, 5};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                s32 mx = tic_api_mouse(tic).x - x;
                *value = mx * Max / (Size-1);
            }
        }

        tic_api_rect(tic, x, y+1, Size, 1, tic_color_black);
        tic_api_rect(tic, x, y, Size, 1, tic_color_white);

        {
            s32 offset = x + *value * (Size-1) / Max - 1;
            drawBitIcon(offset, y, Icon, tic_color_black);
            drawBitIcon(offset, y-1, Icon, tic_color_white);
        }

        {
            char buf[] = "FF";
            sprintf(buf, "%02X", *value);
            tic_api_print(tic, buf, x - 18, y - 2, tic_color_light_grey, true, 1, false);
        }
    }

    {
        static const u8 Icon[] =
        {
            0b01000000,
            0b11000000,
            0b01000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x - 4, y - 1, 2, 3};

        bool down = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                (*value)--;
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_white);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_black);
            drawBitIcon(rect.x, rect.y, Icon, tic_color_white);
        }
    }

    {
        static const u8 Icon[] =
        {
            0b10000000,
            0b11000000,
            0b10000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x + Size + 2, y - 1, 2, 3};

        bool down = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                (*value)++;
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_white);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_black);
            drawBitIcon(rect.x, rect.y, Icon, tic_color_white);
        }
    }
}

static void pasteColor(Sprite* sprite)
{
    bool ovr = sprite->palette.ovr;
    fromClipboard(getBankPalette(ovr)->data, sizeof(tic_palette), false, true);
    fromClipboard(&getBankPalette(ovr)->colors[sprite->color], sizeof(tic_rgb), false, true);
}

static void drawRGBTools(Sprite* sprite, s32 x, s32 y)
{
    {
        enum{Size = 5};
        static const u8 Icon[] = 
        {
            0b11110000,
            0b10010000,
            0b10111000,
            0b11101000,
            0b00111000,
            0b00000000,
            0b00000000,
            0b00000000, 
        };

        tic_rect rect = {x, y, Size, Size};

        bool over = false;
                bool down = false;

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("COPY PALETTE");
            over = true;

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                toClipboard(getBankPalette(sprite->palette.ovr)->data, sizeof(tic_palette), false);
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_light_grey);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_black);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_grey : tic_color_white));
        }
    }

    {
        enum{Size = 5};
        static const u8 Icon[] = 
        {
            0b01110000,
            0b10001000,
            0b11111000,
            0b11011000,
            0b11111000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x, y + 8, Size, Size};
        bool over = false;
        bool down = false;

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("PASTE PALETTE");
            over = true;

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                pasteColor(sprite);
            }
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_light_grey);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_black);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_grey : tic_color_white));
        }
    }
}

static void drawRGBSliders(Sprite* sprite, s32 x, s32 y)
{
    enum{Gap = 6, Count = sizeof(tic_rgb)};

    u8* data = &getBankPalette(sprite->palette.ovr)->data[sprite->color * Count];

    for(s32 i = 0; i < Count; i++)
        drawRGBSlider(sprite, x, y + Gap*i, &data[i]);

    drawRGBTools(sprite, x + 74, y);
}

static tic_palette_dimensions getPaletteDimensions(Sprite* sprite)
{

    tic_bpp bpp = sprite->blit.mode;

    s32 cols = bpp == tic_bpp_4 ? PALETTE_COLS : bpp == tic_bpp_2 ? 4 : 2;
    s32 rows = bpp == tic_bpp_4 ? PALETTE_ROWS : 1;

    s32 cell_w = (PALETTE_COLS / cols) * PALETTE_CELL_SIZE;
    s32 cell_h = (PALETTE_ROWS / rows) * PALETTE_CELL_SIZE;

    return (tic_palette_dimensions){cell_w, cell_h, cols, rows, cols*rows};
}

static void drawPaletteOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, PALETTE_WIDTH-1, PALETTE_HEIGHT-1};
    tic_palette_dimensions palette = getPaletteDimensions(sprite);

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = tic_api_mouse(tic).x - x;
        s32 my = tic_api_mouse(tic).y - y;

        mx /= palette.cell_w;
        my /= palette.cell_h;

        s32 index = mx + my * palette.cols;

        SHOW_TOOLTIP("color [%02i]", index);

        bool left = checkMouseDown(&rect, tic_mouse_left);
        bool right = checkMouseDown(&rect, tic_mouse_right);

        if(left || right)
        {
            if(left) sprite->color = index;
            if(right) sprite->color2 = index;
        }
    }

    enum {Gap = 1};

    drawPanelBorder(tic, x - Gap, y - Gap, PaletteW + Gap, PaletteH + Gap);

    for(s32 row = 0, i = 0; row < palette.rows; row++)
        for(s32 col = 0; col < palette.cols; col++)
        {
            tic_api_rectb(tic, x + col * palette.cell_w - Gap, y + row * palette.cell_h - Gap,
                palette.cell_w + Gap, palette.cell_h + Gap, tic_color_black);
        }

    {
        s32 offsetX = x + (sprite->color % PALETTE_COLS) * palette.cell_w;
        s32 offsetY = y + (sprite->color / PALETTE_COLS) * palette.cell_h;
        tic_api_rectb(tic, offsetX - 1, offsetY - 1, palette.cell_w + 1, palette.cell_h + 1, tic_color_white);
    }

    {
        s32 offsetX = x + (sprite->color2 % PALETTE_COLS) * palette.cell_w;
        s32 offsetY = y + (sprite->color2 / PALETTE_COLS) * palette.cell_h;

        for(u8 i=0; i<palette.cell_w+1;i+=2) {
            tic_api_pix(tic, offsetX+i-1, offsetY-1, tic_color_white, false);
            tic_api_pix(tic, offsetX+i-1, offsetY + palette.cell_h-1, tic_color_white, false);
        }

        for(u8 i=0; i<palette.cell_h+1;i+=2) {
            tic_api_pix(tic, offsetX-1, offsetY+i-1, tic_color_white, false);
            tic_api_pix(tic, offsetX+palette.cell_w-1, offsetY + i-1, tic_color_white, false);
        }
    }

    if(sprite->advanced)
    {
        tic_rect rect = {x - 15, y + 1, 11, 5};

        bool down = false;
        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            showTooltip("SCANLINE PALETTE");

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                sprite->palette.ovr = false;
        }

        {
            static const char* Label = "SCN";
            if(!sprite->palette.ovr)
                tic_api_print(tic, Label, rect.x, rect.y + 1, tic_color_black, false, 1, true);

            tic_api_print(tic, Label, rect.x, rect.y, sprite->palette.ovr ? tic_color_dark_grey : tic_color_white, false, 1, true);
        }
    }

    if(sprite->advanced)
    {
        tic_rect rect = {x - 15, y + 9, 11, 5};

        bool down = false;
        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            showTooltip("OVERLINE PALETTE");

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                sprite->palette.ovr = true;
        }

        {
            static const char* Label = "OVR";
            if(sprite->palette.ovr)
                tic_api_print(tic, Label, rect.x, rect.y + 1, tic_color_black, false, 1, true);

            tic_api_print(tic, Label, rect.x, rect.y, sprite->palette.ovr ? tic_color_white : tic_color_dark_grey, false, 1, true);
        }
    }

    if(sprite->advanced)
    {
        static const u8 Icon[] = 
        {
            0b01000000,
            0b11111111,
            0b00000000,
            0b00000010,
            0b11111111,
            0b00000000,
            0b00010000,
            0b11111111,
        };

        tic_rect rect = {x + PALETTE_WIDTH + 3, y + (PALETTE_HEIGHT-8)/2-1, 8, 8};

        bool down = false;
        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            showTooltip("EDIT PALETTE");

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                sprite->palette.edit = !sprite->palette.edit;
        }

        if(sprite->palette.edit || down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, (over ? tic_color_light_grey : tic_color_white));
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_black);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_grey : tic_color_white));            
        }
    }
}

static void drawPalette(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;
    tic_palette_dimensions palette = getPaletteDimensions(sprite);

    for(s32 row = 0, i = 0; row < palette.rows; row++)
        for(s32 col = 0; col < palette.cols; col++)
            tic_api_rect(tic, x + col * palette.cell_w, y + row * palette.cell_h, palette.cell_w-1, palette.cell_h-1, i++);
}

static void selectSprite(Sprite* sprite, s32 x, s32 y)
{
    {
        s32 size = TIC_SPRITESHEET_SIZE - sprite->size;     
        if(x < 0) x = 0;
        if(y < 0) y = 0;
        if(x > size) x = size;
        if(y > size) y = size;
    }
    sprite->x = x / TIC_SPRITESIZE;
    sprite->y = y / TIC_SPRITESIZE;
    updateIndex(sprite);
}

static void updateSpriteSize(Sprite* sprite, s32 size)
{
    if(size != sprite->size)
    {
        sprite->size = size;
        selectSprite(sprite, sprite->x*TIC_SPRITESIZE, sprite->y*TIC_SPRITESIZE);
    }
}

static void drawSheetOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

    tic_api_rectb(tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, tic_color_white);

    for(s32 i = 1; i < rect.h; i += 4)
    {
        if (sprite->blit.page > 0) 
        {
            tic_api_pix(tic, rect.x-1, rect.y + i, tic_color_black, false);
            tic_api_pix(tic, rect.x-1, rect.y + i + 1, tic_color_black, false);
        }

        if (sprite->blit.page < sprite->blit.pages - 1) 
        {
            tic_api_pix(tic, rect.x+rect.w, rect.y + i, tic_color_black, false);
            tic_api_pix(tic, rect.x+rect.w, rect.y + i + 1, tic_color_black, false);
        }
    }

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            s32 offset = (sprite->size - TIC_SPRITESIZE) / 2;
            selectSprite(sprite, tic_api_mouse(tic).x - x - offset, tic_api_mouse(tic).y - y - offset);
        }
    }

    s32 bx = sprite->x*TIC_SPRITESIZE + x - 1;
    s32 by = sprite->y*TIC_SPRITESIZE + y - 1;

    tic_api_rectb(tic, bx, by, sprite->size + 2, sprite->size + 2, tic_color_white);
}

static void drawSheet(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;
    tiles2ram(&tic->ram, sprite->src);
    tic_tool_poke4(&tic->ram.vram.blit, 0, tic_blit_calc_segment(&sprite->blit));
    tic_api_spr(tic, 0, x, y, TIC_SPRITESHEET_COLS, TIC_SPRITESHEET_COLS, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    tic_tool_poke4(&tic->ram.vram.blit, 0, TIC_DEFAULT_BLIT_MODE);
}

static void flipSpriteHorz(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w/2;
    s32 b = rect.y + rect.h;

    for(s32 y = rect.y; y < b; y++)
        for(s32 x = rect.x, i = rect.x + rect.w - 1; x < r; x++, i--)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
            setSheetPixel(sprite, i, y, color);
        }

    history_add(sprite->history);
}

static void flipSpriteVert(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h/2;

    for(s32 y = rect.y, i = rect.y + rect.h - 1; y < b; y++, i--)
        for(s32 x = rect.x; x < r; x++)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
            setSheetPixel(sprite, x, i, color);
        }

    history_add(sprite->history);
}

static void rotateSprite(Sprite* sprite)
{
    const s32 Size = sprite->size;
    u8* buffer = (u8*)malloc(Size * Size);

    if(buffer)
    {
        {
            tic_rect rect = getSpriteRect(sprite);
            s32 r = rect.x + rect.w;
            s32 b = rect.y + rect.h;

            for(s32 y = rect.y, i = 0; y < b; y++)
                for(s32 x = rect.x; x < r; x++)
                    buffer[i++] = getSheetPixel(sprite, x, y);

            for(s32 y = rect.y, j = 0; y < b; y++, j++)
                for(s32 x = rect.x, i = 0; x < r; x++, i++)
                    setSheetPixel(sprite, x, y, buffer[j + (Size-i-1)*Size]);

            history_add(sprite->history);
        }

        free(buffer);
    }
}

static void deleteSprite(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    for(s32 y = rect.y; y < b; y++)
        for(s32 x = rect.x; x < r; x++)
            setSheetPixel(sprite, x, y, sprite->color2);

    clearCanvasSelection(sprite);

    history_add(sprite->history);
}

static void(* const SpriteToolsFunc[])(Sprite*) = {flipSpriteHorz, flipSpriteVert, rotateSprite, deleteSprite};
static void(* const CanvasToolsFunc[])(Sprite*) = {flipCanvasHorz, flipCanvasVert, rotateCanvas, deleteCanvas};

static void drawSpriteTools(Sprite* sprite, s32 x, s32 y)
{
    static const u8 Icons[] =
    {
        0b11101110,
        0b11010110,
        0b11101110,
        0b11101110,
        0b11101110,
        0b11010110,
        0b11101110,
        0b00000000,

        0b11111110,
        0b11111110,
        0b10111010,
        0b01000100,
        0b10111010,
        0b11111110,
        0b11111110,
        0b00000000,

        0b00111000,
        0b01000100,
        0b10010101,
        0b10001110,
        0b10000100,
        0b01000000,
        0b00111000,
        0b00000000,

        0b00111110,
        0b01111111,
        0b00101010,
        0b00101010,
        0b00101010,
        0b00101010,
        0b00111110,
        0b00000000,
    };
    static const char* Tooltips[] = {"FLIP HORZ [5]", "FLIP VERT [6]", "ROTATE [7]", "ERASE [8]"};

    enum{Gap = TIC_SPRITESIZE + 3};

    for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
    {
        bool pushed = false;
        bool over = false;
        
        tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            over = true;

            showTooltip(Tooltips[i]);

            if(checkMouseDown(&rect, tic_mouse_left)) pushed = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {       
                if(hasCanvasSelection(sprite))
                {
                    CanvasToolsFunc[i](sprite);
                }
                else
                {
                    SpriteToolsFunc[i](sprite);
                    clearCanvasSelection(sprite);
                }
            }
        }

        if(pushed)
        {
            drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_grey : tic_color_white));
        }
        else
        {
            drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, tic_color_black);
            drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_grey : tic_color_white));
        }
    }
}

static void drawTools(Sprite* sprite, s32 x, s32 y)
{
    static const u8 Icons[] = 
    {
        0b00001000,
        0b00011100,
        0b00111110,
        0b01111100,
        0b10111000,
        0b10010000,
        0b11100000,
        0b00000000,

        0b00111000,
        0b00111000,
        0b01111100,
        0b00101000,
        0b00101000,
        0b00101000,
        0b00010000,
        0b00000000,

        0b10101010,
        0b00000000,
        0b10000010,
        0b00000000,
        0b10000010,
        0b00000000,
        0b10101010,
        0b00000000,

        0b00001000,
        0b00000100,
        0b00000010,
        0b01111111,
        0b10111110,
        0b10011100,
        0b10001000,
        0b00000000,
    };

    enum{Gap = TIC_SPRITESIZE + 3};

    for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
    {
        tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            static const char* Tooltips[] = {"BRUSH [1]", "COLOR PICKER [2]", "SELECT [3]", "FILL [4]"};

            showTooltip(Tooltips[i]);

            if(checkMouseClick(&rect, tic_mouse_left))
            {               
                sprite->mode = i;

                clearCanvasSelection(sprite);
            }
        }

        bool pushed = i == sprite->mode;

        if(pushed)
        {
            static const u8 Icon[] = 
            {
                0b01111100,
                0b00111000,
                0b00010000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
            };

            drawBitIcon(rect.x, y - 4, Icon, tic_color_black);
            drawBitIcon(rect.x, y - 5, Icon, tic_color_white);

            drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_grey : tic_color_white));
        }
        else
        {
            drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, tic_color_black);
            drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_grey : tic_color_white));
        }
    }

    drawSpriteTools(sprite, x + COUNT_OF(Icons)/BITS_IN_BYTE * Gap + 1, y);
}

static void copyToClipboard(Sprite* sprite)
{
    s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
    u8* buffer = malloc(size);

    if(buffer)
    {
        tic_rect rect = getSpriteRect(sprite);
        s32 r = rect.x + rect.w;
        s32 b = rect.y + rect.h;

        for(s32 y = rect.y, i = 0; y < b; y++)
            for(s32 x = rect.x; x < r; x++)
                tic_tool_poke4(buffer, i++, getSheetPixel(sprite, x, y) & 0xf);

        toClipboard(buffer, size, true);

        free(buffer);
    }
}

static void cutToClipboard(Sprite* sprite)
{
    copyToClipboard(sprite);
    deleteSprite(sprite);
}

static void copyFromClipboard(Sprite* sprite)
{
    if(sprite->palette.edit)
        pasteColor(sprite);

    s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
    u8* buffer = malloc(size);

    if(buffer)
    {
        if(fromClipboard(buffer, size, true, false))
        {
            tic_rect rect = getSpriteRect(sprite);
            s32 r = rect.x + rect.w;
            s32 b = rect.y + rect.h;

            for(s32 y = rect.y, i = 0; y < b; y++)
                for(s32 x = rect.x; x < r; x++)
                    setSheetPixel(sprite, x, y, tic_tool_peek4(buffer, i++));

            history_add(sprite->history);
        }

        free(buffer);

    }
}

static void upSprite(Sprite* sprite)
{
    if (sprite->y > 0) sprite->y--;
    updateIndex(sprite);
}

static void downSprite(Sprite* sprite)
{
    if ((sprite->y + sprite->size/TIC_SPRITESIZE) < TIC_SPRITESHEET_COLS) sprite->y++;
    updateIndex(sprite);
}

static void leftSprite(Sprite* sprite)
{
    if (sprite->x > 0) sprite->x--;
    updateIndex(sprite);
}

static void rightSprite(Sprite* sprite)
{
    if ((sprite->x + sprite->size/TIC_SPRITESIZE) < TIC_SPRITESHEET_COLS) sprite->x++; 
    updateIndex(sprite);
}

static void undo(Sprite* sprite)
{
    history_undo(sprite->history);
}

static void redo(Sprite* sprite)
{
    history_redo(sprite->history);
}

static void switchBanks(Sprite* sprite)
{
    sprite->blit.bank = !sprite->blit.bank;

    updateIndex(sprite);
    initTileSheet(sprite);
}


static void drawTab(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, const u8* icon, bool active, bool over)
{
    tic_color tab_color = active ? tic_color_white : over ? tic_color_light_grey : tic_color_dark_grey;
    tic_color label_color = active ? tic_color_dark_grey : tic_color_grey;

    tic_api_rect(tic, x+1, y, w-1, h, tab_color);
    tic_api_line(tic, x, y+1, x, y+h-2, tab_color);

    if (active)
    {
        tic_api_line(tic, x+1, y + h, x + w-1, y + h, tic_color_black);
        tic_api_pix(tic, x, y-1 + h, label_color, false);
    }

    drawBitIcon(x, y, icon, label_color);
}

static void drawBankTabs(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    if(hasCanvasSelection(sprite)) return;

    enum {Banks = 2, SizeY = 7, SizeX = 9};

    static const char* tooltips[] = {"TILES [tab]", "SPRITES [tab]"};

    static const u8 Icons[] =
    {
        0b00000000,
        0b00101010,
        0b00000000,
        0b00101010,
        0b00000000,
        0b00101010,
        0b00000000,
        0b00000000,

        0b00000000,
        0b00011100,
        0b00101010,
        0b00111110,
        0b00100010,
        0b00011100,
        0b00000000,
        0b00000000,
    };

    for(s32 i = 0; i < Banks; i++)
    {
        bool current = i == sprite->blit.bank;

        tic_rect rect = {x - SizeX, y + (SizeY + 1) * i, SizeX, SizeY};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            showTooltip(tooltips[i]);

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                if (!current) 
                {
                    switchBanks(sprite);
                }
            }
        }

        drawTab(tic, rect.x, rect.y, SizeX, SizeY, Icons + i * BITS_IN_BYTE, current, over);
    }
}

static void processKeyboard(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    if(tic->ram.input.keyboard.data == 0) return;

    switch(getClipboardEvent())
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(sprite); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(sprite); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(sprite); break;
    default: break;
    }

    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    if(ctrl)
    {   
        if(keyWasPressed(tic_key_z))        undo(sprite);
        else if(keyWasPressed(tic_key_y))   redo(sprite);

        else if(keyWasPressed(tic_key_left))    leftViewport(sprite);
        else if(keyWasPressed(tic_key_right))   rightViewport(sprite);

        else if(keyWasPressed(tic_key_tab))
            switchBitMode(sprite, sprite->blit.mode==4 ? 2 : sprite->blit.mode==2 ? 1 : 4);
    }
    else
    {
        if(hasCanvasSelection(sprite))
        {
            if(!sprite->select.drag)
            {
                if(keyWasPressed(tic_key_up))           upCanvas(sprite);
                else if(keyWasPressed(tic_key_down))    downCanvas(sprite);
                else if(keyWasPressed(tic_key_left))    leftCanvas(sprite);
                else if(keyWasPressed(tic_key_right))   rightCanvas(sprite);
                else if(keyWasPressed(tic_key_delete))  deleteCanvas(sprite);                
            }
        }
        else
        {
            if(keyWasPressed(tic_key_up))           upSprite(sprite);
            else if(keyWasPressed(tic_key_down))    downSprite(sprite);
            else if(keyWasPressed(tic_key_left))    leftSprite(sprite);
            else if(keyWasPressed(tic_key_right))   rightSprite(sprite);
            else if(keyWasPressed(tic_key_delete))  deleteSprite(sprite);
            else if(keyWasPressed(tic_key_tab))     switchBanks(sprite);

            if(!sprite->palette.edit)
            {

                if(keyWasPressed(tic_key_1))        sprite->mode = SPRITE_DRAW_MODE;
                else if(keyWasPressed(tic_key_2))   sprite->mode = SPRITE_PICK_MODE;
                else if(keyWasPressed(tic_key_3))   sprite->mode = SPRITE_SELECT_MODE;
                else if(keyWasPressed(tic_key_4))   sprite->mode = SPRITE_FILL_MODE;

                else if(keyWasPressed(tic_key_5))   flipSpriteHorz(sprite);
                else if(keyWasPressed(tic_key_6))   flipSpriteVert(sprite);
                else if(keyWasPressed(tic_key_7))   rotateSprite(sprite);
                else if(keyWasPressed(tic_key_8))   deleteSprite(sprite);

                if(sprite->mode == SPRITE_DRAW_MODE)
                {
                    if(keyWasPressed(tic_key_leftbracket)) {if(sprite->brushSize > 1) sprite->brushSize--;}
                    else if(keyWasPressed(tic_key_rightbracket)) {if(sprite->brushSize < 4) sprite->brushSize++;}
                }               
            }
        }
    }
}


static void drawSpriteToolbar(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    tic_api_rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    // draw sprite size control
    {
        tic_rect rect = {TIC80_WIDTH - 58, 1, 23, 5};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("CANVAS ZOOM");

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                s32 mx = tic_api_mouse(tic).x - rect.x;
                mx /= 6;

                s32 size = 1;
                while(mx--) size <<= 1;

                updateSpriteSize(sprite, size * TIC_SPRITESIZE);
            }
        }

        for(s32 i = 0; i < 4; i++)
            tic_api_rect(tic, rect.x + i*6, 1, 5, 5, tic_color_black);

        tic_api_rect(tic, rect.x, 2, 23, 3, tic_color_black);
        tic_api_rect(tic, rect.x+1, 3, 21, 1, tic_color_white);

        s32 size = sprite->size / TIC_SPRITESIZE, val = 0;
        while(size >>= 1) val++;

        tic_api_rect(tic, rect.x + val*6, 1, 5, 5, tic_color_black);
        tic_api_rect(tic, rect.x+1 + val*6, 2, 3, 3, tic_color_white);
    }

    {
        u8 nbPages = sprite->blit.pages;

        if (nbPages > 1) {
            enum {SizeX = 7, SizeY = TOOLBAR_SIZE};

            for(s32 page = 0; page < nbPages; page++)
            {
                bool active = page == sprite->blit.page;

                tic_rect rect = {TIC80_WIDTH - 1 - 7*(nbPages-page), 0, 7, TOOLBAR_SIZE};

                bool over = false;
                if(checkMousePos(&rect))
                {
                    setCursor(tic_cursor_hand);
                    over = true;

                    SHOW_TOOLTIP("PAGE %i", page + 1);

                    if(checkMouseClick(&rect, tic_mouse_left))
                    {
                        selectViewportPage(sprite, page);
                    }
                }

                if (active) tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_black);
                tic_api_print(tic, (char[]){'1' + page, '\0'}, rect.x + 2, rect.y + 1, active ? tic_color_white : tic_color_grey, false, 1, true);
            }
        }
    }
}

static void tick(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    // process scroll
    {
        tic80_input* input = &tic->ram.input;

        if(input->mouse.scrolly)
        {
            s32 size = sprite->size;
            s32 delta = input->mouse.scrolly;

            if(delta > 0) 
            {
                if(size < (TIC_SPRITESIZE * TIC_SPRITESIZE)) size <<= 1;                    
            }
            else if(size > TIC_SPRITESIZE) size >>= 1;

            updateSpriteSize(sprite, size); 
        }
    }

    processKeyboard(sprite);

    drawCanvas(sprite, CanvasX, CanvasY);
    drawPalette(sprite, PaletteX, PaletteY);
    drawSheet(sprite, SheetX, SheetY);

    sprite->tickCounter++;
}

static void onStudioEvent(Sprite* sprite, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT: cutToClipboard(sprite); break;
    case TIC_TOOLBAR_COPY: copyToClipboard(sprite); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(sprite); break;
    case TIC_TOOLBAR_UNDO: undo(sprite); break;
    case TIC_TOOLBAR_REDO: redo(sprite); break;
    }
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Sprite* sprite = (Sprite*)data;
    
    if(row == 0)
        memcpy(&tic->ram.vram.palette, getBankPalette(sprite->palette.ovr), sizeof(tic_palette));
}

static void drawAdvancedButton(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, 8, 5};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);
        over = true;
        showTooltip("ADVANCED MODE");

        if(checkMouseClick(&rect, tic_mouse_left))
            sprite->advanced = !sprite->advanced;

        if(!sprite->advanced)
            sprite->palette.edit = false;
    }

    enum {Size = 3, Gap = 1};

    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_black);
    tic_api_rect(tic, rect.x + Gap + (sprite->advanced ? Size : 0), rect.y + Gap, Size, Size, over ? tic_color_light_grey : tic_color_grey);
}

static void overline(tic_mem* tic, void* data)
{
    static const tic_rect bg[] = 
    {
        {0, ToolbarH, SheetX, CanvasY-ToolbarH},
        {0, CanvasY, CanvasX, CanvasH},
        {CanvasX + CanvasW, CanvasY, SheetX - (CanvasX + CanvasW), CanvasH},

        {0, CanvasY + CanvasH, SheetX, PaletteY - CanvasY - CanvasH},

        {0, PaletteY, PaletteX, PaletteH},
        {PaletteX + PaletteW, PaletteY, SheetX - PaletteX - PaletteW, PaletteH},

        {0, PaletteY + PaletteH, SheetX, TIC80_HEIGHT - PaletteY - PaletteH},
    };

    Sprite* sprite = (Sprite*)data;

    for(const tic_rect* r = bg; r < bg + COUNT_OF(bg); r++)
        tic_api_rect(tic, r->x, r->y, r->w, r->h, tic_color_grey);

    drawCanvasOvr(sprite, 24, 20);
    drawMoveButtons(sprite);

    if(sprite->advanced)
    {
        if(sprite->blit.mode == 4)
            drawFlags(sprite, 24+64+7, 20+8);

        drawBitMode(sprite, PaletteX, PaletteY + PaletteH + 2, PaletteW, 8);        
    }

    drawBankTabs(sprite, SheetX, 8);

    sprite->palette.edit 
        ? drawRGBSliders(sprite, 24, 91) 
        : drawTools(sprite, 12, 96);

    drawPaletteOvr(sprite, 24, 112);
    drawSheetOvr(sprite, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, 7);
    drawAdvancedButton(sprite, 4, 11);
    
    drawSpriteToolbar(sprite);
    drawToolbar(tic, false);
}

void initSprite(Sprite* sprite, tic_mem* tic, tic_tiles* src)
{
    if(sprite->select.back == NULL) sprite->select.back = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
    if(sprite->select.front == NULL) sprite->select.front = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
    if(sprite->history) history_delete(sprite->history);
    
    *sprite = (Sprite)
    {
        .tic = tic,
        .tick = tick,
        .tickCounter = 0,
        .src = src,
        .x = 1,
        .y = 0,
        .advanced = false,
        .blit = {0},
        .color = 2,
        .color2 = 0,
        .size = TIC_SPRITESIZE,
        .palette.edit = false,
        .brushSize = 1,
        .select = 
        {
            .rect = {0,0,0,0},
            .start = {0,0},
            .drag = false,
            .back = sprite->select.back,
            .front = sprite->select.front,
        },
        .mode = SPRITE_DRAW_MODE,
        .history = history_create(src, TIC_SPRITES * sizeof(tic_tile)),
        .event = onStudioEvent,
        .overline = overline,
        .scanline = scanline,
    };
    switchBitMode(sprite, TIC_DEFAULT_BIT_DEPTH);
}

void freeSprite(Sprite* sprite)
{
    free(sprite->select.back);
    free(sprite->select.front);
    history_delete(sprite->history);
    free(sprite);
}
