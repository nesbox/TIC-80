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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "tic80.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Entry point, called from the project's main component.
    void tic80_tanmatsu_main(void);

    // ---------------------------------------------------------------------------
    // Display: scales the 256x144 TIC-80 output 3x into the middle of the panel.
    // ---------------------------------------------------------------------------

    esp_err_t tanmatsu_display_init(void);

    // screen points at TIC80_FULLWIDTH * TIC80_FULLHEIGHT pixels in
    // TIC80_PIXEL_COLOR_RGBA8888 format.
    void tanmatsu_display_present(const uint32_t* screen);

    // ---------------------------------------------------------------------------
    // Audio: 44100 Hz, 16 bit, stereo, interleaved, straight to the I2S codec.
    // ---------------------------------------------------------------------------

    esp_err_t tanmatsu_audio_init(void);

    // count is the number of samples (not frames, not bytes). Blocks until the
    // codec has taken them, which is what paces the main loop.
    void tanmatsu_audio_write(const int16_t* samples, size_t count);

    void tanmatsu_audio_set_volume(int32_t percent);

    // ---------------------------------------------------------------------------
    // Input: the built-in keyboard, plus a mouse pointer driven with Fn + arrows.
    // ---------------------------------------------------------------------------

    esp_err_t tanmatsu_input_init(void);

    // Drains the BSP event queue and updates input in place.
    void tanmatsu_input_poll(tic80_input* input);

    // Pops one typed character, for tic_sys_keyboard_text().
    bool tanmatsu_input_text(char* out);

    // True once the user asked to leave (Fn+Esc, or the power button).
    bool tanmatsu_input_quit(void);

    // ---------------------------------------------------------------------------
    // Storage: mounts the internal FAT partition and, when present, the SD card.
    // ---------------------------------------------------------------------------

    esp_err_t tanmatsu_storage_init(void);

    // Directory TIC-80 keeps its carts and config in, with a trailing separator
    // left off. Lives on the SD card when one is mounted, on internal flash if not.
    const char* tanmatsu_storage_root(void);

#ifdef __cplusplus
}
#endif
