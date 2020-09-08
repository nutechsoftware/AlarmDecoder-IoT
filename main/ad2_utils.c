/**
 *  @file    ad2_utils.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *  @version 1.0
 *
 *  @brief AlarmDecoder IoT embedded network appliance for SmartThings
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ad2_utils.h"

#include "esp_log.h"
static const char *TAG = "AD2UTIL";

/**
 * set NV Code
 */
void ad2_set_nv_code(int slot, char *code)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open("codes", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        char key[8] = {0};
        snprintf(key, sizeof(key), "CODE%03i", slot);
        if (code == NULL) {
            err = nvs_erase_key(my_handle, key);
        } else {
            err = nvs_set_str(my_handle, key, code);
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * Get NV code
 */
void ad2_get_nv_code(int slot, char *code, int size)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open("codes", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        size_t code_size = size;
        char key[8] = {0};
        snprintf(key, sizeof(key), "CODE%03i", slot);
        err = nvs_get_str(my_handle, key, code, &code_size);
        nvs_close(my_handle);
    }
}

/**
 * Set NV virtual partition address
 */
void ad2_set_nv_vpaddr(int slot, int32_t address)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open("vpal", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        char key[5] = {0};
        snprintf(key, sizeof(key), "VPA%01i", slot);
        if (address == -1) {
            err = nvs_erase_key(my_handle, key);
        } else {
            err = nvs_set_i32(my_handle, key, address);
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * Get NV virtual partition address
 */
void ad2_get_nv_vpaddr(int slot, int32_t *paddress)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open("vpal", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        char key[5] = {0};
        snprintf(key, sizeof(key), "VPA%01i", slot);
        err = nvs_get_i32(my_handle, key, paddress);
        nvs_close(my_handle);
    }
}

/**
 * Copy the Nth space seperated word from a string.
 */
int ad2_copy_nth_arg(char* dest, char* src, int size, int n)
{
    int start = 0, end = -1;
    int i = 0, word_index = 0;
    int len;

    for (i = 0; src[i] != '\0'; i++) {
        if ((src[i] == ' ') && (src[i+1]!=' ') && (src[i+1]!='\0')) { //start check
            word_index++;
            if (word_index == n) {
                start = i+1;
            }
        } else if ((src[i] != ' ') && ((src[i+1]==' ')||(src[i+1]=='\0'))) { //end check
            if (word_index == n) {
                end = i;
                break;
            }
        }
    }

    if (end == -1) {
        //ESP_LOGD(TAG, "%s: Fail to find %dth arg", __func__, n);
        return -1;
    }

    len = end - start + 1;
    if ( len > size - 1) {
        len = size - 1;
    }

    strncpy(dest, &src[start], len);
    dest[len] = '\0';
    return 0;

}
