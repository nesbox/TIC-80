#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"

static void app_init(void)
{

}

static void app_frame(void)
{

}

static void app_input(const sapp_event* event)
{

}

static void app_cleanup(void)
{

}

sapp_desc sokol_main(int argc, char* argv[]) {
    // args_init(argc, argv);
    return (sapp_desc) {
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = 3 * 256,
        .height = 3 * 144,
        .window_title = "TIC-80",
        .ios_keyboard_resizes_canvas = true
    };
}