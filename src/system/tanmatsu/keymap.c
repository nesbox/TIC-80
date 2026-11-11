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

#include "tanmatsu.h"

#include <string.h>

#include "bsp/input.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "tic.h"
#include "usb_hid.h"

static char const TAG[] = "tic80-input";

// The Tanmatsu has a keyboard but no pointing device, and half of TIC-80 is
// mouse driven. Holding Fn turns the arrow keys into a pointer: Fn+arrows move
// it, Fn+left space clicks, Fn+right space right-clicks, Fn+shift+up/down
// scrolls, and Fn+middle space latches pointer mode so Fn need not be held.
// Fn+Esc leaves TIC-80.
//
// A USB keyboard, mouse or gamepad plugged into the port joins in: see
// usb_hid.c. A real mouse moves the pointer whether or not Fn is held.

#define TEXT_QUEUE_SIZE 32

// How many frames of held arrow before the pointer speeds up.
#define POINTER_RAMP_1 6
#define POINTER_RAMP_2 20

static QueueHandle_t event_queue = NULL;

static bool key_state[tic_keys_count];

static bool arrow_up = false;
static bool arrow_down = false;
static bool arrow_left = false;
static bool arrow_right = false;

static bool fn_held = false;
static bool shift_held = false;
static bool pointer_mode = false;
static bool quit = false;

static int32_t pointer_x = TIC80_FULLWIDTH / 2;
static int32_t pointer_y = TIC80_FULLHEIGHT / 2;
static int32_t pointer_frames = 0;
static bool pointer_left = false;
static bool pointer_right = false;
static int32_t pointer_scroll = 0;

// Codec volume, driven by the dedicated volume keys. TIC-80 has its own volume
// setting which it applies to the samples in software.
static int32_t codec_volume = 80;

static tanmatsu_usb_hid_state_t usb;

static char text_queue[TEXT_QUEUE_SIZE];
static size_t text_head = 0;
static size_t text_tail = 0;

static tic_key scancode_to_tic_key(uint32_t scancode)
{
    switch(scancode)
    {
        case BSP_INPUT_SCANCODE_A: return tic_key_a;
        case BSP_INPUT_SCANCODE_B: return tic_key_b;
        case BSP_INPUT_SCANCODE_C: return tic_key_c;
        case BSP_INPUT_SCANCODE_D: return tic_key_d;
        case BSP_INPUT_SCANCODE_E: return tic_key_e;
        case BSP_INPUT_SCANCODE_F: return tic_key_f;
        case BSP_INPUT_SCANCODE_G: return tic_key_g;
        case BSP_INPUT_SCANCODE_H: return tic_key_h;
        case BSP_INPUT_SCANCODE_I: return tic_key_i;
        case BSP_INPUT_SCANCODE_J: return tic_key_j;
        case BSP_INPUT_SCANCODE_K: return tic_key_k;
        case BSP_INPUT_SCANCODE_L: return tic_key_l;
        case BSP_INPUT_SCANCODE_M: return tic_key_m;
        case BSP_INPUT_SCANCODE_N: return tic_key_n;
        case BSP_INPUT_SCANCODE_O: return tic_key_o;
        case BSP_INPUT_SCANCODE_P: return tic_key_p;
        case BSP_INPUT_SCANCODE_Q: return tic_key_q;
        case BSP_INPUT_SCANCODE_R: return tic_key_r;
        case BSP_INPUT_SCANCODE_S: return tic_key_s;
        case BSP_INPUT_SCANCODE_T: return tic_key_t;
        case BSP_INPUT_SCANCODE_U: return tic_key_u;
        case BSP_INPUT_SCANCODE_V: return tic_key_v;
        case BSP_INPUT_SCANCODE_W: return tic_key_w;
        case BSP_INPUT_SCANCODE_X: return tic_key_x;
        case BSP_INPUT_SCANCODE_Y: return tic_key_y;
        case BSP_INPUT_SCANCODE_Z: return tic_key_z;

        case BSP_INPUT_SCANCODE_0: return tic_key_0;
        case BSP_INPUT_SCANCODE_1: return tic_key_1;
        case BSP_INPUT_SCANCODE_2: return tic_key_2;
        case BSP_INPUT_SCANCODE_3: return tic_key_3;
        case BSP_INPUT_SCANCODE_4: return tic_key_4;
        case BSP_INPUT_SCANCODE_5: return tic_key_5;
        case BSP_INPUT_SCANCODE_6: return tic_key_6;
        case BSP_INPUT_SCANCODE_7: return tic_key_7;
        case BSP_INPUT_SCANCODE_8: return tic_key_8;
        case BSP_INPUT_SCANCODE_9: return tic_key_9;

        case BSP_INPUT_SCANCODE_MINUS: return tic_key_minus;
        case BSP_INPUT_SCANCODE_EQUAL: return tic_key_equals;
        case BSP_INPUT_SCANCODE_LEFTBRACE: return tic_key_leftbracket;
        case BSP_INPUT_SCANCODE_RIGHTBRACE: return tic_key_rightbracket;
        case BSP_INPUT_SCANCODE_BACKSLASH: return tic_key_backslash;
        case BSP_INPUT_SCANCODE_SEMICOLON: return tic_key_semicolon;
        case BSP_INPUT_SCANCODE_APOSTROPHE: return tic_key_apostrophe;
        case BSP_INPUT_SCANCODE_GRAVE: return tic_key_grave;
        case BSP_INPUT_SCANCODE_COMMA: return tic_key_comma;
        case BSP_INPUT_SCANCODE_DOT: return tic_key_period;
        case BSP_INPUT_SCANCODE_SLASH: return tic_key_slash;

        case BSP_INPUT_SCANCODE_SPACE: return tic_key_space;
        case BSP_INPUT_SCANCODE_TAB: return tic_key_tab;
        case BSP_INPUT_SCANCODE_ENTER: return tic_key_return;
        case BSP_INPUT_SCANCODE_BACKSPACE: return tic_key_backspace;
        case BSP_INPUT_SCANCODE_ESC: return tic_key_escape;
        case BSP_INPUT_SCANCODE_CAPSLOCK: return tic_key_capslock;

        case BSP_INPUT_SCANCODE_LEFTCTRL:
        case BSP_INPUT_SCANCODE_ESCAPED_RCTRL:
            return tic_key_ctrl;

        case BSP_INPUT_SCANCODE_LEFTSHIFT:
        case BSP_INPUT_SCANCODE_RIGHTSHIFT:
            return tic_key_shift;

        case BSP_INPUT_SCANCODE_LEFTALT:
        case BSP_INPUT_SCANCODE_ESCAPED_RALT:
            return tic_key_alt;

        case BSP_INPUT_SCANCODE_F1: return tic_key_f1;
        case BSP_INPUT_SCANCODE_F2: return tic_key_f2;
        case BSP_INPUT_SCANCODE_F3: return tic_key_f3;
        case BSP_INPUT_SCANCODE_F4: return tic_key_f4;
        case BSP_INPUT_SCANCODE_F5: return tic_key_f5;
        case BSP_INPUT_SCANCODE_F6: return tic_key_f6;
        case BSP_INPUT_SCANCODE_F7: return tic_key_f7;
        case BSP_INPUT_SCANCODE_F8: return tic_key_f8;
        case BSP_INPUT_SCANCODE_F9: return tic_key_f9;
        case BSP_INPUT_SCANCODE_F10: return tic_key_f10;
        case BSP_INPUT_SCANCODE_F11: return tic_key_f11;
        case BSP_INPUT_SCANCODE_F12: return tic_key_f12;

        case BSP_INPUT_SCANCODE_ESCAPED_GREY_DEL: return tic_key_delete;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_INSERT: return tic_key_insert;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_HOME: return tic_key_home;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_END: return tic_key_end;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_PGUP: return tic_key_pageup;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_PGDN: return tic_key_pagedown;

        default: return tic_key_unknown;
    }
}

static void push_text(char c)
{
    size_t next = (text_head + 1) % TEXT_QUEUE_SIZE;
    if(next == text_tail)
    {
        return; // Full; dropping a keystroke beats blocking the input task.
    }
    text_queue[text_head] = c;
    text_head = next;
}

static bool in_pointer_mode(void)
{
    return fn_held || pointer_mode;
}

static void handle_scancode(uint32_t raw)
{
    bool pressed = (raw & BSP_INPUT_SCANCODE_RELEASE_MODIFIER) == 0;
    uint32_t scancode = raw & ~((uint32_t)BSP_INPUT_SCANCODE_RELEASE_MODIFIER);

    switch(scancode)
    {
        case BSP_INPUT_SCANCODE_FN:
            fn_held = pressed;
            return;
        case BSP_INPUT_SCANCODE_LEFTSHIFT:
        case BSP_INPUT_SCANCODE_RIGHTSHIFT:
            shift_held = pressed;
            break;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_UP:
            arrow_up = pressed;
            break;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_DOWN:
            arrow_down = pressed;
            break;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_LEFT:
            arrow_left = pressed;
            break;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_RIGHT:
            arrow_right = pressed;
            break;
        case BSP_INPUT_SCANCODE_ESC:
            if(pressed && fn_held)
            {
                quit = true;
            }
            break;
        default:
            break;
    }

    tic_key key = scancode_to_tic_key(scancode);
    if(key != tic_key_unknown)
    {
        key_state[key] = pressed;
    }
}

static void handle_navigation(const bsp_input_event_args_navigation_t* nav)
{
    switch(nav->key)
    {
        case BSP_INPUT_NAVIGATION_KEY_SPACE_L:
            if(in_pointer_mode())
            {
                pointer_left = nav->state;
            }
            else
            {
                key_state[tic_key_space] = nav->state;
            }
            break;
        case BSP_INPUT_NAVIGATION_KEY_SPACE_R:
            if(in_pointer_mode())
            {
                pointer_right = nav->state;
            }
            else
            {
                key_state[tic_key_space] = nav->state;
            }
            break;
        case BSP_INPUT_NAVIGATION_KEY_VOLUME_UP:
            if(nav->state)
            {
                codec_volume += 5;
                if(codec_volume > 100)
                {
                    codec_volume = 100;
                }
                tanmatsu_audio_set_volume(codec_volume);
            }
            break;
        case BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN:
            if(nav->state)
            {
                codec_volume -= 5;
                if(codec_volume < 0)
                {
                    codec_volume = 0;
                }
                tanmatsu_audio_set_volume(codec_volume);
            }
            break;
        case BSP_INPUT_NAVIGATION_KEY_SPACE_M:
            if(nav->state && fn_held)
            {
                pointer_mode = !pointer_mode;
                ESP_LOGI(TAG, "Pointer mode %s", pointer_mode ? "latched" : "released");
            }
            else
            {
                key_state[tic_key_space] = nav->state;
            }
            break;
        // The arrow, escape and function keys already arrive as scancodes;
        // taking them here as well would double up.
        default:
            break;
    }
}

static void clamp_pointer(void)
{
    if(pointer_x < 0)
    {
        pointer_x = 0;
    }
    if(pointer_y < 0)
    {
        pointer_y = 0;
    }
    if(pointer_x > TIC80_FULLWIDTH - 1)
    {
        pointer_x = TIC80_FULLWIDTH - 1;
    }
    if(pointer_y > TIC80_FULLHEIGHT - 1)
    {
        pointer_y = TIC80_FULLHEIGHT - 1;
    }
}

static void update_pointer(void)
{
    pointer_scroll = 0;

    if(!in_pointer_mode())
    {
        pointer_frames = 0;
        pointer_left = false;
        pointer_right = false;
        return;
    }

    int32_t dx = (arrow_right ? 1 : 0) - (arrow_left ? 1 : 0);
    int32_t dy = (arrow_down ? 1 : 0) - (arrow_up ? 1 : 0);

    if(dx == 0 && dy == 0)
    {
        pointer_frames = 0;
        return;
    }

    if(shift_held)
    {
        // Vertical movement turns into wheel clicks while shift is down.
        pointer_scroll = -dy;
        return;
    }

    pointer_frames++;
    int32_t speed = 1;
    if(pointer_frames > POINTER_RAMP_2)
    {
        speed = 4;
    }
    else if(pointer_frames > POINTER_RAMP_1)
    {
        speed = 2;
    }

    pointer_x += dx * speed;
    pointer_y += dy * speed;

    clamp_pointer();
}

esp_err_t tanmatsu_input_init(void)
{
    esp_err_t res = bsp_input_get_queue(&event_queue);
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get the input queue: %s", esp_err_to_name(res));
        return res;
    }

    memset(key_state, 0, sizeof(key_state));

    // A device on the USB port is a bonus, not a requirement: the built-in
    // keyboard is what the port is built around.
    res = tanmatsu_usb_hid_init();
    if(res != ESP_OK)
    {
        ESP_LOGW(TAG, "Continuing without USB HID: %s", esp_err_to_name(res));
    }

    return ESP_OK;
}

void tanmatsu_input_poll(tic80_input* input)
{
    if(input == NULL)
    {
        return;
    }

    bsp_input_event_t event;
    while(event_queue != NULL && xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        switch(event.type)
        {
            case INPUT_EVENT_TYPE_SCANCODE:
                handle_scancode((uint32_t)event.args_scancode.scancode);
                break;
            case INPUT_EVENT_TYPE_NAVIGATION:
                handle_navigation(&event.args_navigation);
                break;
            case INPUT_EVENT_TYPE_KEYBOARD:
                if(event.args_keyboard.ascii >= ' ' && event.args_keyboard.ascii < 0x7F)
                {
                    push_text(event.args_keyboard.ascii);
                }
                break;
            case INPUT_EVENT_TYPE_ACTION:
                if(event.args_action.type == BSP_INPUT_ACTION_TYPE_POWER_BUTTON && event.args_action.state)
                {
                    quit = true;
                }
                break;
            default:
                break;
        }
    }

    update_pointer();

    tanmatsu_usb_hid_poll(&usb);

    // A mouse of its own moves the pointer whatever the arrow keys are doing.
    if(usb.mouse)
    {
        pointer_x += usb.mouse_dx;
        pointer_y += usb.mouse_dy;
        clamp_pointer();
    }

    // The arrows drive either the cursor or the game, never both at once.
    bool arrows_to_keys = !in_pointer_mode();
    key_state[tic_key_up] = arrows_to_keys && arrow_up;
    key_state[tic_key_down] = arrows_to_keys && arrow_down;
    key_state[tic_key_left] = arrows_to_keys && arrow_left;
    key_state[tic_key_right] = arrows_to_keys && arrow_right;

    // A USB keyboard adds to the built-in one rather than replacing it. The two
    // are merged here rather than into key_state, which only the scancodes own:
    // a key set there by USB would never be cleared, since only a release
    // scancode clears it, and the studio would repeat it forever.
    memset(&input->keyboard, 0, sizeof(input->keyboard));
    size_t slot = 0;
    for(tic_key key = tic_key_unknown + 1; key < tic_keys_count && slot < TIC80_KEY_BUFFER; key++)
    {
        if(key_state[key] || usb.keys[key])
        {
            input->keyboard.keys[slot++] = key;
        }
    }

    input->mouse.x = (uint8_t)pointer_x;
    input->mouse.y = (uint8_t)pointer_y;
    input->mouse.left = (pointer_left || usb.mouse_left) ? 1 : 0;
    input->mouse.middle = usb.mouse_middle ? 1 : 0;
    input->mouse.right = (pointer_right || usb.mouse_right) ? 1 : 0;
    input->mouse.scrollx = 0;
    input->mouse.scrolly = pointer_scroll + usb.mouse_scroll;
    input->mouse.relative = 0;

    input->gamepads.first.data = usb.gamepad_buttons[0];
    input->gamepads.second.data = usb.gamepad_buttons[1];
    input->gamepads.third.data = usb.gamepad_buttons[2];
    input->gamepads.fourth.data = usb.gamepad_buttons[3];
}

bool tanmatsu_input_text(char* out)
{
    if(text_head == text_tail)
    {
        return false;
    }

    *out = text_queue[text_tail];
    text_tail = (text_tail + 1) % TEXT_QUEUE_SIZE;
    return true;
}

bool tanmatsu_input_quit(void)
{
    return quit;
}
