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

#include "menubar.h"

enum
{
    TitleX = 2,
    TitlePadding = 2,
    TitleGap = 2,
    TextY = 1,
    ItemPad = 3,
    PopupBorder = 1,
    PopupPadY = 2,
    SeparatorY = 3,
    IconWidth = 10,
    SubArrow = TIC_FONT_WIDTH,
    SubOverlap = 2,
    SubRise = 3,
};

static bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
    return (pt->x >= rect->x) && (pt->x < (rect->x + rect->w)) &&
           (pt->y >= rect->y) && (pt->y < (rect->y + rect->h));
}

// The menu owns a click it handles: a double-click must not reach the editor.
static void consumeClick(MouseState* state)
{
    state->click = false;
    state->dbl.click = false;
}

// The pointer state's click, consumed: the press and the release both inside.
static bool menuClick(MouseState* state, const tic_rect* rect)
{
    bool value = state->click && pointInRect(&state->start, rect) &&
                 pointInRect(&state->end, rect);

    if(value)
        consumeClick(state);

    return value;
}

static s32 textWidth(const char* text)
{
    return text ? (s32)strlen(text) * TIC_FONT_WIDTH : 0;
}

static bool hasIcons(const MenuBarCategory* category)
{
    for(s32 i = 0; i < category->count; i++)
        if(category->items[i].icon)
            return true;

    return false;
}

static bool hasSubmenus(const MenuBarCategory* category)
{
    for(s32 i = 0; i < category->count; i++)
        if(category->items[i].submenu)
            return true;

    return false;
}

static bool hasToggles(const MenuBarCategory* category)
{
    for(s32 i = 0; i < category->count; i++)
        if(category->items[i].checked >= 0)
            return true;

    return false;
}

static s32 categoryWidth(const MenuBarCategory* category)
{
    return textWidth(category->title) + TitlePadding * 2 +
           (category->icon ? IconWidth : 0);
}

static s32 rightWidth(const Menubar* menubar)
{
    s32 width = 0;
    s32 count = 0;

    for(s32 i = 0; i < menubar->count; i++)
        if(menubar->categories[i].right && !menubar->categories[i].hidden)
        {
            width += categoryWidth(&menubar->categories[i]);
            count++;
        }

    return count ? width + (count - 1) * TitleGap : 0;
}

static s32 rightStart(const Menubar* menubar)
{
    return TIC80_WIDTH - TitleX - rightWidth(menubar);
}

static bool titleRect(const Menubar* menubar, s32 index, tic_rect* rect)
{
    *rect = (tic_rect){0};

    s32 leftX = TitleX;
    s32 rightX = rightStart(menubar);

    for(s32 i = 0; i < menubar->count; i++)
    {
        const MenuBarCategory* category = &menubar->categories[i];

        if(category->hidden)
            continue;

        s32 width = categoryWidth(category);
        s32 x = category->right ? rightX : leftX;

        if(category->right)
            rightX += width + TitleGap;
        else
            leftX += width + TitleGap;

        if(i == index)
        {
            *rect = (tic_rect){x, 0, width, MENUBAR_HEIGHT};
            return true;
        }
    }

    return false;
}

static tic_rect popupSize(const MenuBarCategory* category)
{
    s32 labels = 0;
    s32 right = 0;

    for(s32 i = 0; i < category->count; i++)
    {
        const MenuBarItem* item = &category->items[i];

        if(item->separator)
            continue;

        labels = MAX(labels, textWidth(item->label));
        right = MAX(right, textWidth(item->accel));
    }

    if(hasSubmenus(category))
        right = MAX(right, SubArrow);

    s32 width = ItemPad * 2 + ((hasIcons(category) || hasToggles(category)) ? IconWidth : 0) +
                labels + (right ? ItemPad * 2 + right : 0);
    s32 height = category->count * MENUBAR_HEIGHT + PopupBorder * 2 + PopupPadY * 2;

    return (tic_rect){0, 0, width, height};
}

static tic_rect itemRect(const tic_rect* popup, s32 index)
{
    return (tic_rect){popup->x + PopupBorder,
                      popup->y + PopupBorder + PopupPadY + index * MENUBAR_HEIGHT,
                      popup->w - PopupBorder * 2,
                      MENUBAR_HEIGHT};
}

static tic_rect popupRect(const Menubar* menubar)
{
    const MenuBarCategory* category = &menubar->categories[menubar->open];
    tic_rect size = popupSize(category);

    tic_rect title;
    titleRect(menubar, menubar->open, &title);

    s32 x = title.x;

    if(x + size.w > TIC80_WIDTH)
        x = title.x + title.w - size.w;

    x = CLAMP(x, 0, MAX(0, TIC80_WIDTH - size.w));

    s32 y = MENUBAR_HEIGHT;

    if(y + size.h > TIC80_HEIGHT)
        y = MAX(0, TIC80_HEIGHT - size.h);

    return (tic_rect){x, y, size.w, size.h};
}

static tic_rect subPopupRect(const Menubar* menubar)
{
    tic_rect size = popupSize(menubar->sub);
    tic_rect parent = popupRect(menubar);
    tic_rect row = itemRect(&parent, menubar->subItem);

    // The submenu overlaps the parent's edge: no bare border column for the
    // pointer to cross, so the submenu cannot close on the way in.
    s32 right = parent.x + parent.w - SubOverlap;
    s32 left = parent.x - size.w + SubOverlap;
    s32 x = right;

    // Right is the default. Flip left only when that placement is on screen;
    // a submenu too wide for both stays on the right, clamped to the edge.
    if(right + size.w > TIC80_WIDTH)
    {
        if(left >= 0)
            x = left;
        else
            x = TIC80_WIDTH - size.w;
    }

    x = CLAMP(x, 0, MAX(0, TIC80_WIDTH - size.w));

    s32 y = CLAMP(row.y - SubRise, MENUBAR_HEIGHT, MAX(MENUBAR_HEIGHT, TIC80_HEIGHT - size.h));

    return (tic_rect){x, y, size.w, size.h};
}

void menubar_init(Menubar* menubar, MenuBarCategory* categories, s32 count,
                  MouseState* mouse, void* data, u8 checkIcon)
{
    *menubar = (Menubar){
        .categories = categories,
        .count = count,
        .mouse = mouse,
        .data = data,
        .mode = MENUBAR_NO_MODE,
        .requested = MENUBAR_NO_MODE,
        .checkIcon = checkIcon,
        .open = -1,
        .sub = NULL,
        .subItem = -1,
        .pending = NULL,
        .pendingId = 0,
    };
}

static void closeMenu(Menubar* menubar)
{
    menubar->open = -1;
    menubar->sub = NULL;
    menubar->subItem = -1;
}

void menubar_step(Menubar* menubar, tic_mem* tic, s32 mode)
{
    menubar->escaped = false;

    if(menubar->mode != mode)
    {
        menubar->mode = mode;
        closeMenu(menubar);
    }

    // No bar on screen: it neither opens nor takes the pointer.
    if(mode == MENUBAR_NO_MODE)
        return;

    if(menubar->open >= 0 && menubar->categories[menubar->open].hidden)
        closeMenu(menubar);

    MouseState* state = &menubar->mouse[tic_mouse_left];
    tic_point pos = tic_api_mouse(tic);

    if(menubar->open >= 0 && tic_api_keyp(tic, tic_key_escape, -1, -1))
    {
        if(menubar->sub)
        {
            menubar->sub = NULL;
            menubar->subItem = -1;
        }
        else
            closeMenu(menubar);

        consumeClick(state);
        menubar->escaped = true;
        return;
    }

    if(menubar->open < 0)
    {
        for(s32 i = 0; i < menubar->count; i++)
        {
            if(menubar->categories[i].hidden)
                continue;

            tic_rect rect;
            titleRect(menubar, i, &rect);

            if(menuClick(state, &rect))
            {
                menubar->open = i;
                menubar->sub = NULL;
                menubar->subItem = -1;
                return;
            }
        }

        return;
    }

    // An open menu follows the pointer to another title, as macOS does.
    for(s32 i = 0; i < menubar->count; i++)
    {
        if(menubar->categories[i].hidden)
            continue;

        tic_rect rect;
        titleRect(menubar, i, &rect);

        if(pos.y < MENUBAR_HEIGHT && pointInRect(&pos, &rect) && i != menubar->open)
        {
            menubar->open = i;
            menubar->sub = NULL;
            menubar->subItem = -1;

            if(state->click)
                consumeClick(state);

            return;
        }
    }

    tic_rect title;
    titleRect(menubar, menubar->open, &title);

    if(menuClick(state, &title))
    {
        closeMenu(menubar);
        return;
    }

    // A submenu follows the pointer: it opens, switches and closes with it.
    {
        tic_rect popup = popupRect(menubar);
        const MenuBarCategory* category = &menubar->categories[menubar->open];
        s32 row = -1;

        if(pointInRect(&pos, &popup))
        {
            for(s32 i = 0; i < category->count; i++)
            {
                if(category->items[i].separator)
                    continue;

                tic_rect rect = itemRect(&popup, i);

                if(pointInRect(&pos, &rect))
                {
                    row = i;
                    break;
                }
            }
        }

        if(menubar->sub)
        {
            tic_rect sub = subPopupRect(menubar);

            if(!pointInRect(&pos, &sub) && row != menubar->subItem)
            {
                if(row >= 0 && category->items[row].enabled && category->items[row].submenu)
                {
                    menubar->sub = category->items[row].submenu;
                    menubar->subItem = row;
                }
                else
                {
                    menubar->sub = NULL;
                    menubar->subItem = -1;
                }
            }
        }
        else if(row >= 0 && category->items[row].enabled && category->items[row].submenu)
        {
            menubar->sub = category->items[row].submenu;
            menubar->subItem = row;
        }
    }

    if(menubar->sub)
    {
        tic_rect popup = subPopupRect(menubar);
        const MenuBarCategory* category = menubar->sub;

        for(s32 i = 0; i < category->count; i++)
        {
            const MenuBarItem* item = &category->items[i];

            if(item->separator)
                continue;

            tic_rect rect = itemRect(&popup, i);

            if(menuClick(state, &rect))
            {
                if(item->enabled && item->handler)
                {
                    menubar->pending = item->handler;
                    menubar->pendingId = item->id;
                    closeMenu(menubar);
                }

                return;
            }
        }

        if(menuClick(state, &popup))
            return;

        // A click on the item that opened the submenu keeps it open; a click
        // on any other parent row puts it away.
        tic_rect parent = popupRect(menubar);
        tic_rect owner = itemRect(&parent, menubar->subItem);

        if(menuClick(state, &owner))
            return;

        if(menuClick(state, &parent))
        {
            menubar->sub = NULL;
            menubar->subItem = -1;
            return;
        }
    }
    else
    {
        tic_rect popup = popupRect(menubar);
        const MenuBarCategory* category = &menubar->categories[menubar->open];

        for(s32 i = 0; i < category->count; i++)
        {
            const MenuBarItem* item = &category->items[i];

            if(item->separator)
                continue;

            tic_rect rect = itemRect(&popup, i);

            if(menuClick(state, &rect))
            {
                if(!item->enabled)
                    return;

                if(item->submenu)
                {
                    menubar->sub = item->submenu;
                    menubar->subItem = i;
                }
                else if(item->handler)
                {
                    menubar->pending = item->handler;
                    menubar->pendingId = item->id;
                    closeMenu(menubar);
                }

                return;
            }
        }

        // A click on the popup's own padding is the popup's, not the screen's.
        if(menuClick(state, &popup))
            return;
    }

    // Anything left is outside: it closes the menu and never reaches the editor.
    if(state->click)
    {
        consumeClick(state);
        closeMenu(menubar);
    }
}

static void drawPopup(tic_mem* tic, const MenuBarCategory* category,
                      const tic_rect* popup, const tic_point* pos, const tic_tiles* icons,
                      u8 checkIcon, s32 activeRow)
{
    bool marks = category->selected >= 0;
    s32 iconX = (hasIcons(category) || hasToggles(category)) ? IconWidth : 0;

    tic_api_rect(tic, popup->x, popup->y, popup->w, popup->h, tic_color_white);

    for(s32 i = 0; i < category->count; i++)
    {
        const MenuBarItem* item = &category->items[i];
        tic_rect rect = itemRect(popup, i);

        if(item->separator)
        {
            tic_api_rect(tic, popup->x + ItemPad, rect.y + SeparatorY,
                         popup->w - ItemPad * 2, 1, tic_color_grey);
            continue;
        }

        bool over = activeRow >= 0 ? i == activeRow
                                   : item->enabled && pointInRect(pos, &rect);

        if(over)
            tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_light_grey);

        if(item->icon)
        {
            u8 color;

            if(marks)
                color = item->id == category->selected
                            ? (item->color ? item->color : tic_color_dark_grey)
                        : over ? tic_color_grey
                               : tic_color_light_grey;
            else if(item->checked >= 0)
                color = item->checked
                            ? (item->color ? item->color : tic_color_dark_grey)
                        : over ? tic_color_grey
                               : tic_color_light_grey;
            else if(item->color)
                color = item->color;
            else
                color = item->enabled ? tic_color_dark_grey : tic_color_grey;

            ui_icon(tic, icons, item->icon, rect.x + ItemPad, rect.y, color);
        }
        else if(item->checked >= 0 && checkIcon)
        {
            ui_icon(tic, icons, checkIcon, rect.x + ItemPad, rect.y,
                    item->checked ? tic_color_dark_grey
                    : over        ? tic_color_grey
                                  : tic_color_light_grey);
        }

        tic_api_print(tic, item->label, rect.x + ItemPad + iconX, rect.y + TextY,
                      item->enabled ? tic_color_dark_grey : tic_color_grey,
                      true, 1, false);

        if(item->submenu)
            tic_api_print(tic, ">", rect.x + rect.w - ItemPad - SubArrow,
                          rect.y + TextY, over ? tic_color_dark_grey : tic_color_grey,
                          true, 1, false);
        else if(item->accel)
            tic_api_print(tic, item->accel,
                          rect.x + rect.w - ItemPad - textWidth(item->accel),
                          rect.y + TextY,
                          !item->enabled ? tic_color_light_grey
                          : over         ? tic_color_dark_grey
                                         : tic_color_grey,
                          true, 1, false);
    }

    // Last, so an icon's eighth row cannot dent the outline.
    tic_api_rectb(tic, popup->x, popup->y, popup->w, popup->h, tic_color_dark_grey);
}

void menubar_draw(Menubar* menubar, tic_mem* tic, s32 mode, const tic_tiles* icons)
{
    if(mode == MENUBAR_NO_MODE)
        return;

    if(menubar->mode != mode)
    {
        menubar->mode = mode;
        closeMenu(menubar);
    }

    if(menubar->open >= 0 && menubar->categories[menubar->open].hidden)
        closeMenu(menubar);

    tic_api_rect(tic, 0, 0, TIC80_WIDTH, MENUBAR_HEIGHT, tic_color_white);

    tic_point pos = tic_api_mouse(tic);

    for(s32 i = 0; i < menubar->count; i++)
    {
        const MenuBarCategory* category = &menubar->categories[i];

        if(category->hidden)
            continue;

        tic_rect rect;
        titleRect(menubar, i, &rect);

        bool open = i == menubar->open;
        bool over = pointInRect(&pos, &rect);

        if(open)
            tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_dark_grey);
        else if(over)
            tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_light_grey);

        s32 textX = rect.x + TitlePadding;

        if(category->icon)
        {
            ui_icon(tic, icons, category->icon, rect.x + TitlePadding, rect.y,
                    category->iconColor ? category->iconColor
                    : open              ? tic_color_white
                                        : tic_color_dark_grey);
            textX += IconWidth;
        }

        tic_api_print(tic, category->title, textX, TextY,
                      open ? tic_color_white : tic_color_dark_grey, true, 1, false);
    }

    if(menubar->open >= 0)
    {
        tic_rect popup = popupRect(menubar);
        const MenuBarCategory* category = &menubar->categories[menubar->open];

        drawPopup(tic, category, &popup, &pos, icons, menubar->checkIcon,
                  menubar->sub ? menubar->subItem : -1);

        if(menubar->sub)
        {
            tic_rect sub = subPopupRect(menubar);
            drawPopup(tic, menubar->sub, &sub, &pos, icons, menubar->checkIcon, -1);
        }
    }

    // The menu owns the pointer: over the bar, or anywhere while it is open.
    if(menubar->open >= 0 || pos.y < MENUBAR_HEIGHT)
        ui_cursor(tic, tic_cursor_arrow);

    if(menubar->pending)
    {
        MenuBarHandler handler = menubar->pending;
        s32 id = menubar->pendingId;

        menubar->pending = NULL;
        handler(menubar->data, id);
    }
}
