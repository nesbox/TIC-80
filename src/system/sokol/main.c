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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "sokol.h"

#include "studio/system.h"
#include "crt.h"

#if defined(__TIC_WINDOWS__)
#include <windows.h>
#endif

#define CRT_SCALE 4

typedef struct
{
    Studio* studio;
    tic80_input input;

    struct
    {
        s32 x, y, dx, dy;
    } mouse;

    struct
    {
        bool state[tic_keys_count];
        char text;
    } keyboard;

    struct
    {
        saudio_desc desc;
        float* samples;
    } audio;

    struct
    {
        sg_image image;
        sg_shader shader;
        sgl_context ctx;
        sgl_pipeline pip;
        sg_attachments att;
    } crt;

    sg_image image;
    sg_sampler linear;
    sg_sampler nearest;

} App;

void tic_sys_clipboard_set(const char* text)
{
    sapp_set_clipboard_string(text);
}

bool tic_sys_clipboard_has()
{
    return sapp_get_clipboard_string() && *sapp_get_clipboard_string();
}

char* tic_sys_clipboard_get()
{
    return strdup(sapp_get_clipboard_string());
}

void tic_sys_clipboard_free(char* text)
{
    free(text);
}

u64 tic_sys_counter_get()
{
    return stm_now();
}

u64 tic_sys_freq_get()
{
    return 1000000000;
}

void tic_sys_fullscreen_set(bool value, void *userdata)
{
    App *app = userdata;
    sapp_toggle_fullscreen();

    // reset keyboard state to prevent key repeating
    ZEROMEM(app->keyboard.state);
}

bool tic_sys_fullscreen_get()
{
    return sapp_is_fullscreen();
}

void tic_sys_message(const char* title, const char* message)
{
}

void tic_sys_title(const char* title)
{
}

void tic_sys_open_path(const char* path) {}
void tic_sys_open_url(const char* url) {}

void tic_sys_preseed()
{
#if defined(__TIC_MACOSX__)
    srandom(time(NULL));
    random();
#else
    srand(time(NULL));
    rand();
#endif
}

void tic_sys_update_config()
{

}

void tic_sys_default_mapping(tic_mapping* mapping)
{
    *mapping = (tic_mapping)
    {
        tic_key_up,
        tic_key_down,
        tic_key_left,
        tic_key_right,

        tic_key_z, // a
        tic_key_x, // b
        tic_key_a, // x
        tic_key_s, // y
    };
}

bool tic_sys_keyboard_text(char* text, void *userdata)
{
    App *app = userdata;
    *text = app->keyboard.text;
    return true;
}

static void init(void *userdata)
{
    App *app = userdata;

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),

#if !defined(NDEBUG)
        .logger = {.func = slog_func},
#endif
    });

    sgl_setup(&(sgl_desc_t){
        .max_vertices = 6,
        .max_commands = 1,
        .sample_count = 1,
#if !defined(NDEBUG)
        .logger = {.func = slog_func},
#endif
    });

    stm_setup();

    app->image = sg_make_image(&(sg_image_desc)
    {
        .width = TIC80_FULLWIDTH,
        .height = TIC80_FULLHEIGHT,
        .usage = SG_USAGE_STREAM,
    });

    app->linear = sg_make_sampler(&(sg_sampler_desc)
    {
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    app->nearest = sg_make_sampler(&(sg_sampler_desc)
    {
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // init crt
    {
        app->crt.ctx = sgl_make_context(&(sgl_context_desc_t)
        {
            .max_vertices = 6,
            .max_commands = 1,
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_NONE,
            .sample_count = 1,
        });

        app->crt.image = sg_make_image(&(sg_image_desc)
        {
            .render_target = true,
            .width = TIC80_FULLWIDTH * CRT_SCALE,
            .height = TIC80_FULLHEIGHT * CRT_SCALE,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .sample_count = 1,
        });

        app->crt.att = sg_make_attachments(&(sg_attachments_desc)
        {
            .colors[0].image = app->crt.image,
        });

        app->crt.shader = sg_make_shader(crt_shader_desc(sg_query_backend()));

        app->crt.pip = sgl_context_make_pipeline(app->crt.ctx, &(sg_pipeline_desc)
        {
            .shader = app->crt.shader,
        });
    }

    app->audio.samples = malloc(sizeof app->audio.samples[0] * saudio_sample_rate() / TIC80_FRAMERATE * TIC80_SAMPLE_CHANNELS);
    app->mouse.x = app->mouse.y = TIC80_FULLWIDTH;
}

static void handleMouse(App *app)
{
    tic80_input *input = &app->input;

    if((bool)input->mouse.relative != sapp_mouse_locked())
    {
        sapp_lock_mouse(input->mouse.relative ? true : false);
    }

    sapp_show_mouse(app->mouse.x < 0 
        || app->mouse.y < 0 
        || app->mouse.x >= TIC80_FULLWIDTH 
        || app->mouse.y >= TIC80_FULLHEIGHT);

    if(sapp_mouse_shown())
    {
        input->mouse.x = input->mouse.y = -1;
    }
    else
    {
        if(sapp_mouse_locked())
        {
            input->mouse.rx = app->mouse.dx;
            input->mouse.ry = app->mouse.dy;
        }
        else
        {
            input->mouse.x = CLAMP(app->mouse.x, 0, TIC80_FULLWIDTH - 1);
            input->mouse.y = CLAMP(app->mouse.y, 0, TIC80_FULLHEIGHT - 1);            
        }
    }
}

static void handleKeyboard(App *app)
{
    tic80_input* input = &app->input;
    input->keyboard.data = 0;

    enum{BufSize = COUNT_OF(input->keyboard.keys)};

    for(s32 i = 0, c = 0; i < COUNT_OF(app->keyboard.state) && c < BufSize; i++)
        if(app->keyboard.state[i])
            input->keyboard.keys[c++] = i;
}

typedef struct
{
    float x, y, w, h;
} Rect;

static Rect viewport()
{
    float widthRatio = sapp_widthf() / TIC80_FULLWIDTH;
    float heightRatio = sapp_heightf() / TIC80_FULLHEIGHT;
    float optimalSize = widthRatio < heightRatio ? widthRatio : heightRatio;

    float w = TIC80_FULLWIDTH * optimalSize;
    float h = TIC80_FULLHEIGHT * optimalSize;
    float x = sapp_widthf() / 2 - w / 2;
    float y = sapp_heightf() / 2 - h / 2;

    return (Rect){x, y, w, h};
}

static void drawImage(Rect r,sg_image image, sg_sampler sampler)
{
    sgl_enable_texture();
    sgl_texture(image, sampler);
    sgl_begin_quads();
    sgl_v2f_t2f(r.x + 0,   r.y + 0,   0, 0);
    sgl_v2f_t2f(r.x + r.w, r.y + 0,   1, 0);
    sgl_v2f_t2f(r.x + r.w, r.y + r.h, 1, 1);
    sgl_v2f_t2f(r.x + 0,   r.y + r.h, 0, 1);
    sgl_end();
    sgl_disable_texture();
}

static void handleGamepad(App *app)
{    
    sgamepad_record_state();

    for(s32 i = 0; i < MIN(sizeof(tic80_gamepads), sgamepad_get_max_supported_gamepads()); i++)
    {
        sgamepad_gamepad_state state;
        sgamepad_get_gamepad_state(i, &state);

        if(state.left_stick.direction_x)
        {
            // !TODO: smth pressed
        }
    }
}

static void frame(void *userdata)
{
    App *app = userdata;

    if(studio_alive(app->studio))
    {
        sapp_quit();
    }

    tic80_input* input = &app->input;

    handleMouse(app);
    handleKeyboard(app);
    handleGamepad(app);
    studio_tick(app->studio, app->input);

    app->input.mouse.relative = studio_mem(app->studio)->ram->input.mouse.relative;

    app->mouse.dx = app->mouse.dy = 0;
    input->mouse.scrollx = input->mouse.scrolly = 0;
    app->keyboard.text = '\0';
    input->gamepads.data = 0;

    const tic80* product = &studio_mem(app->studio)->product;
    sg_update_image(app->image, &(sg_image_data)
    {
        .subimage[0][0] = 
        {
            product->screen, 
            TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof *product->screen,
        },
    });

    if(studio_config(app->studio)->options.crt)
    {
        sgl_set_context(app->crt.ctx);
        sgl_defaults();
        sgl_matrix_mode_modelview();
        sgl_ortho(0, TIC80_FULLWIDTH * CRT_SCALE, TIC80_FULLHEIGHT * CRT_SCALE, 0, -1, +1);
        sgl_push_pipeline();
        sgl_load_pipeline(app->crt.pip);

        drawImage((Rect)
            {
                0, 0, 
                TIC80_FULLWIDTH * CRT_SCALE, 
                TIC80_FULLHEIGHT * CRT_SCALE,
            },
            app->image, app->nearest);

        sgl_pop_pipeline();
    }

    studio_sound(app->studio);
    for(s32 i = 0; i < product->samples.count; i++)
        app->audio.samples[i] = (float)product->samples.buffer[i] / SHRT_MAX;

    saudio_push(app->audio.samples, product->samples.count / TIC80_SAMPLE_CHANNELS);

    sgl_set_context(sgl_default_context());
    sgl_defaults();

    sgl_matrix_mode_projection();
    sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, +1);

    if(studio_config(app->studio)->options.crt)
    {
        drawImage(viewport(), app->crt.image, app->linear);

        // draw crt
        sg_begin_pass(&(sg_pass)
        { 
            .attachments = app->crt.att,
        });

        sgl_context_draw(app->crt.ctx);
        sg_end_pass();
    }
    else
    {
        drawImage(viewport(), app->image, app->nearest);
    }

    // draw screen
    sg_begin_pass(&(sg_pass)
    {
        .action = 
        {
            .colors[0] = 
            {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = sg_make_color_1i(0xff0000ff),
            },
        },
        .swapchain = sglue_swapchain()
    });

    sgl_context_draw(sgl_default_context());
    sg_end_pass();

    sg_commit();

}

static void handleKeydown(App *app, sapp_keycode keycode, bool down)
{
    static const tic_keycode KeyboardCodes[] = 
    {
        [SAPP_KEYCODE_INVALID] = tic_key_unknown,
        [SAPP_KEYCODE_SPACE] = tic_key_space,
        [SAPP_KEYCODE_APOSTROPHE] = tic_key_apostrophe,
        [SAPP_KEYCODE_COMMA] = tic_key_comma,
        [SAPP_KEYCODE_MINUS] = tic_key_minus,
        [SAPP_KEYCODE_PERIOD] = tic_key_period,
        [SAPP_KEYCODE_SLASH] = tic_key_slash,
        [SAPP_KEYCODE_0] = tic_key_0,
        [SAPP_KEYCODE_1] = tic_key_1,
        [SAPP_KEYCODE_2] = tic_key_2,
        [SAPP_KEYCODE_3] = tic_key_3,
        [SAPP_KEYCODE_4] = tic_key_4,
        [SAPP_KEYCODE_5] = tic_key_5,
        [SAPP_KEYCODE_6] = tic_key_6,
        [SAPP_KEYCODE_7] = tic_key_7,
        [SAPP_KEYCODE_8] = tic_key_8,
        [SAPP_KEYCODE_9] = tic_key_9,
        [SAPP_KEYCODE_SEMICOLON] = tic_key_semicolon,
        [SAPP_KEYCODE_EQUAL] = tic_key_equals,
        [SAPP_KEYCODE_A] = tic_key_a,
        [SAPP_KEYCODE_B] = tic_key_b,
        [SAPP_KEYCODE_C] = tic_key_c,
        [SAPP_KEYCODE_D] = tic_key_d,
        [SAPP_KEYCODE_E] = tic_key_e,
        [SAPP_KEYCODE_F] = tic_key_f,
        [SAPP_KEYCODE_G] = tic_key_g,
        [SAPP_KEYCODE_H] = tic_key_h,
        [SAPP_KEYCODE_I] = tic_key_i,
        [SAPP_KEYCODE_J] = tic_key_j,
        [SAPP_KEYCODE_K] = tic_key_k,
        [SAPP_KEYCODE_L] = tic_key_l,
        [SAPP_KEYCODE_M] = tic_key_m,
        [SAPP_KEYCODE_N] = tic_key_n,
        [SAPP_KEYCODE_O] = tic_key_o,
        [SAPP_KEYCODE_P] = tic_key_p,
        [SAPP_KEYCODE_Q] = tic_key_q,
        [SAPP_KEYCODE_R] = tic_key_r,
        [SAPP_KEYCODE_S] = tic_key_s,
        [SAPP_KEYCODE_T] = tic_key_t,
        [SAPP_KEYCODE_U] = tic_key_u,
        [SAPP_KEYCODE_V] = tic_key_v,
        [SAPP_KEYCODE_W] = tic_key_w,
        [SAPP_KEYCODE_X] = tic_key_x,
        [SAPP_KEYCODE_Y] = tic_key_y,
        [SAPP_KEYCODE_Z] = tic_key_z,
        [SAPP_KEYCODE_LEFT_BRACKET] = tic_key_leftbracket,
        [SAPP_KEYCODE_BACKSLASH] = tic_key_backslash,
        [SAPP_KEYCODE_RIGHT_BRACKET] = tic_key_rightbracket,
        [SAPP_KEYCODE_GRAVE_ACCENT] = tic_key_grave,
        [SAPP_KEYCODE_WORLD_1] = tic_key_unknown,
        [SAPP_KEYCODE_WORLD_2] = tic_key_unknown,
        [SAPP_KEYCODE_ESCAPE] = tic_key_escape,
        [SAPP_KEYCODE_ENTER] = tic_key_return,
        [SAPP_KEYCODE_TAB] = tic_key_tab,
        [SAPP_KEYCODE_BACKSPACE] = tic_key_backspace,
        [SAPP_KEYCODE_INSERT] = tic_key_insert,
        [SAPP_KEYCODE_DELETE] = tic_key_delete,
        [SAPP_KEYCODE_RIGHT] = tic_key_right,
        [SAPP_KEYCODE_LEFT] = tic_key_left,
        [SAPP_KEYCODE_DOWN] = tic_key_down,
        [SAPP_KEYCODE_UP] = tic_key_up,
        [SAPP_KEYCODE_PAGE_UP] = tic_key_pageup,
        [SAPP_KEYCODE_PAGE_DOWN] = tic_key_pagedown,
        [SAPP_KEYCODE_HOME] = tic_key_home,
        [SAPP_KEYCODE_END] = tic_key_end,
        [SAPP_KEYCODE_CAPS_LOCK] = tic_key_capslock,
        [SAPP_KEYCODE_SCROLL_LOCK] = tic_key_unknown,
        [SAPP_KEYCODE_NUM_LOCK] = tic_key_unknown,
        [SAPP_KEYCODE_PRINT_SCREEN] = tic_key_unknown,
        [SAPP_KEYCODE_PAUSE] = tic_key_unknown,
        [SAPP_KEYCODE_F1] = tic_key_f1,
        [SAPP_KEYCODE_F2] = tic_key_f2,
        [SAPP_KEYCODE_F3] = tic_key_f3,
        [SAPP_KEYCODE_F4] = tic_key_f4,
        [SAPP_KEYCODE_F5] = tic_key_f5,
        [SAPP_KEYCODE_F6] = tic_key_f6,
        [SAPP_KEYCODE_F7] = tic_key_f7,
        [SAPP_KEYCODE_F8] = tic_key_f8,
        [SAPP_KEYCODE_F9] = tic_key_f9,
        [SAPP_KEYCODE_F10] = tic_key_f10,
        [SAPP_KEYCODE_F11] = tic_key_f11,
        [SAPP_KEYCODE_F12] = tic_key_f12,
        [SAPP_KEYCODE_F13] = tic_key_unknown,
        [SAPP_KEYCODE_F14] = tic_key_unknown,
        [SAPP_KEYCODE_F15] = tic_key_unknown,
        [SAPP_KEYCODE_F16] = tic_key_unknown,
        [SAPP_KEYCODE_F17] = tic_key_unknown,
        [SAPP_KEYCODE_F18] = tic_key_unknown,
        [SAPP_KEYCODE_F19] = tic_key_unknown,
        [SAPP_KEYCODE_F20] = tic_key_unknown,
        [SAPP_KEYCODE_F21] = tic_key_unknown,
        [SAPP_KEYCODE_F22] = tic_key_unknown,
        [SAPP_KEYCODE_F23] = tic_key_unknown,
        [SAPP_KEYCODE_F24] = tic_key_unknown,
        [SAPP_KEYCODE_F25] = tic_key_unknown,
        [SAPP_KEYCODE_KP_0] = tic_key_numpad0,
        [SAPP_KEYCODE_KP_1] = tic_key_numpad1,
        [SAPP_KEYCODE_KP_2] = tic_key_numpad2,
        [SAPP_KEYCODE_KP_3] = tic_key_numpad3,
        [SAPP_KEYCODE_KP_4] = tic_key_numpad4,
        [SAPP_KEYCODE_KP_5] = tic_key_numpad5,
        [SAPP_KEYCODE_KP_6] = tic_key_numpad6,
        [SAPP_KEYCODE_KP_7] = tic_key_numpad7,
        [SAPP_KEYCODE_KP_8] = tic_key_numpad8,
        [SAPP_KEYCODE_KP_9] = tic_key_numpad9,
        [SAPP_KEYCODE_KP_DECIMAL] = tic_key_numpadperiod,
        [SAPP_KEYCODE_KP_DIVIDE] = tic_key_numpaddivide,
        [SAPP_KEYCODE_KP_MULTIPLY] = tic_key_numpadmultiply,
        [SAPP_KEYCODE_KP_SUBTRACT] = tic_key_numpadminus,
        [SAPP_KEYCODE_KP_ADD] = tic_key_numpadplus,
        [SAPP_KEYCODE_KP_ENTER] = tic_key_numpadenter,
        [SAPP_KEYCODE_KP_EQUAL] = tic_key_equals,
        [SAPP_KEYCODE_LEFT_SHIFT] = tic_key_shift,
        [SAPP_KEYCODE_LEFT_CONTROL] = tic_key_ctrl,
        [SAPP_KEYCODE_LEFT_ALT] = tic_key_alt,
        [SAPP_KEYCODE_LEFT_SUPER] = tic_key_ctrl,
        [SAPP_KEYCODE_RIGHT_SHIFT] = tic_key_shift,
        [SAPP_KEYCODE_RIGHT_CONTROL] = tic_key_ctrl,
        [SAPP_KEYCODE_RIGHT_ALT] = tic_key_alt,
        [SAPP_KEYCODE_RIGHT_SUPER] = tic_key_ctrl,
        [SAPP_KEYCODE_MENU] = tic_key_unknown,

    };

    tic_key key = KeyboardCodes[keycode];

    if(key > tic_key_unknown)
    {
        // ALT+TAB fix
        if(key == tic_key_tab && app->keyboard.state[tic_key_alt])
            app->keyboard.state[tic_key_alt] = false;

        app->keyboard.state[key] = down;
    }
}

static void processMouse(App *app, sapp_mousebutton btn, s32 down)
{
    tic80_input* input = &app->input;

    switch(btn)
    {
    case SAPP_MOUSEBUTTON_LEFT: input->mouse.left = down; break;
    case SAPP_MOUSEBUTTON_MIDDLE: input->mouse.middle = down; break;
    case SAPP_MOUSEBUTTON_RIGHT: input->mouse.right = down; break;
    default: break;
    }
}

static void event(const sapp_event* event, void *userdata)
{
    App *app = userdata;
    tic80_input* input = &app->input;

    switch(event->type)
    {
    case SAPP_EVENTTYPE_KEY_DOWN:
        handleKeydown(app, event->key_code, true);
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        handleKeydown(app, event->key_code, false);
        break;
    case SAPP_EVENTTYPE_CHAR:
        if(event->char_code >= 32 && event->char_code < 127)
        {
            app->keyboard.text = event->char_code;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_LEAVE:
        app->mouse.x = app->mouse.y = TIC80_FULLWIDTH;
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        {
            Rect r = viewport();

            app->mouse.x = (event->mouse_x - r.x) * TIC80_FULLWIDTH / r.w;
            app->mouse.y = (event->mouse_y - r.y) * TIC80_FULLHEIGHT / r.h;
            app->mouse.dx = event->mouse_dx;
            app->mouse.dy = event->mouse_dy;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN: 
        processMouse(app, event->mouse_button, 1); break;
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        processMouse(app, event->mouse_button, 0); break;
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        input->mouse.scrolly = event->scroll_y > 0 ? 1 : -1;
        break;
    default:
        break;
    }
}

static void cleanup(void *userdata)
{
    App *app = userdata;

    studio_delete(app->studio);
    free(app->audio.samples);

    // destroy crt
    {
        sgl_destroy_pipeline(app->crt.pip);
        sg_destroy_shader(app->crt.shader);
        sg_destroy_attachments(app->crt.att);
        sg_destroy_image(app->crt.image);
        sgl_destroy_context(app->crt.ctx);
    }

    sg_destroy_image(app->image);
    sg_destroy_sampler(app->linear);
    sg_destroy_sampler(app->nearest);

    free(app);

    sgl_shutdown();
    sg_shutdown();
    saudio_shutdown();
}

sapp_desc sokol_main(s32 argc, char* argv[])
{
#if defined(__TIC_WINDOWS__)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) && !info.dwCursorPosition.X && !info.dwCursorPosition.Y)
            FreeConsole();
    }
#endif

    App *app = NEW(App);
    memset(app, 0, sizeof *app);

    app->audio.desc.num_channels = TIC80_SAMPLE_CHANNELS;
    saudio_setup(&app->audio.desc);
    sgamepad_init();

    app->studio = studio_create(argc, argv, saudio_sample_rate(), TIC80_PIXEL_COLOR_RGBA8888, "./", INT32_MAX, tic_layout_qwerty, app);

    if(studio_config(app->studio)->cli)
    {
        while (!studio_alive(app->studio))
            studio_tick(app->studio, app->input);

        exit(0);
    }

    return(sapp_desc)
    {
        .user_data = app,

        .init_userdata_cb = init,
        .frame_userdata_cb = frame,
        .cleanup_userdata_cb = cleanup,
        .event_userdata_cb = event,

        .width = TIC80_FULLWIDTH * studio_config(app->studio)->uiScale,
        .height = TIC80_FULLHEIGHT * studio_config(app->studio)->uiScale,
        .window_title = TIC_TITLE,
        .ios_keyboard_resizes_canvas = true,
        .high_dpi = true,
#if !defined(NDEBUG)
        .logger = {.func = slog_func},
#endif
        .enable_clipboard = true,
        .clipboard_size = TIC_CODE_SIZE,
    };
}
