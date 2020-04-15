#pragma once

#include "ticapi.h"
#include "ext/file_dialog.h"
#include "version.h"

#if defined(TIC80_PRO)
#define TIC_VERSION_POST " Pro"
#else
#define TIC_VERSION_POST ""
#endif

#define TIC_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define TIC_VERSION TIC_MAKE_VERSION(MYPROJ_VERSION_MAJOR, MYPROJ_VERSION_MINOR, MYPROJ_VERSION_PATCH)

#define DEF2STR2(x) #x
#define DEF2STR(x) DEF2STR2(x)

#define TIC_VERSION_LABEL DEF2STR(TIC_VERSION_MAJOR) "." DEF2STR(TIC_VERSION_MINOR) "." DEF2STR(TIC_VERSION_REVISION) TIC_VERSION_STATUS TIC_VERSION_POST
#define TIC_PACKAGE "com.nesbox.tic"
#define TIC_NAME "TIC-80"
#define TIC_NAME_FULL TIC_NAME " tiny computer"
#define TIC_TITLE TIC_NAME_FULL " " TIC_VERSION_LABEL
#define TIC_HTTP "http://"
#define TIC_HOST "tic80.com"
#define TIC_WEBSITE TIC_HTTP TIC_HOST
#define TIC_YEAR "2020"
#define TIC_COPYRIGHT TIC_WEBSITE " (C) " TIC_YEAR

#define TIC80_OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
#define TIC80_OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)
#define TICNAME_MAX 256
#define KEYBOARD_HOLD 20
#define KEYBOARD_PERIOD 3

typedef struct
{
    enum
    {
        HttpGetProgress,
        HttpGetDone,
        HttpGetError,
    } type;

    union
    {
        struct
        {
            s32 size;
            s32 total;
        } progress;

        struct
        {
            s32 size;
            u8* data;
        } done;

        struct
        {
            s32 code;
        } error;
    };

    void* calldata;
    const char* url;

} HttpGetData;

typedef void(*HttpGetCallback)(const HttpGetData*);

typedef struct
{
    void    (*setClipboardText)(const char* text);
    bool    (*hasClipboardText)();
    char*   (*getClipboardText)();
    void    (*freeClipboardText)(const char* text);

    u64     (*getPerformanceCounter)();
    u64     (*getPerformanceFrequency)();

    void* (*httpGetSync)(const char* url, s32* size);
    void (*httpGet)(const char* url, HttpGetCallback callback, void* userdata);

    void (*fileDialogLoad)(file_dialog_load_callback callback, void* data);
    void (*fileDialogSave)(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode);

    void (*goFullscreen)();
    void (*showMessageBox)(const char* title, const char* message);
    void (*setWindowTitle)(const char* title);

    void (*openSystemPath)(const char* path);
    
    void (*preseed)();
    void (*poll)();

    void (*updateConfig)();

} System;

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
    bool showSync;
    bool crtMonitor;
    bool goFullscreen;

    const char* crtShader;
    const tic_cartridge* cart;

    s32 uiScale;

} StudioConfig;

typedef struct
{
    tic_mem* tic;
    bool quit;
    char text;

    void (*tick)();
    void (*exit)();
    void (*close)();
    void (*updateProject)();
    const StudioConfig* (*config)();

    // TODO: remove this method, system has to know nothing about current mode
    bool (*isGamepadMode)();
} Studio;

#ifdef __cplusplus
extern "C" {
#endif


TIC80_API Studio* studioInit(s32 argc, char **argv, s32 samplerate, const char* appFolder, System* system);

#ifdef __cplusplus
}
#endif

