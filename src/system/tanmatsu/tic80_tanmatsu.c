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
#include "usb_hid.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "appfs.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "studio/system.h"

static char const TAG[] = "tic80";

// Prints where each frame goes, every couple of seconds. Cheap enough to leave
// on: two timer reads per stage.
// Set to 1 to log where each frame goes. Off by default: it prints every
// couple of seconds, which is noise once the numbers are known.
#ifndef TIC80_TANMATSU_PROFILE
#define TIC80_TANMATSU_PROFILE 0
#endif

#define PROFILE_FRAMES 120

static struct
{
    Studio* studio;
    tic80_input input;
    char* clipboard;
    bool audio;
} platform;

// ---------------------------------------------------------------------------
// System hooks the studio expects every port to provide
// ---------------------------------------------------------------------------

void tic_sys_clipboard_set(const char* text)
{
    if(platform.clipboard)
    {
        free(platform.clipboard);
        platform.clipboard = NULL;
    }

    if(text)
    {
        platform.clipboard = strdup(text);
    }
}

bool tic_sys_clipboard_has()
{
    return platform.clipboard != NULL;
}

char* tic_sys_clipboard_get()
{
    return platform.clipboard ? strdup(platform.clipboard) : NULL;
}

void tic_sys_clipboard_free(const char* text)
{
    free((void*)text);
}

u64 tic_sys_counter_get()
{
    return (u64)esp_timer_get_time();
}

u64 tic_sys_freq_get()
{
    return 1000000;
}

bool tic_sys_fullscreen_get()
{
    // The panel is the whole screen and nothing else is drawing on it.
    return true;
}

void tic_sys_fullscreen_set(bool value)
{
    (void)value;
}

void tic_sys_message(const char* title, const char* message)
{
    ESP_LOGI(TAG, "%s: %s", title, message);
}

void tic_sys_title(const char* title)
{
    ESP_LOGI(TAG, "%s", title);
}

void tic_sys_open_path(const char* path)
{
    (void)path;
}

void tic_sys_open_url(const char* url)
{
    (void)url;
}

void tic_sys_preseed()
{
    srand((unsigned int)esp_timer_get_time());
    rand();
}

bool tic_sys_keyboard_text(char* text)
{
    return tanmatsu_input_text(text);
}

void tic_sys_update_config()
{
    // studio_sound() already scales the samples by the configured volume, so
    // the codec is left where the volume keys put it.
}

void tic_sys_default_mapping(tic_mapping* mapping)
{
    static const tic_key keys[] = {
        tic_key_up,
        tic_key_down,
        tic_key_left,
        tic_key_right,
        tic_key_z,
        tic_key_x,
        tic_key_a,
        tic_key_s,
    };

    for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        mapping->data[i] = keys[i];
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

static esp_err_t initialize_hardware(void)
{
    esp_err_t res = nvs_flash_init();
    if(res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(res));
        return res;
    }

    bsp_configuration_t configuration = {
        .display =
            {
                .requested_color_format = BSP_DISPLAY_COLOR_FORMAT_16_565RGB,
                // Two, so the next frame can be drawn off screen and swapped
                // in. See display.c.
                .num_fbs = 2,
            },
    };

    res = bsp_device_initialize(&configuration);
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize the BSP: %s", esp_err_to_name(res));
        return res;
    }

    return ESP_OK;
}

// The cart the launcher started TIC-80 with, or NULL.
//
// A repository app of type "script" names TIC-80 as its interpreter and a
// cart as its executable. The launcher then boots TIC-80 with the cart's full
// path as the AppFS boot argument, the same way Graceloader receives its
// apps. A plain launch passes the app's own directory instead, so only a
// regular file counts as a cart. The studio exits on a cart it cannot load,
// which here would be a crash, so the file has to exist before it is named.
static const char* launched_cart(void)
{
    static char path[APPFS_MAX_ARG_LENGTH + 1];
    char const* arg = NULL;

    if(appfsBootselGet(NULL, &arg) == APPFS_INVALID_FD || arg == NULL || arg[0] == '\0')
    {
        return NULL;
    }

    struct stat st;
    if(stat(arg, &st) != 0 || !S_ISREG(st.st_mode))
    {
        return NULL;
    }

    snprintf(path, sizeof(path), "%s", arg);
    return path;
}

void tic80_tanmatsu_main(void)
{
    if(initialize_hardware() != ESP_OK)
    {
        return;
    }

    ESP_ERROR_CHECK(tanmatsu_display_init());
    ESP_ERROR_CHECK(tanmatsu_input_init());

    if(tanmatsu_storage_init() != ESP_OK)
    {
        ESP_LOGW(TAG, "Continuing without persistent storage");
    }

    platform.audio = tanmatsu_audio_init() == ESP_OK;
    if(!platform.audio)
    {
        ESP_LOGW(TAG, "Continuing without sound");
    }

    const char* cart = launched_cart();
    char* argv[] = {"tic80", (char*)cart, NULL};
    const char* root = tanmatsu_storage_root();

    if(cart != NULL)
    {
        ESP_LOGI(TAG, "Launched with cart %s", cart);
    }

    platform.studio = studio_create(cart != NULL ? 2 : 1, argv, TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_BGRA8888, root, INT32_MAX,
                                    tic_layout_qwerty);
    if(platform.studio == NULL)
    {
        ESP_LOGE(TAG, "Failed to create the studio");
        return;
    }

    const tic80* product = &studio_mem(platform.studio)->product;

    ESP_LOGI(TAG, "TIC-80 running, Fn+Esc to leave");

    TickType_t next_frame = xTaskGetTickCount();

#if TIC80_TANMATSU_PROFILE
    uint64_t spent_input = 0, spent_tick = 0, spent_audio = 0, spent_present = 0;
    uint32_t frames = 0;
    int64_t window = esp_timer_get_time();
#endif

    while(!studio_alive(platform.studio) && !tanmatsu_input_quit())
    {
#if TIC80_TANMATSU_PROFILE
        int64_t mark = esp_timer_get_time();
#define PROFILE_LAP(counter)                 \
    do                                       \
    {                                        \
        int64_t now = esp_timer_get_time();  \
        (counter) += (uint64_t)(now - mark); \
        mark = now;                          \
    } while(0)
#else
#define PROFILE_LAP(counter) \
    do                       \
    {                        \
    } while(0)
#endif

        tanmatsu_input_poll(&platform.input);
        PROFILE_LAP(spent_input);

        studio_tick(platform.studio, platform.input);
        studio_sound(platform.studio);
        PROFILE_LAP(spent_tick);

        if(platform.audio)
        {
            // Writing a frame of audio blocks until the codec has room, which
            // is what keeps the loop at 60 Hz.
            tanmatsu_audio_write(product->samples.buffer, product->samples.count);
        }
        else
        {
            vTaskDelayUntil(&next_frame, pdMS_TO_TICKS(1000 / TIC80_FRAMERATE));
        }
        PROFILE_LAP(spent_audio);

        tanmatsu_display_present(product->screen);
        PROFILE_LAP(spent_present);

#if TIC80_TANMATSU_PROFILE
        if(++frames >= PROFILE_FRAMES)
        {
            int64_t elapsed = esp_timer_get_time() - window;
            ESP_LOGI(TAG,
                     "%u frames in %lld ms (%lld fps) | per frame: input %lluus tick %lluus audio %lluus "
                     "present %lluus",
                     (unsigned)frames, elapsed / 1000, (long long)(frames * 1000000LL / (elapsed ? elapsed : 1)),
                     (unsigned long long)(spent_input / frames), (unsigned long long)(spent_tick / frames),
                     (unsigned long long)(spent_audio / frames), (unsigned long long)(spent_present / frames));

            spent_input = spent_tick = spent_audio = spent_present = 0;
            frames = 0;
            window = esp_timer_get_time();
        }
#endif
    }

    ESP_LOGI(TAG, "Shutting down");

    studio_delete(platform.studio);
    platform.studio = NULL;

    tanmatsu_usb_hid_deinit();

    bsp_device_restart_to_launcher();
}
