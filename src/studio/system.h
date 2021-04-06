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

#define TIC_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define TIC_VERSION TIC_MAKE_VERSION(MYPROJ_VERSION_MAJOR, MYPROJ_VERSION_MINOR, MYPROJ_VERSION_PATCH)
#define TIC_VERSION_LABEL DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) "." DEF2STR(TIC_VERSION_REVISION) TIC_VERSION_STATUS TIC_VERSION_BUILD TIC_VERSION_POST
#define TIC_PACKAGE "com.nesbox.tic"
#define TIC_NAME "TIC-80"
#define TIC_NAME_FULL TIC_NAME " tiny computer"
#define TIC_TITLE TIC_NAME_FULL " " TIC_VERSION_LABEL
#define TIC_HTTP "http://"
#define TIC_HOST "tic80.com"
#define TIC_WEBSITE TIC_HTTP TIC_HOST
#define TIC_COPYRIGHT TIC_WEBSITE " (C) " TIC_VERSION_YEAR

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
void    tic_sys_fullscreen();
void    tic_sys_message(const char* title, const char* message);
void    tic_sys_title(const char* title);
void    tic_sys_open_path(const char* path);
void    tic_sys_preseed();
void    tic_sys_poll();
bool    tic_sys_keyboard_text(char* text);
void    tic_sys_update_config();

typedef struct
{
    struct
    {
        struct
        {
            s32 arrow;
            s32 hand;
            s32 ibeam;

            bool pixelPerfect;
        } cursor;

        struct
        {
            struct tic_code_theme
            {
                u8 string;
                u8 number;
                u8 keyword;
                u8 api;
                u8 comment;
                u8 sign;
                u8 var;
                u8 other;
            } syntax;

            u8 bg;
            u8 select;
            u8 cursor;
            bool shadow;
            bool altFont;
            bool matchDelimiters;

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
    bool noSound;

#if defined(CRT_SHADER_SUPPORT)
    bool crtMonitor;
    struct
    {
        const char* vertex;
        const char* pixel;
    } shader;
#endif
    
    bool goFullscreen;

    const tic_cartridge* cart;

    s32 uiScale;

} StudioConfig;

typedef struct
{
    tic_mem* tic;
    bool quit;

    void (*tick)();
    void (*exit)();
    void (*close)();
    void (*updateProject)();
    const StudioConfig* (*config)();

} Studio;

Studio* studioInit(s32 argc, const char **argv, s32 samplerate, const char* appFolder);

#ifdef __cplusplus
}
#endif
