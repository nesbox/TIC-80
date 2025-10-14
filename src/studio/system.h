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

#pragma once

#include "api.h"
#include "version.h"

#if defined(TIC80_PRO)
#define TIC_VERSION_POST " Pro"
#else
#define TIC_VERSION_POST ""
#endif

#define TIC_VERSION DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) "." DEF2STR(TIC_VERSION_REVISION) TIC_VERSION_STATUS TIC_VERSION_BUILD TIC_VERSION_POST " (" TIC_VERSION_HASH ")"
#define TIC_PACKAGE "com.nesbox.tic"
#define TIC_NAME "TIC-80"
#define TIC_NAME_FULL TIC_NAME " tiny computer"
#define TIC_TITLE TIC_NAME_FULL " " TIC_VERSION
#define TIC_HOST "web.tic80.com"
#if defined(__TIC_WIN7__)
    #define TIC_WEBSITE_PROTOCOL "http://"
#else
    #define TIC_WEBSITE_PROTOCOL "https://"
#endif
#define TIC_WEBSITE TIC_WEBSITE_PROTOCOL TIC_HOST
#define TIC_COPYRIGHT TIC_WEBSITE " (C) 2017-" TIC_VERSION_YEAR

#define TICNAME_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

void    tic_sys_clipboard_set(const char* text);
bool    tic_sys_clipboard_has();
char*   tic_sys_clipboard_get();
void    tic_sys_clipboard_free(char* text);
u64     tic_sys_counter_get();
u64     tic_sys_freq_get();
bool    tic_sys_fullscreen_get();
void    tic_sys_fullscreen_set(bool value);
void    tic_sys_title(const char* title);
void    tic_sys_addfile(void(*callback)(void* userdata, const char* name, const u8* buffer, s32 size), void* userdata);
void    tic_sys_getfile(const char* name, const void* buffer, s32 size);
void    tic_sys_open_path(const char* path);
void    tic_sys_open_url(const char* path);
void    tic_sys_preseed();
bool    tic_sys_keyboard_text(char* text, void* userdata);
void    tic_sys_update_config();
void    tic_sys_default_mapping(tic_mapping* mapping);

typedef enum 
{
    tic_kbdlayout_unknown,
    tic_kbdlayout_first,
    tic_kbdlayout_qwerty = tic_kbdlayout_first,
    tic_kbdlayout_azerty,
    tic_kbdlayout_qwertz,
    tic_kbdlayout_qzerty,
    tic_kbdlayout_deneo,
    tic_kbdlayout_debone,
} tic_kbdlayout;

tic_kbdlayout tic_sys_default_kbdlayout();

#define CODE_COLORS_LIST(macro) \
    macro(BG)       \
    macro(FG)       \
    macro(STRING)   \
    macro(NUMBER)   \
    macro(KEYWORD)  \
    macro(API)      \
    macro(COMMENT)  \
    macro(SIGN)

typedef enum 
{
    KeybindMode_Standard,
    KeybindMode_Emacs,
    KeybindMode_Vi,
} KeybindMode;

typedef enum 
{
    TabMode_Auto,
    TabMode_Tab,
    TabMode_Space,
} TabMode;

typedef struct
{
    struct
    {
        struct
        {
#define     CODE_COLOR_DEF(VAR) u8 VAR;
            CODE_COLORS_LIST(CODE_COLOR_DEF)
#undef      CODE_COLOR_DEF

            u8 select;
            u8 cursor;
            bool shadow;
            bool altFont;
            bool altCaret;
            bool matchDelimiters;
            bool autoDelimiters;

        } code;

        struct
        {
            struct
            {
                u8 alpha;
            } touch;

        } gamepad;

    } theme;

    bool checkNewVersion;
    bool cli;
    bool trim;

    struct StudioOptions
    {
        bool crt;
        bool fullscreen;
        bool integerScale;
        s32 volume;
        bool autosave;
        tic_mapping mapping;
        tic_kbdlayout kbdlayout;

#if defined(BUILD_EDITORS)
        KeybindMode keybindMode;
        TabMode tabMode;
        s32 tabSize;
        bool autohideCursor;
#endif
    } options;

    const tic_cartridge* cart;

    s32 uiScale;
    s32 fft;
    s32 fftcaptureplaybackdevices;
    const char *fftdevice;

} StudioConfig;

typedef struct Studio Studio;

// !TODO: check these functions
void setJustSwitchedToCodeMode(Studio* studio, bool value);
bool hasJustSwitchedToCodeMode(Studio* studio);

const tic_mem* studio_mem(Studio* studio);
void studio_tick(Studio* studio, tic80_input input);
void studio_sound(Studio* studio);
void studio_load(Studio* studio, const char* file);
void studio_keymapchanged(Studio *studio, tic_kbdlayout kbdlayout);
bool studio_alive(Studio* studio);
void studio_exit(Studio* studio);
void studio_delete(Studio* studio);
const StudioConfig* studio_config(Studio* studio);

Studio* studio_create(s32 argc, char **argv, s32 samplerate, tic80_pixel_color_format format, const char* appFolder, s32 maxscale, void *userdata);

#ifdef __cplusplus
}
#endif
