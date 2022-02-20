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
#include "studio_impl.h"

#if defined(BUILD_EDITORS)

#include "editors/code.h"
#include "editors/sprite.h"
#include "editors/map.h"
#include "editors/world.h"
#include "editors/sfx.h"
#include "editors/music.h"
#include "screens/console.h"
#include "screens/surf.h"
#include "ext/history.h"
#include "net.h"
#include "wave_writer.h"
#include "ext/gif.h"

#endif

#include "ext/md5.h"
#include "screens/start.h"
#include "screens/run.h"
#include "screens/menu.h"
#include "screens/main_menu.h"
#include "config.h"
#include "cart.h"

#include "fs.h"

#include "argparse.h"

#include <ctype.h>

#define _USE_MATH_DEFINES
#include <math.h>

#if defined(BUILD_EDITORS)

#define FRAME_SIZE (TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32))

static const char VideoGif[] = "video%i.gif";
static const char ScreenGif[] = "screen%i.gif";

#endif

static void emptyDone(void* data) {}

StudioImplementation impl =
{
    .tic80local = NULL,

    .mode = TIC_START_MODE,
    .prevMode = TIC_CODE_MODE,

#if defined(BUILD_EDITORS)
    .cart = 
    {
        .mdate = 0,
    },

    .menuMode = TIC_CONSOLE_MODE,

    .bank = 
    {
        .show = false,
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

    .video =
    {
        .record = false,
        .buffer = NULL,
        .frames = 0,
    },
#endif
};

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

    s32 val = FLAT4(reg->waveform.data)
        ? (rand() & 1) * MAX_VOLUME
        : tic_tool_peek4(reg->waveform.data, ((offset * reg->freq) >> 7) % WAVE_VALUES);

    return val * reg->volume;
}

#if defined(BUILD_EDITORS)
static const tic_sfx* getSfxSrc()
{
    tic_mem* tic = impl.studio.tic;
    return &tic->cart.banks[impl.bank.index.sfx].sfx;
}

static const tic_music* getMusicSrc()
{
    tic_mem* tic = impl.studio.tic;
    return &tic->cart.banks[impl.bank.index.music].music;
}

const char* studioExportSfx(s32 index, const char* filename)
{
    tic_mem* tic = impl.studio.tic;

    const char* path = tic_fs_path(impl.fs, filename);

    if(wave_open( impl.samplerate, path ))
    {

#if TIC_STEREO_CHANNELS == 2
        wave_enable_stereo();
#endif

        const tic_sfx* sfx = getSfxSrc();

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, getMusicSrc());

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

                wave_write(tic->samples.buffer, tic->samples.size / sizeof(s16));
            }

            sfx_stop(tic, Channel);
            memset(tic->ram->registers, 0, sizeof(tic_sound_register));
        }

        wave_close();

        return path;
    }

    return NULL;
}

const char* studioExportMusic(s32 track, const char* filename)
{
    tic_mem* tic = impl.studio.tic;

    const char* path = tic_fs_path(impl.fs, filename);

    if(wave_open( impl.samplerate, path ))
    {
#if TIC_STEREO_CHANNELS == 2
        wave_enable_stereo();
#endif

        const tic_sfx* sfx = getSfxSrc();
        const tic_music* music = getMusicSrc();

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, music);

        const tic_music_state* state = &tic->ram->music_state;
        const Music* editor = impl.banks.music[impl.bank.index.music];

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

            wave_write(tic->samples.buffer, tic->samples.size / sizeof(s16));

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

char getKeyboardText()
{
    char text;
    if(!tic_sys_keyboard_text(&text))
    {
        tic_mem* tic = impl.studio.tic;
        tic80_input* input = &tic->ram->input;

        static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
        static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";

        enum{Count = sizeof Symbols};

        for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        {
            tic_key key = input->keyboard.keys[i];

            if(key > 0 && key < Count && tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            {
                bool caps = tic_api_key(tic, tic_key_capslock);
                bool shift = tic_api_key(tic, tic_key_shift);

                return caps
                    ? key >= tic_key_a && key <= tic_key_z 
                        ? shift ? Symbols[key] : Shift[key]
                        : shift ? Shift[key] : Symbols[key]
                    : shift ? Shift[key] : Symbols[key];
            }
        }

        return '\0';
    }

    return text;
}

bool keyWasPressed(tic_key key)
{
    tic_mem* tic = impl.studio.tic;
    return tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool anyKeyWasPressed()
{
    tic_mem* tic = impl.studio.tic;

    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
    {
        tic_key key = tic->ram->input.keyboard.keys[i];

        if(tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            return true;
    }

    return false;
}

#if defined(BUILD_EDITORS)
tic_tiles* getBankTiles()
{
    return &impl.studio.tic->cart.banks[impl.bank.index.sprites].tiles;
}

tic_map* getBankMap()
{
    return &impl.studio.tic->cart.banks[impl.bank.index.map].map;
}

tic_palette* getBankPalette(bool vbank)
{
    tic_bank* bank = &impl.studio.tic->cart.banks[impl.bank.index.sprites];
    return vbank ? &bank->palette.vbank1 : &bank->palette.vbank0;
}

tic_flags* getBankFlags()
{
    return &impl.studio.tic->cart.banks[impl.bank.index.sprites].flags;
}
#endif

void playSystemSfx(s32 id)
{
    const tic_sample* effect = &impl.config->cart->bank0.sfx.samples.data[id];
    tic_api_sfx(impl.studio.tic, id, effect->note, effect->octave, -1, 0, MAX_VOLUME, MAX_VOLUME, effect->speed);
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

            for(s32 i = 0; i < size; i++, ptr+=Len)
            {
                sprintf(ptr, "%02x", ((u8*)data)[i]);

                if(flip)
                {
                    char tmp = ptr[0];
                    ptr[0] = ptr[1];
                    ptr[1] = tmp;
                }
            }

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

bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces)
{
    if(tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        SCOPE(tic_sys_clipboard_free(clipboard))
        {
            if (remove_white_spaces)
                removeWhiteSpaces(clipboard);

            bool valid = strlen(clipboard) <= size * 2;

            if(valid) tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, flip);

            return valid;
        }
    }

    return false;
}

void showTooltip(const char* text)
{
    strncpy(impl.tooltip.text, text, sizeof impl.tooltip.text - 1);
}

static void drawExtrabar(tic_mem* tic)
{
    enum {Size = 7};

    s32 x = (COUNT_OF(Modes) + 1) * Size + 17 * TIC_FONT_WIDTH;
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

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            fg = color;
            showTooltip(icon->tip);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                bg = fg;
                fg = tic_color_white;
            }
            else if(checkMouseClick(&rect, tic_mouse_left))
            {
                setStudioEvent(icon->event);
            }
        }

        tic_api_rect(tic, x, y, Size, Size, bg);
        drawBitIcon(icon->id, x, y, fg);

        x += Size;
        color++;
    }
}

struct Sprite* getSpriteEditor()
{
    return impl.banks.sprite[impl.bank.index.sprites];
}
#endif

const StudioConfig* getConfig()
{
    return &impl.config->data;
}

struct Start* getStartScreen()
{
    return impl.start;
}

#if defined (TIC80_PRO)

static void drawBankIcon(s32 x, s32 y)
{
    tic_mem* tic = impl.studio.tic;

    tic_rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

    bool over = false;
    EditorMode mode = 0;

    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == impl.mode)
        {
            mode = i;
            break;
        }

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        over = true;

        showTooltip("SWITCH BANK");

        if(checkMouseClick(&rect, tic_mouse_left))
            impl.bank.show = !impl.bank.show;
    }

    if(impl.bank.show)
    {
        drawBitIcon(tic_icon_bank, x, y, tic_color_red);

        enum{Size = TOOLBAR_SIZE};

        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            tic_rect rect = {x + 2 + (i+1)*Size, 0, Size, Size};

            bool over = false;
            if(checkMousePos(&rect))
            {
                setCursor(tic_cursor_hand);
                over = true;

                if(checkMouseClick(&rect, tic_mouse_left))
                {
                    if(impl.bank.chained) 
                        memset(impl.bank.indexes, i, sizeof impl.bank.indexes);
                    else impl.bank.indexes[mode] = i;
                }
            }

            if(i == impl.bank.indexes[mode])
                tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_red);

            tic_api_print(tic, (char[]){'0' + i, '\0'}, rect.x+1, rect.y+1, 
                i == impl.bank.indexes[mode] 
                    ? tic_color_white 
                    : over 
                        ? tic_color_red 
                        : tic_color_light_grey, 
                false, 1, false);

        }

        {
            tic_rect rect = {x + 4 + (TIC_EDITOR_BANKS+1)*Size, 0, Size, Size};

            bool over = false;

            if(checkMousePos(&rect))
            {
                setCursor(tic_cursor_hand);

                over = true;

                if(checkMouseClick(&rect, tic_mouse_left))
                {
                    impl.bank.chained = !impl.bank.chained;

                    if(impl.bank.chained)
                        memset(impl.bank.indexes, impl.bank.indexes[mode], sizeof impl.bank.indexes);
                }
            }

            drawBitIcon(tic_icon_pin, rect.x, rect.y, impl.bank.chained ? tic_color_red : over ? tic_color_grey : tic_color_light_grey);
        }
    }
    else
    {
        drawBitIcon(tic_icon_bank, x, y, over ? tic_color_red : tic_color_light_grey);
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

    if (x < 1 / d1)         return n1 * x * x;
    else if (x < 2 / d1)    return n1 * (x -= 1.5 / d1) * x + 0.75;
    else if (x < 2.5 / d1)  return n1 * (x -= 2.25 / d1) * x + 0.9375;
    else                    return n1 * (x -= 2.625 / d1) * x + 0.984375;
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
        *it->value = lerp(it->start, it->end, animEffect(it->effect, (float)movie->tick / it->time));
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

static void drawPopup()
{
    if(impl.anim.movie != &impl.anim.idle)
    {
        enum{Width = TIC80_WIDTH, Height = TIC_FONT_HEIGHT + 1};

        tic_api_rect(impl.studio.tic, 0, impl.anim.pos.popup, Width, Height, tic_color_red);
        tic_api_print(impl.studio.tic, impl.popup.message, 
            (s32)(Width - strlen(impl.popup.message) * TIC_FONT_WIDTH)/2,
            impl.anim.pos.popup + 1, tic_color_white, true, 1, false);

        // render popup message
        {
            tic_mem* tic = impl.studio.tic;
            const tic_bank* bank = &getConfig()->cart->bank0;
            u32* dst = tic->screen + TIC80_MARGIN_LEFT + TIC80_MARGIN_TOP * TIC80_FULLWIDTH;

            for(s32 i = 0, y = 0; y < (Height + impl.anim.pos.popup); y++, dst += TIC80_MARGIN_RIGHT + TIC80_MARGIN_LEFT)
                for(s32 x = 0; x < Width; x++)
                *dst++ = tic_rgba(&bank->palette.vbank0.colors[tic_tool_peek4(tic->ram->vram.screen.data, i++)]);
        }
    }
}

void drawToolbar(tic_mem* tic, bool bg)
{
    if(bg)
        tic_api_rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    enum {Size = 7};

    static const u8 Icons[] = {tic_icon_code, tic_icon_sprite, tic_icon_map, tic_icon_sfx, tic_icon_music};
    static const char* Tips[] = {"CODE EDITOR [f1]", "SPRITE EDITOR [f2]", "MAP EDITOR [f3]", "SFX EDITOR [f4]", "MUSIC EDITOR [f5]",};

    s32 mode = -1;

    for(s32 i = 0; i < COUNT_OF(Modes); i++)
    {
        tic_rect rect = {i * Size, 0, Size, Size};

        bool over = false;

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            over = true;

            showTooltip(Tips[i]);

            if(checkMouseClick(&rect, tic_mouse_left))
                impl.toolbarMode = Modes[i];
        }

        if(getStudioMode() == Modes[i]) mode = i;

        if (mode == i)
        {
            drawBitIcon(tic_icon_tab, i * Size, 0, tic_color_grey);
            drawBitIcon(Icons[i], i * Size, 1, tic_color_black);
        }

        drawBitIcon(Icons[i], i * Size, 0, mode == i ? tic_color_white : (over ? tic_color_grey : tic_color_light_grey));
    }

    if(mode >= 0) drawExtrabar(tic);

    static const char* Names[] =
    {
        "CODE EDITOR",
        "SPRITE EDITOR",
        "MAP EDITOR",
        "SFX EDITOR",
        "MUSIC EDITOR",
    };

#if defined (TIC80_PRO)
    enum {TextOffset = (COUNT_OF(Modes) + 2) * Size - 2};
    if(mode >= 1)
        drawBankIcon(COUNT_OF(Modes) * Size + 2, 0);
#else
    enum {TextOffset = (COUNT_OF(Modes) + 1) * Size};
#endif

    if(mode == 0 || (mode >= 1 && !impl.bank.show))
    {
        if(strlen(impl.tooltip.text))
        {
            tic_api_print(tic, impl.tooltip.text, TextOffset, 1, tic_color_dark_grey, false, 1, false);
        }
        else
        {
            tic_api_print(tic, Names[mode], TextOffset, 1, tic_color_grey, false, 1, false);
        }
    }
}

void setStudioEvent(StudioEvent event)
{
    switch(impl.mode)
    {
    case TIC_CODE_MODE:     
        {
            Code* code = impl.code;
            code->event(code, event);           
        }
        break;
    case TIC_SPRITE_MODE:   
        {
            Sprite* sprite = impl.banks.sprite[impl.bank.index.sprites];
            sprite->event(sprite, event); 
        }
    break;
    case TIC_MAP_MODE:
        {
            Map* map = impl.banks.map[impl.bank.index.map];
            map->event(map, event);
        }
        break;
    case TIC_SFX_MODE:
        {
            Sfx* sfx = impl.banks.sfx[impl.bank.index.sfx];
            sfx->event(sfx, event);
        }
        break;
    case TIC_MUSIC_MODE:
        {
            Music* music = impl.banks.music[impl.bank.index.music];
            music->event(music, event);
        }
        break;
    default: break;
    }
}

ClipboardEvent getClipboardEvent()
{
    tic_mem* tic = impl.studio.tic;

    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    if(ctrl)
    {
        if(keyWasPressed(tic_key_insert) || keyWasPressed(tic_key_c)) return TIC_CLIPBOARD_COPY;
        else if(keyWasPressed(tic_key_x)) return TIC_CLIPBOARD_CUT;
        else if(keyWasPressed(tic_key_v)) return TIC_CLIPBOARD_PASTE;
    }
    else if(shift)
    {
        if(keyWasPressed(tic_key_delete)) return TIC_CLIPBOARD_CUT;
        else if(keyWasPressed(tic_key_insert)) return TIC_CLIPBOARD_PASTE;
    }

    return TIC_CLIPBOARD_NONE;
}

static void showPopupMessage(const char* text)
{
    memset(impl.popup.message, '\0', sizeof impl.popup.message);
    strncpy(impl.popup.message, text, sizeof(impl.popup.message) - 1);

    for(char* c = impl.popup.message; c < impl.popup.message + sizeof impl.popup.message; c++)
        if(*c) *c = toupper(*c);

    impl.anim.movie = resetMovie(&impl.anim.show);
}
#endif

static void exitConfirm(bool yes, void* data)
{
    impl.studio.quit = yes;
}

void exitStudio()
{
#if defined(BUILD_EDITORS)
    if(impl.mode != TIC_START_MODE && studioCartChanged())
    {
        static const char* Rows[] =
        {
            "WARNING!",
            "You have unsaved changes",
            "Do you really want to exit?",
        };

        confirmDialog(Rows, COUNT_OF(Rows), exitConfirm, NULL);
    }
    else 
#endif
        exitConfirm(true, NULL);
}

void drawBitIcon(s32 id, s32 x, s32 y, u8 color)
{
    tic_mem* tic = impl.studio.tic;

    const tic_tile* tile = &getConfig()->cart->bank0.tiles.data[id];

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

static void initRunMode()
{
    initRun(impl.run, 
#if defined(BUILD_EDITORS)
        impl.console, 
#else
        NULL,
#endif
        impl.fs, impl.studio.tic);
}

#if defined(BUILD_EDITORS)
static void initWorldMap()
{
    initWorld(impl.world, impl.studio.tic, impl.banks.map[impl.bank.index.map]);
}

static void initSurfMode()
{
    initSurf(impl.surf, impl.studio.tic, impl.console);
}

void gotoSurf()
{
    initSurfMode();
    setStudioMode(TIC_SURF_MODE);
}

void gotoCode()
{
    setStudioMode(TIC_CODE_MODE);
}

#endif

void setStudioMode(EditorMode mode)
{
    if(mode != impl.mode)
    {
        EditorMode prev = impl.mode;

        if(prev == TIC_RUN_MODE)
            tic_core_pause(impl.studio.tic);

        if(mode != TIC_RUN_MODE)
            tic_api_reset(impl.studio.tic);

        switch (prev)
        {
        case TIC_START_MODE:
        case TIC_CONSOLE_MODE:
        case TIC_MENU_MODE:
            break;
        default: impl.prevMode = prev; break;
        }

#if defined(BUILD_EDITORS)
        switch(mode)
        {
        case TIC_RUN_MODE: initRunMode(); break;
        case TIC_CONSOLE_MODE:
            if (prev == TIC_SURF_MODE)
                impl.console->done(impl.console);
            break;
        case TIC_WORLD_MODE: initWorldMap(); break;
        case TIC_SURF_MODE: impl.surf->resume(impl.surf); break;
        default: break;
        }

        impl.mode = mode;
#else
        switch (mode)
        {
        case TIC_START_MODE:
        case TIC_MENU_MODE:
            impl.mode = mode;
            break;
        default:
            impl.mode = TIC_RUN_MODE;
        }
#endif
    }
}

EditorMode getStudioMode()
{
    return impl.mode;
}

#if defined(BUILD_EDITORS)
static void changeStudioMode(s32 dir)
{
    for(size_t i = 0; i < COUNT_OF(Modes); i++)
    {
        if(impl.mode == Modes[i])
        {
            setStudioMode(Modes[(i+dir+ COUNT_OF(Modes)) % COUNT_OF(Modes)]);
            return;
        }
    }
}
#endif

void resumeGame()
{
    tic_core_resume(impl.studio.tic);
    impl.mode = TIC_RUN_MODE;
}

static inline bool pointInRect(const tic_point* pt, const tic_rect* rect)
{
    return (pt->x >= rect->x) 
        && (pt->x < (rect->x + rect->w)) 
        && (pt->y >= rect->y)
        && (pt->y < (rect->y + rect->h));
}

bool checkMousePos(const tic_rect* rect)
{
    tic_point pos = tic_api_mouse(impl.studio.tic);
    return pointInRect(&pos, rect);
}

bool checkMouseClick(const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &impl.mouse.state[button];

    bool value = state->click
        && pointInRect(&state->start, rect)
        && pointInRect(&state->end, rect);

    if(value) state->click = false;

    return value;
}

bool checkMouseDown(const tic_rect* rect, tic_mouse_btn button)
{
    MouseState* state = &impl.mouse.state[button];

    return state->down && pointInRect(&state->start, rect);
}

void setCursor(tic_cursor id)
{
    tic_mem* tic = impl.studio.tic;

    VBANK(tic, 0)
    {
        tic->ram->vram.vars.cursor.sprite = id;
    }
}

#if defined(BUILD_EDITORS)

typedef struct
{
    ConfirmCallback callback;
    void* data;
} ConfirmData;

static void confirmHandler(bool yes, void* data)
{
    if(impl.menuMode == TIC_RUN_MODE)
    {
        tic_core_resume(impl.studio.tic);
        impl.mode = TIC_RUN_MODE;
    }
    else setStudioMode(impl.menuMode);

    ConfirmData* confirmData = data;
    SCOPE(free(confirmData))
    {
        confirmData->callback(yes, confirmData->data);
    }
}

static void confirmNo(void* data, s32 pos)
{
    confirmHandler(false, data);
}

static void confirmYes(void* data, s32 pos)
{
    confirmHandler(true, data);
}

void confirmDialog(const char** text, s32 rows, ConfirmCallback callback, void* data)
{
    if(impl.mode != TIC_MENU_MODE)
    {
        impl.menuMode = impl.mode;
        impl.mode = TIC_MENU_MODE;
    }

    static const MenuItem Answers[] = 
    {
        {"",    NULL},
        {"NO",  confirmNo},
        {"YES", confirmYes},
    };

    s32 count = rows + COUNT_OF(Answers);
    MenuItem* items = malloc(sizeof items[0] * count);
    SCOPE(free(items))
    {
        for(s32 i = 0; i != rows; ++i)
            items[i] = (MenuItem){text[i], NULL};

        memcpy(items + rows, Answers, sizeof Answers);

        studio_menu_init(impl.menu, items, count, count - 2, 0,
            NULL, MOVE((ConfirmData){callback, data}));

        playSystemSfx(0);
    }
}

static void resetBanks()
{
    memset(impl.bank.indexes, 0, sizeof impl.bank.indexes);
}

static void initModules()
{
    tic_mem* tic = impl.studio.tic;

    resetBanks();

    initCode(impl.code, impl.studio.tic, &tic->cart.code);

    for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
    {
        initSprite(impl.banks.sprite[i], impl.studio.tic, &tic->cart.banks[i].tiles);
        initMap(impl.banks.map[i], impl.studio.tic, &tic->cart.banks[i].map);
        initSfx(impl.banks.sfx[i], impl.studio.tic, &tic->cart.banks[i].sfx);
        initMusic(impl.banks.music[i], impl.studio.tic, &tic->cart.banks[i].music);
    }

    initWorldMap();
}

static void updateHash()
{
    md5(&impl.studio.tic->cart, sizeof(tic_cartridge), impl.cart.hash.data);
}

static void updateMDate()
{
    impl.cart.mdate = fs_date(impl.console->rom.path);
}
#endif

static void updateTitle()
{
    char name[TICNAME_MAX] = TIC_TITLE;

#if defined(BUILD_EDITORS)
    if(strlen(impl.console->rom.name))
        snprintf(name, TICNAME_MAX, "%s [%s]", TIC_TITLE, impl.console->rom.name);
#endif

    tic_sys_title(name);
}

#if defined(BUILD_EDITORS)
tic_cartridge* loadPngCart(png_buffer buffer)
{
    png_buffer zip = png_decode(buffer);

    if (zip.size)
    {
        png_buffer buf = png_create(sizeof(tic_cartridge));

        buf.size = tic_tool_unzip(buf.data, buf.size, zip.data, zip.size);
        free(zip.data);

        if(buf.size)
        {
            tic_cartridge* cart = malloc(sizeof(tic_cartridge));
            tic_cart_load(cart, buf.data, buf.size);
            free(buf.data);

            return cart;
        }
    }

    return NULL;
}

void studioRomSaved()
{
    updateTitle();
    updateHash();
    updateMDate();
}

void studioRomLoaded()
{
    initModules();

    updateTitle();
    updateHash();
    updateMDate();
}

bool studioCartChanged()
{
    CartHash hash;
    md5(&impl.studio.tic->cart, sizeof(tic_cartridge), hash.data);

    return memcmp(hash.data, impl.cart.hash.data, sizeof(CartHash)) != 0;
}
#endif

void runGame()
{
#if defined(BUILD_EDITORS)
    if(impl.console->args.keepcmd 
        && impl.console->commands.count
        && impl.console->commands.current >= impl.console->commands.count)
    {
        impl.console->commands.current = 0;
        setStudioMode(TIC_CONSOLE_MODE);
    }
    else
#endif
    {
        tic_api_reset(impl.studio.tic);

        if(impl.mode == TIC_RUN_MODE)
        {
            initRunMode();
            return;
        }

        setStudioMode(TIC_RUN_MODE);

#if defined(BUILD_EDITORS)
        if(impl.mode == TIC_SURF_MODE)
            impl.prevMode = TIC_SURF_MODE;
#endif
    }
}

#if defined(BUILD_EDITORS)
static void saveProject()
{
    CartSaveResult rom = impl.console->save(impl.console);

    if(rom == CART_SAVE_OK)
    {
        char buffer[STUDIO_TEXT_BUFFER_WIDTH];
        char str_saved[] = " saved :)";

        s32 name_len = (s32)strlen(impl.console->rom.name);
        if (name_len + strlen(str_saved) > sizeof(buffer)){
            char subbuf[sizeof(buffer) - sizeof(str_saved) - 5];
            memset(subbuf, '\0', sizeof subbuf);
            strncpy(subbuf, impl.console->rom.name, sizeof subbuf-1);

            snprintf(buffer, sizeof buffer, "%s[...]%s", subbuf, str_saved);
        }
        else
        {
            snprintf(buffer, sizeof buffer, "%s%s", impl.console->rom.name, str_saved);
        }

        showPopupMessage(buffer);
    }
    else if(rom == CART_SAVE_MISSING_NAME) showPopupMessage("error: missing cart name :(");
    else showPopupMessage("error: file not saved :(");
}

static void screen2buffer(u32* buffer, const u32* pixels, const tic_rect* rect)
{
    pixels += rect->y * TIC80_FULLWIDTH;

    for(s32 i = 0; i < rect->h; i++)
    {
        memcpy(buffer, pixels + rect->x, rect->w * sizeof(pixels[0]));
        pixels += TIC80_FULLWIDTH;
        buffer += rect->w;
    }
}

static void setCoverImage()
{
    tic_mem* tic = impl.studio.tic;

    if(impl.mode == TIC_RUN_MODE)
    {
        tic_api_sync(tic, tic_sync_screen, 0, true);
        showPopupMessage("cover image saved :)");
    }
}

static void stopVideoRecord(const char* name)
{
    if(impl.video.buffer)
    {
        {
            s32 size = 0;
            u8* data = malloc(FRAME_SIZE * impl.video.frame);
            s32 i = 0;
            char filename[TICNAME_MAX];

            gif_write_animation(data, &size, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, (const u8*)impl.video.buffer, impl.video.frame, TIC80_FRAMERATE, getConfig()->gifScale);

            // Find an available filename to save.
            do
            {
                snprintf(filename, sizeof filename, name, ++i);
            }
            while(tic_fs_exists(impl.fs, filename));

            // Now that it has found an available filename, save it.
            if(tic_fs_save(impl.fs, filename, data, size, true))
            {
                char msg[TICNAME_MAX];
                sprintf(msg, "%s saved :)", filename);
                showPopupMessage(msg);

                tic_sys_open_path(tic_fs_path(impl.fs, filename));
            }
            else showPopupMessage("error: file not saved :(");
        }

        free(impl.video.buffer);
        impl.video.buffer = NULL;
    }

    impl.video.record = false;
}

static void startVideoRecord()
{
    if(impl.video.record)
    {
        stopVideoRecord(VideoGif);
    }
    else
    {
        impl.video.frames = getConfig()->gifLength * TIC80_FRAMERATE;
        impl.video.buffer = malloc(FRAME_SIZE * impl.video.frames);

        if(impl.video.buffer)
        {
            impl.video.record = true;
            impl.video.frame = 0;
        }
    }
}

static void takeScreenshot()
{
    impl.video.frames = 1;
    impl.video.buffer = malloc(FRAME_SIZE);

    if(impl.video.buffer)
    {
        impl.video.record = true;
        impl.video.frame = 0;
    }
}
#endif

static inline bool keyWasPressedOnce(s32 key)
{
    tic_mem* tic = impl.studio.tic;

    return tic_api_keyp(tic, key, -1, -1);
}

#if defined(CRT_SHADER_SUPPORT)
static void switchCrtMonitor()
{
    impl.config->data.options.crt = !impl.config->data.options.crt;
}
#endif

#if defined(TIC80_PRO)

static void switchBank(s32 bank)
{
    for(s32 i = 0; i < COUNT_OF(BankModes); i++)
        if(BankModes[i] == impl.mode)
        {
            if(impl.bank.chained) 
                memset(impl.bank.indexes, bank, sizeof impl.bank.indexes);
            else impl.bank.indexes[i] = bank;
            break;
        }
}

#endif

void gotoMenu() 
{
    if(impl.mode != TIC_MENU_MODE)
    {
        tic_core_pause(impl.studio.tic);
        tic_api_reset(impl.studio.tic);
        impl.mode = TIC_MENU_MODE;
    }

    impl.mainmenu = studio_mainmenu_init(impl.menu, impl.config);
}

static void processShortcuts()
{
    tic_mem* tic = impl.studio.tic;

    if(impl.mode == TIC_START_MODE) return;

#if defined(BUILD_EDITORS)
    if(impl.mode == TIC_CONSOLE_MODE && !impl.console->active) return;
#endif

    if(studio_mainmenu_keyboard(impl.mainmenu)) return;

    bool alt = tic_api_key(tic, tic_key_alt);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

#if defined(CRT_SHADER_SUPPORT)
    if(keyWasPressedOnce(tic_key_f6)) switchCrtMonitor();
#endif

    if(alt)
    {
        if (keyWasPressedOnce(tic_key_return)) tic_sys_fullscreen_set(!tic_sys_fullscreen_get());
#if defined(BUILD_EDITORS)
        else if(impl.mode != TIC_RUN_MODE)
        {
            if(keyWasPressedOnce(tic_key_grave)) setStudioMode(TIC_CONSOLE_MODE);
            else if(keyWasPressedOnce(tic_key_1)) setStudioMode(TIC_CODE_MODE);
            else if(keyWasPressedOnce(tic_key_2)) setStudioMode(TIC_SPRITE_MODE);
            else if(keyWasPressedOnce(tic_key_3)) setStudioMode(TIC_MAP_MODE);
            else if(keyWasPressedOnce(tic_key_4)) setStudioMode(TIC_SFX_MODE);
            else if(keyWasPressedOnce(tic_key_5)) setStudioMode(TIC_MUSIC_MODE);
        }
#endif
    }
    else if(ctrl)
    {
        if(keyWasPressedOnce(tic_key_q)) exitStudio(NULL);
#if defined(BUILD_EDITORS)
        else if(keyWasPressedOnce(tic_key_pageup)) changeStudioMode(-1);
        else if(keyWasPressedOnce(tic_key_pagedown)) changeStudioMode(1);
        else if(keyWasPressedOnce(tic_key_return)) runGame();
        else if(keyWasPressedOnce(tic_key_r)) runGame();
        else if(keyWasPressedOnce(tic_key_s)) saveProject();
#endif

#if defined(TIC80_PRO)

        else
            for(s32 bank = 0; bank < TIC_BANKS; bank++)
                if(keyWasPressedOnce(tic_key_0 + bank))
                    switchBank(bank);

#endif

    }
    else
    {
        if (keyWasPressedOnce(tic_key_f11)) tic_sys_fullscreen_set(!tic_sys_fullscreen_get());
#if defined(BUILD_EDITORS)
        else if(keyWasPressedOnce(tic_key_escape))
        {
            switch(impl.mode)
            {
            case TIC_MENU_MODE:     
                getConfig()->options.devmode 
                    ? setStudioMode(impl.prevMode) 
                    : studio_menu_back(impl.menu);
                break;
            case TIC_RUN_MODE:      
                getConfig()->options.devmode 
                    ? setStudioMode(impl.prevMode) 
                    : gotoMenu();
                break;
            case TIC_CONSOLE_MODE: setStudioMode(impl.prevMode); break;
            case TIC_CODE_MODE:
                if(impl.code->mode != TEXT_EDIT_MODE)
                {
                    impl.code->escape(impl.code);
                    return;
                }
            default:
                setStudioMode(TIC_CONSOLE_MODE);
            }
        }
        else if(keyWasPressedOnce(tic_key_f8)) takeScreenshot();
        else if(keyWasPressedOnce(tic_key_f9)) startVideoRecord();
        else if(impl.mode == TIC_RUN_MODE && keyWasPressedOnce(tic_key_f7))
            setCoverImage();

        if(getConfig()->options.devmode || impl.mode != TIC_RUN_MODE)
        {
            if(keyWasPressedOnce(tic_key_f1)) setStudioMode(TIC_CODE_MODE);
            else if(keyWasPressedOnce(tic_key_f2)) setStudioMode(TIC_SPRITE_MODE);
            else if(keyWasPressedOnce(tic_key_f3)) setStudioMode(TIC_MAP_MODE);
            else if(keyWasPressedOnce(tic_key_f4)) setStudioMode(TIC_SFX_MODE);
            else if(keyWasPressedOnce(tic_key_f5)) setStudioMode(TIC_MUSIC_MODE);
        }
#else
        else if(keyWasPressedOnce(tic_key_escape))
        {
            switch(impl.mode)
            {
            case TIC_MENU_MODE: studio_menu_back(impl.menu); break;
            case TIC_RUN_MODE: gotoMenu(); break;
            }
        }
#endif
    }
}

#if defined(BUILD_EDITORS)
static void reloadConfirm(bool yes, void* data)
{
    if(yes)
        impl.console->updateProject(impl.console);
    else
        updateMDate();
}

static void checkChanges()
{
    switch(impl.mode)
    {
    case TIC_START_MODE:
        break;
    default:
        {
            Console* console = impl.console;

            u64 date = fs_date(console->rom.path);

            if(impl.cart.mdate && date > impl.cart.mdate)
            {
                if(studioCartChanged())
                {
                    static const char* Rows[] =
                    {
                        "WARNING!",
                        "The cart has changed!",
                        "Do you want to reload it?"
                    };

                    confirmDialog(Rows, COUNT_OF(Rows), reloadConfirm, NULL);
                }
                else console->updateProject(console);
            }
        }
    }
}

static void drawBitIconRaw(u32* frame, s32 sx, s32 sy, s32 id, tic_color color)
{
    const tic_bank* bank = &getConfig()->cart->bank0;

    u32 *dst = frame + sx + sy * TIC80_FULLWIDTH;
    for(s32 src = 0; src != TIC_SPRITESIZE * TIC_SPRITESIZE; dst += TIC80_FULLWIDTH - TIC_SPRITESIZE)
        for(s32 i = 0; i != TIC_SPRITESIZE; ++i, ++dst)
            if(tic_tool_peek4(&bank->tiles.data[id].data, src++))
                *dst = tic_rgba(&bank->palette.vbank0.colors[color]);
}

static void drawRecordLabel(u32* frame, s32 sx, s32 sy)
{
    drawBitIconRaw(frame, sx, sy, tic_icon_rec, tic_color_red);
    drawBitIconRaw(frame, sx + TIC_SPRITESIZE, sy, tic_icon_rec2, tic_color_red);
}

static bool isRecordFrame(void)
{
    return impl.video.record;
}

static void recordFrame(u32* pixels)
{
    if(impl.video.record)
    {
        if(impl.video.frame < impl.video.frames)
        {
            tic_rect rect = {0, 0, TIC80_FULLWIDTH, TIC80_FULLHEIGHT};
            screen2buffer(impl.video.buffer + (TIC80_FULLWIDTH*TIC80_FULLHEIGHT) * impl.video.frame, pixels, &rect);

            if(impl.video.frame % TIC80_FRAMERATE < TIC80_FRAMERATE / 2)
            {
                drawRecordLabel(pixels, TIC80_WIDTH-24, 8);
            }

            impl.video.frame++;

        }
        else
        {
            stopVideoRecord(impl.video.frame == 1 ? ScreenGif : VideoGif);
        }
    }
}

#endif

static void renderStudio()
{
    tic_mem* tic = impl.studio.tic;

#if defined(BUILD_EDITORS)
    showTooltip("");
#endif

    {
        const tic_sfx* sfx = NULL;
        const tic_music* music = NULL;

        switch(impl.mode)
        {
        case TIC_RUN_MODE:
            sfx = &impl.studio.tic->ram->sfx;
            music = &impl.studio.tic->ram->music;
            break;
        case TIC_START_MODE:
        case TIC_MENU_MODE:
        case TIC_SURF_MODE:
            sfx = &impl.config->cart->bank0.sfx;
            music = &impl.config->cart->bank0.music;
            break;
        default:
#if defined(BUILD_EDITORS)
            sfx = getSfxSrc();
            music = getMusicSrc();
#endif
            break;
        }

        sfx2ram(tic->ram, sfx);
        music2ram(tic->ram, music);

        // restore mapping in all the modes except Run mode
        if(impl.mode != TIC_RUN_MODE)
            impl.studio.tic->ram->mapping = getConfig()->options.mapping;

        tic_core_tick_start(tic);
    }

    // SECURITY: It's important that this comes before `tick` and not after
    // to prevent misbehaving cartridges from having an opportunity to
    // tamper with the keyboard input.
    processShortcuts();

    // clear screen for all the modes except the Run mode
    if(impl.mode != TIC_RUN_MODE)
    {
        VBANK(tic, 1)
        {
            tic_api_cls(tic, 0);
        }
    }
    
    switch(impl.mode)
    {
    case TIC_START_MODE:    impl.start->tick(impl.start); break;
    case TIC_RUN_MODE:      impl.run->tick(impl.run); break;
    case TIC_MENU_MODE:     studio_menu_tick(impl.menu); break;

#if defined(BUILD_EDITORS)
    case TIC_CONSOLE_MODE:  impl.console->tick(impl.console); break;
    case TIC_CODE_MODE:     
        {
            Code* code = impl.code;
            code->tick(code);
        }
        break;
    case TIC_SPRITE_MODE:   
        {
            Sprite* sprite = impl.banks.sprite[impl.bank.index.sprites];
            sprite->tick(sprite);       
        }
        break;
    case TIC_MAP_MODE:
        {
            Map* map = impl.banks.map[impl.bank.index.map];
            map->tick(map);
        }
        break;
    case TIC_SFX_MODE:
        {
            Sfx* sfx = impl.banks.sfx[impl.bank.index.sfx];
            sfx->tick(sfx);
        }
        break;
    case TIC_MUSIC_MODE:
        {
            Music* music = impl.banks.music[impl.bank.index.music];
            music->tick(music);
        }
        break;

    case TIC_WORLD_MODE:    impl.world->tick(impl.world); break;
    case TIC_SURF_MODE:     impl.surf->tick(impl.surf); break;
#endif
    default: break;
    }

    tic_core_tick_end(tic);

    switch(impl.mode)
    {
    case TIC_RUN_MODE: break;
    case TIC_SURF_MODE:
    case TIC_MENU_MODE:
        tic->input.data = -1;
        break;
    default:
        tic->input.data = -1;
        tic->input.gamepad = 0;
    }
}

static void updateSystemFont()
{
    tic_mem* tic = impl.studio.tic;

    impl.systemFont = (tic_font)
    {
        .regular =
        {
            .width       = TIC_FONT_WIDTH,
            .height      = TIC_FONT_HEIGHT,
        },
        .alt = 
        {
            .width       = TIC_ALTFONT_WIDTH,
            .height      = TIC_FONT_HEIGHT,            
        }
    };

    u8* dst = (u8*)&impl.systemFont;

    for(s32 i = 0; i < TIC_FONT_CHARS * 2; i++)
        for(s32 y = 0; y < TIC_SPRITESIZE; y++)
            for(s32 x = 0; x < TIC_SPRITESIZE; x++)
                if(tic_tool_peek4(&impl.config->cart->bank0.sprites.data[i], TIC_SPRITESIZE*y + x))
                    dst[i*BITS_IN_BYTE+y] |= 1 << x;

    tic->ram->font = impl.systemFont;
}

void studioConfigChanged()
{
#if defined(BUILD_EDITORS)
    Code* code = impl.code;
    if(code->update)
        code->update(code);
#endif

    updateSystemFont();
    tic_sys_update_config();
}

static void processMouseStates()
{
    for(s32 i = 0; i < COUNT_OF(impl.mouse.state); i++)
        impl.mouse.state[i].click = false;

    tic_mem* tic = impl.studio.tic;

    tic->ram->vram.vars.cursor.sprite = tic_cursor_arrow;
    tic->ram->vram.vars.cursor.system = true;

    for(s32 i = 0; i < COUNT_OF(impl.mouse.state); i++)
    {
        MouseState* state = &impl.mouse.state[i];

        if(!state->down && (tic->ram->input.mouse.btns & (1 << i)))
        {
            state->down = true;
            state->start = tic_api_mouse(tic);
        }
        else if(state->down && !(tic->ram->input.mouse.btns & (1 << i)))
        {
            state->end = tic_api_mouse(tic);

            state->click = true;
            state->down = false;
        }
    }
}

static void blitCursor()
{
    tic_mem* tic = impl.studio.tic;
    tic80_mouse* m = &tic->ram->input.mouse;

    if(tic->input.mouse && !m->relative && m->x < TIC80_FULLWIDTH && m->y < TIC80_FULLHEIGHT)
    {
        const tic_bank* bank = &tic->cart.bank0;

        tic_point hot = {0};

        if(tic->ram->vram.vars.cursor.system)
        {
            bank = &getConfig()->cart->bank0;
            hot = (tic_point[])
            {
                {0, 0},
                {3, 0},
                {2, 3},
            }[tic->ram->vram.vars.cursor.sprite];
        }

        const tic_palette* pal = &bank->palette.vbank0;
        const tic_tile* tile = &bank->sprites.data[tic->ram->vram.vars.cursor.sprite];

        tic_point s = {m->x - hot.x, m->y - hot.y};
        u32* dst = tic->screen + TIC80_FULLWIDTH * s.y + s.x;

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

static void studioTick()
{
    tic_mem* tic = impl.studio.tic;

#if defined(BUILD_EDITORS)
    processAnim(impl.anim.movie, &impl);
    checkChanges();
    tic_net_start(impl.net);
#endif
 
    if(impl.toolbarMode)
    {
        setStudioMode(impl.toolbarMode);
        impl.toolbarMode = 0;
    }

    processMouseStates();
    renderStudio();
    
    {
#if defined(BUILD_EDITORS)
        Sprite* sprite = impl.banks.sprite[impl.bank.index.sprites];
        Map* map = impl.banks.map[impl.bank.index.map];
#endif

        tic_blit_callback callback[TIC_MODES_COUNT] = 
        {
            [TIC_MENU_MODE]     = {studio_menu_anim_scanline, NULL, NULL, impl.menu},

#if defined(BUILD_EDITORS)
            [TIC_SPRITE_MODE]   = {sprite->scanline,        NULL, NULL, sprite},
            [TIC_MAP_MODE]      = {map->scanline,           NULL, NULL, map},
            [TIC_WORLD_MODE]    = {impl.world->scanline,    NULL, NULL, impl.world},
            [TIC_SURF_MODE]     = {impl.surf->scanline,     NULL, NULL, impl.surf},
#endif
        };

        if(impl.mode != TIC_RUN_MODE)
        {
            memcpy(tic->ram->vram.palette.data, getConfig()->cart->bank0.palette.vbank0.data, sizeof(tic_palette));
            tic->ram->font = impl.systemFont;
        }

        callback[impl.mode].data
            ? tic_core_blit_ex(tic, callback[impl.mode])
            : tic_core_blit(tic);

        blitCursor();

#if defined(BUILD_EDITORS)
        if(isRecordFrame())
            recordFrame(tic->screen);

        drawPopup();
#endif
    }

#if defined(BUILD_EDITORS)
    tic_net_end(impl.net);
#endif
}

static void studioSound()
{
    tic_mem* tic = impl.studio.tic;
    tic_core_synth_sound(tic);

    s32 volume = getConfig()->options.volume;

    if(volume != MAX_VOLUME)
    {
        s32 size = tic->samples.size / sizeof tic->samples.buffer[0];
        for(s16* it = tic->samples.buffer, *end = it + size; it != end; ++it)
            *it = *it * volume / MAX_VOLUME;        
    }
}

static void studioLoad(const char* file)
{
#if defined(BUILD_EDITORS)
    showPopupMessage(impl.console->loadCart(impl.console, file) 
        ? "cart successfully loaded :)"
        : "error: cart not loaded :(");
#endif
}

void exitGame()
{
    if(impl.prevMode == TIC_SURF_MODE)
    {
        setStudioMode(TIC_SURF_MODE);
    }
    else
    {
        setStudioMode(TIC_CONSOLE_MODE);
    }
}

static void studioClose()
{
    {
#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            freeSprite  (impl.banks.sprite[i]);
            freeMap     (impl.banks.map[i]);
            freeSfx     (impl.banks.sfx[i]);
            freeMusic   (impl.banks.music[i]);
        }

        freeCode    (impl.code);
        freeConsole (impl.console);
        freeWorld   (impl.world);
        freeSurf    (impl.surf);

        FREE(impl.anim.show.items);
        FREE(impl.anim.hide.items);

#endif

        freeStart   (impl.start);
        freeRun     (impl.run);
        freeConfig  (impl.config);

        studio_mainmenu_free(impl.mainmenu);
        studio_menu_free(impl.menu);
    }

    if(impl.tic80local)
        tic80_delete((tic80*)impl.tic80local);

#if defined(BUILD_EDITORS)
    tic_net_close(impl.net);
#endif

    free(impl.fs);
}

static StartArgs parseArgs(s32 argc, char **argv)
{
    static const char *const usage[] = 
    {
        "tic80 [cart] [options]",
        NULL,
    };

    StartArgs args = {.volume = -1};

    struct argparse_option options[] = 
    {
        OPT_HELP(),
#define CMD_PARAMS_DEF(name, ctype, type, post, help) OPT_##type('\0', #name, &args.name, help),
        CMD_PARAMS_LIST(CMD_PARAMS_DEF)
#undef  CMD_PARAMS_DEF
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
    StudioImplementation* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.idle);
}

static void setPopupWait(void* data)
{
    StudioImplementation* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.wait);
}

static void setPopupHide(void* data)
{
    StudioImplementation* studio = data;
    studio->anim.movie = resetMovie(&studio->anim.hide);
}

#endif

Studio* studioInit(s32 argc, char **argv, s32 samplerate, const char* folder)
{
    //setbuf(stdout, NULL);

    StartArgs args = parseArgs(argc, argv);
    
    if (args.version)
    {
        printf("%s\n", TIC_VERSION);
        exit(0);
    }

    impl.samplerate = samplerate;

#if defined(BUILD_EDITORS)
    impl.net = tic_net_create("http://"TIC_HOST);
#endif

    {
        const char *path = args.fs ? args.fs : folder;

        if (fs_exists(path))
        {
            impl.fs = tic_fs_create(path, 
#if defined(BUILD_EDITORS)
                impl.net
#else
                NULL
#endif
            );
        }
        else
        {
            fprintf(stderr, "error: folder `%s` doesn't exist\n", path);
            exit(1);
        }
    }

    impl.tic80local = (tic80_local*)tic80_create(impl.samplerate);
    impl.studio.tic = impl.tic80local->memory;

    {

#if defined(BUILD_EDITORS)
        for(s32 i = 0; i < TIC_EDITOR_BANKS; i++)
        {
            impl.banks.sprite[i]   = calloc(1, sizeof(Sprite));
            impl.banks.map[i]      = calloc(1, sizeof(Map));
            impl.banks.sfx[i]      = calloc(1, sizeof(Sfx));
            impl.banks.music[i]    = calloc(1, sizeof(Music));
        }

        impl.code       = calloc(1, sizeof(Code));
        impl.console    = calloc(1, sizeof(Console));
        impl.world      = calloc(1, sizeof(World));
        impl.surf       = calloc(1, sizeof(Surf));

        impl.anim.show = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setPopupWait,
        {
            {-TOOLBAR_SIZE, 0, STUDIO_ANIM_TIME, &impl.anim.pos.popup, AnimEaseIn},
        });

        impl.anim.wait = (Movie){.time = TIC80_FRAMERATE * 2, .done = setPopupHide};
        impl.anim.hide = (Movie)MOVIE_DEF(STUDIO_ANIM_TIME, setIdle,
        {
            {0, -TOOLBAR_SIZE, STUDIO_ANIM_TIME, &impl.anim.pos.popup, AnimEaseIn},
        });

        impl.anim.movie = resetMovie(&impl.anim.idle);
#endif

        impl.start      = calloc(1, sizeof(Start));
        impl.run        = calloc(1, sizeof(Run));
        impl.menu       = studio_menu_create(impl.studio.tic);
        impl.config     = calloc(1, sizeof(Config));
    }

    tic_fs_makedir(impl.fs, TIC_LOCAL);
    tic_fs_makedir(impl.fs, TIC_LOCAL_VERSION);
    
    initConfig(impl.config, impl.studio.tic, impl.fs);
    initStart(impl.start, impl.studio.tic, args.cart);
    initRunMode();

#if defined(BUILD_EDITORS)
    initConsole(impl.console, impl.studio.tic, impl.fs, impl.net, impl.config, args);
    initSurfMode();
    initModules();
#endif

    if(args.scale)
        impl.config->data.uiScale = args.scale;

    if(args.volume >= 0)
        impl.config->data.options.volume = args.volume & 0x0f;

#if defined(CRT_SHADER_SUPPORT)
    impl.config->data.options.crt          |= args.crt;
#endif

    impl.config->data.options.fullscreen   |= args.fullscreen;
    impl.config->data.options.vsync        |= args.vsync;
    impl.config->data.soft              |= args.soft;
    impl.config->data.cli               |= args.cli;

    impl.studio.tick    = studioTick;
    impl.studio.sound   = studioSound;
    impl.studio.load    = studioLoad;
    impl.studio.close   = studioClose;
    impl.studio.exit    = exitStudio;
    impl.studio.config  = getConfig;

    if(args.cli)
        args.skip = true;

    if(args.skip)
        setStudioMode(TIC_CONSOLE_MODE);

    return &impl.studio;
}
