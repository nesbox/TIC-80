#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <tic80.h>

sapp_desc sokol_main(int argc, char* argv[])
{
    return (sapp_desc) {0};
    //     .init_cb = app_init,
    //     .frame_cb = app_frame,
    //     .event_cb = app_input,
    //     .cleanup_cb = app_cleanup,
    //     .width = 3 * TIC80_FULLWIDTH,
    //     .height = 3 * TIC80_FULLHEIGHT,
    //     .window_title = "TIC-80 with Sokol renderer",
    //     .ios_keyboard_resizes_canvas = true
    // };
}
