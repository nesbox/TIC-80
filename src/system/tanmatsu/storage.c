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

#include <stdio.h>
#include <sys/stat.h>

#include "bsp/storage.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

static char const TAG[] = "tic80-storage";

#define INTERNAL_MOUNT_POINT "/int"
#define INTERNAL_PARTITION   "locfd"
#define SD_MOUNT_POINT       "/sd"
#define TIC80_DIRECTORY      "tic80"

static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static char root[64] = INTERNAL_MOUNT_POINT "/" TIC80_DIRECTORY;

// The BSP mounts the SD card but not the internal FAT partition, which the
// launcher mounts itself as well.
static esp_err_t mount_internal(void)
{
    esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    return esp_vfs_fat_spiflash_mount_rw_wl(INTERNAL_MOUNT_POINT, INTERNAL_PARTITION, &config, &wl_handle);
}

// The BSP powers the card up through the on-chip LDO and power cycles it, and
// it knows that the card shares its SDMMC controller with the ESP32-C6 radio:
// with ESP-Hosted in the build the radio owns the controller, and the card
// must not initialise it a second time.
static esp_err_t mount_sd_card(void)
{
    if(bsp_storage_get_status(BSP_STORAGE_TYPE_SDCARD) == BSP_STORAGE_STATUS_NOT_PRESENT)
    {
        return ESP_ERR_NOT_FOUND;
    }

    return bsp_storage_mount(BSP_STORAGE_TYPE_SDCARD, SD_MOUNT_POINT);
}

static bool ensure_directory(const char* path)
{
    struct stat info;
    if(stat(path, &info) == 0)
    {
        return true;
    }

    if(mkdir(path, 0777) != 0)
    {
        ESP_LOGW(TAG, "Could not create %s", path);
        return false;
    }

    return true;
}

esp_err_t tanmatsu_storage_init(void)
{
    bool have_internal = false;

    esp_err_t res = mount_internal();
    if(res == ESP_OK)
    {
        have_internal = true;
        ESP_LOGI(TAG, "Internal filesystem mounted at " INTERNAL_MOUNT_POINT);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to mount the internal filesystem: %s", esp_err_to_name(res));
    }

    // Carts live on the SD card when there is one; it is bigger and it is what
    // people can take out and copy files onto.
    esp_err_t sd = mount_sd_card();
    if(sd == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card mounted at " SD_MOUNT_POINT);
        snprintf(root, sizeof(root), SD_MOUNT_POINT "/" TIC80_DIRECTORY);
    }
    else if(have_internal)
    {
        // Worth saying out loud: carts saved on the card are not missing, they
        // are simply not reachable, and anything saved now goes elsewhere.
        ESP_LOGW(TAG, "No SD card (%s), using internal flash instead", esp_err_to_name(sd));
        snprintf(root, sizeof(root), INTERNAL_MOUNT_POINT "/" TIC80_DIRECTORY);
    }
    else
    {
        ESP_LOGE(TAG, "No writable filesystem, TIC-80 will not be able to save");
        return ESP_FAIL;
    }

    if(!ensure_directory(root))
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Using %s", root);

    return ESP_OK;
}

const char* tanmatsu_storage_root(void)
{
    return root;
}
