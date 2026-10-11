// sgamepad.h -- gamepads, in the shape of the other sokol headers.
//
// One API for every platform: setup, a state recorded once a frame, and up to
// four gamepads read back, with one backend compiled per platform. Slot
// assignment is one policy here rather than in any backend: the first pad that
// shows it is being used takes the first free slot, a pad keeps its slot while
// it is connected, and a pad that goes away frees it.
//
//     Do this:
//         #define SGAMEPAD_IMPL
//     before you include this file in *one* C file to create the implementation.
//
//     The following defines are used by the implementation to pick a backend:
//     __EMSCRIPTEN__      the browser's Gamepad API
//     _WIN32              XInput
//     __APPLE__           GameController.framework, which is Objective-C
//     __linux__           evdev, through /dev/input/event*
//
//     ...and this one to compile the backend the tests drive, which asks the
//     platform for nothing and leaves the devices to the caller:
//     SGAMEPAD_TEST
//
//     ...optionally provide the following macros to override defaults:
//     SGAMEPAD_API_DECL   - public function declaration prefix (default: extern)
//     SGAMEPAD_API_IMPL   - public function implementation prefix (default: -)

#pragma once

#ifndef SGAMEPAD_INCLUDED
#define SGAMEPAD_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SGAMEPAD_API_DECL
#define SGAMEPAD_API_DECL extern
#endif

#include <stdbool.h>

// The slots the machine carries, and the devices a backend may report.
#define SGAMEPAD_MAX_GAMEPADS   4
#define SGAMEPAD_MAX_DEVICES    16

// One gamepad's buttons, laid out as the machine reads them, plus the back and
// start the machine has no room for — the layer reads those itself.
typedef struct
{
    union
    {
        struct
        {
#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
            unsigned char a : 1;
            unsigned char b : 1;
            unsigned char x : 1;
            unsigned char y : 1;
            unsigned char right : 1;
            unsigned char left : 1;
            unsigned char down : 1;
            unsigned char up : 1;
#else
            unsigned char up : 1;
            unsigned char down : 1;
            unsigned char left : 1;
            unsigned char right : 1;
            unsigned char a : 1;
            unsigned char b : 1;
            unsigned char x : 1;
            unsigned char y : 1;
#endif
        };

        unsigned char data;
    };

    bool back;
    bool start;

    // The right stick with the deadzone applied, -1..1 in the machine's own
    // axes (x right, y down), zero where the pad has none or it is centered.
    float right_x;
    float right_y;
} sgamepad_state;

typedef struct sgamepad_desc
{
    // The share of the axis travel that counts as pressed, 0..1.
    float deadzone;
} sgamepad_desc;

SGAMEPAD_API_DECL void sgamepad_setup(const sgamepad_desc* desc);
SGAMEPAD_API_DECL void sgamepad_shutdown(void);

// Call once a frame, before reading anything back.
SGAMEPAD_API_DECL void sgamepad_record_state(void);

SGAMEPAD_API_DECL int sgamepad_num_gamepads(void);

// The slot's state; an unassigned slot reads back as nothing held.
SGAMEPAD_API_DECL sgamepad_state sgamepad_gamepad_state(int slot);

#if defined(SGAMEPAD_TEST)

// The backend the tests drive: the device at `index` is there or it is not,
// and it holds whatever the caller says.
SGAMEPAD_API_DECL void sgamepad_test_device(int index, bool present, sgamepad_state state);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SGAMEPAD_INCLUDED */

// ---------------------------------------------------------------------------
//  implementation
// ---------------------------------------------------------------------------
#if defined(SGAMEPAD_IMPL) && !defined(SGAMEPAD_IMPL_INCLUDED)

#ifndef SGAMEPAD_IMPL_INCLUDED
#define SGAMEPAD_IMPL_INCLUDED
#endif

#ifndef SGAMEPAD_API_IMPL
#define SGAMEPAD_API_IMPL
#endif

#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#elif defined(__APPLE__) && defined(__OBJC__)
// The macOS backend is Objective-C, which is how sokol_impl.c is compiled
// there; a translation unit that is not sees no backend at all.
#import <GameController/GameController.h>
#endif

// What a backend reports about one device: what it holds, and whether it has
// shown use the layer would not see otherwise — a button it does not map.
typedef struct
{
    bool            present;
    bool            used;
    sgamepad_state  state;
} _sgamepad_device;

typedef struct
{
    int             ids[SGAMEPAD_MAX_GAMEPADS];     // which device owns each slot, -1 for none
    sgamepad_state  states[SGAMEPAD_MAX_GAMEPADS];
    _sgamepad_device devices[SGAMEPAD_MAX_DEVICES];
    float           deadzone;
} _sgamepad_t;

static _sgamepad_t _sgamepad;

// A backend that holds something of the platform's own — Linux, with a file
// per pad — is told at setup and at shutdown; the others have nothing to do.
#if defined(__linux__) && !defined(SGAMEPAD_TEST)
static void _sgamepad_backend_setup(void);
static void _sgamepad_backend_shutdown(void);
#else
#define _sgamepad_backend_setup()       ((void)0)
#define _sgamepad_backend_shutdown()    ((void)0)
#endif

// The same conditions the backend selection uses: no backend, no helpers.
#if defined(SGAMEPAD_TEST) || defined(__EMSCRIPTEN__) || defined(_WIN32) \
    || (defined(__APPLE__) && defined(__OBJC__)) || defined(__linux__)

// What is inside the deadzone reads as nothing.
static float _sgamepad_deadzone(float value)
{
    return (value > _sgamepad.deadzone || value < -_sgamepad.deadzone) ? value : 0.0f;
}

#if !defined(SGAMEPAD_TEST)
// The share of an axis' travel that counts as pushed in one direction; the
// test backend is handed the direction already made.
static bool _sgamepad_direction(float value, int dir)
{
    return dir < 0 ? value < -_sgamepad.deadzone : value > _sgamepad.deadzone;
}
#endif

// Anything of the pad the layer reads is being held.
static bool _sgamepad_holding(const sgamepad_state* state)
{
    return state->data != 0 || state->back || state->start
        || state->right_x != 0.0f || state->right_y != 0.0f;
}

#endif /* a backend to apply them to */

// One policy for every backend: the first device that shows it is being used
// takes the first free slot, and a device keeps its slot while it is present.
static void _sgamepad_assign(int num_devices)
{
    if (num_devices > SGAMEPAD_MAX_DEVICES)
        num_devices = SGAMEPAD_MAX_DEVICES;

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
        if (_sgamepad.ids[i] >= 0
            && (_sgamepad.ids[i] >= num_devices || !_sgamepad.devices[_sgamepad.ids[i]].present))
            _sgamepad.ids[i] = -1;

    for (int d = 0; d < num_devices; d++)
    {
        const _sgamepad_device* device = &_sgamepad.devices[d];

        if (!device->present)
            continue;

        int slot = -1;

        for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS && slot < 0; i++)
            if (_sgamepad.ids[i] == d)
                slot = i;

        if (slot < 0 && device->used)
            for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS && slot < 0; i++)
                if (_sgamepad.ids[i] < 0)
                    slot = i;

        if (slot < 0)
            continue;

        _sgamepad.ids[slot] = d;
        _sgamepad.states[slot] = device->state;
    }

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
        if (_sgamepad.ids[i] < 0)
            memset(&_sgamepad.states[i], 0, sizeof _sgamepad.states[i]);
}

SGAMEPAD_API_IMPL void sgamepad_setup(const sgamepad_desc* desc)
{
    memset(&_sgamepad, 0, sizeof _sgamepad);

    _sgamepad.deadzone = (desc && desc->deadzone > 0.0f) ? desc->deadzone : 0.5f;

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
        _sgamepad.ids[i] = -1;

    _sgamepad_backend_setup();
}

SGAMEPAD_API_IMPL void sgamepad_shutdown(void)
{
    _sgamepad_backend_shutdown();

    memset(&_sgamepad, 0, sizeof _sgamepad);
}

SGAMEPAD_API_IMPL int sgamepad_num_gamepads(void)
{
    int count = 0;

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
        if (_sgamepad.ids[i] >= 0)
            count++;

    return count;
}

SGAMEPAD_API_IMPL sgamepad_state sgamepad_gamepad_state(int slot)
{
    const sgamepad_state empty = { 0 };

    return (slot >= 0 && slot < SGAMEPAD_MAX_GAMEPADS && _sgamepad.ids[slot] >= 0)
        ? _sgamepad.states[slot]
        : empty;
}

#if defined(SGAMEPAD_TEST)

SGAMEPAD_API_IMPL void sgamepad_test_device(int index, bool present, sgamepad_state state)
{
    if (index < 0 || index >= SGAMEPAD_MAX_DEVICES)
        return;

    state.right_x = _sgamepad_deadzone(state.right_x);
    state.right_y = _sgamepad_deadzone(state.right_y);

    _sgamepad.devices[index].present = present;
    _sgamepad.devices[index].state = state;
    _sgamepad.devices[index].used = _sgamepad_holding(&state);
}

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    _sgamepad_assign(SGAMEPAD_MAX_DEVICES);
}

#elif defined(__EMSCRIPTEN__)

// The browser reports every pad it can see, touched or not, so a pad takes a
// slot once something on it moves — the way the SDL layer picks its pads.
static bool _sgamepad_used(const EmscriptenGamepadEvent* pad)
{
    for (int i = 0; i < pad->numButtons; i++)
        if (pad->digitalButton[i] || pad->analogButton[i] > _sgamepad.deadzone)
            return true;

    for (int i = 0; i < pad->numAxes; i++)
        if (pad->axis[i] > _sgamepad.deadzone || pad->axis[i] < -_sgamepad.deadzone)
            return true;

    return false;
}

static bool _sgamepad_axis(const EmscriptenGamepadEvent* pad, int axis, int dir)
{
    return axis < pad->numAxes && _sgamepad_direction(pad->axis[axis], dir);
}

static bool _sgamepad_button(const EmscriptenGamepadEvent* pad, int button)
{
    return button < pad->numButtons && pad->digitalButton[button];
}

static float _sgamepad_stick(const EmscriptenGamepadEvent* pad, int axis)
{
    return axis < pad->numAxes ? _sgamepad_deadzone(pad->axis[axis]) : 0.0f;
}

// The browser numbers a standard-mapped pad the way the W3C spec does.
enum
{
    _SGAMEPAD_A, _SGAMEPAD_B, _SGAMEPAD_X, _SGAMEPAD_Y,
    _SGAMEPAD_BACK = 8, _SGAMEPAD_START,
    _SGAMEPAD_DPAD_UP = 12, _SGAMEPAD_DPAD_DOWN, _SGAMEPAD_DPAD_LEFT, _SGAMEPAD_DPAD_RIGHT,
    _SGAMEPAD_LEFT_X = 0, _SGAMEPAD_LEFT_Y, _SGAMEPAD_RIGHT_X, _SGAMEPAD_RIGHT_Y,
};

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    // The browser's pads have to be sampled before they can be read; without
    // this the reading call throws.
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
        return;

    const int num = emscripten_get_num_gamepads();

    memset(_sgamepad.devices, 0, sizeof _sgamepad.devices);

    for (int pad = 0; pad < num && pad < SGAMEPAD_MAX_DEVICES; pad++)
    {
        EmscriptenGamepadEvent event;

        if (emscripten_get_gamepad_status(pad, &event) != EMSCRIPTEN_RESULT_SUCCESS)
            continue;

        if (!event.connected)
            continue;

        // The platform's own number is the device's, so a pad keeps it while
        // it is connected and loses it when the browser stops listing it.
        _sgamepad_device* device = &_sgamepad.devices[pad];
        sgamepad_state* state = &device->state;

        state->up = _sgamepad_axis(&event, _SGAMEPAD_LEFT_Y, -1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_UP);
        state->down = _sgamepad_axis(&event, _SGAMEPAD_LEFT_Y, +1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_DOWN);
        state->left = _sgamepad_axis(&event, _SGAMEPAD_LEFT_X, -1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_LEFT);
        state->right = _sgamepad_axis(&event, _SGAMEPAD_LEFT_X, +1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_RIGHT);

        state->a = _sgamepad_button(&event, _SGAMEPAD_A);
        state->b = _sgamepad_button(&event, _SGAMEPAD_B);
        state->x = _sgamepad_button(&event, _SGAMEPAD_X);
        state->y = _sgamepad_button(&event, _SGAMEPAD_Y);

        state->back = _sgamepad_button(&event, _SGAMEPAD_BACK);
        state->start = _sgamepad_button(&event, _SGAMEPAD_START);

        state->right_x = _sgamepad_stick(&event, _SGAMEPAD_RIGHT_X);
        state->right_y = _sgamepad_stick(&event, _SGAMEPAD_RIGHT_Y);

        device->present = true;
        device->used = _sgamepad_used(&event) || _sgamepad_holding(state);
    }

    _sgamepad_assign(num);
}

#elif defined(__APPLE__) && defined(__OBJC__)

// Anything on the pad the layer would not see otherwise is still use: a
// shoulder, a trigger, a stick pressed in.
static bool _sgamepad_use(GCExtendedGamepad* pad, const sgamepad_state* state)
{
    return _sgamepad_holding(state)
        || pad.leftShoulder.isPressed || pad.rightShoulder.isPressed
        || pad.leftTrigger.isPressed || pad.rightTrigger.isPressed
        || pad.leftThumbstickButton.isPressed || pad.rightThumbstickButton.isPressed;
}

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    // The framework lists only controllers it can place, so the list is the
    // claim test; a pad's place in it is its number while it is connected.
    NSArray<GCController*>* controllers = [GCController controllers];

    memset(_sgamepad.devices, 0, sizeof _sgamepad.devices);

    const NSUInteger count = MIN(controllers.count, (NSUInteger)SGAMEPAD_MAX_DEVICES);

    for (NSUInteger index = 0; index < count; index++)
    {
        // A controller with no extended profile is not a pad this layer can
        // read: the framework lists it, the layer leaves it alone.
        GCExtendedGamepad* pad = controllers[index].extendedGamepad;

        if (pad == nil)
            continue;

        _sgamepad_device* device = &_sgamepad.devices[index];
        sgamepad_state* state = &device->state;

        // The thumbstick's y grows upwards, the machine's downwards.
        state->up = pad.dpad.up.isPressed || _sgamepad_direction(pad.leftThumbstick.yAxis.value, +1);
        state->down = pad.dpad.down.isPressed || _sgamepad_direction(pad.leftThumbstick.yAxis.value, -1);
        state->left = pad.dpad.left.isPressed || _sgamepad_direction(pad.leftThumbstick.xAxis.value, -1);
        state->right = pad.dpad.right.isPressed || _sgamepad_direction(pad.leftThumbstick.xAxis.value, +1);

        state->a = pad.buttonA.isPressed;
        state->b = pad.buttonB.isPressed;
        state->x = pad.buttonX.isPressed;
        state->y = pad.buttonY.isPressed;

        state->back = pad.buttonOptions.isPressed;
        state->start = pad.buttonMenu.isPressed;

        state->right_x = _sgamepad_deadzone(pad.rightThumbstick.xAxis.value);
        state->right_y = _sgamepad_deadzone(-pad.rightThumbstick.yAxis.value);

        device->present = true;
        device->used = _sgamepad_use(pad, state);
    }

    _sgamepad_assign((int)controllers.count);
}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

// XInput's sticks are signed shorts with y upwards, and the machine's axis is
// the mouse's: x to the right, y downwards.
static float _sgamepad_xinput_thumb(SHORT value)
{
    return value / 32767.0f;
}

// Anything on the pad the layer would not see otherwise is still use: a
// shoulder, a trigger held past the threshold XInput itself uses.
static bool _sgamepad_xinput_use(const XINPUT_GAMEPAD* pad, const sgamepad_state* state)
{
    return _sgamepad_holding(state)
        || (pad->wButtons & (XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER))
        || pad->bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD
        || pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
}

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    memset(_sgamepad.devices, 0, sizeof _sgamepad.devices);

    // XInput numbers the pads itself: a slot it does not answer for is a pad
    // that is not there, and one plugged in later starts answering.
    for (DWORD index = 0; index < SGAMEPAD_MAX_DEVICES && index < XUSER_MAX_COUNT; index++)
    {
        XINPUT_STATE xinput;

        if (XInputGetState(index, &xinput) != ERROR_SUCCESS)
            continue;

        const XINPUT_GAMEPAD* pad = &xinput.Gamepad;

        _sgamepad_device* device = &_sgamepad.devices[index];
        sgamepad_state* state = &device->state;

        const float left_x = _sgamepad_xinput_thumb(pad->sThumbLX);
        const float left_y = _sgamepad_xinput_thumb(pad->sThumbLY);

        state->up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) || _sgamepad_direction(left_y, +1);
        state->down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || _sgamepad_direction(left_y, -1);
        state->left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || _sgamepad_direction(left_x, -1);
        state->right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || _sgamepad_direction(left_x, +1);

        state->a = (pad->wButtons & XINPUT_GAMEPAD_A) != 0;
        state->b = (pad->wButtons & XINPUT_GAMEPAD_B) != 0;
        state->x = (pad->wButtons & XINPUT_GAMEPAD_X) != 0;
        state->y = (pad->wButtons & XINPUT_GAMEPAD_Y) != 0;

        state->back = (pad->wButtons & XINPUT_GAMEPAD_BACK) != 0;
        state->start = (pad->wButtons & XINPUT_GAMEPAD_START) != 0;

        state->right_x = _sgamepad_deadzone(_sgamepad_xinput_thumb(pad->sThumbRX));
        state->right_y = _sgamepad_deadzone(-_sgamepad_xinput_thumb(pad->sThumbRY));

        device->present = true;
        device->used = _sgamepad_xinput_use(pad, state);
    }

    _sgamepad_assign((int)XUSER_MAX_COUNT);
}

#elif defined(__linux__)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

// The kernel spells a pad's buttons by position since 4.16; a build whose
// headers are older has the same codes under the older names.
#ifndef BTN_SOUTH
#define BTN_SOUTH BTN_A
#endif
#ifndef BTN_EAST
#define BTN_EAST BTN_B
#endif
#ifndef BTN_NORTH
#define BTN_NORTH BTN_X
#endif
#ifndef BTN_WEST
#define BTN_WEST BTN_Y
#endif

// What the layer reads of a pad's axes: the hat is the direction pad, the
// other four are the two sticks.
enum
{
    _SGAMEPAD_LINUX_X, _SGAMEPAD_LINUX_Y,
    _SGAMEPAD_LINUX_RX, _SGAMEPAD_LINUX_RY,
    _SGAMEPAD_LINUX_AXES,
};

static int _sgamepad_linux_axis(int code)
{
    switch (code)
    {
    case ABS_X:  return _SGAMEPAD_LINUX_X;
    case ABS_Y:  return _SGAMEPAD_LINUX_Y;
    case ABS_RX: return _SGAMEPAD_LINUX_RX;
    case ABS_RY: return _SGAMEPAD_LINUX_RY;
    default:     return -1;
    }
}

static bool _sgamepad_linux_bit(const unsigned char* bits, int code)
{
    return (bits[code / 8] & (1u << (code % 8))) != 0;
}

typedef struct
{
    bool    present;
    int     fd;
    char    path[sizeof "/dev/input/" + 256];
    bool    resync;                     // the kernel dropped events: read the pad again
    bool    used;                       // a button or axis the layer does not read
    struct input_absinfo axis[_SGAMEPAD_LINUX_AXES];
    float   stick[_SGAMEPAD_LINUX_AXES];
    int     hat_x, hat_y;
    sgamepad_state state;               // the buttons, held between the events that set them
} _sgamepad_linux_device;

static struct
{
    _sgamepad_linux_device devices[SGAMEPAD_MAX_DEVICES];
    int inotify;
} _sgamepad_linux = { .inotify = -1 };

static float _sgamepad_linux_stick(const struct input_absinfo* info, int value)
{
    const int range = info->maximum - info->minimum;

    if (range == 0)
        return 0.0f;

    return ((value - info->minimum) / (float)range) * 2.0f - 1.0f;
}

// What the pad holds right now, from the device itself: a pad that arrives with
// a stick held, or one whose events the kernel dropped, reads what it holds.
static void _sgamepad_linux_read_all(_sgamepad_linux_device* pad)
{
    static const int Axes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY };

    for (size_t i = 0; i < sizeof Axes / sizeof Axes[0]; i++)
    {
        const int index = _sgamepad_linux_axis(Axes[i]);

        if (ioctl(pad->fd, EVIOCGABS(Axes[i]), &pad->axis[index]) == 0)
            pad->stick[index] = _sgamepad_linux_stick(&pad->axis[index], pad->axis[index].value);
    }

    struct input_absinfo hat;

    if (ioctl(pad->fd, EVIOCGABS(ABS_HAT0X), &hat) == 0)
        pad->hat_x = hat.value;

    if (ioctl(pad->fd, EVIOCGABS(ABS_HAT0Y), &hat) == 0)
        pad->hat_y = hat.value;

    unsigned char keys[(KEY_CNT + 7) / 8] = { 0 };

    if (ioctl(pad->fd, EVIOCGKEY(sizeof keys), keys) < 0)
        return;

    pad->state.a = _sgamepad_linux_bit(keys, BTN_SOUTH);
    pad->state.b = _sgamepad_linux_bit(keys, BTN_EAST);
    pad->state.x = _sgamepad_linux_bit(keys, BTN_NORTH);
    pad->state.y = _sgamepad_linux_bit(keys, BTN_WEST);
    pad->state.back = _sgamepad_linux_bit(keys, BTN_SELECT);
    pad->state.start = _sgamepad_linux_bit(keys, BTN_START);
}

// A device the layer can read is one laid out the way the kernel's gamepad
// drivers do; anything else is left unclaimed rather than read wrongly.
static bool _sgamepad_linux_claimed(int fd)
{
    unsigned char abs[(ABS_CNT + 7) / 8] = { 0 };
    unsigned char keys[(KEY_CNT + 7) / 8] = { 0 };

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs), abs) < 0
        || ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) < 0)
        return false;

    return _sgamepad_linux_bit(abs, ABS_HAT0X) && _sgamepad_linux_bit(abs, ABS_HAT0Y)
        && _sgamepad_linux_bit(keys, BTN_SOUTH) && _sgamepad_linux_bit(keys, BTN_EAST)
        && _sgamepad_linux_bit(keys, BTN_NORTH) && _sgamepad_linux_bit(keys, BTN_WEST);
}

static void _sgamepad_linux_close(_sgamepad_linux_device* pad)
{
    close(pad->fd);
    memset(pad, 0, sizeof *pad);
    pad->fd = -1;
}

static void _sgamepad_linux_open(const char* path)
{
    for (int i = 0; i < SGAMEPAD_MAX_DEVICES; i++)
        if (_sgamepad_linux.devices[i].present && strcmp(_sgamepad_linux.devices[i].path, path) == 0)
            return;

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    if (fd < 0)
        return;

    if (!_sgamepad_linux_claimed(fd))
    {
        close(fd);
        return;
    }

    int slot = -1;

    for (int i = 0; i < SGAMEPAD_MAX_DEVICES && slot < 0; i++)
        if (!_sgamepad_linux.devices[i].present)
            slot = i;

    if (slot < 0)
    {
        close(fd);
        return;
    }

    _sgamepad_linux_device* pad = &_sgamepad_linux.devices[slot];

    memset(pad, 0, sizeof *pad);

    pad->fd = fd;
    pad->present = true;

    snprintf(pad->path, sizeof pad->path, "%s", path);

    _sgamepad_linux_read_all(pad);
}

static void _sgamepad_linux_scan(void)
{
    DIR* dir = opendir("/dev/input");

    if (dir == NULL)
        return;

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        char path[sizeof "/dev/input/" + sizeof entry->d_name];

        snprintf(path, sizeof path, "/dev/input/%s", entry->d_name);

        _sgamepad_linux_open(path);
    }

    closedir(dir);
}

// The watch says a device node came or went; the directory is read again only
// when it does, not once a frame.
static bool _sgamepad_linux_touched(void)
{
    char events[1024];
    bool touched = false;

    for (;;)
    {
        const ssize_t bytes = read(_sgamepad_linux.inotify, events, sizeof events);

        if (bytes <= 0)
            return touched;

        touched = true;
    }
}

static void _sgamepad_linux_event(_sgamepad_linux_device* pad, const struct input_event* event)
{
    if (event->type == EV_SYN)
    {
        // The kernel's queue overflowed and events were lost: whatever the pad
        // is holding now, it is not what this stream says.
        if (event->code == SYN_DROPPED)
            pad->resync = true;

        return;
    }

    if (event->type == EV_ABS)
    {
        switch (event->code)
        {
        case ABS_HAT0X:
            pad->hat_x = event->value;
            break;

        case ABS_HAT0Y:
            pad->hat_y = event->value;
            break;

        default:
        {
            const int index = _sgamepad_linux_axis(event->code);

            // An axis the layer does not read is not use on its own: triggers
            // jitter, and a pad nobody picked up must not take a slot.
            if (index >= 0)
                pad->stick[index] = _sgamepad_linux_stick(&pad->axis[index], event->value);

            break;
        }
        }
    }
    else if (event->type == EV_KEY)
    {
        const bool held = event->value != 0;

        switch (event->code)
        {
        case BTN_SOUTH:  pad->state.a = held; break;
        case BTN_EAST:   pad->state.b = held; break;
        case BTN_NORTH:  pad->state.x = held; break;
        case BTN_WEST:   pad->state.y = held; break;
        case BTN_SELECT: pad->state.back = held; break;
        case BTN_START:  pad->state.start = held; break;

        default:
            // A shoulder, a stick pressed in, the guide button: nothing the
            // machine reads, and use all the same.
            pad->used = pad->used || held;
            break;
        }
    }
}

// Reads everything the pad has queued; false when the pad has gone away, which
// gives its file back and frees the slot it took.
static bool _sgamepad_linux_poll(_sgamepad_linux_device* pad)
{
    struct input_event events[32];

    for (;;)
    {
        const ssize_t bytes = read(pad->fd, events, sizeof events);

        if (bytes < 0)
            return errno != ENODEV && errno != EIO;

        if (bytes == 0)
            return true;

        for (size_t i = 0; i < (size_t)bytes / sizeof events[0]; i++)
            _sgamepad_linux_event(pad, &events[i]);
    }
}

// The session holds the event devices, so no udev rule is needed; a sandbox
// that hides /dev/input — a Flatpak without --device=all — leaves no pads.
static void _sgamepad_backend_setup(void)
{
    memset(_sgamepad_linux.devices, 0, sizeof _sgamepad_linux.devices);

    _sgamepad_linux.inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (_sgamepad_linux.inotify >= 0)
        inotify_add_watch(_sgamepad_linux.inotify, "/dev/input", IN_CREATE | IN_DELETE | IN_ATTRIB);

    _sgamepad_linux_scan();
}

static void _sgamepad_backend_shutdown(void)
{
    for (int i = 0; i < SGAMEPAD_MAX_DEVICES; i++)
        if (_sgamepad_linux.devices[i].present)
            _sgamepad_linux_close(&_sgamepad_linux.devices[i]);

    if (_sgamepad_linux.inotify >= 0)
        close(_sgamepad_linux.inotify);

    _sgamepad_linux.inotify = -1;
}

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    // The pads are read before the directory is, so a pad replugged between
    // two frames has its old entry closed before the scan runs.
    bool gone = false;

    for (int i = 0; i < SGAMEPAD_MAX_DEVICES; i++)
    {
        _sgamepad_linux_device* pad = &_sgamepad_linux.devices[i];

        if (pad->present && !_sgamepad_linux_poll(pad))
        {
            _sgamepad_linux_close(pad);
            gone = true;
        }
    }

    if (gone || (_sgamepad_linux.inotify >= 0 && _sgamepad_linux_touched()))
        _sgamepad_linux_scan();

    memset(_sgamepad.devices, 0, sizeof _sgamepad.devices);

    for (int i = 0; i < SGAMEPAD_MAX_DEVICES; i++)
    {
        _sgamepad_linux_device* pad = &_sgamepad_linux.devices[i];

        if (!pad->present)
            continue;

        if (pad->resync)
        {
            _sgamepad_linux_read_all(pad);
            pad->resync = false;
        }

        _sgamepad_device* device = &_sgamepad.devices[i];
        sgamepad_state* state = &device->state;

        // The buttons are the ones the events left behind; everything else is
        // what the axes say right now, the way the other backends read it.
        *state = pad->state;

        const float left_y = pad->stick[_SGAMEPAD_LINUX_Y];
        const float left_x = pad->stick[_SGAMEPAD_LINUX_X];

        // The kernel's y grows downwards, the machine's too.
        state->up = state->up || pad->hat_y < 0 || _sgamepad_direction(left_y, -1);
        state->down = state->down || pad->hat_y > 0 || _sgamepad_direction(left_y, +1);
        state->left = state->left || pad->hat_x < 0 || _sgamepad_direction(left_x, -1);
        state->right = state->right || pad->hat_x > 0 || _sgamepad_direction(left_x, +1);

        state->right_x = _sgamepad_deadzone(pad->stick[_SGAMEPAD_LINUX_RX]);
        state->right_y = _sgamepad_deadzone(pad->stick[_SGAMEPAD_LINUX_RY]);

        device->present = true;
        device->used = pad->used || _sgamepad_holding(state);
    }

    _sgamepad_assign(SGAMEPAD_MAX_DEVICES);
}

#else

// No backend here yet: the layer builds and reports no gamepads, which is what
// a platform without one needs until its backend arrives.
SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    _sgamepad_assign(0);
}

#endif /* backend */

#endif /* SGAMEPAD_IMPL */
