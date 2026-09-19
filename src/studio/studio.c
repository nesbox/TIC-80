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

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif

#if defined(BUILD_EDITORS)
#include "editors/code.h"
#include "editors/sprite.h"
#include "editors/map.h"
#include "editors/world.h"
#include "editors/sfx.h"
#include "editors/music.h"
#include "ext/history.h"
#include "wave_writer.h"
#include "ext/gif.h"
#define MSF_GIF_IMPL
#include "ext/msf_gif.h"

#include "../fftdata.h"
#include "ext/fft.h"
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
#include "screens/console.h"
#endif

#if defined(BUILD_SURF)
#include "screens/surf.h"
#include "net.h"
#endif

#include "ext/md5.h"
#include "config.h"
#include "cart.h"
#include "screens/start.h"
#include "screens/run.h"
#include "screens/menu.h"
#include "screens/mainmenu.h"

#include "fs.h"

#include "argparse.h"
#include "tic_assert.h"

#include <ctype.h>

#define _USE_MATH_DEFINES
#include <math.h>

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

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
typedef struct
{
    u8 data[MD5_HASHSIZE];
} CartHash;

// The editor tabs are the machine's ring now (sm_editor_ring) — the studio
// only draws them and walks them; SM_EDITOR_RING_COUNT is what the layout
// arithmetic below counts with.

static const EditorMode BankModes[] =
{
    TIC_SPRITE_MODE,
    TIC_MAP_MODE,
    TIC_SFX_MODE,
    TIC_MUSIC_MODE,
};

#endif

typedef struct
{
    bool down;
    bool click;

    struct
    {
        s32 start;
        s32 ticks;
        bool click;
    } dbl;

    tic_point start;
    tic_point end;


} MouseState;

// A dialog on screen: who raised it, what to call when it is answered, and the
// data that callback wants. Declared up here because the studio keeps the one
// it is showing (Studio.dialogData), and it is the answer path that frees it.
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
typedef struct
{
    Studio* studio;
    ConfirmCallback callback;
    void* data;
} ConfirmData;
#endif

struct Studio
{
    tic_mem* tic;

    // Set when the studio has been told to quit (the exit dialog, a window
    // close, the console's `exit`); every platform loop reads it.
    bool quit_requested;

    // Every mode question — which screen is on, where a run came from, who
    // asked for it, where a menu goes back to — is this one state, and
    // machine.c is the only thing that changes it (see studioSend).
    SmState machine;

    struct
    {
        MouseState state[3];
    } mouse;

#if defined(BUILD_EDITORS)
    ViMode viMode;
#endif
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    struct
    {
        CartHash hash;
        u64 mdate;
    }cart;

    // The dialog on screen, if any, and what to call when it is answered. The
    // menu widget holds the same pointer for its own handlers, but an answer
    // raised in the menu rebuilds those rows on its way out and the widget's
    // data goes with them — so the executor keeps its own pointer, and the
    // callback outlives the menu it was raised in.
    ConfirmData* dialogData;
#endif
#if defined(BUILD_EDITORS)
    struct
    {
        bool show;
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

            s8 indexes[COUNT_OF(BankModes)];
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
        char text[STUDIO_TEXT_BUFFER_WIDTH];
    } tooltip;

    struct
    {
        bool record;
        bool screenshot;

        u32* buffer;
        s32 frame;

        MsfGifState gif;

    } video;

    Code*       code;

    struct
    {
        Sprite* sprite[TIC_EDITOR_BANKS];
        Map*    map[TIC_EDITOR_BANKS];
        Sfx*    sfx[TIC_EDITOR_BANKS];
        Music*  music[TIC_EDITOR_BANKS];
    } banks;

    World*      world;
    Bytebattle bytebattle;
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    Console*    console;
#endif

#if defined(BUILD_SURF)
    Surf*       surf;
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    tic_net* net;
#endif

    Start*      start;
    Run*        run;
    Menu*       menu;
    Config*     config;

    StudioMainMenu* mainmenu;

    tic_fs* fs;
    s32 samplerate;
    tic_font systemFont;

};

static void emptyDone(void* data) {}

void fadePalette(tic_palette* pal, s32 value)
{
    for(u8 *i = pal->data, *end = i + sizeof(tic_palette); i < end; i++)
        *i = *i * value >> 8;
}

void map2ram(tic_ram* ram, const tic_map* src)
{
    memcpy(ram->map.data, src, sizeof ram->map);
}

void tiles2ram(tic_ram* ram, const tic_tiles* src)
{
    memcpy(ram->tiles.data, src, sizeof ram->tiles * TIC_SPRITE_BANKS);
}

static inline void sfx2ram(tic_ram* ram, const tic_sfx* src)
{
    memcpy(&ram->sfx, src, sizeof ram->sfx);
}

static inline void music2ram(tic_ram* ram, const tic_music* src)
{
    memcpy(&ram->music, src, sizeof ram->music);
}

s32 calcWaveAnimation(tic_mem* tic, u32 offset, s32 channel)
{
    const tic_sound_register* reg = &tic->ram->registers[channel];

    s32 val = tic_tool_noise(&reg->waveform)
        ? (rand() & 1) * MAX_VOLUME
        : tic_tool_peek4(reg->waveform.data, ((offset * tic_sound_register_get_freq(reg)) >> 7) % WAVE_VALUES);

    return val * reg->volume;
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

    if(wave_open( studio->samplerate, path ))
    {

#if TIC80_SAMPLE_CHANNELS == 2
        wave_enable_stereo();
#endif

        const tic_sfx* sfx = getSfxSrc(studio);

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, getMusicSrc(studio));

        {
            const tic_sample* effect = &sfx->samples.data[index];

            enum{Channel = 0};
            sfx_stop(tic, Channel);
            tic_api_sfx(tic, index, effect->note, effect->octave, -1, Channel, MAX_VOLUME, MAX_VOLUME, SFX_DEF_SPEED);

            for(s32 ticks = 0, pos = 0; pos < SFX_TICKS; pos = tic_tool_sfx_pos(effect->speed, ++ticks))
            {
                tic_core_tick_start(tic);
                tic_core_tick_end(tic);
                tic_core_synth_sound(tic);

                wave_write(tic->product.samples.buffer, tic->product.samples.count);
            }

            sfx_stop(tic, Channel);
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

    if(wave_open( studio->samplerate, path ))
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
                    studio->bank.indexes[i] = bank;
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

            for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
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

void sfx_stop(tic_mem* tic, s32 channel)
{
    tic_api_sfx(tic, -1, 0, 0, -1, channel, MAX_VOLUME, MAX_VOLUME, SFX_DEF_SPEED);
}

char getKeyboardText(Studio* studio)
{
    tic_mem* tic = studio->tic;

    // Ctrl-based shortcuts should not also inject printable characters.
    if(tic_api_key(tic, tic_key_ctrl) && !tic_api_key(tic, tic_key_alt))
        return '\0';

    char text;
    if(!tic_sys_keyboard_text(&text))
    {
        tic80_input* input = &tic->ram->input;

#ifdef KEYBOARD_LAYOUT_ES
        // US KEYS:                     " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
        static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789'!`+cn'o,.- ";
        static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ=!\" $%&/()??^*CN\"a;:_ ";
        static const char Alt[] =       "                            |@#        []} {\\    ";
#else
        static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
        static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";
        static const char Alt[] =       "                                                  ";
#endif

        enum{Count = sizeof Symbols};

        for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        {
            tic_key key = input->keyboard.keys[i];

            if(key > 0 && key < Count && tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            {
                bool caps = tic_api_key(tic, tic_key_capslock);
                bool shift = tic_api_key(tic, tic_key_shift);
                bool alt = tic_api_key(tic, tic_key_alt);

                if (caps && key >= tic_key_a && key <= tic_key_z) shift = !shift;

                return shift ? Shift[key]
                    : alt ? Alt[key]
                    : Symbols[key];
            }
        }

        return '\0';
    }

    return text;
}

bool keyWasPressed(Studio* studio, tic_key key)
{
    tic_mem* tic = studio->tic;
    return tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool enterWasPressed(Studio* studio)
{
    tic_mem* tic = studio->tic;
    return ticEnterWasPressed(tic, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool ticEnterWasPressed(tic_mem* tic, s32 hold, s32 period)
{
    return tic_api_keyp(tic, tic_key_return, hold, period) ||
           tic_api_keyp(tic, tic_key_numpadenter, hold, period);
}


bool anyKeyWasPressed(Studio* studio)
{
    tic_mem* tic = studio->tic;

    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
    {
        tic_key key = tic->ram->input.keyboard.keys[i];

        if(tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            return true;
    }

    return false;
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
    const tic_sample* effect = &studio->config->cart->bank0.sfx.samples.data[id];
    tic_api_sfx(studio->tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, MAX_VOLUME, effect->speed);
}

static void md5(const void* voidData, s32 length, u8 digest[MD5_HASHSIZE])
{
    enum {Size = 512};

    const u8* data = voidData;

    MD5_CTX c;
    MD5_Init(&c);

    while (length > 0)
    {
        MD5_Update(&c, data, length > Size ? Size: length);

        length -= Size;
        data += Size;
    }

    MD5_Final(digest, &c);
}

const char* md5str(const void* data, s32 length)
{
    static char res[MD5_HASHSIZE * 2 + 1];

    u8 digest[MD5_HASHSIZE];

    md5(data, length, digest);

    for (s32 n = 0; n < MD5_HASHSIZE; ++n)
        snprintf(res + n*2, sizeof("ff"), "%02x", digest[n]);

    return res;
}

static u8* getSpritePtr(tic_tile* tiles, s32 x, s32 y)
{
    enum { SheetCols = (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE) };
    return tiles[x / TIC_SPRITESIZE + y / TIC_SPRITESIZE * SheetCols].data;
}


void setSpritePixel(tic_tile* tiles, s32 x, s32 y, u8 color)
{
    // TODO: check spritesheet rect
    tic_tool_poke4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE, color);
}

u8 getSpritePixel(tic_tile* tiles, s32 x, s32 y)
{
    // TODO: check spritesheet rect
    return tic_tool_peek4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE);
}

#if defined(BUILD_EDITORS)
void toClipboard(const void* data, s32 size, bool flip)
{
    if(data)
    {
        enum {Len = 2};

        char* clipboard = (char*)malloc(size*Len + 1);

        if(clipboard)
        {
            char* ptr = clipboard;

            tic_tool_buf2str(data, size, clipboard, flip);
            tic_sys_clipboard_set(clipboard);
            free(clipboard);
        }
    }
}

static void removeWhiteSpaces(char* str)
{
    s32 i = 0;
    s32 len = (s32)strlen(str);

    for (s32 j = 0; j < len; j++)
        if(!isspace(str[j]))
            str[i++] = str[j];

    str[i] = '\0';
}

bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces, bool sameSize)
{
    if(tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        SCOPE(tic_sys_clipboard_free(clipboard))
        {
            if (remove_white_spaces)
                removeWhiteSpaces(clipboard);

            bool valid = sameSize
                ? strlen(clipboard) == size * 2
                : strlen(clipboard) <= size * 2;

            if(valid) tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, flip);

            return valid;
        }
    }

    return false;
}

void showTooltip(Studio* studio, const char* text)
{
    strncpy(studio->tooltip.text, text, sizeof studio->tooltip.text - 1);
}

static void drawExtrabar(Studio* studio, tic_mem* tic)
{
    enum {Size = 7};

    s32 x = (SM_EDITOR_RING_COUNT + 1) * Size + 17 * TIC_FONT_WIDTH;
    s32 y = 0;

    static struct Icon {u8 id; StudioEvent event; const char* tip;} Icons[] =
    {
        {tic_icon_cut,      TIC_TOOLBAR_CUT,    "CUT [ctrl+x]"},
        {tic_icon_copy,     TIC_TOOLBAR_COPY,   "COPY [ctrl+c]"},
        {tic_icon_paste,    TIC_TOOLBAR_PASTE,  "PASTE [ctrl+v]"},
        {tic_icon_undo,     TIC_TOOLBAR_UNDO,   "UNDO [ctrl+z]"},
        {tic_icon_redo,     TIC_TOOLBAR_REDO,   "REDO [ctrl+y]"},
    };

    u8 color = tic_color_red;
    FOR(const struct Icon*, icon, Icons)
    {
        tic_rect rect = {x, y, Size, Size};

        u8 bg = tic_color_white;
        u8 fg = tic_color_light_grey;

        if(checkMousePos(studio, &rect))
        {
            setCursor(studio, tic_cursor_hand);

            fg = color;
            showTooltip(studio, icon->tip);

            if(checkMouseDown(studio, &rect, tic_mouse_left))
            {
                bg = fg;
                fg = tic_color_white;
            }
            else if(checkMouseClick(studio, &rect, tic_mouse_left))
            {
                setStudioEvent(studio, icon->event);
            }
        }

        tic_api_rect(tic, x, y, Size, Size, bg);
        drawBitIcon(studio, icon->id, x, y, fg);

        x += Size;
        color++;
    }
}

struct Sprite* getSpriteEditor(Studio* studio)
{
    return studio->banks.sprite[studio->bank.index.sprites];
}
#endif

const StudioConfig* studio_config(Studio* studio)
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

struct Start* getStartScreen(Studio* studio)
{
    return studio->start;
}

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)

static void drawBankIcon(Studio* studio, s32 x, s32 y)
{
    tic_mem* tic = studio->tic;

    tic_rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

    bool over = false;
    EditorMode mode = 0;

    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == getStudioMode(studio))
        {
            mode = i;
            break;
        }

    if(checkMousePos(studio, &rect))
    {
        setCursor(studio, tic_cursor_hand);

        over = true;

        showTooltip(studio, "SWITCH BANK");

        if(checkMouseClick(studio, &rect, tic_mouse_left))
            studio->bank.show = !studio->bank.show;
    }

    if(studio->bank.show)
    {
        drawBitIcon(studio, tic_icon_bank, x, y, tic_color_red);

        enum{Size = TOOLBAR_SIZE};

        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            tic_rect rect = {x + 2 + (i+1)*Size, 0, Size, Size};

            bool over = false;
            if(checkMousePos(studio, &rect))
            {
                setCursor(studio, tic_cursor_hand);
                over = true;

                if(checkMouseClick(studio, &rect, tic_mouse_left))
                {
                    if(studio->bank.chained)
                        memset(studio->bank.indexes, i, sizeof studio->bank.indexes);
                    else studio->bank.indexes[mode] = i;
                }
            }

            if(i == studio->bank.indexes[mode])
                tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_red);

            tic_api_print(tic, (char[]){'0' + i, '\0'}, rect.x+1, rect.y+1,
                i == studio->bank.indexes[mode]
                    ? tic_color_white
                    : over
                        ? tic_color_red
                        : tic_color_light_grey,
                false, 1, false);

        }

        {
            tic_rect rect = {x + 4 + (TIC_EDITOR_BANKS+1)*Size, 0, Size, Size};

            bool over = false;

            if(checkMousePos(studio, &rect))
            {
                setCursor(studio, tic_cursor_hand);

                over = true;

                if(checkMouseClick(studio, &rect, tic_mouse_left))
                {
                    studio->bank.chained = !studio->bank.chained;

                    if(studio->bank.chained)
                        memset(studio->bank.indexes, studio->bank.indexes[mode], sizeof studio->bank.indexes);
                }
            }

            drawBitIcon(studio, tic_icon_pin, rect.x, rect.y, studio->bank.chained ? tic_color_red : over ? tic_color_grey : tic_color_light_grey);
        }
    }
    else
    {
        drawBitIcon(studio, tic_icon_bank, x, y, over ? tic_color_red : tic_color_light_grey);
    }
}

#endif

static inline s32 lerp(s32 a, s32 b, float d)
{
    return (b - a) * d + a;
}

static inline float bounceOut(float x)
{
    const float n1 = 7.5625;
    const float d1 = 2.75;

    if (x < 1 / d1) return n1 * x * x;
    else if (x < 2 / d1)
    {
        x -= 1.5 / d1;
        return n1 * x * x + 0.75;
    }
    else if (x < 2.5 / d1)
    {
        x -= 2.25 / d1;
        return n1 * x * x + 0.9375;
    }
    else
    {
        x -= 2.625 / d1;
        return n1 * x * x + 0.984375;
    }
}

static inline float animEffect(AnimEffect effect, float x)
{
    const float c1 = 1.70158;
    const float c2 = c1 * 1.525;
    const float c3 = c1 + 1;
    const float c4 = (2 * M_PI) / 3;
    const float c5 = (2 * M_PI) / 4.5;

    switch(effect)
    {
    case AnimLinear:
        return x;

    case AnimEaseIn:
        return x * x;

    case AnimEaseOut:
        return 1 - (1 - x) * (1 - x);

    case AnimEaseInOut:
        return x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2;

    case AnimEaseInCubic:
        return x * x * x;

    case AnimEaseOutCubic:
        return 1 - pow(1 - x, 3);

    case AnimEaseInOutCubic:
        return x < 0.5 ? 4 * x * x * x : 1 - pow(-2 * x + 2, 3) / 2;

    case AnimEaseInQuart:
        return x * x * x * x;

    case AnimEaseOutQuart:
        return 1 - pow(1 - x, 4);

    case AnimEaseInOutQuart:
        return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;

    case AnimEaseInQuint:
        return x * x * x * x * x;

    case AnimEaseOutQuint:
        return 1 - pow(1 - x, 5);

    case AnimEaseInOutQuint:
        return x < 0.5 ? 16 * x * x * x * x * x : 1 - pow(-2 * x + 2, 5) / 2;

    case AnimEaseInSine:
        return 1 - cos((x * M_PI) / 2);

    case AnimEaseOutSine:
        return sin((x * M_PI) / 2);

    case AnimEaseInOutSine:
        return -(cos(M_PI * x) - 1) / 2;

    case AnimEaseInExpo:
        return x == 0 ? 0 : pow(2, 10 * x - 10);

    case AnimEaseOutExpo:
        return x == 1 ? 1 : 1 - pow(2, -10 * x);

    case AnimEaseInOutExpo:
        return x == 0
            ? 0
            : x == 1
                ? 1
                : x < 0.5
                    ? pow(2, 20 * x - 10) / 2
                    : (2 - pow(2, -20 * x + 10)) / 2;

    case AnimEaseInCirc:
        return 1 - sqrt(1 - pow(x, 2));

    case AnimEaseOutCirc:
        return sqrt(1 - pow(x - 1, 2));

    case AnimEaseInOutCirc:
        return x < 0.5
            ? (1 - sqrt(1 - pow(2 * x, 2))) / 2
            : (sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2;

    case AnimEaseInBack:
        return c3 * x * x * x - c1 * x * x;

    case AnimEaseOutBack:
        return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2);

    case AnimEaseInOutBack:
        return x < 0.5
            ? (pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
            : (pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2;

    case AnimEaseInElastic:
        return x == 0
            ? 0
            : x == 1
                ? 1
                : -pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4);

    case AnimEaseOutElastic:
        return x == 0
            ? 0
            : x == 1
                ? 1
                : pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1;

    case AnimEaseInOutElastic:
        return x == 0
            ? 0
            : x == 1
                ? 1
                : x < 0.5
                    ? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
                    : (pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1;

    case AnimEaseInBounce:
        return 1 - bounceOut(1 - x);

    case AnimEaseOutBounce:
        return bounceOut(x);

    case AnimEaseInOutBounce:
        return x < 0.5
            ? (1 - bounceOut(1 - 2 * x)) / 2
            : (1 + bounceOut(2 * x - 1)) / 2;
    }

    return x;
}

static void animTick(Movie* movie)
{
    for(Anim* it = movie->items, *end = it + movie->count; it != end; ++it)
    {
	float tick = (float)(movie->tick < it->time ? movie->tick : it->time);
        *it->value = lerp(it->start, it->end, animEffect(it->effect, tick / it->time));
    }
}

void processAnim(Movie* movie, void* data)
{
    animTick(movie);

    if(movie->tick == movie->time)
        movie->done(data);

    movie->tick++;
}

Movie* resetMovie(Movie* movie)
{
    movie->tick = 0;
    animTick(movie);

    return movie;
}

#if defined(BUILD_EDITORS)

static void drawPopup(Studio* studio)
{
    if(studio->anim.movie != &studio->anim.idle)
    {
        enum{Width = TIC80_WIDTH, Height = TIC_FONT_HEIGHT + 1};

        tic_api_rect(studio->tic, 0, studio->anim.pos.popup, Width, Height, tic_color_red);
        tic_api_print(studio->tic, studio->popup.message,
            (s32)(Width - strlen(studio->popup.message) * TIC_FONT_WIDTH)/2,
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

void drawToolbar(Studio* studio, tic_mem* tic, bool bg)
{
    if(bg)
        tic_api_rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    enum {Size = 7};

    static const u8 Icons[] = {tic_icon_code, tic_icon_sprite, tic_icon_map, tic_icon_sfx, tic_icon_music};
    static const char* Tips[] = {"CODE EDITOR [f1]", "SPRITE EDITOR [f2]", "MAP EDITOR [f3]", "SFX EDITOR [f4]", "MUSIC EDITOR [f5]",};

    s32 mode = -1;

    const EditorMode* modes = sm_editor_ring();

    for(s32 i = 0; i < SM_EDITOR_RING_COUNT; i++)
    {
        tic_rect rect = {i * Size, 0, Size, Size};

        bool over = false;

        if(checkMousePos(studio, &rect))
        {
            setCursor(studio, tic_cursor_hand);

            over = true;

            showTooltip(studio, Tips[i]);

            if(checkMouseClick(studio, &rect, tic_mouse_left))
                studio->machine.pending_mode = modes[i];
        }

        if(getStudioMode(studio) == modes[i]) mode = i;

        if (mode == i)
        {
            drawBitIcon(studio, tic_icon_tab, i * Size, 0, tic_color_grey);
            drawBitIcon(studio, Icons[i], i * Size, 1, tic_color_black);
        }

        drawBitIcon(studio, Icons[i], i * Size, 0, mode == i ? tic_color_white : (over ? tic_color_grey : tic_color_light_grey));
    }

    if(mode >= 0) drawExtrabar(studio, tic);

    static const char* Names[] =
    {
        "CODE EDITOR",
        "SPRITE EDITOR",
        "MAP EDITOR",
        "SFX EDITOR",
        "MUSIC EDITOR",
    };

#if defined (TIC80_PRO) && defined(BUILD_EDITORS)
    enum {TextOffset = (SM_EDITOR_RING_COUNT + 2) * Size - 2};
    if(mode >= 1)
        drawBankIcon(studio, SM_EDITOR_RING_COUNT * Size + 2, 0);
#else
    enum {TextOffset = (SM_EDITOR_RING_COUNT + 1) * Size};
#endif

    if(mode == 0 || (mode >= 1 && !studio->bank.show))
    {
        if(strlen(studio->tooltip.text))
        {
            tic_api_print(tic, studio->tooltip.text, TextOffset, 1, tic_color_dark_grey, false, 1, false);
        }
        else
        {
            tic_api_print(tic, Names[mode], TextOffset, 1, tic_color_grey, false, 1, false);
        }
    }
}

void setStudioEvent(Studio* studio, StudioEvent event)
{
    switch(getStudioMode(studio))
    {
    case TIC_CODE_MODE:
        {
            Code* code = studio->code;
            code->event(code, event);
        }
        break;
    case TIC_SPRITE_MODE:
        {
            Sprite* sprite = studio->banks.sprite[studio->bank.index.sprites];
            sprite->event(sprite, event);
        }
    break;
    case TIC_MAP_MODE:
        {
            Map* map = studio->banks.map[studio->bank.index.map];
            map->event(map, event);
        }
        break;
    case TIC_SFX_MODE:
        {
            Sfx* sfx = studio->banks.sfx[studio->bank.index.sfx];
            sfx->event(sfx, event);
        }
        break;
    case TIC_MUSIC_MODE:
        {
            Music* music = studio->banks.music[studio->bank.index.music];
            music->event(music, event);
        }
        break;
    default: break;
    }
}

ClipboardEvent getClipboardEvent(Studio* studio)
{
    tic_mem* tic = studio->tic;

    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    if(ctrl)
    {
        if(keyWasPressed(studio, tic_key_insert) || keyWasPressed(studio, tic_key_c)) return TIC_CLIPBOARD_COPY;
        else if(keyWasPressed(studio, tic_key_x)) return TIC_CLIPBOARD_CUT;
        else if(keyWasPressed(studio, tic_key_v)) return TIC_CLIPBOARD_PASTE;
    }
    else if(shift)
    {
        if(keyWasPressed(studio, tic_key_delete)) return TIC_CLIPBOARD_CUT;
        else if(keyWasPressed(studio, tic_key_insert)) return TIC_CLIPBOARD_PASTE;
    }

    return TIC_CLIPBOARD_NONE;
}

static void showPopupMessage(Studio* studio, const char* text)
{
    memset(studio->popup.message, '\0', sizeof studio->popup.message);
    strncpy(studio->popup.message, text, sizeof(studio->popup.message) - 1);

    for(char* c = studio->popup.message; c < studio->popup.message + sizeof studio->popup.message; c++)
        if(*c) *c = toupper(*c);

    studio->anim.movie = resetMovie(&studio->anim.show);
}
#endif

static void exitConfirm(Studio* studio, bool yes, void* data)
{
    studio->quit_requested = yes;
}

void studio_exit(Studio* studio)
{
#if defined(BUILD_EDITORS)
    if(getStudioMode(studio) != TIC_START_MODE && studioCartChanged(studio))
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
    tic_mem* tic = studio->tic;

    const tic_tile* tile = &getConfig(studio)->cart->bank0.tiles.data[id];

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

static void initRunMode(Studio* studio)
{
    initRun(studio->run,
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        studio->console,
#else
        NULL,
#endif
        studio->fs, studio);
}

#if defined(BUILD_EDITORS)
static void initWorldMap(Studio* studio)
{
    initWorld(studio->world, studio, studio->banks.map[studio->bank.index.map]);
}
#endif

// Forward declarations: the executor reaches for two helpers that are defined
// further down the file, and the mutators between here and it drive the machine
// through studioSend.
static void studioSend(Studio* studio, const SmEvent* event);
static void rebuildMainMenu(Studio* studio);
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
static void answerDialog(Studio* studio, bool yes);
#endif
#if defined(BUILD_EDITORS)
static bool showGameMenu(Studio* studio);
#endif

#if defined(BUILD_EDITORS)

void gotoCode(Studio* studio)
{
    setStudioMode(studio, TIC_CODE_MODE);
}
#endif

#if defined(BUILD_SURF)
static void initSurfMode(Studio* studio)
{
    initSurf(studio->surf, studio, studio->console);
}

void gotoSurf(Studio* studio)
{
    // The browser is built by the effect, not by this function: which screens
    // need waking up when a mode is entered is the machine's answer (it is the
    // same one the switch to SURF used to carry).
    SmEvent event = { .kind = SM_EV_OPEN_SURF };
    studioSend(studio, &event);
}
#endif

bool studio_is_cart_loaded(Studio* studio)
{
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    return strlen(studio->console->rom.name) > 0 || (studio->start && studio->start->embed);
#else
    // Editorless builds (export stubs) have no console; only an embedded
    // cartridge counts as loaded.
    return studio->start && studio->start->embed;
#endif
}

// Who owns ESC this frame. The machine cannot look inside the code editor, so
// the executor answers the question and the machine decides what it means.
static SmEscapeGate studioEscapeGate(Studio* studio)
{
#if defined(BUILD_EDITORS)
    if(getConfig(studio)->options.keybindMode == KEYBIND_VI
        && getStudioViMode(studio) != VI_NORMAL)
        return SM_ESC_GATE_VI;

    if(getStudioMode(studio) == TIC_CODE_MODE && studio->code->mode != TEXT_EDIT_MODE)
        return SM_ESC_GATE_CODE;
#endif

    return SM_ESC_GATE_OPEN;
}

// The world the machine reads, gathered once per dispatch. Every field is a
// question the studio already knows how to answer; a build without the editors
// simply has fewer of them to answer.
static SmEnv studioEnv(Studio* studio)
{
    SmEnv env =
    {
        .cart_loaded = studio_is_cart_loaded(studio),
        .has_game_menu = false,
        .menu_has_back = studio_menu_has_back(studio->menu),
        .keepcmd_replay = false,
        .escape_gate = studioEscapeGate(studio),
    };

#if defined(BUILD_EDITORS)
    env.has_game_menu = showGameMenu(studio);

    env.keepcmd_replay = studio->console->args.keepcmd
        && studio->console->commands.count
        && studio->console->commands.current >= studio->console->commands.count;
#endif

    return env;
}

// The effects. Everything here used to sit inside setStudioMode's switch or in
// a caller that poked the fields itself; now the machine decides and this is
// the only place that touches the screens.
static void runEffect(Studio* studio, const SmResult* result, SmEffect effect)
{
    switch(effect)
    {
    case SM_EFF_PAUSE_CORE:  tic_core_pause(studio->tic); break;
    case SM_EFF_RESET_CORE:  tic_api_reset(studio->tic); break;
    case SM_EFF_RESUME_CORE: tic_core_resume(studio->tic); break;
    case SM_EFF_INIT_RUN:    initRunMode(studio); break;

    case SM_EFF_REBUILD_MAINMENU: rebuildMainMenu(studio); break;

    case SM_EFF_MENU_BACK_ANIM: studio_menu_back(studio->menu); break;

    case SM_EFF_DIALOG_CALLBACK:
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        answerDialog(studio, result->yes);
#endif
        break;

    case SM_EFF_EXIT_STUDIO: studio_exit(studio); break;

    case SM_EFF_INIT_SURF:
#if defined(BUILD_SURF)
        initSurfMode(studio);
#endif
        break;

    case SM_EFF_SURF_RESUME:
#if defined(BUILD_SURF)
        studio->surf->resume(studio->surf);
#endif
        break;

    case SM_EFF_CONSOLE_DONE:
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        studio->console->done(studio->console);
#endif
        break;

    case SM_EFF_INIT_WORLD:
#if defined(BUILD_EDITORS)
        initWorldMap(studio);
#endif
        break;

    case SM_EFF_MUSIC_NEXT_TAB:
#if defined(BUILD_EDITORS)
        {
            Music* music = studio->banks.music[studio->bank.index.music];
            music->tab = (music->tab + 1) % MUSIC_TAB_COUNT;
        }
#endif
        break;

    case SM_EFF_VI_MODE_RESET:
        // Asked for in every build; a build without the editors has no vi mode
        // for it to mean anything to.
#if defined(BUILD_EDITORS)
        studio->viMode = 0;
#endif
        break;

    case SM_EFF_CODE_FOCUS:
#if defined(BUILD_EDITORS)
        studio->machine.code_focus = true;
#endif
        break;

    case SM_EFF_EDITOR_ESCAPE:
#if defined(BUILD_EDITORS)
        studio->code->escape(studio->code);
#endif
        break;

    case SM_EFF_COUNT:
        break;
    }
}

// Every transition there is goes through here: read the world, ask the machine
// what to do, do it. TIC80_TRACE_MODES=1 prints each one, which is what makes a
// before and after of this rewrite comparable.
static void traceTransition(const SmResult* result)
{
    static int enabled = -1;

    if(enabled < 0)
        enabled = getenv("TIC80_TRACE_MODES") != NULL;

    if(!enabled)
        return;

    // A frame that decided nothing is not a transition: the empty per-frame
    // event would otherwise print one line per frame of the session.
    if(!result->moved && !result->effects.count)
        return;

    printf("[mode] %s -> %s", sm_mode_name(result->from), sm_mode_name(result->to));

    for(u8 i = 0; i < result->effects.count; ++i)
        printf(" %s", sm_effect_name(result->effects.items[i]));

    // The trace is read from a pipe as often as from a terminal, and stdout is
    // block-buffered there: a session that ends abruptly would lose its tail.
    printf("\n");
    fflush(stdout);
}

static void studioSend(Studio* studio, const SmEvent* event)
{
    SmEnv env = studioEnv(studio);
    SmResult result = sm_dispatch(&studio->machine, &env, event);

    for(u8 i = 0; i < result.effects.count; ++i)
        runEffect(studio, &result, result.effects.items[i]);

    traceTransition(&result);
}

void setStudioMode(Studio* studio, EditorMode mode)
{
    SmEvent event = { .kind = SM_EV_REQUEST_MODE, .mode = mode };
    studioSend(studio, &event);
}

EditorMode getStudioMode(Studio* studio)
{
    return studio->machine.mode;
}

#if defined(BUILD_EDITORS)
static void changeStudioMode(Studio* studio, s32 dir)
{
    SmEvent event = { .kind = SM_EV_CYCLE_EDITOR, .dir = dir };
    studioSend(studio, &event);
}
#endif

void resumeGame(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_RESUME_GAME };
    studioSend(studio, &event);
}

#if defined(BUILD_EDITORS)
void setStudioViMode(Studio* studio, ViMode mode) {
    studio->viMode = mode;
}

ViMode getStudioViMode(Studio* studio) {
    return studio->viMode;
}

bool checkStudioViMode(Studio* studio, ViMode mode) {
    return (
        getConfig(studio)->options.keybindMode == KEYBIND_VI
        && getStudioViMode(studio) == mode
    );
}
#endif

static inline bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
    return (pt->x >= rect->x)
        && (pt->x < (rect->x + rect->w))
        && (pt->y >= rect->y)
        && (pt->y < (rect->y + rect->h));
}

bool checkMousePos(Studio* studio, const tic_rect* rect)
{
    tic_point pos = tic_api_mouse(studio->tic);
    return pointInRect(&pos, rect);
}

bool checkMouseClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &studio->mouse.state[button];

    bool value = state->click
        && pointInRect(&state->start, rect)
        && pointInRect(&state->end, rect);

    if(value) state->click = false;

    return value;
}

bool checkMouseDblClick(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &studio->mouse.state[button];

    bool value = state->dbl.click
        && pointInRect(&state->start, rect)
        && pointInRect(&state->end, rect);

    if(value) state->dbl.click = false;

    return value;
}

bool checkMouseDown(Studio* studio, const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &studio->mouse.state[button];

    return state->down && pointInRect(&state->start, rect);
}

void setCursor(Studio* studio, tic_cursor id)
{
    tic_mem* tic = studio->tic;

    VBANK(tic, 0)
    {
        tic->ram->vram.vars.cursor.sprite = id;
    }
}

// The main menu's rows belong to the menu widget, so whoever puts them back —
// gotoMenu, and a dialog that was raised in the menu and is now answered —
// rebuilds them here.
static void rebuildMainMenu(Studio* studio)
{
    studio_mainmenu_free(studio->mainmenu);
    studio->mainmenu = studio_mainmenu_init(studio->menu, studio->config);
}

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)

// A dialog's rows answer it: the machine decides where an answer goes and what
// else it means, and the callback rides along as an effect.
// The answer, handed to whoever raised the dialog. Ran as an effect, after the
// screen it goes back to is back, because that is where the callback expects to
// be told — the same order confirmHandler called it in.
static void answerDialog(Studio* studio, bool yes)
{
    ConfirmData* dialog = studio->dialogData;

    studio->dialogData = NULL;

    if(dialog)
    {
        dialog->callback(studio, yes, dialog->data);
        free(dialog);
    }
}

static void confirmNo(void* data, s32 pos)
{
    ConfirmData* dialog = data;
    SmEvent event = { .kind = SM_EV_CONFIRM_ANSWERED, .yes = false };
    studioSend(dialog->studio, &event);
}

static void confirmYes(void* data, s32 pos)
{
    ConfirmData* dialog = data;
    SmEvent event = { .kind = SM_EV_CONFIRM_ANSWERED, .yes = true };
    studioSend(dialog->studio, &event);
}

void confirmDialog(Studio* studio, const char** text, s32 rows, ConfirmCallback callback, void* data)
{
    // The machine records where the answer goes back to — see the same rule in
    // machine.c, which is where it lives now.
    SmEvent event = { .kind = SM_EV_CONFIRM_OPENED };
    studioSend(studio, &event);

    static MenuItem Answers[] =
    {
        {"",    NULL},
        {"(N)O",  confirmNo},
        {"(Y)ES", confirmYes},
    };

    Answers[1].hotkey = tic_key_n;
    Answers[2].hotkey = tic_key_y;

    s32 count = rows + COUNT_OF(Answers);
    MenuItem* items = malloc(sizeof items[0] * count);
    SCOPE(free(items))
    {
        for(s32 i = 0; i != rows; ++i)
            items[i] = (MenuItem){text[i], NULL};

        memcpy(items + rows, Answers, sizeof Answers);

        studio->dialogData = MOVE((ConfirmData){studio, callback, data});

        // The dialog's back is its own answer "no": ESC (and the gamepad's B)
        // means the same thing as picking it. Passing NULL here left ESC to
        // the fallback in processShortcuts, which asks prevMode where to go —
        // for a dialog raised over a run that is RUN, remapped to CONSOLE, so
        // ESC abandoned the run and never reached the callback at all.
        studio_menu_init(studio->menu, items, count, count - 2, 0,
            confirmNo, studio->dialogData);

        playSystemSfx(studio, 0);
    }
}

#if defined(BUILD_EDITORS)
static void resetBanks(Studio* studio)
{
    memset(studio->bank.indexes, 0, sizeof studio->bank.indexes);
}
#endif

static void initModules(Studio* studio)
{
#if defined(BUILD_EDITORS)
    tic_mem* tic = studio->tic;

    resetBanks(studio);

    initCode(studio->code, studio);

    for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
    {
        initSprite(studio->banks.sprite[i], studio, &tic->cart.banks[i].tiles);
        initMap(studio->banks.map[i], studio, &tic->cart.banks[i].map);
        initSfx(studio->banks.sfx[i], studio, &tic->cart.banks[i].sfx);
        initMusic(studio->banks.music[i], studio, &tic->cart.banks[i].music);
    }

    initWorldMap(studio);
#endif
}

static void updateHash(Studio* studio)
{
    md5(&studio->tic->cart, sizeof(tic_cartridge), studio->cart.hash.data);
}

static void updateMDate(Studio* studio)
{
    studio->cart.mdate = fs_date(studio->console->rom.path);
}
#endif

static void updateTitle(Studio* studio)
{
    char name[TICNAME_MAX] = TIC_TITLE;

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    if(strlen(studio->console->rom.name))
        snprintf(name, TICNAME_MAX, "%s [%s]", TIC_TITLE, studio->console->rom.name);
#endif

    tic_sys_title(name);
}

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)

bool project_ext(const char* name)
{
    FOREACH_LANG(script)
    {
        if(tic_tool_has_ext(name, script->fileExtension))
            return true;
    }

    return false;
}

void studioRomSaved(Studio* studio)
{
    updateTitle(studio);
    updateHash(studio);
    updateMDate(studio);
}

void studioRomLoaded(Studio* studio)
{
    initModules(studio);

    updateTitle(studio);
    updateHash(studio);
    updateMDate(studio);
}

bool studioCartChanged(Studio* studio)
{
    CartHash hash;
    if (!studio_is_cart_loaded(studio)) return false;

    md5(&studio->tic->cart, sizeof(tic_cartridge), hash.data);

    return memcmp(hash.data, studio->cart.hash.data, sizeof(CartHash)) != 0;
}
#endif

void runGame(Studio* studio, RunOrigin origin)
{
    bool replay = false;

#if defined(BUILD_EDITORS)

    if (studio->config->data.fft) {
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

    // --keepcmd with the queue exhausted: no run at all, the commands are
    // replayed and the console comes back. The machine reads the queue to
    // decide that (env.keepcmd_replay), so it is asked *before* the rewind
    // below — rewinding first would hide the replay from it.
    replay = studio->console->args.keepcmd
        && studio->console->commands.count
        && studio->console->commands.current >= studio->console->commands.count;
#endif

    // One reset per run, done here because that is where it has always been:
    // a cart asked for from the pause menu is a restart of the same run, and
    // the switch into RUN never reset anything.
    if(!replay)
        tic_api_reset(studio->tic);

    // The machine owns every rule the rest of this function used to spell out:
    // the origin, the owner, the restart of the run the menu sits over.
    SmEvent event = { .kind = SM_EV_RUN_GAME, .origin = origin };
    studioSend(studio, &event);

#if defined(BUILD_EDITORS)
    if(replay)
        studio->console->commands.current = 0;
#endif
}

#if defined(BUILD_EDITORS)
void saveProject(Studio* studio)
{
    if(getConfig(studio)->trim) trimWhitespace(studio->code);

    CartSaveResult rom = studio->console->save(studio->console);

    if(rom == CART_SAVE_OK)
    {
        char buffer[STUDIO_TEXT_BUFFER_WIDTH];
        char str_saved[] = " saved :)";

        s32 name_len = (s32)strlen(studio->console->rom.name);
        if (name_len + strlen(str_saved) > sizeof(buffer)){
            char subbuf[sizeof(buffer) - sizeof(str_saved) - 5];
            memset(subbuf, '\0', sizeof subbuf);
            strncpy(subbuf, studio->console->rom.name, sizeof subbuf-1);

            snprintf(buffer, sizeof buffer, "%s[...]%s", subbuf, str_saved);
        }
        else
        {
            snprintf(buffer, sizeof buffer, "%s%s", studio->console->rom.name, str_saved);
        }

        showPopupMessage(studio, buffer);
    }
    else if(rom == CART_SAVE_MISSING_NAME) showPopupMessage(studio, "error: missing cart name :(");
    else showPopupMessage(studio, "error: file not saved :(");
}

static void setCoverImage(Studio* studio)
{
    tic_mem* tic = studio->tic;

    if(getStudioMode(studio) == TIC_RUN_MODE)
    {
        tic_api_sync(tic, tic_sync_screen, 0, true);
        showPopupMessage(studio, "cover image saved :)");
    }
}

static void generateScreenshotName(Studio* studio, const char* extension, char filenameOut[TICNAME_MAX])
{
    const char* romName = studio->console->rom.name;

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
    size_t maxBaseLen = TICNAME_MAX
                      - strlen(timestamp)
                      - strlen(extension)
                      - 1; // for null terminator

    if (baseLen > maxBaseLen)
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
    if(tic_fs_save(studio->fs, filename, result.data, result.dataSize, true))
    {
        char msg[TICNAME_MAX];
        sprintf(msg, "%s saved :)", filename);
        showPopupMessage(studio, msg);

        tic_sys_open_path(tic_fs_path(studio->fs, filename));
    }
    else showPopupMessage(studio, "error: file not saved :(");

    msf_gif_free(result);

    studio->video.record = false;
}

static void startVideoRecord(Studio* studio)
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

static void takeScreenshot(Studio* studio)
{
    startVideoRecord(studio);
    studio->video.screenshot = true;
}
#endif

static inline bool keyWasPressedOnce(Studio* studio, s32 key)
{
    tic_mem* tic = studio->tic;

    return tic_api_keyp(tic, key, -1, -1);
}

static void gotoFullscreen(Studio* studio)
{
    tic_sys_fullscreen_set(studio->config->data.options.fullscreen = !tic_sys_fullscreen_get());
}

#if defined(CRT_SHADER_SUPPORT)
static void switchCrtMonitor(Studio* studio)
{
    studio->config->data.options.crt = !studio->config->data.options.crt;
}
#endif

#if defined(BUILD_EDITORS)
static u32 getTime()
{
    return tic_sys_counter_get() * 1000 / tic_sys_freq_get();
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
        if(BankModes[i] == getStudioMode(studio))
        {
            if(studio->bank.chained)
                memset(studio->bank.indexes, bank, sizeof studio->bank.indexes);
            else studio->bank.indexes[i] = bank;
            break;
        }
}

#endif

void gotoMenu(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_OPEN_MENU };
    studioSend(studio, &event);
}

// ESC in a dev run, and the back of the pause menu over one: the run was asked
// for from the studio, so it is left the way it was entered — one press, and
// no walk to CLOSE GAME (#2937).
void leaveRun(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_LEAVE_RUN };
    studioSend(studio, &event);
}

void studioMenuBack(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_MENU_BACK };
    studioSend(studio, &event);
}

void studioRunExited(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_RUN_EXITED };
    studioSend(studio, &event);
}

void studioRunErrored(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_RUN_ERRORED };
    studioSend(studio, &event);
}

static bool enterWasPressedOnce(Studio* studio)
{
    return keyWasPressedOnce(studio, tic_key_return) ||
           keyWasPressedOnce(studio, tic_key_numpadenter);
}

#if defined(BUILD_EDITORS)
// These three are to fix the f1 hotkey conflict

void setJustSwitchedToCodeMode(Studio* studio, bool value)
{
    studio->machine.code_focus = value;
}

bool hasJustSwitchedToCodeMode(Studio* studio)
{
    return studio->machine.code_focus;
}
#endif

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

    if(getStudioMode(studio) == TIC_START_MODE) return;

#if defined(BUILD_EDITORS)
    if(!studio->console->active) return;
#endif

    if(studio_mainmenu_keyboard(studio->mainmenu)) return;

    bool alt = tic_api_key(tic, tic_key_alt);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

#if defined(CRT_SHADER_SUPPORT)
    if(keyWasPressedOnce(studio, tic_key_f6)) switchCrtMonitor(studio);
#endif

    if(alt)
    {
        if (enterWasPressedOnce(studio)) gotoFullscreen(studio);
#if defined(BUILD_EDITORS)
        else if(getStudioMode(studio) != TIC_RUN_MODE && studio->config->data.keyboardLayout != tic_layout_azerty)
        {
#ifndef KEYBOARD_LAYOUT_ES
            if(keyWasPressedOnce(studio, tic_key_grave)) setStudioMode(studio, TIC_CONSOLE_MODE);
            else if(keyWasPressedOnce(studio, tic_key_1))
            {
                SmEvent event = { .kind = SM_EV_REQUEST_CODE_FOCUS };
                studioSend(studio, &event);
            }
            else if(keyWasPressedOnce(studio, tic_key_2)) setStudioMode(studio, TIC_SPRITE_MODE);
            else if(keyWasPressedOnce(studio, tic_key_3)) setStudioMode(studio, TIC_MAP_MODE);
            else if(keyWasPressedOnce(studio, tic_key_4)) setStudioMode(studio, TIC_SFX_MODE);
            else if(keyWasPressedOnce(studio, tic_key_5)) setStudioMode(studio, TIC_MUSIC_MODE);
#endif
        }
#endif
    }
    else if(ctrl)
    {
        if(keyWasPressedOnce(studio, tic_key_q)) studio_exit(studio);
#if defined(BUILD_EDITORS)
        else if(keyWasPressedOnce(studio, tic_key_pageup)) changeStudioMode(studio, -1);
        else if(keyWasPressedOnce(studio, tic_key_pagedown)) changeStudioMode(studio, +1);
        else if(enterWasPressedOnce(studio)) runGame(studio, RUN_FROM_STUDIO);
        else if(keyWasPressedOnce(studio, tic_key_r)) runGame(studio, RUN_FROM_STUDIO);
        else if(keyWasPressedOnce(studio, tic_key_s)) saveProject(studio);
#endif

#if defined(TIC80_PRO) && defined(BUILD_EDITORS)

        else
            for(s32 bank = 0; bank < TIC_BANKS; bank++)
                if(keyWasPressedOnce(studio, tic_key_0 + bank))
                    switchBank(studio, bank);

#endif

    }
    else
    {
        if (keyWasPressedOnce(studio, tic_key_f11)) gotoFullscreen(studio);
#if defined(BUILD_EDITORS)
        else if(keyWasPressedOnce(studio, tic_key_escape))
        {
            SmEvent escape = { .kind = SM_EV_ESCAPE };

            // A vi mode out of normal, or a popup inside the code editor, owns
            // ESC — and both ended the frame here before: ask the machine for
            // whatever that case has to say, and stop.
            if(studioEscapeGate(studio) != SM_ESC_GATE_OPEN)
            {
                studioSend(studio, &escape);
                return;
            }

            // Everything else — the menu's back, a run's owner, the editor each
            // screen falls back to — is the machine's ladder now.
            studioSend(studio, &escape);
        }
        else if(keyWasPressedOnce(studio, tic_key_f8)) takeScreenshot(studio);
        else if(keyWasPressedOnce(studio, tic_key_f9)) startVideoRecord(studio);
        else if(keyWasPressedOnce(studio, tic_key_f10)) hideBattleTime(studio);
        else if(keyWasPressedOnce(studio, tic_key_f12)) startBattle(studio);
        else if(getStudioMode(studio) == TIC_RUN_MODE && keyWasPressedOnce(studio, tic_key_f7))
            setCoverImage(studio);

        // A running game owns the function keys: a player's run always hands
        // them over, and a cart that declares a game menu asks for them in a
        // dev's run as well. Leaving a run is ESC's job (see leaveRun), so the
        // studio has no use for them there.
        if((!studio->machine.player_run && !showGameMenu(studio)) || getStudioMode(studio) != TIC_RUN_MODE)
        {
            if(keyWasPressedOnce(studio, tic_key_f1))
            {
                SmEvent event = { .kind = SM_EV_REQUEST_CODE_FOCUS };
                studioSend(studio, &event);
            }
            else if(keyWasPressedOnce(studio, tic_key_f2)) setStudioMode(studio, TIC_SPRITE_MODE);
            else if(keyWasPressedOnce(studio, tic_key_f3)) setStudioMode(studio, TIC_MAP_MODE);
            else if(keyWasPressedOnce(studio, tic_key_f4)) setStudioMode(studio, TIC_SFX_MODE);
            else if(keyWasPressedOnce(studio, tic_key_f5)) setStudioMode(studio, TIC_MUSIC_MODE);
        }
#else
        else if(keyWasPressedOnce(studio, tic_key_escape))
        {
            SmEvent escape = { .kind = SM_EV_ESCAPE };
            studioSend(studio, &escape);
        }
#endif
    }
}

#if defined(BUILD_EDITORS)
static void reloadConfirm(Studio* studio, bool yes, void* data)
{
    if(yes)
        studio->console->updateProject(studio->console);
    else
        updateMDate(studio);
}

static void checkChanges(Studio* studio)
{
    switch(getStudioMode(studio))
    {
    case TIC_START_MODE:
        break;
    default:
        {
            Console* console = studio->console;

            u64 date = fs_date(console->rom.path);

            if(studio->cart.mdate && date > studio->cart.mdate)
            {
                if(studioCartChanged(studio) && getStudioMode(studio) != TIC_MENU_MODE)
                {
                    static const char* Rows[] =
                    {
                        "WARNING!",
                        "The cart has changed!",
                        "Do you want to reload it?"
                    };

                    confirmDialog(studio, Rows, COUNT_OF(Rows), reloadConfirm, NULL);
                }
                else console->updateProject(console);
            }
        }
    }
}

static void drawBitIconRaw(Studio* studio, u32* frame, s32 sx, s32 sy, s32 id, tic_color color)
{
    const tic_bank* bank = &getConfig(studio)->cart->bank0;

    u32 *dst = frame + sx + sy * TIC80_FULLWIDTH;
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

            u32 *ptr = studio->video.buffer;
            const u32 *src = pixels;
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

// What only the studio can supply per mode: the tick itself, and the scanline
// callback the frame is composited with. Everything else a mode means — its
// sound source, how it treats input, the vbank wipe, the palette — is data in
// machine.c (sm_mode_policy), which is what lets the tests cover all eleven.
static void tickStart(Studio* studio)   { studio->start->tick(studio->start); }
static void tickRun(Studio* studio)     { studio->run->tick(studio->run); }
static void tickMenu(Studio* studio)    { studio_menu_tick(studio->menu); }

#if defined(BUILD_EDITORS)
static void tickConsole(Studio* studio) { studio->console->tick(studio->console); }
static void tickCode(Studio* studio)    { studio->code->tick(studio->code); }
static void tickWorld(Studio* studio)   { studio->world->tick(studio->world); }

static void tickSprite(Studio* studio)
{
    Sprite* sprite = studio->banks.sprite[studio->bank.index.sprites];
    sprite->tick(sprite);
}

static void tickMap(Studio* studio)
{
    Map* map = studio->banks.map[studio->bank.index.map];
    map->tick(map);
}

static void tickSfx(Studio* studio)
{
    Sfx* sfx = studio->banks.sfx[studio->bank.index.sfx];
    sfx->tick(sfx);
}

static void tickMusic(Studio* studio)
{
    Music* music = studio->banks.music[studio->bank.index.music];
    music->tick(music);
}
#endif

#if defined(BUILD_SURF)
static void tickSurf(Studio* studio)    { studio->surf->tick(studio->surf); }
#endif

static void (*const Dispatch[TIC_MODES_COUNT])(Studio*) =
{
    [TIC_START_MODE]    = tickStart,
    [TIC_RUN_MODE]      = tickRun,
    [TIC_MENU_MODE]     = tickMenu,
#if defined(BUILD_EDITORS)
    [TIC_CONSOLE_MODE]  = tickConsole,
    [TIC_CODE_MODE]     = tickCode,
    [TIC_SPRITE_MODE]   = tickSprite,
    [TIC_MAP_MODE]      = tickMap,
    [TIC_WORLD_MODE]    = tickWorld,
    [TIC_SFX_MODE]      = tickSfx,
    [TIC_MUSIC_MODE]    = tickMusic,
#endif
#if defined(BUILD_SURF)
    [TIC_SURF_MODE]     = tickSurf,
#endif
};

// The screens that draw themselves into vbank 1 with a per-row callback hand
// one over here; the rest are composited with a plain blit.
static tic_blit_callback studioScanline(Studio* studio)
{
    switch(getStudioMode(studio))
    {
    case TIC_MENU_MODE:
        return (tic_blit_callback){studio_menu_anim_scanline, NULL, NULL, studio->menu};

#if defined(BUILD_EDITORS)
    case TIC_SPRITE_MODE:
        {
            Sprite* sprite = studio->banks.sprite[studio->bank.index.sprites];
            return (tic_blit_callback){sprite->scanline, NULL, NULL, sprite};
        }
    case TIC_MAP_MODE:
        {
            Map* map = studio->banks.map[studio->bank.index.map];
            return (tic_blit_callback){map->scanline, NULL, NULL, map};
        }
    case TIC_WORLD_MODE:
        return (tic_blit_callback){studio->world->scanline, NULL, NULL, studio->world};
#endif
#if defined(BUILD_SURF)
    case TIC_SURF_MODE:
        return (tic_blit_callback){studio->surf->scanline, NULL, NULL, studio->surf};
#endif
    case TIC_START_MODE:
    case TIC_RUN_MODE:
#if defined(BUILD_EDITORS)
    case TIC_CONSOLE_MODE:
    case TIC_CODE_MODE:
    case TIC_SFX_MODE:
    case TIC_MUSIC_MODE:
#else
    // Screens an editorless build cannot be in, named so the list stays
    // complete whichever way the build goes.
    case TIC_CONSOLE_MODE:
    case TIC_CODE_MODE:
    case TIC_SPRITE_MODE:
    case TIC_MAP_MODE:
    case TIC_WORLD_MODE:
    case TIC_SFX_MODE:
    case TIC_MUSIC_MODE:
#if !defined(BUILD_SURF)
    case TIC_SURF_MODE:                         // no browser in this build
#endif
#endif
    case TIC_MODES_COUNT:
        break;                                  // drawn with a plain blit
    }

    return (tic_blit_callback){ 0 };
}

static void renderStudio(Studio* studio)
{
    tic_mem* tic = studio->tic;

#if defined(BUILD_EDITORS)
    showTooltip(studio, "");
#endif

    {
        const tic_sfx* sfx = NULL;
        const tic_music* music = NULL;

        switch(sm_mode_policy(getStudioMode(studio)).sound)
        {
        case SM_SOUND_RAM:
            sfx = &studio->tic->ram->sfx;
            music = &studio->tic->ram->music;
            break;

        case SM_SOUND_CONFIG:
            sfx = &studio->config->cart->bank0.sfx;
            music = &studio->config->cart->bank0.music;
            break;

        case SM_SOUND_BANKS:
#if defined(BUILD_EDITORS)
            sfx = getSfxSrc(studio);
            music = getMusicSrc(studio);
#else
            // An editorless build has no banks and no mode that plays from
            // them: the two cases above cover every screen it has.
            assert(false);
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

    // The studio draws into vbank 1 and wipes it first — every screen but the
    // game, whose frame is the cart's own.
    if(sm_mode_policy(getStudioMode(studio)).clear_vbank1)
    {
        VBANK(tic, 1)
        {
            tic_api_cls(tic, 0);
        }
    }

    assert(Dispatch[getStudioMode(studio)]);
    Dispatch[getStudioMode(studio)](studio);

    tic_core_tick_end(tic);

    switch(sm_mode_policy(getStudioMode(studio)).input)
    {
    case SM_INPUT_KEEP:
        break;                                  // the cart owns its input

    case SM_INPUT_CLEAR:
        tic->input.data = -1;
        break;

    case SM_INPUT_CLEAR_PAD:
        tic->input.data = -1;
        tic->input.gamepad = 0;
        break;
    }
}

static void updateSystemFont(Studio* studio)
{
    tic_mem* tic = studio->tic;

    studio->systemFont = (tic_font)
    {
        .regular =
        {
	    { 0 },
	    {
		{
		    .width       = TIC_FONT_WIDTH,
		    .height      = TIC_FONT_HEIGHT,
		},
	    },
        },
        .alt =
        {
	    { 0 },
	    {
		{
		    .width       = TIC_ALTFONT_WIDTH,
		    .height      = TIC_FONT_HEIGHT,
		},
	    },
        }
    };

    u8* dst = (u8*)&studio->systemFont;

    for(s32 i = 0; i < TIC_FONT_CHARS * 2; i++)
        for(s32 y = 0; y < TIC_SPRITESIZE; y++)
            for(s32 x = 0; x < TIC_SPRITESIZE; x++)
                if(tic_tool_peek4(&studio->config->cart->bank0.sprites.data[i], TIC_SPRITESIZE*y + x))
                    dst[i*BITS_IN_BYTE+y] |= 1 << x;

    tic->ram->font = studio->systemFont;
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
    tic_sys_update_config();
}

static void processMouseStates(Studio* studio)
{
    for(s32 i = 0; i < COUNT_OF(studio->mouse.state); i++)
        studio->mouse.state[i].dbl.click = studio->mouse.state[i].click = false;

    tic_mem* tic = studio->tic;

    tic->ram->vram.vars.cursor.sprite = tic_cursor_arrow;
    tic->ram->vram.vars.cursor.system = true;

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

        if(getStudioMode(studio) != TIC_RUN_MODE)
        {
            codeGetPos(studio->code, &x, &y);
            x++; y++;
        }

        sprintf(pos, "-- pos: %i,%i\n", x, y);
    }

    if(strcmp(studio->bytebattle.last.postag, pos) || strcmp(studio->bytebattle.last.code.data, studio->code->src))
    {
        FILE* file = fopen(studio->bytebattle.exp, "wb");

        if(file)
        {
            strcpy(studio->bytebattle.last.postag, pos);
            strcpy(studio->bytebattle.last.code.data, studio->code->src);

            fwrite(pos, 1, strlen(pos), file);
            fwrite(studio->code->src, 1, strlen(studio->code->src), file);
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
            enum{TagSize = sizeof PosTag - 1};

            if(memcmp(code.data, PosTag, TagSize) == 0)
            {
                char* start = code.data + TagSize;
                char* sep = strchr(start, ',');

                if(sep)
                {
                    *sep = *end = '\0';
                    s32 x = atoi(start);
                    s32 y = atoi(sep + 1);

                    if(x == 0 && y == 0)
                    {
                        if(getStudioMode(studio) != TIC_RUN_MODE)
                            runGame(studio, RUN_FROM_STUDIO);
                    }
                    else
                    {
                        s32 offset = end - code.data + 1;
                        memcpy(studio->code->src, code.data + offset, sizeof(tic_code) - offset);
                        codeSetPos(studio->code, x - 1, y - 1);

                        if(getStudioMode(studio) == TIC_RUN_MODE)
                            setStudioMode(studio, TIC_CODE_MODE);
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

    if(tic->input.mouse && !m->relative && (s32)m->x < TIC80_FULLWIDTH && (s32)m->y < TIC80_FULLHEIGHT)
    {
        s32 sprite = CLAMP(tic->ram->vram.vars.cursor.sprite, 0, TIC_BANK_SPRITES - 1);
        const tic_bank* bank = &tic->cart.bank0;

        tic_point hot = {0};

        if(tic->ram->vram.vars.cursor.system)
        {
            bank = &getConfig(studio)->cart->bank0;
            hot = (tic_point[])
            {
                {0, 0},
                {3, 0},
                {2, 3},
            }[CLAMP(sprite, 0, 2)];
        }
        else if(sprite == 0) return;

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
#if defined(BUILD_SURF)
    tic_net_start(studio->net);
#endif

    {
        // The toolbar's click lands here, one frame after the click itself.
        SmEvent event = { .kind = SM_EV_FRAME };
        studioSend(studio, &event);
    }

    processMouseStates(studio);
    renderStudio(studio);

    {
#if defined(BUILD_EDITORS)
        Sprite* sprite = studio->banks.sprite[studio->bank.index.sprites];
        Map* map = studio->banks.map[studio->bank.index.map];
#endif

        if(sm_mode_policy(getStudioMode(studio)).config_palette)
        {
            memcpy(tic->ram->vram.palette.data, getConfig(studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));
            tic->ram->font = studio->systemFont;
        }

        {
            tic_blit_callback callback = studioScanline(studio);

            callback.data
                ? tic_core_blit_ex(tic, callback)
                : tic_core_blit(tic);
        }

        blitCursor(studio);

#if defined(BUILD_EDITORS)
        if(isRecordFrame(studio))
            recordFrame(studio, tic->product.screen);

        drawPopup(studio);
#endif
    }

#if defined(BUILD_SURF)
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
        for(s16* it = tic->product.samples.buffer, *end = it + size; it != end; ++it)
            *it = *it * volume / MAX_VOLUME;
    }
}

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
static void onStudioLoadConfirmed(Studio* studio, bool yes, void* data)
{
    if(yes)
    {
        const char* file = data;
#if defined(BUILD_EDITORS)
        showPopupMessage(studio, studio->console->loadCart(studio->console, file)
            ? "cart successfully loaded :)"
            : "error: cart not loaded :(");
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

#endif

void studio_load(Studio* studio, const char* file)
{
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    studioCartChanged(studio)
        ? confirmLoadCart(studio, onStudioLoadConfirmed, (void*)file)
        : onStudioLoadConfirmed(studio, true, (void*)file);
#endif
}

// Where a run was entered from decides where CLOSE GAME goes: a cart played in
// SURF returns to the browser it was opened in, any other run lands in the
// console as it always has. That origin is runFrom, not prevMode — opening the
// pause menu over the run makes RUN the previous mode, so prevMode has already
// forgotten SURF by the time the menu is answered (#3015).
void exitGame(Studio* studio)
{
    SmEvent event = { .kind = SM_EV_CLOSE_GAME };
    studioSend(studio, &event);
}

void studio_delete(Studio* studio)
{
    {
#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            freeSprite  (studio->banks.sprite[i]);
            freeMap     (studio->banks.map[i]);
            freeSfx     (studio->banks.sfx[i]);
            freeMusic   (studio->banks.music[i]);
        }

        freeCode    (studio->code);
        freeConsole (studio->console);
        freeWorld   (studio->world);

        FREE(studio->anim.show.items);
        FREE(studio->anim.hide.items);

#endif
#if defined(BUILD_SURF)
        freeSurf    (studio->surf);
#endif


        freeStart   (studio->start);
        freeRun     (studio->run);
        freeConfig  (studio->config);

        studio_mainmenu_free(studio->mainmenu);
        studio_menu_free(studio->menu);
    }

    tic_core_close(studio->tic);

#if defined(BUILD_SURF)
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

#if defined(BUILD_EDITORS)
Bytebattle* getBytebattle(Studio* studio)
{
    return studio->bytebattle.exp || studio->bytebattle.imp ? &(studio->bytebattle) : NULL;
}
#endif

static StartArgs parseArgs(s32 argc, char **argv)
{
    static const char *const usage[] =
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
#undef  CMD_PARAMS_DEF
#if defined(BUILD_EDITORS)
        OPT_GROUP("Byte battle options:\n"),
        OPT_STRING('\0',    "codeexport",    &args.codeexport,   "export code to filename"),
        OPT_STRING('\0',    "codeimport",    &args.codeimport,   "import code from filename"),
        OPT_INTEGER('\0',   "delay",         &args.delay,        "codeexport / codeimport update interval in ticks"),
        OPT_INTEGER('\0',   "lowerlimit",    &args.lowerlimit,   "lower limit for code size (256 by default)"),
        OPT_INTEGER('\0',   "upperlimit",    &args.upperlimit,   "upper limit for code size (512 by default)"),
        OPT_INTEGER('\0',   "battletime",    &args.battletime,   "battletime in minutes"),
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

bool studio_quit_requested(Studio* studio)
{
    return studio->quit_requested;
}

#if defined(TIC_MODULE_EXT)
static bool onEnumModule(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    if(strstr(name, TIC_MODULE_EXT))
    {
        void* module = dlopen(name, RTLD_NOW | RTLD_LOCAL);

        if(module)
        {
            const tic_script *config = dlsym(module, DEF2STR(SCRIPT_CONFIG));

            if(config)
            {
                tic_add_script(config);
            }
            else
            {
                dlclose(module);
            }
        }
    }

    return true;
}
#endif

Studio* studio_create(s32 argc, char **argv, s32 samplerate, tic80_pixel_color_format format, const char* folder, s32 maxscale, tic_layout keyboardLayout)
{
    setbuf(stdout, NULL);

    StartArgs args = parseArgs(argc, argv);

    if (args.version)
    {
        printf("%s\n", TIC_VERSION);
        exit(0);
    }

#if defined(TIC_MODULE_EXT)
    // load script modules
    fs_enum(fs_appfolder(), onEnumModule, NULL);
#endif

    Studio* studio = NEW(Studio);
    *studio = (Studio)
    {
#if defined(BUILD_EDITORS)

        .bank =
        {
            .chained = true,
        },

        .anim =
        {
            .pos =
            {
                .popup = -TOOLBAR_SIZE,
            },
            .idle = {.done = emptyDone,}
        },

        .popup =
        {
            .message = "\0",
        },

        .tooltip =
        {
            .text = "\0",
        },
#endif
#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        .samplerate = samplerate,
#endif
#if defined(BUILD_SURF)
        .net = tic_net_create(TIC_WEBSITE),
#endif
#if defined(BUILD_EDITORS)
        .bytebattle = {0},
#endif
        .tic = tic_core_create(samplerate, format),
    };

    {
        const char *path = args.fs ? args.fs : folder;

        if (fs_isdir(path))
        {
            studio->fs = tic_fs_create(path,
#if defined(BUILD_SURF)
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
            studio->banks.sprite[i]   = calloc(1, sizeof(Sprite));
            studio->banks.map[i]      = calloc(1, sizeof(Map));
            studio->banks.sfx[i]      = calloc(1, sizeof(Sfx));
            studio->banks.music[i]    = calloc(1, sizeof(Music));
        }

        studio->code       = calloc(1, sizeof(Code));
        studio->world      = calloc(1, sizeof(World));

        studio->anim.show = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setPopupWait,
        {
            {-TOOLBAR_SIZE, 0, STUDIO_ANIM_TIME, &studio->anim.pos.popup, AnimEaseIn},
        });

        studio->anim.wait = (Movie){.time = TIC80_FRAMERATE * 2, .done = setPopupHide};
        studio->anim.hide = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
        {
            {0, -TOOLBAR_SIZE, STUDIO_ANIM_TIME, &studio->anim.pos.popup, AnimEaseIn},
        });

        studio->anim.movie = resetMovie(&studio->anim.idle);
#endif

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
        studio->console    = calloc(1, sizeof(Console));
#endif

#if defined(BUILD_SURF)
        studio->surf       = calloc(1, sizeof(Surf));
#endif

        studio->start      = calloc(1, sizeof(Start));
        studio->run        = calloc(1, sizeof(Run));
        studio->menu       = studio_menu_create(studio);
        studio->config     = calloc(1, sizeof(Config));
    }
    studio->mainmenu = NULL;
    tic_fs_makedir(studio->fs, TIC_LOCAL);
    tic_fs_makedir(studio->fs, TIC_LOCAL_VERSION);

    initConfig(studio->config, studio, studio->fs);

    if (studio->config->data.uiScale > maxscale)
    {
        printf("Overriding specified uiScale of %i; the maximum your screen will accommodate is %i", studio->config->data.uiScale, maxscale);
        studio->config->data.uiScale = maxscale;
    }

    initStart(studio->start, studio, args.cart);
    initRunMode(studio);

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    initConsole(studio->console, studio, studio->fs, studio->net, studio->config, args);
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
    studio->config->data.options.crt        |= args.crt;
#endif

    studio->config->data.options.fullscreen |= args.fullscreen;
    studio->config->data.options.vsync      |= args.vsync;
    studio->config->data.soft               |= args.soft;
    studio->config->data.cli                |= args.cli;

#if defined(BUILD_EDITORS)
    if(args.codeexport)
        studio->bytebattle.exp = strdup(args.codeexport);
    else if(args.codeimport)
        studio->bytebattle.imp = strdup(args.codeimport);

    studio->bytebattle.delay = args.delay;
    studio->bytebattle.limit.lower = args.lowerlimit;

    studio->bytebattle.limit.current
        = studio->bytebattle.limit.upper
        = args.upperlimit;

    studio->bytebattle.battle.left
        = studio->bytebattle.battle.time
        = args.battletime * 60 * 1000;

    if (args.fftlist)
    {
        FFT_EnumerateDevices();
        exit(0);
    }
    studio->config->data.fft = args.fft;
    studio->config->data.fftcaptureplaybackdevices = args.fftcaptureplaybackdevices;
    studio->config->data.fftdevice = args.fftdevice;
    studio->config->data.keyboardLayout = keyboardLayout;
#endif

    // The mode machinery starts where a session has always started: the start
    // screen, with the code editor behind it and the player owning the run.
    sm_init(&studio->machine, sm_build_features());

    studioConfigChanged(studio);

    if(args.cli)
        args.skip = true;

#if defined(BUILD_EDITORS)
    if(args.skip)
    {
        if(getBytebattle(studio))
        {
            studio->console->tick(studio->console);
            gotoCode(studio);
        }
        else
            setStudioMode(studio, TIC_CONSOLE_MODE);
    }
#endif

    return studio;
}
