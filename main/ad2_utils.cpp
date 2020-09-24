/**
 *  @file    ad2_utils.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief AD2IOT common utils shared between main and components
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

// esp includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"
static const char *TAG = "AD2UTIL";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"

#include "ad2_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic get NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [out]valueout int * to store search results.
 *
 */
void ad2_get_nv_slot_key_int(const char *key, int slot, int *valueout) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = (char*)malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
        err = nvs_get_i32(my_handle, tkey, valueout);
        free(tkey);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [in]value int value to store for search results.
 *
 * @note  value < 0 will remove entry
 */
void ad2_set_nv_slot_key_int(const char *key, int slot, int value) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = (char*)malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
        if (value == -1) {
            err = nvs_erase_key(my_handle, tkey);
        } else {
            err = nvs_set_i32(my_handle, tkey, value);
        }
        free(tkey);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic get NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [out]valueout pointer store search results.
 *
 */
void ad2_get_nv_slot_key_string(const char *key, int slot, char *valueout, size_t size) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = (char*)malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
        err = nvs_get_str(my_handle, tkey, valueout, &size);
        free(tkey);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV string value by key and slot(0-99).
 * A value pointer of 0 or NULL will remove the entry if found.
 *
 * @param [in]key pointer to key to save value under
 * @param [in]slot int slot# from 0 - 99
 * @param [in]value pointer to string to store for search results.
 *
 */
void ad2_set_nv_slot_key_string(const char *key, int slot, char *value) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = (char*)malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot); // MAX XX(99)
        if (value == NULL) {
            err = nvs_erase_key(my_handle, tkey);
        } else {
            err = nvs_set_str(my_handle, tkey, value);
        }
        free(tkey);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic get NV value by key.
 *
 * @param [in]key pointer to key for the value to find
 * @param [out]valueout pointer to store value string if found
 * @param [in]size size of results storage.
 *
 * @note string will be truncated if larger than size including
 * the null terminator.
 */
void ad2_get_nv_arg(const char *key, char *valueout, size_t size)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        err = nvs_get_str(my_handle, key, valueout, &size);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV value by key.
 *
 * @param [in]key pointer to key for stored value
 * @param [in]value pointer to value string to store
 *
 */
void ad2_set_nv_arg(const char *key, char *value)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        err = nvs_set_str(my_handle, key, value);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}


/**
 * @brief Copy the Nth space seperated word from a string.
 *
 * @param dest pointer to output bytes
 * @param [in]src pointer to input bytes
 * @param [in]size size of input buffer
 * @param [in]n argument number to return if possible
 *
 * @return 0 on success -1 on failure
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
        ESP_LOGD(TAG, "%s: Fail to find %dth arg", __func__, n);
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

/**
 * @brief Send the ARM AWAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_arm_away(int codeId, int vpartId) {

    // Get user code
    char code[7];
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, code, sizeof(code));

    // Get the address/partition mask
    // Message format KXXYYYYZ
    char msg[9] = {0};
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, &address);
    snprintf(msg, sizeof(msg), "K%02i%s%s", address, code, "3");
    ESP_LOGI(TAG ,"Sending ARM AWAY command");
    ad2_send(msg);
}

/**
 * @brief Toggle Chime mode.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_chime_toggle(int codeId, int vpartId) {

    // Get user code
    char code[7];
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, code, sizeof(code));

    // Get the address/partition mask
    // Message format KXXYYYYZ
    char msg[9] = {0};
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, &address);
    // FIXME DSC panel switch
    snprintf(msg, sizeof(msg), "K%02i%s%s", address, code, "6");
    ESP_LOGI(TAG ,"Sending CHIME toggle command");
    ad2_send(msg);
}

/**
 * @brief Send the ARM STAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_arm_stay(int codeId, int vpartId) {

    // Get user code
    char code[7];
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, code, sizeof(code));

    // Get the address/partition mask
    // Message format KXXYYYYZ
    char msg[9] = {0};
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, &address);
    snprintf(msg, sizeof(msg), "K%02i%s%s", address, code, "2");
    ESP_LOGI(TAG ,"Sending ARM AWAY command");
    ad2_send(msg);
}

/**
 * @brief send RAW string to the AD2 devices.
 *
 * @param [in]buf Pointer to string to send to AD2 devices.
 *
 */
void ad2_send(char *buf)
{
  if(g_ad2_client_handle>-1) {
    if (g_ad2_mode == 'C') {
      uart_write_bytes((uart_port_t)g_ad2_client_handle, buf, strlen(buf));
    } else
    if (g_ad2_mode == 'S') {
      // the handle is a socket fd use send()
      send(g_ad2_client_handle, buf, strlen(buf), 0);
    } else {
        ESP_LOGE(TAG, "invalid ad2 connection mode");
    }
  } else {
        ESP_LOGE(TAG, "invalid handle in send_to_ad2");
        return;
  }
}

#ifdef __cplusplus
} // extern "C"
#endif
