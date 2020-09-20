/**
 *  @file    ota_util.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *
 *  @brief OTA update support
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

#ifndef ota_util_h
#define ota_util_h

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_log.h>

#define OTA_FIRMWARE_VERSION "AD2IOT-100A"

#define CONFIG_OTA_SERVER_URL "https://ad2iotota.alarmdecoder.com:4443/"
#define CONFIG_FIRMWARE_VERSOIN_INFO_URL CONFIG_OTA_SERVER_URL"ad2iotv10_version_info.json"
#define CONFIG_FIRMWARE_UPGRADE_URL CONFIG_OTA_SERVER_URL"signed_alarmdecoder_stsdk_esp32.bin"

#define OTA_SIGNATURE_SIZE 256
#define OTA_SIGNATURE_FOOTER_SIZE 6
#define OTA_SIGNATURE_PREFACE_SIZE 6
#define OTA_DEFAULT_SIGNATURE_BUF_SIZE OTA_SIGNATURE_PREFACE_SIZE + OTA_SIGNATURE_SIZE + OTA_SIGNATURE_FOOTER_SIZE

#define OTA_DEFAULT_BUF_SIZE 256
#define OTA_CRYPTO_SHA256_LEN 32

extern TaskHandle_t ota_task_handle;

int ota_get_polling_period_day();
void ota_nvs_flash_init();
esp_err_t ota_api_get_available_version(char *update_info, unsigned int update_info_len, char **new_version);
esp_err_t ota_https_update_device();
esp_err_t ota_https_read_version_info(char **version_info, unsigned int *version_info_len);
void do_ota_update();

#ifdef __cplusplus
}
#endif
#endif // ota_util_h
