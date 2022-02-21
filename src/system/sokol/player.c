#include "system/sokol/sokol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <tic80.h>

#define TIC80_WINDOW_TITLE "TIC-80"
#define TIC80_DEFAULT_CART "cart.tic"

static tic80* tic = NULL;

static void app_init(void)
{
    saudio_desc desc = {0};
    desc.num_channels = 2;
    saudio_setup(&desc);

    FILE* file = fopen(TIC80_DEFAULT_CART, "rb");

    if(file)
    {
        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);

        void* cart = malloc(size);
        if(cart) fread(cart, size, 1, file);
        fclose(file);

        if(cart)
        {
            printf("%s\n", "cart loaded");
            tic = tic80_create(saudio_sample_rate(), TIC80_PIXEL_COLOR_RGBA8888);

            if(tic)
            {
                printf("%s\n", "tic created");
                tic80_load(tic, cart, size);
            }
        }
    }

    sokol_gfx_init(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, 1, 1, false, false);

    stm_setup();
}

static tic80_input tic_input;

static void app_frame(void)
{
    if(tic)
    {
        tic80_tick(tic, tic_input);
        tic80_sound(tic);        
    }

    sokol_gfx_draw(tic->screen);

    static float floatSamples[TIC80_SAMPLERATE * TIC80_SAMPLE_CHANNELS / TIC80_FRAMERATE];

    for(s32 i = 0; i < tic->samples.count; i++)
        floatSamples[i] = (float)tic->samples.buffer[i] / SHRT_MAX;

    saudio_push(floatSamples, tic->samples.count / TIC80_SAMPLE_CHANNELS);
}

static void app_input(const sapp_event* event)
{
    static const sapp_keycode Keys[] = 
    { 
        SAPP_KEYCODE_UP,
        SAPP_KEYCODE_DOWN,
        SAPP_KEYCODE_LEFT,
        SAPP_KEYCODE_RIGHT,

        SAPP_KEYCODE_Z,
        SAPP_KEYCODE_X,
        SAPP_KEYCODE_A,
        SAPP_KEYCODE_S,
    };

    switch (event->type)
    {
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:

        for (int i = 0; i < sizeof Keys / sizeof Keys[0]; i++)
        {
            if (event->key_code == Keys[i])
            {
                if(event->type == SAPP_EVENTTYPE_KEY_DOWN)
                    tic_input.gamepads.first.data |= (1 << i);
                else
                    tic_input.gamepads.first.data &= ~(1 << i);
            }
        }
        break;
    default:
        break;
    }
}

static void app_cleanup(void)
{
    if(tic)
        tic80_delete(tic);
    
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    // args_init(argc, argv);
    return (sapp_desc) {
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = 3 * TIC80_FULLWIDTH,
        .height = 3 * TIC80_FULLHEIGHT,
        .window_title = TIC80_WINDOW_TITLE,
        .ios_keyboard_resizes_canvas = true
    };
}
