#ifndef SOKOL_GAMEPAD_INCLUDED
/*
    sokol_gamepad.h -- cross-platform gamepad API

    Project URL: https://github.com/floooh/sokol

    Do this:
        #define SOKOL_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    To use:  
    - In you app initialization code call sgamepad_init()
    - At the exact time you want to record input state call sgamepad_record_state()
    - Get the state for a particular gamepad like this:
        sgamepad_gamepad_state state;
        sgamepad_get_gamepad_state(0, &state);
    
    sgamepad_gamepad_state's members are set to the contoller's state as recorded previously.

    Analog stick states are pre-processed to take dead zones into account: in most cases you should rely on
    direction_x/direction_y/magnitude for input processing.
*/

#define SOKOL_GAMEPAD_INCLUDED (1)
#include <stdint.h>
#include <stdbool.h>

#ifndef SOKOL_API_DECL
#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_IMPL)
#define SOKOL_API_DECL __declspec(dllexport)
#elif defined(_WIN32) && defined(SOKOL_DLL)
#define SOKOL_API_DECL __declspec(dllimport)
#else
#define SOKOL_API_DECL extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* Bit flags for digital_inputs bitfield */
    typedef enum sgamepad_digital_inputs {
        SGAMEPAD_BUTTON_DPAD_UP     = 0x0001,
        SGAMEPAD_BUTTON_DPAD_DOWN   = 0x0002,
        SGAMEPAD_BUTTON_DPAD_LEFT   = 0x0004,
        SGAMEPAD_BUTTON_DPAD_RIGHT  = 0x0008,
        SGAMEPAD_BUTTON_START       = 0x0010,
        SGAMEPAD_BUTTON_BACK        = 0x0020, // Select on DualShock
        SGAMEPAD_BUTTON_A           = 0x0040, // X on DualShock
        SGAMEPAD_BUTTON_B           = 0x0080, // Circle on DualShock
        SGAMEPAD_BUTTON_X           = 0x0100, // Square on DualShock
        SGAMEPAD_BUTTON_Y           = 0x0200, // Triangle on DualShock
        SGAMEPAD_BUTTON_LEFT_THUMB  = 0x0400, // L3 on DualShock
        SGAMEPAD_BUTTON_RIGHT_THUMB = 0x0800, // R3 on DualShock
    } sgamepad_digital_inputs;

    typedef struct sgamepad_analog_stick_state {
        float normalized_x; //X component as reported by underlying API, scaled 0 to 1
        float normalized_y; //Y component as reported by underlying API, scaled 0 to 1
        float direction_x; //X component of normalized direction vector
        float direction_y; //Y component of normalized direction vector
        float magnitude;   //Normalized magnitude
    } sgamepad_analog_stick_state;

    typedef struct sgamepad_gamepad_state {
        uint16_t digital_inputs;
        sgamepad_analog_stick_state left_stick;
        sgamepad_analog_stick_state right_stick;
        float left_shoulder;
        float right_shoulder;
        float left_trigger;
        float right_trigger;
    } sgamepad_gamepad_state;

    SOKOL_API_DECL unsigned int sgamepad_get_max_supported_gamepads();

    SOKOL_API_DECL void sgamepad_init();

    SOKOL_API_DECL void sgamepad_shutdown();

    SOKOL_API_DECL void sgamepad_record_state();

    SOKOL_API_DECL void sgamepad_get_gamepad_state(unsigned int index, sgamepad_gamepad_state* pstate);

#ifdef __cplusplus
} /* extern "C" */

/* reference-based equivalents for c++ */

#endif
#endif // SOKOL_GAMEPAD_INCLUDED

#ifdef SOKOL_IMPL
#define SOKOL_GAMEPAD_IMPL_INCLUDED (1)
#include <string.h> /* memset */
#include <math.h>

#ifndef SOKOL_API_IMPL
    #define SOKOL_API_IMPL
#endif
#ifndef SOKOL_DEBUG
    #ifndef NDEBUG
        #define SOKOL_DEBUG (1)
    #endif
#endif

#ifndef SOKOL_LOG
    #ifdef SOKOL_DEBUG
        #if defined(__ANDROID__)
            #include <android/log.h>
            #define SOKOL_LOG(s) { SOKOL_ASSERT(s); __android_log_write(ANDROID_LOG_INFO, "SOKOL_APP", s); }
        #else
            #include <stdio.h>
            #define SOKOL_LOG(s) { SOKOL_ASSERT(s); puts(s); }
        #endif
    #else
        #define SOKOL_LOG(s)
    #endif
#endif

#ifndef _SOKOL_PRIVATE
    #if defined(__GNUC__) || defined(__clang__)
        #define _SOKOL_PRIVATE __attribute__((unused)) static
    #else
        #define _SOKOL_PRIVATE static
    #endif
#endif

/*== COMMON INCLUDES AND DEFINES ==================================*/

_SOKOL_PRIVATE float _sgamepad_normalize_analog_trigger(float value, float max_value, float activation_value) {
    if (value < activation_value) {
        return 0.0f;
    }

    float output = (value - activation_value) / (max_value - activation_value);
    return output;
}

_SOKOL_PRIVATE void _sgamepad_generate_analog_stick_state(float x_value, float y_value, float max_magnitude, float dead_zone_magnitude, sgamepad_analog_stick_state* pstate) {
    float magnitude = 0.0f;
    if (max_magnitude != 1.0f) {
        pstate->normalized_x = fmax(fmin(x_value/max_magnitude, 1.0f), -1.0f);
        pstate->normalized_y = fmax(fmin(y_value/max_magnitude, 1.0f), -1.0f);
        magnitude = sqrtf(x_value * x_value + y_value * y_value);
    } else {
        pstate->normalized_x = x_value;
        pstate->normalized_y = y_value;
        magnitude = fmin(sqrtf(x_value * x_value + y_value * y_value), 1.0f);
    }
        
    if (magnitude <= dead_zone_magnitude) {
        pstate->direction_x = 0.0f;
        pstate->direction_y = 0.0f;
        pstate->magnitude = 0.0f;
        return;
    }

    pstate->direction_x = x_value / magnitude;
    pstate->direction_y = y_value / magnitude;
    magnitude = fmin(magnitude, max_magnitude);
    pstate->magnitude = (magnitude - dead_zone_magnitude) / (max_magnitude - dead_zone_magnitude);
}

/*== PLATFORM SPECIFIC INCLUDES AND DEFINES ==================================*/
#if defined (_SAPP_WIN32) || defined(_SAPP_APPLE) || defined(_SAPP_LINUX) || defined(_SAPP_EMSCRIPTEN)
    #define SGAMEPAD_MAX_SUPPORTED_GAMEPADS 4
#else
    #define SGAMEPAD_MAX_SUPPORTED_GAMEPADS 0
#endif

typedef struct sgamepad {
    sgamepad_gamepad_state gamepad_states[SGAMEPAD_MAX_SUPPORTED_GAMEPADS];
} sgamepad;

_SOKOL_PRIVATE sgamepad _sgamepad;

/*== Desktop Implementation ============================================*/

#if defined (_SAPP_WIN32) || defined(_SAPP_APPLE) || defined(_SAPP_LINUX)

#define SOKOL_RELEASE                0
#define SOKOL_PRESS                  1

#define SOKOL_HAT_CENTERED           0
#define SOKOL_HAT_UP                 1
#define SOKOL_HAT_RIGHT              2
#define SOKOL_HAT_DOWN               4
#define SOKOL_HAT_LEFT               8
#define SOKOL_HAT_RIGHT_UP           (SOKOL_HAT_RIGHT | SOKOL_HAT_UP)
#define SOKOL_HAT_RIGHT_DOWN         (SOKOL_HAT_RIGHT | SOKOL_HAT_DOWN)
#define SOKOL_HAT_LEFT_UP            (SOKOL_HAT_LEFT  | SOKOL_HAT_UP)
#define SOKOL_HAT_LEFT_DOWN          (SOKOL_HAT_LEFT  | SOKOL_HAT_DOWN)

#define SOKOL_JOYSTICK_1             0
#define SOKOL_JOYSTICK_2             1
#define SOKOL_JOYSTICK_3             2
#define SOKOL_JOYSTICK_4             3
#define SOKOL_JOYSTICK_5             4
#define SOKOL_JOYSTICK_6             5
#define SOKOL_JOYSTICK_7             6
#define SOKOL_JOYSTICK_8             7
#define SOKOL_JOYSTICK_9             8
#define SOKOL_JOYSTICK_10            9
#define SOKOL_JOYSTICK_11            10
#define SOKOL_JOYSTICK_12            11
#define SOKOL_JOYSTICK_13            12
#define SOKOL_JOYSTICK_14            13
#define SOKOL_JOYSTICK_15            14
#define SOKOL_JOYSTICK_16            15
#define SOKOL_JOYSTICK_LAST          SOKOL_JOYSTICK_16

#define SOKOL_GAMEPAD_BUTTON_A               0
#define SOKOL_GAMEPAD_BUTTON_B               1
#define SOKOL_GAMEPAD_BUTTON_X               2
#define SOKOL_GAMEPAD_BUTTON_Y               3
#define SOKOL_GAMEPAD_BUTTON_LEFT_BUMPER     4
#define SOKOL_GAMEPAD_BUTTON_RIGHT_BUMPER    5
#define SOKOL_GAMEPAD_BUTTON_BACK            6
#define SOKOL_GAMEPAD_BUTTON_START           7
#define SOKOL_GAMEPAD_BUTTON_GUIDE           8
#define SOKOL_GAMEPAD_BUTTON_LEFT_THUMB      9
#define SOKOL_GAMEPAD_BUTTON_RIGHT_THUMB     10
#define SOKOL_GAMEPAD_BUTTON_DPAD_UP         11
#define SOKOL_GAMEPAD_BUTTON_DPAD_RIGHT      12
#define SOKOL_GAMEPAD_BUTTON_DPAD_DOWN       13
#define SOKOL_GAMEPAD_BUTTON_DPAD_LEFT       14
#define SOKOL_GAMEPAD_BUTTON_LAST            SOKOL_GAMEPAD_BUTTON_DPAD_LEFT

#define SOKOL_GAMEPAD_AXIS_LEFT_X        0
#define SOKOL_GAMEPAD_AXIS_LEFT_Y        1
#define SOKOL_GAMEPAD_AXIS_RIGHT_X       2
#define SOKOL_GAMEPAD_AXIS_RIGHT_Y       3
#define SOKOL_GAMEPAD_AXIS_LEFT_TRIGGER  4
#define SOKOL_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define SOKOL_GAMEPAD_AXIS_LAST          SOKOL_GAMEPAD_AXIS_RIGHT_TRIGGER

#define SOKOL_NOT_INITIALIZED        0x00010001
#define SOKOL_INVALID_ENUM           0x00010003
#define SOKOL_INVALID_VALUE          0x00010004
#define SOKOL_PLATFORM_ERROR         0x00010008
#define SOKOL_CONNECTED              0x00040001
#define SOKOL_DISCONNECTED           0x00040002

typedef struct SOKOL_gamepadstate
{
    unsigned char buttons[15];
    float axes[6];
} SOKOL_gamepadstate;

#define _SOKOL_POLL_PRESENCE     0
#define _SOKOL_POLL_AXES         1
#define _SOKOL_POLL_BUTTONS      2
#define _SOKOL_POLL_ALL          (_SOKOL_POLL_AXES | _SOKOL_POLL_BUTTONS)

typedef struct _SOKOL_library     _SOKOL_library;
typedef struct _SOKOL_mapelement  _SOKOL_mapelement;
typedef struct _SOKOL_mapping     _SOKOL_mapping;
typedef struct _SOKOL_joystick    _SOKOL_joystick;

struct _SOKOL_mapelement
{
    uint8_t         type;
    uint8_t         index;
    int8_t          axisScale;
    int8_t          axisOffset;
};

struct _SOKOL_mapping
{
    const char *name;
    const char *guid;
    _SOKOL_mapelement buttons[15];
    _SOKOL_mapelement axes[6];
};

_SOKOL_PRIVATE void _sokol_InputJoystick(_SOKOL_joystick* js, int event);
_SOKOL_PRIVATE void _sokol_InputJoystickAxis(_SOKOL_joystick* js, int axis, float value);
_SOKOL_PRIVATE void _sokol_InputJoystickButton(_SOKOL_joystick* js, int button, char value);
_SOKOL_PRIVATE void _sokol_InputJoystickHat(_SOKOL_joystick* js, int hat, char value);
_SOKOL_PRIVATE void _sokol_InputError(int code, const char* format, ...);

_SOKOL_PRIVATE _SOKOL_joystick* _sokol_AllocJoystick(const char* name, const char* guid, int axisCount, int buttonCount, int hatCount);
_SOKOL_PRIVATE void _sokol_FreeJoystick(_SOKOL_joystick* js);


#define _SOKOL_JOYSTICK_AXIS     1
#define _SOKOL_JOYSTICK_BUTTON   2
#define _SOKOL_JOYSTICK_HATBIT   3

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

#if defined (_SAPP_WIN32)

#include <dinput.h>
#include <xinput.h>
#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
 #define XINPUT_DEVSUBTYPE_FLIGHT_STICK 0x04
#endif

#ifndef DIDFT_OPTIONAL
 #define DIDFT_OPTIONAL 0x80000000
#endif

// xinput.dll function pointer typedefs
typedef DWORD (WINAPI * PFN_XInputGetCapabilities)(DWORD,DWORD,XINPUT_CAPABILITIES*);
typedef DWORD (WINAPI * PFN_XInputGetState)(DWORD,XINPUT_STATE*);
#define XInputGetCapabilities _sokol_.js.xinput.GetCapabilities
#define XInputGetState _sokol_.js.xinput.GetState

// dinput8.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_DirectInput8Create)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
#define DirectInput8Create _sokol_.js.dinput8.Create

typedef struct _SOKOL_libraryPlatform
{
    HINSTANCE           instance;

    struct {
        HINSTANCE                       instance;
        PFN_DirectInput8Create          Create;
        IDirectInput8W*                 api;
    } dinput8;

    struct {
        HINSTANCE                       instance;
        PFN_XInputGetCapabilities       GetCapabilities;
        PFN_XInputGetState              GetState;
    } xinput;

} _SOKOL_libraryPlatform;

typedef struct _SOKOL_joyobjectWin32
{
    int                     offset;
    int                     type;
} _SOKOL_joyobjectWin32;

typedef struct _SOKOL_joystickPlatform
{
    _SOKOL_joyobjectWin32*    objects;
    int                     objectCount;
    IDirectInputDevice8W*   device;
    DWORD                   index;
    GUID                    guid;
} _SOKOL_joystickPlatform;

#elif defined (_SAPP_APPLE)

#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDKeys.h>

typedef struct _SOKOL_libraryPlatform
{
    IOHIDManagerRef     hidManager;
} _SOKOL_libraryPlatform;

typedef struct _SOKOL_joystickPlatform
{
    IOHIDDeviceRef      device;
    CFMutableArrayRef   axes;
    CFMutableArrayRef   buttons;
    CFMutableArrayRef   hats;
} _SOKOL_joystickPlatform;

#elif defined(_SAPP_LINUX)

#include <linux/input.h>
#include <regex.h>

typedef struct _SOKOL_joystickPlatform
{
    int                     fd;
    char                    path[PATH_MAX];
    int                     keyMap[KEY_CNT - BTN_MISC];
    int                     absMap[ABS_CNT];
    struct input_absinfo    absInfo[ABS_CNT];
    int                     hats[4][2];
} _SOKOL_joystickPlatform;

// Linux-specific joystick API data
//
typedef struct _SOKOL_libraryPlatform
{
    int                     inotify;
    int                     watch;
    regex_t                 regex;
    bool                regexCompiled;
    bool                dropped;
} _SOKOL_libraryPlatform;

#endif

_SOKOL_PRIVATE bool _sokol_InitJoysticks(void);
_SOKOL_PRIVATE void _sokol_TerminateJoysticks(void);
_SOKOL_PRIVATE bool _sokol_PollJoystick(_SOKOL_joystick* js, int mode);

struct _SOKOL_joystick
{
    bool        allocated;
    bool        connected;
    float*          axes;
    int             axisCount;
    unsigned char*  buttons;
    int             buttonCount;
    unsigned char*  hats;
    int             hatCount;
    char            name[128];
    char            guid[33];
    const _SOKOL_mapping*   mapping;

    _SOKOL_joystickPlatform js;
};

struct _SOKOL_library
{
    bool                    joysticksInitialized;
    _SOKOL_joystick         joysticks[SOKOL_JOYSTICK_LAST + 1];
    const _SOKOL_mapping*   mappings;
    int                     mappingCount;

    _SOKOL_libraryPlatform js;
};

_SOKOL_PRIVATE _SOKOL_library _sokol_;

_SOKOL_PRIVATE void _sokol_InputError(int code, const char* format, ...)
{
#ifdef SOKOL_DEBUG
    char buf[1024];

    va_list vl;

    va_start(vl, format);
    vsnprintf(buf, sizeof(buf), format, vl);
    va_end(vl);

    SOKOL_LOG(buf);
#endif
}

_SOKOL_PRIVATE bool initJoysticks(void)
{
    if (!_sokol_.joysticksInitialized)
    {
        if (!_sokol_InitJoysticks())
        {
            _sokol_TerminateJoysticks();
            return false;
        }
    }

    return _sokol_.joysticksInitialized = true;
}

_SOKOL_PRIVATE const _SOKOL_mapping* findMapping(const char* guid)
{
    for (int i = 0;  i < _sokol_.mappingCount;  i++)
    {
        if (strcmp(_sokol_.mappings[i].guid, guid) == 0)
            return _sokol_.mappings + i;
    }

    return NULL;
}

_SOKOL_PRIVATE bool isValidElementForJoystick(const _SOKOL_mapelement* e,
                                          const _SOKOL_joystick* js)
{
    if (e->type == _SOKOL_JOYSTICK_HATBIT && (e->index >> 4) >= js->hatCount)
        return false;
    else if (e->type == _SOKOL_JOYSTICK_BUTTON && e->index >= js->buttonCount)
        return false;
    else if (e->type == _SOKOL_JOYSTICK_AXIS && e->index >= js->axisCount)
        return false;

    return true;
}

_SOKOL_PRIVATE const _SOKOL_mapping* findValidMapping(const _SOKOL_joystick* js)
{
    const _SOKOL_mapping* mapping = findMapping(js->guid);
    
    if (mapping)
    {
        int i;

        for (i = 0;  i <= SOKOL_GAMEPAD_BUTTON_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->buttons + i, js))
                return NULL;
        }

        for (i = 0;  i <= SOKOL_GAMEPAD_AXIS_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->axes + i, js))
                return NULL;
        }
    }

    return mapping;
}

_SOKOL_PRIVATE void _sokol_InputJoystick(_SOKOL_joystick* js, int event)
{
    assert(js != NULL);
    assert(event == SOKOL_CONNECTED || event == SOKOL_DISCONNECTED);

    if (event == SOKOL_CONNECTED)
        js->connected = true;
    else if (event == SOKOL_DISCONNECTED)
        js->connected = false;
}

_SOKOL_PRIVATE void _sokol_InputJoystickAxis(_SOKOL_joystick* js, int axis, float value)
{
    assert(js != NULL);
    assert(axis >= 0);
    assert(axis < js->axisCount);

    js->axes[axis] = value;
}

_SOKOL_PRIVATE void _sokol_InputJoystickButton(_SOKOL_joystick* js, int button, char value)
{
    assert(js != NULL);
    assert(button >= 0);
    assert(button < js->buttonCount);
    assert(value == SOKOL_PRESS || value == SOKOL_RELEASE);

    js->buttons[button] = value;
}

_SOKOL_PRIVATE void _sokol_InputJoystickHat(_SOKOL_joystick* js, int hat, char value)
{
    int base;

    assert(js != NULL);
    assert(hat >= 0);
    assert(hat < js->hatCount);

    // Valid hat values only use the least significant nibble
    assert((value & 0xf0) == 0);
    // Valid hat values do not have both bits of an axis set
    assert((value & SOKOL_HAT_LEFT) == 0 || (value & SOKOL_HAT_RIGHT) == 0);
    assert((value & SOKOL_HAT_UP) == 0 || (value & SOKOL_HAT_DOWN) == 0);

    base = js->buttonCount + hat * 4;

    js->buttons[base + 0] = (value & 0x01) ? SOKOL_PRESS : SOKOL_RELEASE;
    js->buttons[base + 1] = (value & 0x02) ? SOKOL_PRESS : SOKOL_RELEASE;
    js->buttons[base + 2] = (value & 0x04) ? SOKOL_PRESS : SOKOL_RELEASE;
    js->buttons[base + 3] = (value & 0x08) ? SOKOL_PRESS : SOKOL_RELEASE;

    js->hats[hat] = value;
}

_SOKOL_PRIVATE _SOKOL_joystick* _sokol_AllocJoystick(const char* name, const char* guid, int axisCount, int buttonCount, int hatCount)
{
    int jid;
    _SOKOL_joystick* js;

    for (jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        if (!_sokol_.joysticks[jid].allocated)
            break;
    }

    if (jid > SOKOL_JOYSTICK_LAST)
        return NULL;

    js = _sokol_.joysticks + jid;
    js->allocated   = true;
    js->axes        = calloc(axisCount, sizeof(float));
    js->buttons     = calloc(buttonCount + (size_t) hatCount * 4, 1);
    js->hats        = calloc(hatCount, 1);
    js->axisCount   = axisCount;
    js->buttonCount = buttonCount;
    js->hatCount    = hatCount;

    strncpy(js->name, name, sizeof(js->name) - 1);
    strncpy(js->guid, guid, sizeof(js->guid) - 1);
    js->mapping = findValidMapping(js);

    return js;
}

_SOKOL_PRIVATE void _sokol_FreeJoystick(_SOKOL_joystick* js)
{
    free(js->axes);
    free(js->buttons);
    free(js->hats);
    memset(js, 0, sizeof(_SOKOL_joystick));
}

_SOKOL_PRIVATE int sokol_GetGamepadState(int jid, SOKOL_gamepadstate* state)
{
    int i;
    _SOKOL_joystick* js;

    assert(jid >= SOKOL_JOYSTICK_1);
    assert(jid <= SOKOL_JOYSTICK_LAST);
    assert(state != NULL);

    memset(state, 0, sizeof(SOKOL_gamepadstate));

    if (jid < 0 || jid > SOKOL_JOYSTICK_LAST)
    {
        _sokol_InputError(SOKOL_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return false;
    }

    if (!initJoysticks())
        return false;

    js = _sokol_.joysticks + jid;
    if (!js->connected)
        return false;

    if (!_sokol_PollJoystick(js, _SOKOL_POLL_ALL))
        return false;
    
    if (!js->mapping)
        return false;

    for (i = 0;  i <= SOKOL_GAMEPAD_BUTTON_LAST;  i++)
    {
        const _SOKOL_mapelement* e = js->mapping->buttons + i;
        if (e->type == _SOKOL_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            // HACK: This should be baked into the value transform
            // TODO: Bake into transform when implementing output modifiers
            if (e->axisOffset < 0 || (e->axisOffset == 0 && e->axisScale > 0))
            {
                if (value >= 0.f)
                    state->buttons[i] = SOKOL_PRESS;
            }
            else
            {
                if (value <= 0.f)
                    state->buttons[i] = SOKOL_PRESS;
            }
        }
        else if (e->type == _SOKOL_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->buttons[i] = SOKOL_PRESS;
        }
        else if (e->type == _SOKOL_JOYSTICK_BUTTON)
            state->buttons[i] = js->buttons[e->index];
    }

    for (i = 0;  i <= SOKOL_GAMEPAD_AXIS_LAST;  i++)
    {
        const _SOKOL_mapelement* e = js->mapping->axes + i;
        if (e->type == _SOKOL_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            state->axes[i] = fminf(fmaxf(value, -1.f), 1.f);
        }
        else if (e->type == _SOKOL_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->axes[i] = 1.f;
            else
                state->axes[i] = -1.f;
        }
        else if (e->type == _SOKOL_JOYSTICK_BUTTON)
            state->axes[i] = js->buttons[e->index] * 2.f - 1.f;
    }

    return true;
}

_SOKOL_PRIVATE void _sgamepad_record_state() {
    for (int i = 0; i < SGAMEPAD_MAX_SUPPORTED_GAMEPADS; i++)
    {
        sgamepad_gamepad_state* target = _sgamepad.gamepad_states + i;

        SOKOL_gamepadstate state;
        if(sokol_GetGamepadState(i, &state))
        {
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_A])            target->digital_inputs |= SGAMEPAD_BUTTON_A;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_B])            target->digital_inputs |= SGAMEPAD_BUTTON_B;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_X])            target->digital_inputs |= SGAMEPAD_BUTTON_X;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_Y])            target->digital_inputs |= SGAMEPAD_BUTTON_Y;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_BACK])         target->digital_inputs |= SGAMEPAD_BUTTON_BACK;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_START])        target->digital_inputs |= SGAMEPAD_BUTTON_START;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_LEFT_THUMB])   target->digital_inputs |= SGAMEPAD_BUTTON_LEFT_THUMB;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_RIGHT_THUMB])  target->digital_inputs |= SGAMEPAD_BUTTON_RIGHT_THUMB;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_DPAD_UP])      target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_UP;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_DPAD_RIGHT])   target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_RIGHT;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_DPAD_DOWN])    target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_DOWN;
            if(state.buttons[SOKOL_GAMEPAD_BUTTON_DPAD_LEFT])    target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_LEFT;

            _sgamepad_generate_analog_stick_state(state.axes[SOKOL_GAMEPAD_AXIS_LEFT_X], state.axes[SOKOL_GAMEPAD_AXIS_LEFT_Y], 1.0f, 0.01f, &(target->left_stick));
            _sgamepad_generate_analog_stick_state(state.axes[SOKOL_GAMEPAD_AXIS_RIGHT_X], state.axes[SOKOL_GAMEPAD_AXIS_RIGHT_Y], 1.0f, 0.01f, &(target->right_stick));

            target->left_shoulder = state.buttons[SOKOL_GAMEPAD_BUTTON_LEFT_BUMPER] ? 1.0f : 0.0f;
            target->right_shoulder = state.buttons[SOKOL_GAMEPAD_BUTTON_RIGHT_BUMPER] ? 1.0f : 0.0f;
            target->left_trigger = state.axes[SOKOL_GAMEPAD_AXIS_LEFT_TRIGGER];
            target->right_trigger = state.axes[SOKOL_GAMEPAD_AXIS_RIGHT_TRIGGER];
        }
    }
}

#if defined (_SAPP_WIN32)

#define _SOKOL_TYPE_AXIS     0
#define _SOKOL_TYPE_SLIDER   1
#define _SOKOL_TYPE_BUTTON   2
#define _SOKOL_TYPE_POV      3

typedef struct _SOKOL_objenumWin32
{
    IDirectInputDevice8W*   device;
    _SOKOL_joyobjectWin32*    objects;
    int                     objectCount;
    int                     axisCount;
    int                     sliderCount;
    int                     buttonCount;
    int                     povCount;
} _SOKOL_objenumWin32;

_SOKOL_PRIVATE const GUID _sokol_IID_IDirectInput8W =
    {0xbf798031,0x483a,0x4da2,{0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_XAxis =
    {0xa36d02e0,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_YAxis =
    {0xa36d02e1,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_ZAxis =
    {0xa36d02e2,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_RxAxis =
    {0xa36d02f4,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_RyAxis =
    {0xa36d02f5,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_RzAxis =
    {0xa36d02e3,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_Slider =
    {0xa36d02e4,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
_SOKOL_PRIVATE const GUID _sokol_GUID_POV =
    {0xa36d02f2,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};

#define IID_IDirectInput8W _sokol_IID_IDirectInput8W
#define GUID_XAxis _sokol_GUID_XAxis
#define GUID_YAxis _sokol_GUID_YAxis
#define GUID_ZAxis _sokol_GUID_ZAxis
#define GUID_RxAxis _sokol_GUID_RxAxis
#define GUID_RyAxis _sokol_GUID_RyAxis
#define GUID_RzAxis _sokol_GUID_RzAxis
#define GUID_Slider _sokol_GUID_Slider
#define GUID_POV _sokol_GUID_POV

_SOKOL_PRIVATE DIOBJECTDATAFORMAT _sokol_ObjectDataFormats[] =
{
    { &GUID_XAxis,DIJOFS_X,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_YAxis,DIJOFS_Y,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_ZAxis,DIJOFS_Z,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RxAxis,DIJOFS_RX,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RyAxis,DIJOFS_RY,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RzAxis,DIJOFS_RZ,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_POV,DIJOFS_POV(0),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(1),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(2),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(3),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(0),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(1),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(2),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(3),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(4),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(5),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(6),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(7),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(8),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(9),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(10),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(11),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(12),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(13),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(14),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(15),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(16),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(17),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(18),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(19),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(20),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(21),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(22),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(23),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(24),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(25),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(26),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(27),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(28),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(29),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(30),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(31),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
};

_SOKOL_PRIVATE const DIDATAFORMAT _sokol_DataFormat =
{
    sizeof(DIDATAFORMAT),
    sizeof(DIOBJECTDATAFORMAT),
    DIDFT_ABSAXIS,
    sizeof(DIJOYSTATE),
    sizeof(_sokol_ObjectDataFormats) / sizeof(DIOBJECTDATAFORMAT),
    _sokol_ObjectDataFormats
};

_SOKOL_PRIVATE const char* getDeviceDescription(const XINPUT_CAPABILITIES* xic)
{
    switch (xic->SubType)
    {
        case XINPUT_DEVSUBTYPE_WHEEL:
            return "XInput Wheel";
        case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            return "XInput Arcade Stick";
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            return "XInput Flight Stick";
        case XINPUT_DEVSUBTYPE_DANCE_PAD:
            return "XInput Dance Pad";
        case XINPUT_DEVSUBTYPE_GUITAR:
            return "XInput Guitar";
        case XINPUT_DEVSUBTYPE_DRUM_KIT:
            return "XInput Drum Kit";
        case XINPUT_DEVSUBTYPE_GAMEPAD:
        {
            if (xic->Flags & XINPUT_CAPS_WIRELESS)
                return "Wireless Xbox Controller";
            else
                return "Xbox Controller";
        }
    }

    return "Unknown XInput Device";
}

_SOKOL_PRIVATE int compareJoystickObjects(const void* first, const void* second)
{
    const _SOKOL_joyobjectWin32* fo = first;
    const _SOKOL_joyobjectWin32* so = second;

    if (fo->type != so->type)
        return fo->type - so->type;

    return fo->offset - so->offset;
}

_SOKOL_PRIVATE bool supportsXInput(const GUID* guid)
{
    UINT i, count = 0;
    RAWINPUTDEVICELIST* ridl;
    bool result = false;

    if (GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST)) != 0)
        return false;

    ridl = calloc(count, sizeof(RAWINPUTDEVICELIST));

    if (GetRawInputDeviceList(ridl, &count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1)
    {
        free(ridl);
        return false;
    }

    for (i = 0;  i < count;  i++)
    {
        RID_DEVICE_INFO rdi;
        char name[256];
        UINT size;

        if (ridl[i].dwType != RIM_TYPEHID)
            continue;

        ZeroMemory(&rdi, sizeof(rdi));
        rdi.cbSize = sizeof(rdi);
        size = sizeof(rdi);

        if ((INT) GetRawInputDeviceInfoA(ridl[i].hDevice,
                                         RIDI_DEVICEINFO,
                                         &rdi, &size) == -1)
        {
            continue;
        }

        if (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != (LONG) guid->Data1)
            continue;

        memset(name, 0, sizeof(name));
        size = sizeof(name);

        if ((INT) GetRawInputDeviceInfoA(ridl[i].hDevice,
                                         RIDI_DEVICENAME,
                                         name, &size) == -1)
        {
            break;
        }

        name[sizeof(name) - 1] = '\0';
        if (strstr(name, "IG_"))
        {
            result = true;
            break;
        }
    }

    free(ridl);
    return result;
}

_SOKOL_PRIVATE void closeJoystick(_SOKOL_joystick* js)
{
    _sokol_InputJoystick(js, SOKOL_DISCONNECTED);

    if (js->js.device)
    {
        IDirectInputDevice8_Unacquire(js->js.device);
        IDirectInputDevice8_Release(js->js.device);
    }

    free(js->js.objects);
    _sokol_FreeJoystick(js);
}

_SOKOL_PRIVATE BOOL CALLBACK deviceObjectCallback(const DIDEVICEOBJECTINSTANCEW* doi,
                                          void* user)
{
    _SOKOL_objenumWin32* data = user;
    _SOKOL_joyobjectWin32* object = data->objects + data->objectCount;

    if (DIDFT_GETTYPE(doi->dwType) & DIDFT_AXIS)
    {
        DIPROPRANGE dipr;

        if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
            object->offset = DIJOFS_SLIDER(data->sliderCount);
        else if (memcmp(&doi->guidType, &GUID_XAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_X;
        else if (memcmp(&doi->guidType, &GUID_YAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_Y;
        else if (memcmp(&doi->guidType, &GUID_ZAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_Z;
        else if (memcmp(&doi->guidType, &GUID_RxAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RX;
        else if (memcmp(&doi->guidType, &GUID_RyAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RY;
        else if (memcmp(&doi->guidType, &GUID_RzAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RZ;
        else
            return DIENUM_CONTINUE;

        ZeroMemory(&dipr, sizeof(dipr));
        dipr.diph.dwSize = sizeof(dipr);
        dipr.diph.dwHeaderSize = sizeof(dipr.diph);
        dipr.diph.dwObj = doi->dwType;
        dipr.diph.dwHow = DIPH_BYID;
        dipr.lMin = -32768;
        dipr.lMax =  32767;

        if (FAILED(IDirectInputDevice8_SetProperty(data->device,
                                                   DIPROP_RANGE,
                                                   &dipr.diph)))
        {
            return DIENUM_CONTINUE;
        }

        if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
        {
            object->type = _SOKOL_TYPE_SLIDER;
            data->sliderCount++;
        }
        else
        {
            object->type = _SOKOL_TYPE_AXIS;
            data->axisCount++;
        }
    }
    else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_BUTTON)
    {
        object->offset = DIJOFS_BUTTON(data->buttonCount);
        object->type = _SOKOL_TYPE_BUTTON;
        data->buttonCount++;
    }
    else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_POV)
    {
        object->offset = DIJOFS_POV(data->povCount);
        object->type = _SOKOL_TYPE_POV;
        data->povCount++;
    }

    data->objectCount++;
    return DIENUM_CONTINUE;
}

_SOKOL_PRIVATE BOOL CALLBACK deviceCallback(const DIDEVICEINSTANCE* di, void* user)
{
    int jid = 0;
    DIDEVCAPS dc;
    DIPROPDWORD dipd;
    IDirectInputDevice8* device;
    _SOKOL_objenumWin32 data;
    _SOKOL_joystick* js;
    char guid[33];
    char name[256];

    for (jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        js = _sokol_.joysticks + jid;
        if (js->connected)
        {
            if (memcmp(&js->js.guid, &di->guidInstance, sizeof(GUID)) == 0)
                return DIENUM_CONTINUE;
        }
    }

    if (supportsXInput(&di->guidProduct))
        return DIENUM_CONTINUE;

    if (FAILED(IDirectInput8_CreateDevice(_sokol_.js.dinput8.api,
                                          &di->guidInstance,
                                          &device,
                                          NULL)))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR, "Win32: Failed to create device");
        return DIENUM_CONTINUE;
    }

    if (FAILED(IDirectInputDevice8_SetDataFormat(device, &_sokol_DataFormat)))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Win32: Failed to set device data format");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    ZeroMemory(&dc, sizeof(dc));
    dc.dwSize = sizeof(dc);

    if (FAILED(IDirectInputDevice8_GetCapabilities(device, &dc)))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Win32: Failed to query device capabilities");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    ZeroMemory(&dipd, sizeof(dipd));
    dipd.diph.dwSize = sizeof(dipd);
    dipd.diph.dwHeaderSize = sizeof(dipd.diph);
    dipd.diph.dwHow = DIPH_DEVICE;
    dipd.dwData = DIPROPAXISMODE_ABS;

    if (FAILED(IDirectInputDevice8_SetProperty(device,
                                               DIPROP_AXISMODE,
                                               &dipd.diph)))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Win32: Failed to set device axis mode");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    memset(&data, 0, sizeof(data));
    data.device = device;
    data.objects = calloc(dc.dwAxes + (size_t) dc.dwButtons + dc.dwPOVs,
                                sizeof(_SOKOL_joyobjectWin32));

    if (FAILED(IDirectInputDevice8_EnumObjects(device,
                                               deviceObjectCallback,
                                               &data,
                                               DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV)))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Win32: Failed to enumerate device objects");

        IDirectInputDevice8_Release(device);
        free(data.objects);
        return DIENUM_CONTINUE;
    }

    qsort(data.objects, data.objectCount,
          sizeof(_SOKOL_joyobjectWin32),
          compareJoystickObjects);

    if (!WideCharToMultiByte(CP_UTF8, 0,
                             di->tszInstanceName, -1,
                             name, sizeof(name),
                             NULL, NULL))
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Win32: Failed to convert joystick name to UTF-8");

        IDirectInputDevice8_Release(device);
        free(data.objects);
        return DIENUM_STOP;
    }

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (memcmp(&di->guidProduct.Data4[2], "PIDVID", 6) == 0)
    {
        sprintf(guid, "03000000%02x%02x0000%02x%02x000000000000",
                (uint8_t) di->guidProduct.Data1,
                (uint8_t) (di->guidProduct.Data1 >> 8),
                (uint8_t) (di->guidProduct.Data1 >> 16),
                (uint8_t) (di->guidProduct.Data1 >> 24));
    }
    else
    {
        sprintf(guid, "05000000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    js = _sokol_AllocJoystick(name, guid,
                            data.axisCount + data.sliderCount,
                            data.buttonCount,
                            data.povCount);
    if (!js)
    {
        IDirectInputDevice8_Release(device);
        free(data.objects);
        return DIENUM_STOP;
    }

    js->js.device = device;
    js->js.guid = di->guidInstance;
    js->js.objects = data.objects;
    js->js.objectCount = data.objectCount;

    _sokol_InputJoystick(js, SOKOL_CONNECTED);
    return DIENUM_CONTINUE;
}

_SOKOL_PRIVATE void _sokol_DetectJoystickConnectionWin32(void)
{
    if (_sokol_.js.xinput.instance)
    {
        DWORD index;

        for (index = 0;  index < XUSER_MAX_COUNT;  index++)
        {
            int jid;
            char guid[33];
            XINPUT_CAPABILITIES xic;
            _SOKOL_joystick* js;

            for (jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
            {
                if (_sokol_.joysticks[jid].connected &&
                    _sokol_.joysticks[jid].js.device == NULL &&
                    _sokol_.joysticks[jid].js.index == index)
                {
                    break;
                }
            }

            if (jid <= SOKOL_JOYSTICK_LAST)
                continue;

            if (XInputGetCapabilities(index, 0, &xic) != ERROR_SUCCESS)
                continue;

            // Generate a joystick GUID that matches the SDL 2.0.5+ one
            sprintf(guid, "78696e707574%02x000000000000000000",
                    xic.SubType & 0xff);

            js = _sokol_AllocJoystick(getDeviceDescription(&xic), guid, 6, 10, 1);
            if (!js)
                continue;

            js->js.index = index;

            _sokol_InputJoystick(js, SOKOL_CONNECTED);
        }
    }

    if (_sokol_.js.dinput8.api)
    {
        if (FAILED(IDirectInput8_EnumDevices(_sokol_.js.dinput8.api,
                                             DI8DEVCLASS_GAMECTRL,
                                             deviceCallback,
                                             NULL,
                                             DIEDFL_ALLDEVICES)))
        {
            _sokol_InputError(SOKOL_PLATFORM_ERROR,
                            "Failed to enumerate DirectInput8 devices");
            return;
        }
    }
}

_SOKOL_PRIVATE bool _sokol_InitJoysticks(void)
{
    static const _SOKOL_mapping mapping[] = 
    {
        {"3DRUDDER", "03000000fa2d00000100000000000000", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8bitdo", "03000000c82d00002038000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Dogbone Modkit", "03000000c82d00000951000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo F30", "03000000c82d000011ab000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo F30 Pro", "03000000c82d00001038000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo FC30 Pro", "03000000c82d00000090000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo M30", "03000000c82d00000650000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 1, 4, 1, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 1, 0, 2, 7, 0, 0, },
        {"8BitDo M30 Gamepad", "03000000c82d00005106000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo M30 ModKit", "03000000c82d00000151000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, },
        {"8BitDo N30", "03000000c82d00000310000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo N30", "03000000c82d00002028000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo N30", "03000000c82d00008010000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo N30 Modkit", "03000000c82d00000451000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo N30 Pro", "03000000c82d00000190000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo N30 Pro 2", "03000000c82d00001590000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo N30 Pro 2", "03000000c82d00006528000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000022000000090000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000203800000900000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Pro 2", "03000000c82d00000360000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo S30 Modkit", "03000000c82d00002867000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, },
        {"8BitDo SF30", "03000000c82d00000130000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo SF30 Pro", "03000000c82d00000060000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo SF30 Pro", "03000000c82d00000061000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SFC30", "03000000c82d000021ab000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SFC30 GamePad", "03000000102800000900000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SFC30 GamePad", "03000000c82d00003028000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30", "03000000c82d00000030000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30", "03000000c82d00001290000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30", "03000000c82d000020ab000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30", "03000000c82d00004028000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30", "03000000c82d00006228000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30 Modkit", "03000000c82d00000351000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000160000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000161000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro for Android", "03000000c82d00000121000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "03000000c82d00000260000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "03000000c82d00000261000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Wireless Adapter", "03000000c82d00000031000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Zero 2", "03000000c82d00001890000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo Zero 2", "03000000c82d00003032000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo Zero GamePad", "03000000a00500003232000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Astro City Mini", "03000000a30c00002700000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Astro City Mini", "03000000a30c00002800000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Acme GA-02", "030000008f0e00001200000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"ACRUX USB GAME PAD", "03000000c01100000355000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Acteck AGJ-3200", "03000000fa190000f0ff000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow", "030000006f0e00001413000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "03000000341a00003608000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00000263000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00001101000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00001401000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00001402000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00001901000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow PS3 Controller", "030000006f0e00001a01000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Airflo PS3 Controller", "03000000d62000001d57000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Amazon Luna Controller", "03000000491900001904000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Amazon Luna Controller", "03000000710100001904000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 11, 0, 0, 2, 8, 0, 0, 2, 7, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"AxisPad", "03000000ef0500000300000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Batarang", "03000000d6200000e557000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Battalife Joystick", "03000000c01100001352000000000000", 2, 6, 0, 0, 2, 7, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Battlefield 4 PS3 Controller", "030000006f0e00003201000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"BDA PS4 Fightpad", "03000000d62000002a79000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Betop 2126F", "03000000bc2000006012000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop BFM Gamepad", "03000000bc2000000055000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Betop Controller", "03000000bc2000006312000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"BETOP CONTROLLER", "03000000bc2000006321000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop Controller", "03000000bc2000006412000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop Controller", "03000000c01100000555000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop Controller", "03000000c01100000655000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop Gamepad", "03000000790000000700000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Betop Gamepad", "03000000808300000300000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Bigben PS3 Controller", "030000006b1400000055000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Bigben PS3 Controller", "030000006b1400000103000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Brook Mars", "03000000120c0000210e000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"BrutalLegendTest", "0300000066f700000500000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"BUFFALO BSGP1601 Series ", "03000000d81d00000b00000000000000", 2, 5, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 2, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Cideko AK08b", "03000000e82000006058000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Cobra", "03000000457500000401000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Controller (XBOX 360 For Windows)", "030000005e0400008e02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 2, -1, 1, 2, 2, 1, },
        {"Controller (Xbox 360 Wireless Receiver for Windows)", "030000005e040000a102000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 2, -1, 1, 2, 2, 1, },
        {"Controller (Xbox One For Windows) - Wired", "030000005e040000ff02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 2, -1, 1, 2, 2, 1, },
        {"Controller (Xbox One For Windows) - Wireless", "030000005e040000ea02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 2, -1, 1, 2, 2, 1, },
        {"Cyber Gadget GameCube Controller", "03000000260900008888000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Cyborg V.3 Rumble Pad", "03000000a306000022f6000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 1, 3, 2, -1, 1, 3, 2, 1, },
        {"Defender Game Racer X7", "03000000451300000830000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Dual Box WIIltpad", "03000000791d00000103000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Dual USB Vibration Joystick", "03000000bd12000002e0000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"DualShock 2", "030000008f0e00000910000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"EA SPORTS PS3 Controller", "030000006f0e00003001000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Elecom Gamepad", "03000000b80500000410000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Elecom Gamepad", "03000000b80500000610000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Elite", "03000000120c0000f61c000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"EXEQ", "030000008f0e00000f31000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"EXEQ RF USB Gamepad 8206", "03000000341a00000108000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 7, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Faceoff Deluxe+ Audio Wired Controller for Nintendo Switch", "030000006f0e00008401000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Faceoff Wired Pro Controller for Nintendo Switch", "030000006f0e00008001000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"FF-GP1", "03000000852100000201000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Fighting Commander 2016 PS3", "030000000d0f00008500000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Fighting Commander 5", "030000000d0f00008400000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Fighting Stick mini 4", "030000000d0f00008700000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Fighting Stick mini 4", "030000000d0f00008800000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"FIGHTING STICK V3", "030000000d0f00002700000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Fightstick TES", "78696e70757403000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Game Controller for PC", "03000000790000002201000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Game VIB Joystick", "0300000066f700000100000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gamecube Controller", "03000000260900002625000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"GameCube Controller Adapter", "03000000790000004618000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"GAMEPAD 3 TURBO", "030000008f0e00000d31000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GamePad Pro USB", "03000000280400000140000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GameSir", "03000000ac0500003d03000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"GameSir", "03000000ac0500004d04000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"GameStop Gamepad", "03000000ffff00000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GameStop PS4 Fun Controller", "03000000c01100000140000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"GC/N64 to USB v3.4", "030000009b2800003200000000000000", 2, 0, 0, 0, 2, 7, 0, 0, 2, 1, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 2, -1, 1, 2, 2, -1, },
        {"GC/N64 to USB v3.6", "030000009b2800006000000000000000", 2, 0, 0, 0, 2, 7, 0, 0, 2, 1, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 2, -1, 1, 2, 2, -1, },
        {"Genius", "030000008305000009a0000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Genius Maxfire Blaze 3", "030000008305000031b0000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Genius Maxfire Grandias 12", "03000000451300000010000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Genius MaxFire Grandias 12V", "030000005c1a00003330000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 7, 0, 0, 2, 5, 0, 0, },
        {"GGE909 Recoil Pad", "03000000300f00000b01000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gioteck", "03000000f0250000c283000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gioteck PS3 Controller", "03000000f025000021c1000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gioteck VX2 Controller", "03000000f0250000c383000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gioteck VX2 Controller", "03000000f0250000c483000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gravis Eliminator GamePad Pro", "030000007d0400000540000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hama Scorpad", "03000000341a00000302000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hatsune Miku Sho Controller", "030000000d0f00004900000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Havit HV-G60", "030000001008000001e1000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HitBox Edition Cthulhu+", "03000000d81400000862000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, },
        {"HJD-X", "03000000632500002605000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Hori Fighting Commander 3 Pro", "030000000d0f00002d00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS3)", "030000000d0f00005f00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS4)", "030000000d0f00005e00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Hori Fighting Stick Mini 3", "030000000d0f00004000000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, },
        {"Hori Pad 3", "030000000d0f00005400000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Pad 3 Turbo", "030000000d0f00000900000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Pad A", "030000000d0f00004d00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Pokken Tournament DX Pro Pad", "030000000d0f00009200000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Real Arcade Pro EX-SE (Xbox 360)", "030000000d0f00001600000000007803", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Hori TAC Pro", "030000000d0f00009c00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Horipad", "030000000d0f0000c100000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS3)", "030000000d0f00006e00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS4)", "030000000d0f00006600000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Horipad 4 FPS", "030000000d0f00005500000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"HORIPAD mini4", "030000000d0f0000ee00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HRAP2 on PS/SS/N64 Joypad to USB BOX", "03000000250900000017000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, },
        {"HuiJia SNES Controller", "030000008f0e00001330000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"iBUFFALO BSGP1204 Series", "03000000d81d00000f00000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"iBUFFALO BSGP1204P Series", "03000000d81d00001000000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"iBuffalo SNES Controller", "03000000830500006020000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Impact Black", "03000000b50700001403000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"INJUSTICE FightStick PS3 Controller", "030000006f0e00002401000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"IPEGA", "03000000ac0500002c02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Ipega PG-9023", "03000000491900000204000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"JC-DUX60 ELECOM MMO Gamepadamepad", "030000006e0500000a20000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 17, 0, 0, 2, 20, 0, 0, 0, 0, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 12, 0, 0, 2, 13, 0, 0, },
        {"JC-P301U", "030000006e0500000520000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JC-U3613M (DInput)", "030000006e0500000320000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JC-W01U", "030000006e0500000720000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Joypad Alpha Shock", "03000000bd12000003c0000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JY-P70UR", "03000000bd12000003c0000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 7, 0, 0, 2, 9, 0, 0, },
        {"JYS Wireless Adapter", "03000000242f00002d00000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JYS Wireless Adapter", "03000000242f00008a00000000000000", 2, 1, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"King PS3 Controller", "03000000790000000200000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech ChillStream", "030000006d040000d1ca000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Cordless Precision", "030000006d040000d2ca000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Cordless Wingman", "030000006d04000011c2000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F510 Gamepad", "030000006d04000018c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F710 Gamepad", "030000006d04000019c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Precision Gamepad", "030000006d0400001ac2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech WingMan RumblePad", "030000006d0400000ac2000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 7, 0, 0, 2, 2, 0, 0, },
        {"Mad Catz C.T.R.L.R", "03000000380700006652000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS3)", "03000000380700005032000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS4)", "03000000380700005082000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz FightStick TE S+ (PS3)", "03000000380700008433000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightStick TE S+ (PS4)", "03000000380700008483000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz FightStick TE2+ PS3", "03000000380700008134000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 4, 0, 0, },
        {"Mad Catz FightStick TE2+ PS4", "03000000380700008184000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, 2, 7, 0, 0, },
        {"Mad Catz Micro C.T.R.L.R", "03000000380700006252000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz TE2 PS3 Fightstick", "03000000380700008034000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz TE2 PS4 Fightstick", "03000000380700008084000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Madcatz Arcade Fightstick TE S PS3", "03000000380700008532000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Madcatz Arcade Fightstick TE S+ PS3", "03000000380700003888000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"MadCatz SFIV FightStick PS3", "03000000380700001888000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 7, 0, 0, 2, 6, 0, 0, },
        {"MADCATZ SFV Arcade FightStick Alpha PS4", "03000000380700008081000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Matricom", "030000002a0600001024000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"MaxJoypad Virtual Controller", "030000009f000000adbb000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Arcade Stick", "03000000250900000128000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, },
        {"Mayflash GameCube Controller", "03000000790000004418000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mayflash GameCube Controller Adapter", "03000000790000004318000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, 2, 0, 0, 0, 2, 9, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mayflash Magic NS", "03000000242f00007300000000000000", 2, 1, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Mayflash Magic NS", "0300000079000000d218000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Magic NS", "03000000d620000010a7000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash USB Adapter for original Sega Saturn controller", "030000008f0e00001030000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Wii Classic Controller", "0300000025090000e803000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash WiiU Pro Game Controller Adapter (DInput)", "03000000790000000018000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mega Drive", "03000000790000002418000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"MLG GamePad PS3 Controller", "03000000380700006382000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"MOGA XP5-A Plus", "03000000c62400002a89000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 15, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"MOGA XP5-A Plus", "03000000c62400002b89000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"MOGA XP5-X Plus", "03000000c62400001a89000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"MOGA XP5-X Plus", "03000000c62400001b89000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Monect Virtual Controller", "03000000efbe0000edfe000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"MP-8866 Super Dual Box", "03000000250900006688000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"NACON GC-400ES", "030000006b140000010c000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"NES 2-port Adapter", "03000000921200004b46000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"NEXILUX GAMECUBE Controller Adapter", "03000000790000004518000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"NEXT SNES Controller", "030000001008000001e5000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, },
        {"NGDS", "03000000152000000182000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Retrolink USB Super SNES Classic Controller", "03000000bd12000015d0000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Nintendo Switch Pro Controller", "030000007e0500000920000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nostromo N45", "030000000d0500000308000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"NVIDIA Controller v01.04", "03000000550900001472000000000000", 2, 11, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, 2, 13, 0, 0, 2, 3, 0, 0, 2, 12, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 6, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"NYKO AIRFLO", "030000004b120000014d000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 1, 0, 1, 0, 1, 2, 1, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"NSW wired controller", "03000000d620000013a7000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Onlive Wireless Controller", "03000000782300000a10000000000000", 2, 15, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 11, 0, 0, 2, 10, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"OPP PS3 Controller", "03000000d62000006d57000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Orange Controller", "030000006b14000001a1000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"OUYA Game Controller", "03000000362800000100000000000000", 2, 0, 0, 0, 2, 3, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 2, 13, 0, 0, },
        {"P4 Wired Gamepad", "03000000120c0000f60e000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, },
        {"PDP Versus Fighting Pad", "030000006f0e00000901000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Piranha xtreme", "030000008f0e00000300000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PlayStation Classic Controller", "030000004c050000da0c000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PlayStation Vita", "030000004c0500003713000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"PowerA Pro Ex", "03000000d62000006dca000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Pro Elite PS3 Controller", "03000000d62000009557000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Pro Ex mini PS3 Controller", "03000000d62000009f31000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Pro Ex mini PS3 Controller", "03000000d6200000c757000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS Controller", "03000000632500002306000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS to USB convert cable", "03000000e30500009605000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS1 Controller", "03000000100800000100000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS1 Controller", "030000008f0e00007530000000000000", 0, 0, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 1, 0, 0, },
        {"PS2 Controller", "03000000100800000300000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS2 Controller", "03000000250900008888000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS2 Controller", "03000000666600006706000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS2 Controller", "030000006b1400000303000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS2 Controller", "030000009d0d00001330000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "03000000250900000500000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 8, 0, 0, 3, 4, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, -1, 0, 1, 4, -1, 0, },
        {"PS3 Controller", "03000000632500007505000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "03000000888800000803000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 8, 0, 0, 3, 4, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "030000008f0e00001431000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 RF pad", "030000003807000056a8000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS360+ v1.66", "03000000100000008200000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS4 Controller", "030000004c050000a00b000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000c405000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000cc09000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS5 Controller", "030000004c050000e60c000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PSP", "03000000ff000000cb01000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"QanBa Arcade JoyStick 1008", "03000000300f00000011000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"QanBa Arcade JoyStick 4018", "03000000300f00001611000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"QANBA DRONE ARCADE JOYSTICK", "03000000222c00000020000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"QanBa Joystick Plus", "03000000300f00001210000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"QanBa Joystick Q4RAF", "03000000341a00000104000000000000", 2, 5, 0, 0, 2, 6, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, },
        {"Qanba Obsidian Arcade Joystick PS3 Mode", "03000000222c00000223000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Qanba Obsidian Arcade Joystick PS4 Mode", "03000000222c00000023000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 13, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Hydra", "03000000321500000003000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 1, 2, 1, 0, },
        {"Razer Panthera (PS3)", "03000000321500000204000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Razer Panthera (PS4)", "03000000321500000104000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Raiju Mobile", "03000000321500000507000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Razer Raiju Mobile", "03000000321500000707000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Razer Raion Fightpad for PS4", "03000000321500000011000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"REAL ARCADE PRO.3", "030000000d0f00001100000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Real Arcade Pro.4", "030000000d0f00006a00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Real Arcade Pro.4", "030000000d0f00006b00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Real Arcade Pro.4", "030000000d0f00008a00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Real Arcade Pro.4", "030000000d0f00008b00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"REAL ARCADE PRO.4 VLX", "030000000d0f00007000000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"REAL ARCADE Pro.V3", "030000000d0f00002200000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Real Arcade Pro.V4", "030000000d0f00005b00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Real Arcade Pro.V4", "030000000d0f00005c00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Retrolink SNES Controller", "03000000790000001100000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 2, 1, 1, 3, 2, -1, 1, 4, 2, -1, 1, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Retrolink USB SEGA Saturn Classic", "03000000bd12000013d0000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"RetroUSB.com RetroPad", "0300000000f000000300000000000000", 2, 1, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"RetroUSB.com Super RetroPort", "0300000000f00000f100000000000000", 2, 1, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Revolution Pro Controller", "030000006b140000010d000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Revolution Pro Controller 2(1/2)", "030000006b140000020d000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Revolution Pro Controller 3", "030000006b140000130d000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Rock Candy PS3 Controller", "030000006f0e00001e01000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Rock Candy PS3 Controller", "030000006f0e00002801000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Rock Candy PS3 Controller", "030000006f0e00002f01000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"run'n'drive", "030000004f04000003d0000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Saitek Cyborg", "03000000a30600001af5000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek Cyborg V.1 Game pad", "03000000a306000023f6000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek Dual Analog Pad", "03000000300f00001201000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Saitek P220", "03000000a30600000701000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 6, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, },
        {"Saitek P2500 Force Rumble Pad", "03000000a30600000cff000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 11, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P2900", "03000000a30600000c04000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P480 Rumble Pad", "03000000300f00001001000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Saitek P990", "03000000a30600000b04000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P990 Dual Analog Pad", "03000000a30600000b04000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek PS1000", "03000000a30600002106000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek PS2700", "03000000a306000020f6000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek Rumble Pad", "03000000300f00001101000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Sanwa PlayOnline Mobile", "03000000730700000401000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Saturn_Adapter_2.0", "0300000000050000289b000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Saturn_Adapter_2.0", "030000009b2800000500000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Sega Genesis Mini 3B controller", "03000000a30c00002500000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 2, 1, 1, 3, 2, -1, 1, 4, 2, -1, 1, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Sega Mega Drive Mini 6B controller", "03000000a30c00002400000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 2, 1, 1, 3, 2, -1, 1, 4, 2, -1, 1, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"SL-6555-SBK", "03000000341a00000208000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 1, 4, 2, 1, 1, 4, 1, 0, },
        {"SL-6566", "03000000341a00000908000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SpeedLink Strike FX", "030000008f0e00000800000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Speedlink Torid", "03000000c01100000591000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Stadia Controller", "03000000d11800000094000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 12, 0, 0, 2, 11, 0, 0, },
        {"SteelSeries", "03000000110100001914000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Free", "03000000381000001214000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 12, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"SteelSeries Stratus Duo", "03000000110100003114000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"SteelSeries Stratus XL", "03000000381000001814000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 18, 0, 0, 2, 9, 0, 0, 2, 19, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 15, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"STK-7024X", "03000000790000001c18000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"SVEN X-PAD", "03000000ff1100003133000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Switch", "03000000d620000011a7000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SZMY-POWER PC Gamepad", "03000000457500002211000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"T Mini Wireless", "030000004f04000007d0000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"T.16000M", "030000004f0400000ab1000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, },
        {"Team 5", "03000000fa1900000706000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Techmobility X6-38V", "03000000b50700001203000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Dual Analog 4", "030000004f04000015b3000000000000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Dual Trigger 3-in-1", "030000004f04000023b3000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"ThrustMaster eSwap PRO Controller", "030000004f0400000ed0000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Thrustmaster Firestorm Dual Power", "030000004f04000000b3000000000000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Firestorm Dual Power 3", "030000004f04000004b3000000000000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"TigerGame PS/PS2 Game Controller Adapter", "03000000666600000488000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Tournament PS3 Controller", "03000000d62000006000000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Trust Gamepad", "030000005f140000c501000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Trust Gamepad", "03000000b80500000210000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"TWCS Throttle", "030000004f04000087b6000000000000", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 2, 1, 1, 5, 2, -1, },
        {"TwinShock PS2", "03000000d90400000200000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"U4113", "030000006e0500001320000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"uRage Gamepad", "03000000101c0000171c000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB 4-Axis 12-Button Gamepad", "03000000300f00000701000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB gamepad", "03000000341a00002308000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB gamepad", "030000005509000000b4000000000000", 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 5, 0, 0, 2, 4, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"USB gamepad", "030000006b1400000203000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB gamepad", "03000000790000000a00000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB gamepad", "03000000f0250000c183000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB gamepad", "03000000ff1100004133000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"USB Vibration Joystick (BM)", "03000000632500002305000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Venom", "03000000790000001a18000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Venom Arcade Joystick", "03000000790000001b18000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000302000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000702000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"vJoy Device", "0300000034120000adbe000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 15, 0, 0, 2, 4, 0, 0, 2, 16, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 11, 0, 0, 2, 12, 0, 0, },
        {"Xbox Adaptive Controller", "030000005e0400000a0b000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 2, -1, 1, 2, 2, 1, },
        {"Xbox Series Controller", "030000005e040000130b000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xeox", "03000000341a00000608000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"XEOX Gamepad SL-6556-BK", "03000000450c00002043000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Xiaoji Gamesir-G3w", "03000000ac0500005b05000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"XiaoMi Game Controller", "03000000172700004431000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 20, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 1, 7, 1, 0, },
        {"XInput Controller", "03000000786901006e70000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"ZD-T Android", "03000000790000004f18000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"ZEROPLUS P4 Wired Gamepad", "03000000120c0000101e000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"XInput Gamepad", "78696e70757401000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Wheel", "78696e70757402000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Arcade Stick", "78696e70757403000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Flight Stick", "78696e70757404000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Dance Pad", "78696e70757405000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Guitar", "78696e70757406000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"XInput Drum Kit", "78696e70757408000000000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },

    };

    _sokol_.mappings = mapping;
    _sokol_.mappingCount = _countof(mapping);

    if (_sokol_.js.dinput8.instance)
    {
        if (FAILED(DirectInput8Create(_sokol_.js.instance,
                                      DIRECTINPUT_VERSION,
                                      &IID_IDirectInput8W,
                                      (void**) &_sokol_.js.dinput8.api,
                                      NULL)))
        {
            _sokol_InputError(SOKOL_PLATFORM_ERROR,
                            "Win32: Failed to create interface");
            return false;
        }
    }

    _sokol_DetectJoystickConnectionWin32();
    return true;
}

_SOKOL_PRIVATE void _sokol_TerminateJoysticks(void)
{
    int jid;

    for (jid = SOKOL_JOYSTICK_1;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
        closeJoystick(_sokol_.joysticks + jid);

    if (_sokol_.js.dinput8.api)
        IDirectInput8_Release(_sokol_.js.dinput8.api);
}

_SOKOL_PRIVATE bool _sokol_PollJoystick(_SOKOL_joystick* js, int mode)
{
    if (js->js.device)
    {
        int i, ai = 0, bi = 0, pi = 0;
        HRESULT result;
        DIJOYSTATE state = {0};

        IDirectInputDevice8_Poll(js->js.device);
        result = IDirectInputDevice8_GetDeviceState(js->js.device,
                                                    sizeof(state),
                                                    &state);
        if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST)
        {
            IDirectInputDevice8_Acquire(js->js.device);
            IDirectInputDevice8_Poll(js->js.device);
            result = IDirectInputDevice8_GetDeviceState(js->js.device,
                                                        sizeof(state),
                                                        &state);
        }

        if (FAILED(result))
        {
            closeJoystick(js);
            return false;
        }

        if (mode == _SOKOL_POLL_PRESENCE)
            return true;

        for (i = 0;  i < js->js.objectCount;  i++)
        {
            const void* data = (char*) &state + js->js.objects[i].offset;

            switch (js->js.objects[i].type)
            {
                case _SOKOL_TYPE_AXIS:
                case _SOKOL_TYPE_SLIDER:
                {
                    const float value = (*((LONG*) data) + 0.5f) / 32767.5f;
                    _sokol_InputJoystickAxis(js, ai, value);
                    ai++;
                    break;
                }

                case _SOKOL_TYPE_BUTTON:
                {
                    const char value = (*((BYTE*) data) & 0x80) != 0;
                    _sokol_InputJoystickButton(js, bi, value);
                    bi++;
                    break;
                }

                case _SOKOL_TYPE_POV:
                {
                    const int states[9] =
                    {
                        SOKOL_HAT_UP,
                        SOKOL_HAT_RIGHT_UP,
                        SOKOL_HAT_RIGHT,
                        SOKOL_HAT_RIGHT_DOWN,
                        SOKOL_HAT_DOWN,
                        SOKOL_HAT_LEFT_DOWN,
                        SOKOL_HAT_LEFT,
                        SOKOL_HAT_LEFT_UP,
                        SOKOL_HAT_CENTERED
                    };

                    // Screams of horror are appropriate at this point
                    int stateIndex = LOWORD(*(DWORD*) data) / (45 * DI_DEGREES);
                    if (stateIndex < 0 || stateIndex > 8)
                        stateIndex = 8;

                    _sokol_InputJoystickHat(js, pi, states[stateIndex]);
                    pi++;
                    break;
                }
            }
        }
    }
    else
    {
        int i, dpad = 0;
        DWORD result;
        XINPUT_STATE xis;
        const WORD buttons[10] =
        {
            XINPUT_GAMEPAD_A,
            XINPUT_GAMEPAD_B,
            XINPUT_GAMEPAD_X,
            XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER,
            XINPUT_GAMEPAD_RIGHT_SHOULDER,
            XINPUT_GAMEPAD_BACK,
            XINPUT_GAMEPAD_START,
            XINPUT_GAMEPAD_LEFT_THUMB,
            XINPUT_GAMEPAD_RIGHT_THUMB
        };

        result = XInputGetState(js->js.index, &xis);
        if (result != ERROR_SUCCESS)
        {
            if (result == ERROR_DEVICE_NOT_CONNECTED)
                closeJoystick(js);

            return false;
        }

        if (mode == _SOKOL_POLL_PRESENCE)
            return true;

        _sokol_InputJoystickAxis(js, 0, (xis.Gamepad.sThumbLX + 0.5f) / 32767.5f);
        _sokol_InputJoystickAxis(js, 1, -(xis.Gamepad.sThumbLY + 0.5f) / 32767.5f);
        _sokol_InputJoystickAxis(js, 2, (xis.Gamepad.sThumbRX + 0.5f) / 32767.5f);
        _sokol_InputJoystickAxis(js, 3, -(xis.Gamepad.sThumbRY + 0.5f) / 32767.5f);
        _sokol_InputJoystickAxis(js, 4, xis.Gamepad.bLeftTrigger / 127.5f - 1.f);
        _sokol_InputJoystickAxis(js, 5, xis.Gamepad.bRightTrigger / 127.5f - 1.f);

        for (i = 0;  i < 10;  i++)
        {
            const char value = (xis.Gamepad.wButtons & buttons[i]) ? 1 : 0;
            _sokol_InputJoystickButton(js, i, value);
        }

        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
            dpad |= SOKOL_HAT_UP;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
            dpad |= SOKOL_HAT_RIGHT;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
            dpad |= SOKOL_HAT_DOWN;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
            dpad |= SOKOL_HAT_LEFT;

        // Treat invalid combinations as neither being pressed
        // while preserving what data can be preserved
        if ((dpad & SOKOL_HAT_RIGHT) && (dpad & SOKOL_HAT_LEFT))
            dpad &= ~(SOKOL_HAT_RIGHT | SOKOL_HAT_LEFT);
        if ((dpad & SOKOL_HAT_UP) && (dpad & SOKOL_HAT_DOWN))
            dpad &= ~(SOKOL_HAT_UP | SOKOL_HAT_DOWN);

        _sokol_InputJoystickHat(js, 0, dpad);
    }

    return true;
}

SOKOL_API_IMPL void sgamepad_init() {

    _sokol_.js.instance = GetModuleHandle(NULL);

    _sokol_.js.dinput8.instance = LoadLibraryA("dinput8.dll");
    if (_sokol_.js.dinput8.instance)
    {
        _sokol_.js.dinput8.Create = (PFN_DirectInput8Create)
            GetProcAddress(_sokol_.js.dinput8.instance, "DirectInput8Create");
    }    

    {
        const char* names[] =
        {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll",
            "xinput1_2.dll",
            "xinput1_1.dll",
        };

        for (int i = 0; _countof(names); i++)
        {
            _sokol_.js.xinput.instance = LoadLibraryA(names[i]);
            if (_sokol_.js.xinput.instance)
            {
                _sokol_.js.xinput.GetCapabilities = (PFN_XInputGetCapabilities)
                    GetProcAddress(_sokol_.js.xinput.instance, "XInputGetCapabilities");
                _sokol_.js.xinput.GetState = (PFN_XInputGetState)
                    GetProcAddress(_sokol_.js.xinput.instance, "XInputGetState");

                break;
            }
        }
    }
}

SOKOL_API_IMPL void sgamepad_shutdown() {

    _sokol_TerminateJoysticks();
}

#elif defined(_SAPP_APPLE)

typedef struct _SOKOL_joyelementNS
{
    IOHIDElementRef native;
    uint32_t        usage;
    int             index;
    long            minimum;
    long            maximum;

} _SOKOL_joyelementNS;

_SOKOL_PRIVATE long getElementValue(_SOKOL_joystick* js, _SOKOL_joyelementNS* element)
{
    IOHIDValueRef valueRef;
    long value = 0;

    if (js->js.device)
    {
        if (IOHIDDeviceGetValue(js->js.device,
                                element->native,
                                &valueRef) == kIOReturnSuccess)
        {
            value = IOHIDValueGetIntegerValue(valueRef);
        }
    }

    return value;
}

_SOKOL_PRIVATE CFComparisonResult compareElements(const void* fp,
                                          const void* sp,
                                          void* user)
{
    const _SOKOL_joyelementNS* fe = fp;
    const _SOKOL_joyelementNS* se = sp;
    if (fe->usage < se->usage)
        return kCFCompareLessThan;
    if (fe->usage > se->usage)
        return kCFCompareGreaterThan;
    if (fe->index < se->index)
        return kCFCompareLessThan;
    if (fe->index > se->index)
        return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

_SOKOL_PRIVATE void closeJoystick(_SOKOL_joystick* js)
{
    _sokol_InputJoystick(js, SOKOL_DISCONNECTED);

    for (int i = 0;  i < CFArrayGetCount(js->js.axes);  i++)
        free((void*) CFArrayGetValueAtIndex(js->js.axes, i));
    CFRelease(js->js.axes);

    for (int i = 0;  i < CFArrayGetCount(js->js.buttons);  i++)
        free((void*) CFArrayGetValueAtIndex(js->js.buttons, i));
    CFRelease(js->js.buttons);

    for (int i = 0;  i < CFArrayGetCount(js->js.hats);  i++)
        free((void*) CFArrayGetValueAtIndex(js->js.hats, i));
    CFRelease(js->js.hats);

    _sokol_FreeJoystick(js);
}

_SOKOL_PRIVATE void matchCallback(void* context,
                          IOReturn result,
                          void* sender,
                          IOHIDDeviceRef device)
{
    int jid;
    char name[256];
    char guid[33];
    CFTypeRef property;
    uint32_t vendor = 0, product = 0, version = 0;
    _SOKOL_joystick* js;
    CFMutableArrayRef axes, buttons, hats;

    for (jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        if (_sokol_.joysticks[jid].js.device == device)
            return;
    }

    CFArrayRef elements =
        IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);

    // It is reportedly possible for this to fail on macOS 13 Ventura
    // if the application does not have input monitoring permissions
    if (!elements)
        return;

    axes    = CFArrayCreateMutable(NULL, 0, NULL);
    buttons = CFArrayCreateMutable(NULL, 0, NULL);
    hats    = CFArrayCreateMutable(NULL, 0, NULL);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    if (property)
    {
        CFStringGetCString(property,
                           name,
                           sizeof(name),
                           kCFStringEncodingUTF8);
    }
    else
        strncpy(name, "Unknown", sizeof(name));

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &vendor);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &product);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVersionNumberKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &version);

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (vendor && product)
    {
        sprintf(guid, "03000000%02x%02x0000%02x%02x0000%02x%02x0000",
                (uint8_t) vendor, (uint8_t) (vendor >> 8),
                (uint8_t) product, (uint8_t) (product >> 8),
                (uint8_t) version, (uint8_t) (version >> 8));
    }
    else
    {
        sprintf(guid, "05000000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    for (CFIndex i = 0;  i < CFArrayGetCount(elements);  i++)
    {
        IOHIDElementRef native = (IOHIDElementRef)
            CFArrayGetValueAtIndex(elements, i);
        if (CFGetTypeID(native) != IOHIDElementGetTypeID())
            continue;

        const IOHIDElementType type = IOHIDElementGetType(native);
        if ((type != kIOHIDElementTypeInput_Axis) &&
            (type != kIOHIDElementTypeInput_Button) &&
            (type != kIOHIDElementTypeInput_Misc))
        {
            continue;
        }

        CFMutableArrayRef target = NULL;

        const uint32_t usage = IOHIDElementGetUsage(native);
        const uint32_t page = IOHIDElementGetUsagePage(native);
        if (page == kHIDPage_GenericDesktop)
        {
            switch (usage)
            {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                case kHIDUsage_GD_Rx:
                case kHIDUsage_GD_Ry:
                case kHIDUsage_GD_Rz:
                case kHIDUsage_GD_Slider:
                case kHIDUsage_GD_Dial:
                case kHIDUsage_GD_Wheel:
                    target = axes;
                    break;
                case kHIDUsage_GD_Hatswitch:
                    target = hats;
                    break;
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadLeft:
                case kHIDUsage_GD_SystemMainMenu:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_Start:
                    target = buttons;
                    break;
            }
        }
        else if (page == kHIDPage_Simulation)
        {
            switch (usage)
            {
                case kHIDUsage_Sim_Accelerator:
                case kHIDUsage_Sim_Brake:
                case kHIDUsage_Sim_Throttle:
                case kHIDUsage_Sim_Rudder:
                case kHIDUsage_Sim_Steering:
                    target = axes;
                    break;
            }
        }
        else if (page == kHIDPage_Button || page == kHIDPage_Consumer)
            target = buttons;

        if (target)
        {
            _SOKOL_joyelementNS* element = calloc(1, sizeof(_SOKOL_joyelementNS));
            element->native  = native;
            element->usage   = usage;
            element->index   = (int) CFArrayGetCount(target);
            element->minimum = IOHIDElementGetLogicalMin(native);
            element->maximum = IOHIDElementGetLogicalMax(native);
            CFArrayAppendValue(target, element);
        }
    }

    CFRelease(elements);

    CFArraySortValues(axes, CFRangeMake(0, CFArrayGetCount(axes)),
                      compareElements, NULL);
    CFArraySortValues(buttons, CFRangeMake(0, CFArrayGetCount(buttons)),
                      compareElements, NULL);
    CFArraySortValues(hats, CFRangeMake(0, CFArrayGetCount(hats)),
                      compareElements, NULL);

    js = _sokol_AllocJoystick(name, guid,
                            (int) CFArrayGetCount(axes),
                            (int) CFArrayGetCount(buttons),
                            (int) CFArrayGetCount(hats));

    js->js.device  = device;
    js->js.axes    = axes;
    js->js.buttons = buttons;
    js->js.hats    = hats;

    _sokol_InputJoystick(js, SOKOL_CONNECTED);
}

_SOKOL_PRIVATE void removeCallback(void* context,
                           IOReturn result,
                           void* sender,
                           IOHIDDeviceRef device)
{
    for (int jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        if (_sokol_.joysticks[jid].connected && _sokol_.joysticks[jid].js.device == device)
        {
            closeJoystick(&_sokol_.joysticks[jid]);
            break;
        }
    }
}

_SOKOL_PRIVATE bool _sokol_InitJoysticks()
{
    static const _SOKOL_mapping mappings[] =
    {
        {"2In1 USB Joystick", "030000008f0e00000300000009010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"8BitDo FC30 Pro", "03000000c82d00000090000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"8BitDo FC30 Pro", "03000000c82d00001038000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"8BitDo M30", "03000000c82d00000650000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, },
        {"8BitDo M30 Gamepad", "03000000c82d00005106000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"8BitDo N30 Pro 2", "03000000c82d00001590000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"8BitDo N30 Pro 2", "03000000c82d00006528000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"8BitDo NES30 Gamepad", "030000003512000012ab000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000022000000090000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000203800000900000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000c82d00000190000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo SFC30 GamePad Joystick", "03000000102800000900000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30 Gamepad", "03000000c82d00001290000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SN30 GamePad", "03000000c82d00004028000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000160000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000161000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "03000000c82d00000260000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "03000000c82d00000261000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Wireless Adapter", "03000000c82d00000031000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Zero 2", "03000000c82d00001890000001000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo Zero 2", "03000000c82d00003032000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 31, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo Zero GamePad", "03000000a00500003232000008010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo Zero GamePad", "03000000a00500003232000009010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Astro City Mini", "03000000a30c00002700000003030000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Astro City Mini", "03000000a30c00002800000003030000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"ASUS Gamepad", "03000000050b00000045000031000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"AxisPad", "03000000ef0500000300000000020000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Amazon Luna Controller", "03000000491900001904000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Amazon Luna Controller", "03000000710100001904000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 11, 0, 0, 2, 6, 0, 0, 2, 10, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"BDA MOGA XP5-X Plus", "03000000c62400001a89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 2, 16, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"BDA MOGA XP5-X Plus", "03000000c62400001b89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"BDA PS4 Fightpad", "03000000d62000002a79000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Brook Mars", "03000000120c0000200e000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Brook Mars", "03000000120c0000210e000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Cideko AK08b", "030000008305000031b0000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Cyber Gadget GameCube Controller", "03000000260900008888000088020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"Cyborg V.3 Rumble Pad", "03000000a306000022f6000001030000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 1, 3, 2, -1, 1, 3, 2, 1, },
        {"GameCube Controller Adapter", "03000000790000004618000000010000", 2, 4, 0, 0, 2, 0, 0, 0, 2, 8, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 2, 28, 0, 0, 0, 0, 0, 0, 2, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 48, 0, 0, 2, 52, 0, 0, 2, 56, 0, 0, 2, 60, 0, 0, 1, 0, 1, 0, 1, 4, 1, 0, 1, 20, 1, 0, 1, 8, 1, 0, 1, 12, 1, 0, 1, 16, 1, 0, },
        {"Gamestop BB-070 X360 Controller", "03000000ad1b000001f9000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"GameStop Gamepad", "0500000047532047616d657061640000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GameStop PS4 Fun Controller", "03000000c01100000140000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"GameStop Xbox 360 Wired Controller", "030000006f0e00000102000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Gravis Eliminator GamePad Pro", "030000007d0400000540000001010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gravis Gamepad Pro", "03000000280400000140000000020000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GreenAsia Inc. USB Joystick", "030000008f0e00000300000007010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 3 Pro", "030000000d0f00002d00000000100000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS3)", "030000000d0f00005f00000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS4)", "030000000d0f00005e00000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"HORI Fighting Commander 4 PS3", "030000000d0f00005f00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Fighting Commander 4 PS4", "030000000d0f00005e00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Gem Pad 3", "030000000d0f00004d00000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Pokken Tournament DX Pro Pad", "030000000d0f00009200000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS3)", "030000000d0f00006e00000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS4)", "030000000d0f00006600000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"HORIPAD FPS PLUS 4", "030000000d0f00006600000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 1, 4, 1, 0, },
        {"HORIPAD mini4", "030000000d0f0000ee00000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HuiJia SNES Controller", "030000008f0e00001330000011010000", 2, 4, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 0, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 16, 0, 0, 2, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 0, 2, -1, 1, 2, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"iBuffalo SNES Controller", "03000000830500006020000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"iBuffalo USB 2-axis 8-button Gamepad", "03000000830500006020000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"JYS Wireless Adapter", "03000000242f00002d00000007010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000000020000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000000030000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000014040000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F310 Gamepad (DInput)", "030000006d04000016c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F510 Gamepad (DInput)", "030000006d04000018c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F710", "030000006d04000019c2000005030000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F710 Gamepad (XInput)", "030000006d0400001fc2000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech RumblePad 2 USB", "030000006d04000018c2000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Wireless Gamepad (DInput)", "030000006d04000019c2000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS3)", "03000000380700005032000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS4)", "03000000380700005082000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz FightStick TE S+ (PS3)", "03000000380700008433000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightStick TE S+ (PS4)", "03000000380700008483000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Marvo GT-004", "03000000790000000600000007010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash GameCube Controller", "03000000790000004418000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mayflash Magic NS", "03000000242f00007300000000020000", 2, 1, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Mayflash Magic NS", "0300000079000000d218000026010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Magic NS", "03000000d620000010a7000003010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Wii Classic Controller", "0300000025090000e803000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Wii U Pro Controller Adapter", "03000000790000000018000000010000", 2, 4, 0, 0, 2, 8, 0, 0, 2, 0, 0, 0, 2, 12, 0, 0, 2, 16, 0, 0, 2, 20, 0, 0, 2, 32, 0, 0, 2, 36, 0, 0, 0, 0, 0, 0, 2, 40, 0, 0, 2, 44, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 4, 1, 0, 1, 8, 1, 0, 1, 12, 1, 0, 2, 24, 0, 0, 2, 28, 0, 0, },
        {"Mayflash WiiU Pro Game Controller Adapter (DInput)", "03000000790000000018000000000000", 2, 4, 0, 0, 2, 8, 0, 0, 2, 0, 0, 0, 2, 12, 0, 0, 2, 16, 0, 0, 2, 20, 0, 0, 2, 32, 0, 0, 2, 36, 0, 0, 0, 0, 0, 0, 2, 40, 0, 0, 2, 44, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 4, 1, 0, 1, 8, 1, 0, 1, 12, 1, 0, 2, 24, 0, 0, 2, 28, 0, 0, },
        {"MC Cthulhu", "03000000d8140000cecf000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Microsoft SideWinder Plug & Play Game Pad", "030000005e0400002700000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Moga Pro 2 HID", "03000000d62000007162000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MOGA XP5-A Plus", "03000000c62400002a89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 21, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MOGA XP5-A Plus", "03000000c62400002b89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"NEOGEO mini PAD Controller", "03000000632500007505000000020000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"NES 2-port Adapter", "03000000921200004b46000003020000", 2, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"NEXT SNES Controller", "030000001008000001e5000006010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, },
        {"Nintendo Switch Core (Plus) Wired Controller", "03000000d620000011a7000000020000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Switch PowerA Wired Controller", "03000000d620000011a7000010050000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Switch Pro Controller", "030000007e0500000920000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Switch Pro Controller", "030000007e0500000920000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"NVIDIA Controller v01.04", "03000000550900001472000025050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 17, 0, 0, 2, 6, 0, 0, 2, 15, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PDP Versus Fighting Pad", "030000006f0e00000901000002010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Piranha xtreme", "030000008f0e00000300000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Playstation Classic Controller", "030000004c050000da0c000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PlayStation Vita", "030000004c0500003713000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"PowerA Pro Ex", "03000000d62000006dca000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS2 Adapter", "03000000100800000300000006010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 4, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000000000000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000000010000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS4 Controller", "030000004c050000a00b000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000c405000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000c405000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000cc09000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS5 Controller", "050000004c050000e60c000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Onza TE", "030000008916000000fd000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Panthera (PS3)", "03000000321500000204000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Razer Panthera (PS4)", "03000000321500000104000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer RAIJU", "03000000321500000010000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Raiju Mobile", "03000000321500000507000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 21, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Raion Fightpad for PS4", "03000000321500000011000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Serval", "03000000321500000009000000020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Serval", "030000003215000000090000163a0000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Wildcat", "0300000032150000030a000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Retrolink Classic Controller", "03000000790000001100000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Retrolink SNES Controller", "03000000790000001100000006010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 2, 1, 1, 3, 2, -1, 1, 4, 2, -1, 1, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Revolution Pro Controller", "030000006b140000010d000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Revolution Pro Controller 3", "030000006b140000130d000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Rock Candy Gamepad for PS3", "03000000c6240000fefa000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Sanwa PlayOnline Mobile", "03000000730700000401000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Sega Saturn", "03000000811700007e05000000000000", 2, 2, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 17, 0, 0, 2, 14, 0, 0, 2, 16, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Sega Saturn USB Gamepad", "03000000b40400000a01000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"SFC30 Joystick", "030000003512000021ab000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"SNES RetroPort", "0300000000f00000f100000000000000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Sony DualSense", "030000004c050000e60c000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Sony DualShock 4 V2", "030000004c050000cc09000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 13, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Sony DualShock 4 Wireless Adaptor", "030000004c050000a00b000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 13, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Stadia Controller", "03000000d11800000094000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Steam Virtual Gamepad", "030000005e0400008e02000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"SteelSeries Nimbus", "03000000110100002014000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Nimbus", "03000000110100002014000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Nimbus", "03000000381000002014000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Nimbus Plus", "050000004e696d6275732b0000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 15, 0, 0, 2, 14, 0, 0, 2, 16, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 12, 0, 0, 2, 11, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Stratus XL", "03000000110100001714000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SteelSeries Stratus XL", "03000000110100001714000020010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SZMY-POWER PC Gamepad", "03000000457500002211000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Dual Analog 3.2", "030000004f04000015b3000000000000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"ThrustMaster eSwap PRO Controller", "030000004f0400000ed0000000020000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Thrustmaster Firestorm Dual Power", "030000004f04000000b3000000000000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Tomee SNES USB Controller", "03000000bd12000015d0000000000000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Tomee SNES USB Controller", "03000000bd12000015d0000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Twin USB Joystick", "03000000100800000100000000000000", 2, 4, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 0, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 16, 0, 0, 2, 18, 0, 0, 0, 0, 0, 0, 2, 20, 0, 0, 2, 22, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 2, 1, 0, 1, 6, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 10, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000302000025040000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000702000003060000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Wii Classic Controller", "03000000791d00000103000009010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Wii Remote", "050000005769696d6f74652028303000", 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, },
        {"Wii U Pro Controller", "050000005769696d6f74652028313800", 2, 16, 0, 0, 2, 15, 0, 0, 2, 18, 0, 0, 2, 17, 0, 0, 2, 19, 0, 0, 2, 20, 0, 0, 2, 7, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 23, 0, 0, 2, 24, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 21, 0, 0, 2, 22, 0, 0, },
        {"X360 Controller", "030000005e0400008e02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox 360 Wired Controller", "030000006f0e00000104000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox 360 Wired Controller", "03000000c6240000045d000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Adaptive Controller", "030000005e0400000a0b000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Elite Wireless Controller Series 2", "030000005e040000050b000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 31, 0, 0, 2, 11, 0, 0, 2, 53, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 6, 1, 0, 1, 5, 1, 0, },
        {"Xbox One PowerA Wired Controller", "03000000c62400003a54000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wired Controller", "030000005e040000d102000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wired Controller", "030000005e040000dd02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wired Controller", "030000005e040000e302000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Series Controller", "030000005e040000130b000001050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox Series Controller", "030000005e040000130b000005050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox Wireless Controller", "030000005e040000e002000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Wireless Controller", "030000005e040000e002000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Wireless Controller", "030000005e040000ea02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Wireless Controller", "030000005e040000fd02000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 16, 0, 0, 2, 11, 0, 0, 2, 15, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"XiaoMi Game Controller", "03000000172700004431000029010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 15, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 1, 6, 1, 0, },
        {"ZEROPLUS P4 Gamepad", "03000000120c0000100e000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"ZEROPLUS P4 Wired Gamepad", "03000000120c0000101e000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
    };

    _sokol_.mappings = mappings;
    _sokol_.mappingCount = _countof(mappings);

    CFMutableArrayRef matching;
    const long usages[] =
    {
        kHIDUsage_GD_Joystick,
        kHIDUsage_GD_GamePad,
        kHIDUsage_GD_MultiAxisController
    };

    _sokol_.js.hidManager = IOHIDManagerCreate(kCFAllocatorDefault,
                                             kIOHIDOptionsTypeNone);

    matching = CFArrayCreateMutable(kCFAllocatorDefault,
                                    0,
                                    &kCFTypeArrayCallBacks);
    if (!matching)
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR, "Cocoa: Failed to create array");
        return false;
    }

    for (size_t i = 0;  i < sizeof(usages) / sizeof(long);  i++)
    {
        const long page = kHIDPage_GenericDesktop;

        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault,
                                      0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        if (!dict)
            continue;

        CFNumberRef pageRef = CFNumberCreate(kCFAllocatorDefault,
                                             kCFNumberLongType,
                                             &page);
        CFNumberRef usageRef = CFNumberCreate(kCFAllocatorDefault,
                                              kCFNumberLongType,
                                              &usages[i]);
        if (pageRef && usageRef)
        {
            CFDictionarySetValue(dict,
                                 CFSTR(kIOHIDDeviceUsagePageKey),
                                 pageRef);
            CFDictionarySetValue(dict,
                                 CFSTR(kIOHIDDeviceUsageKey),
                                 usageRef);
            CFArrayAppendValue(matching, dict);
        }

        if (pageRef)
            CFRelease(pageRef);
        if (usageRef)
            CFRelease(usageRef);

        CFRelease(dict);
    }

    IOHIDManagerSetDeviceMatchingMultiple(_sokol_.js.hidManager, matching);
    CFRelease(matching);

    IOHIDManagerRegisterDeviceMatchingCallback(_sokol_.js.hidManager,
                                               &matchCallback, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(_sokol_.js.hidManager,
                                              &removeCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(_sokol_.js.hidManager,
                                    CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    IOHIDManagerOpen(_sokol_.js.hidManager, kIOHIDOptionsTypeNone);

    // Execute the run loop once in order to register any initially-attached
    // joysticks
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    return true;
}

_SOKOL_PRIVATE void _sokol_TerminateJoysticks(void)
{
    for (int jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        if (_sokol_.joysticks[jid].connected)
            closeJoystick(&_sokol_.joysticks[jid]);
    }

    if (_sokol_.js.hidManager)
    {
        CFRelease(_sokol_.js.hidManager);
        _sokol_.js.hidManager = NULL;
    }
}


_SOKOL_PRIVATE bool _sokol_PollJoystick(_SOKOL_joystick* js, int mode)
{
    if (mode & _SOKOL_POLL_AXES)
    {
        for (CFIndex i = 0;  i < CFArrayGetCount(js->js.axes);  i++)
        {
            _SOKOL_joyelementNS* axis = (_SOKOL_joyelementNS*)
                CFArrayGetValueAtIndex(js->js.axes, i);

            const long raw = getElementValue(js, axis);
            // Perform auto calibration
            if (raw < axis->minimum)
                axis->minimum = raw;
            if (raw > axis->maximum)
                axis->maximum = raw;

            const long size = axis->maximum - axis->minimum;
            if (size == 0)
                _sokol_InputJoystickAxis(js, (int) i, 0.f);
            else
            {
                const float value = (2.f * (raw - axis->minimum) / size) - 1.f;
                _sokol_InputJoystickAxis(js, (int) i, value);
            }
        }
    }

    if (mode & _SOKOL_POLL_BUTTONS)
    {
        for (CFIndex i = 0;  i < CFArrayGetCount(js->js.buttons);  i++)
        {
            _SOKOL_joyelementNS* button = (_SOKOL_joyelementNS*)
                CFArrayGetValueAtIndex(js->js.buttons, i);
            const char value = getElementValue(js, button) - button->minimum;
            const int state = (value > 0) ? SOKOL_PRESS : SOKOL_RELEASE;
            _sokol_InputJoystickButton(js, (int) i, state);
        }

        for (CFIndex i = 0;  i < CFArrayGetCount(js->js.hats);  i++)
        {
            const int states[9] =
            {
                SOKOL_HAT_UP,
                SOKOL_HAT_RIGHT_UP,
                SOKOL_HAT_RIGHT,
                SOKOL_HAT_RIGHT_DOWN,
                SOKOL_HAT_DOWN,
                SOKOL_HAT_LEFT_DOWN,
                SOKOL_HAT_LEFT,
                SOKOL_HAT_LEFT_UP,
                SOKOL_HAT_CENTERED
            };

            _SOKOL_joyelementNS* hat = (_SOKOL_joyelementNS*)
                CFArrayGetValueAtIndex(js->js.hats, i);
            long state = getElementValue(js, hat) - hat->minimum;
            if (state < 0 || state > 8)
                state = 8;

            _sokol_InputJoystickHat(js, (int) i, states[state]);
        }
    }

    return js->connected;
}

SOKOL_API_IMPL void sgamepad_init() {

    initJoysticks();
}

SOKOL_API_IMPL void sgamepad_shutdown() {

    _sokol_TerminateJoysticks();
}

#elif defined(_SAPP_LINUX)

#include <sys/inotify.h>
#include <dirent.h>

_SOKOL_PRIVATE void handleKeyEvent(_SOKOL_joystick* js, int code, int value)
{
    _sokol_InputJoystickButton(js, js->js.keyMap[code - BTN_MISC], value ? SOKOL_PRESS : SOKOL_RELEASE);
}

_SOKOL_PRIVATE void handleAbsEvent(_SOKOL_joystick* js, int code, int value)
{
    const int index = js->js.absMap[code];

    if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
    {
        static const char stateMap[3][3] =
        {
            { SOKOL_HAT_CENTERED, SOKOL_HAT_UP,       SOKOL_HAT_DOWN },
            { SOKOL_HAT_LEFT,     SOKOL_HAT_LEFT_UP,  SOKOL_HAT_LEFT_DOWN },
            { SOKOL_HAT_RIGHT,    SOKOL_HAT_RIGHT_UP, SOKOL_HAT_RIGHT_DOWN },
        };

        const int hat = (code - ABS_HAT0X) / 2;
        const int axis = (code - ABS_HAT0X) % 2;
        int* state = js->js.hats[hat];

        // NOTE: Looking at several input drivers, it seems all hat events use
        //       -1 for left / up, 0 for centered and 1 for right / down
        if (value == 0)
            state[axis] = 0;
        else if (value < 0)
            state[axis] = 1;
        else if (value > 0)
            state[axis] = 2;

        _sokol_InputJoystickHat(js, index, stateMap[state[0]][state[1]]);
    }
    else
    {
        const struct input_absinfo* info = &js->js.absInfo[code];
        float normalized = value;

        const int range = info->maximum - info->minimum;
        if (range)
        {
            // Normalize to 0.0 -> 1.0
            normalized = (normalized - info->minimum) / range;
            // Normalize to -1.0 -> 1.0
            normalized = normalized * 2.0f - 1.0f;
        }

        _sokol_InputJoystickAxis(js, index, normalized);
    }
}

_SOKOL_PRIVATE void pollAbsState(_SOKOL_joystick* js)
{
    for (int code = 0;  code < ABS_CNT;  code++)
    {
        if (js->js.absMap[code] < 0)
            continue;

        struct input_absinfo* info = &js->js.absInfo[code];

        if (ioctl(js->js.fd, EVIOCGABS(code), info) < 0)
            continue;

        handleAbsEvent(js, code, info->value);
    }
}

#define isBitSet(bit, arr) (arr[(bit) / 8] & (1 << ((bit) % 8)))

_SOKOL_PRIVATE bool openJoystickDevice(const char* path)
{
    for (int jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        if (!_sokol_.joysticks[jid].connected)
            continue;
        if (strcmp(_sokol_.joysticks[jid].js.path, path) == 0)
            return false;
    }

    _SOKOL_joystickPlatform js = {0};
    js.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (js.fd == -1)
        return false;

    char evBits[(EV_CNT + 7) / 8] = {0};
    char keyBits[(KEY_CNT + 7) / 8] = {0};
    char absBits[(ABS_CNT + 7) / 8] = {0};
    struct input_id id;

    if (ioctl(js.fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0 ||
        ioctl(js.fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0 ||
        ioctl(js.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 ||
        ioctl(js.fd, EVIOCGID, &id) < 0)
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR,
                        "Linux: Failed to query input device: %s",
                        strerror(errno));
        close(js.fd);
        return false;
    }

    // Ensure this device supports the events expected of a joystick
    if (!isBitSet(EV_ABS, evBits))
    {
        close(js.fd);
        return false;
    }

    char name[256] = "";

    if (ioctl(js.fd, EVIOCGNAME(sizeof(name)), name) < 0)
        strncpy(name, "Unknown", sizeof(name));

    char guid[33] = "";

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (id.vendor && id.product && id.version)
    {
        sprintf(guid, "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                id.bustype & 0xff, id.bustype >> 8,
                id.vendor & 0xff,  id.vendor >> 8,
                id.product & 0xff, id.product >> 8,
                id.version & 0xff, id.version >> 8);
    }
    else
    {
        sprintf(guid, "%02x%02x0000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                id.bustype & 0xff, id.bustype >> 8,
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    int axisCount = 0, buttonCount = 0, hatCount = 0;

    for (int code = BTN_MISC;  code < KEY_CNT;  code++)
    {
        if (!isBitSet(code, keyBits))
            continue;

        js.keyMap[code - BTN_MISC] = buttonCount;
        buttonCount++;
    }

    for (int code = 0;  code < ABS_CNT;  code++)
    {
        js.absMap[code] = -1;
        if (!isBitSet(code, absBits))
            continue;

        if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
        {
            js.absMap[code] = hatCount;
            hatCount++;
            // Skip the Y axis
            code++;
        }
        else
        {
            if (ioctl(js.fd, EVIOCGABS(code), &js.absInfo[code]) < 0)
                continue;

            js.absMap[code] = axisCount;
            axisCount++;
        }
    }

    _SOKOL_joystick* joys =
        _sokol_AllocJoystick(name, guid, axisCount, buttonCount, hatCount);
    if (!joys)
    {
        close(js.fd);
        return false;
    }

    strncpy(js.path, path, sizeof(js.path) - 1);
    memcpy(&joys->js, &js, sizeof(js));

    pollAbsState(joys);

    _sokol_InputJoystick(joys, SOKOL_CONNECTED);
    return true;
}

#undef isBitSet

_SOKOL_PRIVATE void closeJoystick(_SOKOL_joystick* js)
{
    _sokol_InputJoystick(js, SOKOL_DISCONNECTED);
    close(js->js.fd);
    _sokol_FreeJoystick(js);
}

_SOKOL_PRIVATE int compareJoysticks(const void* fp, const void* sp)
{
    const _SOKOL_joystick* fj = fp;
    const _SOKOL_joystick* sj = sp;
    return strcmp(fj->js.path, sj->js.path);
}

_SOKOL_PRIVATE bool _sokol_InitJoysticks()
{
    static const _SOKOL_mapping mappings[] =
    {
        {"8BitDo FC30 Pro", "03000000c82d00000090000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"8Bitdo FC30 Pro", "05000000c82d00001038000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo M30", "05000000c82d00005106000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, },
        {"8BitDo N30 Pro 2", "03000000c82d00001590000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo N30 Pro 2", "05000000c82d00006528000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo NES30", "03000000c82d00000310000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, },
        {"8BitDo NES30", "05000000c82d00008010000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, },
        {"8Bitdo NES30 Pro", "03000000022000000090000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "05000000203800000900000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo NES30 Pro", "05000000c82d00002038000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"8Bitdo NES30 Pro 8Bitdo NES30 Pro", "03000000c82d00000190000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"8BitDo SF30 Pro", "05000000c82d00000060000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8Bitdo SF30 Pro", "05000000c82d00000061000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"8BitDo SFC30", "03000000c82d000021ab000010010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SFC30 GamePad", "030000003512000012ab000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SFC30 GamePad", "05000000102800000900000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo SFC30 GamePad", "05000000c82d00003028000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000160000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000160000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00000161000000000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "03000000c82d00001290000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "05000000c82d00000161000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro", "05000000c82d00006228000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "03000000c82d00000260000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SN30 Pro+", "05000000c82d00000261000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo SNES30 Gamepad", "05000000202800000900000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 117, 0, 0, 2, 120, 0, 0, 2, 122, 0, 0, 2, 119, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo Wireless Adapter (DInput)", "03000000c82d00000031000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 2, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"8BitDo Wireless Adapter (XInput)", "030000005e0400008e02000020010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"8BitDo Zero 2", "03000000c82d00001890000011010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo Zero 2", "05000000c82d00003032000000010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8BitDo Zero 2 (XInput)", "050000005e040000e002000030110000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo Zero GamePad", "05000000a00500003232000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"8Bitdo Zero GamePad", "05000000a00500003232000008010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"ACRUX USB GAME PAD", "03000000c01100000355000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Afterglow", "030000006f0e00001302000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Afterglow Controller for Xbox One", "030000006f0e00003901000020060000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Afterglow Prismatic Controller", "030000006f0e00003901000000430000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Afterglow Prismatic Wired Controller 048-007-NA", "030000006f0e00003901000013020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Akishop Customs PS360+ v1.66", "03000000100000008200000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Alienware Dual Compatible Game Pad", "030000007c1800000006000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Amazon Fire Game Controller", "05000000491900000204000021000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 17, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Amazon Luna Controller", "03000000491900001904000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Amazon Luna Controller", "05000000710100001904000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 6, 0, 0, 2, 10, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Arcade Fightstick F300", "03000000790000003018000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Astro City Mini", "03000000a30c00002700000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Astro City Mini", "03000000a30c00002800000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"ASUS Gamepad", "05000000050b00000045000031000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"ASUS Gamepad", "05000000050b00000045000040000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Atari Classic Controller", "03000000503200000110000000000000", 2, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Atari Classic Controller", "05000000503200000110000000000000", 2, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Atari Game Controller", "03000000503200000210000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Atari Game Controller", "05000000503200000210000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"AxisPad", "03000000120c00000500000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"AxisPad", "03000000ef0500000300000000010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"BDA MOGA XP5-X Plus", "03000000c62400001b89000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"BDA PS4 Fightpad", "03000000d62000002a79000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Be1 GC101 Controller 1.03 mode", "03000000c21100000791000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Be1 GC101 GAMEPAD 1.03 mode", "03000000c31100000791000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Be1 GC101 Xbox 360 Controller mode", "030000005e0400008e02000003030000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"BETOP AX1 BFM", "05000000bc2000000055000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"boom PSX to PC Converter", "03000000666600006706000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Brook Mars", "03000000120c0000200e000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Brook Mars", "03000000120c0000210e000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Brook Universal Fighting Board", "03000000120c0000f70e000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Chinese-made Xbox Controller", "03000000ffff0000ffff000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Cideko AK08b", "03000000e82000006058000001010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Competition Pro", "030000000b0400003365000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Cyber Gadget GameCube Controller", "03000000260900008888000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"Cyborg V.3 Rumble Pad", "03000000a306000022f6000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 1, 3, 2, -1, 1, 3, 2, 1, },
        {"CYPRESS USB Gamepad", "03000000b40400000a01000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"DragonRise Inc. Generic USB Joystick", "03000000790000000600000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Dual Power 2", "030000004f04000004b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"EA Sports PS3 Controller", "030000006f0e00003001000001010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GameCube {HuiJia USB box}", "03000000341a000005f7000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"GameSir G3w", "03000000bc2000000055000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"GameStop Gamepad", "0500000047532047616d657061640000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gamestop Logic3 Controller", "030000006f0e00000104000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Gasia Co. Ltd PS(R) Gamepad", "030000008f0e00000800000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Generic X-Box pad", "030000006f0e00001304000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Genius Maxfire Grandias 12", "03000000451300000010000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Goodbetterbest Ltd USB Controller", "03000000f0250000c183000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GPD Win 2 Controller", "0300000079000000d418000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Gravis Eliminator GamePad Pro", "030000007d0400000540000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Gravis GamePad Pro USB ", "03000000280400000140000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GreenAsia Electronics 4Axes 12Keys GamePad ", "030000008f0e00000610000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"GreenAsia Inc. USB Joystick", "030000008f0e00001200000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"GS gamepad", "0500000047532067616d657061640000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"GT VX2", "03000000f0250000c383000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hidromancer Game Controller", "06000000adde0000efbe000002010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"HitBox (PS3/PC) Analog Mode", "03000000d81400000862000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HJC Game GAMEPAD", "03000000c9110000f055000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HJD-X", "03000000632500002605000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"hori", "030000000d0f00000d00000000010000", 2, 0, 0, 0, 2, 6, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"HORI CO. LTD. FIGHTING STICK 3", "030000000d0f00001000000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI CO. LTD. HORIPAD S", "030000000d0f0000c100000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 13, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI CO. LTD. Real Arcade Pro.4", "030000000d0f00006a00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"HORI CO. LTD. Real Arcade Pro.4", "030000000d0f00006b00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI CO. LTD. REAL ARCADE Pro.V3", "030000000d0f00002200000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Fighting Commander", "030000000d0f00008500000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander", "030000000d0f00008600000002010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS3)", "030000000d0f00005f00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Hori Fighting Commander 4 (PS4)", "030000000d0f00005e00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Hori Pad EX Turbo 2", "03000000ad1b000001f5000033050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Hori Pokken Tournament DX Pro Pad", "030000000d0f00009200000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Real Arcade Pro", "030000000d0f0000aa00000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORI Real Arcade Pro S", "030000000d0f0000d800000072056800", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"Hori Real Arcade Pro.EX-SE (Xbox 360)", "030000000d0f00001600000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS3)", "030000000d0f00006e00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD 4 (PS4)", "030000000d0f00006600000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"HORIPAD mini4", "030000000d0f0000ee00000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"HORIPAD ONE", "030000000d0f00006700000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"HuiJia SNES Controller", "030000008f0e00001330000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Hyperkin X91", "03000000242e00008816000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"iBuffalo SNES Controller", "03000000830500006020000010010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"idroid:con", "050000006964726f69643a636f6e0000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"impact", "03000000b50700001503000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"IMS PCU#0 Gamepad Interface", "03000000d80400008200000003000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"InterAct GoPad I-73000 (Fighting Game Layout)", "03000000fd0500000030000000010000", 2, 3, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, },
        {"Ipega PG-9069 - Bluetooth Gamepad", "0500000049190000020400001b010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 161, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Ipega PG-9099 - Bluetooth Gamepad", "03000000632500007505000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JC-U3613M - DirectInput Mode", "030000006e0500000320000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Jess Tech Dual Analog Rumble Pad", "03000000300f00001001000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Jess Tech GGE909 PC Recoil Pad", "03000000300f00000b01000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Jess Technology USB Game Controller", "03000000ba2200002010000001010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Joypad Alpha Shock", "03000000bd12000003c0000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JYS Wireless Adapter", "03000000242f00002d00000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"JYS Wireless Adapter", "03000000242f00008a00000011010000", 2, 1, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Logic3 Controller", "030000006f0e00000103000000020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech ChillStream", "030000006d040000d1ca000000000000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Cordless RumblePad 2", "030000006d04000019c2000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech Dual Action", "030000006d04000016c2000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F310 Gamepad (XInput)", "030000006d0400001dc2000014400000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech F510 Gamepad (XInput)", "030000006d0400001ec2000019200000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech F510 Gamepad (XInput)", "030000006d0400001ec2000020200000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech F710 Gamepad (DInput)", "030000006d04000019c2000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech F710 Gamepad (XInput)", "030000006d0400001fc2000005030000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Logitech Inc. WingMan RumblePad", "030000006d0400000ac2000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 7, 0, 0, 2, 2, 0, 0, },
        {"Logitech RumblePad 2", "030000006d04000018c2000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Logitech WingMan Cordless RumblePad", "030000006d04000011c2000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 8, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 9, 0, 0, 2, 10, 0, 0, },
        {"M54-PC", "050000004d4f435554452d3035305800", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz C.T.R.L.R ", "05000000380700006652000025010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS3)", "03000000380700005032000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightPad PRO (PS4)", "03000000380700005082000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz Fightpad SFxT", "03000000ad1b00002ef0000090040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Mad Catz fightstick (PS3)", "03000000380700008034000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz fightstick (PS4)", "03000000380700008084000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz FightStick TE S+ (PS3)", "03000000380700008433000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz FightStick TE S+ (PS4)", "03000000380700008483000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mad Catz Wired Xbox 360 Controller", "03000000380700001647000010040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Mad Catz Wired Xbox 360 Controller (SFIV)", "03000000380700003847000090040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mad Catz Xbox 360 Controller", "03000000ad1b000016f0000090040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"MadCatz PC USB Wired Stick 8818", "03000000380700001888000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"MadCatz PC USB Wired Stick 8838", "03000000380700003888000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Magic-S Pro", "03000000242f0000f700000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Manta Dualshock 2", "03000000120c00000500000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash GameCube Controller", "03000000790000004418000010010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mayflash GameCube Controller Adapter", "03000000790000004318000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Mayflash Magic NS", "03000000242f00007300000011010000", 2, 1, 0, 0, 2, 4, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Mayflash Magic NS", "0300000079000000d218000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Magic NS", "03000000d620000010a7000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Mayflash Wii Classic Controller", "0300000025090000e803000001010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 1, 4, 1, 0, 1, 5, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Microntek USB Joystick", "03000000780000000600000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Microsoft SideWinder", "030000005e0400000e00000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Microsoft X-Box 360 pad", "030000005e0400008e02000004010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box 360 pad", "030000005e0400008e02000062230000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box One Elite 2 pad", "050000005e040000050b000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 17, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 6, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box One Elite pad", "030000005e040000e302000003020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box One pad", "030000005e040000d102000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box One pad (Firmware 2015)", "030000005e040000dd02000003020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box One pad v2", "030000005e040000d102000003020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box pad (Japan)", "030000005e0400008502000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft X-Box pad v2 (US)", "030000005e0400008902000021010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft Xbox One Elite 2 pad - Wired", "030000005e040000000b000008040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Microsoft Xbox One S pad - Wired", "030000005e040000ea02000008040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Mini PE", "03000000c62400001a53000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Miroof", "03000000030000000300000002000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Moga 2 HID", "05000000d6200000e589000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Moga Pro", "05000000d6200000ad0d000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Moga Pro 2 HID", "05000000d62000007162000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MOGA XP5-A Plus", "03000000c62400002b89000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MOGA XP5-A Plus", "05000000c62400002a89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 22, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MOGA XP5-X Plus", "05000000c62400001a89000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"MP-8866 Super Dual Box", "03000000250900006688000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"NACON GC-400ES", "030000006b140000010c000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Natec Genesis P44", "030000000d0f00000900000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"NEXILUX GAMECUBE Controller Adapter", "03000000790000004518000010010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"NEXT SNES Controller", "030000001008000001e5000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, },
        {"Nintendo 3DS", "060000007e0500003713000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Nintendo Combined Joy-Cons (joycond)", "060000007e0500000820000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 17, 0, 0, 2, 15, 0, 0, 2, 16, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 7, 0, 0, 2, 8, 0, 0, },
        {"Nintendo GameCube Controller", "030000007e0500003703000000016800", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 4, 0, 0, 1, 0, 1, 0, 1, 1, -1, 0, 1, 2, 1, 0, 1, 3, -1, 0, 1, 4, 1, 0, 1, 5, 1, 0, },
        {"Nintendo GameCube Controller Adapter", "03000000790000004618000010010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 5, -1, 0, 1, 2, -1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Nintendo Switch Left Joy-Con", "050000007e0500000620000001800000", 2, 9, 0, 0, 2, 8, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Nintendo Switch Pro Controller", "030000007e0500000920000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 7, 0, 0, 2, 8, 0, 0, },
        {"Nintendo Switch Pro Controller", "050000007e0500000920000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Switch Pro Controller", "050000007e0500000920000001800000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 7, 0, 0, 2, 8, 0, 0, },
        {"Nintendo Switch Right Joy-Con", "050000007e0500000720000001800000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, -1, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Nintendo Switch SNES Controller", "050000007e0500001720000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Nintendo Wii Remote Pro Controller", "050000007e0500003003000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nintendo Wiimote", "05000000010000000100000003000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Nostromo n45 Dual Analog Gamepad", "030000000d0500000308000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"NVIDIA Controller", "03000000550900001072000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 13, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"NVIDIA Controller v01.04", "03000000550900001472000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 16, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"NVIDIA Controller v01.04", "05000000550900001472000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 16, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"NYKO CORE", "03000000451300000830000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"odroidgo2_joypad", "19000000010000000100000001010000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 15, 0, 0, 2, 10, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, },
        {"odroidgo2_joypad_v11", "19000000010000000200000011000000", 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 17, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, },
        {"Old Xbox pad", "030000005e0400000202000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 2, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"OnyxSoft Dual JoyDivision", "03000000c0160000dc27000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"OUYA Game Controller", "05000000362800000100000002010000", 2, 0, 0, 0, 2, 3, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"OUYA Game Controller", "05000000362800000100000003010000", 2, 0, 0, 0, 2, 3, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Padix Co. Ltd. Rockfire PSX/USB Bridge", "03000000830500005020000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PC Game Controller", "03000000790000001c18000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PC Game Controller", "03000000ff1100003133000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PDP AFTERGLOW Wired Xbox One Controller", "030000006f0e0000b802000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PDP AFTERGLOW Wired Xbox One Controller", "030000006f0e0000b802000013020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PDP Battlefield One", "030000006f0e00006401000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PDP CO. LTD. Faceoff Wired Pro Controller for Nintendo Switch", "030000006f0e00008001000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PDP EA Sports Controller", "030000006f0e00003101000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PDP Kingdom Hearts Controller", "030000006f0e0000c802000012010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PDP Rock Candy Wired Controller for Nintendo Switch", "030000006f0e00008701000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 13, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PDP Versus Fighting Pad", "030000006f0e00000901000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PDP Wired Controller for Xbox One", "030000006f0e0000a802000023020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PDP Wired Fight Pad Pro for Nintendo Switch", "030000006f0e00008501000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PG-9099", "0500000049190000030400001b010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PG-9118", "05000000491900000204000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Playstation Controller", "030000004c050000da0c000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PlayStation Vita", "030000004c0500003713000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"PowerA", "03000000c62400000053000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PowerA 1428124-01", "03000000c62400003a54000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PowerA Pro Ex", "03000000d62000006dca000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PowerA Wired Controller for Xbox One", "03000000d62000000228000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PowerA Xbox One Cabled", "03000000c62400001a58000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PowerA Xbox One Mini Wired Controller", "03000000c62400001a54000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Precision Controller", "030000006d040000d2ca000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS2 Controller", "03000000ff1100004133000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"PS3 Controller", "03000000341a00003608000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000010010000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000010810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS3 Controller", "030000004c0500006802000011010000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS3 Controller", "030000004c0500006802000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS3 Controller", "030000006f0e00001402000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "030000008f0e00000300000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS3 Controller", "050000004c0500006802000000000000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS3 Controller", "050000004c0500006802000000010000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 12, 1, 0, 1, 13, 1, 0, },
        {"PS3 Controller", "050000004c0500006802000000800000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS3 Controller", "050000004c0500006802000000810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 16, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS3 Controller", "05000000504c415953544154494f4e00", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS3 Controller", "060000004c0500006802000000010000", 2, 14, 0, 0, 2, 13, 0, 0, 2, 15, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 16, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"PS4 Controller", "030000004c050000a00b000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000a00b000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "030000004c050000c405000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000c405000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "030000004c050000cc09000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000cc09000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "030000004c050000cc09000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "03000000c01100000140000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"PS4 Controller", "050000004c050000c405000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "050000004c050000c405000000810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "050000004c050000c405000001800000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "050000004c050000cc09000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS4 Controller", "050000004c050000cc09000000810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS4 Controller", "050000004c050000cc09000001800000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"PS5 Controller", "030000004c050000e60c000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PS5 Controller", "050000004c050000e60c000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"PSP", "03000000ff000000cb01000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"QanBa Arcade JoyStick", "03000000300f00001211000011010000", 2, 2, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, },
        {"Raphnet Technologies Dual NES to USB v2.0", "030000009b2800004200000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Raphnet Technologies GC/N64 to USB v3.4", "030000009b2800003200000001010000", 2, 0, 0, 0, 2, 7, 0, 0, 2, 1, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Raphnet Technologies GC/N64 to USB v3.6", "030000009b2800006000000001010000", 2, 0, 0, 0, 2, 7, 0, 0, 2, 1, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"raphnet.net 4nes4snes v1.5", "030000009b2800000300000001010000", 2, 0, 0, 0, 2, 4, 0, 0, 2, 1, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Razer Onza Classic Edition", "030000008916000001fd000024010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Onza Tournament Edition", "030000008916000000fd000024010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Panthera (PS3)", "03000000321500000204000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Razer Panthera (PS4)", "03000000321500000104000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Panthera Evo Arcade Stick for PS4", "03000000321500000810000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 13, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer RAIJU", "03000000321500000010000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Raiju Mobile", "03000000321500000507000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 21, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Raion Fightpad for PS4", "03000000321500000011000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Razer Sabertooth", "030000008916000000fe000024010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Sabertooth", "03000000c6240000045d000024010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Sabertooth", "03000000c6240000045d000025010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Razer Serval", "03000000321500000009000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Serval", "050000003215000000090000163a0000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Razer Wildcat", "0300000032150000030a000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Retrolink SNES Controller", "03000000790000001100000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Retronic Adapter", "0300000081170000990a000001010000", 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"RetroPad", "0300000000f000000300000000010000", 2, 1, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Revolution Pro Controller", "030000006b140000010d000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Revolution Pro Controller 3", "030000006b140000130d000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Rock Candy", "030000006f0e00001f01000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Rock Candy PS3 Controller", "030000006f0e00001e01000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Rock Candy Xbox One Controller", "030000006f0e00004601000001010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Saitek Cyborg V.1 Game Pad", "03000000a306000023f6000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P150", "03000000a30600001005000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 5, 0, 0, },
        {"Saitek P220", "03000000a30600000701000000010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 6, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 5, 0, 0, },
        {"Saitek P2500 Force Rumble Pad", "03000000a30600000cff000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 11, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P2900 Wireless Pad", "03000000a30600000c04000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 12, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Saitek P380", "03000000300f00001201000010010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Saitek P880", "03000000a30600000901000000010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek P990 Dual Analog Pad", "03000000a30600000b04000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Saitek PLC Saitek P3200 Rumble Pad", "03000000a306000018f5000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 2, 7, 0, 0, },
        {"Saitek PS2700 Rumble Pad", "03000000a306000020f6000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Savior", "03000000d81d00000e00000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 2, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 3, 0, 0, },
        {"ShanWan Gioteck PS3 Wired Controller", "03000000f025000021c1000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SHANWAN PS3/PC Gamepad", "03000000632500007505000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"ShanWan PS3/PC Wired GamePad", "03000000bc2000000055000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"SHANWAN Trust Gamepad", "030000005f140000c501000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"ShanWan USB Gamepad", "03000000632500002305000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SL-6566", "03000000341a00000908000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Sony DualSense", "030000004c050000e60c000011810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Sony DualSense ", "050000004c050000e60c000000810000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Sony PS2 pad with SmartJoy adapter", "03000000250900000500000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Speedlink TORID Wireless Gamepad", "030000005e0400008e02000073050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"SpeedLink XEOX Pro Analog Gamepad pad", "030000005e0400008e02000020200000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Stadia Controller", "03000000d11800000094000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Steam Controller", "03000000de2800000112000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Controller", "03000000de2800000211000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Controller", "03000000de2800000211000011010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 0, 0, 0, 0, 2, 17, 0, 0, 2, 20, 0, 0, 2, 18, 0, 0, 2, 19, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Steam Controller", "03000000de2800004211000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Controller", "03000000de2800004211000011010000", 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 0, 0, 0, 0, 2, 17, 0, 0, 2, 20, 0, 0, 2, 18, 0, 0, 2, 19, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Steam Controller", "03000000de280000fc11000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Steam Controller", "05000000de2800000212000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Controller", "05000000de2800000511000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Controller", "05000000de2800000611000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 3, 1, 0, },
        {"Steam Virtual Gamepad", "03000000de280000ff11000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"SteelSeries Stratus Duo", "03000000381000003014000075010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"SteelSeries Stratus Duo", "03000000381000003114000075010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"SteelSeries Stratus Duo", "0500000011010000311400001b010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 32, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"SteelSeries Stratus XL", "05000000110100001914000009010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Street Fighter IV FightStick TE", "03000000ad1b000038f0000090040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Suncom SFX Plus for USB", "030000003b07000004a1000000010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Super Joy Box 5 Pro", "03000000666600000488000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 2, 15, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Super RetroPort", "0300000000f00000f100000000010000", 2, 1, 0, 0, 2, 5, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"SZMY-POWER CO. LTD. GAMEPAD", "03000000457500002211000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SZMY-POWER CO. LTD. GAMEPAD 3 TURBO", "030000008f0e00000d31000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"SZMY-POWER CO. LTD. PS3 gamepad", "030000008f0e00001431000010010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster 2 in 1 DT", "030000004f04000020b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Dual Analog 4", "030000004f04000015b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Dual Trigger 3-in-1", "030000004f04000023b3000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"ThrustMaster eSwap PRO Controller", "030000004f0400000ed0000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"Thrustmaster Firestorm Digital 2", "03000000b50700000399000000010000", 2, 2, 0, 0, 2, 4, 0, 0, 2, 3, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, },
        {"Thrustmaster Firestorm Dual Analog 2", "030000004f04000003b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 8, 0, 0, 2, 9, 0, 0, },
        {"Thrustmaster Firestorm Dual Power", "030000004f04000000b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Gamepad GP XID", "030000004f04000026b3000002040000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Thrustmaster GPX Gamepad", "03000000c6240000025b000002020000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Thrustmaster Run N Drive Wireless", "030000004f04000008d0000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster Run N Drive Wireless PS3", "030000004f04000009d0000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster T Mini Wireless", "030000004f04000007d0000000010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Thrustmaster vibrating gamepad", "030000004f04000012b3000010010000", 2, 0, 0, 0, 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 5, 0, 0, 2, 7, 0, 0, },
        {"Tomee SNES USB Controller", "03000000bd12000015d0000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 2, -1, 1, 1, 2, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Toodles 2008 Chimp PC/PS3", "03000000d814000007cd000011010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 2, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Torid", "030000005e0400008e02000070050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Torid", "03000000c01100000591000011010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Twin USB PS2 Adapter", "03000000100800000100000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"USB Gamepad", "03000000100800000300000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"USB gamepad", "03000000790000000600000007010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"USB Gamepad1", "03000000790000001100000000010000", 2, 2, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000302000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Victrix Pro Fight Stick for PS4", "030000006f0e00000702000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"VR-BOX", "05000000ac0500003232000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 2, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Wii Classic Controller", "03000000791d00000103000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 4, 0, 0, 2, 5, 0, 0, },
        {"Wireless HORIPAD Switch Pro Controller", "050000000d0f0000f600000001000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"X360 Controller", "030000005e0400008e02000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"X360 Controller", "030000005e0400008e02000014010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"X360 Wireless Controller", "030000005e0400001907000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"X360 Wireless Controller", "030000005e0400009102000007010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"X360 Wireless Controller", "030000005e040000a102000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"X360 Wireless Controller", "030000005e040000a102000007010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox 360 Wireless Controller", "0000000058626f782033363020576900", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 14, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox 360 Wireless Receiver (XBOX)", "030000005e040000a102000014010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 2, 13, 0, 0, 2, 12, 0, 0, 2, 14, 0, 0, 2, 11, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Gamepad (userspace driver)", "0000000058626f782047616d65706100", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox One Controller", "030000005e040000d102000002010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Controller", "050000005e040000fd02000030110000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Elite Series 2", "050000005e040000050b000002090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 136, 0, 0, 2, 11, 0, 0, 0, 0, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 6, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wireless Controller", "030000005e040000ea02000000000000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wireless Controller", "050000005e040000e002000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox One Wireless Controller", "050000005e040000fd02000003090000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 15, 0, 0, 2, 11, 0, 0, 2, 16, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox One Wireless Controller (Model 1708)", "030000005e040000ea02000001030000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Series Controller", "030000005e040000120b000001050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"Xbox Series Controller", "030000005e040000130b000005050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox Series Controller", "050000005e040000130b000001050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"Xbox Series Controller", "050000005e040000130b000005050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 1, 5, 1, 0, 1, 4, 1, 0, },
        {"XBox Series pad", "030000005e040000120b000005050000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"xbox360 Wireless EasySMX", "030000005e0400008e02000000010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 10, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, },
        {"XEOX Gamepad SL-6556-BK", "03000000450c00002043000010010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"Xiaoji Gamesir-G3w", "03000000ac0500005b05000010010000", 2, 2, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 0, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 3, 1, 0, 2, 6, 0, 0, 2, 7, 0, 0, },
        {"XiaoMi Game Controller", "05000000172700004431000029010000", 2, 0, 0, 0, 2, 1, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 2, 20, 0, 0, 2, 13, 0, 0, 2, 14, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 7, 1, 0, 1, 6, 1, 0, },
        {"Xin-Mo Xin-Mo Dual Arcade", "03000000c0160000e105000001010000", 2, 4, 0, 0, 2, 3, 0, 0, 2, 1, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 2, 5, 0, 0, 2, 6, 0, 0, 2, 7, 0, 0, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 11, 0, 0, 2, 14, 0, 0, 2, 12, 0, 0, 2, 13, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        {"ZEROPLUS P4 Gamepad", "03000000120c0000100e000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },
        {"ZEROPLUS P4 Wired Gamepad", "03000000120c0000101e000011010000", 2, 1, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0, 2, 3, 0, 0, 2, 4, 0, 0, 2, 5, 0, 0, 2, 8, 0, 0, 2, 9, 0, 0, 2, 12, 0, 0, 2, 10, 0, 0, 2, 11, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 3, 4, 0, 0, 3, 8, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 2, 1, 0, 1, 5, 1, 0, 1, 3, 1, 0, 1, 4, 1, 0, },

    };

    _sokol_.mappings = mappings;
    _sokol_.mappingCount = _countof(mappings);

    const char* dirname = "/dev/input";

    _sokol_.js.inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (_sokol_.js.inotify > 0)
    {
        // HACK: Register for IN_ATTRIB to get notified when udev is done
        //       This works well in practice but the true way is libudev

        _sokol_.js.watch = inotify_add_watch(_sokol_.js.inotify,
                                              dirname,
                                              IN_CREATE | IN_ATTRIB | IN_DELETE);
    }

    // Continue without device connection notifications if inotify fails

    _sokol_.js.regexCompiled = (regcomp(&_sokol_.js.regex, "^event[0-9]\\+$", 0) == 0);
    if (!_sokol_.js.regexCompiled)
    {
        _sokol_InputError(SOKOL_PLATFORM_ERROR, "Linux: Failed to compile regex");
        return false;
    }

    int count = 0;

    DIR* dir = opendir(dirname);
    if (dir)
    {
        struct dirent* entry;

        while ((entry = readdir(dir)))
        {
            regmatch_t match;

            if (regexec(&_sokol_.js.regex, entry->d_name, 1, &match, 0) != 0)
                continue;

            char path[PATH_MAX];

            snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

            if (openJoystickDevice(path))
                count++;
        }

        closedir(dir);
    }

    // Continue with no joysticks if enumeration fails

    qsort(_sokol_.joysticks, count, sizeof(_SOKOL_joystick), compareJoysticks);
    return true;
}

_SOKOL_PRIVATE bool _sokol_PollJoystick(_SOKOL_joystick* js, int mode)
{
    // Read all queued events (non-blocking)
    for (;;)
    {
        struct input_event e;

        errno = 0;
        if (read(js->js.fd, &e, sizeof(e)) < 0)
        {
            // Reset the joystick slot if the device was disconnected
            if (errno == ENODEV)
                closeJoystick(js);

            break;
        }

        if (e.type == EV_SYN)
        {
            if (e.code == SYN_DROPPED)
                _sokol_.js.dropped = true;
            else if (e.code == SYN_REPORT)
            {
                _sokol_.js.dropped = false;
                pollAbsState(js);
            }
        }

        if (_sokol_.js.dropped)
            continue;

        if (e.type == EV_KEY)
            handleKeyEvent(js, e.code, e.value);
        else if (e.type == EV_ABS)
            handleAbsEvent(js, e.code, e.value);
    }

    return js->connected;
}

_SOKOL_PRIVATE void _sokol_TerminateJoysticks()
{
    for (int jid = 0;  jid <= SOKOL_JOYSTICK_LAST;  jid++)
    {
        _SOKOL_joystick* js = _sokol_.joysticks + jid;
        if (js->connected)
            closeJoystick(js);
    }

    if (_sokol_.js.inotify > 0)
    {
        if (_sokol_.js.watch > 0)
            inotify_rm_watch(_sokol_.js.inotify, _sokol_.js.watch);

        close(_sokol_.js.inotify);
    }

    if (_sokol_.js.regexCompiled)
        regfree(&_sokol_.js.regex);

}

SOKOL_API_IMPL void sgamepad_init() 
{
    initJoysticks();
}

SOKOL_API_IMPL void sgamepad_shutdown() 
{
    _sokol_TerminateJoysticks();
}

#endif

#elif defined(_SAPP_EMSCRIPTEN)

#include <emscripten.h>

SOKOL_API_IMPL void sgamepad_init() {
}

SOKOL_API_IMPL void sgamepad_shutdown() {
}

_SOKOL_PRIVATE void _sgamepad_record_state() 
{
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS) 
        return;

    for (int i = 0; i < emscripten_get_num_gamepads() && i < SGAMEPAD_MAX_SUPPORTED_GAMEPADS; ++i) 
    {
        sgamepad_gamepad_state* target = _sgamepad.gamepad_states + i;

        EmscriptenGamepadEvent ge;
        if(emscripten_get_gamepad_status(i, &ge) == EMSCRIPTEN_RESULT_SUCCESS)
        {
            // Standart gamepad remapping: https://www.w3.org/TR/gamepad/#remapping
            if(ge.numButtons >= 4)
            {
                if(ge.digitalButton[0]) target->digital_inputs |= SGAMEPAD_BUTTON_A;
                if(ge.digitalButton[1]) target->digital_inputs |= SGAMEPAD_BUTTON_B;
                if(ge.digitalButton[2]) target->digital_inputs |= SGAMEPAD_BUTTON_X;
                if(ge.digitalButton[3]) target->digital_inputs |= SGAMEPAD_BUTTON_Y;

                if(ge.numButtons >= 6)
                {
                    target->left_shoulder = ge.analogButton[4];
                    target->right_shoulder = ge.analogButton[5];

                    if(ge.numButtons >= 8)
                    {
                        target->left_trigger = ge.analogButton[6];
                        target->right_trigger = ge.analogButton[7];

                        if(ge.numButtons >= 10)
                        {
                            if(ge.digitalButton[8]) target->digital_inputs |= SGAMEPAD_BUTTON_BACK;
                            if(ge.digitalButton[9]) target->digital_inputs |= SGAMEPAD_BUTTON_START;

                            if(ge.numButtons >= 12)
                            {
                                if(ge.digitalButton[10]) target->digital_inputs |= SGAMEPAD_BUTTON_LEFT_THUMB;
                                if(ge.digitalButton[11]) target->digital_inputs |= SGAMEPAD_BUTTON_RIGHT_THUMB;

                                if(ge.numButtons >= 16)
                                {
                                    if(ge.digitalButton[12]) target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_UP;
                                    if(ge.digitalButton[13]) target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_DOWN;
                                    if(ge.digitalButton[14]) target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_LEFT;
                                    if(ge.digitalButton[15]) target->digital_inputs |= SGAMEPAD_BUTTON_DPAD_RIGHT;
                                }
                            }
                        }
                    }
                }
            }

            if(ge.numAxes >= 2)
            {
                _sgamepad_generate_analog_stick_state(ge.axis[0], ge.axis[1], 1.0f, 0.01f, &(target->left_stick));

                if(ge.numAxes >= 4)
                {
                    _sgamepad_generate_analog_stick_state(ge.axis[2], ge.axis[3], 1.0f, 0.01f, &(target->right_stick));
                }
            }

        }
    }
}

/*== Fallback dummy Implementation ============================================*/
#else

SOKOL_API_IMPL void sgamepad_init() {
}

SOKOL_API_IMPL void sgamepad_shutdown() {
}

_SOKOL_PRIVATE void _sgamepad_record_state() {
}

#endif

/*=== PUBLIC API FUNCTIONS ===================================================*/
SOKOL_API_IMPL unsigned int sgamepad_get_max_supported_gamepads() {
    return SGAMEPAD_MAX_SUPPORTED_GAMEPADS;
}

SOKOL_API_IMPL void sgamepad_record_state() {
    memset(_sgamepad.gamepad_states, 0, sizeof(_sgamepad.gamepad_states));
    _sgamepad_record_state();
}

SOKOL_API_IMPL void sgamepad_get_gamepad_state(unsigned int index, sgamepad_gamepad_state* pstate) {
    if (index < SGAMEPAD_MAX_SUPPORTED_GAMEPADS) {
        *pstate = _sgamepad.gamepad_states[index];
    }
}

#endif /* SOKOL_IMPL */
