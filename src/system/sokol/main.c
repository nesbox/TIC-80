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
#if defined(TOUCH_INPUT_SUPPORT)
#include "controls.h"
#endif
#include "sgamepad.h"
#include "sokol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#elif defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// The machine runs at sixty ticks a second whatever the display does; a frame
// after a stall replays at most this many ticks of the backlog.
#define MAX_CATCH_UP 4

#if defined(TOUCH_INPUT_SUPPORT)
// How long the controls stay out after the last touch.
#define TOUCH_TIMEOUT 10.0f
#endif

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
        // Characters arrive faster than the machine ticks; a single slot would
        // keep only the last of a burst.
        char queue[64];
        s32  head, tail;

        // A CHAR event carries no key code, so the key it belongs to is the
        // one that went down last.
        sapp_keycode lastCode;
    } keyboard;

#if defined(TOUCH_INPUT_SUPPORT)
    struct
    {
        ControlsPointer list[CONTROLS_MAX_POINTERS];
        s32   count;
        float timeout;
        bool  seen;
        bool  mouseDown;        // a finger is holding the studio's mouse button
    } touch;
#endif

    struct
    {
        float x, y;             // the mouse in window pixels
        float scrollX, scrollY; // fractions of a notch the wheel has not reached
        float moveX, moveY;     // motion since the last tick, for a captured pointer
        bool left, right, middle;
    } pointer;

    // The page's safe-area insets, in pixels; zero where there are none.
    struct
    {
        float top, right, bottom, left;
    } insets;

    bool fullscreen;
    bool layoutKnown;

    // The menu button — the on-screen control or a pad's own — as it was last
    // frame: the ESC it stands for is written on its edge.
    bool menuDown;
} platform;

// sokol keeps no logger of its own: given none it says nothing, which is what
// a release build wants and what a release build gets — the field is left
// empty, and the printing below is not compiled into it at all. What a build
// with a reader wants is a debug build's, where the chatter about a sample
// rate is worth as much as the errors.
#if !defined(NDEBUG)

// Printed where a machine's own output goes: a desktop's stdout and stderr,
// and on a page the browser console, where stderr is what it calls an error.
static void on_log(const char* tag, uint32_t log_level, uint32_t log_item, const char* message,
    uint32_t line_nr, const char* filename, void* user_data)
{
    (void)user_data;

    static const char* Levels[] = { "panic", "error", "warning", "info" };
    const char* level = Levels[log_level < COUNT_OF(Levels) ? log_level : COUNT_OF(Levels) - 1];

    FILE* out = log_level <= 1 ? stderr : stdout;

    if (message)
    {
        if (filename)
            fprintf(out, "[%s] %s: %s (%s:%u)\n", tag ? tag : "sokol", level, message, filename, line_nr);
        else
            fprintf(out, "[%s] %s: %s\n", tag ? tag : "sokol", level, message);
    }
    else
        fprintf(out, "[%s] %s: item %u at line %u\n", tag ? tag : "sokol", level, log_item, line_nr);

    fflush(out);
}

#define TIC80_LOGGER .logger.func = on_log,

#else

#define TIC80_LOGGER

#endif

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

#if !defined(__EMSCRIPTEN__)
// The folder has to be there before the studio writes anything into it, and
// nobody makes it for us here — the SDL layer is handed one that
// SDL_GetPrefPath has already made. Every component is made, since a fresh
// machine has none of them.
static void make_folder(const char* path)
{
    char buffer[TICNAME_MAX];

    snprintf(buffer, sizeof buffer, "%s", path);

    for (char* p = buffer + 1; *p; p++)
    {
        if (*p != '/' && *p != '\\')
            continue;

        const char separator = *p;
        *p = '\0';

#if defined(_WIN32)
        _mkdir(buffer);
#else
        mkdir(buffer, 0777);
#endif

        *p = separator;
    }
}
#endif

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

// Moves the machine's mouse to a window point; false when the point is off the
// machine's screen, where it has no mouse to move — a press there is not a
// click on whatever the mouse was last over.
static bool update_mouse(float x, float y)
{
    float rx, ry, rw, rh;
    render_player_rect(platform.studio, &rx, &ry, &rw, &rh);

    const tic_point m =
    {
        (s32)((x - rx) * TIC80_FULLWIDTH / rw),
        (s32)((y - ry) * TIC80_FULLHEIGHT / rh)
    };

    // The mouse is only on the machine's screen when it is inside it.
    const bool inside = m.x >= 0 && m.y >= 0 && m.x < TIC80_FULLWIDTH && m.y < TIC80_FULLHEIGHT;

    // The machine draws a cursor of its own on its screen, so the system one
    // is hidden there and left alone everywhere else on the window.
    sapp_show_mouse(!inside);

    // A captured pointer has no place on the screen to be at, and a pointer
    // off the screen is a mouse the machine is told is off its screen: left
    // where it was, it would keep drawing its cursor in the corner it left by.
    if (!inside || platform.input.mouse.relative)
    {
        platform.input.mouse.x = -1;
        platform.input.mouse.y = -1;
        return false;
    }

    platform.input.mouse.x = m.x;
    platform.input.mouse.y = m.y;

    return true;
}

// Handles the keyboard state over to the tick's input, and clears what a tick
// has consumed: a press that arrives between ticks is not reported twice.
static void build_input(void)
{
    tic80_input* input = &platform.input;
    s32 c = 0;

#if defined(TOUCH_INPUT_SUPPORT)
    const ControlsState* controls = controls_state();
#endif

    for (tic_key i = 0; i < tic_keys_count && c < TIC80_KEY_BUFFER; i++)
        if (platform.keyboard.state[i] || platform.keyboard.pressed[i]
#if defined(TOUCH_INPUT_SUPPORT)
            || (controls->visible && controls->keys[i])
#endif
            )
            input->keyboard.keys[c++] = i;

    while (c < TIC80_KEY_BUFFER)
        input->keyboard.keys[c++] = tic_key_unknown;

    memset(platform.keyboard.pressed, 0, sizeof platform.keyboard.pressed);

    // The physical pads and the on-screen controls are the same input: a pad
    // in one hand and a thumb on the screen work at once.
    bool padMenu = false;

    {
        tic80_gamepads pads = { 0 };
        tic80_gamepad* slots[TIC_GAMEPADS] = { &pads.first, &pads.second, &pads.third, &pads.fourth };

        for (s32 i = 0; i < SGAMEPAD_MAX_GAMEPADS && i < TIC_GAMEPADS; i++)
        {
            const sgamepad_state pad = sgamepad_gamepad_state(i);

            slots[i]->data = pad.data;

            // The machine's gamepad has no button for a menu, so the pad's own
            // back and start are read here — the SDL layer's back was the menu.
            padMenu = padMenu || pad.back || pad.start;
        }

#if defined(TOUCH_INPUT_SUPPORT)
        if (controls->visible)
            pads.first.data |= controls->gamepad.data;
#endif

        input->gamepads.data = pads.data;
    }

    // The menu is ESC, the way the SDL layer's BACK button is, and a pad's own
    // back and start are that same menu. One press is one ESC: the key is
    // written on the edge alone, because a button held through a mode change —
    // where the machine's previous keyboard state is wiped — would read as a
    // second press and walk a player two steps back.
    bool menu = padMenu;

#if defined(TOUCH_INPUT_SUPPORT)
    menu = menu || controls->menu;
#endif

    if (menu && !platform.menuDown)
        input->keyboard.keys[0] = tic_key_escape;

    platform.menuDown = menu;
}

#if defined(TOUCH_INPUT_SUPPORT)

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

#if !defined(__EMSCRIPTEN__)
// A desktop has no touch screen to try the controls on, so the mouse stands in
// for a finger: moving it brings the controls out and holding the button
// presses what it is over. Debug affordance, and only for this experiment.
static void desktop_pointer(void)
{
    if (platform.touch.seen)
        return;

    platform.touch.list[0].x = platform.pointer.x;
    platform.touch.list[0].y = platform.pointer.y;
    platform.touch.list[0].down = platform.pointer.left;
    platform.touch.count = 1;
}
#endif

static void controls_frame(float dt)
{
#if !defined(__EMSCRIPTEN__)
    desktop_pointer();
#endif

    // A finger still on the screen keeps the controls out however long it has
    // been still — a browser sends nothing for a motionless touch. The desktop
    // mouse stands in for a finger, so its held button does the same.
    const bool held = platform.touch.count > 0 && (platform.touch.seen || platform.touch.list[0].down);

    if (held)
        platform.touch.timeout = TOUCH_TIMEOUT;
    else if (platform.touch.timeout > 0.0f)
        platform.touch.timeout = MAX(platform.touch.timeout - dt, 0.0f);

    const ControlsInput input = {
        .width = (float)sapp_width(),
        .height = (float)sapp_height(),
        .insets = {
            .left = platform.insets.left,
            .top = platform.insets.top,
            .right = platform.insets.right,
            .bottom = platform.insets.bottom
        },
        .pointerCount = platform.touch.count,
        .mode = controls_mode(),
        // What the machine held last tick, which the core has already merged
        // the keyboard's own mapping into — a key that stands for a button
        // lights it, the way a physical pad does — and what the pads hold now.
        .gamepad = platform.input.gamepads.first.data
            | studio_mem(platform.studio)->ram->input.gamepads.first.data,
        .keys = platform.keyboard.state,
        .portrait = sapp_height() > sapp_width(),
        .visible = platform.touch.timeout > 0.0f,
        .dt = dt,
        .alpha = studio_config(platform.studio)->theme.gamepad.touch.alpha,
    };

    memcpy((void*)input.pointers, platform.touch.list, sizeof input.pointers);

    controls_update(&input);

    // A touch a control took is not the studio's mouse; the others are, so the
    // editors stay usable on a phone.
    if (platform.touch.seen)
    {
        const ControlsState* state = controls_state();
        s32 free = -1;

        for (s32 i = 0; i < platform.touch.count; i++)
            if (!(state->claimed & (1 << i)))
            {
                free = i;
                break;
            }

        platform.touch.mouseDown = free >= 0
            && update_mouse(platform.touch.list[free].x, platform.touch.list[free].y);
    }

    // The button is whichever way pressed it: a finger that lets go says
    // nothing about a mouse that is still holding it down.
    platform.input.mouse.left = platform.pointer.left || platform.touch.mouseDown;
    platform.input.mouse.right = platform.pointer.right;
    platform.input.mouse.middle = platform.pointer.middle;
}

#else

// Without the controls the buttons are the mouse's own, and a frame has
// nothing of theirs to do.
static void controls_frame(float dt)
{
    TIC_UNUSED(dt);

    platform.input.mouse.left = platform.pointer.left;
    platform.input.mouse.right = platform.pointer.right;
    platform.input.mouse.middle = platform.pointer.middle;
}

#endif

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
    // A cart that wants the pointer captured says so in its input, the way the
    // SDL layer reads it: the lock follows the machine, and while it holds, the
    // mouse is the motion since the last tick rather than a place on the screen.
    {
        const tic_mem* tic = studio_mem(platform.studio);
        const bool relative = tic->ram->input.mouse.relative != 0;

        if (relative != sapp_mouse_locked())
            sapp_lock_mouse(relative);

        // The request goes back to the machine rather than the state the
        // platform reports: a browser grants the lock a click later, and a
        // machine told otherwise would stop asking in the meantime.
        platform.input.mouse.relative = relative ? 1 : 0;

        // Motion and position are one field of the machine's mouse, so only
        // the one it reads is written: a captured pointer has no place on the
        // screen, and the tick before it is captured still has one.
        if (relative)
        {
            platform.input.mouse.rx = (s32)platform.pointer.moveX;
            platform.input.mouse.ry = (s32)platform.pointer.moveY;
        }

        platform.pointer.moveX = platform.pointer.moveY = 0;
    }

    build_input();

    studio_tick(platform.studio, platform.input);
    studio_sound(platform.studio);

    // The wheel is an event rather than a state: what turned since the last
    // tick is what this tick sees, the way the SDL layer's input, rebuilt on
    // every poll, carries it. The motion is cleared with the request, in the
    // block above, since it is written into the position's own field.
    platform.input.mouse.scrollx = platform.input.mouse.scrolly = 0;

    push_audio();
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

// A browser starts the audio context suspended, and only a gesture may start
// it. sokol's own hook is one-shot and listens on the release, so a gesture
// that lands while the module is still loading — or a resume the browser
// turns down — leaves the machine silent for good. Ours retries on every
// press, on the press itself, until the context is running.
static void resume_audio(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        const ctx = Module._saudio_context;

        if (ctx && ctx.state !== 'running')
            ctx.resume().catch(function() {});
    });
#endif
}

static void event_cb(const sapp_event* event)
{
    switch (event->type)
    {
    case SAPP_EVENTTYPE_KEY_DOWN:
        resume_audio();
        platform.keyboard.lastCode = event->key_code;
        handle_key(event->key_code, true);
        break;

    case SAPP_EVENTTYPE_KEY_UP:
        handle_key(event->key_code, false);
        break;

    case SAPP_EVENTTYPE_CHAR:
        // Control characters are keys of their own, not text.
        if (event->char_code >= 32 && event->char_code < 127
            && platform.keyboard.head - platform.keyboard.tail < (s32)COUNT_OF(platform.keyboard.queue))
            platform.keyboard.queue[platform.keyboard.head++ % COUNT_OF(platform.keyboard.queue)] = (char)event->char_code;

        learn_layout(platform.keyboard.lastCode, event->char_code);
        break;

    case SAPP_EVENTTYPE_UNFOCUSED:
        // A key or a button let go of outside the window keeps its last state:
        // the release happens where the window cannot hear it.
        memset(platform.keyboard.state, 0, sizeof platform.keyboard.state);
        memset(platform.keyboard.pressed, 0, sizeof platform.keyboard.pressed);
        platform.pointer.left = platform.pointer.right = platform.pointer.middle = false;
        break;

    case SAPP_EVENTTYPE_MOUSE_LEAVE:
        // The pointer left the canvas — which on a page is the player's own
        // box — so it is off the machine's screen, and the machine would keep
        // drawing its cursor where the pointer left it if it were not told.
        platform.pointer.left = platform.pointer.right = platform.pointer.middle = false;
        platform.pointer.x = platform.pointer.y = -1.0f;
        platform.input.mouse.x = -1;
        platform.input.mouse.y = -1;
        sapp_show_mouse(true);
        break;

    case SAPP_EVENTTYPE_MOUSE_MOVE:
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
        platform.pointer.x = event->mouse_x;
        platform.pointer.y = event->mouse_y;

#if defined(TOUCH_INPUT_SUPPORT) && !defined(__EMSCRIPTEN__)
        // Desktop only: see desktop_pointer() — the mouse stands in for a finger.
        if (!platform.touch.seen)
            platform.touch.timeout = TOUCH_TIMEOUT;
#endif

        // A captured pointer moves the machine's mouse by the motion alone;
        // the sum of it is handed over by the tick.
        platform.pointer.moveX += event->mouse_dx;
        platform.pointer.moveY += event->mouse_dy;

        update_mouse(event->mouse_x, event->mouse_y);
        if (event->type != SAPP_EVENTTYPE_MOUSE_MOVE)
        {
            const bool down = event->type == SAPP_EVENTTYPE_MOUSE_DOWN;
            platform.pointer.left = event->mouse_button == SAPP_MOUSEBUTTON_LEFT ? down : platform.pointer.left;
            platform.pointer.right = event->mouse_button == SAPP_MOUSEBUTTON_RIGHT ? down : platform.pointer.right;
            platform.pointer.middle = event->mouse_button == SAPP_MOUSEBUTTON_MIDDLE ? down : platform.pointer.middle;

            if (down)
                resume_audio();
        }
        break;

    case SAPP_EVENTTYPE_MOUSE_SCROLL:
    {
        // A trackpad sends a gesture as fractions of a notch and the machine
        // reads whole ones, so the fraction is kept until it adds up.
        platform.pointer.scrollX += event->scroll_x;
        platform.pointer.scrollY += event->scroll_y;

        const s32 x = (s32)platform.pointer.scrollX;
        const s32 y = (s32)platform.pointer.scrollY;

        platform.pointer.scrollX -= (float)x;
        platform.pointer.scrollY -= (float)y;

        platform.input.mouse.scrollx += x;
        platform.input.mouse.scrolly += y;
        break;
    }

    case SAPP_EVENTTYPE_RESUMED:
        // A browser takes the GL context away when the page goes to the
        // background and hands a new one back on the way in — everything the
        // layer made belonged to the old one, so all of it is made again.
        render_shutdown();
#if defined(TOUCH_INPUT_SUPPORT)
        controls_shutdown();
#endif
        sg_shutdown();
        sg_setup(&(sg_desc){
            .environment = sglue_environment(),
            TIC80_LOGGER
        });
        render_init();
#if defined(TOUCH_INPUT_SUPPORT)
        controls_init(studio_config(platform.studio)->cart);
#endif
        break;

#if defined(TOUCH_INPUT_SUPPORT)
    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        // The list is the fingers still on the screen. An ENDED event also
        // carries the finger that just lifted, flagged as changed — keeping it
        // would leave its control held for good.
        platform.touch.count = 0;

        if (event->type != SAPP_EVENTTYPE_TOUCHES_CANCELLED)
            for (s32 i = 0; i < event->num_touches && platform.touch.count < CONTROLS_MAX_POINTERS; i++)
            {
                if (event->type == SAPP_EVENTTYPE_TOUCHES_ENDED && event->touches[i].changed)
                    continue;

                ControlsPointer* pointer = &platform.touch.list[platform.touch.count++];

                pointer->x = event->touches[i].pos_x;
                pointer->y = event->touches[i].pos_y;
                pointer->down = true;
            }

        platform.touch.timeout = TOUCH_TIMEOUT;
        platform.touch.seen = true;

        resume_audio();

        break;
#endif

    case SAPP_EVENTTYPE_RESIZED:
#if defined(__EMSCRIPTEN__)
        EM_ASM(Module.tic80Viewport());
#endif
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

    sgamepad_record_state();
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

#if defined(__EMSCRIPTEN__)
    // A file the studio wrote only reaches IndexedDB through a flush, and the
    // studio asks for one per write; taking them here, once a frame, keeps the
    // file system's own write out of the studio's call.
    EM_ASM(
    {
        if (!Module.syncing && Module.syncFSRequests)
        {
            Module.syncing = true;
            Module.syncFSRequests = 0;
            FS.syncfs(false, function() { Module.syncing = false; });
        }
    });
#endif
}

#if defined(__EMSCRIPTEN__)
// The page pushes the insets in; only CSS can read env(safe-area-inset-*).
// See build/html/prejs.js.
EMSCRIPTEN_KEEPALIVE void tic80_insets(float top, float right, float bottom, float left)
{
    platform.insets.top = top;
    platform.insets.right = right;
    platform.insets.bottom = bottom;
    platform.insets.left = left;
}
#endif

static void init_cb(void)
{
    stm_setup();
    platform.lastTime = stm_now();

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        TIC80_LOGGER
    });

    platform.appFolder = getAppFolder();

#if !defined(__EMSCRIPTEN__)
    // The page mounts its own folder from IndexedDB before anything starts.
    make_folder(platform.appFolder);
#endif

    render_init();

    saudio_setup(&(saudio_desc){
        .sample_rate = TIC80_SAMPLERATE,
        .num_channels = TIC80_SAMPLE_CHANNELS,
        TIC80_LOGGER
    });

    platform.studio = studio_create(platform.argc, platform.argv, TIC80_SAMPLERATE,
        TIC80_PIXEL_COLOR_RGBA8888, platform.appFolder, max_scale(), tic_layout_qwerty);

#if defined(TOUCH_INPUT_SUPPORT)
    controls_init(studio_config(platform.studio)->cart);
#endif

    sgamepad_setup(&(sgamepad_desc){ .deadzone = 0.5f });

#if defined(__EMSCRIPTEN__)
    EM_ASM(Module.tic80Viewport());
#endif

    platform.fullscreen = sapp_is_fullscreen();
}

static void cleanup_cb(void)
{
#if defined(TOUCH_INPUT_SUPPORT)
    controls_shutdown();
#endif
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
        // The studio reads the window it was given as the largest scale that
        // fits it, so the window is opened at the scale the config asks for
        // by default — the SDL layer opens its window at that scale outright.
        .width = TIC80_FULLWIDTH * 4,
        .height = TIC80_FULLHEIGHT * 4,
        .window_title = TIC_TITLE,
        TIC80_LOGGER
        // The framebuffer follows the display's own pixels wherever there are
        // more of them than the window has: on a Retina screen and on a phone
        // the machine's pixels land on all of them, and the CRT effect is
        // drawn at its own size either way, so the picture is as sharp as the
        // display can be and the fill costs the same.
        .high_dpi = true,
        // Always, never from the config: the window is made here, before the
        // studio exists to read one, and sokol has no setter for it. The
        // machine's sixty ticks a second do not depend on it either way, and
        // the menu leaves the option out for the same reason.
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

// An exported game's page passes its cartridge as the first argument, and the
// file sits next to the page rather than in the folder: it has to be fetched
// into the folder before the studio starts, and argv[1] becomes the path the
// studio opens — the SDL layer's emsStart does the same.
static const char* cart_argument(char** argv)
{
    static char path[TICNAME_MAX];
    const size_t len = strlen(argv[1]);

    if (len < 4 || strcmp(&argv[1][len - 4], ".tic"))
        return NULL;

    snprintf(path, sizeof path, "%s%s", getAppFolder(), argv[1]);
    argv[1] = path;

    return path;
}

int main(int argc, char* argv[])
{
    platform.argc = argc;
    platform.argv = argv;

    const char* url = argc >= 2 ? argv[1] : NULL;
    const char* cart = url ? cart_argument(argv) : NULL;

    EM_ASM({
        Module.syncFSRequests = 0;
        Module.syncing = false;

        const dir = UTF8ToString($0);
        const file = $1 ? UTF8ToString($1) : null;
        const url = $2 ? UTF8ToString($2) : null;

        FS.mkdirTree(dir);
        FS.mount(IDBFS, {}, dir);
        FS.syncfs(true, function()
        {
            // A session without a cartridge argument, the console and the
            // editors, starts as soon as its folder is there.
            if (!file)
            {
                Module._sokol_start();
                return;
            }

            var path = PATH_FS.resolve(file);
            var parent = PATH.dirname(path);

            FS.createPreloadedFile(parent, PATH.basename(path), url, true, true,
                function() { Module._sokol_start(); },
                function() {},
                false, false, function()
                {
                    // A cartridge the folder kept from an earlier session is
                    // in the way of the one this page carries.
                    try { FS.unlink(path); } catch (e) {}
                    FS.mkdirTree(parent);
                });
        });
    }, getAppFolder(), cart, url);

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
    {
        sapp_toggle_fullscreen();

        // The window leaves the keyboard behind while the system moves it to
        // a space of its own: the key that asked for this is never released,
        // and held it counts as pressed again on every mode switch.
        memset(platform.keyboard.state, 0, sizeof platform.keyboard.state);
        memset(platform.keyboard.pressed, 0, sizeof platform.keyboard.pressed);
    }

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
    if (platform.keyboard.tail == platform.keyboard.head)
        return false;

    *text = platform.keyboard.queue[platform.keyboard.tail++ % COUNT_OF(platform.keyboard.queue)];
    return true;
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
