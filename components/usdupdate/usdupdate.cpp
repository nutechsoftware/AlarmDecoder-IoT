/**
 *  @file    usdupdate_util.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *  @version 1.0.3
 *
 *  @brief uSD firmware update support. Flash firmware from file on uSD disk.
 *
 *  @copyright Copyright (C) 2020 Nu Tech Software Solutions, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

static const char *TAG = "AD2LUPDATE";

 // AlarmDecoder std includes
#include "alarmdecoder_main.h"

 // Disable via config
#if CONFIG_AD2IOT_USDUPDATE

// esp component includes
#include <esp_ota_ops.h>
#include <esp_partition.h>

//#define DEBUG_LUPDATE

#define CONFIG_FIRMWARE_PATH "/sdcard/firmware.bin"

#define USDUPDATE_UPGRADE_CMD   "upgradeusd"
#define USDUPDATE_VERSION_CMD   "versionusd"


// forward decl
void usd_do_version(const char *arg);
void usd_do_update(const char *command);

// OTA Update task
TaskHandle_t usdupdate_task_handle = NULL;

/**
 * @brief Firmware update task that preforms the update to the flash
 * from the uSD disk.
 */
static void usd_task_func(void * command)
{
    ad2_printf_host(false, "Starting uSD update");

    FILE *f = fopen("/sdcard/firmware.bin", "rb");
    if (f == NULL) {
        ad2_printf_host(false, "uSD update '/sdcard/firmware.bin' not found, aborting.");
        vTaskDelete(NULL);
    }

    esp_err_t ota_res;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    // Start OTA
    esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);

    char *buffer = (char *)malloc(4096);
    while (true) {
        size_t read = fread(buffer, 1, 4096, f);
        if (read == 0) break;
        esp_ota_write(update_handle, buffer, read);
    }

    // Finish and set boot partition
    esp_ota_end(update_handle);
    esp_ota_set_boot_partition(update_partition);

    free(buffer);
    fclose(f);
    # remove the firmware file on success
    ad2_printf_host(true, "%s Prepare to restart system!", TAG);
    hal_restart();
}

/**
 * @brief Initiate and uSD update process
 */
void usd_do_update(const char *command)
{
    if (usdupdate_task_handle != NULL) {
        ESP_LOGW(TAG, "Device is currently updating.");
        return;
    }
    xTaskCreate(&usd_task_func, "AD2 uSD Update", 1024*8, strdup(command), tskIDLE_PRIORITY+2, &usdupdate_task_handle);
}

/**
 * @brief command list for module
 */
static struct cli_command uSDupdate_cmd_list[] = {
    {
        (char*)USDUPDATE_UPGRADE_CMD,(char*)
        "Usage: upgradeusd\r\n"
        "\r\n"
        "    Preform upgrade from connected uSD flash drive.\r\n"
        , usd_do_update
    },
    {
        (char*)USDUPDATE_VERSION_CMD,(char*)
        "Usage: versionusd\r\n"
        "\r\n"
        "    Report the current version\r\n"
        , usd_do_version
    }
};


/**
 * @brief Register component cli commands.
 */
void usdupdate_register_cmds()
{
    // Register CLI commands
    for (int i = 0; i < ARRAY_SIZE(uSDupdate_cmd_list); i++) {
        cli_register_command(&uSDupdate_cmd_list[i]);
    }
}

/**
 * @brief Init.
 */
void usdupdate_init()
{
     ad2_printf_host(true, "%s: Init done", TAG);
}

/**
 * @brief Show installed and available version
 */
void usd_do_version(const char *arg)
{
    ad2_printf_host(false, "Installed version(" FIRMWARE_VERSION  ") build flag (" FIRMWARE_BUILDFLAGS ").\r\n");
}

#endif /* CONFIG_AD2IOT_USDUPDATE */
