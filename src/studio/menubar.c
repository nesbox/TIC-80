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
#include "keyboard.h"

#include <stdio.h>

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
    AutoHideSpeed = 2,
    PageRows = 5,
    CompactCell = 7,
    GroupGap = 4,
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

// The accelerators use the small font, so their right edge needs its width.
static s32 accelWidth(const char* text)
{
    return text ? (s32)strlen(text) * TIC_ALTFONT_WIDTH : 0;
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
    s32 text = textWidth(category->title);

    // An icon-only title is a compact cell, as a pin is.
    if(category->icon && !text)
        return CompactCell;

    return text + TitlePadding * 2 + (category->icon ? IconWidth : 0);
}

// The home stop becomes its icon in compact mode; its title stays for the hint.
static bool isCompactHome(const Menubar* menubar, const MenuBarCategory* category)
{
    return menubar->compact && menubar->home >= 0 && category->icon &&
           category == &menubar->categories[menubar->home];
}

// The overflow menu becomes the three-dot icon; its title stays for the hint.
static bool isCompactMore(const Menubar* menubar, const MenuBarCategory* category)
{
    return menubar->compact && menubar->more == category && category->icon;
}

static s32 stopWidth(const Menubar* menubar, s32 index)
{
    const MenuBarCategory* category = &menubar->categories[index];

    return isCompactHome(menubar, category) || isCompactMore(menubar, category)
               ? CompactCell
               : categoryWidth(category);
}

static s32 rightWidth(const Menubar* menubar)
{
    s32 width = 0;
    s32 count = 0;

    for(s32 i = 0; i < menubar->count; i++)
        if(menubar->categories[i].right && !menubar->categories[i].hidden)
        {
            width += stopWidth(menubar, i);
            count++;
        }

    return count ? width + (count - 1) * TitleGap : 0;
}

static s32 rightStart(const Menubar* menubar)
{
    return TIC80_WIDTH - TitleX - rightWidth(menubar);
}

// The host's tabs category trades its title for icon cells in compact mode.
static bool isTabs(const Menubar* menubar, const MenuBarCategory* category)
{
    return menubar->compact && menubar->tabs == category;
}

static bool focusableCategory(const Menubar* menubar, s32 index)
{
    return !menubar->categories[index].hidden &&
           !isTabs(menubar, &menubar->categories[index]);
}

// Tabs are the icon run before the first separator; what follows is the rest
// of the apps menu, which the compact bar leaves out.
static s32 tabsCount(const Menubar* menubar)
{
    s32 count = 0;

    if(menubar->compact && menubar->tabs)
        for(s32 i = 0; i < menubar->tabs->count && !menubar->tabs->items[i].separator; i++)
            if(menubar->tabs->items[i].icon)
                count++;

    return count;
}

static s32 tabItem(const Menubar* menubar, s32 tab)
{
    for(s32 i = 0; i < menubar->tabs->count && !menubar->tabs->items[i].separator; i++)
        if(menubar->tabs->items[i].icon && tab-- == 0)
            return i;

    return -1;
}

static tic_rect tabRect(const Menubar* menubar, s32 tab)
{
    return (tic_rect){TitleX + tab * CompactCell, menubar->y, CompactCell, MENUBAR_HEIGHT};
}

// The left titles' width on one side of the compact pins: the overflow menu
// follows them, the rest lead.
static s32 titleBlockWidth(const Menubar* menubar, bool afterPins)
{
    s32 width = 0;
    s32 count = 0;

    for(s32 i = 0; i < menubar->count; i++)
    {
        const MenuBarCategory* category = &menubar->categories[i];

        if(category->hidden || category->right || isTabs(menubar, category))
            continue;

        if(isCompactMore(menubar, category) != afterPins)
            continue;

        width += stopWidth(menubar, i) + (count ? TitleGap : 0);
        count++;
    }

    return width;
}

// The compact row: tabs, the hint, the titles that lead, the EDIT pins, the
// mode pins, then the overflow menu and the right stops. Pins that do not fit
// are left out of the second group first.
typedef struct
{
    s32 tabs;
    s32 tabsX;     // the first tab's x, left-packed
    s32 hintX;     // the hint's left edge, right after the tabs
    s32 hintWidth; // the room the controls leave for it
    s32 beforeX;   // the first title before the pins
    s32 editX;
    s32 editCount;
    s32 modeX;
    s32 modeCount;
    s32 titlesX; // the first title after the pins (the overflow menu)
} Compact;

static Compact compactLayout(const Menubar* menubar)
{
    Compact compact = {.titlesX = TitleX};

    if(!menubar->compact)
        return compact;

    compact.tabs = tabsCount(menubar);
    compact.tabsX = TitleX;
    compact.hintX = TitleX + compact.tabs * CompactCell;

    if(compact.tabs)
        compact.hintX += GroupGap;

    s32 beforeWidth = titleBlockWidth(menubar, false);
    s32 afterWidth = titleBlockWidth(menubar, true);
    s32 rightLimit = rightStart(menubar) - GroupGap;
    s32 pinsStart = compact.hintX + beforeWidth + (beforeWidth ? GroupGap : 0);
    s32 pinsLimit = rightLimit - afterWidth - (afterWidth ? GroupGap : 0);

    s32 split = MIN(menubar->buttonSplit, menubar->buttonCount);
    s32 modeCount = menubar->buttonCount - split;
    s32 fit = (pinsLimit - pinsStart) / CompactCell;

    if(fit > split && modeCount)
        fit = (pinsLimit - pinsStart - GroupGap) / CompactCell;

    fit = CLAMP(fit, 0, menubar->buttonCount);

    compact.editCount = MIN(fit, split);
    compact.modeCount = MIN(fit - compact.editCount, modeCount);

    s32 pinsWidth = compact.editCount * CompactCell + compact.modeCount * CompactCell;

    if(compact.editCount && compact.modeCount)
        pinsWidth += GroupGap;

    // The controls are packed after the hint; whatever they leave between the
    // tabs and the right stops is the hint's room.
    s32 controlsEnd = pinsWidth ? pinsStart + pinsWidth : compact.hintX + beforeWidth;

    if(afterWidth)
        controlsEnd += ((pinsWidth || beforeWidth) ? GroupGap : 0) + afterWidth;

    s32 slack = rightLimit - controlsEnd;
    compact.hintWidth = MAX(0, slack - GroupGap);
    s32 shift = compact.hintWidth + (compact.hintWidth ? GroupGap : 0);

    compact.beforeX = compact.hintX + shift;
    compact.editX = compact.beforeX + beforeWidth + (beforeWidth && pinsWidth ? GroupGap : 0);
    compact.modeX = compact.editX + compact.editCount * CompactCell;

    if(compact.editCount && compact.modeCount)
        compact.modeX += GroupGap;

    compact.titlesX = pinsWidth ? compact.editX + pinsWidth : compact.beforeX + beforeWidth;

    if(afterWidth && (pinsWidth || beforeWidth))
        compact.titlesX += GroupGap;

    return compact;
}

static bool titleRect(const Menubar* menubar, s32 index, tic_rect* rect)
{
    *rect = (tic_rect){0};

    Compact compact = compactLayout(menubar);
    s32 leftX = menubar->compact ? compact.beforeX : TitleX;
    s32 afterX = compact.titlesX;
    s32 rightX = rightStart(menubar);

    for(s32 i = 0; i < menubar->count; i++)
    {
        const MenuBarCategory* category = &menubar->categories[i];

        if(category->hidden || isTabs(menubar, category))
            continue;

        s32 width = stopWidth(menubar, i);
        s32 x;

        if(category->right)
        {
            x = rightX;
            rightX += width + TitleGap;
        }
        else if(menubar->compact && isCompactMore(menubar, category))
        {
            x = afterX;
            afterX += width + TitleGap;
        }
        else
        {
            x = leftX;
            leftX += width + TitleGap;
        }

        if(i == index)
        {
            *rect = (tic_rect){x, menubar->y, width, MENUBAR_HEIGHT};
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
        right = MAX(right, accelWidth(item->accel));
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

    s32 barBottom = menubar->y + MENUBAR_HEIGHT;
    s32 y = barBottom;

    if(y + size.h > TIC80_HEIGHT)
        y = MAX(barBottom, TIC80_HEIGHT - size.h);

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

    s32 barBottom = menubar->y + MENUBAR_HEIGHT;
    s32 y = CLAMP(row.y - SubRise, barBottom, MAX(barBottom, TIC80_HEIGHT - size.h));

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
        .tabIcon = 0,
        .autohide = false,
        .compact = false,
        .tabs = NULL,
        .more = NULL,
        .caption = NULL,
        .tooltip = NULL,
        .buttons = NULL,
        .buttonCount = 0,
        .buttonSplit = 0,
        .y = 0,
        .open = -1,
        .sub = NULL,
        .subItem = -1,
        .pending = NULL,
        .pendingId = 0,
        .keyboard = false,
        .focus = -1,
        .home = -1,
        .select = -1,
        .subSelect = -1,
        .captured = false,
        .lastMouse = {-1, -1},
    };
}

static void closePopup(Menubar* menubar)
{
    menubar->open = -1;
    menubar->sub = NULL;
    menubar->subItem = -1;
    menubar->select = -1;
    menubar->subSelect = -1;
}

// Closing the menu ends keyboard control too; ESC's popup level uses
// closePopup() to keep the bar focused.
static void closeMenu(Menubar* menubar)
{
    closePopup(menubar);
    menubar->keyboard = false;
    menubar->focus = -1;
}

static void updateCaptured(Menubar* menubar)
{
    menubar->captured = menubar->keyboard || menubar->open >= 0;
}

static bool selectable(const MenuBarItem* item)
{
    return item->enabled && !item->separator;
}

// The next selectable row in `dir`, wrapping. A `from` of -1 starts outside
// the list: the first row on the way down, the last one on the way up.
static s32 nextSelectable(const MenuBarCategory* category, s32 from, s32 dir)
{
    if(category->count <= 0)
        return -1;

    s32 row = from;

    for(s32 n = 0; n < category->count; n++)
    {
        row = row < 0 ? (dir > 0 ? 0 : category->count - 1)
                      : (row + dir + category->count) % category->count;

        if(selectable(&category->items[row]))
            return row;
    }

    return -1;
}

// A page move: up to PageRows selectable rows in `dir`, stopping at the end
// instead of wrapping around it.
static s32 pageSelectable(const MenuBarCategory* category, s32 from, s32 dir)
{
    if(from < 0)
        return nextSelectable(category, -1, dir);

    s32 row = from;

    for(s32 n = 0; n < PageRows; n++)
    {
        s32 next = nextSelectable(category, row, dir);

        if(next < 0 || next == row || (dir > 0 ? next < row : next > row))
            break;

        row = next;
    }

    return row;
}

// The row a menu opens on: the selection group's mark, else the first or the
// last selectable row for the opening direction.
static s32 initialRow(const MenuBarCategory* category, s32 dir)
{
    if(category->selected >= 0)
        for(s32 i = 0; i < category->count; i++)
            if(category->items[i].id == category->selected && selectable(&category->items[i]))
                return i;

    return nextSelectable(category, -1, dir);
}

// A title's place in visual order: the left-packed titles in index order,
// then the right-packed ones.
static s32 titleAt(const Menubar* menubar, s32 ordinal)
{
    for(s32 pass = 0; pass < 2; pass++)
        for(s32 i = 0; i < menubar->count; i++)
        {
            const MenuBarCategory* category = &menubar->categories[i];

            if(!focusableCategory(menubar, i) || (!category->right) != (pass == 0))
                continue;

            if(ordinal-- == 0)
                return i;
        }

    return -1;
}

// Focus moves between visible titles and wraps: Right on TIC-80 lands on the
// app switcher, Left on the first title lands on TIC-80.
static s32 adjacentTitle(const Menubar* menubar, s32 from, s32 dir)
{
    s32 visible = 0;
    s32 ordinal = -1;

    // The same two-pass order titleAt() uses.
    for(s32 pass = 0; pass < 2; pass++)
        for(s32 i = 0; i < menubar->count; i++)
        {
            const MenuBarCategory* category = &menubar->categories[i];

            if(!focusableCategory(menubar, i) || (!category->right) != (pass == 0))
                continue;

            if(i == from)
                ordinal = visible;

            visible++;
        }

    if(visible <= 0)
        return -1;

    if(ordinal < 0)
        return titleAt(menubar, dir > 0 ? 0 : visible - 1);

    return titleAt(menubar, (ordinal + dir + visible) % visible);
}

static s32 visibleTitles(const Menubar* menubar)
{
    s32 count = 0;

    for(s32 i = 0; i < menubar->count; i++)
        if(focusableCategory(menubar, i))
            count++;

    return count;
}

// The selectable row under the pointer, or -1; the keyboard starts here when
// it takes over a mouse-opened popup.
static s32 pointedRow(const Menubar* menubar, tic_point pos)
{
    tic_rect popup = popupRect(menubar);
    const MenuBarCategory* category = &menubar->categories[menubar->open];

    for(s32 i = 0; i < category->count; i++)
    {
        tic_rect rect = itemRect(&popup, i);

        if(selectable(&category->items[i]) && pointInRect(&pos, &rect))
            return i;
    }

    return -1;
}

static s32 pointedSubRow(const Menubar* menubar, tic_point pos)
{
    tic_rect popup = subPopupRect(menubar);

    for(s32 i = 0; i < menubar->sub->count; i++)
    {
        tic_rect rect = itemRect(&popup, i);

        if(selectable(&menubar->sub->items[i]) && pointInRect(&pos, &rect))
            return i;
    }

    return -1;
}

static void openCategory(Menubar* menubar, s32 index, s32 dir)
{
    if(index < 0)
        return;

    const MenuBarCategory* category = &menubar->categories[index];
    s32 row = initialRow(category, dir);

    // A category with nothing to run does not open.
    if(row < 0)
        return;

    menubar->open = index;
    menubar->focus = index;
    menubar->select = row;
    menubar->sub = NULL;
    menubar->subItem = -1;
    menubar->subSelect = -1;
}

static void openSubmenu(Menubar* menubar, s32 row)
{
    const MenuBarItem* item = &menubar->categories[menubar->open].items[row];
    s32 first = item->submenu ? nextSelectable(item->submenu, -1, +1) : -1;

    if(first < 0)
        return;

    menubar->sub = item->submenu;
    menubar->subItem = row;
    menubar->subSelect = first;
}

// Running a row behaves exactly like clicking it: a submenu opens, a toggle or
// a selection-group item stays open, anything else closes the menu.
static void runRow(Menubar* menubar, const MenuBarCategory* category, s32 row)
{
    if(row < 0)
        return;

    const MenuBarItem* item = &category->items[row];

    if(!selectable(item))
        return;

    if(item->submenu && category == &menubar->categories[menubar->open])
    {
        openSubmenu(menubar, row);
        return;
    }

    if(item->handler)
    {
        menubar->pending = item->handler;
        menubar->pendingId = item->id;

        if(item->checked < 0 && category->selected < 0)
            closeMenu(menubar);
    }
}

// The menu takes the pointer: while a popup is open anywhere, and over the
// bar itself when one is drawn.
bool menubar_owns_pointer(const Menubar* menubar, tic_mem* tic)
{
    if(menubar->mode == MENUBAR_NO_MODE)
        return false;

    if(menubar->open >= 0)
        return true;

    return tic_api_mouse(tic).y < menubar->y + MENUBAR_HEIGHT;
}

static bool pressed(tic_mem* tic, tic_key key)
{
    return tic_api_keyp(tic, key, -1, -1);
}

static bool repeated(tic_mem* tic, tic_key key)
{
    return tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

static bool enterPressed(tic_mem* tic)
{
    return pressed(tic, tic_key_return) || pressed(tic, tic_key_numpadenter);
}

// A navigation key pressed while the pointer owns an open popup hands it to
// the keyboard.
static bool navKey(tic_mem* tic)
{
    return pressed(tic, tic_key_up) || pressed(tic, tic_key_down) ||
           pressed(tic, tic_key_left) || pressed(tic, tic_key_right) ||
           pressed(tic, tic_key_home) || pressed(tic, tic_key_end) ||
           pressed(tic, tic_key_pageup) || pressed(tic, tic_key_pagedown) ||
           enterPressed(tic) || pressed(tic, tic_key_space);
}

// F10 is the bar's, Ctrl+F10 is the bytebattle toggle.
static bool barKey(tic_mem* tic)
{
    return pressed(tic, tic_key_f10) &&
           !tic_api_key(tic, tic_key_ctrl) && !tic_api_key(tic, tic_key_alt);
}

// The keyboard's frame: F10 and ESC, then the keys of the level focus is on.
// Returns true when the bar owns the frame, so the pointer stays out of it.
static bool keyboardStep(Menubar* menubar, tic_mem* tic, MouseState* state, tic_point pos)
{
    if(barKey(tic))
    {
        // F10 toggles the menu: it closes anything open, else opens the host's
        // home category, the TIC-80 menu.
        if(menubar->open >= 0)
            closeMenu(menubar);
        else
        {
            menubar->keyboard = true;

            // An autohidden bar is shown at once, so the popup opens where the
            // pointer would have opened it.
            menubar->y = 0;

            if(menubar->home >= 0 && !menubar->categories[menubar->home].hidden)
            {
                menubar->focus = menubar->home;
                openCategory(menubar, menubar->home, +1);
            }
            else
                menubar->focus = titleAt(menubar, 0);
        }

        return true;
    }

    if(pressed(tic, tic_key_escape) && (menubar->open >= 0 || menubar->keyboard))
    {
        if(menubar->sub)
        {
            // Back to the parent row; the popup and focus stay.
            menubar->select = menubar->subItem;
            menubar->sub = NULL;
            menubar->subItem = -1;
            menubar->subSelect = -1;
        }
        else if(menubar->open >= 0)
        {
            bool focused = menubar->keyboard;

            closePopup(menubar);

            if(!focused)
                closeMenu(menubar);
        }
        else
            closeMenu(menubar);

        consumeClick(state);
        menubar->escaped = true;
        return true;
    }

    // The first navigation key on a mouse-opened popup takes it over from the
    // row the pointer is on, or from outside the list.
    if(!menubar->keyboard && menubar->open >= 0 && navKey(tic))
    {
        menubar->keyboard = true;
        menubar->focus = menubar->open;
        menubar->select = pointedRow(menubar, pos);

        if(menubar->sub)
            menubar->subSelect = pointedSubRow(menubar, pos);
    }

    if(!menubar->keyboard)
        return false;

    // The bar: titles, and the key that opens the focused menu.
    if(menubar->open < 0)
    {
        if(menubar->focus < 0)
            menubar->focus = titleAt(menubar, 0);

        if(repeated(tic, tic_key_left))
            menubar->focus = adjacentTitle(menubar, menubar->focus, -1);
        else if(repeated(tic, tic_key_right))
            menubar->focus = adjacentTitle(menubar, menubar->focus, +1);
        else if(repeated(tic, tic_key_home) || repeated(tic, tic_key_pageup))
            menubar->focus = titleAt(menubar, 0);
        else if(repeated(tic, tic_key_end) || repeated(tic, tic_key_pagedown))
            menubar->focus = titleAt(menubar, visibleTitles(menubar) - 1);
        else if(repeated(tic, tic_key_down) || enterPressed(tic))
            openCategory(menubar, menubar->focus, +1);
        else if(repeated(tic, tic_key_up))
            openCategory(menubar, menubar->focus, -1);

        return true;
    }

    // The submenu: rows, Left back to the parent.
    if(menubar->sub)
    {
        const MenuBarCategory* category = menubar->sub;

        if(repeated(tic, tic_key_up))
            menubar->subSelect = nextSelectable(category, menubar->subSelect, -1);
        else if(repeated(tic, tic_key_down))
            menubar->subSelect = nextSelectable(category, menubar->subSelect, +1);
        else if(repeated(tic, tic_key_home))
            menubar->subSelect = nextSelectable(category, -1, +1);
        else if(repeated(tic, tic_key_end))
            menubar->subSelect = nextSelectable(category, -1, -1);
        else if(repeated(tic, tic_key_pageup))
            menubar->subSelect = pageSelectable(category, menubar->subSelect, -1);
        else if(repeated(tic, tic_key_pagedown))
            menubar->subSelect = pageSelectable(category, menubar->subSelect, +1);
        else if(enterPressed(tic) || pressed(tic, tic_key_space))
        {
            if(menubar->subSelect < 0)
                menubar->subSelect = initialRow(category, +1);

            runRow(menubar, category, menubar->subSelect);
        }
        else if(repeated(tic, tic_key_left))
        {
            menubar->select = menubar->subItem;
            menubar->sub = NULL;
            menubar->subItem = -1;
            menubar->subSelect = -1;
        }

        return true;
    }

    // The popup: rows, adjacent menus, and a submenu on Right or Enter.
    {
        const MenuBarCategory* category = &menubar->categories[menubar->open];

        if(repeated(tic, tic_key_up))
            menubar->select = nextSelectable(category, menubar->select, -1);
        else if(repeated(tic, tic_key_down))
            menubar->select = nextSelectable(category, menubar->select, +1);
        else if(repeated(tic, tic_key_home))
            menubar->select = nextSelectable(category, -1, +1);
        else if(repeated(tic, tic_key_end))
            menubar->select = nextSelectable(category, -1, -1);
        else if(repeated(tic, tic_key_pageup))
            menubar->select = pageSelectable(category, menubar->select, -1);
        else if(repeated(tic, tic_key_pagedown))
            menubar->select = pageSelectable(category, menubar->select, +1);
        else if(repeated(tic, tic_key_left))
        {
            s32 index = adjacentTitle(menubar, menubar->open, -1);

            if(index != menubar->open)
                openCategory(menubar, index, +1);
        }
        else if(repeated(tic, tic_key_right))
        {
            if(menubar->select >= 0 && category->items[menubar->select].submenu)
                openSubmenu(menubar, menubar->select);
            else
            {
                s32 index = adjacentTitle(menubar, menubar->open, +1);

                if(index != menubar->open)
                    openCategory(menubar, index, +1);
            }
        }
        else if(enterPressed(tic) || pressed(tic, tic_key_space))
        {
            if(menubar->select < 0)
                menubar->select = initialRow(category, +1);

            runRow(menubar, category, menubar->select);
        }
    }

    return true;
}

static void runCompactItem(Menubar* menubar, const MenuBarItem* item)
{
    if(item->enabled && item->handler)
    {
        menubar->pending = item->handler;
        menubar->pendingId = item->id;
    }

    closeMenu(menubar);
}

// A compact tab or pin under the click takes it; true when one did.
static bool compactClick(Menubar* menubar, MouseState* state)
{
    if(!menubar->compact)
        return false;

    if(menubar->tabs)
        for(s32 tab = 0; tab < tabsCount(menubar); tab++)
        {
            tic_rect rect = tabRect(menubar, tab);

            if(menuClick(state, &rect))
            {
                s32 item = tabItem(menubar, tab);

                if(item >= 0)
                    runCompactItem(menubar, &menubar->tabs->items[item]);

                return true;
            }
        }

    Compact layout = compactLayout(menubar);

    for(s32 i = 0; i < layout.editCount; i++)
    {
        tic_rect rect = {layout.editX + i * CompactCell, menubar->y,
                         CompactCell, MENUBAR_HEIGHT};

        if(menuClick(state, &rect))
        {
            runCompactItem(menubar, menubar->buttons[i]);
            return true;
        }
    }

    for(s32 i = 0; i < layout.modeCount; i++)
    {
        tic_rect rect = {layout.modeX + i * CompactCell, menubar->y,
                         CompactCell, MENUBAR_HEIGHT};

        if(menuClick(state, &rect))
        {
            runCompactItem(menubar, menubar->buttons[layout.editCount + i]);
            return true;
        }
    }

    return false;
}

static void pointerStep(Menubar*, MouseState*, tic_point);

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
    {
        menubar->captured = false;
        return;
    }

    if(menubar->open >= 0 && menubar->categories[menubar->open].hidden)
        closeMenu(menubar);

    MouseState* state = &menubar->mouse[tic_mouse_left];
    tic_point pos = tic_api_mouse(tic);

    bool moved = pos.x != menubar->lastMouse.x || pos.y != menubar->lastMouse.y;
    menubar->lastMouse = pos;

    // The pointer takes the menu back: a move or a press drops keyboard focus
    // and hover behaves as before.
    if(menubar->keyboard && (moved || state->down || state->click))
    {
        menubar->keyboard = false;
        menubar->focus = -1;
        menubar->select = -1;
        menubar->subSelect = -1;
    }

    // Autohide follows the pointer: the bar is shown while a menu is open, the
    // bar has focus, or the pointer is in the top strip, and slides away
    // otherwise.
    s32 target = menubar->autohide && menubar->open < 0 && !menubar->keyboard &&
                         pos.y >= MENUBAR_HEIGHT
                     ? -MENUBAR_HEIGHT
                     : 0;

    if(menubar->y < target)
        menubar->y = MIN(menubar->y + AutoHideSpeed, target);
    else if(menubar->y > target)
        menubar->y = MAX(menubar->y - AutoHideSpeed, target);

    if(keyboardStep(menubar, tic, state, pos))
    {
        // The bar answered this frame's keys, even when the one that closed
        // the menu left it unfocused: the editor must not see that key.
        menubar->captured = true;
        return;
    }

    pointerStep(menubar, state, pos);
    updateCaptured(menubar);
}

static void pointerStep(Menubar* menubar, MouseState* state, tic_point pos)
{
    if(compactClick(menubar, state))
        return;

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

        if(pos.y < menubar->y + MENUBAR_HEIGHT && pointInRect(&pos, &rect) && i != menubar->open)
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

                    // A toggle, or an item in a selection group, stays open
                    // so several can be chosen in one visit.
                    if(item->checked < 0 && category->selected < 0)
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

                    if(item->checked < 0 && category->selected < 0)
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

// A checked circle carries a power LED: a 3x3 green body with a lit corner.
static void drawCheckLed(tic_mem* tic, s32 x, s32 y)
{
    tic_api_rect(tic, x + 2, y + 2, 3, 3, tic_color_green);
    tic_api_pix(tic, x + 4, y + 2, tic_color_white, false);
}

// The compact bar's tab cells and pinned buttons; the hovered item's label is
// left for the caller to print in the free space.
static void drawCompact(Menubar* menubar, tic_mem* tic, const tic_tiles* icons,
                        tic_point pos, const MenuBarItem** hovered)
{
    *hovered = NULL;

    if(!menubar->compact)
        return;

    for(s32 tab = 0; tab < tabsCount(menubar); tab++)
    {
        s32 index = tabItem(menubar, tab);

        if(index < 0)
            continue;

        const MenuBarItem* item = &menubar->tabs->items[index];
        tic_rect rect = tabRect(menubar, tab);
        bool active = item->id == menubar->tabs->selected;
        bool over = pointInRect(&pos, &rect);

        if(active)
        {
            // The old toolbar's tab: a grey tab glyph, the icon's black shadow
            // one row down, and the icon itself in white.
            if(menubar->tabIcon)
                ui_icon(tic, icons, menubar->tabIcon, rect.x, rect.y, tic_color_grey);

            ui_icon(tic, icons, item->icon, rect.x, rect.y + 1, tic_color_black);
        }

        ui_icon(tic, icons, item->icon, rect.x, rect.y,
                active ? tic_color_white
                : over ? tic_color_grey
                       : tic_color_light_grey);

        if(over)
            *hovered = item;
    }

    Compact layout = compactLayout(menubar);
    const MouseState* mouse = &menubar->mouse[tic_mouse_left];

    for(s32 group = 0; group < 2; group++)
    {
        s32 x = group ? layout.modeX : layout.editX;
        s32 count = group ? layout.modeCount : layout.editCount;
        s32 start = group ? layout.editCount : 0;

        for(s32 i = 0; i < count; i++)
        {
            const MenuBarItem* item = menubar->buttons[start + i];
            tic_rect rect = {x + i * CompactCell, menubar->y, CompactCell, MENUBAR_HEIGHT};
            bool over = pointInRect(&pos, &rect);
            bool held = mouse->down && pointInRect(&mouse->start, &rect);
            u8 color = tic_color_light_grey;
            u8 bg = 0;

            if(!item->enabled)
                color = tic_color_grey;
            else if(held)
            {
                // A press fills the cell with the item's colour and flips the
                // glyph to white, as the old toolbar did.
                bg = item->color ? item->color : tic_color_dark_grey;
                color = tic_color_white;
            }
            else if(item->checked > 0)
                color = item->color ? item->color : tic_color_dark_grey;
            else if(over)
                color = item->color ? item->color : tic_color_grey;

            if(bg)
                tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, bg);

            ui_icon(tic, icons, item->icon, rect.x, rect.y, color);

            if(over)
                *hovered = item;
        }
    }
}

// The hint follows the titles: the hovered control's label and accelerator, the
// editor's own hover text, or the current app's full name, in the wide font.
static void drawHint(Menubar* menubar, tic_mem* tic,
                     const MenuBarItem* hovered, s32 hoveredTitle)
{
    char text[64];
    const char* hint = NULL;
    bool tooltip = false;
    s32 hintX = 0;
    s32 room = 0;

    if(menubar->compact)
    {
        Compact layout = compactLayout(menubar);

        hint = menubar->caption;
        hintX = layout.hintX;
        room = layout.hintWidth / TIC_FONT_WIDTH;

        if(hovered)
        {
            if(hovered->accel && hovered->accel[0])
                snprintf(text, sizeof text, "%s [%s]", hovered->label, hovered->accel);
            else
                snprintf(text, sizeof text, "%s", hovered->label);

            // Drop the accelerator rather than cut it in half.
            if(hovered->accel && hovered->accel[0] && (s32)strlen(text) > room)
                snprintf(text, sizeof text, "%s", hovered->label);

            hint = text;
            tooltip = true;
        }
        else if(hoveredTitle >= 0)
        {
            const MenuBarCategory* category = &menubar->categories[hoveredTitle];
            const char* label = category->title;
            bool home = isCompactHome(menubar, category);

            // A stop that shows only its number (BANK) names itself by its
            // selected item.
            if(category->items && category->selected >= 0 && category->selected < category->count)
                label = category->items[category->selected].label;

            if(home)
                snprintf(text, sizeof text, "%s [F10]", label);
            else
                snprintf(text, sizeof text, "%s", label);

            if(home && (s32)strlen(text) > room)
                snprintf(text, sizeof text, "%s", label);

            hint = text;
            tooltip = true;
        }
        else if(menubar->tooltip && menubar->tooltip[0])
        {
            // The editor's own hover text, as the old toolbar showed it.
            hint = menubar->tooltip;
            tooltip = true;
        }
    }
    else
    {
        s32 left = TitleX;

        for(s32 i = 0; i < menubar->count; i++)
        {
            const MenuBarCategory* category = &menubar->categories[i];

            if(category->hidden || category->right)
                continue;

            left += stopWidth(menubar, i) + TitleGap;
        }

        hintX = left + GroupGap;
        room = MAX(0, (rightStart(menubar) - hintX) / TIC_FONT_WIDTH);

        // The editor's own hover text, as the old toolbar showed it.
        if(menubar->tooltip && menubar->tooltip[0])
        {
            hint = menubar->tooltip;
            tooltip = true;
        }
    }

    if(hint && hint[0])
    {
        s32 len = MIN((s32)strlen(hint), room);

        if(len > 0)
        {
            char shown[64];
            memcpy(shown, hint, len);
            shown[len] = '\0';

            // A tooltip is darker than the app's name.
            tic_api_print(tic, shown, hintX, menubar->y + TextY,
                          tooltip ? tic_color_dark_grey : tic_color_grey,
                          true, 1, false);
        }
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

            if(item->checked)
                drawCheckLed(tic, rect.x + ItemPad, rect.y);
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
                          rect.x + rect.w - ItemPad - accelWidth(item->accel),
                          rect.y + TextY,
                          !item->enabled ? tic_color_light_grey
                          : over         ? tic_color_grey
                                         : tic_color_light_grey,
                          true, 1, true);
    }

    // Last, so an icon's eighth row cannot dent the outline.
    tic_api_rectb(tic, popup->x, popup->y, popup->w, popup->h, tic_color_dark_grey);
}

// Whether the pointer is over something the bar runs: a compact tab or pin, a
// title, or an enabled popup row.
static bool pointerHand(const Menubar* menubar, tic_point pos, const MenuBarItem* hovered)
{
    if(hovered)
        return true;

    if(pos.y >= menubar->y && pos.y < menubar->y + MENUBAR_HEIGHT)
        for(s32 i = 0; i < menubar->count; i++)
        {
            if(!focusableCategory(menubar, i))
                continue;

            tic_rect rect;
            titleRect(menubar, i, &rect);

            if(pointInRect(&pos, &rect))
                return true;
        }

    if(menubar->open >= 0)
    {
        tic_rect popup = popupRect(menubar);
        const MenuBarCategory* category = &menubar->categories[menubar->open];

        for(s32 i = 0; i < category->count; i++)
        {
            const MenuBarItem* item = &category->items[i];

            if(item->separator || !item->enabled)
                continue;

            tic_rect rect = itemRect(&popup, i);

            if(pointInRect(&pos, &rect))
                return true;
        }

        if(menubar->sub)
        {
            tic_rect sub = subPopupRect(menubar);

            for(s32 i = 0; i < menubar->sub->count; i++)
            {
                const MenuBarItem* item = &menubar->sub->items[i];

                if(item->separator || !item->enabled)
                    continue;

                tic_rect rect = itemRect(&sub, i);

                if(pointInRect(&pos, &rect))
                    return true;
            }
        }
    }

    return false;
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

    tic_api_rect(tic, 0, menubar->y, TIC80_WIDTH, MENUBAR_HEIGHT, tic_color_white);

    tic_point pos = tic_api_mouse(tic);
    const MenuBarItem* hovered = NULL;
    s32 hoveredTitle = -1;

    drawCompact(menubar, tic, icons, pos, &hovered);

    for(s32 i = 0; i < menubar->count; i++)
    {
        const MenuBarCategory* category = &menubar->categories[i];

        if(category->hidden || isTabs(menubar, category))
            continue;

        tic_rect rect;
        titleRect(menubar, i, &rect);

        bool open = i == menubar->open;
        bool over = pointInRect(&pos, &rect);
        bool focused = menubar->keyboard && i == menubar->focus;
        bool iconStop = isCompactHome(menubar, category) ||
                        isCompactMore(menubar, category) ||
                        !category->title[0];

        if(over)
            hoveredTitle = i;

        if(open)
            tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_dark_grey);
        else if(over || focused)
            tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_light_grey);

        s32 textX = rect.x + TitlePadding;

        if(category->icon)
        {
            // An icon-only stop draws its glyph on the cell's edge, as a pin
            // does.
            ui_icon(tic, icons, category->icon, iconStop ? rect.x : textX, rect.y,
                    category->iconColor ? category->iconColor
                    : open              ? tic_color_white
                                        : tic_color_dark_grey);
            textX += IconWidth;
        }

        if(!iconStop)
            tic_api_print(tic, category->title, textX, rect.y + TextY,
                          open ? tic_color_white : tic_color_dark_grey, true, 1, false);
    }

    drawHint(menubar, tic, hovered, hoveredTitle);

    if(menubar->open >= 0)
    {
        tic_rect popup = popupRect(menubar);
        const MenuBarCategory* category = &menubar->categories[menubar->open];

        drawPopup(tic, category, &popup, &pos, icons, menubar->checkIcon,
                  menubar->sub        ? menubar->subItem
                  : menubar->keyboard ? menubar->select
                                      : -1);

        if(menubar->sub)
        {
            tic_rect sub = subPopupRect(menubar);
            drawPopup(tic, menubar->sub, &sub, &pos, icons, menubar->checkIcon,
                      menubar->keyboard ? menubar->subSelect : -1);
        }
    }

    // The menu owns the pointer: over the bar, or anywhere while it is open.
    // Titles, compact tabs/pins and enabled popup rows are buttons: the hand.
    if(pointerHand(menubar, pos, hovered))
        ui_cursor(tic, tic_cursor_hand);
    else if(menubar->open >= 0 || pos.y < menubar->y + MENUBAR_HEIGHT)
        ui_cursor(tic, tic_cursor_arrow);

    if(menubar->pending)
    {
        MenuBarHandler handler = menubar->pending;
        s32 id = menubar->pendingId;

        menubar->pending = NULL;
        handler(menubar->data, id);
    }
}
