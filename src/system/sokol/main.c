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
#include "render.h"
#include "controls.h"
#include "sokol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

// The machine runs at sixty ticks a second whatever the display does; a frame
// after a stall replays at most this many ticks of the backlog.
#define MAX_CATCH_UP 4

// How long the controls stay out after the last touch.
#define TOUCH_TIMEOUT 10.0f

static struct
{
    Studio* studio;
    tic80_input input;
    const char* appFolder;

    s32 argc;
    char** argv;

    double accumulator;
    uint64_t lastTime;

    struct
    {
        bool state[tic_keys_count];
        bool pressed[tic_keys_count];
        char text;
    } keyboard;

    struct
    {
        ControlsPointer list[CONTROLS_MAX_POINTERS];
        s32   count;
        float timeout;
    } touch;

    bool fullscreen;
    bool layoutKnown;
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

// sokol's key codes are physical positions, the same as SDL's scancodes, so
// this table is the whole keyboard translation.
static const u8 Keymap[] =
{
    [SAPP_KEYCODE_SPACE]            = tic_key_space,
    [SAPP_KEYCODE_APOSTROPHE]       = tic_key_apostrophe,
    [SAPP_KEYCODE_COMMA]            = tic_key_comma,
    [SAPP_KEYCODE_MINUS]            = tic_key_minus,
    [SAPP_KEYCODE_PERIOD]           = tic_key_period,
    [SAPP_KEYCODE_SLASH]            = tic_key_slash,
    [SAPP_KEYCODE_0]                = tic_key_0,
    [SAPP_KEYCODE_1]                = tic_key_1,
    [SAPP_KEYCODE_2]                = tic_key_2,
    [SAPP_KEYCODE_3]                = tic_key_3,
    [SAPP_KEYCODE_4]                = tic_key_4,
    [SAPP_KEYCODE_5]                = tic_key_5,
    [SAPP_KEYCODE_6]                = tic_key_6,
    [SAPP_KEYCODE_7]                = tic_key_7,
    [SAPP_KEYCODE_8]                = tic_key_8,
    [SAPP_KEYCODE_9]                = tic_key_9,
    [SAPP_KEYCODE_SEMICOLON]        = tic_key_semicolon,
    [SAPP_KEYCODE_EQUAL]            = tic_key_equals,
    [SAPP_KEYCODE_A]                = tic_key_a,
    [SAPP_KEYCODE_B]                = tic_key_b,
    [SAPP_KEYCODE_C]                = tic_key_c,
    [SAPP_KEYCODE_D]                = tic_key_d,
    [SAPP_KEYCODE_E]                = tic_key_e,
    [SAPP_KEYCODE_F]                = tic_key_f,
    [SAPP_KEYCODE_G]                = tic_key_g,
    [SAPP_KEYCODE_H]                = tic_key_h,
    [SAPP_KEYCODE_I]                = tic_key_i,
    [SAPP_KEYCODE_J]                = tic_key_j,
    [SAPP_KEYCODE_K]                = tic_key_k,
    [SAPP_KEYCODE_L]                = tic_key_l,
    [SAPP_KEYCODE_M]                = tic_key_m,
    [SAPP_KEYCODE_N]                = tic_key_n,
    [SAPP_KEYCODE_O]                = tic_key_o,
    [SAPP_KEYCODE_P]                = tic_key_p,
    [SAPP_KEYCODE_Q]                = tic_key_q,
    [SAPP_KEYCODE_R]                = tic_key_r,
    [SAPP_KEYCODE_S]                = tic_key_s,
    [SAPP_KEYCODE_T]                = tic_key_t,
    [SAPP_KEYCODE_U]                = tic_key_u,
    [SAPP_KEYCODE_V]                = tic_key_v,
    [SAPP_KEYCODE_W]                = tic_key_w,
    [SAPP_KEYCODE_X]                = tic_key_x,
    [SAPP_KEYCODE_Y]                = tic_key_y,
    [SAPP_KEYCODE_Z]                = tic_key_z,
    [SAPP_KEYCODE_LEFT_BRACKET]     = tic_key_leftbracket,
    [SAPP_KEYCODE_BACKSLASH]        = tic_key_backslash,
    [SAPP_KEYCODE_RIGHT_BRACKET]    = tic_key_rightbracket,
    [SAPP_KEYCODE_GRAVE_ACCENT]     = tic_key_grave,
    [SAPP_KEYCODE_ESCAPE]           = tic_key_escape,
    [SAPP_KEYCODE_ENTER]            = tic_key_return,
    [SAPP_KEYCODE_TAB]              = tic_key_tab,
    [SAPP_KEYCODE_BACKSPACE]        = tic_key_backspace,
    [SAPP_KEYCODE_INSERT]           = tic_key_insert,
    [SAPP_KEYCODE_DELETE]           = tic_key_delete,
    [SAPP_KEYCODE_RIGHT]            = tic_key_right,
    [SAPP_KEYCODE_LEFT]             = tic_key_left,
    [SAPP_KEYCODE_DOWN]             = tic_key_down,
    [SAPP_KEYCODE_UP]               = tic_key_up,
    [SAPP_KEYCODE_PAGE_UP]          = tic_key_pageup,
    [SAPP_KEYCODE_PAGE_DOWN]        = tic_key_pagedown,
    [SAPP_KEYCODE_HOME]             = tic_key_home,
    [SAPP_KEYCODE_END]              = tic_key_end,
    [SAPP_KEYCODE_CAPS_LOCK]        = tic_key_capslock,
    [SAPP_KEYCODE_F1]               = tic_key_f1,
    [SAPP_KEYCODE_F2]               = tic_key_f2,
    [SAPP_KEYCODE_F3]               = tic_key_f3,
    [SAPP_KEYCODE_F4]               = tic_key_f4,
    [SAPP_KEYCODE_F5]               = tic_key_f5,
    [SAPP_KEYCODE_F6]               = tic_key_f6,
    [SAPP_KEYCODE_F7]               = tic_key_f7,
    [SAPP_KEYCODE_F8]               = tic_key_f8,
    [SAPP_KEYCODE_F9]               = tic_key_f9,
    [SAPP_KEYCODE_F10]              = tic_key_f10,
    [SAPP_KEYCODE_F11]              = tic_key_f11,
    [SAPP_KEYCODE_F12]              = tic_key_f12,
    [SAPP_KEYCODE_KP_0]             = tic_key_numpad0,
    [SAPP_KEYCODE_KP_1]             = tic_key_numpad1,
    [SAPP_KEYCODE_KP_2]             = tic_key_numpad2,
    [SAPP_KEYCODE_KP_3]             = tic_key_numpad3,
    [SAPP_KEYCODE_KP_4]             = tic_key_numpad4,
    [SAPP_KEYCODE_KP_5]             = tic_key_numpad5,
    [SAPP_KEYCODE_KP_6]             = tic_key_numpad6,
    [SAPP_KEYCODE_KP_7]             = tic_key_numpad7,
    [SAPP_KEYCODE_KP_8]             = tic_key_numpad8,
    [SAPP_KEYCODE_KP_9]             = tic_key_numpad9,
    [SAPP_KEYCODE_KP_ADD]           = tic_key_numpadplus,
    [SAPP_KEYCODE_KP_SUBTRACT]      = tic_key_numpadminus,
    [SAPP_KEYCODE_KP_MULTIPLY]      = tic_key_numpadmultiply,
    [SAPP_KEYCODE_KP_DIVIDE]        = tic_key_numpaddivide,
    [SAPP_KEYCODE_KP_ENTER]         = tic_key_numpadenter,
    [SAPP_KEYCODE_KP_DECIMAL]       = tic_key_numpadperiod,
    [SAPP_KEYCODE_LEFT_SHIFT]       = tic_key_shift,
    [SAPP_KEYCODE_RIGHT_SHIFT]      = tic_key_shift,
    [SAPP_KEYCODE_LEFT_CONTROL]     = tic_key_ctrl,
    [SAPP_KEYCODE_RIGHT_CONTROL]    = tic_key_ctrl,
    [SAPP_KEYCODE_LEFT_ALT]         = tic_key_alt,
    [SAPP_KEYCODE_RIGHT_ALT]        = tic_key_alt,
};

static tic_key translate_key(sapp_keycode code)
{
    return code < COUNT_OF(Keymap) ? (tic_key)Keymap[code] : tic_key_unknown;
}

static void handle_key(sapp_keycode code, bool down)
{
    const tic_key key = translate_key(code);

    if (key == tic_key_unknown)
        return;

    // A key that goes down and up between two ticks is still reported for one.
    if (down && !platform.keyboard.state[key])
        platform.keyboard.pressed[key] = true;

    platform.keyboard.state[key] = down;
}

static void update_mouse(float x, float y)
{
    float rx, ry, rw, rh;
    render_player_rect(platform.studio, &rx, &ry, &rw, &rh);

    const tic_point m =
    {
        (s32)((x - rx) * TIC80_FULLWIDTH / rw),
        (s32)((y - ry) * TIC80_FULLHEIGHT / rh)
    };

    // The mouse is only on the machine's screen when it is inside it.
    if (m.x < 0 || m.y < 0 || m.x >= TIC80_FULLWIDTH || m.y >= TIC80_FULLHEIGHT)
        return;

    platform.input.mouse.x = m.x;
    platform.input.mouse.y = m.y;
}

// Handles the keyboard state over to the tick's input, and clears what a tick
// has consumed: a press that arrives between ticks is not reported twice.
static void build_input(void)
{
    tic80_input* input = &platform.input;
    const ControlsState* controls = controls_state();
    s32 c = 0;

    for (tic_key i = 0; i < tic_keys_count && c < TIC80_KEY_BUFFER; i++)
        if (platform.keyboard.state[i] || platform.keyboard.pressed[i])
            input->keyboard.keys[c++] = i;

    while (c < TIC80_KEY_BUFFER)
        input->keyboard.keys[c++] = tic_key_unknown;

    memset(platform.keyboard.pressed, 0, sizeof platform.keyboard.pressed);

    // The on-screen controls are the first gamepad; the menu control is ESC,
    // the way the SDL layer's BACK button is.
    input->gamepads.data = controls->visible ? controls->gamepad.data : 0;

    if (controls->menu)
        input->keyboard.keys[0] = tic_key_escape;
}

// What the on-screen controls need to know, gathered from the studio's state.
static ControlsMode controls_mode(void)
{
    // The cart's `-- input:` tag; with no tag every bit is set and the
    // controls are a gamepad, which is what most carts play on.
    const tic_mem* tic = studio_mem(platform.studio);

    if (tic->input.gamepad)
        return controls_mode_gamepad;

    if (tic->input.keyboard)
        return controls_mode_keyboard;

    return controls_mode_none;
}

static void controls_frame(float dt)
{
    if (platform.touch.timeout > 0.0f)
        platform.touch.timeout = MAX(platform.touch.timeout - dt, 0.0f);

    const ControlsInput input = {
        .width = (float)sapp_width(),
        .height = (float)sapp_height(),
        .insets = { 0 },
        .pointerCount = platform.touch.count,
        .mode = controls_mode(),
        .portrait = sapp_height() > sapp_width(),
        .visible = platform.touch.timeout > 0.0f,
        .dt = dt,
        .alpha = studio_config(platform.studio)->theme.gamepad.touch.alpha,
    };

    memcpy((void*)input.pointers, platform.touch.list, sizeof input.pointers);

    controls_update(&input);
}

static void push_audio(void)
{
    const tic_mem* tic = studio_mem(platform.studio);

    enum { MaxSamples = 8192 };
    static float buffer[MaxSamples];

    s32 count = tic->product.samples.count;

    if (count > MaxSamples)
        count = MaxSamples;

    for (s32 i = 0; i < count; i++)
        buffer[i] = tic->product.samples.buffer[i] / 32768.0f;

    saudio_push(buffer, count / TIC80_SAMPLE_CHANNELS);
}

static void tick(void)
{
    build_input();

    studio_tick(platform.studio, platform.input);
    studio_sound(platform.studio);

    push_audio();

    platform.keyboard.text = '\0';
}

// How large the studio's own UI may be drawn on this display.
static s32 max_scale(void)
{
    const s32 scale = MIN(sapp_width() / TIC80_FULLWIDTH, sapp_height() / TIC80_FULLHEIGHT);
    return scale > 0 ? scale : 1;
}

// Which layout the keyboard is in, guessed from a typed character: sokol's key
// codes are positions, so a position that types something else names the layout.
static void learn_layout(sapp_keycode code, uint32_t unicode)
{
    static const struct { sapp_keycode code; char us; tic_layout layout; } Probes[] =
    {
        { SAPP_KEYCODE_Q, 'q', tic_layout_qwerty },
        { SAPP_KEYCODE_W, 'w', tic_layout_qwerty },
        { SAPP_KEYCODE_Q, 'a', tic_layout_azerty },
        { SAPP_KEYCODE_W, 'z', tic_layout_azerty },
        { SAPP_KEYCODE_Y, 'z', tic_layout_qwertz },
        { SAPP_KEYCODE_Q, 'x', tic_layout_de_neo },
        { SAPP_KEYCODE_Q, 'j', tic_layout_de_bone },
    };

    if (platform.layoutKnown || unicode > 0x7f)
        return;

    for (s32 i = 0; i < COUNT_OF(Probes); i++)
        if (Probes[i].code == code && (char)unicode == Probes[i].us)
        {
            platform.layoutKnown = true;
            studio_keymapchanged(platform.studio, Probes[i].layout);
            return;
        }
}

static void event_cb(const sapp_event* event)
{
    switch (event->type)
    {
    case SAPP_EVENTTYPE_KEY_DOWN:
        handle_key(event->key_code, true);
        break;

    case SAPP_EVENTTYPE_KEY_UP:
        handle_key(event->key_code, false);
        break;

    case SAPP_EVENTTYPE_CHAR:
        platform.keyboard.text = (char)event->char_code;
        learn_layout(event->key_code, event->char_code);
        break;

    case SAPP_EVENTTYPE_MOUSE_MOVE:
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
        update_mouse(event->mouse_x, event->mouse_y);
        if (event->type != SAPP_EVENTTYPE_MOUSE_MOVE)
        {
            const bool down = event->type == SAPP_EVENTTYPE_MOUSE_DOWN;
            platform.input.mouse.left = event->mouse_button == SAPP_MOUSEBUTTON_LEFT ? down : platform.input.mouse.left;
            platform.input.mouse.right = event->mouse_button == SAPP_MOUSEBUTTON_RIGHT ? down : platform.input.mouse.right;
            platform.input.mouse.middle = event->mouse_button == SAPP_MOUSEBUTTON_MIDDLE ? down : platform.input.mouse.middle;
        }
        break;

    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        // sokol reports the touches still on the screen, not the one that moved.
        platform.touch.count = MIN(event->num_touches, CONTROLS_MAX_POINTERS);

        for (s32 i = 0; i < platform.touch.count; i++)
        {
            platform.touch.list[i].x = event->touches[i].pos_x;
            platform.touch.list[i].y = event->touches[i].pos_y;
            platform.touch.list[i].down = true;
        }

        platform.touch.timeout = TOUCH_TIMEOUT;

        if (platform.touch.count > 0)
        {
            update_mouse(event->touches[0].pos_x, event->touches[0].pos_y);
            platform.input.mouse.left = 1;
        }
        else
            platform.input.mouse.left = 0;
        break;

    case SAPP_EVENTTYPE_RESIZED:
        break;

    default:
        break;
    }
}

static void frame_cb(void)
{
    if (studio_alive(platform.studio))
    {
        sapp_quit();
        return;
    }

    const double dt = stm_sec(stm_laptime(&platform.lastTime));

    controls_frame((float)dt);

    platform.accumulator += dt;

    bool ticked = false;
    s32 ticks = 0;

    while (platform.accumulator >= 1.0 / TIC80_FRAMERATE && ticks < MAX_CATCH_UP)
    {
        tick();
        platform.accumulator -= 1.0 / TIC80_FRAMERATE;
        ticks++;
        ticked = true;
    }

    if (platform.accumulator > MAX_CATCH_UP / (double)TIC80_FRAMERATE)
        platform.accumulator = 0;

    render_frame(platform.studio, studio_mem(platform.studio)->product.screen, ticked);
}

static void init_cb(void)
{
    stm_setup();
    platform.lastTime = stm_now();

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    platform.appFolder = getAppFolder();

    render_init();

    saudio_setup(&(saudio_desc){
        .sample_rate = TIC80_SAMPLERATE,
        .num_channels = TIC80_SAMPLE_CHANNELS,
        .logger.func = slog_func,
    });

    platform.studio = studio_create(platform.argc, platform.argv, TIC80_SAMPLERATE,
        TIC80_PIXEL_COLOR_RGBA8888, platform.appFolder, max_scale(), tic_layout_qwerty);

    controls_init(studio_config(platform.studio)->cart);

    platform.fullscreen = sapp_is_fullscreen();
}

static void cleanup_cb(void)
{
    controls_shutdown();
    saudio_shutdown();
    render_shutdown();
    sg_shutdown();
}

static sapp_desc app_desc(void)
{
    return (sapp_desc){
        .init_cb = init_cb,
        .frame_cb = frame_cb,
        .cleanup_cb = cleanup_cb,
        .event_cb = event_cb,
        .width = TIC80_FULLWIDTH * 3,
        .height = TIC80_FULLHEIGHT * 3,
        .window_title = TIC_TITLE,
        .logger.func = slog_func,
        .high_dpi = false,
        .swap_interval = 1,
        .html5.canvas_resize = false,
        .enable_clipboard = true,
        .clipboard_size = 4096,
    };
}

#if defined(__EMSCRIPTEN__)

// The studio cannot start before its folder is mounted from IndexedDB, and
// mounting is asynchronous — so the application starts from the callback.
EMSCRIPTEN_KEEPALIVE void sokol_start(void)
{
    sapp_desc desc = app_desc();
    sapp_run(&desc);
}

int main(int argc, char* argv[])
{
    platform.argc = argc;
    platform.argv = argv;

    EM_ASM({
        const dir = UTF8ToString($0);
        FS.mkdirTree(dir);
        FS.mount(IDBFS, {}, dir);
        FS.syncfs(true, () => Module._sokol_start());
    }, getAppFolder());

    return 0;
}

#else

sapp_desc sokol_main(int argc, char* argv[])
{
    platform.argc = argc;
    platform.argv = argv;

    return app_desc();
}

#endif

////// the studio's side of the layer //////

void tic_sys_clipboard_set(const char* text)
{
    sapp_set_clipboard_string(text);
}

bool tic_sys_clipboard_has(void)
{
    const char* text = sapp_get_clipboard_string();
    return text && *text;
}

char* tic_sys_clipboard_get(void)
{
    const char* text = sapp_get_clipboard_string();

    if (!text)
        return NULL;

    char* copy = malloc(strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}

void tic_sys_clipboard_free(const char* text)
{
    free((void*)text);
}

u64 tic_sys_counter_get(void)
{
    return stm_now();
}

u64 tic_sys_freq_get(void)
{
    // sokol_time counts nanoseconds on every platform.
    return 1000000000ull;
}

bool tic_sys_fullscreen_get(void)
{
    return platform.fullscreen;
}

void tic_sys_fullscreen_set(bool value)
{
    if (value != sapp_is_fullscreen())
        sapp_toggle_fullscreen();

    platform.fullscreen = sapp_is_fullscreen();
}

void tic_sys_message(const char* title, const char* message)
{
    TIC_UNUSED(title);
    printf("%s\n", message);
}

void tic_sys_title(const char* title)
{
    sapp_set_window_title(title);
}

void tic_sys_open_path(const char* path)
{
    TIC_UNUSED(path);
}

void tic_sys_open_url(const char* url)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({ window.open(UTF8ToString($0), '_blank'); }, url);
#else
    TIC_UNUSED(url);
#endif
}

void tic_sys_preseed(void)
{
    srand((unsigned)time(NULL));
    rand();
}

bool tic_sys_keyboard_text(char* text)
{
    if (platform.keyboard.text)
    {
        *text = platform.keyboard.text;
        return true;
    }

    return false;
}

void tic_sys_update_config(void)
{
}

void tic_sys_default_mapping(tic_mapping* mapping)
{
    // sokol's key codes are physical positions, so the default keys are the
    // positions themselves whatever the layout types on them.
    static const tic_key Keys[] =
    {
        tic_key_up, tic_key_down, tic_key_left, tic_key_right,
        tic_key_z, tic_key_x, tic_key_a, tic_key_s,
    };

    for (s32 i = 0; i < COUNT_OF(Keys); i++)
        mapping->data[i] = Keys[i];
}
