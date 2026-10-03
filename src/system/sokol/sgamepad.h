// sgamepad.h -- gamepads, in the shape of the other sokol headers.
//
// What the layer above it sees is one API for every platform: setup, a state
// that is recorded once a frame, and up to four gamepads read back. Which
// backend answers depends on where it is compiled — the browser's Gamepad API
// today, XInput, GameController.framework or evdev behind the same calls later.
// A platform with no backend reports no gamepads and builds all the same.
//
//     Do this:
//         #define SGAMEPAD_IMPL
//     before you include this file in *one* C file to create the implementation.
//
//     The following defines are used by the implementation to pick a backend:
//     __EMSCRIPTEN__
//
//     ...optionally provide the following macros to override defaults:
//     SOKOL_ASSERT(c)     - your own assert macro (default: assert(c))
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

// The most gamepads the layer carries, the way the machine numbers them.
#define SGAMEPAD_MAX_GAMEPADS   4

// One gamepad's buttons, laid out the way the machine reads them, plus the
// two the machine has no room for: the layer reads back and start itself —
// a pad's back opens the menu, the way the SDL layer's did.
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
#endif

typedef struct
{
    bool            seen[SGAMEPAD_MAX_GAMEPADS];    // the device slots in use
    int             ids[SGAMEPAD_MAX_GAMEPADS];     // which browser pad owns each
    sgamepad_state  states[SGAMEPAD_MAX_GAMEPADS];
    float           deadzone;
} _sgamepad_t;

static _sgamepad_t _sgamepad;

SGAMEPAD_API_IMPL void sgamepad_setup(const sgamepad_desc* desc)
{
    memset(&_sgamepad, 0, sizeof _sgamepad);

    _sgamepad.deadzone = (desc && desc->deadzone > 0.0f) ? desc->deadzone : 0.5f;
    _sgamepad.ids[0] = _sgamepad.ids[1] = _sgamepad.ids[2] = _sgamepad.ids[3] = -1;
}

SGAMEPAD_API_IMPL void sgamepad_shutdown(void)
{
    memset(&_sgamepad, 0, sizeof _sgamepad);
}

SGAMEPAD_API_IMPL int sgamepad_num_gamepads(void)
{
    int count = 0;

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
        if (_sgamepad.seen[i])
            count++;

    return count;
}

SGAMEPAD_API_IMPL sgamepad_state sgamepad_gamepad_state(int slot)
{
    sgamepad_state empty = { 0 };

    return (slot >= 0 && slot < SGAMEPAD_MAX_GAMEPADS && _sgamepad.seen[slot])
        ? _sgamepad.states[slot]
        : empty;
}

#if defined(__EMSCRIPTEN__)

// The browser reports every pad it can see, whether or not the page ever asked
// about it, so a pad that has never been touched is only assigned a slot once
// something on it moves — the way the SDL layer picks the pads to listen to.
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
    if (axis >= pad->numAxes)
        return false;

    return dir < 0 ? pad->axis[axis] < -_sgamepad.deadzone : pad->axis[axis] > _sgamepad.deadzone;
}

static bool _sgamepad_button(const EmscriptenGamepadEvent* pad, int button)
{
    return button < pad->numButtons && pad->digitalButton[button];
}

// The browser numbers a standard-mapped pad the way the W3C spec does.
enum
{
    _SGAMEPAD_A, _SGAMEPAD_B, _SGAMEPAD_X, _SGAMEPAD_Y,
    _SGAMEPAD_BACK = 8, _SGAMEPAD_START,
    _SGAMEPAD_DPAD_UP = 12, _SGAMEPAD_DPAD_DOWN, _SGAMEPAD_DPAD_LEFT, _SGAMEPAD_DPAD_RIGHT,
};

SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
    // The browser's pads have to be sampled before they can be read; without
    // this the reading call throws.
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
        return;

    const int num = emscripten_get_num_gamepads();

    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
    {
        _sgamepad.states[i].data = 0;
        _sgamepad.states[i].back = false;
        _sgamepad.states[i].start = false;
    }

    for (int pad = 0; pad < num; pad++)
    {
        EmscriptenGamepadEvent event;

        if (emscripten_get_gamepad_status(pad, &event) != EMSCRIPTEN_RESULT_SUCCESS)
            continue;

        if (!event.connected)
            continue;

        // A pad keeps its slot until it goes away; a new one takes the first
        // free slot as soon as it shows it is being used.
        int slot = -1;

        for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
            if (_sgamepad.seen[i] && _sgamepad.ids[i] == pad)
                slot = i;

        if (slot < 0)
        {
            if (!_sgamepad_used(&event))
                continue;

            for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS && slot < 0; i++)
                if (!_sgamepad.seen[i])
                    slot = i;

            if (slot < 0)
                continue;

            _sgamepad.seen[slot] = true;
            _sgamepad.ids[slot] = pad;
        }

        sgamepad_state* state = &_sgamepad.states[slot];

        state->up = _sgamepad_axis(&event, 1, -1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_UP);
        state->down = _sgamepad_axis(&event, 1, +1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_DOWN);
        state->left = _sgamepad_axis(&event, 0, -1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_LEFT);
        state->right = _sgamepad_axis(&event, 0, +1) || _sgamepad_button(&event, _SGAMEPAD_DPAD_RIGHT);

        state->a = _sgamepad_button(&event, _SGAMEPAD_A);
        state->b = _sgamepad_button(&event, _SGAMEPAD_B);
        state->x = _sgamepad_button(&event, _SGAMEPAD_X);
        state->y = _sgamepad_button(&event, _SGAMEPAD_Y);

        state->back = _sgamepad_button(&event, _SGAMEPAD_BACK);
        state->start = _sgamepad_button(&event, _SGAMEPAD_START);
    }

    // A pad the browser no longer lists has gone away, and frees its slot.
    for (int i = 0; i < SGAMEPAD_MAX_GAMEPADS; i++)
    {
        if (!_sgamepad.seen[i])
            continue;

        EmscriptenGamepadEvent event;

        if (_sgamepad.ids[i] >= num
            || emscripten_get_gamepad_status(_sgamepad.ids[i], &event) != EMSCRIPTEN_RESULT_SUCCESS
            || !event.connected)
        {
            _sgamepad.seen[i] = false;
            _sgamepad.ids[i] = -1;
        }
    }
}

#else

// No backend here yet: the layer builds and reports no gamepads, which is what
// the desktop needs until its own backend arrives.
SGAMEPAD_API_IMPL void sgamepad_record_state(void)
{
}

#endif /* backend */

#endif /* SGAMEPAD_IMPL */
