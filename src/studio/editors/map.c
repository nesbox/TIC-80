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

#include "map.h"
#include "ext/history.h"

#define MAP_WIDTH (TIC80_WIDTH)
#define MAP_HEIGHT (TIC80_HEIGHT - TOOLBAR_SIZE)
#define MAP_X (0)
#define MAP_Y (TOOLBAR_SIZE)

#define MAX_SCROLL_X (TIC_MAP_WIDTH * TIC_SPRITESIZE)
#define MAX_SCROLL_Y (TIC_MAP_HEIGHT * TIC_SPRITESIZE)

#define ICON_SIZE 7

#define MIN_SCALE 1
#define MAX_SCALE 4
#define FILL_STACK_SIZE (TIC_MAP_WIDTH*TIC_MAP_HEIGHT)

static void normalizeMap(s32* x, s32* y)
{
    while(*x < 0) *x += MAX_SCROLL_X;
    while(*y < 0) *y += MAX_SCROLL_Y;
    while(*x >= MAX_SCROLL_X) *x -= MAX_SCROLL_X;
    while(*y >= MAX_SCROLL_Y) *y -= MAX_SCROLL_Y;
}

static tic_point getTileOffset(Map* map)
{
    return (tic_point){(map->sheet.rect.w - 1)*TIC_SPRITESIZE / 2, (map->sheet.rect.h - 1)*TIC_SPRITESIZE / 2};
}

static void getMouseMap(Map* map, s32* x, s32* y)
{
    tic_mem* tic = map->tic;
    tic_point offset = getTileOffset(map);

    s32 mx = tic_api_mouse(tic).x + map->scroll.x - offset.x;
    s32 my = tic_api_mouse(tic).y + map->scroll.y - offset.y;

    normalizeMap(&mx, &my);

    *x = mx / TIC_SPRITESIZE;
    *y = my / TIC_SPRITESIZE;
}

static s32 drawWorldButton(Map* map, s32 x, s32 y)
{
    enum{Size = 8};

    x -= Size;

    tic_rect rect = {x, y, Size, ICON_SIZE};

    bool over = false;

    if(checkMousePos(map->studio, &rect))
    {
        setCursor(map->studio, tic_cursor_hand);

        over = true;

        showTooltip(map->studio, "WORLD MAP [tab]");

        if(checkMouseClick(map->studio, &rect, tic_mouse_left))
            setStudioMode(map->studio, TIC_WORLD_MODE);
    }

    drawBitIcon(map->studio, tic_icon_world, x, y, over ? tic_color_grey : tic_color_light_grey);

    return x;

}

static s32 drawGridButton(Map* map, s32 x, s32 y)
{
    x -= ICON_SIZE;

    tic_rect rect = {x, y, ICON_SIZE, ICON_SIZE};

    bool over = false;

    if(checkMousePos(map->studio, &rect))
    {
        setCursor(map->studio, tic_cursor_hand);

        over = true;

        showTooltip(map->studio, "SHOW/HIDE GRID [`]");

        if(checkMouseClick(map->studio, &rect, tic_mouse_left))
            map->canvas.grid = !map->canvas.grid;
    }

    drawBitIcon(map->studio, tic_icon_grid, x, y, map->canvas.grid ? tic_color_black : over ? tic_color_grey : tic_color_light_grey);

    return x;
}

static inline bool isIdle(Map* map)
{
    return map->anim.movie == &map->anim.idle;
}

static inline bool sheetVisible(Map* map)
{
    return map->anim.pos.sheet >= 0;
}

static s32 drawSheetButton(Map* map, s32 x, s32 y)
{
    x -= ICON_SIZE;

    tic_rect rect = {x, y, ICON_SIZE, ICON_SIZE};

    bool over = false;
    if(checkMousePos(map->studio, &rect))
    {
        setCursor(map->studio, tic_cursor_hand);

        over = true;
        showTooltip(map->studio, "SHOW TILES [shift]");

        if(isIdle(map) && checkMouseClick(map->studio, &rect, tic_mouse_left))
        {
            map->anim.movie = resetMovie(sheetVisible(map) ? &map->anim.hide : &map->anim.show);
            map->sheet.keep = true;
        }
    }

    drawBitIcon(map->studio, sheetVisible(map) ? tic_icon_up : tic_icon_down, rect.x, rect.y, 
        over ? tic_color_grey : tic_color_light_grey);

    return x;
}

static s32 drawToolButton(Map* map, s32 x, s32 y, u8 icon, s32 width, const char* tip, s32 mode)
{
    x -= width;

    tic_rect rect = {x, y, width, ICON_SIZE};

    bool over = false;
    if(checkMousePos(map->studio, &rect))
    {
        setCursor(map->studio, tic_cursor_hand);

        over = true;

        showTooltip(map->studio, tip);

        if(checkMouseClick(map->studio, &rect, tic_mouse_left))
        {
            map->mode = mode;
        }
    }

    drawBitIcon(map->studio, icon, rect.x, rect.y, map->mode == mode ? tic_color_black : over ? tic_color_grey : tic_color_light_grey);

    return x;
}

static s32 drawFillButton(Map* map, s32 x, s32 y)
{
    enum{Size = 8};

    return drawToolButton(map, x, y, tic_icon_fill, Size, "FILL [4]", MAP_FILL_MODE);
}

static s32 drawSelectButton(Map* map, s32 x, s32 y)
{
    return drawToolButton(map, x, y, tic_icon_select, ICON_SIZE, "SELECT [3]", MAP_SELECT_MODE);
}

static s32 drawHandButton(Map* map, s32 x, s32 y)
{
    return drawToolButton(map, x, y, tic_icon_hand, ICON_SIZE, "DRAG MAP [2]", MAP_DRAG_MODE);
}

static s32 drawPenButton(Map* map, s32 x, s32 y)
{
    return drawToolButton(map, x, y, tic_icon_pen, ICON_SIZE, "DRAW [1]", MAP_DRAW_MODE);
}

static void drawTileIndex(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;
    s32 index = -1;

    if(sheetVisible(map))
    {
        tic_rect rect = {TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, TOOLBAR_SIZE, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};
        
        if(checkMousePos(map->studio, &rect))
        {
            s32 mx = tic_api_mouse(tic).x - rect.x;
            s32 my = tic_api_mouse(tic).y - rect.y;

            mx /= TIC_SPRITESIZE;
            my /= TIC_SPRITESIZE;

            index = my * map->sheet.blit.pages * TIC_SPRITESHEET_COLS + mx + tic_blit_calc_index(&map->sheet.blit);
        }
    }
    else
    {
        tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

        if(checkMousePos(map->studio, &rect))
        {
            s32 tx = 0, ty = 0;
            getMouseMap(map, &tx, &ty);
            map2ram(tic->ram, map->src);
            index = tic_api_mget(map->tic, tx, ty);
        }
    }

    if(index >= 0)
    {
        char buf[sizeof "#9999"];
        sprintf(buf, "#%03i", index);
        tic_api_print(map->tic, buf, x, y, tic_color_light_grey, true, 1, false);
    }
}

static void drawBppButtons(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;

    static const char Labels[] = "421";

    for(s32 i = 0; i < sizeof Labels - 1; i++)
    {
        tic_rect rect = {x + i * TIC_ALTFONT_WIDTH, y, TIC_ALTFONT_WIDTH, TIC_FONT_HEIGHT};
        tic_bpp mode = 1 << (2 - i);

        bool hover = false;
        if(checkMousePos(map->studio, &rect))
        {
            setCursor(map->studio, tic_cursor_hand);
            hover = true;

            if(mode > 1)
                SHOW_TOOLTIP(map->studio, "%iBITS PER PIXEL", mode);
            else
                SHOW_TOOLTIP(map->studio, "%iBIT PER PIXEL", mode);

            if(checkMouseClick(map->studio, &rect, tic_mouse_left))
            {
                tic_blit_update_bpp(&map->sheet.blit, mode);
            }
        }

        const char* label = (char[]){Labels[i], '\0'};
        tic_api_print(tic, label, rect.x, rect.y, 
            mode == map->sheet.blit.mode 
                ? tic_color_dark_grey 
                : hover 
                    ? tic_color_grey 
                    : tic_color_light_grey, 
            true, 1, true);   
    }
}

static void drawBankButtons(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;

    enum{Size = 6};

    static const u8 Icons[] = {tic_icon_tiles, tic_icon_sprites};

    for(s32 i = 0; i < COUNT_OF(Icons); i++)
    {
        tic_rect rect = {x + i * Size, y, Size, Size};

        bool hover = false;
        if(checkMousePos(map->studio, &rect))
        {
            setCursor(map->studio, tic_cursor_hand);
            hover = true;

            showTooltip(map->studio, i ? "SPRITES" : "TILES");

            if(isIdle(map) && checkMouseClick(map->studio, &rect, tic_mouse_left))
            {
                Anim* anim = map->anim.bank.items;
                anim->start = (i - map->sheet.blit.bank) * TIC_SPRITESHEET_SIZE;
                map->anim.movie = resetMovie(&map->anim.bank);

                map->sheet.blit.bank = i;
            }
        }

        drawBitIcon(map->studio, Icons[i], rect.x, rect.y, 
            i == map->sheet.blit.bank 
                ? tic_color_dark_grey 
                : hover 
                    ? tic_color_grey 
                    : tic_color_light_grey);
    }
}

static void drawPagesButtons(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;

    enum{Width = TIC_ALTFONT_WIDTH + 1, Height = TOOLBAR_SIZE};

    for(s32 i = 0; i < map->sheet.blit.pages; i++)
    {
        tic_rect rect = {x + i * Width - 1, y, Width, Height};

        bool hover = false;
        if(checkMousePos(map->studio, &rect))
        {
            setCursor(map->studio, tic_cursor_hand);
            hover = true;

            SHOW_TOOLTIP(map->studio, "PAGE %i", i);

            if(isIdle(map) && checkMouseClick(map->studio, &rect, tic_mouse_left))
            {
                Anim* anim = map->anim.page.items;
                anim->start = (i - map->sheet.blit.page) * TIC_SPRITESHEET_SIZE;
                map->anim.movie = resetMovie(&map->anim.page);

                map->sheet.blit.page = i;
            }
        }

        bool active = i == map->sheet.blit.page;
        if(active)
        {
            tic_api_rect(tic, rect.x, rect.y, Width, Height, tic_color_black);
        }

        const char* label = (char[]){i + '1', '\0'};
        tic_api_print(tic, label, rect.x + 1, rect.y + 1, 
            active
                ? tic_color_white
                : hover 
                    ? tic_color_grey 
                    : tic_color_light_grey, 
            true, 1, true);   
    }        
}

static void drawMapToolbar(Map* map, s32 x, s32 y)
{
    tic_api_rect(map->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    drawTileIndex(map, TIC80_WIDTH/2 - TIC_FONT_WIDTH, y);

    x = drawSheetButton(map, x, 0);

    if(sheetVisible(map))
    {
        drawBankButtons(map, 183, 0);
        drawBppButtons(map, 199, 1);

        if(map->sheet.blit.pages > 1)
            drawPagesButtons(map, map->sheet.blit.pages == 4 ? 213 : 222, 0);
    }
    else
    {
        x = drawFillButton(map, x, 0);
        x = drawSelectButton(map, x, 0);
        x = drawHandButton(map, x, 0);
        x = drawPenButton(map, x, 0);

        x = drawGridButton(map, x - 5, 0);
        drawWorldButton(map, x, 0);        
    }
}

static void drawSheetVBank1(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;
    const tic_blit* blit = &map->sheet.blit;

    tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

    tic_api_rectb(map->tic, rect.x - 1, rect.y - 1 + map->anim.pos.sheet, rect.w + 2, rect.h + 2, tic_color_white);

    for(s32 i = 1; i < rect.h; i += 4)
    {
        if (blit->page > 0) 
        {
            tic_api_pix(tic, rect.x-1, rect.y + i, tic_color_black, false);
            tic_api_pix(tic, rect.x-1, rect.y + i + 1, tic_color_black, false);
        }

        if (blit->page < blit->pages - 1) 
        {
            tic_api_pix(tic, rect.x+rect.w, rect.y + i, tic_color_black, false);
            tic_api_pix(tic, rect.x+rect.w, rect.y + i + 1, tic_color_black, false);
        }
    }

    {
        s32 bx = map->sheet.rect.x * TIC_SPRITESIZE - 1 + x;
        s32 by = map->sheet.rect.y * TIC_SPRITESIZE - 1 + y;
        s32 bw = map->sheet.rect.w * TIC_SPRITESIZE + 2;
        s32 bh = map->sheet.rect.h * TIC_SPRITESIZE + 2;

        tic_api_rectb(map->tic, bx, by + map->anim.pos.sheet, bw, bh, tic_color_white);
    }
}

static void initBlitMode(Map* map)
{
    tic_mem* tic = map->tic;
    tiles2ram(tic->ram, getBankTiles(map->studio));
    tic->ram->vram.blit.segment = tic_blit_calc_segment(&map->sheet.blit);
}

static void resetBlitMode(tic_mem* tic)
{
    tic->ram->vram.blit.segment = TIC_DEFAULT_BLIT_MODE;
}

static void drawSheetReg(Map* map, s32 x, s32 y)
{
    tic_mem* tic = map->tic;

    tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

    if(isIdle(map) && sheetVisible(map) && checkMousePos(map->studio, &rect))
    {
        setCursor(map->studio, tic_cursor_hand);

        if(checkMouseDown(map->studio, &rect, tic_mouse_left))
        {
            s32 mx = tic_api_mouse(tic).x - rect.x;
            s32 my = tic_api_mouse(tic).y - rect.y;

            mx /= TIC_SPRITESIZE;
            my /= TIC_SPRITESIZE;

            if(map->sheet.drag)
            {
                s32 rl = MIN(mx, map->sheet.start.x);
                s32 rt = MIN(my, map->sheet.start.y);
                s32 rr = MAX(mx, map->sheet.start.x);
                s32 rb = MAX(my, map->sheet.start.y);

                map->sheet.rect = (tic_rect){rl, rt, rr-rl+1, rb-rt+1};

                map->mode = MAP_DRAW_MODE;
            }
            else
            {
                map->sheet.drag = true;
                map->sheet.start = (tic_point){mx, my};
            }
        }
        else
        {
            if(map->sheet.keep && map->sheet.drag)
                map->anim.movie = resetMovie(&map->anim.hide);
            
            map->sheet.drag = false;
        }
    }

    tic_api_clip(tic, x, y + map->anim.pos.sheet, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE);

    tiles2ram(tic->ram, getBankTiles(map->studio));

    tic_blit blit = map->sheet.blit;
    SCOPE(resetBlitMode(map->tic), tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT))
    {
        tic_point start = 
        {
            x - blit.page * TIC_SPRITESHEET_SIZE + map->anim.pos.page,
            y - blit.bank * TIC_SPRITESHEET_SIZE + map->anim.pos.bank
        }, pos = start;

        for(blit.bank = 0; blit.bank < TIC_SPRITE_BANKS; ++blit.bank, pos.y += TIC_SPRITESHEET_SIZE, pos.x = start.x)
        {
            for(blit.page = 0; blit.page < blit.pages; ++blit.page, pos.x += TIC_SPRITESHEET_SIZE)
            {
                tic->ram->vram.blit.segment = tic_blit_calc_segment(&blit);
                tic_api_spr(tic, 0, pos.x, pos.y + map->anim.pos.sheet, TIC_SPRITESHEET_COLS, TIC_SPRITESHEET_COLS, NULL, 0, 1, tic_no_flip, tic_no_rotate);
            }
        }
    }
}

static void drawCursorPos(Map* map, s32 x, s32 y)
{
    char pos[sizeof "999:999"];

    s32 tx = 0, ty = 0;
    getMouseMap(map, &tx, &ty);

    sprintf(pos, "%03i:%03i", tx, ty);

    s32 width = tic_api_print(map->tic, pos, TIC80_WIDTH, 0, tic_color_dark_green, true, 1, false);

    s32 px = x + (TIC_SPRITESIZE + 3);
    if(px + width >= TIC80_WIDTH) px = x - (width + 2);

    s32 py = y - (TIC_FONT_HEIGHT + 2);
    if(py <= TOOLBAR_SIZE) py = y + (TIC_SPRITESIZE + 3);

    tic_api_rect(map->tic, px - 1, py - 1, width + 1, TIC_FONT_HEIGHT + 1, tic_color_white);
    tic_api_print(map->tic, pos, px, py, tic_color_light_grey, true, 1, false);

    if(map->mode == MAP_FILL_MODE && tic_api_key(map->tic, tic_key_ctrl))
    {
        tic_api_rect(map->tic, px - 1, py - 1 + TIC_FONT_HEIGHT, width + 1, TIC_FONT_HEIGHT + 1, tic_color_white);
        tic_api_print(map->tic, "replace", px, py + TIC_FONT_HEIGHT, tic_color_dark_blue, true, 1, false);
    }
}

static inline void ram2map(const tic_ram* ram, tic_map* src)
{
    memcpy(src, ram->map.data, sizeof ram->map);
}

static void setMapSprite(Map* map, s32 x, s32 y)
{
    s32 mx = map->sheet.rect.x;
    s32 my = map->sheet.rect.y;


    for(s32 j = 0; j < map->sheet.rect.h; j++)
        for(s32 i = 0; i < map->sheet.rect.w; i++)
            tic_api_mset(map->tic, (x+i)%TIC_MAP_WIDTH, (y+j)%TIC_MAP_HEIGHT, (mx+i) + (my+j) * TIC_SPRITESHEET_COLS);

    ram2map(map->tic->ram, map->src);

    history_add(map->history);
}

static tic_point getCursorPos(Map* map)
{
    tic_mem* tic = map->tic;
    tic_point offset = getTileOffset(map);

    s32 mx = tic_api_mouse(tic).x + map->scroll.x - offset.x;
    s32 my = tic_api_mouse(tic).y + map->scroll.y - offset.y;

    mx -= mx % TIC_SPRITESIZE;
    my -= my % TIC_SPRITESIZE;

    mx += -map->scroll.x;
    my += -map->scroll.y;

    return (tic_point){mx, my};
}

static void drawTileCursor(Map* map)
{
    tic_mem* tic = map->tic;

    if(map->scroll.active)
        return;

    tic_point pos = getCursorPos(map);

    {
        s32 sx = map->sheet.rect.x;
        s32 sy = map->sheet.rect.y;

        initBlitMode(map);
        tic_api_spr(tic, sx + map->sheet.blit.pages * sy * TIC_SPRITESHEET_COLS, pos.x, pos.y, map->sheet.rect.w, map->sheet.rect.h, NULL, 0, 1, tic_no_flip, tic_no_rotate);
        resetBlitMode(map->tic);
    }
}

static void drawTileCursorVBank1(Map* map)
{
    if(map->scroll.active)
        return;

    tic_point pos = getCursorPos(map);

    {
        s32 width = map->sheet.rect.w * TIC_SPRITESIZE + 2;
        s32 height = map->sheet.rect.h * TIC_SPRITESIZE + 2;
        tic_api_rectb(map->tic, pos.x - 1, pos.y - 1, width, height, tic_color_white);
    }

    drawCursorPos(map, pos.x, pos.y);
}

static void processMouseDrawMode(Map* map)
{
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    setCursor(map->studio, tic_cursor_hand);

    drawTileCursor(map);

    if(checkMouseDown(map->studio, &rect, tic_mouse_left))
    {
        s32 tx = 0, ty = 0;
        getMouseMap(map, &tx, &ty);

        if(map->canvas.draw)
        {
            s32 w = tx - map->canvas.start.x;
            s32 h = ty - map->canvas.start.y;

            if(w % map->sheet.rect.w == 0 && h % map->sheet.rect.h == 0)
                setMapSprite(map, tx, ty);
        }
        else
        {
            map->canvas.draw    = true;
            map->canvas.start = (tic_point){tx, ty};
        }
    }
    else
    {
        map->canvas.draw    = false;
    }

    if(checkMouseDown(map->studio, &rect, tic_mouse_middle))
    {
        s32 tx = 0, ty = 0;
        getMouseMap(map, &tx, &ty);

        tic_mem* tic = map->tic;
        map2ram(tic->ram, map->src);
        s32 index = tic_api_mget(map->tic, tx, ty);

        map->sheet.rect = (tic_rect){index % TIC_SPRITESHEET_COLS, index / TIC_SPRITESHEET_COLS, 1, 1};
    }
}

static void processScrolling(Map* map, bool pressed)
{
    tic_mem* tic = map->tic;
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    if(map->scroll.active)
    {
        if(pressed)
        {
            map->scroll.x = map->scroll.start.x - tic_api_mouse(tic).x;
            map->scroll.y = map->scroll.start.y - tic_api_mouse(tic).y;

            normalizeMap(&map->scroll.x, &map->scroll.y);

            setCursor(map->studio, tic_cursor_hand);
        }
        else map->scroll.active = false;
    }
    else if(checkMousePos(map->studio, &rect))
    {
        if(pressed)
        {
            map->scroll.active = true;

            map->scroll.start.x = tic_api_mouse(tic).x + map->scroll.x;
            map->scroll.start.y = tic_api_mouse(tic).y + map->scroll.y;
        }
    }
}

static void processMouseDragMode(Map* map)
{
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    processScrolling(map, checkMouseDown(map->studio, &rect, tic_mouse_left) || 
        checkMouseDown(map->studio, &rect, tic_mouse_right));
}

static void resetSelection(Map* map)
{
    map->select.rect = (tic_rect){0,0,0,0};
}

static void drawSelectionRect(Map* map, s32 x, s32 y, s32 w, s32 h)
{
    enum{Step = 3};
    u8 color = tic_color_white;

    s32 index = map->tickCounter / 10;
    for(s32 i = x; i < (x+w); i++)      {tic_api_pix(map->tic, i, y, index++ % Step ? color : 0, false);} index++;
    for(s32 i = y; i < (y+h); i++)      {tic_api_pix(map->tic, x + w-1, i, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (x+w-1); i >= x; i--)   {tic_api_pix(map->tic, i, y + h-1, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (y+h-1); i >= y; i--)   {tic_api_pix(map->tic, x, i, index++ % Step ? color : 0, false);}
}

static void drawPasteData(Map* map)
{
    tic_mem* tic = map->tic;

    s32 w = map->paste[0];
    s32 h = map->paste[1];

    u8* data = map->paste + 2;

    s32 mx = tic_api_mouse(tic).x + map->scroll.x - (w - 1)*TIC_SPRITESIZE / 2;
    s32 my = tic_api_mouse(tic).y + map->scroll.y - (h - 1)*TIC_SPRITESIZE / 2;

    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    if(checkMouseClick(map->studio, &rect, tic_mouse_left))
    {
        normalizeMap(&mx, &my);

        mx /= TIC_SPRITESIZE;
        my /= TIC_SPRITESIZE;

        for(s32 j = 0; j < h; j++)
            for(s32 i = 0; i < w; i++)
                tic_api_mset(tic, (mx+i)%TIC_MAP_WIDTH, (my+j)%TIC_MAP_HEIGHT, data[i + j * w]);

        ram2map(tic->ram, map->src);

        history_add(map->history);

        free(map->paste);
        map->paste = NULL;
    }
    else
    {
        mx -= mx % TIC_SPRITESIZE;
        my -= my % TIC_SPRITESIZE;

        mx += -map->scroll.x;
        my += -map->scroll.y;

        initBlitMode(map);

        for(s32 j = 0; j < h; j++)
            for(s32 i = 0; i < w; i++)
            {
                s32 index = data[i + j * w];
                s32 sx = index % TIC_SPRITESHEET_COLS;
                s32 sy = index / TIC_SPRITESHEET_COLS;
                tic_api_spr(tic, sx + map->sheet.blit.pages * sy * TIC_SPRITESHEET_COLS, 
                    mx + i * TIC_SPRITESIZE, my + j * TIC_SPRITESIZE, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
            }

        resetBlitMode(map->tic);
    }
}

static void drawPasteDataVBank1(Map* map)
{
    tic_mem* tic = map->tic;
    s32 w = map->paste[0];
    s32 h = map->paste[1];

    s32 mx = tic_api_mouse(tic).x + map->scroll.x - (w - 1) * TIC_SPRITESIZE / 2;
    s32 my = tic_api_mouse(tic).y + map->scroll.y - (h - 1) * TIC_SPRITESIZE / 2;

    mx -= mx % TIC_SPRITESIZE;
    my -= my % TIC_SPRITESIZE;

    mx += -map->scroll.x;
    my += -map->scroll.y;

    drawSelectionRect(map, mx - 1, my - 1, w * TIC_SPRITESIZE + 2, h * TIC_SPRITESIZE + 2);
}

static void normalizeMapRect(s32* x, s32* y)
{
    while(*x < 0) *x += TIC_MAP_WIDTH;
    while(*y < 0) *y += TIC_MAP_HEIGHT;
    while(*x >= TIC_MAP_WIDTH) *x -= TIC_MAP_WIDTH;
    while(*y >= TIC_MAP_HEIGHT) *y -= TIC_MAP_HEIGHT;
}

static void processMouseSelectMode(Map* map)
{
    tic_mem* tic = map->tic;
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    if(checkMousePos(map->studio, &rect))
    {
        if(map->paste)
            drawPasteData(map);
        else
        {
            if(checkMouseDown(map->studio, &rect, tic_mouse_left))
            {
                s32 mx = tic_api_mouse(tic).x + map->scroll.x;
                s32 my = tic_api_mouse(tic).y + map->scroll.y;

                mx /= TIC_SPRITESIZE;
                my /= TIC_SPRITESIZE;

                if(map->select.drag)
                {
                    s32 rl = MIN(mx, map->select.start.x);
                    s32 rt = MIN(my, map->select.start.y);
                    s32 rr = MAX(mx, map->select.start.x);
                    s32 rb = MAX(my, map->select.start.y);

                    map->select.rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};
                }
                else
                {
                    map->select.drag = true;
                    map->select.start = (tic_point){mx, my};
                    map->select.rect = (tic_rect){map->select.start.x, map->select.start.y, 1, 1};
                }
            }
            else if(map->select.drag)
            {
                map->select.drag = false;

                if(map->select.rect.w <= 1 && map->select.rect.h <= 1)
                    resetSelection(map);
            }
        }
    }
}

typedef struct
{
    tic_point* data;
    tic_point* head;
} FillStack;

static bool push(FillStack* stack, s32 x, s32 y)
{
    if(stack->head == NULL)
    {
        stack->head = stack->data;
        stack->head->x = x;
        stack->head->y = y;

        return true;
    }

    if(stack->head < (stack->data + FILL_STACK_SIZE-1))
    {       
        stack->head++;
        stack->head->x = x;
        stack->head->y = y;

        return true;
    }

    return false;
}

static bool pop(FillStack* stack, s32* x, s32* y)
{
    if(stack->head > stack->data)
    {
        *x = stack->head->x;
        *y = stack->head->y;

        stack->head--;

        return true;
    }

    if(stack->head == stack->data)
    {
        *x = stack->head->x;
        *y = stack->head->y;

        stack->head = NULL;

        return true;
    }

    return false;
}

static void fillMap(Map* map, s32 x, s32 y, u8 tile)
{
    if(tile == (map->sheet.rect.x + map->sheet.rect.y * TIC_SPRITESHEET_COLS)) return;

    static FillStack stack = {NULL, NULL};

    if(!stack.data)
        stack.data = (tic_point*)malloc(FILL_STACK_SIZE * sizeof(tic_point));

    stack.head = NULL;

    static const s32 dx[4] = {0, 1, 0, -1};
    static const s32 dy[4] = {-1, 0, 1, 0};

    if(!push(&stack, x, y)) return;

    s32 mx = map->sheet.rect.x;
    s32 my = map->sheet.rect.y;

    struct
    {
        s32 l;
        s32 t;
        s32 r;
        s32 b;
    }clip = { 0, 0, TIC_MAP_WIDTH, TIC_MAP_HEIGHT };

    if (map->select.rect.w > 0 && map->select.rect.h > 0)
    {
        clip.l = map->select.rect.x;
        clip.t = map->select.rect.y;
        clip.r = map->select.rect.x + map->select.rect.w;
        clip.b = map->select.rect.y + map->select.rect.h;
    }


    while(pop(&stack, &x, &y))
    {
        for(s32 j = 0; j < map->sheet.rect.h; j++)
            for(s32 i = 0; i < map->sheet.rect.w; i++)
                tic_api_mset(map->tic, x+i, y+j, (mx+i) + (my+j) * TIC_SPRITESHEET_COLS);

        for(s32 i = 0; i < COUNT_OF(dx); i++) 
        {
            s32 nx = x + dx[i]*map->sheet.rect.w;
            s32 ny = y + dy[i]*map->sheet.rect.h;

            if(nx >= clip.l && nx < clip.r && ny >= clip.t && ny < clip.b) 
            {
                bool match = true;
                for(s32 j = 0; j < map->sheet.rect.h; j++)
                    for(s32 i = 0; i < map->sheet.rect.w; i++)
                        if(tic_api_mget(map->tic, nx+i, ny+j) != tile)
                            match = false;

                if(match)
                {
                    if(!push(&stack, nx, ny)) return;
                }
            }
        }
    }   
}

static s32 moduloWrap(s32 x, s32 m)
{
   s32 y = x % m;
   return (y < 0) ? (y + m) : y; // always between 0 and m-1 inclusive
}

// replace tile with another tile or pattern
static void replaceTile(Map* map, s32 x, s32 y, u8 tile)
{
    if(tile == (map->sheet.rect.x + map->sheet.rect.y * TIC_SPRITESHEET_COLS)) return;

    s32 mx = map->sheet.rect.x;
    s32 my = map->sheet.rect.y;

    struct
    {
        s32 l;
        s32 t;
        s32 r;
        s32 b;
    } clip = { 0, 0, TIC_MAP_WIDTH, TIC_MAP_HEIGHT };

    if (map->select.rect.w > 0 && map->select.rect.h > 0)
    {
        clip.l = map->select.rect.x;
        clip.t = map->select.rect.y;
        clip.r = map->select.rect.x + map->select.rect.w;
        clip.b = map->select.rect.y + map->select.rect.h;
    }

    // for each tile in selection/full map
    for(s32 j = clip.t; j < clip.b; j++)
        for(s32 i = clip.l; i < clip.r; i++)
            if(tic_api_mget(map->tic, i, j) == tile)
            {
                // offset pattern based on click position
                s32 oy = moduloWrap(j - y, map->sheet.rect.h);
                s32 ox = moduloWrap(i - x, map->sheet.rect.w);

                u8 newtile = (mx + ox) + (my + oy) * TIC_SPRITESHEET_COLS;
                tic_api_mset(map->tic, i, j, newtile);
            }
}

static void processMouseFillMode(Map* map)
{
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    setCursor(map->studio, tic_cursor_hand);

    drawTileCursor(map);

    if(checkMouseClick(map->studio, &rect, tic_mouse_left))
    {
        s32 tx = 0, ty = 0;
        getMouseMap(map, &tx, &ty);

        {
            tic_mem* tic = map->tic;
            map2ram(tic->ram, map->src);
            if(tic_api_key(tic, tic_key_ctrl))
                replaceTile(map, tx, ty, tic_api_mget(map->tic, tx, ty));
            else
                fillMap(map, tx, ty, tic_api_mget(map->tic, tx, ty));
            ram2map(tic->ram, map->src);
        }

        history_add(map->history);
    }
}

static void drawSelectionVBank1(Map* map)
{
    tic_rect* sel = &map->select.rect;

    if(sel->w > 0 && sel->h > 0)
    {
        s32 x = sel->x * TIC_SPRITESIZE - map->scroll.x;
        s32 y = sel->y * TIC_SPRITESIZE - map->scroll.y;
        s32 w = sel->w * TIC_SPRITESIZE;
        s32 h = sel->h * TIC_SPRITESIZE;

        while(x+w<0)x+=MAX_SCROLL_X;
        while(y+h<0)y+=MAX_SCROLL_Y;
        while(x+w>=MAX_SCROLL_X)x-=MAX_SCROLL_X;
        while(y+h>=MAX_SCROLL_Y)y-=MAX_SCROLL_Y;

        drawSelectionRect(map, x-1, y-1, w+2, h+2);
    }
}

static void drawGrid(Map* map)
{
    tic_mem* tic = map->tic;

    s32 scrollX = map->scroll.x % TIC_SPRITESIZE;
    s32 scrollY = map->scroll.y % TIC_SPRITESIZE;

    for(s32 j = -scrollY; j <= TIC80_HEIGHT-scrollY; j += TIC_SPRITESIZE)
    {
        if(j >= 0 && j < TIC80_HEIGHT)
            for(s32 i = 0; i < TIC80_WIDTH; i++)
            {               
                u8 color = tic_api_pix(tic, i, j, 0, true);
                tic_api_pix(tic, i, j, (color+1)%TIC_PALETTE_SIZE, false);
            }
    }

    for(s32 j = -scrollX; j <= TIC80_WIDTH-scrollX; j += TIC_SPRITESIZE)
    {
        if(j >= 0 && j < TIC80_WIDTH)
            for(s32 i = 0; i < TIC80_HEIGHT; i++)
            {
                if((i+scrollY) % TIC_SPRITESIZE)
                {
                    u8 color = tic_api_pix(tic, j, i, 0, true);
                    tic_api_pix(tic, j, i, (color+1)%TIC_PALETTE_SIZE, false);
                }
            }
    }
}

static void drawMapReg(Map* map)
{
    tic_mem* tic = map->tic;
    tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

    bool handle = !sheetVisible(map) && checkMousePos(map->studio, &rect);
    bool space = tic_api_key(tic, tic_key_space);

    if(handle)
        processScrolling(map, 
            ((space || map->mode == MAP_DRAG_MODE) && checkMouseDown(map->studio, &rect, tic_mouse_left)) || 
                checkMouseDown(map->studio, &rect, tic_mouse_right));

    {
        s32 scrollX = map->scroll.x % TIC_SPRITESIZE;
        s32 scrollY = map->scroll.y % TIC_SPRITESIZE;

        map2ram(tic->ram, map->src);

        initBlitMode(map);
        tic_api_map(tic, map->scroll.x / TIC_SPRITESIZE, map->scroll.y / TIC_SPRITESIZE,
            TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, -scrollX, -scrollY, 0, 0, 1, NULL, NULL);
        resetBlitMode(map->tic);

        if (map->canvas.grid)
            drawGrid(map);
    }

    if(handle && !space)
    {
        static void(*const Handlers[])(Map*) = {processMouseDrawMode, processMouseDragMode, processMouseSelectMode, processMouseFillMode};
        Handlers[map->mode](map);
    }
}

static void undo(Map* map)
{
    history_undo(map->history);
}

static void redo(Map* map)
{
    history_redo(map->history);
}

static void copySelectionToClipboard(Map* map)
{
    tic_rect* sel = &map->select.rect;

    if(sel->w > 0 && sel->h > 0)
    {   
        s32 size = sel->w * sel->h + 2;
        u8* buffer = malloc(size);

        if(buffer)
        {
            buffer[0] = sel->w;
            buffer[1] = sel->h;

            u8* ptr = buffer + 2;

            for(s32 j = sel->y; j < sel->y+sel->h; j++)
                for(s32 i = sel->x; i < sel->x+sel->w; i++)
                {
                    s32 x = i, y = j;
                    normalizeMapRect(&x, &y);

                    s32 index = x + y * TIC_MAP_WIDTH;
                    *ptr++ = map->src->data[index];
                }       

            toClipboard(buffer, size, true);
            free(buffer);           
        }
    }
}

static void copyToClipboard(Map* map)
{
    copySelectionToClipboard(map);
    resetSelection(map);
}

static void deleteSelection(Map* map)
{
    tic_rect* sel = &map->select.rect;

    if(sel->w > 0 && sel->h > 0)
    {
        for(s32 j = sel->y; j < sel->y+sel->h; j++)
            for(s32 i = sel->x; i < sel->x+sel->w; i++)
            {
                s32 x = i, y = j;
                normalizeMapRect(&x, &y);

                s32 index = x + y * TIC_MAP_WIDTH;
                map->src->data[index] = 0;
            }

        history_add(map->history);
    }
}

static void cutToClipboard(Map* map)
{
    copySelectionToClipboard(map);
    deleteSelection(map);
    resetSelection(map);
}

static void copyFromClipboard(Map* map)
{
    if(tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        if(clipboard)
        {
            s32 size = (s32)strlen(clipboard)/2;

            if(size > 2)
            {
                u8* data = malloc(size);

                tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, true);

                if(data[0] * data[1] == size - 2)
                {
                    map->paste = data;
                    map->mode = MAP_SELECT_MODE;
                }
                else free(data);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void processKeyboard(Map* map)
{
    tic_mem* tic = map->tic;

    if(isIdle(map))
    {
        if(tic_api_key(tic, tic_key_shift))
        {
            if(!sheetVisible(map))
            {
                map->anim.movie = resetMovie(&map->anim.show);
                map->sheet.keep = false;
            }
        }
        else
        {
            if(!map->sheet.keep && sheetVisible(map))
                map->anim.movie = resetMovie(&map->anim.hide);
        }
    }

    if(tic->ram->input.keyboard.data == 0) return;
    
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    switch(getClipboardEvent(map->studio))
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(map); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(map); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(map); break;
    default: break;
    }
    
    if(tic_api_key(tic, tic_key_alt))
        return;

    if(ctrl)
    {
        if(keyWasPressed(map->studio, tic_key_z))        undo(map);
        else if(keyWasPressed(map->studio, tic_key_y))   redo(map);
    }
    else
    {
        if(keyWasPressed(map->studio, tic_key_tab)) setStudioMode(map->studio, TIC_WORLD_MODE);
        else if(keyWasPressed(map->studio, tic_key_1)) map->mode = MAP_DRAW_MODE;
        else if(keyWasPressed(map->studio, tic_key_2)) map->mode = MAP_DRAG_MODE;
        else if(keyWasPressed(map->studio, tic_key_3)) map->mode = MAP_SELECT_MODE;
        else if(keyWasPressed(map->studio, tic_key_4)) map->mode = MAP_FILL_MODE;
        else if(keyWasPressed(map->studio, tic_key_delete)) deleteSelection(map);
        else if(keyWasPressed(map->studio, tic_key_grave)) map->canvas.grid = !map->canvas.grid;
    }

    enum{Step = 1};

    if(tic_api_key(tic, tic_key_up)) map->scroll.y -= Step;
    if(tic_api_key(tic, tic_key_down)) map->scroll.y += Step;
    if(tic_api_key(tic, tic_key_left)) map->scroll.x -= Step;
    if(tic_api_key(tic, tic_key_right)) map->scroll.x += Step;

    static const tic_key Keycodes[] = {tic_key_up, tic_key_down, tic_key_left, tic_key_right};

    for(s32 i = 0; i < COUNT_OF(Keycodes); i++)
        if(tic_api_key(tic, Keycodes[i]))
        {
            normalizeMap(&map->scroll.x, &map->scroll.y);
            break;
        }
}

static void tick(Map* map)
{
    tic_mem* tic = map->tic;
    map->tickCounter++;

    processAnim(map->anim.movie, map);

    // process scroll
    if(tic->ram->input.mouse.scrolly < 0) 
    {
        setStudioMode(map->studio, TIC_WORLD_MODE);
        return;
    }

    processKeyboard(map);

    drawMapReg(map);
    drawSheetReg(map, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, TOOLBAR_SIZE);

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_dark_blue);

        memcpy(tic->ram->vram.palette.data, getConfig(map->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        tic_api_clip(tic, 0, TOOLBAR_SIZE, TIC80_WIDTH - (sheetVisible(map) ? TIC_SPRITESHEET_SIZE+2 : 0), TIC80_HEIGHT - TOOLBAR_SIZE);
        {
            s32 screenScrollX = map->scroll.x % TIC80_WIDTH;
            s32 screenScrollY = map->scroll.y % TIC80_HEIGHT;

            tic_api_line(tic, 0, TIC80_HEIGHT - screenScrollY, TIC80_WIDTH, TIC80_HEIGHT - screenScrollY, tic_color_grey);
            tic_api_line(tic, TIC80_WIDTH - screenScrollX, 0, TIC80_WIDTH - screenScrollX, TIC80_HEIGHT, tic_color_grey);
        }
        tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

        drawSheetVBank1(map, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, TOOLBAR_SIZE);

        {
            tic_rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};
            if(!sheetVisible(map) && checkMousePos(map->studio, &rect) && !tic_api_key(tic, tic_key_space))
            {            
                switch(map->mode)
                {
                case MAP_DRAW_MODE:
                case MAP_FILL_MODE:
                    drawTileCursorVBank1(map);
                    break;
                case MAP_SELECT_MODE:
                    if(map->paste)
                        drawPasteDataVBank1(map);
                    break;
                default:
                    break;
                }
            }
        }

        if(!sheetVisible(map))
            drawSelectionVBank1(map);

        drawMapToolbar(map, TIC80_WIDTH, 1);
        drawToolbar(map->studio, map->tic, false);
    }
}

static void onStudioEvent(Map* map, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT:   cutToClipboard(map); break;
    case TIC_TOOLBAR_COPY:  copyToClipboard(map); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(map); break;
    case TIC_TOOLBAR_UNDO:  undo(map); break;
    case TIC_TOOLBAR_REDO:  redo(map); break;
    default: break;
    }
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Map* map = data;
    if(row == 0)
        memcpy(&tic->ram->vram.palette, getBankPalette(map->studio, false), sizeof(tic_palette));
}

static void emptyDone(void* data) {}

static void setIdle(void* data)
{
    Map* map = data;
    map->anim.movie = resetMovie(&map->anim.idle);
}

static void freeAnim(Map* map)
{
    FREE(map->anim.show.items);
    FREE(map->anim.hide.items);
    FREE(map->anim.bank.items);
    FREE(map->anim.page.items);
}

void initMap(Map* map, Studio* studio, tic_map* src)
{
    enum {SheetStart = -(TIC_SPRITESHEET_SIZE + TOOLBAR_SIZE)};

    if(map->history) history_delete(map->history);
    freeAnim(map);

    *map = (Map)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .tick = tick,
        .src = src,
        .mode = MAP_DRAW_MODE,
        .canvas = 
        {
            .grid = true,
            .draw = false,
            .start = {0, 0},
        },
        .sheet = 
        {
            .rect = {0, 0, 1, 1},
            .start = {0, 0},
            .drag = false,
            .blit = {0},
        },
        .select = 
        {
            .rect = {0, 0, 0, 0},
            .start = {0, 0},
            .drag = false,
        },
        .paste = NULL,
        .tickCounter = 0,
        .scroll = 
        {
            .x = 0, 
            .y = 0, 
            .active = false,
            .gesture = false,
            .start = {0, 0},
        },
        .history = history_create(src, sizeof(tic_map)),
        .anim =
        {
            .pos.sheet = SheetStart,

            .idle = {.done = emptyDone,},

            .show = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
            {
                {SheetStart, 0, STUDIO_ANIM_TIME, &map->anim.pos.sheet, AnimEaseIn},
            }),

            .hide = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
            {
                {0, SheetStart, STUDIO_ANIM_TIME, &map->anim.pos.sheet, AnimEaseIn},
            }),

            .bank = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
            {
                {0, 0, STUDIO_ANIM_TIME, &map->anim.pos.bank, AnimEaseIn},
            }),

            .page = MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
            {
                {0, 0, STUDIO_ANIM_TIME, &map->anim.pos.page, AnimEaseIn},
            }),
        },
        .event = onStudioEvent,
        .scanline = scanline,
    };

    map->anim.movie = resetMovie(&map->anim.idle);

    normalizeMap(&map->scroll.x, &map->scroll.y);
    tic_blit_update_bpp(&map->sheet.blit, TIC_DEFAULT_BIT_DEPTH);
}

void freeMap(Map* map)
{
    freeAnim(map);
    history_delete(map->history);
    free(map);
}
