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
#include <time.h>
#include <limits.h>

#include "studio/system.h"
#include "system/sokol/sokol.h"
#include "ext/net.h"

static struct
{
    Studio* studio;

    struct
    {
        bool state[tic_keys_count];
    } keyboard;

    struct
    {
        saudio_desc desc;
        float* samples;
    } audio;

    char* clipboard;

    Net* net;

} platform;

static void setClipboardText(const char* text)
{
    if(platform.clipboard)
    {
        free(platform.clipboard);
        platform.clipboard = NULL;
    }

    platform.clipboard = strdup(text);
}

static bool hasClipboardText()
{
    return platform.clipboard != NULL;
}

static char* getClipboardText()
{
    return platform.clipboard ? strdup(platform.clipboard) : NULL;
}

static void freeClipboardText(const char* text)
{
    free((void*)text);
}

static u64 getPerformanceCounter()
{
    return stm_now();
}

static u64 getPerformanceFrequency()
{
    return 1000000000;
}

static void* httpGetSync(const char* url, s32* size)
{
    return netGetSync(platform.net, url, size);
}

static void httpGet(const char* url, HttpGetCallback callback, void* calldata)
{
    return netGet(platform.net, url, callback, calldata);
}

static void goFullscreen()
{
}

static void showMessageBox(const char* title, const char* message)
{
}

static void setWindowTitle(const char* title)
{
}

static void openSystemPath(const char* path)
{

}

static void preseed()
{
#if defined(__TIC_MACOSX__)
    srandom(time(NULL));
    random();
#else
    srand(time(NULL));
    rand();
#endif
}

static void pollEvent()
{

}

static void updateConfig()
{

}

static System systemInterface = 
{
    .setClipboardText = setClipboardText,
    .hasClipboardText = hasClipboardText,
    .getClipboardText = getClipboardText,
    .freeClipboardText = freeClipboardText,

    .getPerformanceCounter = getPerformanceCounter,
    .getPerformanceFrequency = getPerformanceFrequency,

    .httpGetSync = httpGetSync,
    .httpGet = httpGet,

    .goFullscreen = goFullscreen,
    .showMessageBox = showMessageBox,
    .setWindowTitle = setWindowTitle,

    .openSystemPath = openSystemPath,
    .preseed = preseed,
    .poll = pollEvent,
    .updateConfig = updateConfig,
};

static void app_init(void)
{
    sokol_gfx_init(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, 1, 1, false, true);

    stm_setup();

    platform.audio.samples = calloc(sizeof platform.audio.samples[0], saudio_sample_rate() / TIC80_FRAMERATE * TIC_STEREO_CHANNELS);
}

static void handleKeyboard()
{
    tic_mem* tic = platform.studio->tic;

    tic80_input* input = &tic->ram.input;
    input->keyboard.data = 0;

    enum{BufSize = COUNT_OF(input->keyboard.keys)};

    for(s32 i = 0, c = 0; i < COUNT_OF(platform.keyboard.state) && c < BufSize; i++)
        if(platform.keyboard.state[i])
            input->keyboard.keys[c++] = i;
}

static void app_frame(void)
{
    if(platform.studio->quit) exit(0);

    netTick(platform.net);

    tic_mem* tic = platform.studio->tic;
    tic80_input* input = &tic->ram.input;

    input->gamepads.data = 0;
    handleKeyboard();
    platform.studio->tick(input);

    sokol_gfx_draw(platform.studio->tic->screen);

    s32 count = tic->samples.size / sizeof tic->samples.buffer[0];
    for(s32 i = 0; i < count; i++)
        platform.audio.samples[i] = (float)tic->samples.buffer[i] / SHRT_MAX;

    saudio_push(platform.audio.samples, count / 2);
    
    input->mouse.scrollx = input->mouse.scrolly = 0;
}

static void handleKeydown(sapp_keycode keycode, bool down)
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
        [SAPP_KEYCODE_KP_0] = tic_key_0,
        [SAPP_KEYCODE_KP_1] = tic_key_1,
        [SAPP_KEYCODE_KP_2] = tic_key_2,
        [SAPP_KEYCODE_KP_3] = tic_key_3,
        [SAPP_KEYCODE_KP_4] = tic_key_4,
        [SAPP_KEYCODE_KP_5] = tic_key_5,
        [SAPP_KEYCODE_KP_6] = tic_key_6,
        [SAPP_KEYCODE_KP_7] = tic_key_7,
        [SAPP_KEYCODE_KP_8] = tic_key_8,
        [SAPP_KEYCODE_KP_9] = tic_key_9,
        [SAPP_KEYCODE_KP_DECIMAL] = tic_key_unknown,
        [SAPP_KEYCODE_KP_DIVIDE] = tic_key_unknown,
        [SAPP_KEYCODE_KP_MULTIPLY] = tic_key_unknown,
        [SAPP_KEYCODE_KP_SUBTRACT] = tic_key_unknown,
        [SAPP_KEYCODE_KP_ADD] = tic_key_unknown,
        [SAPP_KEYCODE_KP_ENTER] = tic_key_return,
        [SAPP_KEYCODE_KP_EQUAL] = tic_key_equals,
        [SAPP_KEYCODE_LEFT_SHIFT] = tic_key_shift,
        [SAPP_KEYCODE_LEFT_CONTROL] = tic_key_ctrl,
        [SAPP_KEYCODE_LEFT_ALT] = tic_key_alt,
        [SAPP_KEYCODE_LEFT_SUPER] = tic_key_unknown,
        [SAPP_KEYCODE_RIGHT_SHIFT] = tic_key_shift,
        [SAPP_KEYCODE_RIGHT_CONTROL] = tic_key_ctrl,
        [SAPP_KEYCODE_RIGHT_ALT] = tic_key_alt,
        [SAPP_KEYCODE_RIGHT_SUPER] = tic_key_unknown,
        [SAPP_KEYCODE_MENU] = tic_key_unknown,

    };

    tic_key key = KeyboardCodes[keycode];

    if(key > tic_key_unknown)
    {
        // ALT+TAB fix
        if(key == tic_key_tab && platform.keyboard.state[tic_key_alt])
            platform.keyboard.state[tic_key_alt] = false;

        platform.keyboard.state[key] = down;
    }
}

static void processMouse(sapp_mousebutton btn, s32 down)
{
    tic_mem* tic = platform.studio->tic;
    tic80_input* input = &tic->ram.input;


    switch(btn)
    {
    case SAPP_MOUSEBUTTON_LEFT: input->mouse.left = down; break;
    case SAPP_MOUSEBUTTON_MIDDLE: input->mouse.middle = down; break;
    case SAPP_MOUSEBUTTON_RIGHT: input->mouse.right = down; break;
    default: break;
    }
}

static void app_input(const sapp_event* event)
{
    tic_mem* tic = platform.studio->tic;
    tic80_input* input = &tic->ram.input;

    switch(event->type)
    {
    case SAPP_EVENTTYPE_KEY_DOWN:
        handleKeydown(event->key_code, true);
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        handleKeydown(event->key_code, false);
        break;
    case SAPP_EVENTTYPE_CHAR:
        if(event->char_code < 128)
            platform.studio->text = event->char_code;
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        {
            struct {s32 x, y, w, h;}rect;
            sokol_calc_viewport(&rect.x, &rect.y, &rect.w, &rect.h);

            if(rect.w) input->mouse.x = ((s32)event->mouse_x - rect.x) * TIC80_FULLWIDTH / rect.w;
            if(rect.h) input->mouse.y = ((s32)event->mouse_y - rect.y) * TIC80_FULLHEIGHT / rect.h;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN: 
        processMouse(event->mouse_button, 1); break;
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        processMouse(event->mouse_button, 0); break;
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        input->mouse.scrollx = event->scroll_x;
        input->mouse.scrolly = event->scroll_y;
        break;
    default:
        break;
    }
}

static void app_cleanup(void)
{
    platform.studio->close();
    closeNet(platform.net);
    free(platform.audio.samples);
}

sapp_desc sokol_main(s32 argc, char* argv[])
{
    memset(&platform, 0, sizeof platform);

    platform.audio.desc.num_channels = TIC_STEREO_CHANNELS;
    saudio_setup(&platform.audio.desc);

    platform.net = createNet();

    platform.studio = studioInit(argc, argv, saudio_sample_rate(), "./", &systemInterface);

    const s32 Width = TIC80_FULLWIDTH * platform.studio->config()->uiScale;
    const s32 Height = TIC80_FULLHEIGHT * platform.studio->config()->uiScale;

    return(sapp_desc)
    {
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = Width,
        .height = Height,
        .window_title = TIC_TITLE,
        .ios_keyboard_resizes_canvas = true,
        .high_dpi = true,
    };
}
