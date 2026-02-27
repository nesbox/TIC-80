#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "studio/studio.h"
#include "cart.h"
#include "console.h"
#include "tools.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/config.h"
#include "ext/json.h"
#include "retro_endianness.h"

static void load(Console* console, const char* path)
{
    s32 size = 0;
    void* data = tic_fs_load(console->fs, path, &size);

    if(data) SCOPE(free(data))
    {
        tic_cart_load(&console->tic->cart, data, size);
        tic_api_reset(console->tic);

        strcpy(console->rom.name, path);
        studioRomLoaded(console->studio);
    }
}

typedef struct
{
    Console* console;
    char* name;
    fs_done_callback callback;
    void* calldata;
} LoadByHashData;

static void loadByHashDone(const u8* buffer, s32 size, void* data)
{
    LoadByHashData* loadByHashData = data;
    Console* console = loadByHashData->console;

    tic_cart_load(&console->tic->cart, buffer, size);
    tic_api_reset(console->tic);

    strcpy(console->rom.name, loadByHashData->name);
    studioRomLoaded(console->studio);

    if (loadByHashData->callback)
        loadByHashData->callback(loadByHashData->calldata);

    free(loadByHashData->name);
    free(loadByHashData);
}

static void loadByHash(Console* console, const char* name, const char* hash, const char* section, fs_done_callback callback, void* data)
{
    LoadByHashData* loadByHashData = malloc(sizeof(LoadByHashData));
    *loadByHashData = (LoadByHashData){ console, strdup(name), callback, data};
    tic_fs_hashload(console->fs, name, hash, loadByHashDone, loadByHashData);
}

static void empty(Console* console) {}
static void emptyTrace(Console* console, const char* text, u8 color) {}
static void emptyError(Console* console, const char* text) {}

void initConsole(Console* console, Studio* studio, tic_fs* fs, tic_net* net, Config* config, StartArgs args)
{
    *console = (Console)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .fs = fs,
        .net = net,
        .config = config,
        .load = load,
        .loadByHash = loadByHash,
        .done = empty,
        .tick = empty,
        .updateProject = empty,
        .trace = emptyTrace,
        .error = emptyError,
    };
}

void freeConsole(Console* console)
{
    free(console);
}

void forceAutoSave(Console* console, const char* cart_name) {}
