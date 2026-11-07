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

#include "bsp/audio.h"
#include "driver/i2s_common.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static char const TAG[] = "tic80-audio";

// The codec runs 16 bit stereo Philips I2S, which is exactly the layout of
// TIC-80's sample buffer, so the frames go out without any conversion.
static i2s_chan_handle_t i2s_handle = NULL;

// TIC-80 produces exactly one frame of audio per rendered frame. Writing that
// straight to the codec ties the sound to the frame rate, so a frame that runs
// long is heard as a gap. Instead frames go into a stream buffer that a task on
// the other core drains at the codec's own pace, which rides out the jitter.
//
// It cannot conjure samples that were never generated: a cart that genuinely
// sustains 20 fps produces a third of the audio it needs, and the gaps are
// filled with silence rather than with the wrong pitch.
#define AUDIO_FRAME_SAMPLES (TIC80_SAMPLERATE * TIC80_SAMPLE_CHANNELS / TIC80_FRAMERATE)
#define AUDIO_FRAME_BYTES   (AUDIO_FRAME_SAMPLES * sizeof(int16_t))
#define AUDIO_QUEUE_FRAMES  6

static StreamBufferHandle_t audio_stream = NULL;
static TaskHandle_t audio_task = NULL;

static void audio_task_entry(void* arg)
{
    (void)arg;

    int16_t* chunk = heap_caps_calloc(1, AUDIO_FRAME_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if(chunk == NULL)
    {
        ESP_LOGE(TAG, "No memory for the audio chunk buffer");
        audio_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    while(true)
    {
        size_t got = xStreamBufferReceive(audio_stream, chunk, AUDIO_FRAME_BYTES, pdMS_TO_TICKS(100));

        if(got < AUDIO_FRAME_BYTES)
        {
            // Starved, because the cart is running slower than 60 fps. Pad with
            // silence so what did arrive still plays at the right pitch.
            memset((uint8_t*)chunk + got, 0, AUDIO_FRAME_BYTES - got);
        }

        size_t written = 0;
        i2s_channel_write(i2s_handle, chunk, AUDIO_FRAME_BYTES, &written, 1000);
    }
}

esp_err_t tanmatsu_audio_init(void)
{
    esp_err_t res = bsp_audio_get_i2s_handle(&i2s_handle);
    if(res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get the I2S handle: %s", esp_err_to_name(res));
        return res;
    }

    // The BSP already brings the codec up at 44100 Hz, which is what TIC-80
    // wants, but say so anyway in case that default ever moves. The clock can
    // only be reconfigured while the channel is disabled, and the BSP leaves
    // it enabled.
    if(i2s_channel_disable(i2s_handle) == ESP_OK)
    {
        res = bsp_audio_set_rate(TIC80_SAMPLERATE);
        if(res != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set the sample rate: %s", esp_err_to_name(res));
        }

        res = i2s_channel_enable(i2s_handle);
        if(res != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to re-enable the I2S channel: %s", esp_err_to_name(res));
            return res;
        }
    }
    else
    {
        ESP_LOGW(TAG, "Could not disable the I2S channel, keeping the rate the BSP set");
    }

    res = bsp_audio_set_amplifier(true);
    if(res != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to enable the amplifier: %s", esp_err_to_name(res));
    }

    tanmatsu_audio_set_volume(80);

    audio_stream = xStreamBufferCreate(AUDIO_FRAME_BYTES * AUDIO_QUEUE_FRAMES, AUDIO_FRAME_BYTES);
    if(audio_stream == NULL)
    {
        ESP_LOGW(TAG, "No audio stream buffer, writing to the codec from the main loop");
        return ESP_OK;
    }

    // Core 1, which the rest of this port leaves idle.
    if(xTaskCreatePinnedToCore(audio_task_entry, "tic80-audio", 4096, NULL, 10, &audio_task, 1) != pdPASS)
    {
        ESP_LOGW(TAG, "No audio task, writing to the codec from the main loop");
        vStreamBufferDelete(audio_stream);
        audio_stream = NULL;
    }

    return ESP_OK;
}

void tanmatsu_audio_set_volume(int32_t percent)
{
    if(percent < 0)
    {
        percent = 0;
    }
    if(percent > 100)
    {
        percent = 100;
    }

    esp_err_t res = bsp_audio_set_volume((float)percent);
    if(res != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to set the volume: %s", esp_err_to_name(res));
    }
}

void tanmatsu_audio_write(const int16_t* samples, size_t count)
{
    if(i2s_handle == NULL || samples == NULL || count == 0)
    {
        return;
    }

    if(audio_stream != NULL)
    {
        // Blocks once the buffer is full, which is what paces the main loop:
        // the codec consumes in real time, so the loop cannot run ahead.
        xStreamBufferSend(audio_stream, samples, count * sizeof(int16_t), pdMS_TO_TICKS(100));
        return;
    }

    size_t written = 0;
    // One frame's worth of audio is 1/60th of a second, so a full second of
    // timeout only ever trips when the codec has stopped.
    esp_err_t res = i2s_channel_write(i2s_handle, samples, count * sizeof(int16_t), &written, 1000);
    if(res != ESP_OK)
    {
        ESP_LOGW(TAG, "I2S write failed: %s", esp_err_to_name(res));
    }
}
