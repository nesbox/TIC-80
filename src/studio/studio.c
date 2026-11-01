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

#include "studio.h"
#include "apps.h"
#include "mouse.h"
#include "boot.h"
#include "sound.h"
#include "keyboard.h"
#include "ui.h"

// The bar is drawn by the editors' screens alone, so its header comes with them.
#if defined(BUILD_EDITORS)
#include "menubar.h"
#include "studiomenu.h"

static void drawStudioMenubar(Studio* studio);
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif

#if defined(BUILD_EDITORS)
#include "editors/code/code.h"
#include "editors/sprite/sprite.h"
#include "editors/map/map.h"
#include "editors/map/world.h"
#include "editors/sfx/sfx.h"
#include "editors/music/music.h"
#include "wave_writer.h"
#define MSF_GIF_IMPL
#include "ext/msf_gif.h"

#include "../fftdata.h"
#include "ext/fft.h"

static MapView mapFrame(Studio* studio);
#endif

#if defined(BUILD_EDITORS)
#include "screens/console.h"
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
#include "net.h"
#endif

#if defined(BUILD_SURF)
#include "screens/surf.h"
#endif

#include "ext/md5.h"
#include "config.h"
#include "cart.h"
#include "screens/start.h"
#include "screens/run.h"
#include "modes.h"
#include "screens/menu.h"
#include "screens/mainmenu.h"

#include "fs.h"

#include "argparse.h"

// argparse's option macros stop at the help string and leave the tail of
// struct argparse_option to the compiler; the table below spells it out. -h
// is the header's own option, callback and all, so it keeps its full form.
#undef OPT_HELP
// clang-format off
#define OPT_HELP()       { ARGPARSE_OPT_BOOLEAN, 'h', "help", NULL,           \
                           "show this help message and exit",                 \
                           argparse_help_cb, 0, OPT_NONEG }
// clang-format on
#undef OPT_STRING
#undef OPT_INTEGER
#undef OPT_BOOLEAN
#define OPT_STRING(...)  {ARGPARSE_OPT_STRING, __VA_ARGS__, NULL, 0, 0}
#define OPT_INTEGER(...) {ARGPARSE_OPT_INTEGER, __VA_ARGS__, NULL, 0, 0}
#define OPT_BOOLEAN(...) {ARGPARSE_OPT_BOOLEAN, __VA_ARGS__, NULL, 0, 0}

#include <ctype.h>

#if defined(TIC_MODULE_EXT)
#include <dlfcn.h>
#endif

#define MD5_HASHSIZE 16

// interval between the Windows and Unix epoch
#define UNIX_EPOCH_IN_FILETIME 116444736000000000ULL

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)
#define TIC_EDITOR_BANKS (TIC_BANKS)
#else
#define TIC_EDITOR_BANKS 1
#endif

typedef struct
{
    u8 data[MD5_HASHSIZE];
} CartHash;

// The editors in tab order, expanded into the mode list and the registry
// entries, so the app switcher, the function keys and the number shortcuts agree.
// clang-format off
#define EDITOR_APPS(APP)                                                                                            \
    APP(TIC_CODE_MODE,   "CODE EDITOR",   tic_icon_code,   tic_key_f1, codeApp,   codeTick,   NULL,           &CodeClipboard,   false)   \
    APP(TIC_SPRITE_MODE, "SPRITE EDITOR", tic_icon_sprite, tic_key_f2, spriteApp, spriteTick, spriteScanline, &SpriteClipboard, true) \
    APP(TIC_MAP_MODE,    "MAP EDITOR",    tic_icon_map,    tic_key_f3, mapApp,    mapTick,    mapScanline,    &MapClipboard,    true)    \
    APP(TIC_SFX_MODE,    "SFX EDITOR",    tic_icon_sfx,    tic_key_f4, sfxApp,    sfxTick,    NULL,           &SfxClipboard,    false)    \
    APP(TIC_MUSIC_MODE,  "MUSIC EDITOR",  tic_icon_music,  tic_key_f5, musicApp,  musicTick,  NULL,           &MusicClipboard,  false)
// clang-format on

#if defined(BUILD_EDITORS)
static const EditorMode Modes[] =
    {
#define APP_MODE(MODE, ...) MODE,
        EDITOR_APPS(APP_MODE)
#undef APP_MODE
};

// Only the PRO build banks the editors, so only it needs the list.
#if defined(TIC80_PRO)
static const EditorMode BankModes[] =
    {
        TIC_SPRITE_MODE,
        TIC_MAP_MODE,
        TIC_SFX_MODE,
        TIC_MUSIC_MODE,
};
#endif

#endif

struct Studio
{
    tic_mem* tic;

    bool alive;

    EditorMode mode;
    // Where a modal screen returns: set when the menu opens, kept while it is
    // open (a confirm over it), used by its back with no handler.
    EditorMode returnTo;

    // The run the studio may sit over: whose it is (#2937), where it belongs
    // (#3015), and whether it is paused under the menu.
    RunRecord runState;

    struct
    {
        MouseState state[3];

        // Ticks since the mouse last moved or was pressed: the studio draws
        // its own cursor while the mouse is being used and stops drawing it
        // once it has been still for a while. The position itself is left
        // alone, so a click is a click whether the cursor is drawn or not.
        s32 idle;
        tic_point last;
#if defined(BUILD_RENDER_CACHE)
        struct
        {
            s32 x, y;
            tic_cursor sprite;
            bool system;
            bool visible;
        } prev;
#endif
    } mouse;

    // Written by the editors; the bar and the panels draw it. The buffer is not
    // the editors' to guard.
    struct
    {
        char text[STUDIO_TEXT_BUFFER_WIDTH];
    } tooltip;

#if defined(BUILD_EDITORS)
    ViMode viMode;

    // The code editor's frame, kept here so the editor's view pointer stays
    // valid between ticks: a config change or a console save reads it too.
    CodeView codeView;
    CodeBattle codeBattle;

    // The map and the world share one frame; the world reads it through its Map.
    MapView mapView;

    // The sfx editor's frame, refreshed every tick like the others.
    SfxView sfxView;

    // The sprite editor's frame; its scanline reads it, so it is filled at
    // init too.
    SpriteView spriteView;

    // The music editor's frame, refreshed every tick like the others.
    MusicView musicView;
#endif
    struct
    {
        CartHash hash;
        u64 mdate;
    } cart;
#if defined(BUILD_EDITORS)
    struct
    {
        bool chained;

        union
        {
            struct
            {
                s8 sprites;
                s8 map;
                s8 sfx;
                s8 music;
            } index;

#if defined(TIC80_PRO)
            s8 indexes[COUNT_OF(BankModes)];
#endif
        };
    } bank;

    struct
    {
        struct
        {
            s32 popup;
        } pos;

        Movie* movie;

        Movie idle;
        Movie show;
        Movie wait;
        Movie hide;
    } anim;

    struct
    {
        char message[STUDIO_TEXT_BUFFER_WIDTH];
    } popup;

    struct
    {
        bool record;
        bool screenshot;

        u32* buffer;
        s32 frame;

        MsfGifState gif;

    } video;

    Code* code;

    struct
    {
        Sprite* sprite[TIC_EDITOR_BANKS];
        Map* map[TIC_EDITOR_BANKS];
        Sfx* sfx[TIC_EDITOR_BANKS];
        Music* music[TIC_EDITOR_BANKS];
    } banks;

    World* world;
    Bytebattle bytebattle;
#endif

    // The loaded cart's identity, shared by the console and the browser.
    CartName rom;

    // The cart service's inputs, kept here so a queued load-by-hash outlives
    // the call that asked for it.
    RomDeps romDeps;

#if defined(BUILD_EDITORS)
    Console* console;
    // What the console reads of the host: the host refreshes it before every
    // tick it gives the console.
    ConsoleView consoleView;
#endif

#if defined(BUILD_SURF)
    Surf* surf;
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    tic_net* net;
#endif

    Start* start;
    Run* run;
    Menu* menu;

    // What the menu reads of the host, refreshed before its tick.
    MenuView menuView;

    Config* config;

    StudioMainMenu* mainmenu;

    tic_fs* fs;
    s32 samplerate;
    tic_font systemFont;

#if defined(BUILD_EDITORS)
    Menubar menubar;
#endif
    Boot boot;
};

// The screens' ticks take their own type, hence the adapters.
static void* startApp(Studio* studio)
{
    return studio->start;
}
static void* runApp(Studio* studio)
{
    return studio->run;
}
static void* menuApp(Studio* studio)
{
    return studio->menu;
}

static void startTick(void* app)
{
    start_tick(app);
}
static void runTick(void* app)
{
    Run* run = app;
    run->tick(run);
}
static void menuTick(void* app)
{
    menu_tick(app);
}
static void menuScanline(tic_mem* tic, s32 row, void* data)
{
    menu_anim_scanline(tic, row, data);
}

#if defined(BUILD_EDITORS)
static void* consoleApp(Studio* studio)
{
    return studio->console;
}
static void* codeApp(Studio* studio)
{
    return studio->code;
}
static void* worldApp(Studio* studio)
{
    return studio->world;
}
static void* spriteApp(Studio* studio)
{
    return studio->banks.sprite[studio->bank.index.sprites];
}
static void* mapApp(Studio* studio)
{
    return studio->banks.map[studio->bank.index.map];
}
static void* sfxApp(Studio* studio)
{
    return studio->banks.sfx[studio->bank.index.sfx];
}
static void* musicApp(Studio* studio)
{
    return studio->banks.music[studio->bank.index.music];
}

static void consoleTick(void* app)
{
    Console* console = app;
    console_tick(console);
}
static void codeTick(void* app)
{
    code_tick(app);
}
static void worldTick(void* app)
{
    world_tick(app);
}
static void spriteTick(void* app)
{
    sprite_tick(app);
}
static void mapTick(void* app)
{
    map_tick(app);
}
static void sfxTick(void* app)
{
    sfx_tick(app);
}
static void musicTick(void* app)
{
    music_tick(app);
}

static void spriteScanline(tic_mem* tic, s32 row, void* data)
{
    sprite_scanline(tic, row, data);
}
static void mapScanline(tic_mem* tic, s32 row, void* data)
{
    map_scanline(tic, row, data);
}
static void worldScanline(tic_mem* tic, s32 row, void* data)
{
    world_scanline(tic, row, data);
}
#endif

#if defined(BUILD_SURF)
static void* surfApp(Studio* studio)
{
    return studio->surf;
}

static void surfTick(void* app)
{
    Surf* surf = app;
    surf->tick(surf);
}
static void surfScanline(tic_mem* tic, s32 row, void* data)
{
    Surf* surf = data;
    surf->scanline(tic, row, surf);
}
#endif

// One entry per EditorMode; the editors are the entries with a name, in tab
// order. Every other list of editors follows from this table.
static void runEnter(Studio* studio, EditorMode from);
static void menuEnter(Studio* studio, EditorMode from);
#if defined(BUILD_EDITORS)
static void consoleEnter(Studio* studio, EditorMode from);
static void worldEnter(Studio* studio, EditorMode from);
#endif
#if defined(BUILD_SURF)
static void surfEnter(Studio* studio, EditorMode from);
#endif

static const EditorApp Apps[TIC_MODES_COUNT] =
    {
        [TIC_START_MODE] = {.instance = startApp, .tick = startTick},
        [TIC_RUN_MODE] = {.instance = runApp, .tick = runTick, .enter = runEnter},
        [TIC_MENU_MODE] = {.instance = menuApp, .tick = menuTick, .scanline = menuScanline, .enter = menuEnter},

#if defined(BUILD_EDITORS)
        [TIC_CONSOLE_MODE] = {.instance = consoleApp, .tick = consoleTick, .enter = consoleEnter},
        [TIC_WORLD_MODE] = {.instance = worldApp, .tick = worldTick, .scanline = worldScanline, .enter = worldEnter},

// The parameters are capitalised: lowercase ones would be substituted inside
// the `.name =` designators, which is not what an initialiser means.
// clang-format off
#define APP_ENTRY(MODE, NAME, ICON, KEY, INST, TICK, SCAN, CLIP, VB1)                   \
    [MODE] = {.name = NAME, .icon = ICON, .hotkey = KEY,                               \
              .instance = INST, .tick = TICK, .scanline = SCAN, .clipboard = CLIP,    \
              .vbank1 = VB1},
        // clang-format on
        EDITOR_APPS(APP_ENTRY)
#undef APP_ENTRY
#endif
#if defined(BUILD_SURF)
            [TIC_SURF_MODE] = {.instance = surfApp, .tick = surfTick, .scanline = surfScanline, .enter = surfEnter},
#endif
};

static void studioGoto(Studio* studio, EditorMode target, StudioGo intent);

#if defined(BUILD_EDITORS)
void* studiomenu_editor(Studio* studio, EditorMode mode)
{
    return Apps[mode].instance ? Apps[mode].instance(studio) : NULL;
}

s32 studiomenu_editor_bank(Studio* studio, EditorMode mode)
{
    switch(mode)
    {
        case TIC_SPRITE_MODE: return studio->bank.index.sprites;
        case TIC_MAP_MODE: return studio->bank.index.map;
        case TIC_MUSIC_MODE: return studio->bank.index.music;
        case TIC_SFX_MODE: return studio->bank.index.sfx;
        default: break;
    }

    return 0;
}

void studiomenu_command(Studio* studio, const char* command)
{
    console_exec(studio->console, command);
    studio->menubar.requested = TIC_CONSOLE_MODE;
}
#endif

#if defined(BUILD_EDITORS)
static void emptyDone(void* data)
{
    (void)data;
}
#endif

static inline void sfx2ram(tic_ram* ram, const tic_sfx* src)
{
    memcpy(&ram->sfx, src, sizeof ram->sfx);
}

static inline void music2ram(tic_ram* ram, const tic_music* src)
{
    memcpy(&ram->music, src, sizeof ram->music);
}

#if defined(BUILD_EDITORS)
static const tic_sfx* getSfxSrc(Studio* studio)
{
    tic_mem* tic = studio->tic;
    return &tic->cart.banks[studio->bank.index.sfx].sfx;
}

static const tic_music* getMusicSrc(Studio* studio)
{
    tic_mem* tic = studio->tic;
    return &tic->cart.banks[studio->bank.index.music].music;
}

const char* studioExportSfx(Studio* studio, s32 index, const char* filename)
{
    tic_mem* tic = studio->tic;

    const char* path = tic_fs_path(studio->fs, filename);

    if(wave_open(studio->samplerate, path))
    {

#if TIC80_SAMPLE_CHANNELS == 2
        wave_enable_stereo();
#endif

        const tic_sfx* sfx = getSfxSrc(studio);

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, getMusicSrc(studio));

        {
            const tic_sample* effect = &sfx->samples.data[index];

            enum
            {
                Channel = 0
            };
            sound_stop(tic, Channel);
            tic_api_sfx(tic, index, effect->note, effect->octave, -1, Channel, MAX_VOLUME, MAX_VOLUME, SFX_DEF_SPEED);

            for(s32 ticks = 0, pos = 0; pos < SFX_TICKS; pos = tic_tool_sfx_pos(effect->speed, ++ticks))
            {
                tic_core_tick_start(tic);
                tic_core_tick_end(tic);
                tic_core_synth_sound(tic);

                wave_write(tic->product.samples.buffer, tic->product.samples.count);
            }

            sound_stop(tic, Channel);
            memset(tic->ram->registers, 0, sizeof(tic_sound_register));
        }

        wave_close();

        return path;
    }

    return NULL;
}

const char* studioExportMusic(Studio* studio, s32 track, s32 bank, const char* filename)
{
    tic_mem* tic = studio->tic;

    const char* path = tic_fs_path(studio->fs, filename);

    if(wave_open(studio->samplerate, path))
    {
#if TIC80_SAMPLE_CHANNELS == 2
        wave_enable_stereo();
#endif
#if defined(TIC80_PRO) && defined(BUILD_EDITORS)
        // chained = true in CLI. Set to false if want to use unchained
        bool chained = studio->bank.chained;
        if(chained)
            memset(studio->bank.indexes, bank, sizeof studio->bank.indexes);
        else
            for(s32 i = 0; i < COUNT_OF(BankModes); i++)
                if(BankModes[i] == TIC_MUSIC_MODE)
                    studio->bank.indexes[i] = (s8)bank;
#endif
        const tic_sfx* sfx = getSfxSrc(studio);
        const tic_music* music = getMusicSrc(studio);

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, music);

        const tic_music_state* state = &tic->ram->music_state;
        const Music* editor = studio->banks.music[bank];

        tic_api_music(tic, track, -1, -1, false, editor->sustain, -1, -1);

        s32 frame = state->music.frame;
        s32 frames = MUSIC_FRAMES * 16;

        while(frames && state->flag.music_status == tic_music_play)
        {
            tic_core_tick_start(tic);

            for(s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
                if(!editor->on[i])
                    tic->ram->registers[i].volume = 0;

            tic_core_tick_end(tic);
            tic_core_synth_sound(tic);

            wave_write(tic->product.samples.buffer, tic->product.samples.count);

            if(frame != state->music.frame)
            {
                --frames;
                frame = state->music.frame;
            }
        }

        tic_api_music(tic, -1, -1, -1, false, false, -1, -1);

        wave_close();
        return path;
    }

    return NULL;
}
#endif

char getKeyboardText(Studio* studio)
{
#if defined(BUILD_EDITORS)
    // The bar owns the frame's keys: nothing typed reaches the editor. The
    // character stays unread, and the system layer drops it with the frame.
    if(studio->menubar.captured)
        return '\0';
#endif

    return keyboard_text(studio->tic);
}

bool keyWasPressed(Studio* studio, tic_key key)
{
#if defined(BUILD_EDITORS)
    // The menu closed on ESC this frame; the editor must not see the same key.
    if(key == tic_key_escape && studio->menubar.escaped)
        return false;
#endif

    return keyboard_pressed(studio->tic, key);
}

bool studio_keys_captured(Studio* studio)
{
#if defined(BUILD_EDITORS)
    return studio->menubar.captured;
#else
    (void)studio;
    return false;
#endif
}

bool studio_keys_escaped(Studio* studio)
{
#if defined(BUILD_EDITORS)
    return studio->menubar.escaped;
#else
    (void)studio;
    return false;
#endif
}

bool keyWasPressedOnce(Studio* studio, tic_key key)
{
#if defined(BUILD_EDITORS)
    // The menu closed on ESC this frame; the editor must not see the same key.
    if(key == tic_key_escape && studio->menubar.escaped)
        return false;
#endif

    return keyboard_pressed_once(studio->tic, key);
}

bool enterWasPressed(Studio* studio)
{
    return ticEnterWasPressed(studio->tic, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool ticEnterWasPressed(tic_mem* tic, s32 hold, s32 period)
{
    return keyboard_enter(tic, hold, period);
}

#if defined(BUILD_EDITORS)
tic_tiles* getBankTiles(Studio* studio)
{
    return &studio->tic->cart.banks[studio->bank.index.sprites].tiles;
}

tic_map* getBankMap(Studio* studio)
{
    return &studio->tic->cart.banks[studio->bank.index.map].map;
}

tic_palette* getBankPalette(Studio* studio, bool vbank)
{
    tic_bank* bank = &studio->tic->cart.banks[studio->bank.index.sprites];
    return vbank ? &bank->palette.vbank1 : &bank->palette.vbank0;
}

tic_flags* getBankFlags(Studio* studio)
{
    return &studio->tic->cart.banks[studio->bank.index.sprites].flags;
}
#endif

void playSystemSfx(Studio* studio, s32 id)
{
    sound_play(studio->tic, &getConfig(studio)->cart->bank0.sfx, id);
}

static void md5(const void* voidData, s32 length, u8 digest[MD5_HASHSIZE])
{
    enum
    {
        Size = 512
    };

    const u8* data = voidData;

    MD5_CTX c;
    MD5_Init(&c);

    while(length > 0)
    {
        MD5_Update(&c, data, length > Size ? Size : length);

        length -= Size;
        data += Size;
    }

    MD5_Final(digest, &c);
}

#if defined(BUILD_EDITORS)
void showTooltip(Studio* studio, const char* text)
{
    snprintf(studio->tooltip.text, sizeof studio->tooltip.text, "%.*s",
             (s32)sizeof studio->tooltip.text - 1, text);
}

#endif

// The config's own accessors. They sit outside the editors guard on purpose:
// main.c, rom.c and mainmenu.c read the config in a build with no editors, and
// a browser-only build linked nowhere with them inside it.
const StudioConfig* studio_config(const Studio* studio)
{
    return &studio->config->data;
}

const StudioConfig* getConfig(Studio* studio)
{
    return studio_config(studio);
}

Config* studio_config_get(Studio* studio)
{
    return studio->config;
}

// The studio is opaque outside studio.c, so its boot state is handed out the
// way the config and the start screen are.
Boot* getBoot(Studio* studio)
{
    return &studio->boot;
}

#if defined(BUILD_EDITORS)

// The popup unrolls by its own height; it was never STUDIO_BAR_HEIGHT, the two
// just happen to be 7.
#define POPUP_HEIGHT (TIC_FONT_HEIGHT + 1)

static void drawPopup(Studio* studio)
{
    if(studio->anim.movie != &studio->anim.idle)
    {
        enum
        {
            Width = TIC80_WIDTH,
            Height = POPUP_HEIGHT
        };

        tic_api_rect(studio->tic, 0, studio->anim.pos.popup, Width, Height, tic_color_red);
        tic_api_print(studio->tic, studio->popup.message,
                      (s32)(Width - strlen(studio->popup.message) * TIC_FONT_WIDTH) / 2,
                      studio->anim.pos.popup + 1, tic_color_white, true, 1, false);

        // render popup message
        {
            tic_mem* tic = studio->tic;
            const tic_bank* bank = &getConfig(studio)->cart->bank0;
            u32* dst = tic->product.screen + TIC80_MARGIN_LEFT + TIC80_MARGIN_TOP * TIC80_FULLWIDTH;

            for(s32 i = 0, y = 0; y < (Height + studio->anim.pos.popup); y++, dst += TIC80_MARGIN_RIGHT + TIC80_MARGIN_LEFT)
                for(s32 x = 0; x < Width; x++)
                    *dst++ = tic_rgba(&bank->palette.vbank0.colors[tic_tool_peek4(tic->ram->vram.screen.data, i++)]);
        }
    }
}

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)
// The pro bank row belongs to the editors that have banks, which is not the
// same set as Modes[]: the code editor is not one of them.
static bool isBanked(EditorMode mode)
{
    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == mode)
            return true;

    return false;
}
#endif

ClipboardEvent getClipboardEvent(Studio* studio)
{
    return keyboard_clipboard(studio->tic);
}

static void showPopupMessage(Studio* studio, const char* text)
{
    memset(studio->popup.message, '\0', sizeof studio->popup.message);
    snprintf(studio->popup.message, sizeof studio->popup.message, "%.*s",
             (s32)sizeof studio->popup.message - 1, text);

    for(char* c = studio->popup.message; c < studio->popup.message + sizeof studio->popup.message; c++)
        if(*c) *c = (char)(toupper((u8)(*c)));

    studio->anim.movie = resetMovie(&studio->anim.show);
}
#endif

static void exitConfirm(Studio* studio, bool yes, void* data)
{
    (void)data;
    studio->alive = yes;
}

void studio_exit(Studio* studio)
{
#if defined(BUILD_EDITORS)
    if(studio->mode != TIC_START_MODE && studioCartChanged(studio))
    {
        static const char* Rows[] =
            {
                "WARNING!",
                "You have unsaved changes",
                "Do you really want to exit?",
            };

        confirmDialog(studio, Rows, COUNT_OF(Rows), exitConfirm, NULL);
    }
    else
#endif
        exitConfirm(studio, true, NULL);
}

void exitStudio(Studio* studio)
{
    studio_exit(studio);
}

void drawBitIcon(Studio* studio, s32 id, s32 x, s32 y, u8 color)
{
    ui_icon(studio->tic, &getConfig(studio)->cart->bank0.tiles, id, x, y, color);
}

// The runner's host side: the console it reports to, the exit and the mode it
// asks about, as one-line wrappers.
#if defined(BUILD_EDITORS)
static void runEnded(void* host)
{
    studioGoto(host, TIC_CONSOLE_MODE, StudioGoScreen);
}

static void runError(void* host, const char* info)
{
    console_error(((Studio*)host)->console, info);
}

static void runTrace(void* host, const char* text, u8 color)
{
    console_trace(((Studio*)host)->console, text, color);
}
#else
static void runExited(void* host)
{
    studio_exit(host);
}
#endif

static bool runIsActive(void* host)
{
    return getStudioMode(host) == TIC_RUN_MODE;
}

static void initRunMode(Studio* studio)
{
    studio->runState.paused = false;

    RunDeps deps = {
        .host = studio,
        .tic = studio->tic,
        .fs = studio->fs,
        .isActive = runIsActive,
#if defined(BUILD_EDITORS)
        .ended = runEnded,
        .error = runError,
        .trace = runTrace,
#else
        .exited = runExited,
#endif
    };

    run_init(studio->run, &deps);
}

#if defined(BUILD_EDITORS)
static void initWorldMap(Studio* studio)
{
    initWorld(studio->world, studio->banks.map[studio->bank.index.map]);
}

void gotoCode(Studio* studio)
{
    studioGoto(studio, TIC_CODE_MODE, StudioGoScreen);
}
#endif

#if defined(BUILD_SURF)
// The browser's host side: the cart loads and the exits it asks for, as
// one-line wrappers, so the screen keeps its decisions.
static void surfLoadCart(void* host, const char* name)
{
    studioLoadCart(host, name);
}

static void surfLoadByHash(void* host, const char* name, const char* hash, const char* section, fs_done_callback done, void* data)
{
    studioLoadByHash(host, name, hash, section, done, data);
}

static void surfAutoSave(void* host)
{
    studioAutoSave(host);
}

static bool surfCartChanged(void* host)
{
    return studioCartChanged(host);
}

static void surfRun(void* host)
{
    runGame(host, RUN_FROM_PLAYER);
}

static void surfExit(void* host)
{
    exitSurf(host);
}

static bool surfIsActive(void* host)
{
    return getStudioMode(host) == TIC_SURF_MODE;
}

// The confirm's answer is the screen's own callback; the dialog holds it until
// one of its items is picked.
typedef struct
{
    SurfConfirm callback;
    void* data;
} SurfConfirmData;

static void surfConfirmAnswered(Studio* studio, bool yes, void* data)
{
    SurfConfirmData* confirm = data;

    confirm->callback(studio, yes, confirm->data);
    free(confirm);
}

static void surfConfirmLoadCart(void* host, SurfConfirm callback, void* data)
{
    SurfConfirmData* confirm = malloc(sizeof(SurfConfirmData));
    *confirm = (SurfConfirmData){callback, data};

    confirmLoadCart(host, surfConfirmAnswered, confirm);
}

// What the browser gets once: its filesystem, its net and config, and the
// actions.
static SurfDeps surfDeps(Studio* studio)
{
    return (SurfDeps){
        .host = studio,
        .tic = studio->tic,
        .fs = studio->fs,
        .net = studio->net,
        .config = studio->config,

        .loadCart = surfLoadCart,
        .loadByHash = surfLoadByHash,
        .autoSave = surfAutoSave,
        .cartChanged = surfCartChanged,
        .confirmLoadCart = surfConfirmLoadCart,
        .run = surfRun,
        .exit = surfExit,
        .isActive = surfIsActive,
    };
}

static void initSurfMode(Studio* studio)
{
    SurfDeps deps = surfDeps(studio);

    surf_init(studio->surf, &deps);
}

void gotoSurf(Studio* studio)
{
#if defined(BUILD_EDITORS)
    // The console hands the screen over: end its command, or its active flag
    // stays false and the shortcuts it gates never come back.
    console_done(studio->console);
#endif
    initSurfMode(studio);
    studioGoto(studio, TIC_SURF_MODE, StudioGoScreen);
}

// Out of the browser: home, which is the console in a build that has one. A
// build that is only the browser has nowhere to go and stays where it is.
void exitSurf(Studio* studio)
{
#if defined(BUILD_EDITORS)
    studioGoto(studio, TIC_HOME_MODE, StudioGoScreen);
#else
    (void)studio;
#endif
}
#endif

bool studio_is_cart_loaded(Studio* studio)
{
    return strlen(studio->rom.name) > 0 || studio->boot.cart;
}

s32 getStudioEditorTop(Studio* studio)
{
#if defined(BUILD_EDITORS)
    // With autohide the editors fill the screen and the bar overlays them.
    return studio->menubar.autohide ? 0 : STUDIO_BAR_HEIGHT;
#else
    (void)studio;
    return STUDIO_BAR_HEIGHT;
#endif
}

// The pause/options screen's host side: the run and mode actions it asks for,
// as one-line wrappers, so the screen keeps the decisions.
static bool mainMenuRunPaused(void* host)
{
    return studio_run_paused(host);
}

static void mainMenuResume(void* host)
{
    resumeGame(host);
}

static void mainMenuRun(void* host)
{
    runGame(host, RUN_FROM_STUDIO);
}

static void mainMenuLeave(void* host)
{
    leaveRun(host);
}

static void mainMenuExitStudio(void* host)
{
    exitStudio(host);
}

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
static void mainMenuExitGame(void* host)
{
    exitGame(host);
}
#endif

static void mainMenuSetRunMode(void* host)
{
    studioGoto(host, TIC_RUN_MODE, StudioGoScreen);
}

static bool mainMenuOverPlayerRun(void* host)
{
    return studio_menu_over_player_run(host);
}

static bool mainMenuIsCartLoaded(void* host)
{
    return studio_is_cart_loaded(host);
}

static void mainMenuSaveOptions(void* host)
{
    Config* config = studio_config_get(host);
    config->saveOptions(config);
}

// What the screen gets once: its menu widget, the options and the actions.
static MainMenuDeps mainMenuDeps(Studio* studio)
{
    return (MainMenuDeps){
        .host = studio,
        .tic = studio->tic,
        .menu = studio->menu,
        .options = &studio->config->data.options,
        .saveOptions = mainMenuSaveOptions,

        .runPaused = mainMenuRunPaused,
        .resume = mainMenuResume,
        .run = mainMenuRun,
        .leave = mainMenuLeave,
        .exitStudio = mainMenuExitStudio,
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        .exitGame = mainMenuExitGame,
#endif
        .setRunMode = mainMenuSetRunMode,
        .menuOverPlayerRun = mainMenuOverPlayerRun,
        .isCartLoaded = mainMenuIsCartLoaded,
    };
}

// The pause menu is rebuilt wherever it is opened: a fresh screen over the
// host it acts on.
static void resetMainMenu(Studio* studio)
{
    MainMenuDeps deps = mainMenuDeps(studio);

    mainmenu_free(studio->mainmenu);
    studio->mainmenu = mainmenu_init(&deps);
}

#if defined(BUILD_EDITORS)
static MapView mapFrame(Studio* studio);
static bool showGameMenu(Studio* studio);
#endif

// The modes' entries: what each sets up when the studio enters it. They live
// with the app table, so the machine itself carries no build guards.

static void runEnter(Studio* studio, EditorMode from)
{
    (void)from;
    initRunMode(studio);
}

static void menuEnter(Studio* studio, EditorMode from)
{
    (void)from;
    resetMainMenu(studio);
}

#if defined(BUILD_EDITORS)
static void consoleEnter(Studio* studio, EditorMode from)
{
#if defined(BUILD_SURF)
    // The console hands the browser over: end its command, or its active flag
    // stays false and the shortcuts it gates never come back.
    if(from == TIC_SURF_MODE)
        console_done(studio->console);
    else
        (void)studio;
#else
    (void)studio;
    (void)from;
#endif
}

static void worldEnter(Studio* studio, EditorMode from)
{
    (void)from;

    studio->mapView = mapFrame(studio);
    map_set_view(studio->banks.map[studio->bank.index.map], &studio->mapView);
    initWorldMap(studio);
    worldRefresh(studio->world);
}
#endif

#if defined(BUILD_SURF)
static void surfEnter(Studio* studio, EditorMode from)
{
    (void)from;
    studio->surf->resume(studio->surf);
}
#endif

// The machine's view of the studio, and the studio's of the machine.
static ModeState modeState(const Studio* studio)
{
    return (ModeState){
        .mode = studio->mode,
        .returnTo = studio->returnTo,
        .run = studio->runState,
    };
}

// The door's effects: the machine first, as the old switch did, then the
// mode's own entry, which may touch what the transition set.
static void studioApply(Studio* studio, ModeState* state, const ModeAction* action, StudioGo intent)
{
    EditorMode from = studio->mode;

    if(action->pauseCore)
        tic_core_pause(studio->tic);

    if(action->resumeCore)
        tic_core_resume(studio->tic);

    if(action->resetCore)
        tic_api_reset(studio->tic);

    if(action->clearExit)
        studio->run->exit = false;

    EditorMode target = action->target;

    // A mode this build has not got falls back to the menu or the run.
    if(target != from && Apps[target].instance == NULL)
        target = studio_is_cart_loaded(studio) ? TIC_RUN_MODE : TIC_MENU_MODE;

    studio->returnTo = state->returnTo;
    studio->runState = state->run;

    if(action->enter && Apps[target].enter)
        Apps[target].enter(studio, from);

    if(target != from)
        studio->mode = target;

    studio->menuView.active = studio->mode == TIC_MENU_MODE;

#if !defined(BUILD_EDITORS)
    (void)intent;
#else
    if(intent != StudioGoResume)
    {
        // The music editor's tab toggles when its own mode is asked for.
        if(intent == StudioGoScreen && target == from && target == TIC_MUSIC_MODE)
        {
            Music* music = studio->banks.music[studio->bank.index.music];
            music->tab = (music->tab + 1) % MUSIC_TAB_COUNT;

            studio->config->data.options.musicTracker = music->tab == MUSIC_TRACKER_TAB;
            studio->config->saveOptions(studio->config);
        }

        studio->viMode = 0;
    }
#endif
}

// The door: the only writer of the studio's mode. Every driver ends here.
static void studioGoto(Studio* studio, EditorMode target, StudioGo intent)
{
    ModeState state = modeState(studio);
    ModeAction action = modesGo(&state, target, intent);

    studioApply(studio, &state, &action, intent);
}

// The run's start and restart: runGame's other half.
static void studioRunGame(Studio* studio, RunOrigin origin)
{
    ModeState state = modeState(studio);
    ModeAction action = modesRun(&state, origin);

    studioApply(studio, &state, &action, StudioGoScreen);
}

// The ESC key: the screen first — the menu's back, the code editor's own
// escape — then where the machine sends it.
static void studioEscape(Studio* studio)
{
    if(studio->mode == TIC_MENU_MODE && menu_back(studio->menu))
        return;

#if defined(BUILD_EDITORS)
    if(studio->mode == TIC_CODE_MODE && studio->code->mode != TEXT_EDIT_MODE)
    {
        studio->code->escape(studio->code);
        return;
    }
#endif

    ModeState state = modeState(studio);

#if defined(BUILD_EDITORS)
    EscapeAction escape = modesEscape(&state, showGameMenu(studio), false);
#else
    EscapeAction escape = modesEscape(&state, false, true);
#endif

    if(escape.go)
        studioGoto(studio, escape.target, escape.intent);
}

EditorMode getStudioMode(Studio* studio)
{
    return studio->mode;
}

#if defined(BUILD_EDITORS)
static void changeStudioMode(Studio* studio, s32 dir)
{
    for(size_t i = 0; i < COUNT_OF(Modes); i++)
    {
        if(studio->mode == Modes[i])
        {
            studioGoto(studio, Modes[(i + dir + COUNT_OF(Modes)) % COUNT_OF(Modes)], StudioGoScreen);
            return;
        }
    }
}
#endif

bool studio_run_paused(Studio* studio)
{
    return studio->runState.paused;
}

void resumeGame(Studio* studio)
{
    studioGoto(studio, TIC_RUN_MODE, StudioGoResume);
}

#if defined(BUILD_EDITORS)
void setStudioViMode(Studio* studio, ViMode mode)
{
    studio->viMode = mode;
}

ViMode getStudioViMode(Studio* studio)
{
    return studio->viMode;
}

bool checkStudioViMode(Studio* studio, ViMode mode)
{
    return (
        getConfig(studio)->options.keybindMode == KEYBIND_VI && getStudioViMode(studio) == mode);
}
#endif

bool checkMousePos(Studio* studio, const tic_rect* rect)
{
#if defined(BUILD_EDITORS)
    // The menu owns the pointer: the editor must not draw hover under it.
    if(menubar_owns_pointer(&studio->menubar, studio->tic))
        return false;
#endif

    return mouse_over(studio->tic, rect);
}

bool checkMouseClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_click(&studio->mouse.state[button], rect);
}

bool checkMouseDblClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_dblclick(&studio->mouse.state[button], rect);
}

bool checkMouseDown(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    return mouse_down(&studio->mouse.state[button], rect);
}

void setCursor(Studio* studio, tic_cursor id)
{
    ui_cursor(studio->tic, id);
}

typedef struct
{
    Studio* studio;
    ConfirmCallback callback;
    void* data;
} ConfirmData;

static void confirmHandler(bool yes, void* data)
{
    ConfirmData* confirmData = data;
    SCOPE(free(confirmData))
    {
        Studio* studio = confirmData->studio;

        if(studio->returnTo == TIC_RUN_MODE)
            resumeGame(studio);
        else
            studioGoto(studio, studio->returnTo, StudioGoScreen);

        confirmData->callback(studio, yes, confirmData->data);
    }
}

static void confirmNo(void* data, s32 pos)
{
    (void)pos;
    confirmHandler(false, data);
}

static void confirmYes(void* data, s32 pos)
{
    (void)pos;
    confirmHandler(true, data);
}

void confirmDialog(Studio* studio, const char** text, s32 rows, ConfirmCallback callback, void* data)
{
    studioGoto(studio, TIC_MENU_MODE, StudioGoConfirm);

    static MenuItem Answers[] =
        {
            {.label = ""},
            {.label = "(N)O", .handler = confirmNo},
            {.label = "(Y)ES", .handler = confirmYes},
        };

    Answers[1].hotkey = tic_key_n;
    Answers[2].hotkey = tic_key_y;

    s32 count = rows + COUNT_OF(Answers);
    MenuItem* items = malloc(sizeof items[0] * count);
    SCOPE(free(items))
    {
        for(s32 i = 0; i != rows; ++i)
            items[i] = (MenuItem){.label = text[i]};

        memcpy(items + rows, Answers, sizeof Answers);

        menu_init(studio->menu, items, count, count - 2, 0,
                  NULL, MOVE((ConfirmData){studio, callback, data}));

        playSystemSfx(studio, 0);
    }
}

#if defined(BUILD_EDITORS)
static void resetBanks(Studio* studio)
{
    memset(&studio->bank.index, 0, sizeof studio->bank.index);
}
#endif

// What the menu reads this frame: whether it is the studio's mode, and whether
// the bar owns the pointer.
static MenuView menuFrame(Studio* studio)
{
    return (MenuView){
        .active = studio->mode == TIC_MENU_MODE,
#if defined(BUILD_EDITORS)
        .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
#endif
    };
}

#if defined(BUILD_EDITORS)
static Bytebattle* getBytebattle(Studio* studio);

static ViMode codeGetViMode(void* host)
{
    return getStudioViMode(host);
}

static void codeSetViMode(void* host, ViMode mode)
{
    setStudioViMode(host, mode);
}

static void codeRunGame(void* host)
{
    runGame(host, RUN_FROM_STUDIO);
}

static void codeSaveProject(void* host)
{
    saveProject(host);
}

static void codeTooltip(void* host, const char* text)
{
    showTooltip(host, text);
}

static void codeViewToggle(void* host, CodeViewToggle option)
{
    static const CodeViewOption Options[] =
        {
            CodeViewFont,
            CodeViewShadow,
            CodeViewLineNumbers,
            CodeViewSidebar,
            CodeViewStatusbar,
        };

    studioCodeViewToggle(host, Options[option]);
}

// What the editor reads this frame: the bar's flags and the values the menu
// may change while the editor is open.
static CodeView codeFrame(Studio* studio, CodeBattle* battle)
{
    const StudioConfig* config = getConfig(studio);
    const struct StudioOptions* options = &config->options;
    const tic_script* script = tic_get_script(studio->tic);
    const Bytebattle* bb = getBytebattle(studio);

    CodeView view =
        {
            .frame =
                {
                    .captured = studio_keys_captured(studio),
                    .escaped = studio_keys_escaped(studio),
                    .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
                    .clipboard = getClipboardEvent(studio),
                },
            .options =
                {
                    .script = script,
                    .tabSize = options->tabSize,
                    .spacesForTab = options->tabMode == TAB_SPACE || (options->tabMode == TAB_AUTO && (script->id == 20 || script->id == 13)),
                    .structuredEdit = script->useStructuredEdition && options->keybindMode == KEYBIND_EMACS,
                    .autoDelimiters = config->theme.code.autoDelimiters,
                    .matchDelimiters = config->theme.code.matchDelimiters,
                    .vi = options->keybindMode == KEYBIND_VI,
                    .emacs = options->keybindMode == KEYBIND_EMACS,
                    .altFont = options->codeAltFont,
                    .shadowText = options->codeShadow,
                    .lineNumbers = options->codeLineNumbers,
                    .sidebar = options->codeSidebar,
                    .statusbar = options->codeStatusbar,
                },
            .editorTop = getStudioEditorTop(studio),
            .battle = NULL,
        };

#define CODE_COLOR_DEF(VAR) view.theme.VAR = config->theme.code.VAR;
    CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef CODE_COLOR_DEF

    view.theme.select = config->theme.code.select;
    view.theme.cursor = config->theme.code.cursor;
    view.theme.altCaret = config->theme.code.altCaret;

    if(bb)
    {
        *battle = (CodeBattle){
            .limit = {.lower = bb->limit.lower, .current = bb->limit.current},
            .battle = {.hidetime = bb->battle.hidetime, .left = bb->battle.left},
        };
        view.battle = battle;
    }

    return view;
}

static void mapOpenWorld(void* host)
{
    studioGoto(host, TIC_WORLD_MODE, StudioGoScreen);
}

static void mapOpenMap(void* host)
{
    studioGoto(host, TIC_MAP_MODE, StudioGoScreen);
}

// What the map and the world read this frame: the bar's flags, the editor top,
// and the bank data the sheet, the palette and the world preview draw from.
static MapView mapFrame(Studio* studio)
{
    const StudioConfig* config = getConfig(studio);

    return (MapView){
        .frame =
            {
                .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
                .clipboard = getClipboardEvent(studio),
            },
        .editorTop = getStudioEditorTop(studio),
        .tiles = getBankTiles(studio),
        .palette = getBankPalette(studio, false),
        .cartPalette = &config->cart->bank0.palette.vbank0,
    };
}

static void sfxTooltip(void* host, const char* text)
{
    showTooltip(host, text);
}

// What the sfx editor draws with this frame: the bar's pointer flag, the
// clipboard gesture and the cart's bank0 tiles the icons come from.
static SfxView sfxFrame(Studio* studio)
{
    return (SfxView){
        .frame =
            {
                .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
                .clipboard = getClipboardEvent(studio),
            },
        .cartTiles = &getConfig(studio)->cart->bank0.tiles,
    };
}

static void spriteTooltip(void* host, const char* text)
{
    showTooltip(host, text);
}

// What the sprite editor draws with this frame: the bar's flags, the cart's
// bank0 tiles and palette, and the current sprite bank's palettes and flags.
static SpriteView spriteFrame(Studio* studio)
{
    const StudioConfig* config = getConfig(studio);

    return (SpriteView){
        .frame =
            {
                .captured = studio_keys_captured(studio),
                .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
                .clipboard = getClipboardEvent(studio),
            },
        .cartTiles = &config->cart->bank0.tiles,
        .cartPalette = &config->cart->bank0.palette.vbank0,
        .vbank0 = getBankPalette(studio, false),
        .vbank1 = getBankPalette(studio, true),
        .flags = getBankFlags(studio),
    };
}

static void musicTooltip(void* host, const char* text)
{
    showTooltip(host, text);
}

// What the music editor draws with this frame: the bar's flags and the cart's
// bank0 tiles the icons come from.
static MusicView musicFrame(Studio* studio)
{
    return (MusicView){
        .frame =
            {
                .captured = studio_keys_captured(studio),
                .pointerOwned = menubar_owns_pointer(&studio->menubar, studio->tic),
                .clipboard = getClipboardEvent(studio),
            },
        .cartTiles = &getConfig(studio)->cart->bank0.tiles,
    };
}

// The active editor's per-tick view. The bar's bank switch runs outside the
// tick, and the next blit's scanline reads this view, so it is rebuilt there
// too.
static void refreshEditorView(Studio* studio)
{
    switch(studio->mode)
    {
        case TIC_MAP_MODE:
        case TIC_WORLD_MODE:
            studio->mapView = mapFrame(studio);
            map_set_view(studio->banks.map[studio->bank.index.map], &studio->mapView);
            break;
        case TIC_SFX_MODE:
            studio->sfxView = sfxFrame(studio);
            sfx_set_view(studio->banks.sfx[studio->bank.index.sfx], &studio->sfxView);
            break;
        case TIC_SPRITE_MODE:
            studio->spriteView = spriteFrame(studio);
            sprite_set_view(studio->banks.sprite[studio->bank.index.sprites], &studio->spriteView);
            break;
        case TIC_MUSIC_MODE:
            studio->musicView = musicFrame(studio);
            music_set_view(studio->banks.music[studio->bank.index.music], &studio->musicView);
            break;
        default:
            break;
    }
}
#endif

static void initModules(Studio* studio)
{
#if defined(BUILD_EDITORS)
    tic_mem* tic = studio->tic;

    resetBanks(studio);

    CodeDeps deps =
        {
            .host = studio,
            .tic = tic,
            .mouse = studio->mouse.state,
            .tiles = &getConfig(studio)->cart->bank0.tiles,
            .getViMode = codeGetViMode,
            .setViMode = codeSetViMode,
            .runGame = codeRunGame,
            .saveProject = codeSaveProject,
            .viewToggle = codeViewToggle,
            .tooltip = codeTooltip,
        };

    initCode(studio->code, &deps);

    studio->codeView = (CodeView){0};
    code_set_view(studio->code, &studio->codeView);

    MapDeps mapDeps =
        {
            .host = studio,
            .tic = tic,
            .mouse = studio->mouse.state,
            .openWorld = mapOpenWorld,
            .openMap = mapOpenMap,
        };

    studio->mapView = (MapView){0};

    SfxDeps sfxDeps =
        {
            .host = studio,
            .tic = tic,
            .mouse = studio->mouse.state,
            .tooltip = sfxTooltip,
        };

    studio->sfxView = (SfxView){0};

    SpriteDeps spriteDeps =
        {
            .host = studio,
            .tic = tic,
            .mouse = studio->mouse.state,
            .tooltip = spriteTooltip,
        };

    studio->spriteView = (SpriteView){0};

    MusicDeps musicDeps =
        {
            .host = studio,
            .tic = tic,
            .mouse = studio->mouse.state,
            .tracker = getConfig(studio)->options.musicTracker,
            .tooltip = musicTooltip,
        };

    studio->musicView = (MusicView){0};

    for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
    {
        initSprite(studio->banks.sprite[i], &spriteDeps, &tic->cart.banks[i].tiles);
        sprite_set_view(studio->banks.sprite[i], &studio->spriteView);
        initMap(studio->banks.map[i], &mapDeps, &tic->cart.banks[i].map);
        map_set_view(studio->banks.map[i], &studio->mapView);
        initSfx(studio->banks.sfx[i], &sfxDeps, &tic->cart.banks[i].sfx);
        sfx_set_view(studio->banks.sfx[i], &studio->sfxView);
        initMusic(studio->banks.music[i], &musicDeps, &tic->cart.banks[i].music);
        music_set_view(studio->banks.music[i], &studio->musicView);
    }

    // A confirmed reload calls this mid-frame: fill the views before the
    // renderer can reach their scanlines.
    studio->mapView = mapFrame(studio);
    studio->spriteView = spriteFrame(studio);

    initWorldMap(studio);
    worldRefresh(studio->world);
#else
    (void)studio;
#endif
}

static void updateHash(Studio* studio)
{
    md5(&studio->tic->cart, sizeof(tic_cartridge), studio->cart.hash.data);
}

static void updateMDate(Studio* studio)
{
    studio->cart.mdate = fs_date(studio->rom.path);
}

static void updateTitle(Studio* studio)
{
    char name[TICNAME_MAX + sizeof TIC_TITLE + 3] = TIC_TITLE;

    if(strlen(studio->rom.name))
        snprintf(name, sizeof name, "%s [%s]", TIC_TITLE, studio->rom.name);

    tic_sys_title(name);
}

void studioRomSaved(Studio* studio)
{
    updateTitle(studio);
    updateHash(studio);
    updateMDate(studio);
}

void studioRomLoaded(Studio* studio)
{
    studio->runState.paused = false;

    initModules(studio);

    updateTitle(studio);
    updateHash(studio);
    updateMDate(studio);
}

bool studioCartChanged(Studio* studio)
{
    CartHash hash;
    if(!studio_is_cart_loaded(studio)) return false;

    md5(&studio->tic->cart, sizeof(tic_cartridge), hash.data);

    return memcmp(hash.data, studio->cart.hash.data, sizeof(CartHash)) != 0;
}

void runGame(Studio* studio, RunOrigin origin)
{
#if defined(BUILD_EDITORS)

    if(studio->config->data.fft)
    {
        // initialize FFT data structures
        fPeakMinValue = 0.01f;
        fPeakSmoothing = 0.995f;
        fPeakSmoothValue = 0.0f;
        fAmplification = 1.0f;
        memset(fftData, 0, sizeof(fftData[0]) * FFT_SIZE);
        memset(fftSmoothingData, 0, sizeof(fftSmoothingData[0]) * FFT_SIZE);
        memset(fftRawData, 0, sizeof fftRawData);
        memset(fftRawSmoothingData, 0, sizeof fftRawSmoothingData);
        memset(fftNormalizedData, 0, sizeof(fftNormalizedData[0]) * FFT_SIZE);
        memset(fftNormalizedMaxData, 0, sizeof(fftNormalizedMaxData[0]) * FFT_SIZE);
    }

    if(console_replay(studio->console))
        studioGoto(studio, TIC_CONSOLE_MODE, StudioGoScreen);
    else
#endif
        studioRunGame(studio, origin);
}

#if defined(BUILD_EDITORS)
// The cart a fetch found is in the machine and taken as the host's own by the
// time this is called; all that is left is telling the console it arrived.
static void onConsoleFetched(void* data)
{
    Studio* studio = data;

    ConsoleAnswer answer = {.name = studioCart(studio)->name};
    console_answer(studio->console, &answer);
}

// The dialog the console asked for has been answered; its own flow continues
// from there, whichever of its commands wanted one.
static void onConsoleConfirmed(Studio* studio, bool yes, void* data)
{
    (void)data;
    ConsoleAnswer answer = {.yes = yes};
    console_answer(studio->console, &answer);
}

// What the console asks the host for: it reports, and this does it and answers.
// A move is answered on the spot, and the answer can lead to the next ask — a
// dialog, then the load it confirmed — while a cart fetched by hash is answered
// later, from wherever it arrives.
static void serveConsole(Studio* studio)
{
    ConsoleAsk ask;

    while(console_take(studio->console, &ask))
    {
        ConsoleAnswer answer = {0};
        bool answered = true;

        switch(ask.kind)
        {
            case CONSOLE_ASK_MENU: gotoMenu(studio); break;
            case CONSOLE_ASK_CODE: gotoCode(studio); break;
            case CONSOLE_ASK_EXIT: exitStudio(studio); break;
            case CONSOLE_ASK_RESUME: resumeGame(studio); break;
            case CONSOLE_ASK_RUN: runGame(studio, RUN_FROM_STUDIO); break;

            case CONSOLE_ASK_CONFIG_RESET:
                studio->config->reset(studio->config);
                break;

#if defined(BUILD_SURF)
            case CONSOLE_ASK_SURF: gotoSurf(studio); break;
#endif

            case CONSOLE_ASK_CONFIRM:
                // The dialog belongs to the menu; the answer comes back when the
                // user picks one of its two items.
                answered = false;
                confirmDialog(studio, ask.rows, ask.rowsCount, onConsoleConfirmed, studio);
                break;

            case CONSOLE_ASK_PLAY:
                runGame(studio, RUN_FROM_PLAYER);
                studio->boot.cart = false;
                break;

            case CONSOLE_ASK_CART_LOADED:
                if(ask.name)
                    studioSetCartName(studio, ask.name, tic_fs_path(studio->fs, ask.name));
                else if(ask.clear)
                    memset(studioCart(studio)->name, 0, sizeof studioCart(studio)->name);

                studioRomLoaded(studio);

                answer.name = studioCart(studio)->name;
                break;

            case CONSOLE_ASK_CART_SAVE:
                answer.save = studioSaveCart(studio, ask.name);
                answer.name = studioCart(studio)->name;
                break;

            case CONSOLE_ASK_CART_FETCH:
                // Answered from the fetch's own callback, not here.
                answered = false;
                studioLoadByHash(studio, ask.name, ask.hash, ask.section, onConsoleFetched, studio);
                break;

            // Both answer with the written path, or nothing when they could not
            // write it.
            case CONSOLE_ASK_EXPORT_SFX:
                answer.ok = studioExportSfx(studio, ask.index, ask.name) != NULL;
                break;

            case CONSOLE_ASK_EXPORT_MUSIC:
                answer.ok = studioExportMusic(studio, ask.index, ask.bank, ask.name) != NULL;
                break;

            default: break;
        }

        if(answered)
            console_answer(studio->console, &answer);
    }
}

void saveProject(Studio* studio)
{
    if(getConfig(studio)->trim) trimWhitespace(studio->code);

    CartSaveResult rom = studioSaveCart(studio, NULL);

    if(rom == CART_SAVE_OK)
    {
        char buffer[STUDIO_TEXT_BUFFER_WIDTH];
        char str_saved[] = " saved :)";

        s32 name_len = (s32)strlen(studio->rom.name);
        if(name_len + strlen(str_saved) > sizeof(buffer))
        {
            char subbuf[sizeof(buffer) - sizeof(str_saved) - 5];
            memset(subbuf, '\0', sizeof subbuf);
            strncpy(subbuf, studio->rom.name, sizeof subbuf - 1);

            snprintf(buffer, sizeof buffer, "%s[...]%s", subbuf, str_saved);
        }
        else
        {
            snprintf(buffer, sizeof buffer, "%.*s%s",
                     (s32)(sizeof buffer - sizeof str_saved), studio->rom.name, str_saved);
        }

        showPopupMessage(studio, buffer);
    }
    else if(rom == CART_SAVE_MISSING_NAME)
        showPopupMessage(studio, "error: missing cart name :(");
    else
        showPopupMessage(studio, "error: file not saved :(");
}

static void setCoverImage(Studio* studio)
{
    tic_mem* tic = studio->tic;

    if(studio->mode == TIC_RUN_MODE)
    {
        tic_api_sync(tic, tic_sync_screen, 0, true);
        showPopupMessage(studio, "cover image saved :)");
    }
}

static void generateScreenshotName(Studio* studio, const char* extension, char filenameOut[TICNAME_MAX])
{
    const char* romName = studio->rom.name;

    // --- Strip extension ---
    const char* dot = strrchr(romName, '.');
    size_t baseLen = dot ? (size_t)(dot - romName) : strlen(romName);

    // --- Get current time with millisecond precision ---
    time_t sec = 0;
    long msec = 0;

#if defined(_WIN32)
    // Windows: Use GetSystemTimeAsFileTime for high precision.
    // It provides time in 100-nanosecond intervals since Jan 1, 1601.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Convert FILETIME to Unix seconds and milliseconds
    sec = (time_t)((uli.QuadPart - UNIX_EPOCH_IN_FILETIME) / 10000000ULL);
    msec = (long)((uli.QuadPart / 10000) % 1000);
#else
    // Other systems (Linux, macOS, 3DS, etc.): Use gettimeofday, which is widely available.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    msec = (long)(tv.tv_usec / 1000);
#endif

    // --- Convert seconds to a local time structure (thread-safe) ---
    struct tm tm_local;

    // Thread-safe localtime
#if defined(_WIN32)
    // Windows secure version
    localtime_s(&tm_local, &sec);
#else
    // POSIX re-entrant version
    localtime_r(&sec, &tm_local);
#endif

    // --- Format the timestamp string ---
    char timestamp[32] = {0};

    // Format as -yymmdd-hhmmss
    strftime(timestamp, sizeof(timestamp), "-%y%m%d-%H%M%S", &tm_local);

    // Append milliseconds
    size_t len = strlen(timestamp);
    snprintf(timestamp + len, sizeof(timestamp) - len, "-%03ld", msec);

    // --- Adjust baseLen to prevent overflow ---
    size_t maxBaseLen = TICNAME_MAX - strlen(timestamp) - strlen(extension) - 1; // for null terminator

    if(baseLen > maxBaseLen)
        baseLen = maxBaseLen;

    // --- Build the final filename string ---
    snprintf(filenameOut, TICNAME_MAX, "%.*s%s%s",
             (int)baseLen, romName, timestamp, extension);
}

static void stopVideoRecord(Studio* studio)
{
    MsfGifResult result = msf_gif_end(&studio->video.gif);

    char filename[TICNAME_MAX];
    generateScreenshotName(studio, ".gif", filename);

    // Now that it has found an available filename, save it.
    if(tic_fs_save(studio->fs, filename, result.data, (s32)(result.dataSize), true))
    {
        char msg[TICNAME_MAX + sizeof " saved :)"];
        sprintf(msg, "%s saved :)", filename);
        showPopupMessage(studio, msg);

        tic_sys_open_path(tic_fs_path(studio->fs, filename));
    }
    else
        showPopupMessage(studio, "error: file not saved :(");

    msf_gif_free(result);

    studio->video.record = false;
}

void startVideoRecord(Studio* studio)
{
    if(studio->video.record)
    {
        stopVideoRecord(studio);
    }
    else
    {
        studio->video.record = true;
        studio->video.frame = 0;

        s32 scale = studio->config->data.uiScale;
        msf_gif_begin(&studio->video.gif, TIC80_FULLWIDTH * scale, TIC80_FULLHEIGHT * scale);
    }
}

void takeScreenshot(Studio* studio)
{
    startVideoRecord(studio);
    studio->video.screenshot = true;
}
#endif

void gotoFullscreen(Studio* studio)
{
    tic_sys_fullscreen_set(studio->config->data.options.fullscreen = !tic_sys_fullscreen_get());
}

#if defined(BUILD_EDITORS)
void studioCodeViewToggle(Studio* studio, CodeViewOption option)
{
    Code* code = studio->code;
    struct StudioOptions* options = &studio->config->data.options;

    switch(option)
    {
        case CodeViewFont:
            options->codeAltFont = !options->codeAltFont;
            code->altFont = options->codeAltFont;
            break;

        case CodeViewShadow:
            options->codeShadow = !options->codeShadow;
            code->shadowText = options->codeShadow;
            break;

        case CodeViewLineNumbers:
            // LINES with the sidebar hidden brings the sidebar back with them on
            if(options->codeSidebar)
                options->codeLineNumbers = !options->codeLineNumbers;
            else
                options->codeSidebar = options->codeLineNumbers = true;

            code->lineNumbers = options->codeLineNumbers;
            code->showGutter = options->codeSidebar;
            code->update(code);
            break;

        case CodeViewSidebar:
            options->codeSidebar = !options->codeSidebar;

            code->showGutter = options->codeSidebar;
            code->update(code);
            break;

        case CodeViewStatusbar:
            options->codeStatusbar = !options->codeStatusbar;
            code->showStatusbar = options->codeStatusbar;
            code->update(code);
            break;
    }

    studio->config->saveOptions(studio->config);
}

static void updateMenubar(Studio* studio)
{
    EditorMode mode = getStudioMode(studio);
    const EditorApp* app = &Apps[mode];

    studiomenu_sync(studio, &studio->menubar, mode, app);

    void* instance = app->instance ? app->instance(studio) : NULL;
    studiomenu_sync_editor(studio, &studio->menubar, mode, instance);
}

static void drawStudioMenubar(Studio* studio)
{
    if(!Apps[studio->mode].name)
        return;

    updateMenubar(studio);
    studio->menubar.tooltip = studio->tooltip.text[0] ? studio->tooltip.text : NULL;
    menubar_draw(&studio->menubar, studio->tic, studio->mode,
                 &getConfig(studio)->cart->bank0.tiles);
}
#endif

#if defined(CRT_SHADER_SUPPORT)
static void switchCrtMonitor(Studio* studio)
{
    studio->config->data.options.crt = !studio->config->data.options.crt;
}
#endif

#if defined(BUILD_EDITORS)
static u32 getTime()
{
    return (u32)(tic_sys_counter_get() * 1000 / tic_sys_freq_get());
}

static void hideBattleTime(Studio* studio)
{
    studio->bytebattle.battle.hidetime = !studio->bytebattle.battle.hidetime;
}

static void startBattle(Studio* studio)
{
    studio->bytebattle.battle.started = getTime();
}
#endif

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)

static void switchBank(Studio* studio, s32 bank)
{
    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == studio->mode)
        {
            if(studio->bank.chained)
                memset(studio->bank.indexes, bank, sizeof studio->bank.indexes);
            else
                studio->bank.indexes[i] = (s8)bank;
            break;
        }

    refreshEditorView(studio);
}

void studiomenu_bank_switch(Studio* studio, s32 bank)
{
    switchBank(studio, bank);
}

void studiomenu_bank_chain(Studio* studio)
{
    studio->bank.chained = !studio->bank.chained;

    if(studio->bank.chained)
    {
        s32 banked = 0;

        for(s32 i = 0; i < COUNT_OF(BankModes); i++)
            if(BankModes[i] == studio->mode)
            {
                banked = i;
                break;
            }

        memset(studio->bank.indexes, studio->bank.indexes[banked], sizeof studio->bank.indexes);
    }
}

bool studiomenu_bank_state(Studio* studio, s32* index, bool* chained, bool* banked)
{
    EditorMode mode = getStudioMode(studio);

    *index = 0;
    *chained = studio->bank.chained;
    *banked = isBanked(mode);

    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == mode)
        {
            *index = studio->bank.indexes[i];
            break;
        }

    return *banked;
}

#endif

void gotoMenu(Studio* studio)
{
    studioGoto(studio, TIC_MENU_MODE, StudioGoMenu);

    // A menu asked for while it is already the mode is rebuilt all the same.
    resetMainMenu(studio);
}

// Whether the menu's back resumes a game: it does when a player's run is
// paused under the menu. A dev's run and a menu opened in the studio both
// leave for the studio — the same step ESC takes in RUN mode (#2937).
bool studio_menu_over_player_run(Studio* studio)
{
    ModeState state = modeState(studio);

    return modesMenuOverPlayerRun(&state);
}

// ESC in a dev run, and the back of the pause menu over one: the run was asked
// for from the studio, so it is left the way it was entered — one press, and
// no walk to CLOSE GAME (#2937).
void leaveRun(Studio* studio)
{
    studioGoto(studio, studio->runState.origin, StudioGoLeave);
}

bool enterWasPressedOnce(Studio* studio)
{
    return keyboard_pressed_once(studio->tic, tic_key_return) ||
           keyboard_pressed_once(studio->tic, tic_key_numpadenter);
}

#if defined(BUILD_EDITORS)

static bool showGameMenu(Studio* studio)
{
    tic_mem* tic = studio->tic;

    char tag[TICNAME_MAX];
    snprintf(tag, sizeof tag, "\n%s menu:", tic_get_script(tic)->singleComment);

    return strstr(tic->cart.code.data, tag);
}

#endif

static void processShortcuts(Studio* studio)
{
    tic_mem* tic = studio->tic;

    if(studio->mode == TIC_START_MODE) return;

#if defined(BUILD_EDITORS)
    if(!console_active(studio->console)) return;
#endif

    if(mainmenu_keyboard(studio->mainmenu)) return;

#if defined(BUILD_EDITORS)
    // ESC was the menu's this frame: it closed it and goes no further. The
    // flag stays set until the next menubar_step, so keyWasPressed() hides the
    // key from the editor's tick too.
    if(studio->menubar.escaped)
        return;
#endif

    bool alt = tic_api_key(tic, tic_key_alt);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

#if defined(CRT_SHADER_SUPPORT)
    if(keyboard_pressed_once(studio->tic, tic_key_f6)) switchCrtMonitor(studio);
#endif

    if(alt)
    {
        if(enterWasPressedOnce(studio)) gotoFullscreen(studio);
#if defined(BUILD_EDITORS)
        else if(studio->mode != TIC_RUN_MODE && studio->config->data.keyboardLayout != tic_layout_azerty)
        {
#ifndef KEYBOARD_LAYOUT_ES
            if(keyboard_pressed_once(studio->tic, tic_key_grave))
                studioGoto(studio, TIC_CONSOLE_MODE, StudioGoScreen);
            else if(keyboard_pressed_once(studio->tic, tic_key_1))
            {
                if(studio->mode != TIC_CODE_MODE)
                {
                    studioGoto(studio, TIC_CODE_MODE, StudioGoScreen);
                    code_just_switched(studio->code);
                }
            }
            else
            {
                // ctrl+1 is the code editor's, with its "just switched" flag; the
                // rest follow the tab order.
                for(s32 i = 1; i < COUNT_OF(Modes); i++)
                    if(keyboard_pressed_once(studio->tic, tic_key_1 + i))
                    {
                        studioGoto(studio, Modes[i], StudioGoScreen);
                        break;
                    }
            }
#endif
        }
#endif
    }
    else if(ctrl)
    {
        if(keyboard_pressed_once(studio->tic, tic_key_q)) studio_exit(studio);
#if defined(BUILD_EDITORS)
        else if(keyboard_pressed_once(studio->tic, tic_key_pageup))
            changeStudioMode(studio, -1);
        else if(keyboard_pressed_once(studio->tic, tic_key_pagedown))
            changeStudioMode(studio, +1);
        else if(enterWasPressedOnce(studio))
            runGame(studio, RUN_FROM_STUDIO);
        else if(keyboard_pressed_once(studio->tic, tic_key_r))
            runGame(studio, RUN_FROM_STUDIO);
        else if(keyboard_pressed_once(studio->tic, tic_key_s))
            saveProject(studio);
        else if(keyboard_pressed_once(studio->tic, tic_key_f10))
            hideBattleTime(studio);
#endif

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)

        else
            for(s32 bank = 0; bank < TIC_BANKS; bank++)
                if(keyboard_pressed_once(studio->tic, tic_key_0 + bank))
                    switchBank(studio, bank);

#endif
    }
    else
    {
        if(keyboard_pressed_once(studio->tic, tic_key_f11)) gotoFullscreen(studio);
#if defined(BUILD_EDITORS)
        else if(keyboard_pressed_once(studio->tic, tic_key_escape))
        {
            if(
                getConfig(studio)->options.keybindMode == KEYBIND_VI && getStudioViMode(studio) != VI_NORMAL)
                return;

            studioEscape(studio);
        }
        else if(keyboard_pressed_once(studio->tic, tic_key_f8))
            takeScreenshot(studio);
        else if(keyboard_pressed_once(studio->tic, tic_key_f9))
            startVideoRecord(studio);
        else if(keyboard_pressed_once(studio->tic, tic_key_f12))
            startBattle(studio);
        else if(studio->mode == TIC_RUN_MODE && keyboard_pressed_once(studio->tic, tic_key_f7))
            setCoverImage(studio);

        // A running game owns the function keys: a player's run always hands
        // them over, and a cart that declares a game menu asks for them in a
        // dev's run as well. Leaving a run is ESC's job (see leaveRun), so the
        // studio has no use for them there.
        if((studio->runState.owner != RUN_FROM_PLAYER && !showGameMenu(studio)) || studio->mode != TIC_RUN_MODE)
        {
            if(keyboard_pressed_once(studio->tic, Apps[TIC_CODE_MODE].hotkey))
            {
                if(studio->mode != TIC_CODE_MODE)
                {
                    studioGoto(studio, TIC_CODE_MODE, StudioGoScreen);
                    code_just_switched(studio->code);
                }
            }
            else
            {
                // F1's editor carries the "just switched" flag; the rest are uniform.
                for(s32 i = 1; i < COUNT_OF(Modes); i++)
                    if(keyboard_pressed_once(studio->tic, Apps[Modes[i]].hotkey))
                    {
                        studioGoto(studio, Modes[i], StudioGoScreen);
                        break;
                    }
            }
        }
#else
        else if(keyboard_pressed_once(studio->tic, tic_key_escape))
            studioEscape(studio);
#endif
    }
}

#if defined(BUILD_EDITORS)
static void reloadConfirm(Studio* studio, bool yes, void* data)
{
    (void)data;
    if(yes)
        studioReloadCart(studio);
    else
        updateMDate(studio);
}

static void checkChanges(Studio* studio)
{
    static u32 tick_count = 0;
    tick_count++;
    if(tick_count < 30)
    {
        return;
    }
    tick_count = 0;

    switch(studio->mode)
    {
        case TIC_START_MODE:
            break;
        default:
        {
            u64 date = fs_date(studio->rom.path);

            if(studio->cart.mdate && date > studio->cart.mdate)
            {
                if(studioCartChanged(studio) && studio->mode != TIC_MENU_MODE)
                {
                    static const char* Rows[] =
                        {
                            "WARNING!",
                            "The cart has changed!",
                            "Do you want to reload it?"};

                    confirmDialog(studio, Rows, COUNT_OF(Rows), reloadConfirm, NULL);
                }
                else
                    studioReloadCart(studio);
            }
        }
    }
}

static void drawBitIconRaw(Studio* studio, u32* frame, s32 sx, s32 sy, s32 id, tic_color color)
{
    const tic_bank* bank = &getConfig(studio)->cart->bank0;

    u32* dst = frame + sx + sy * TIC80_FULLWIDTH;
    for(s32 src = 0; src != TIC_SPRITESIZE * TIC_SPRITESIZE; dst += TIC80_FULLWIDTH - TIC_SPRITESIZE)
        for(s32 i = 0; i != TIC_SPRITESIZE; ++i, ++dst)
            if(tic_tool_peek4(&bank->tiles.data[id].data, src++))
                *dst = tic_rgba(&bank->palette.vbank0.colors[color]);
}

static void drawRecordLabel(Studio* studio, u32* frame, s32 sx, s32 sy)
{
    drawBitIconRaw(studio, frame, sx, sy, tic_icon_rec, tic_color_red);
    drawBitIconRaw(studio, frame, sx + TIC_SPRITESIZE, sy, tic_icon_rec2, tic_color_red);
}

static bool isRecordFrame(Studio* studio)
{
    return studio->video.record;
}

static void recordFrame(Studio* studio, u32* pixels)
{
    if(studio->video.record)
    {
        if(studio->video.frame % 2 == 0)
        {
            s32 scale = studio->config->data.uiScale;
            s32 w = TIC80_FULLWIDTH * scale;
            s32 h = TIC80_FULLHEIGHT * scale;

            u32* ptr = studio->video.buffer;
            const u32* src = pixels;
            for(s32 y = 0; y < h; y++, src = pixels + (y / scale) * TIC80_FULLWIDTH)
                for(s32 x = 0; x < w; x++)
                    *ptr++ = src[x / scale];

            // with centiSecondsPerFame == 3 we have 1000/(3*10)=~33.3fps, so we have to save every second frame
            msf_gif_frame(&studio->video.gif, (u8*)studio->video.buffer, 3, 16, w * sizeof(u32));
        }

        if(studio->video.screenshot)
        {
            studio->video.screenshot = false;
            stopVideoRecord(studio);
            return;
        }

        if(studio->video.frame % TIC80_FRAMERATE < TIC80_FRAMERATE / 2)
        {
            drawRecordLabel(studio, pixels, TIC80_WIDTH - 24, 8);
        }

        studio->video.frame++;
    }
}

#endif

static void renderStudio(Studio* studio)
{
    tic_mem* tic = studio->tic;

    {
        const tic_sfx* sfx = NULL;
        const tic_music* music = NULL;

        switch(studio->mode)
        {
            case TIC_RUN_MODE:
                sfx = &studio->tic->ram->sfx;
                music = &studio->tic->ram->music;
                break;
            case TIC_START_MODE:
            case TIC_MENU_MODE:
#if defined(BUILD_SURF)
            case TIC_SURF_MODE:
#endif
                sfx = &studio->config->cart->bank0.sfx;
                music = &studio->config->cart->bank0.music;
                break;
            default:
#if defined(BUILD_EDITORS)
                sfx = getSfxSrc(studio);
                music = getMusicSrc(studio);
#endif
                break;
        }

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, music);

        // restore mapping
        studio->tic->ram->mapping = getConfig(studio)->options.mapping;

        tic_core_tick_start(tic);
    }

    // SECURITY: It's important that this comes before `tick` and not after
    // to prevent misbehaving cartridges from having an opportunity to
    // tamper with the keyboard input.
    processShortcuts(studio);

#if defined(BUILD_EDITORS)
    // The bar owns the frame's keys: keep them from the editor's tick. This
    // runs after tick_start's snapshot and the global shortcuts, so `now`
    // keeps the real keys and the menu's repeats stay correct. Ctrl and Alt
    // combinations type nothing and carry the accelerators the menu shows, so
    // they pass; getKeyboardText() holds back typed text on its own.
    if(studio->menubar.captured && !tic_api_key(tic, tic_key_ctrl) &&
       !tic_api_key(tic, tic_key_alt))
        tic->ram->input.keyboard.data = 0;
#endif

    // clear screen for all the modes except the Run mode
    if(studio->mode != TIC_RUN_MODE)
    {
        VBANK(tic, 1)
        {
            tic_api_cls(tic, 0);
        }
    }

    {
        const EditorApp* app = &Apps[studio->mode];

#if defined(BUILD_EDITORS)
        // What the console reads of the host, fresh before its tick; the
        // startup tick the mode has not caught up with says it is not in front.
        if(studio->mode == TIC_CONSOLE_MODE)
            studio->consoleView = (ConsoleView){
                .inFront = true,
                .cartChanged = studioCartChanged(studio),
            };
#endif

#if defined(BUILD_EDITORS)
        // The menu is modal: its presses and clicks must not reach the editor.
        MouseState* menuMouse = studio->mouse.state;
        bool menuMasked[COUNT_OF(studio->mouse.state)];
        bool menuOpen = studio->menubar.open >= 0;

        for(s32 i = 0; i < COUNT_OF(studio->mouse.state); i++)
        {
            // With autohide the editor fills the screen under the bar, so a
            // press that starts in the top strip belongs to the menu.
            bool owner = menuOpen ||
                         (studio->menubar.autohide && (menuMouse[i].down || menuMouse[i].click) &&
                          menuMouse[i].start.y < MENUBAR_HEIGHT);

            menuMasked[i] = owner && menuMouse[i].down;

            if(menuMasked[i])
                menuMouse[i].down = false;

            if(owner)
            {
                menuMouse[i].click = false;
                menuMouse[i].dbl.click = false;
            }
        }

#endif

#if defined(BUILD_EDITORS)
        if(studio->mode == TIC_CODE_MODE)
        {
            studio->codeView = codeFrame(studio, &studio->codeBattle);
            code_set_view(studio->code, &studio->codeView);
        }

        refreshEditorView(studio);
#endif

        // The menu reads the mode and the bar's pointer ownership, fresh before
        // its tick; the view is filled in every mode.
        studio->menuView = menuFrame(studio);

        if(app->tick)
            app->tick(app->instance(studio));

#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < COUNT_OF(studio->mouse.state); i++)
            if(menuMasked[i])
                menuMouse[i].down = true;

        serveConsole(studio);
#endif

        // The splash reports that the intro is over; where that leads is the
        // host's: a cart the launch came with, in a build that has no console to
        // run it from, and the home mode everywhere else.
        if(studio->mode == TIC_START_MODE && start_done(studio->start))
        {
#if !defined(BUILD_EDITORS)
            if(getBoot(studio)->cart)
                runGame(studio, RUN_FROM_PLAYER);
            else
#endif
                studioGoto(studio, TIC_HOME_MODE, StudioGoScreen);
        }

        // The bar follows the live mode: the editor's tick may have switched
        // it, and a mode with no name gets no bar.
#if defined(BUILD_EDITORS)
        const EditorApp* barApp = &Apps[studio->mode];

        if(barApp->name)
        {
            if(barApp->vbank1)
            {
                VBANK(tic, 1)
                {
                    drawStudioMenubar(studio);
                }
            }
            else
                drawStudioMenubar(studio);
        }
#else
        TIC_UNUSED(app);
#endif
    }

    tic_core_tick_end(tic);

    switch(studio->mode)
    {
        case TIC_RUN_MODE: break;
#if defined(BUILD_SURF)
        case TIC_SURF_MODE:
#endif
        case TIC_MENU_MODE:
            tic->input.data = (u8)-1;
            break;
        default:
            tic->input.data = (u8)-1;
            tic->input.gamepad = 0;
    }
}

static void updateSystemFont(Studio* studio)
{
    tic_mem* tic = studio->tic;

    studio->systemFont = (tic_font){
        .regular =
            {
                {0},
                {
                    {
                        .width = TIC_FONT_WIDTH,
                        .height = TIC_FONT_HEIGHT,
                    },
                },
            },
        .alt =
            {
                {0},
                {
                    {
                        .width = TIC_ALTFONT_WIDTH,
                        .height = TIC_FONT_HEIGHT,
                    },
                },
            }};

    u8* dst = (u8*)&studio->systemFont;

    for(s32 i = 0; i < TIC_FONT_CHARS * 2; i++)
        for(s32 y = 0; y < TIC_SPRITESIZE; y++)
            for(s32 x = 0; x < TIC_SPRITESIZE; x++)
                if(tic_tool_peek4(&studio->config->cart->bank0.sprites.data[i], TIC_SPRITESIZE * y + x))
                    dst[i * BITS_IN_BYTE + y] |= 1 << x;

    tic->ram->font = studio->systemFont;
}

static void configChanged(void* host)
{
    studioConfigChanged(host);
}

static void romLoaded(void* host)
{
    studioRomLoaded(host);
}

static void romSaved(void* host)
{
    studioRomSaved(host);
}

void studioConfigChanged(Studio* studio)
{
#if defined(BUILD_EDITORS)
    Code* code = studio->code;
    if(code->update)
        code->update(code);

    s32 scale = studio->config->data.uiScale;
    studio->video.buffer = realloc(studio->video.buffer, TIC80_FULLWIDTH * scale * TIC80_FULLHEIGHT * scale * sizeof(u32));
#endif

    updateSystemFont(studio);
#if defined(BUILD_RENDER_CACHE)
    tic_core_draw_cache_set_enabled(studio->tic, studio->config->data.options.drawCache);
#endif
    tic_sys_update_config();
}

// Ticks of stillness after which the studio stops drawing its own cursor.
// Ten seconds: long enough to read a line of code without the cursor being
// taken for the mouse having left.
#define CURSOR_HIDE_TICKS (TIC80_FRAMERATE * 10)

static void processMouseStates(Studio* studio)
{
    for(s32 i = 0; i < COUNT_OF(studio->mouse.state); i++)
        studio->mouse.state[i].dbl.click = studio->mouse.state[i].click = false;

    tic_mem* tic = studio->tic;

    tic->ram->vram.vars.cursor.sprite = tic_cursor_arrow;
    tic->ram->vram.vars.cursor.system = true;

    {
        const tic_point pos = tic_api_mouse(tic);
        const bool used = pos.x != studio->mouse.last.x || pos.y != studio->mouse.last.y || tic->ram->input.mouse.btns;

        studio->mouse.last = pos;
        studio->mouse.idle = used ? 0 : MIN(studio->mouse.idle + 1, CURSOR_HIDE_TICKS);
    }

    for(s32 i = 0; i < COUNT_OF(studio->mouse.state); i++)
    {
        MouseState* state = &studio->mouse.state[i];

        s32 down = tic->ram->input.mouse.btns & (1 << i);

        if(!state->down && down)
        {
            state->down = true;
            state->start = tic_api_mouse(tic);
        }
        else if(state->down && !down)
        {
            state->end = tic_api_mouse(tic);

            state->click = true;
            state->down = false;

            state->dbl.click = state->dbl.ticks - state->dbl.start < 1000 / TIC80_FRAMERATE;
            state->dbl.start = state->dbl.ticks;
        }

        state->dbl.ticks++;
    }
#if !defined(__TIC_MACOSX__)
    tic->ram->input.mouse.scrollx *= -1;
#endif
}

#if defined(BUILD_EDITORS)
static void doCodeExport(Studio* studio)
{
#ifndef BAREMETALPI
    char pos[sizeof studio->bytebattle.last.postag];
    {
        s32 x = 0, y = 0;

        if(studio->mode != TIC_RUN_MODE)
        {
            codeGetPos(studio->code, &x, &y);
            x++;
            y++;
        }

        sprintf(pos, "-- pos: %i,%i\n", x, y);
    }

    if(strcmp(studio->bytebattle.last.postag, pos) || strcmp(studio->bytebattle.last.code.data, studio->code->text.src))
    {
        FILE* file = fopen(studio->bytebattle.exp, "wb");

        if(file)
        {
            strcpy(studio->bytebattle.last.postag, pos);
            strcpy(studio->bytebattle.last.code.data, studio->code->text.src);

            fwrite(pos, 1, strlen(pos), file);
            fwrite(studio->code->text.src, 1, strlen(studio->code->text.src), file);
            fclose(file);
        }
    }
#endif
}

static void doCodeImport(Studio* studio)
{
#ifndef BAREMETALPI
    FILE* file = fopen(studio->bytebattle.imp, "rb");

    if(file)
    {
        static tic_code code;
        code.data[fread(code.data, 1, sizeof(tic_code), file)] = '\0';

        char* end = strchr(code.data, '\n');

        if(end)
        {
            static const char PosTag[] = "-- pos: ";
            enum
            {
                TagSize = sizeof PosTag - 1
            };

            if(memcmp(code.data, PosTag, TagSize) == 0)
            {
                char* start = code.data + TagSize;
                char* sep = strchr(start, ',');

                if(sep)
                {
                    *sep = *end = '\0';
                    s32 x = atoi(start);
                    s32 y = atoi(sep + 1);

                    s32 offset = (s32)(end - code.data + 1);
                    memcpy(studio->code->text.src, code.data + offset, sizeof(tic_code) - offset);
                    if(x == 0 && y == 0)
                    {
                        if(studio->mode != TIC_RUN_MODE)
                            runGame(studio, RUN_FROM_STUDIO);
                    }
                    else
                    {
                        codeSetPos(studio->code, x - 1, y - 1);

                        if(studio->mode == TIC_RUN_MODE)
                            studioGoto(studio, TIC_CODE_MODE, StudioGoScreen);
                    }
                }
            }
        }

        fclose(file);
    }
#endif
}
#endif

static void blitCursor(Studio* studio)
{
    tic_mem* tic = studio->tic;
    tic80_mouse* m = &tic->ram->input.mouse;

    // Leaving the app writes -1 into both axes, which a byte reads as 255; at
    // that height the cursor's spans would run off the end of the framebuffer.
    if(studio->mouse.idle < CURSOR_HIDE_TICKS && tic->input.mouse && !m->relative && (s32)m->y < TIC80_FULLHEIGHT)
    {
        s32 sprite = CLAMP(tic->ram->vram.vars.cursor.sprite, 0, TIC_BANK_SPRITES - 1);
        const tic_bank* bank = &tic->cart.bank0;

        tic_point hot = {0};

        if(tic->ram->vram.vars.cursor.system)
        {
            bank = &getConfig(studio)->cart->bank0;
            hot = (tic_point[]){
                {0, 0},
                {3, 0},
                {2, 3},
            }[CLAMP(sprite, 0, 2)];
        }
        else if(sprite == 0)
            return;

        const tic_palette* pal = &bank->palette.vbank0;
        const tic_tile* tile = &bank->sprites.data[sprite];

        tic_point s = {m->x - hot.x, m->y - hot.y};
        u32* dst = tic->product.screen + TIC80_FULLWIDTH * s.y + s.x;

        for(s32 y = s.y, endy = MIN(y + TIC_SPRITESIZE, TIC80_FULLHEIGHT), i = 0; y != endy; ++y, dst += TIC80_FULLWIDTH - TIC_SPRITESIZE)
            for(s32 x = s.x, endx = x + TIC_SPRITESIZE; x != endx; ++x, ++i, ++dst)
                if(x < TIC80_FULLWIDTH)
                {
                    u8 c = tic_tool_peek4(tile->data, i);
                    if(c)
                        *dst = tic_rgba(&pal->colors[c]);
                }
    }
}

tic_mem* getMemory(Studio* studio)
{
    return studio->tic;
}

CartName* studioCart(Studio* studio)
{
    return &studio->rom;
}

tic_fs* studio_fs(Studio* studio)
{
    return studio->fs;
}

// The public cart API: the studio's names over the service's functions.
void studioSetCartName(Studio* studio, const char* name, const char* path)
{
    rom_set_cart_name(&studio->romDeps, name, path);
}

bool studioLoadCart(Studio* studio, const char* path)
{
    return rom_load_cart(&studio->romDeps, path);
}

void studioReloadCart(Studio* studio)
{
    rom_reload_cart(&studio->romDeps);
}

void studioLoadByHash(Studio* studio, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data)
{
    rom_load_by_hash(&studio->romDeps, name, hash, section, callback, data);
}

CartSaveResult studioSaveCart(Studio* studio, const char* name)
{
    return rom_save_cart(&studio->romDeps, name);
}

CartSaveResult studioAutoSave(Studio* studio)
{
    return rom_autosave(&studio->romDeps);
}

const tic_mem* studio_mem(Studio* studio)
{
    return getMemory(studio);
}

void studio_tick(Studio* studio, tic80_input input)
{
    tic_mem* tic = studio->tic;
    tic->ram->input = input;

#if defined(BUILD_EDITORS)
    processAnim(studio->anim.movie, studio);
    checkChanges(studio);
#endif
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    tic_net_start(studio->net);
#endif

    // A menu item lands here rather than switching mid-frame: the mode's blit
    // callback is chosen after its tick, so the switch has to wait for that.
#if defined(BUILD_EDITORS)
    bool menubarSwitched = studio->menubar.requested != MENUBAR_NO_MODE;

    if(menubarSwitched)
    {
        EditorMode mode = studio->menubar.requested;

        studio->menubar.requested = MENUBAR_NO_MODE;

        if(mode == TIC_MENU_MODE)
            gotoMenu(studio);
#if defined(BUILD_SURF)
        else if(mode == TIC_SURF_MODE)
            gotoSurf(studio);
#endif
        else
            studioGoto(studio, mode, StudioGoScreen);
    }
#endif

    // After processMouseStates so a press is seen the frame it arrives, and
    // before renderStudio. The menu takes the pointer here too, before the
    // editor under its popup sees it.
    processMouseStates(studio);

    // The editors write it as they draw; the bar shows what this frame set.
    studio->tooltip.text[0] = '\0';
#if defined(BUILD_EDITORS)
    if(Apps[studio->mode].name)
    {
        updateMenubar(studio);
        menubar_step(&studio->menubar, studio->tic, (s32)studio->mode);
    }
    else
        menubar_step(&studio->menubar, studio->tic, MENUBAR_NO_MODE);

    // The item ran last frame; the key that ran it is still the bar's, not the
    // editor it just opened. A mode change resets the core's key history, so
    // a still-held Enter would read there as a new press. Text is muted for
    // the console too, which reads the system queue directly.
    if(menubarSwitched)
    {
        studio->menubar.captured = true;
        keyboard_mute_text(true);
    }
#endif

    renderStudio(studio);

#if defined(BUILD_EDITORS)
    keyboard_mute_text(false);
#endif

    {
        const EditorApp* app = &Apps[studio->mode];
        tic_blit_callback callback = {.scanline = app->scanline};

        // Only a mode that has a scanline needs its instance, and only those
        // have one — this is the fallback to a plain blit.
        if(app->scanline)
            callback.data = app->instance(studio);

        if(studio->mode != TIC_RUN_MODE)
        {
            memcpy(tic->ram->vram.palette.data, getConfig(studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));
            tic->ram->font = studio->systemFont;
        }

#if defined(BUILD_RENDER_CACHE)
        tic80_mouse* m = &tic->ram->input.mouse;
        bool mouse_visible = (studio->mouse.idle < CURSOR_HIDE_TICKS && tic->input.mouse && !m->relative && (s32)m->y < TIC80_FULLHEIGHT);
        if(mouse_visible || studio->mouse.prev.visible)
        {
            if(m->x != studio->mouse.prev.x || m->y != studio->mouse.prev.y ||
               tic->ram->vram.vars.cursor.sprite != studio->mouse.prev.sprite ||
               tic->ram->vram.vars.cursor.system != studio->mouse.prev.system ||
               mouse_visible != studio->mouse.prev.visible)
            {
                tic_core_invalidate(tic);
                studio->mouse.prev.x = m->x;
                studio->mouse.prev.y = m->y;
                studio->mouse.prev.sprite = tic->ram->vram.vars.cursor.sprite;
                studio->mouse.prev.system = tic->ram->vram.vars.cursor.system;
                studio->mouse.prev.visible = mouse_visible;
            }
        }
#endif

        callback.scanline
            ? tic_core_blit_ex(tic, callback)
            : tic_core_blit(tic);

        // Studio chrome in the studio's palette, after the blit and outside
        // the gif, screenshot and cover image — hence recordFrame here.
        blitCursor(studio);

#if defined(BUILD_EDITORS)
        if(isRecordFrame(studio))
            recordFrame(studio, tic->product.screen);

        drawPopup(studio);
#endif
    }

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    tic_net_end(studio->net);
#endif
#if defined(BUILD_EDITORS)

    {
        Bytebattle* bb = &(studio->bytebattle);
        if(bb->battle.started)
        {
            u32 passed = getTime() - bb->battle.started;
            bb->battle.left = bb->battle.time - passed;

            if(bb->battle.left > 0)
            {
                s32 delta = bb->battle.time / (bb->limit.upper - bb->limit.lower);
                bb->limit.current = bb->limit.upper - passed / delta;
            }
            else
            {
                bb->battle.left = 0;
                bb->limit.current = bb->limit.lower;
            }
        }

        if(bb->delay)
            if(bb->ticks++ < bb->delay)
                return;

        if(bb->exp)
            doCodeExport(studio);
        else if(bb->imp)
            doCodeImport(studio);

        bb->ticks = 0;
    }
#endif
}

void studio_sound(Studio* studio)
{
    tic_mem* tic = studio->tic;
    tic_core_synth_sound(tic);

    s32 volume = getConfig(studio)->options.volume;

    if(volume != MAX_VOLUME)
    {
        s32 size = tic->product.samples.count;
        for(s16 *it = tic->product.samples.buffer, *end = it + size; it != end; ++it)
            *it = (s16)(*it * volume / MAX_VOLUME);
    }
}

static void onStudioLoadConfirmed(Studio* studio, bool yes, void* data)
{
    if(yes)
    {
        const char* file = data;
        bool done = studioLoadCart(studio, file);

#if defined(BUILD_EDITORS)
        showPopupMessage(studio, done
                                     ? "cart successfully loaded :)"
                                     : "error: cart not loaded :(");
#else
        (void)done;
#endif
    }
}

void confirmLoadCart(Studio* studio, ConfirmCallback callback, void* data)
{
    static const char* Warning[] =
        {
            "WARNING!",
            "You have unsaved changes",
            "Do you really want to load cart?",
        };

    confirmDialog(studio, Warning, COUNT_OF(Warning), callback, data);
}

void studio_load(Studio* studio, const char* file)
{
    studioCartChanged(studio)
        ? confirmLoadCart(studio, onStudioLoadConfirmed, (void*)file)
        : onStudioLoadConfirmed(studio, true, (void*)file);
}

// Where a run was entered from decides where CLOSE GAME goes: a cart played in
// SURF returns to the browser, any other run lands on the build's home screen.
// That origin is runFrom, not prevMode — opening the pause menu over the run
// makes RUN the previous mode, so prevMode has forgotten SURF by then (#3015).
void exitGame(Studio* studio)
{
    studioGoto(studio, studio->runState.origin, StudioGoClose);
}

void studio_delete(Studio* studio)
{
    {
#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            freeSprite(studio->banks.sprite[i]);
            freeMap(studio->banks.map[i]);
            freeSfx(studio->banks.sfx[i]);
            freeMusic(studio->banks.music[i]);
        }

        freeCode(studio->code);
        console_free(studio->console);
        freeWorld(studio->world);

        FREE(studio->anim.show.items);
        FREE(studio->anim.hide.items);

#endif
#if defined(BUILD_SURF)
        surf_free(studio->surf);
#endif

        start_free(studio->start);
        run_free(studio->run);
        freeConfig(studio->config);

        mainmenu_free(studio->mainmenu);
        menu_free(studio->menu);
    }

    tic_core_close(studio->tic);

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    tic_net_close(studio->net);
#endif
#if defined(BUILD_EDITORS)
    free(studio->video.buffer);
    if(studio->bytebattle.exp) free(studio->bytebattle.exp);
    if(studio->bytebattle.imp) free(studio->bytebattle.imp);
#endif

    free(studio->fs);
    free(studio);
}

#if defined(BUILD_RENDER_CACHE)
bool studio_is_dirty(Studio* studio)
{
    return tic_core_is_dirty(studio->tic);
}
#endif

#if defined(BUILD_EDITORS)
Bytebattle* getBytebattle(Studio* studio)
{
    return studio->bytebattle.exp || studio->bytebattle.imp ? &(studio->bytebattle) : NULL;
}
#endif

static StartArgs parseArgs(s32 argc, char** argv)
{
    static const char* const usage[] =
        {
            "tic80 [cart] [options]",
            NULL,
        };

    StartArgs args = {0};
    args.volume = -1;

#if defined(BUILD_EDITORS)
    args.lowerlimit = 256;
    args.upperlimit = 512;
    args.fftlist = 0;
#endif

    struct argparse_option options[] =
        {
            OPT_HELP(),
#define CMD_PARAMS_DEF(name, ctype, type, post, help) OPT_##type('\0', #name, &args.name, help),
            CMD_PARAMS_LIST(CMD_PARAMS_DEF)
#undef CMD_PARAMS_DEF
#if defined(BUILD_EDITORS)
                OPT_GROUP("Byte battle options:\n"),
            OPT_STRING('\0', "codeexport", &args.codeexport, "export code to filename"),
            OPT_STRING('\0', "codeimport", &args.codeimport, "import code from filename"),
            OPT_INTEGER('\0', "delay", &args.delay, "codeexport / codeimport update interval in ticks"),
            OPT_INTEGER('\0', "lowerlimit", &args.lowerlimit, "lower limit for code size (256 by default)"),
            OPT_INTEGER('\0', "upperlimit", &args.upperlimit, "upper limit for code size (512 by default)"),
            OPT_INTEGER('\0', "battletime", &args.battletime, "battletime in minutes"),
            OPT_GROUP("FFT options:\n"),
            OPT_BOOLEAN('\0', "fft", &args.fft, "enable FFT support"),
            OPT_BOOLEAN('\0', "fftlist", &args.fftlist, "list FFT devices"),
            OPT_BOOLEAN('\0', "fftcaptureplaybackdevices", &args.fftcaptureplaybackdevices, "Capture playback devices for loopback (Windows only)"),
            OPT_STRING('\0', "fftdevice", &args.fftdevice, "name of the device to use with FFT"),
#endif
            OPT_END(),
        };

    struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
    argparse_describe(&argparse, "\n" TIC_NAME " startup options:", NULL);
    argc = argparse_parse(&argparse, argc, (const char**)argv);

    if(argc == 1)
        args.cart = argv[0];

    return args;
}

#if defined(BUILD_EDITORS)

static void setIdle(void* data)
{
    Studio* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.idle);
}

static void setPopupWait(void* data)
{
    Studio* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.wait);
}

static void setPopupHide(void* data)
{
    Studio* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.hide);
}

#endif

void studio_keymapchanged(Studio* studio, tic_layout keyboardLayout)
{
    studio->config->data.keyboardLayout = keyboardLayout;
}

bool studio_alive(Studio* studio)
{
    return studio->alive;
}

#if defined(TIC_MODULE_EXT)
static bool onEnumModule(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    (void)data;
    (void)dir;
    (void)hash;
    (void)id;
    (void)title;

    if(strstr(name, TIC_MODULE_EXT))
    {
        char dlpath[TICNAME_MAX];
        snprintf(dlpath, sizeof(dlpath), "%s/%s", fs_appfolder(), name);

        void* module = dlopen(dlpath, RTLD_NOW | RTLD_LOCAL);

        if(module)
        {
            const tic_script* config = dlsym(module, DEF2STR(SCRIPT_CONFIG));

            if(config)
            {
                tic_add_script(config);
            }
            else
            {
                // The studio isn't running yet, so log straight to the host console
                fprintf(stderr, "ERROR: %s not exported from %s\n", DEF2STR(SCRIPT_CONFIG), name);
                dlclose(module);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Failed to load %s\n", name);
        }
    }

    return true;
}
#endif

Studio* studio_create(s32 argc, char** argv, s32 samplerate, tic80_pixel_color_format format, const char* folder, s32 maxscale, tic_layout keyboardLayout)
{
    setbuf(stdout, NULL);

    StartArgs args = parseArgs(argc, argv);

    if(args.version)
    {
        printf("%s\n", TIC_VERSION);
        exit(0);
    }

#if defined(TIC_MODULE_EXT)
    // load script modules
    fs_enum(fs_appfolder(), onEnumModule, NULL);
#endif

    Studio* studio = NEW(Studio);
    *studio = (Studio){
        // Still since the day it was born: see where `last` is set below.
        .mouse = {.idle = CURSOR_HIDE_TICKS},

        .mode = TIC_START_MODE,
        .returnTo = TIC_HOME_MODE,
        .runState = {.owner = RUN_FROM_PLAYER, .origin = TIC_HOME_MODE},
#if defined(BUILD_EDITORS)

        .bank =
            {
                .chained = true,
            },

        .anim =
            {
                .pos =
                    {
                        .popup = -POPUP_HEIGHT,
                    },
                .idle = {
                    .done = emptyDone,
                }},

        .popup = {
            .message = "\0",
        },

        .tooltip = {
            .text = "\0",
        },
#endif
        .samplerate = samplerate,
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        .net = tic_net_create(TIC_WEBSITE),
#endif
#if defined(BUILD_EDITORS)
        .bytebattle = {0},
#endif
        .tic = tic_core_create(samplerate, format),
    };

    // Where the machine's mouse starts is not a move: a layer that has not
    // seen a mouse yet reports just this, and nothing is drawn until one
    // arrives — no cursor in the corner of a fresh application.
    studio->mouse.last = tic_api_mouse(studio->tic);

    {
        const char* temp_path = args.fs ? args.fs : folder;
        char path[1024];
        strcpy(path, temp_path);
        for(int i = (int)strlen(path) - 1; i >= 0; i--)
        {
            if(path[i] == '\\' || path[i] == '/')
                path[i] = '\0';
            else
                break;
        }

        if(fs_isdir(path))
        {
            studio->fs = tic_fs_create(path,
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
                                       studio->net
#else
                                       NULL
#endif
            );
        }
        else
        {
            fprintf(stderr, "error: `%s` is not a folder\n", path);
            exit(1);
        }
    }

    {

#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            studio->banks.sprite[i] = calloc(1, sizeof(Sprite));
            studio->banks.map[i] = calloc(1, sizeof(Map));
            studio->banks.sfx[i] = calloc(1, sizeof(Sfx));
            studio->banks.music[i] = calloc(1, sizeof(Music));
        }

        studio->code = calloc(1, sizeof(Code));
        studio->world = calloc(1, sizeof(World));

        studio->anim.show = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setPopupWait,
                                             {
                                                 {-POPUP_HEIGHT, 0, STUDIO_ANIM_TIME, &studio->anim.pos.popup, AnimEaseIn},
                                             });

        studio->anim.wait = (Movie){.time = TIC80_FRAMERATE * 2, .done = setPopupHide};
        studio->anim.hide = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
                                             {
                                                 {0, -POPUP_HEIGHT, STUDIO_ANIM_TIME, &studio->anim.pos.popup, AnimEaseIn},
                                             });

        studio->anim.movie = resetMovie(&studio->anim.idle);
#endif

#if defined(BUILD_SURF)
        studio->surf = calloc(1, sizeof(Surf));
#endif

        studio->run = calloc(1, sizeof(Run));
        studio->config = calloc(1, sizeof(Config));
    }
    studio->mainmenu = NULL;

    tic_fs_makedir(studio->fs, TIC_LOCAL);
    tic_fs_makedir(studio->fs, TIC_LOCAL_VERSION);

    initConfig(studio->config, studio->tic, studio->fs, configChanged, studio);

    // The widget draws and plays from the config cart, so it is created after
    // the config: nothing ticks or draws it before that point, and the tick
    // loop fills the view before the menu hears of it.
    studio->menu = menu_create(&(MenuDeps){
        .tic = studio->tic,
        .cart = studio->config->cart,
        .mouse = studio->mouse.state,
    });
    menu_set_view(studio->menu, &studio->menuView);

    studio->romDeps = (RomDeps){
        .tic = studio->tic,
        .fs = studio->fs,
        .cart = &studio->rom,
        .config = studio->config,
        .host = studio,
        .loaded = romLoaded,
        .saved = romSaved,
    };

#if defined(BUILD_EDITORS)
    void* menuData = studiomenu_init(studio, &studio->menubar, Modes, COUNT_OF(Modes), Apps);

    s32 categoriesCount = 0;
    MenuBarCategory* categories = studiomenu_categories(&categoriesCount);

    menubar_init(&studio->menubar, categories, categoriesCount,
                 studio->mouse.state, menuData, tic_icon_circle);

    // F10 opens the TIC-80 menu: the last category is the host's home.
    studio->menubar.home = categoriesCount - 1;
    // The overflow menu sits before the bank and TIC-80; the module knows where.
    studio->menubar.more = studiomenu_more();
    studio->menubar.autohide = studio->config->data.options.autohide;
    studio->menubar.compact = studio->config->data.options.compactBar;
    studio->menubar.tabs = &categories[0];
    studio->menubar.tabIcon = tic_icon_tab;
#endif

    if(studio->config->data.uiScale > maxscale)
    {
        printf("Overriding specified uiScale of %i; the maximum your screen will accommodate is %i", studio->config->data.uiScale, maxscale);
        studio->config->data.uiScale = maxscale;
    }

    studio->start = start_create(&(StartDeps){.tic = studio->tic, .config = getConfig(studio)});

    boot_init(&studio->boot, &(BootDeps){
                                 .host = studio,
                                 .tic = studio->tic,
                                 .loaded = romLoaded,
                             },
              args.cart);

    initRunMode(studio);

#if defined(BUILD_EDITORS)
    // The cart the command line named is the host's to load: the console only
    // hears, through its launch flag, that the launch came with one.
    if(args.cart)
    {
        if(studioLoadCart(studio, args.cart))
            studio->boot.cart = true;
        else
        {
            printf("error: cart `%s` not loaded\n", args.cart);
            exit(1);
        }
    }

    studio->console = console_create(&(ConsoleDeps){
        .tic = studio->tic,
        .fs = studio->fs,
        .net = studio->net,
        .config = getConfig(studio),
        .mouse = studio->mouse.state,
        .view = &studio->consoleView,

        .args =
            {
                .cmd = args.cmd,
                .cli = args.cli,
                .skip = args.skip,
                .keepcmd = args.keepcmd,
            },

        .boot = studio->boot.cart,
    });
#else
    // No console: whatever is on the command line is loaded right here, and
    // played once the splash hands the transition over.
    if(args.cart)
    {
        if(studioLoadCart(studio, args.cart))
            studio->boot.cart = true;
        else
        {
            fprintf(stderr, "error: cart `%s` not loaded\n", args.cart);
            exit(1);
        }
    }
#endif
#if defined(BUILD_SURF)
    initSurfMode(studio);
#endif
#if defined(BUILD_EDITORS)
    initModules(studio);
#endif

    if(args.scale)
        studio->config->data.uiScale = args.scale;

    if(args.volume >= 0)
        studio->config->data.options.volume = args.volume & 0x0f;

#if defined(CRT_SHADER_SUPPORT)
    studio->config->data.options.crt |= args.crt;
#endif

    studio->config->data.options.fullscreen |= args.fullscreen;
    studio->config->data.options.vsync |= args.vsync;
    studio->config->data.soft |= args.soft;
    studio->config->data.cli |= args.cli;

#if defined(BUILD_EDITORS)
    if(args.codeexport)
        studio->bytebattle.exp = strdup(args.codeexport);
    else if(args.codeimport)
        studio->bytebattle.imp = strdup(args.codeimport);

    studio->bytebattle.delay = args.delay;
    studio->bytebattle.limit.lower = args.lowerlimit;

    studio->bytebattle.limit.current = studio->bytebattle.limit.upper = args.upperlimit;

    studio->bytebattle.battle.left = studio->bytebattle.battle.time = args.battletime * 60 * 1000;

    if(args.fftlist)
    {
        FFT_EnumerateDevices();
        exit(0);
    }
    studio->config->data.fft = args.fft;
    studio->config->data.fftcaptureplaybackdevices = args.fftcaptureplaybackdevices;
    studio->config->data.fftdevice = args.fftdevice;
    studio->config->data.keyboardLayout = keyboardLayout;
#else
    (void)keyboardLayout;
#endif

    studioConfigChanged(studio);

    if(args.cli)
        args.skip = true;

#if defined(BUILD_EDITORS)
    if(args.skip)
    {
        if(getBytebattle(studio))
        {
            // The console's first frame, drawn before the code editor takes
            // over: the mode is still the start screen's, so the tick greets
            // and loads the demo without running any command.
            studio->consoleView = (ConsoleView){.cartChanged = studioCartChanged(studio)};

            console_tick(studio->console);
            serveConsole(studio);

            gotoCode(studio);
        }
        else
            studioGoto(studio, TIC_CONSOLE_MODE, StudioGoScreen);
    }
#endif

    return studio;
}
