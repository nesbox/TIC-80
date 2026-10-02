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

#include "studio/system.h"
#include "tools.h"

#include "sokol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct
{
    const char* appFolder;
    sg_pass_action pass_action;
} platform;

// Where the studio keeps its files, the same place the SDL layer picks:
// the platform's per-application data folder.
static const char* getAppFolder(void)
{
    static char appFolder[TICNAME_MAX];

#if defined(TIC_DATA_PATH)

    snprintf(appFolder, sizeof appFolder, "%s", TIC_DATA_PATH);

#elif defined(__EMSCRIPTEN__)

    // Mounted from IndexedDB by the page, see the change's M1 notes.
    snprintf(appFolder, sizeof appFolder, "/" TIC_PACKAGE "/" TIC_NAME "/");

#elif defined(__APPLE__)

    snprintf(appFolder, sizeof appFolder, "%s/Library/Application Support/" TIC_PACKAGE "/" TIC_NAME "/", getenv("HOME"));

#elif defined(_WIN32)

    snprintf(appFolder, sizeof appFolder, "%s\\" TIC_PACKAGE "\\" TIC_NAME "\\", getenv("APPDATA"));

#else

    const char* data = getenv("XDG_DATA_HOME");

    if (data && *data)
        snprintf(appFolder, sizeof appFolder, "%s/" TIC_PACKAGE "/" TIC_NAME "/", data);
    else
        snprintf(appFolder, sizeof appFolder, "%s/.local/share/" TIC_PACKAGE "/" TIC_NAME "/", getenv("HOME"));

#endif

    return appFolder;
}

static void init_cb(void)
{
    stm_setup();

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    platform.appFolder = getAppFolder();
    platform.pass_action.colors[0].clear_value = (sg_color){ 0.1f, 0.11f, 0.17f, 1.0f };

    printf("TIC-80 data folder: %s\n", platform.appFolder);
}

static void frame_cb(void)
{
    sg_begin_pass(&(sg_pass){ .action = platform.pass_action, .swapchain = sglue_swapchain() });
    sg_end_pass();
    sg_commit();
}

static void cleanup_cb(void)
{
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[])
{
    TIC_UNUSED(argc);
    TIC_UNUSED(argv);

    return (sapp_desc){
        .init_cb = init_cb,
        .frame_cb = frame_cb,
        .cleanup_cb = cleanup_cb,
        .width = TIC80_FULLWIDTH,
        .height = TIC80_FULLHEIGHT,
        .window_title = TIC_TITLE,
        .logger.func = slog_func,
        .high_dpi = false,
        .swap_interval = 1,
        .html5_canvas_resize = false,
    };
}
