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

#include "bsp/display.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "driver/ppa.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"

static char const TAG[] = "tic80-display";

// The Tanmatsu panel is a 480x800 portrait ST7701 that the BSP reports with a
// default rotation of 270 degrees, so the landscape picture people see is
// turned a quarter turn in panel memory. TIC-80 draws 256x144 pixels including
// the border; that is scaled by a whole number of pixels, rotated to match the
// panel, and centred, with the rest of the panel left black.
//
// Nearest-neighbour on purpose: TIC-80 is a pixel machine and interpolation
// would only smear it.
// The panel is a DPI one: the driver owns a framebuffer that the DSI scans out
// continuously, and esp_lcd_panel_draw_bitmap() copies into it. Writing that
// framebuffer directly saves a 663 KiB copy in and 663 KiB out of PSRAM every
// frame, which was most of the cost of putting a frame on screen.
//
// panel_fb is the driver's buffer when we got hold of it. framebuffer is the
// fallback buffer used with bsp_display_blit() when we could not.
static uint16_t* panel_fb = NULL;
static uint16_t* panel_fb_back = NULL;
static uint16_t* framebuffer = NULL;

// With two framebuffers the PPA draws into whichever one is not on screen, and
// handing that buffer back to the driver switches to it rather than copying it:
// esp_lcd_panel_dpi.c takes a no-copy path when the buffer it is given lies
// inside one of its own framebuffers. Costs a second framebuffer's worth of
// PSRAM and removes the tearing.
static esp_lcd_panel_handle_t panel_handle = NULL;

// Distance between two panel rows, in pixels. The whole panel width when
// drawing into the driver's framebuffer, only the drawn area otherwise.
static size_t stride = 0;

static size_t panel_width = 0;
static size_t panel_height = 0;
static size_t scale = 1;

// Size and position of the drawn area, in panel coordinates.
static size_t blit_width = 0;
static size_t blit_height = 0;
static size_t offset_x = 0;
static size_t offset_y = 0;

// How a TIC-80 pixel lands in panel memory. One TIC-80 row or column always
// becomes one panel row, which is what makes the scaling a memcpy.
static bool transpose = false;
static bool flip_outer = false;
static bool flip_inner = false;

static bool swap_bytes = false;

// The P4's Pixel Processing Accelerator does the scale and the rotation in
// hardware, straight from TIC-80's buffer into the panel's, so the CPU does no
// per-pixel work at all. The software path below stays as the fallback.
static ppa_client_handle_t ppa_client = NULL;
static ppa_srm_rotation_angle_t ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;

// Height of the scratch band used to paint the letterbox once at startup.
#define CLEAR_BAND_HEIGHT 40

static inline uint16_t to_rgb565(uint32_t pixel)
{
    // TIC80_PIXEL_COLOR_BGRA8888 lays the channels out as B,G,R,A in memory,
    // which on this little-endian core reads back as 0xAARRGGBB. That is
    // exactly what the PPA calls ARGB8888, which is why the format was chosen.
    uint32_t b = (pixel >> 0) & 0xFF;
    uint32_t g = (pixel >> 8) & 0xFF;
    uint32_t r = (pixel >> 16) & 0xFF;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void clear_panel(void)
{
    size_t band_pixels = panel_width * CLEAR_BAND_HEIGHT;
    uint16_t* band = heap_caps_calloc(band_pixels, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if(band == NULL)
    {
        ESP_LOGW(TAG, "No memory for the letterbox scratch buffer, skipping the clear");
        return;
    }

    for(size_t y = 0; y < panel_height; y += CLEAR_BAND_HEIGHT)
    {
        size_t rows = panel_height - y;
        if(rows > CLEAR_BAND_HEIGHT)
        {
            rows = CLEAR_BAND_HEIGHT;
        }
        bsp_display_blit(0, y, panel_width, y + rows, band);
    }

    heap_caps_free(band);
}

esp_err_t tanmatsu_display_init(void)
{
    bsp_display_color_format_t color_format = 0;
    bsp_display_endianness_t endianness = BSP_DISPLAY_ENDIAN_LITTLE;

    esp_err_t res = bsp_display_get_parameters(&panel_width, &panel_height, &color_format, &endianness);
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read the display parameters: %s", esp_err_to_name(res));
        return res;
    }

    if(color_format != BSP_DISPLAY_COLOR_FORMAT_16_565RGB)
    {
        ESP_LOGE(TAG, "Unsupported display colour format %d, this port needs RGB565", (int)color_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    swap_bytes = (endianness == BSP_DISPLAY_ENDIAN_BIG);

    switch(bsp_display_get_default_rotation())
    {
        case BSP_DISPLAY_ROTATION_90: // Counter-clockwise
            transpose = true;
            flip_outer = true;
            flip_inner = false;
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
            break;
        case BSP_DISPLAY_ROTATION_180:
            transpose = false;
            flip_outer = true;
            flip_inner = true;
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
            break;
        case BSP_DISPLAY_ROTATION_270: // Clockwise, which is what the Tanmatsu uses
            transpose = true;
            flip_outer = false;
            flip_inner = true;
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
            break;
        case BSP_DISPLAY_ROTATION_0:
        default:
            transpose = false;
            flip_outer = false;
            flip_inner = false;
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
            break;
    }

    // Rotating by a quarter turn swaps which TIC-80 axis has to fit which
    // panel axis.
    size_t needed_width = transpose ? TIC80_FULLHEIGHT : TIC80_FULLWIDTH;
    size_t needed_height = transpose ? TIC80_FULLWIDTH : TIC80_FULLHEIGHT;

    size_t scale_x = panel_width / needed_width;
    size_t scale_y = panel_height / needed_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if(scale < 1)
    {
        ESP_LOGE(TAG, "Display %ux%u is smaller than TIC-80's %ux%u output", (unsigned)panel_width,
                 (unsigned)panel_height, (unsigned)needed_width, (unsigned)needed_height);
        return ESP_ERR_NOT_SUPPORTED;
    }

    blit_width = needed_width * scale;
    blit_height = needed_height * scale;
    offset_x = (panel_width - blit_width) / 2;
    offset_y = (panel_height - blit_height) / 2;

    if(bsp_display_get_panel(&panel_handle) == ESP_OK &&
       esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, (void**)&panel_fb, (void**)&panel_fb_back) == ESP_OK &&
       panel_fb != NULL)
    {
        // Drawing into the driver's own framebuffers. The letterbox only has to
        // be painted once per buffer, since nothing else writes here.
        stride = panel_width;
        size_t buffer_bytes = panel_width * panel_height * sizeof(uint16_t);

        memset(panel_fb, 0, buffer_bytes);
        esp_cache_msync(panel_fb, buffer_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        if(panel_fb_back != NULL)
        {
            memset(panel_fb_back, 0, buffer_bytes);
            esp_cache_msync(panel_fb_back, buffer_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        }

        ppa_client_config_t ppa_config = {
            .oper_type = PPA_OPERATION_SRM,
            .max_pending_trans_num = 1,
        };
        if(ppa_register_client(&ppa_config, &ppa_client) != ESP_OK)
        {
            ESP_LOGW(TAG, "No PPA client, scaling and rotating on the CPU instead");
            ppa_client = NULL;
        }
    }
    else
    {
        ESP_LOGW(TAG, "No direct framebuffer, falling back to blitting");
        panel_fb = NULL;
        stride = blit_width;
        framebuffer = heap_caps_aligned_calloc(64, blit_width * blit_height, sizeof(uint16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(framebuffer == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate the %ux%u framebuffer", (unsigned)blit_width, (unsigned)blit_height);
            return ESP_ERR_NO_MEM;
        }
        clear_panel();
    }

    ESP_LOGI(TAG, "Panel %ux%u, TIC-80 at %ux scale as %ux%u at %u,%u%s, %s", (unsigned)panel_width,
             (unsigned)panel_height, (unsigned)scale, (unsigned)blit_width, (unsigned)blit_height, (unsigned)offset_x,
             (unsigned)offset_y, transpose ? ", rotated a quarter turn" : "",
             ppa_client ? "scaled and rotated by the PPA into the panel framebuffer"
             : panel_fb ? "drawn into the panel framebuffer by the CPU"
                        : "blitted from a scratch buffer");

    return ESP_OK;
}

// Hands a framebuffer back to the driver. Because the buffer is one of its own,
// esp_lcd_panel_dpi.c writes the cache back and switches to it instead of
// copying, so this is the swap. With a single buffer there is nothing to switch
// to and it is only the cache write back.
static void swap_buffers(uint16_t* drawn)
{
    bsp_display_blit(0, offset_y, panel_width, offset_y + blit_height, drawn);

    if(panel_fb_back != NULL)
    {
        uint16_t* previous = panel_fb;
        panel_fb = panel_fb_back;
        panel_fb_back = previous;
    }
}

void tanmatsu_display_present(const uint32_t* screen)
{
    if(screen == NULL)
    {
        return;
    }

    // With two buffers the next frame is drawn off screen and swapped in; with
    // one there is nowhere else to draw, so it goes straight to the visible one.
    uint16_t* draw_fb = panel_fb_back != NULL ? panel_fb_back : panel_fb;

    if(ppa_client != NULL)
    {
        // TIC-80's buffer is BGRA8888 in memory, which is what the PPA calls
        // ARGB8888, so it is fed to the hardware untouched. The driver takes
        // care of flushing it out of the cache and invalidating the result.
        ppa_srm_oper_config_t operation = {
            .in =
                {
                    .buffer = screen,
                    .pic_w = TIC80_FULLWIDTH,
                    .pic_h = TIC80_FULLHEIGHT,
                    .block_w = TIC80_FULLWIDTH,
                    .block_h = TIC80_FULLHEIGHT,
                    .block_offset_x = 0,
                    .block_offset_y = 0,
                    .srm_cm = PPA_SRM_COLOR_MODE_ARGB8888,
                },
            .out =
                {
                    .buffer = draw_fb,
                    .buffer_size = panel_width * panel_height * sizeof(uint16_t),
                    .pic_w = panel_width,
                    .pic_h = panel_height,
                    .block_offset_x = offset_x,
                    .block_offset_y = offset_y,
                    .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
                },
            .rotation_angle = ppa_rotation,
            .scale_x = (float)scale,
            .scale_y = (float)scale,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        esp_err_t res = ppa_do_scale_rotate_mirror(ppa_client, &operation);
        if(res == ESP_OK)
        {
            swap_buffers(draw_fb);
            return;
        }

        ESP_LOGW(TAG, "PPA operation failed (%s), falling back to the CPU", esp_err_to_name(res));
        ppa_client = NULL;
    }

    uint16_t* target = panel_fb ? draw_fb + offset_y * stride + offset_x : framebuffer;
    if(target == NULL)
    {
        return;
    }

    // outer walks whichever TIC-80 axis maps to panel rows, inner walks the
    // one that maps along a row.
    const size_t outer_count = transpose ? TIC80_FULLWIDTH : TIC80_FULLHEIGHT;
    const size_t inner_count = transpose ? TIC80_FULLHEIGHT : TIC80_FULLWIDTH;

    for(size_t outer = 0; outer < outer_count; outer++)
    {
        size_t row_index = flip_outer ? (outer_count - 1 - outer) : outer;
        uint16_t* row = target + (row_index * scale) * stride;

        for(size_t inner = 0; inner < inner_count; inner++)
        {
            uint32_t pixel = transpose ? screen[inner * TIC80_FULLWIDTH + outer]
                                       : screen[outer * TIC80_FULLWIDTH + inner];

            uint16_t colour = to_rgb565(pixel);
            if(swap_bytes)
            {
                colour = (uint16_t)((colour >> 8) | (colour << 8));
            }

            size_t column = flip_inner ? (inner_count - 1 - inner) : inner;
            uint16_t* out = row + column * scale;
            for(size_t i = 0; i < scale; i++)
            {
                out[i] = colour;
            }
        }

        // Copying the finished row down beats converting it again.
        for(size_t i = 1; i < scale; i++)
        {
            memcpy(row + i * stride, row, blit_width * sizeof(uint16_t));
        }
    }

    if(panel_fb)
    {
        swap_buffers(draw_fb);
    }
    else
    {
        // bsp_display_blit takes end coordinates, not a width and a height.
        bsp_display_blit(offset_x, offset_y, offset_x + blit_width, offset_y + blit_height, framebuffer);
    }
}
