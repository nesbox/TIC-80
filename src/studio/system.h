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
#define TIC_HOST "tic80.com"
#define TIC_WEBSITE "https://" TIC_HOST
#define TIC_COPYRIGHT TIC_WEBSITE " (C) 2017-" TIC_VERSION_YEAR

#define TICNAME_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

void    tic_sys_clipboard_set(const char* text);
bool    tic_sys_clipboard_has();
char*   tic_sys_clipboard_get();
void    tic_sys_clipboard_free(const char* text);
u64     tic_sys_counter_get();
u64     tic_sys_freq_get();
bool    tic_sys_fullscreen_get();
void    tic_sys_fullscreen_set(bool value);
void    tic_sys_message(const char* title, const char* message);
void    tic_sys_title(const char* title);
void    tic_sys_open_path(const char* path);
void    tic_sys_open_url(const char* path);
void    tic_sys_preseed();
bool    tic_sys_keyboard_text(char* text);
void    tic_sys_update_config();
void    tic_sys_default_mapping(tic_mapping* mapping);

#define CODE_COLORS_LIST(macro) \
    macro(BG)       \
    macro(FG)       \
    macro(STRING)   \
    macro(NUMBER)   \
    macro(KEYWORD)  \
    macro(API)      \
    macro(COMMENT)  \
    macro(SIGN)

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

    s32 gifScale;
    s32 gifLength;
    
    bool checkNewVersion;
    bool cli;
    bool soft;

#if defined(CRT_SHADER_SUPPORT)
    struct
    {
        const char* vertex;
        const char* pixel;
    } shader;
#endif

    struct StudioOptions
    {
#if defined(CRT_SHADER_SUPPORT)
        bool crt;
#endif
        
        bool fullscreen;
        bool vsync;
        s32 volume;
        tic_mapping mapping;

#if defined(BUILD_EDITORS)
        bool devmode;
#endif
    } options;

    const tic_cartridge* cart;

    s32 uiScale;

} StudioConfig;

typedef struct Studio Studio;

const tic_mem* studio_mem(Studio* studio);
void studio_tick(Studio* studio, tic80_input input);
void studio_sound(Studio* studio);
void studio_load(Studio* studio, const char* file);
bool studio_alive(Studio* studio);
void studio_exit(Studio* studio);
void studio_delete(Studio* studio);
const StudioConfig* studio_config(Studio* studio);

Studio* studio_create(s32 argc, char **argv, s32 samplerate, tic80_pixel_color_format format, const char* appFolder, s32 maxscale);

#ifdef __cplusplus
}
#endif
