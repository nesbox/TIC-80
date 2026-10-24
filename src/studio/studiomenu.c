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

#include "studiomenu.h"
#include "config.h"
#include "rom.h"
#include "editors/code.h"
#include "editors/sprite.h"
#include "editors/map.h"
#include "editors/music.h"
#include "editors/sfx.h"
#include "screens/console.h"

#include <ctype.h>
#include <stdio.h>

// One studio per process, as the tables below are; filled by studiomenu_init
// before the bar is created.
static StudioMenuContext MenuState;

// clang-format off
#define MENU_ITEM(label, accel, handler, id)                    {label, accel, handler, id, true, false, 0, 0, NULL, -1}
#define MENU_ITEM_ICON(label, accel, handler, id, icon)         {label, accel, handler, id, true, false, icon, 0, NULL, -1}
#define MENU_ITEM_COLOR(label, accel, handler, id, icon, color) {label, accel, handler, id, true, false, icon, color, NULL, -1}
#define MENU_TOGGLE(label, accel, handler, id)                  {label, accel, handler, id, true, false, 0, 0, NULL, 0}
#define MENU_TOGGLE_ICON(label, accel, handler, id, icon)       {label, accel, handler, id, true, false, icon, 0, NULL, 0}
#define MENU_SUBMENU(label, menu)                               {label, NULL, NULL, 0, true, false, 0, 0, menu, -1}
#define MENU_SEPARATOR                                          {"", NULL, NULL, 0, true, true, 0, 0, NULL, -1}
#define MENU_CATEGORY(title, items)                             {title, items, COUNT_OF(items), false, 0, -1, 0, false}
// clang-format on

enum
{
    MenuActionScreenshot,
    MenuActionRecord,
    MenuActionQuit,
    MenuActionSave,
    MenuActionRun,
    MenuActionResume,
    MenuActionUndo,
    MenuActionRedo,
    MenuActionCut,
    MenuActionCopy,
    MenuActionPaste,
};

static void menubarClipboard(Studio* studio, s32 action)
{
    const EditorApp* app = &MenuState.apps[getStudioMode(studio)];

    if(!app->clipboard)
        return;

    void* instance = app->instance(studio);

    switch(action)
    {
        case MenuActionUndo: app->clipboard->undo(instance); break;
        case MenuActionRedo: app->clipboard->redo(instance); break;
        case MenuActionCut: app->clipboard->cut(instance); break;
        case MenuActionCopy: app->clipboard->copy(instance); break;
        case MenuActionPaste: app->clipboard->paste(instance); break;
    }
}

static void menubarAction(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;

    switch(id)
    {
        case MenuActionScreenshot: takeScreenshot(studio); break;
        case MenuActionRecord: startVideoRecord(studio); break;
        case MenuActionQuit: exitStudio(studio); break;
        case MenuActionSave: saveProject(studio); break;
        case MenuActionRun: runGame(studio, RUN_FROM_STUDIO); break;
        case MenuActionResume: resumeGame(studio); break;

        case MenuActionUndo:
        case MenuActionRedo:
        case MenuActionCut:
        case MenuActionCopy:
        case MenuActionPaste:
            menubarClipboard(studio, id);
            break;
    }
}

static void menubarSetMode(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    ctx->bar->requested = id;
}

enum
{
    CodeActionFind = 100,
    CodeActionGoto,
    CodeActionBookmarks,
    CodeActionOutline,
    CodeActionFont,
    CodeActionShadow,
    CodeActionLineNumbers,
    CodeActionSidebar,
    CodeActionStatusbar,
};

enum
{
    SpriteActionTiles = 200,
    SpriteActionSprites,
    SpriteActionFlags,
    SpriteActionDraw,
    SpriteActionPick,
    SpriteActionSelect,
    SpriteActionFill,
    SpriteActionFlipHorz,
    SpriteActionFlipVert,
    SpriteActionRotate,
    SpriteActionErase,
    SpriteActionBpp4,
    SpriteActionBpp2,
    SpriteActionBpp1,
    SpriteActionVbank0,
    SpriteActionVbank1,
    SpriteActionPalette,
    SpriteActionSize8,
    SpriteActionSize16,
    SpriteActionSize32,
    SpriteActionSize64,
    SpriteActionBrush1,
    SpriteActionBrush2,
    SpriteActionBrush3,
    SpriteActionBrush4,
    SpriteActionPage1,
    SpriteActionPage2,
    SpriteActionPage3,
    SpriteActionPage4,
};

enum
{
    MapActionTiles = 300,
    MapActionSprites,
    MapActionGrid,
    MapActionRandom,
    MapActionTileset,
    MapActionWorld,
    MapActionDraw,
    MapActionDrag,
    MapActionSelect,
    MapActionFill,
    MapActionDepth4,
    MapActionDepth2,
    MapActionDepth1,
    MapActionPage1,
    MapActionPage2,
    MapActionPage3,
    MapActionPage4,
};

enum
{
    MusicActionPiano = 400,
    MusicActionTracker,
    MusicActionFollow,
    MusicActionLoop,
    MusicActionSustain,
    MusicActionPlayNow,
    MusicActionPlayFrame,
    MusicActionPlayTrack,
    MusicActionResetTrack,
    MusicActionStop,
};

// clang-format off
enum
{
#define EXPORT_ENUM(name, label) Export_##name,
    EXPORT_CMD_LIST(EXPORT_ENUM)
#undef EXPORT_ENUM
    ExportActionAlone,
};
// clang-format on

enum
{
#define HELP_ENUM(name, label) Help_##name,
    HELP_CMD_LIST(HELP_ENUM)
#undef HELP_ENUM
};

enum
{
    EditorTabSize1,
    EditorTabSize2,
    EditorTabSize4,
    EditorTabSize8,
    EditorTabModeAuto,
    EditorTabModeTabs,
    EditorTabModeSpaces,
    EditorKeybindStandard,
    EditorKeybindEmacs,
    EditorKeybindVi,
};

static void menubarCodeAction(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Code* code = studiomenu_editor(studio, TIC_CODE_MODE);

    switch(id)
    {
        case CodeActionFind: codeFind(code); break;
        case CodeActionGoto: codeGoto(code); break;
        case CodeActionBookmarks: codeBookmarks(code); break;
        case CodeActionOutline: codeOutline(code); break;
        case CodeActionFont: studioCodeViewToggle(studio, CodeViewFont); break;
        case CodeActionShadow: studioCodeViewToggle(studio, CodeViewShadow); break;
        case CodeActionLineNumbers: studioCodeViewToggle(studio, CodeViewLineNumbers); break;
        case CodeActionSidebar: studioCodeViewToggle(studio, CodeViewSidebar); break;
        case CodeActionStatusbar: studioCodeViewToggle(studio, CodeViewStatusbar); break;
    }
}

static void menubarSpriteAction(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Sprite* sprite = studiomenu_editor(studio, TIC_SPRITE_MODE);

    switch(id)
    {
        case SpriteActionTiles: spriteSelectBank(sprite, 0); break;
        case SpriteActionSprites: spriteSelectBank(sprite, 1); break;
        case SpriteActionDraw: spriteSetMode(sprite, SPRITE_DRAW_MODE); break;
        case SpriteActionPick: spriteSetMode(sprite, SPRITE_PICK_MODE); break;
        case SpriteActionSelect: spriteSetMode(sprite, SPRITE_SELECT_MODE); break;
        case SpriteActionFill: spriteSetMode(sprite, SPRITE_FILL_MODE); break;
        case SpriteActionFlipHorz: spriteToolAction(sprite, SPRITE_TOOL_FLIP_HORZ); break;
        case SpriteActionFlipVert: spriteToolAction(sprite, SPRITE_TOOL_FLIP_VERT); break;
        case SpriteActionRotate: spriteToolAction(sprite, SPRITE_TOOL_ROTATE); break;
        case SpriteActionErase: spriteToolAction(sprite, SPRITE_TOOL_ERASE); break;
        case SpriteActionBpp4: spriteSetBpp(sprite, tic_bpp_4); break;
        case SpriteActionBpp2: spriteSetBpp(sprite, tic_bpp_2); break;
        case SpriteActionBpp1: spriteSetBpp(sprite, tic_bpp_1); break;
        case SpriteActionVbank0: sprite->palette.vbank1 = false; break;
        case SpriteActionVbank1: sprite->palette.vbank1 = true; break;
        case SpriteActionPalette:
            sprite->palette.edit = !sprite->palette.edit;

            if(!sprite->palette.edit)
                sprite->palette.focus = -1;
            break;
        case SpriteActionSize8: spriteSetSize(sprite, 8); break;
        case SpriteActionSize16: spriteSetSize(sprite, 16); break;
        case SpriteActionSize32: spriteSetSize(sprite, 32); break;
        case SpriteActionSize64: spriteSetSize(sprite, 64); break;
        case SpriteActionBrush1: spriteSetBrushSize(sprite, 1); break;
        case SpriteActionBrush2: spriteSetBrushSize(sprite, 2); break;
        case SpriteActionBrush3: spriteSetBrushSize(sprite, 3); break;
        case SpriteActionBrush4: spriteSetBrushSize(sprite, 4); break;

        case SpriteActionPage1: spriteSelectPage(sprite, 0); break;
        case SpriteActionPage2: spriteSelectPage(sprite, 1); break;
        case SpriteActionPage3: spriteSelectPage(sprite, 2); break;
        case SpriteActionPage4: spriteSelectPage(sprite, 3); break;

        case SpriteActionFlags:
            sprite->flags.show = !sprite->flags.show;

            if(!sprite->flags.show)
                sprite->flags.edit = false;
            break;
    }
}

static void menubarMapAction(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Map* map = studiomenu_editor(studio, TIC_MAP_MODE);

    switch(id)
    {
        case MapActionTiles: map_select_bank(map, 0); break;
        case MapActionSprites: map_select_bank(map, 1); break;
        case MapActionGrid: map->canvas.grid = !map->canvas.grid; break;
        case MapActionRandom: map->canvas.random = !map->canvas.random; break;
        case MapActionTileset: map_toggle_sheet(map); break;
        case MapActionWorld: ctx->bar->requested = TIC_WORLD_MODE; break;

        case MapActionDraw: map->mode = MAP_DRAW_MODE; break;
        case MapActionDrag: map->mode = MAP_DRAG_MODE; break;
        case MapActionSelect: map->mode = MAP_SELECT_MODE; break;
        case MapActionFill: map->mode = MAP_FILL_MODE; break;

        case MapActionDepth4: mapSetBpp(map, tic_bpp_4); break;
        case MapActionDepth2: mapSetBpp(map, tic_bpp_2); break;
        case MapActionDepth1: mapSetBpp(map, tic_bpp_1); break;

        case MapActionPage1: map_select_page(map, 0); break;
        case MapActionPage2: map_select_page(map, 1); break;
        case MapActionPage3: map_select_page(map, 2); break;
        case MapActionPage4: map_select_page(map, 3); break;
    }
}

static void menubarMusicAction(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Music* music = studiomenu_editor(studio, TIC_MUSIC_MODE);

    switch(id)
    {
        case MusicActionPiano: music->tab = MUSIC_PIANO_TAB; break;
        case MusicActionTracker: music->tab = MUSIC_TRACKER_TAB; break;
        case MusicActionFollow: musicToggleFollow(music); break;
        case MusicActionLoop: musicToggleLoop(music); break;
        case MusicActionSustain: musicToggleSustain(music); break;
        case MusicActionPlayNow: musicPlayFromNow(music); break;
        case MusicActionPlayFrame: musicPlayFrame(music); break;
        case MusicActionPlayTrack: musicPlayTrack(music, false); break;
        case MusicActionResetTrack: musicPlayTrack(music, true); break;
        case MusicActionStop: musicStop(music); break;
    }
}

#if defined(TIC80_PRO)
static bool ExportAlone;
#endif

static const char* const ExportTypes[] =
    {
#define EXPORT_TYPE(name, label) #name,
        EXPORT_CMD_LIST(EXPORT_TYPE)
#undef EXPORT_TYPE
};

static const char* exportSuffix(Studio* studio, s32 id, char* buffer, s32 size)
{
    switch(id)
    {
        case Export_win: return "-win";
        case Export_linux: return "-linux";
        case Export_linuxarm: return "-linuxarm";
        case Export_mac: return "-macos";
        case Export_macintel: return "-macintel";
        case Export_html: return "-html";
        case Export_tiles: return "-tiles";
        case Export_sprites: return "-sprites";
        case Export_map: return "-map";
        case Export_mapimg: return "-map";
        case Export_screen: return "-screen";

        case Export_sfx:
        {
            Sfx* sfx = studiomenu_editor(studio, TIC_SFX_MODE);

            snprintf(buffer, size, "-sfx-%02i", sfx ? sfx->index : 0);
            return buffer;
        }

        case Export_music:
        {
            Music* music = studiomenu_editor(studio, TIC_MUSIC_MODE);

            snprintf(buffer, size, "-music-%02i", music ? music->track : 0);
            return buffer;
        }

        default:
            return "";
    }
}

#if defined(TIC80_PRO)
static s32 exportBank(Studio* studio, s32 id)
{
    switch(id)
    {
        case Export_tiles:
        case Export_sprites:
            return studiomenu_editor_bank(studio, TIC_SPRITE_MODE);

        case Export_map:
        case Export_mapimg:
        case Export_screen:
            return studiomenu_editor_bank(studio, TIC_MAP_MODE);

        case Export_music:
            return studiomenu_editor_bank(studio, TIC_MUSIC_MODE);

        default:
            return 0;
    }
}
#endif

static s32 exportIndex(Studio* studio, s32 id)
{
    switch(id)
    {
        case Export_sfx:
        {
            Sfx* sfx = studiomenu_editor(studio, TIC_SFX_MODE);
            return sfx ? sfx->index : 0;
        }

        case Export_music:
        {
            Music* music = studiomenu_editor(studio, TIC_MUSIC_MODE);
            return music ? music->track : 0;
        }

        default:
            return -1;
    }
}

static s32 exportVbank(Studio* studio)
{
    Sprite* sprite = studiomenu_editor(studio, TIC_SPRITE_MODE);

    return sprite && sprite->palette.vbank1 ? 1 : 0;
}

static void menubarExport(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;

    if(id < 0 || id >= (s32)COUNT_OF(ExportTypes))
        return;

    char suffix[16];
    char filename[TICNAME_MAX + 16];

    if(id == Export_help)
        snprintf(filename, sizeof filename, "tic80-help");
    else
    {
        const CartName* cart = studioCart(studio);
        const char* base = cart->name[0] && !strchr(cart->name, ' ') ? cart->name : "game";

        snprintf(filename, sizeof filename, "%s%s", base, exportSuffix(studio, id, suffix, sizeof suffix));
    }

    char command[TICNAME_MAX + 64];
    s32 length = snprintf(command, sizeof command, "export %s %s", ExportTypes[id], filename);

    if(length < 0 || (size_t)length >= sizeof command)
        return;

    switch(id)
    {
        case Export_tiles:
        case Export_sprites:
        case Export_map:
        case Export_mapimg:
#if defined(TIC80_PRO)
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " bank=%i", exportBank(studio, id));
#endif
            break;

        case Export_screen:
#if defined(TIC80_PRO)
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " bank=%i", exportBank(studio, id));
#endif
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " vbank=%i", exportVbank(studio));
            break;

        case Export_sfx:
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " id=%i", exportIndex(studio, id));
            break;

        case Export_music:
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " id=%i", exportIndex(studio, id));
#if defined(TIC80_PRO)
            length += snprintf(command + length, sizeof command - (size_t)length,
                               " bank=%i", exportBank(studio, id));
#endif
            break;

        default:
            break;
    }

#if defined(TIC80_PRO)
    if(ExportAlone)
        switch(id)
        {
            case Export_win:
            case Export_linux:
            case Export_linuxarm:
            case Export_mac:
            case Export_macintel:
                snprintf(command + length, sizeof command - (size_t)length, " alone=1");
                break;

            default:
                break;
        }
#endif

    studiomenu_command(studio, command);
}

#if defined(TIC80_PRO)
static void menubarExportAlone(void* data, s32 id)
{
    (void)data;
    (void)id;

    ExportAlone = !ExportAlone;
}
#endif

static void menubarNew(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    char command[TICNAME_MAX];

    if(id >= 0)
    {
        const tic_script* script = tic_scripts()[id];

        if(!script)
            return;

        snprintf(command, sizeof command, "new %.*s", (s32)sizeof command - 5, script->name);
    }
    else
        snprintf(command, sizeof command, "new");

    studiomenu_command(studio, command);
}

static const char* const HelpTopics[] =
    {
#define HELP_TOPIC(name, label) #name,
        HELP_CMD_LIST(HELP_TOPIC)
#undef HELP_TOPIC
};

static void menubarHelp(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;

    if(id < 0 || id >= (s32)COUNT_OF(HelpTopics))
        return;

    char command[64];
    snprintf(command, sizeof command, "help %s", HelpTopics[id]);

    studiomenu_command(studio, command);
}

enum
{
    OptionCrt,
    OptionFullscreen,
    OptionIntegerScale,
    OptionAutohide,
    OptionCompact,
#if defined(BUILD_SURF)
    OptionAutosave,
#endif
};

static void menubarOption(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Menubar* bar = ctx->bar;
    Config* config = studio_config_get(studio);

    switch(id)
    {
        case OptionCrt:
#if defined(CRT_SHADER_SUPPORT)
            config->data.options.crt = !config->data.options.crt;
#endif
            break;

        case OptionFullscreen:
            gotoFullscreen(studio);
            break;

        case OptionIntegerScale:
            config->data.options.integerScale = !config->data.options.integerScale;
            break;

        case OptionAutohide:
            bar->autohide = !bar->autohide;
            config->data.options.autohide = bar->autohide;
            break;

        case OptionCompact:
            bar->compact = !bar->compact;
            config->data.options.compactBar = bar->compact;
            break;

#if defined(BUILD_SURF)
        case OptionAutosave:
            config->data.options.autosave = !config->data.options.autosave;
            break;
#endif
    }

    config->saveOptions(config);
}

static void menubarEditorOption(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    Config* config = studio_config_get(studio);

    switch(id)
    {
        case EditorTabSize1: config->data.options.tabSize = 1; break;
        case EditorTabSize2: config->data.options.tabSize = 2; break;
        case EditorTabSize4: config->data.options.tabSize = 4; break;
        case EditorTabSize8: config->data.options.tabSize = 8; break;
        case EditorTabModeAuto: config->data.options.tabMode = TAB_AUTO; break;
        case EditorTabModeTabs: config->data.options.tabMode = TAB_TAB; break;
        case EditorTabModeSpaces: config->data.options.tabMode = TAB_SPACE; break;
        case EditorKeybindStandard: config->data.options.keybindMode = KEYBIND_STANDARD; break;
        case EditorKeybindEmacs: config->data.options.keybindMode = KEYBIND_EMACS; break;
        case EditorKeybindVi: config->data.options.keybindMode = KEYBIND_VI; break;
    }

    config->saveOptions(config);
}

#if defined(TIC80_PRO)
static void menubarBank(void* data, s32 id)
{
    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    studiomenu_bank_switch(studio, id);
}

static void menubarBankChain(void* data, s32 id)
{
    (void)id;

    StudioMenuContext* ctx = data;
    Studio* studio = ctx->studio;
    studiomenu_bank_chain(studio);
}
#endif

// The app switcher: the editor items are built from the host's mode list, the
// one list the keys and the cycling follow, then the screens that are not editors.
enum
{
    // The editors, a separator, the console, the browser and the game menu,
    // with one spare. TIC_MODES_COUNT is the compile-time bound; the host's
    // mode list fills it.
    MenubarAppSlots = TIC_MODES_COUNT + 5,
};

static MenuBarItem MenubarApps[MenubarAppSlots];
static s32 MenubarAppsCount;
static char MenubarAppLabels[TIC_MODES_COUNT][16];
static char MenubarAppAccels[TIC_MODES_COUNT][4];

static void shortName(const char* name, char* out, s32 size)
{
    s32 length = 0;

    while(name && name[length] && name[length] != ' ' && length < size - 1)
    {
        out[length] = name[length];
        length++;
    }

    out[length] = '\0';
}

static void initMenubarApps(void)
{
    s32 count = 0;

    for(s32 i = 0; i < MenuState.modeCount; i++)
    {
        const EditorApp* app = &MenuState.apps[MenuState.modes[i]];

        shortName(app->name, MenubarAppLabels[i], sizeof MenubarAppLabels[i]);

        if(app->hotkey >= tic_key_f1 && app->hotkey <= tic_key_f12)
            snprintf(MenubarAppAccels[i], sizeof MenubarAppAccels[i], "F%i",
                     (s32)(app->hotkey - tic_key_f1) + 1);
        else
            MenubarAppAccels[i][0] = '\0';

        MenubarApps[count++] = (MenuBarItem){
            .label = MenubarAppLabels[i],
            .accel = MenubarAppAccels[i][0] ? MenubarAppAccels[i] : NULL,
            .handler = menubarSetMode,
            .id = MenuState.modes[i],
            .enabled = true,
            .separator = false,
            .icon = app->icon,
            .color = 0,
            .submenu = NULL,
            .checked = -1,
        };
    }

    MenubarApps[count++] = (MenuBarItem){"", NULL, NULL, 0, true, true, 0, 0, NULL, -1};
    MenubarApps[count++] = (MenuBarItem){"CONSOLE", "ESC", menubarSetMode, TIC_CONSOLE_MODE, true, false, tic_icon_console, 0, NULL, -1};
#if defined(BUILD_SURF)
    MenubarApps[count++] = (MenuBarItem){"SURF", NULL, menubarSetMode, TIC_SURF_MODE, true, false, tic_icon_surf, 0, NULL, -1};
#endif
    MenubarApps[count++] = (MenuBarItem){"GAME MENU", NULL, menubarSetMode, TIC_MENU_MODE, true, false, tic_icon_game_menu, 0, NULL, -1};

    MenubarAppsCount = count;
}

// clang-format off
static MenuBarItem MenubarExport[] =
    {
#define EXPORT_ASSET_ITEM(name, label) MENU_ITEM(label, NULL, menubarExport, Export_## name),
        EXPORT_ASSET_LIST(EXPORT_ASSET_ITEM)
#undef EXPORT_ASSET_ITEM
        MENU_SEPARATOR,
#define EXPORT_GAME_ITEM(name, label) MENU_ITEM(label, NULL, menubarExport, Export_## name),
        EXPORT_GAME_LIST(EXPORT_GAME_ITEM)
#undef EXPORT_GAME_ITEM
#if defined(TIC80_PRO)
        MENU_SEPARATOR,
        MENU_TOGGLE("ALONE", NULL, menubarExportAlone, ExportActionAlone),
#endif
};
// clang-format on

static const MenuBarCategory MenubarExportMenu = MENU_CATEGORY("EXPORT", MenubarExport);

static const MenuBarItem MenubarHelp[] =
    {
#define HELP_ITEM(name, label) MENU_ITEM(label, NULL, menubarHelp, Help_##name),
        HELP_CMD_LIST(HELP_ITEM)
#undef HELP_ITEM
};

static const MenuBarCategory MenubarHelpMenu = MENU_CATEGORY("HELP", MenubarHelp);

static MenuBarItem MenubarEditor[] =
    {
        MENU_TOGGLE("TAB SIZE 1", NULL, menubarEditorOption, EditorTabSize1),
        MENU_TOGGLE("TAB SIZE 2", NULL, menubarEditorOption, EditorTabSize2),
        MENU_TOGGLE("TAB SIZE 4", NULL, menubarEditorOption, EditorTabSize4),
        MENU_TOGGLE("TAB SIZE 8", NULL, menubarEditorOption, EditorTabSize8),
        MENU_SEPARATOR,
        MENU_TOGGLE("MODE AUTO", NULL, menubarEditorOption, EditorTabModeAuto),
        MENU_TOGGLE("MODE TABS", NULL, menubarEditorOption, EditorTabModeTabs),
        MENU_TOGGLE("MODE SPACES", NULL, menubarEditorOption, EditorTabModeSpaces),
        MENU_SEPARATOR,
        MENU_TOGGLE("KEY STANDARD", NULL, menubarEditorOption, EditorKeybindStandard),
        MENU_TOGGLE("KEY EMACS", NULL, menubarEditorOption, EditorKeybindEmacs),
        MENU_TOGGLE("KEY VI", NULL, menubarEditorOption, EditorKeybindVi),
};

static const MenuBarCategory MenubarEditorMenu = MENU_CATEGORY("EDITOR", MenubarEditor);

static MenuBarItem MenubarOptions[] =
    {
#if defined(CRT_SHADER_SUPPORT)
        MENU_TOGGLE("CRT MONITOR", "F6", menubarOption, OptionCrt),
#endif
        MENU_TOGGLE("FULLSCREEN", "F11", menubarOption, OptionFullscreen),
        MENU_TOGGLE("INTEGER SCALE", NULL, menubarOption, OptionIntegerScale),
        MENU_TOGGLE("AUTOHIDE MENU", NULL, menubarOption, OptionAutohide),
        MENU_TOGGLE("COMPACT BAR", NULL, menubarOption, OptionCompact),
#if defined(BUILD_SURF)
        MENU_SEPARATOR,
        MENU_TOGGLE("SAVE SURF CARTS", NULL, menubarOption, OptionAutosave),
#endif
};

static const MenuBarCategory MenubarOptionsMenu = MENU_CATEGORY("OPTIONS", MenubarOptions);

#if defined(TIC80_PRO)
enum
{
    BankActionChain = TIC_BANKS,
};

#define BANK_ITEM(n) {"BANK " #n, NULL, menubarBank, n, true, false, tic_icon_bank, tic_color_red, NULL, -1}
static MenuBarItem MenubarBanks[] =
    {
        BANK_ITEM(0),
        BANK_ITEM(1),
        BANK_ITEM(2),
        BANK_ITEM(3),
        BANK_ITEM(4),
        BANK_ITEM(5),
        BANK_ITEM(6),
        BANK_ITEM(7),
        MENU_SEPARATOR,
        MENU_TOGGLE("CHAIN", NULL, menubarBankChain, BankActionChain),
};
#undef BANK_ITEM
#endif

static MenuBarItem MenubarCodeApp[] =
    {
        MENU_ITEM_ICON("FIND", "CTRL+F", menubarCodeAction, CodeActionFind, tic_icon_find),
        MENU_ITEM_ICON("GOTO", "CTRL+G", menubarCodeAction, CodeActionGoto, tic_icon_goto),
        MENU_ITEM_ICON("BOOKMARKS", "CTRL+B", menubarCodeAction, CodeActionBookmarks, tic_icon_bookmark),
        MENU_ITEM_ICON("OUTLINE", "CTRL+O", menubarCodeAction, CodeActionOutline, tic_icon_outline),
        MENU_SEPARATOR,
        MENU_TOGGLE("TINY FONT", "ALT+F", menubarCodeAction, CodeActionFont),
        MENU_TOGGLE_ICON("SHADOW", "ALT+S", menubarCodeAction, CodeActionShadow, tic_icon_shadow),
        MENU_TOGGLE_ICON("LINES", "ALT+L", menubarCodeAction, CodeActionLineNumbers, tic_icon_line_numbers),
        MENU_TOGGLE("SIDEBAR", NULL, menubarCodeAction, CodeActionSidebar),
        MENU_TOGGLE("STATUSBAR", NULL, menubarCodeAction, CodeActionStatusbar),
        MENU_SEPARATOR,
        MENU_SUBMENU("OPTIONS", &MenubarEditorMenu),
};

static MenuBarItem MenubarPalette[] =
    {
        MENU_TOGGLE("VBANK0", NULL, menubarSpriteAction, SpriteActionVbank0),
        MENU_TOGGLE("VBANK1", NULL, menubarSpriteAction, SpriteActionVbank1),
        MENU_SEPARATOR,
        MENU_TOGGLE("EDIT", NULL, menubarSpriteAction, SpriteActionPalette),
};

static const MenuBarCategory MenubarPaletteMenu = MENU_CATEGORY("PALETTE", MenubarPalette);

static MenuBarItem MenubarCanvasSize[] =
    {
        MENU_TOGGLE("8x8", NULL, menubarSpriteAction, SpriteActionSize8),
        MENU_TOGGLE("16x16", NULL, menubarSpriteAction, SpriteActionSize16),
        MENU_TOGGLE("32x32", NULL, menubarSpriteAction, SpriteActionSize32),
        MENU_TOGGLE("64x64", NULL, menubarSpriteAction, SpriteActionSize64),
};

static const MenuBarCategory MenubarCanvasSizeMenu = MENU_CATEGORY("CANVAS", MenubarCanvasSize);

static MenuBarItem MenubarBrushSize[] =
    {
        MENU_TOGGLE("1x1", NULL, menubarSpriteAction, SpriteActionBrush1),
        MENU_TOGGLE("2x2", NULL, menubarSpriteAction, SpriteActionBrush2),
        MENU_TOGGLE("3x3", NULL, menubarSpriteAction, SpriteActionBrush3),
        MENU_TOGGLE("4x4", NULL, menubarSpriteAction, SpriteActionBrush4),
};

static const MenuBarCategory MenubarBrushSizeMenu = MENU_CATEGORY("BRUSH", MenubarBrushSize);

static MenuBarItem MenubarSpriteTileset[] =
    {
        MENU_TOGGLE("4BPP", NULL, menubarSpriteAction, SpriteActionBpp4),
        MENU_TOGGLE("2BPP", NULL, menubarSpriteAction, SpriteActionBpp2),
        MENU_TOGGLE("1BPP", NULL, menubarSpriteAction, SpriteActionBpp1),
        MENU_SEPARATOR,
        MENU_TOGGLE("PAGE 1", NULL, menubarSpriteAction, SpriteActionPage1),
        MENU_TOGGLE("PAGE 2", NULL, menubarSpriteAction, SpriteActionPage2),
        MENU_TOGGLE("PAGE 3", NULL, menubarSpriteAction, SpriteActionPage3),
        MENU_TOGGLE("PAGE 4", NULL, menubarSpriteAction, SpriteActionPage4),
};

static const MenuBarCategory MenubarSpriteTilesetMenu = MENU_CATEGORY("TILESET", MenubarSpriteTileset);

static MenuBarItem MenubarSpriteTools[] =
    {
        MENU_TOGGLE_ICON("BRUSH", "1", menubarSpriteAction, SpriteActionDraw, tic_icon_pen),
        MENU_TOGGLE_ICON("PICKER", "2", menubarSpriteAction, SpriteActionPick, tic_icon_tinypicker),
        MENU_TOGGLE_ICON("SELECT", "3", menubarSpriteAction, SpriteActionSelect, tic_icon_select),
        MENU_TOGGLE_ICON("FILL", "4", menubarSpriteAction, SpriteActionFill, tic_icon_fill),
        MENU_SEPARATOR,
        MENU_ITEM_ICON("FLIP HORZ", "5", menubarSpriteAction, SpriteActionFlipHorz, tic_icon_tinyfliphorz),
        MENU_ITEM_ICON("FLIP VERT", "6", menubarSpriteAction, SpriteActionFlipVert, tic_icon_tinyflipvert),
        MENU_ITEM_ICON("ROTATE", "7", menubarSpriteAction, SpriteActionRotate, tic_icon_tinyrotate),
        MENU_ITEM_ICON("ERASE", "8", menubarSpriteAction, SpriteActionErase, tic_icon_tinyerase),
};

static const MenuBarCategory MenubarSpriteToolsMenu = MENU_CATEGORY("TOOLS", MenubarSpriteTools);

static MenuBarItem MenubarSpriteApp[] =
    {
        MENU_TOGGLE_ICON("TILES", "TAB", menubarSpriteAction, SpriteActionTiles, tic_icon_tiles),
        MENU_TOGGLE_ICON("SPRITES", NULL, menubarSpriteAction, SpriteActionSprites, tic_icon_sprites),
        MENU_SEPARATOR,
        MENU_SUBMENU("TOOLS", &MenubarSpriteToolsMenu),
        MENU_SUBMENU("TILESET", &MenubarSpriteTilesetMenu),
        MENU_SEPARATOR,
        MENU_TOGGLE("FLAGS", NULL, menubarSpriteAction, SpriteActionFlags),
        MENU_SEPARATOR,
        MENU_SUBMENU("PALETTE", &MenubarPaletteMenu),
        MENU_SUBMENU("CANVAS", &MenubarCanvasSizeMenu),
        MENU_SUBMENU("BRUSH", &MenubarBrushSizeMenu),
};

static MenuBarItem MenubarMapTileset[] =
    {
        MENU_TOGGLE_ICON("TILES", NULL, menubarMapAction, MapActionTiles, tic_icon_tiles),
        MENU_TOGGLE_ICON("SPRITES", NULL, menubarMapAction, MapActionSprites, tic_icon_sprites),
        MENU_SEPARATOR,
        MENU_TOGGLE("4BPP", NULL, menubarMapAction, MapActionDepth4),
        MENU_TOGGLE("2BPP", NULL, menubarMapAction, MapActionDepth2),
        MENU_TOGGLE("1BPP", NULL, menubarMapAction, MapActionDepth1),
        MENU_SEPARATOR,
        MENU_TOGGLE("PAGE 1", NULL, menubarMapAction, MapActionPage1),
        MENU_TOGGLE("PAGE 2", NULL, menubarMapAction, MapActionPage2),
        MENU_TOGGLE("PAGE 3", NULL, menubarMapAction, MapActionPage3),
        MENU_TOGGLE("PAGE 4", NULL, menubarMapAction, MapActionPage4),
};

static const MenuBarCategory MenubarMapTilesetMenu = MENU_CATEGORY("TILESET", MenubarMapTileset);

static MenuBarItem MenubarMapApp[] =
    {
        MENU_TOGGLE_ICON("DRAW", "1", menubarMapAction, MapActionDraw, tic_icon_pen),
        MENU_TOGGLE_ICON("DRAG MAP", "2", menubarMapAction, MapActionDrag, tic_icon_hand),
        MENU_TOGGLE_ICON("SELECT", "3", menubarMapAction, MapActionSelect, tic_icon_select),
        MENU_TOGGLE_ICON("FILL", "4", menubarMapAction, MapActionFill, tic_icon_fill),
        MENU_SEPARATOR,
        MENU_SUBMENU("TILESET", &MenubarMapTilesetMenu),
        MENU_SEPARATOR,
        MENU_TOGGLE_ICON("GRID", "`", menubarMapAction, MapActionGrid, tic_icon_grid),
        MENU_TOGGLE_ICON("RANDOM", "R", menubarMapAction, MapActionRandom, tic_icon_random),
        MENU_TOGGLE_ICON("SHOW TILES", "SHIFT", menubarMapAction, MapActionTileset, tic_icon_show_tiles),
        MENU_SEPARATOR,
        MENU_ITEM_ICON("WORLD MAP", "TAB", menubarMapAction, MapActionWorld, tic_icon_world),
};

static MenuBarItem MenubarMusicApp[] =
    {
        MENU_TOGGLE_ICON("PIANO", NULL, menubarMusicAction, MusicActionPiano, tic_icon_piano),
        MENU_TOGGLE_ICON("TRACKER", NULL, menubarMusicAction, MusicActionTracker, tic_icon_tracker),
        MENU_SEPARATOR,
        MENU_TOGGLE_ICON("FOLLOW", "CTRL+F", menubarMusicAction, MusicActionFollow, tic_icon_follow),
        MENU_TOGGLE_ICON("LOOP", NULL, menubarMusicAction, MusicActionLoop, tic_icon_loop),
        MENU_TOGGLE_ICON("SUSTAIN", NULL, menubarMusicAction, MusicActionSustain, tic_icon_sustain),
        MENU_SEPARATOR,
        MENU_ITEM_ICON("PLAY FROM NOW", "SHIFT+ENTER", menubarMusicAction, MusicActionPlayNow, tic_icon_playnow),
        MENU_ITEM_ICON("PLAY FRAME", "ENTER", menubarMusicAction, MusicActionPlayFrame, tic_icon_playframe),
        MENU_ITEM_ICON("PLAY TRACK", "SPACE", menubarMusicAction, MusicActionPlayTrack, tic_icon_right),
        MENU_ITEM_ICON("RESET TRACK", "SHIFT+SPACE", menubarMusicAction, MusicActionResetTrack, tic_icon_tinyrotate),
        MENU_ITEM_ICON("STOP", "SPACE", menubarMusicAction, MusicActionStop, tic_icon_stop),
};

static const MenuBarItem MenubarEdit[] =
    {
        MENU_ITEM_COLOR("UNDO", "CTRL+Z", menubarAction, MenuActionUndo, tic_icon_undo, tic_color_light_green),
        MENU_ITEM_COLOR("REDO", "CTRL+Y", menubarAction, MenuActionRedo, tic_icon_redo, tic_color_green),
        MENU_SEPARATOR,
        MENU_ITEM_COLOR("CUT", "CTRL+X", menubarAction, MenuActionCut, tic_icon_cut, tic_color_red),
        MENU_ITEM_COLOR("COPY", "CTRL+C", menubarAction, MenuActionCopy, tic_icon_copy, tic_color_orange),
        MENU_ITEM_COLOR("PASTE", "CTRL+V", menubarAction, MenuActionPaste, tic_icon_paste, tic_color_yellow),
};

static MenuBarItem MenubarTic[] =
    {
        MENU_ITEM("NEW", NULL, NULL, 0),
        MENU_ITEM("RUN", "CTRL+R", menubarAction, MenuActionRun),
        MENU_ITEM("RESUME", NULL, menubarAction, MenuActionResume),
        MENU_ITEM("SAVE", "CTRL+S", menubarAction, MenuActionSave),
        MENU_SUBMENU("EXPORT", &MenubarExportMenu),
        MENU_SEPARATOR,
        MENU_ITEM("SCREENSHOT", "F8", menubarAction, MenuActionScreenshot),
        MENU_ITEM("RECORD GIF", "F9", menubarAction, MenuActionRecord),
        MENU_SEPARATOR,
        MENU_SUBMENU("OPTIONS", &MenubarOptionsMenu),
        MENU_SUBMENU("HELP", &MenubarHelpMenu),
        MENU_SEPARATOR,
        MENU_ITEM("QUIT", "CTRL+Q", menubarAction, MenuActionQuit),
};

// The compact bar's pins: pointers at the menu items, so a pin and its menu row
// share one state. EDIT is expanded into pins in every editor; RUN and SAVE
// stay in the TIC-80 menu.
#define EDIT_PINS &MenubarEdit[3], &MenubarEdit[4], &MenubarEdit[5], &MenubarEdit[0], &MenubarEdit[1],

static const MenuBarItem* const MenubarCodePins[] =
    {
        EDIT_PINS & MenubarCodeApp[0], // FIND
        &MenubarCodeApp[1],            // GOTO
};

static const MenuBarItem* const MenubarSpritePins[] =
    {
        EDIT_PINS & MenubarSpriteApp[0], // TILES
        &MenubarSpriteApp[1],            // SPRITES
};

static const MenuBarItem* const MenubarMapPins[] =
    {
        EDIT_PINS & MenubarMapApp[0], // DRAW
        &MenubarMapApp[1],            // DRAG MAP
        &MenubarMapApp[2],            // SELECT
        &MenubarMapApp[3],            // FILL
        &MenubarMapApp[9],            // SHOW TILES
};

static const MenuBarItem* const MenubarMusicPins[] =
    {
        EDIT_PINS & MenubarMusicApp[0], // PIANO
        &MenubarMusicApp[1],            // TRACKER
        &MenubarMusicApp[4],            // LOOP
        &MenubarMusicApp[9],            // PLAY TRACK
        &MenubarMusicApp[10],           // STOP
};

static const MenuBarItem* const MenubarSfxPins[] =
    {
        EDIT_PINS};

#undef EDIT_PINS

// The NEW item grows a submenu when the build has more than one language.
enum
{
    MenubarNewMax = MAX_SUPPORTED_LANGS,
    MenubarNewIndex = 0, // NEW, RUN, RESUME, SAVE, EXPORT
};

static MenuBarItem MenubarNewItems[MenubarNewMax];
static char MenubarNewLabels[MenubarNewMax][16];
static MenuBarCategory MenubarNewMenu = {"NEW", MenubarNewItems, 0, false, 0, -1, 0, false};

static void initMenubarNew(void)
{
    s32 count = 0;

    FOREACH_LANG(script)
    {
        if(count < MenubarNewMax)
        {
            s32 j = 0;

            for(; script->name[j] && j < (s32)sizeof MenubarNewLabels[count] - 1; j++)
                MenubarNewLabels[count][j] = (char)toupper((u8)script->name[j]);

            MenubarNewLabels[count][j] = '\0';

            MenubarNewItems[count] = (MenuBarItem){
                .label = MenubarNewLabels[count],
                .accel = NULL,
                .handler = menubarNew,
                .id = count,
                .enabled = true,
                .separator = false,
                .icon = 0,
                .color = 0,
                .submenu = NULL,
                .checked = -1,
            };
        }

        count++;
    }

    s32 available = MIN(count, MenubarNewMax);
    MenubarNewMenu.count = available;

    MenuBarItem* item = &MenubarTic[MenubarNewIndex];

    if(available == 1)
    {
        item->handler = menubarNew;
        item->id = -1;
        item->submenu = NULL;
        item->enabled = true;
    }
    else if(available > 1)
    {
        item->handler = NULL;
        item->id = 0;
        item->submenu = &MenubarNewMenu;
        item->enabled = true;
    }
    else
        item->enabled = false;
}

static MenuBarCategory MenubarCategories[] =
    {
        // The first title is the current editor's, set by updateMenubar.
        {"", MenubarApps, 0, false, 0, -1, 0, false},
        {"EDIT", MenubarEdit, COUNT_OF(MenubarEdit), false, 0, -1, 0, false},
        // The overflow menu; the bank follows it, and TIC-80 stays last.
        {"MENU", NULL, 0, false, 0, -1, 0, false},
#if defined(TIC80_PRO)
        {"BANK", MenubarBanks, COUNT_OF(MenubarBanks), false, tic_icon_bank, -1, tic_color_red, false},
#endif
        {"TIC-80", MenubarTic, COUNT_OF(MenubarTic), true, 0, -1, 0, false},
};

MenuBarCategory* studiomenu_more(void)
{
#if defined(TIC80_PRO)
    return &MenubarCategories[COUNT_OF(MenubarCategories) - 3];
#else
    return &MenubarCategories[COUNT_OF(MenubarCategories) - 2];
#endif
}

static void menubarCheck(MenuBarItem* items, s32 count, s32 id, bool on)
{
    for(s32 i = 0; i < count; i++)
        if(items[i].id == id)
            items[i].checked = on ? 1 : 0;
}

static void menubarEnable(MenuBarItem* items, s32 count, s32 id, bool enabled)
{
    for(s32 i = 0; i < count; i++)
        if(items[i].id == id)
            items[i].enabled = enabled;
}

static void menubarAccel(MenuBarItem* items, s32 count, s32 id, const char* accel)
{
    for(s32 i = 0; i < count; i++)
        if(items[i].id == id)
            items[i].accel = accel;
}

void* studiomenu_init(Studio* studio, Menubar* bar, const EditorMode* modes, s32 modeCount, const EditorApp* apps)
{
    MenuState.studio = studio;
    MenuState.bar = bar;
    MenuState.modes = modes;
    MenuState.modeCount = modeCount;
    MenuState.apps = apps;

    initMenubarApps();
    initMenubarNew();

    return &MenuState;
}

MenuBarCategory* studiomenu_categories(s32* count)
{
    *count = COUNT_OF(MenubarCategories);

    return MenubarCategories;
}

void studiomenu_sync(Studio* studio, Menubar* bar, EditorMode mode, const EditorApp* app)
{
    static char title[16];

    shortName(app->name, title, sizeof title);

    // The compact bar shows the full app name beside the tabs; the title uses
    // the short one.
    bar->caption = app->name;

    MenubarCategories[0].title = title;
    MenubarCategories[0].icon = app->icon;
    MenubarCategories[0].items = MenubarApps;
    MenubarCategories[0].count = MenubarAppsCount;
    MenubarCategories[0].selected = (s32)mode;

    // RESUME only means something while a paused run is still in the core.
    menubarEnable(MenubarTic, COUNT_OF(MenubarTic), MenuActionResume, studio_run_paused(studio));

    // The home stop carries the TIC-80 mark in both layouts; compact draws the
    // icon alone, full keeps the title beside it.
    for(s32 i = 0; i < COUNT_OF(MenubarCategories); i++)
        if(MenubarCategories[i].items == MenubarTic)
            MenubarCategories[i].icon = tic_icon_tic80;

    // In compact mode EDIT's items are pinned in every editor, so its title
    // gives up the room.
    for(s32 i = 0; i < COUNT_OF(MenubarCategories); i++)
        if(MenubarCategories[i].items == MenubarEdit)
            MenubarCategories[i].hidden = bar->compact;

    Config* config = studio_config_get(studio);

#if defined(CRT_SHADER_SUPPORT)
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionCrt, config->data.options.crt);
#endif
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionFullscreen, tic_sys_fullscreen_get());
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionIntegerScale, config->data.options.integerScale);
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionAutohide, bar->autohide);
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionCompact, bar->compact);
#if defined(BUILD_SURF)
    menubarCheck(MenubarOptions, COUNT_OF(MenubarOptions), OptionAutosave, config->data.options.autosave);
#endif

    s32 tabSize = config->data.options.tabSize;

    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabSize1, tabSize == 1);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabSize2, tabSize == 2);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabSize4, tabSize == 4);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabSize8, tabSize == 8);

    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabModeAuto,
                 config->data.options.tabMode == TAB_AUTO);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabModeTabs,
                 config->data.options.tabMode == TAB_TAB);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorTabModeSpaces,
                 config->data.options.tabMode == TAB_SPACE);

    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorKeybindStandard,
                 config->data.options.keybindMode == KEYBIND_STANDARD);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorKeybindEmacs,
                 config->data.options.keybindMode == KEYBIND_EMACS);
    menubarCheck(MenubarEditor, COUNT_OF(MenubarEditor), EditorKeybindVi,
                 config->data.options.keybindMode == KEYBIND_VI);

#if defined(TIC80_PRO)
    menubarCheck(MenubarExport, COUNT_OF(MenubarExport), ExportActionAlone, ExportAlone);

    s32 bank = 0;
    bool chained = false;
    bool banked = false;
    static char bankTitle[16];

    studiomenu_bank_state(studio, &bank, &chained, &banked);

    // The compact bar has no room for the word: the bank icon and its number.
    snprintf(bankTitle, sizeof bankTitle, bar->compact ? "%i" : "BANK%i", bank);

    menubarCheck(MenubarBanks, COUNT_OF(MenubarBanks), BankActionChain, chained);

    for(s32 i = 0; i < COUNT_OF(MenubarCategories); i++)
        if(MenubarCategories[i].items == MenubarBanks)
        {
            MenubarCategories[i].selected = bank;
            MenubarCategories[i].title = bankTitle;
            MenubarCategories[i].hidden = !banked;
        }
#endif
}

void studiomenu_sync_editor(Studio* studio, Menubar* bar, EditorMode mode, void* instance)
{
    const MenuBarItem* items = NULL;
    s32 count = 0;

    bar->buttons = NULL;
    bar->buttonCount = 0;

    switch(mode)
    {
        case TIC_CODE_MODE:
        {
            Code* code = instance;
            bool vi = studio_config_get(studio)->data.options.keybindMode == KEYBIND_VI;

            // These two are VI-only: in the standard keybind ALT+F moves a
            // word and ALT+S does nothing.
            menubarAccel(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionFont, vi ? "ALT+F" : NULL);
            menubarAccel(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionShadow, vi ? "ALT+S" : NULL);

            menubarCheck(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionFont, code->altFont);
            menubarCheck(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionShadow, code->shadowText);
            menubarCheck(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionLineNumbers, code->lineNumbers);
            menubarCheck(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionSidebar, code->showGutter);
            menubarCheck(MenubarCodeApp, COUNT_OF(MenubarCodeApp), CodeActionStatusbar, code->showStatusbar);

            items = MenubarCodeApp;
            count = COUNT_OF(MenubarCodeApp);

            bar->buttons = MenubarCodePins;
            bar->buttonCount = COUNT_OF(MenubarCodePins);
            break;
        }

        case TIC_SPRITE_MODE:
        {
            Sprite* sprite = instance;

            menubarCheck(MenubarSpriteApp, COUNT_OF(MenubarSpriteApp), SpriteActionTiles, sprite->blit.bank == 0);
            menubarCheck(MenubarSpriteApp, COUNT_OF(MenubarSpriteApp), SpriteActionSprites, sprite->blit.bank == 1);
            menubarCheck(MenubarSpriteApp, COUNT_OF(MenubarSpriteApp), SpriteActionFlags, sprite->flags.show);

            menubarCheck(MenubarSpriteTools, COUNT_OF(MenubarSpriteTools), SpriteActionDraw, sprite->mode == SPRITE_DRAW_MODE);
            menubarCheck(MenubarSpriteTools, COUNT_OF(MenubarSpriteTools), SpriteActionPick, sprite->mode == SPRITE_PICK_MODE);
            menubarCheck(MenubarSpriteTools, COUNT_OF(MenubarSpriteTools), SpriteActionSelect, sprite->mode == SPRITE_SELECT_MODE);
            menubarCheck(MenubarSpriteTools, COUNT_OF(MenubarSpriteTools), SpriteActionFill, sprite->mode == SPRITE_FILL_MODE);

            menubarCheck(MenubarSpriteTileset, COUNT_OF(MenubarSpriteTileset), SpriteActionBpp4, sprite->blit.mode == tic_bpp_4);
            menubarCheck(MenubarSpriteTileset, COUNT_OF(MenubarSpriteTileset), SpriteActionBpp2, sprite->blit.mode == tic_bpp_2);
            menubarCheck(MenubarSpriteTileset, COUNT_OF(MenubarSpriteTileset), SpriteActionBpp1, sprite->blit.mode == tic_bpp_1);

            menubarCheck(MenubarPalette, COUNT_OF(MenubarPalette), SpriteActionVbank0, !sprite->palette.vbank1);
            menubarCheck(MenubarPalette, COUNT_OF(MenubarPalette), SpriteActionVbank1, sprite->palette.vbank1);
            menubarCheck(MenubarPalette, COUNT_OF(MenubarPalette), SpriteActionPalette, sprite->palette.edit);

            menubarCheck(MenubarCanvasSize, COUNT_OF(MenubarCanvasSize), SpriteActionSize8, sprite->size == 8);
            menubarCheck(MenubarCanvasSize, COUNT_OF(MenubarCanvasSize), SpriteActionSize16, sprite->size == 16);
            menubarCheck(MenubarCanvasSize, COUNT_OF(MenubarCanvasSize), SpriteActionSize32, sprite->size == 32);
            menubarCheck(MenubarCanvasSize, COUNT_OF(MenubarCanvasSize), SpriteActionSize64, sprite->size == 64);

            menubarCheck(MenubarBrushSize, COUNT_OF(MenubarBrushSize), SpriteActionBrush1, sprite->brushSize == 1);
            menubarCheck(MenubarBrushSize, COUNT_OF(MenubarBrushSize), SpriteActionBrush2, sprite->brushSize == 2);
            menubarCheck(MenubarBrushSize, COUNT_OF(MenubarBrushSize), SpriteActionBrush3, sprite->brushSize == 3);
            menubarCheck(MenubarBrushSize, COUNT_OF(MenubarBrushSize), SpriteActionBrush4, sprite->brushSize == 4);

            {
                static const s32 Pages[] = {SpriteActionPage1, SpriteActionPage2, SpriteActionPage3, SpriteActionPage4};

                for(s32 page = 0; page < (s32)COUNT_OF(Pages); page++)
                {
                    menubarEnable(MenubarSpriteTileset, COUNT_OF(MenubarSpriteTileset), Pages[page], page < sprite->blit.pages);
                    menubarCheck(MenubarSpriteTileset, COUNT_OF(MenubarSpriteTileset), Pages[page], page == sprite->blit.page);
                }
            }

            items = MenubarSpriteApp;
            count = COUNT_OF(MenubarSpriteApp);

            bar->buttons = MenubarSpritePins;
            bar->buttonCount = COUNT_OF(MenubarSpritePins);
            break;
        }

        case TIC_MAP_MODE:
        {
            Map* map = instance;

            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionGrid, map->canvas.grid);
            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionRandom, map->canvas.random);
            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionTileset, map->anim.pos.sheet >= 0);

            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionDraw, map->mode == MAP_DRAW_MODE);
            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionDrag, map->mode == MAP_DRAG_MODE);
            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionSelect, map->mode == MAP_SELECT_MODE);
            menubarCheck(MenubarMapApp, COUNT_OF(MenubarMapApp), MapActionFill, map->mode == MAP_FILL_MODE);

            menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), MapActionTiles, map->sheet.blit.bank == 0);
            menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), MapActionSprites, map->sheet.blit.bank == 1);
            menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), MapActionDepth4, map->sheet.blit.mode == tic_bpp_4);
            menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), MapActionDepth2, map->sheet.blit.mode == tic_bpp_2);
            menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), MapActionDepth1, map->sheet.blit.mode == tic_bpp_1);

            {
                static const s32 Pages[] = {MapActionPage1, MapActionPage2, MapActionPage3, MapActionPage4};

                for(s32 page = 0; page < (s32)COUNT_OF(Pages); page++)
                {
                    menubarEnable(MenubarMapTileset, COUNT_OF(MenubarMapTileset), Pages[page], page < map->sheet.blit.pages);
                    menubarCheck(MenubarMapTileset, COUNT_OF(MenubarMapTileset), Pages[page], page == map->sheet.blit.page);
                }
            }

            items = MenubarMapApp;
            count = COUNT_OF(MenubarMapApp);

            bar->buttons = MenubarMapPins;
            bar->buttonCount = COUNT_OF(MenubarMapPins);
            break;
        }

        case TIC_MUSIC_MODE:
        {
            Music* music = instance;

            menubarCheck(MenubarMusicApp, COUNT_OF(MenubarMusicApp), MusicActionPiano, music->tab == MUSIC_PIANO_TAB);
            menubarCheck(MenubarMusicApp, COUNT_OF(MenubarMusicApp), MusicActionTracker, music->tab == MUSIC_TRACKER_TAB);
            menubarCheck(MenubarMusicApp, COUNT_OF(MenubarMusicApp), MusicActionFollow, music->follow);
            menubarCheck(MenubarMusicApp, COUNT_OF(MenubarMusicApp), MusicActionLoop, music->loop);
            menubarCheck(MenubarMusicApp, COUNT_OF(MenubarMusicApp), MusicActionSustain, music->sustain);

            items = MenubarMusicApp;
            count = COUNT_OF(MenubarMusicApp);

            bar->buttons = MenubarMusicPins;
            bar->buttonCount = COUNT_OF(MenubarMusicPins);
            break;
        }

        case TIC_SFX_MODE:
            bar->buttons = MenubarSfxPins;
            bar->buttonCount = COUNT_OF(MenubarSfxPins);
            break;

        default:
            break;
    }

    // The first five pins of every list are EDIT's.
    bar->buttonSplit = MIN(5, bar->buttonCount);

    MenuBarCategory* menu = studiomenu_more();

    menu->title = "MENU";
    menu->icon = bar->compact ? tic_icon_more : 0;
    menu->items = items;
    menu->count = count;
    menu->selected = -1;
    menu->hidden = count == 0;
}

#undef MENU_ITEM
#undef MENU_ITEM_ICON
#undef MENU_ITEM_COLOR
#undef MENU_TOGGLE
#undef MENU_TOGGLE_ICON
#undef MENU_SUBMENU
#undef MENU_SEPARATOR
#undef MENU_CATEGORY
