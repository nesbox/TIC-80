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

// USB HID host. A keyboard types into the editors, a mouse drives the pointer
// TIC-80 needs for half of them, and a gamepad plays.
//
// The badgeteam/usb-input component runs the USB host, tells keyboards, mice
// and gamepads apart and decodes their reports; what is here is the mapping
// onto tic_key and tic80_gamepad.
//
// Events arrive on the component's tasks. Keyboard and gamepad states are
// posted to a queue the main loop drains in tanmatsu_usb_hid_poll(), which
// keeps every field TIC-80 reads owned by one task. A mouse is the one
// exception: see the accumulator below for why it cannot go through a queue.

#include "usb_hid.h"

#include <string.h>

#include "bsp/power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "usb/hid_usage_keyboard.h"
#include "usb_input.h"

static char const TAG[] = "tic80-usb";

#define MAX_DEVICES USB_INPUT_MAX_DEVICES

#define EVENT_QUEUE_LENGTH 16

// Mice count in their own units and a modern one counts fast. TIC-80's screen
// is 256 pixels wide, so at 1:1 an inch of desk crosses it several times over.
#define MOUSE_DIVISOR 2

typedef enum
{
    EVENT_KEYBOARD = 0,
    EVENT_GAMEPAD,
    EVENT_CONNECTED,
    EVENT_DISCONNECTED,
} event_type_t;

typedef struct
{
    event_type_t type;
    uint8_t slot; // Which device it came from, for the per-device state
    usb_input_kind_t kind;
    uint8_t player; // Gamepads: which of TIC-80's four it is
    union
    {
        hid_keyboard_state_t keyboard;
        uint8_t buttons; // tic80_gamepad bits
    };
} event_t;

// Written by the component's tasks, read by the main loop through the queue.
static QueueHandle_t event_queue = NULL;

// Main loop side: what the last reports added up to. The keyboard state is
// kept per device rather than merged as it arrives: a keyboard that presents
// two interfaces reports the same key on one and nothing on the other, and
// shared state would see that as the key being pressed over and over.
static bool key_state[MAX_DEVICES][tic_keys_count];
static usb_input_kind_t slot_kind[MAX_DEVICES];

// One pad per player rather than one shared state: two pads reporting into the
// same byte would each undo the other's buttons on their next report.
static uint8_t gamepad_state[TANMATSU_USB_HID_MAX_GAMEPADS];

// A mouse does not go through the event queue. A gaming mouse reports at a
// kilohertz and the queue is drained once a frame, so a report per movement
// would both throw motion away when the queue filled and crowd the keyboard's
// events out of it. Movement adds up here instead and the main loop takes the
// total, which is what it wanted of the reports anyway.
// Movement from every mouse adds into one pointer, which is what two mice on
// one pointer should do. Buttons are per device and OR'd, so one mouse letting
// go of its button does not let go of the other's.
static portMUX_TYPE mouse_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t mouse_dx = 0;
static int32_t mouse_dy = 0;
static int32_t mouse_scroll = 0;
static uint8_t mouse_buttons[MAX_DEVICES];

static int32_t leftover_dx = 0; // The half pixels MOUSE_DIVISOR leaves behind
static int32_t leftover_dy = 0;

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

static tic_key usage_to_tic_key(uint8_t usage)
{
    if(usage >= HID_KEY_A && usage <= HID_KEY_Z)
    {
        return (tic_key)(tic_key_a + (usage - HID_KEY_A));
    }
    if(usage >= HID_KEY_1 && usage <= HID_KEY_9)
    {
        return (tic_key)(tic_key_1 + (usage - HID_KEY_1));
    }
    if(usage >= HID_KEY_F1 && usage <= HID_KEY_F12)
    {
        return (tic_key)(tic_key_f1 + (usage - HID_KEY_F1));
    }
    if(usage >= HID_KEY_KEYPAD_1 && usage <= HID_KEY_KEYPAD_9)
    {
        return (tic_key)(tic_key_numpad1 + (usage - HID_KEY_KEYPAD_1));
    }

    switch(usage)
    {
        case HID_KEY_0: return tic_key_0;
        case HID_KEY_ENTER: return tic_key_return;
        case HID_KEY_ESC: return tic_key_escape;
        case HID_KEY_DEL: return tic_key_backspace;
        case HID_KEY_TAB: return tic_key_tab;
        case HID_KEY_SPACE: return tic_key_space;
        case HID_KEY_MINUS: return tic_key_minus;
        case HID_KEY_EQUAL: return tic_key_equals;
        case HID_KEY_OPEN_BRACKET: return tic_key_leftbracket;
        case HID_KEY_CLOSE_BRACKET: return tic_key_rightbracket;
        case HID_KEY_BACK_SLASH:
        case HID_KEY_SHARP: return tic_key_backslash;
        case HID_KEY_COLON: return tic_key_semicolon;
        case HID_KEY_QUOTE: return tic_key_apostrophe;
        case HID_KEY_TILDE: return tic_key_grave;
        case HID_KEY_LESS: return tic_key_comma;
        case HID_KEY_GREATER: return tic_key_period;
        case HID_KEY_SLASH: return tic_key_slash;
        case HID_KEY_CAPS_LOCK: return tic_key_capslock;
        case HID_KEY_INSERT: return tic_key_insert;
        case HID_KEY_HOME: return tic_key_home;
        case HID_KEY_PAGEUP: return tic_key_pageup;
        case HID_KEY_DELETE: return tic_key_delete;
        case HID_KEY_END: return tic_key_end;
        case HID_KEY_PAGEDOWN: return tic_key_pagedown;
        case HID_KEY_RIGHT: return tic_key_right;
        case HID_KEY_LEFT: return tic_key_left;
        case HID_KEY_DOWN: return tic_key_down;
        case HID_KEY_UP: return tic_key_up;
        case HID_KEY_KEYPAD_DIV: return tic_key_numpaddivide;
        case HID_KEY_KEYPAD_MUL: return tic_key_numpadmultiply;
        case HID_KEY_KEYPAD_SUB: return tic_key_numpadminus;
        case HID_KEY_KEYPAD_ADD: return tic_key_numpadplus;
        case HID_KEY_KEYPAD_ENTER: return tic_key_numpadenter;
        case HID_KEY_KEYPAD_0: return tic_key_numpad0;
        case HID_KEY_KEYPAD_DELETE: return tic_key_numpadperiod;
        default: return tic_key_unknown;
    }
}

// A report says every key that is down, so the state is rebuilt from it
// outright rather than tracked press by press. hid-host has already turned
// the report into a set of usages, whichever shape the keyboard sent: the boot
// report's six slots or a bitmap with no limit on how many keys are held.
//
// The characters themselves are left to the studio, which works them out from
// the keys it is handed and repeats them at its own rate. Handing it text as
// well would be a second source of the same keystroke.
static void apply_keyboard(uint8_t slot, const hid_keyboard_state_t* keys)
{
    bool* state = key_state[slot];

    memset(state, 0, sizeof(key_state[slot]));

    state[tic_key_shift] = (keys->modifiers & (HID_KEYBOARD_LEFT_SHIFT | HID_KEYBOARD_RIGHT_SHIFT)) != 0;
    state[tic_key_ctrl] = (keys->modifiers & (HID_KEYBOARD_LEFT_CTRL | HID_KEYBOARD_RIGHT_CTRL)) != 0;
    state[tic_key_alt] = (keys->modifiers & (HID_KEYBOARD_LEFT_ALT | HID_KEYBOARD_RIGHT_ALT)) != 0;

    // The modifiers are in the set as usages too; they were read as a byte.
    uint16_t usage = 0;
    while(hid_keyboard_next_down(keys, &usage) && usage < HID_KEYBOARD_USAGE_FIRST_MODIFIER)
    {
        tic_key key = usage_to_tic_key((uint8_t)usage);
        if(key != tic_key_unknown)
        {
            state[key] = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Events, on the component's tasks
// ---------------------------------------------------------------------------

// TIC-80 has four buttons, and a pad names its own however it likes. The first
// four the descriptor numbers are the closest thing to a convention there is;
// a pad that disagrees still steers, it just labels oddly.
static uint8_t gamepad_buttons(const hid_gamepad_state_t* state)
{
    uint8_t buttons = 0;
    if(state->up)
    {
        buttons |= 1u << 0;
    }
    if(state->down)
    {
        buttons |= 1u << 1;
    }
    if(state->left)
    {
        buttons |= 1u << 2;
    }
    if(state->right)
    {
        buttons |= 1u << 3;
    }

    uint32_t named = state->usage_buttons != 0 ? state->usage_buttons : state->buttons;
    for(uint8_t i = 0; i < 4; i++)
    {
        if(named & (1u << i))
        {
            buttons |= (uint8_t)(1u << (4 + i));
        }
    }
    return buttons;
}

static void post(const event_t* event)
{
    if(event_queue != NULL)
    {
        xQueueSend(event_queue, event, 0);
    }
}

static void on_input(const usb_input_event_t* input, void* arg)
{
    (void)arg;

    if(input->device >= MAX_DEVICES)
    {
        return;
    }

    event_t event = {.slot = input->device, .kind = input->kind, .player = input->player};

    switch(input->type)
    {
        case USB_INPUT_EVENT_KEYBOARD:
            event.type = EVENT_KEYBOARD;
            event.keyboard = input->keyboard;
            post(&event);
            break;

        case USB_INPUT_EVENT_GAMEPAD:
            event.type = EVENT_GAMEPAD;
            event.buttons = gamepad_buttons(&input->gamepad);
            post(&event);
            break;

        // The buttons are a state, so the newest report wins; the movement is
        // a change, so every report counts and none may be dropped.
        case USB_INPUT_EVENT_MOUSE:
            portENTER_CRITICAL(&mouse_lock);
            mouse_dx += input->mouse.dx;
            mouse_dy += input->mouse.dy;
            mouse_scroll += input->mouse.wheel;
            mouse_buttons[input->device] = input->mouse.buttons & 0x07;
            portEXIT_CRITICAL(&mouse_lock);
            break;

        case USB_INPUT_EVENT_CONNECTED:
            event.type = EVENT_CONNECTED;
            post(&event);
            break;

        // Whatever a device was holding down when it was pulled out is not held
        // down any more.
        case USB_INPUT_EVENT_DISCONNECTED:
            portENTER_CRITICAL(&mouse_lock);
            mouse_buttons[input->device] = 0;
            portEXIT_CRITICAL(&mouse_lock);
            event.type = EVENT_DISCONNECTED;
            post(&event);
            break;
    }
}

// ---------------------------------------------------------------------------
// Start and stop
// ---------------------------------------------------------------------------

esp_err_t tanmatsu_usb_hid_init(void)
{
    memset(key_state, 0, sizeof(key_state));
    memset(slot_kind, 0, sizeof(slot_kind));
    memset(gamepad_state, 0, sizeof(gamepad_state));
    memset(mouse_buttons, 0, sizeof(mouse_buttons));

    // Nothing enumerates without power on the port, and the port has none
    // until the coprocessor turns the boost converter on.
    esp_err_t res = bsp_power_set_usb_host_boost_enabled(true);
    if(res != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to power the USB port: %s", esp_err_to_name(res));
        return res;
    }

    event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(event_t));
    if(event_queue == NULL)
    {
        ESP_LOGE(TAG, "Out of memory");
        tanmatsu_usb_hid_deinit();
        return ESP_ERR_NO_MEM;
    }

    // Core 0: core 1 runs the audio task, and the main loop wants what is left
    // of it. USB is not busy enough to mind sharing with the rest.
    usb_input_config_t config = USB_INPUT_CONFIG_DEFAULT();
    config.callback = on_input;
    config.core_id = 0;

    res = usb_input_start(&config);
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start the USB host: %s", esp_err_to_name(res));
        tanmatsu_usb_hid_deinit();
        return res;
    }

    ESP_LOGI(TAG, "USB HID host started");
    return ESP_OK;
}

void tanmatsu_usb_hid_deinit(void)
{
    usb_input_stop();

    // After the stop, so the disconnects it sends land in a queue that is
    // still there; nothing reads it again.
    if(event_queue != NULL)
    {
        QueueHandle_t events = event_queue;
        event_queue = NULL;
        vQueueDelete(events);
    }

    memset(slot_kind, 0, sizeof(slot_kind));

    // Leaving the boost converter on would keep draining the battery after
    // TIC-80 has handed the device back to the launcher.
    bsp_power_set_usb_host_boost_enabled(false);
}

// ---------------------------------------------------------------------------
// Main loop side
// ---------------------------------------------------------------------------

void tanmatsu_usb_hid_poll(tanmatsu_usb_hid_state_t* out)
{
    if(out == NULL)
    {
        return;
    }

    event_t event;
    while(event_queue != NULL && xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        switch(event.type)
        {
            case EVENT_KEYBOARD: apply_keyboard(event.slot, &event.keyboard); break;

            case EVENT_GAMEPAD:
                if(event.player < TANMATSU_USB_HID_MAX_GAMEPADS)
                {
                    gamepad_state[event.player] = event.buttons;
                }
                break;

            case EVENT_CONNECTED: slot_kind[event.slot] = event.kind; break;

            case EVENT_DISCONNECTED:
                slot_kind[event.slot] = USB_INPUT_KIND_NONE;
                memset(key_state[event.slot], 0, sizeof(key_state[event.slot]));
                if(event.kind == USB_INPUT_KIND_GAMEPAD && event.player < TANMATSU_USB_HID_MAX_GAMEPADS)
                {
                    gamepad_state[event.player] = 0;
                }
                break;
        }
    }

    memset(out->keys, 0, sizeof(out->keys));
    out->keyboard = out->mouse = out->gamepad = false;
    for(size_t i = 0; i < MAX_DEVICES; i++)
    {
        switch(slot_kind[i])
        {
            case USB_INPUT_KIND_KEYBOARD:
                out->keyboard = true;
                for(tic_key key = tic_key_unknown + 1; key < tic_keys_count; key++)
                {
                    if(key_state[i][key])
                    {
                        out->keys[key] = true;
                    }
                }
                break;
            case USB_INPUT_KIND_MOUSE: out->mouse = true; break;
            case USB_INPUT_KIND_GAMEPAD: out->gamepad = true; break;
            default: break;
        }
    }

    portENTER_CRITICAL(&mouse_lock);
    int32_t dx = mouse_dx;
    int32_t dy = mouse_dy;
    int32_t scroll = mouse_scroll;
    uint8_t buttons = 0;
    for(size_t i = 0; i < MAX_DEVICES; i++)
    {
        buttons |= mouse_buttons[i];
    }
    mouse_dx = 0;
    mouse_dy = 0;
    mouse_scroll = 0;
    portEXIT_CRITICAL(&mouse_lock);

    // Divide with the remainder carried over, so slow movement still gets
    // there rather than being rounded away one report at a time.
    dx += leftover_dx;
    dy += leftover_dy;
    out->mouse_dx = dx / MOUSE_DIVISOR;
    out->mouse_dy = dy / MOUSE_DIVISOR;
    leftover_dx = dx - out->mouse_dx * MOUSE_DIVISOR;
    leftover_dy = dy - out->mouse_dy * MOUSE_DIVISOR;

    out->mouse_scroll = scroll;

    out->mouse_left = (buttons & (1u << 0)) != 0;
    out->mouse_right = (buttons & (1u << 1)) != 0;
    out->mouse_middle = (buttons & (1u << 2)) != 0;

    memcpy(out->gamepad_buttons, gamepad_state, sizeof(out->gamepad_buttons));
}
