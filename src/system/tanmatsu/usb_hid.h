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

// USB HID host: a keyboard, a mouse or a gamepad plugged into the USB-C port,
// on top of the keyboard the Tanmatsu already has. Only used by keymap.c, so
// it is not part of tanmatsu.h.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "tic.h"

// TIC-80 plays four, and the port has room for no more devices than that.
#define TANMATSU_USB_HID_MAX_GAMEPADS 4

// What the plugged-in devices have reported since the last poll. Movement and
// scrolling are deltas; everything else is the state as it stands.
typedef struct
{
    bool keyboard; // A keyboard is plugged in
    bool mouse;    // A mouse is plugged in
    bool gamepad;  // A gamepad is plugged in

    bool keys[tic_keys_count];

    int32_t mouse_dx;
    int32_t mouse_dy;
    int32_t mouse_scroll;
    bool mouse_left;
    bool mouse_middle;
    bool mouse_right;

    // The tic80_gamepad bits - up, down, left, right, a, b, x, y - one entry
    // per pad, in the order they were plugged in. A pad keeps its player for
    // as long as it stays plugged in, so unplugging the first one does not
    // shuffle the rest along.
    uint8_t gamepad_buttons[TANMATSU_USB_HID_MAX_GAMEPADS];
} tanmatsu_usb_hid_state_t;

// Powers the USB port and starts the host stack. Failing is not fatal: the
// built-in keyboard keeps working, so this only logs and returns the error.
esp_err_t tanmatsu_usb_hid_init(void);

// Stops the host stack and cuts power to the port again, on the way out. Safe
// to call on a failed or partial init, which is what the error paths do.
void tanmatsu_usb_hid_deinit(void);

// Folds everything the devices reported since the last call into *out.
void tanmatsu_usb_hid_poll(tanmatsu_usb_hid_state_t* out);
