/**
 *  @file    ad2_utils.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief ad2 specifc utils
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

/**
 * Generic get NV int value by key and slot(0-99)
 */
void ad2_get_nv_slot_key_int(const char *key, int slot, int *value) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
        err = nvs_get_i32(my_handle, tkey, value);
        free(tkey);
        nvs_close(my_handle);
    }
}

/**
 * Generic set NV int value by key and slot(0-99)
 * Value < 0 will remove entry
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
        char *tkey = malloc(tlen);
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
 * Generic get NV string value by key and slot(0-99)
 */
void ad2_get_nv_slot_key_string(const char *key, int slot, char *value, size_t size) {
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        int tlen = strlen(key)+3; // add space for XX\n
        char *tkey = malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
        err = nvs_get_str(my_handle, tkey, value, &size);
        free(tkey);
        nvs_close(my_handle);
    }
}

/**
 * Generic set NV string value by key and slot(0-99)
 * Value < 0 will remove entry
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
        char *tkey = malloc(tlen);
        snprintf(tkey, tlen, "%02i", slot);
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
 * Generic get NV arg
 */
void ad2_get_nv_arg(const char *key, char *arg, size_t size)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        err = nvs_get_str(my_handle, key, arg, &size);
        nvs_close(my_handle);
    }
}

/**
 * Generic set NV arg
 */
void ad2_set_nv_arg(const char *key, char *arg)
{
    esp_err_t err;
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        err = nvs_set_str(my_handle, key, arg);
        err = nvs_commit(my_handle);
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

void ad2_arm_away() {
// TODO: uint32_t amask = 0xffffff7f;
    // TODO: AD2VirtualPartitionState * s = AD2Parse.getAD2PState(&amask);

    // send back our current state
    //cap_securitySystem_data->attr_securitySystemStatus_send(cap_securitySystem_data);

    // Get user code
    char code[7];
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, AD2_DEFAULT_CODE_SLOT, code, sizeof(code));

    // Get the address/partition mask
    // Message format KXXYYYYZ
    char msg[9] = {0};
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, AD2_DEFAULT_VPA_SLOT, &address);
    snprintf(msg, sizeof(msg), "K%02i%s%s", address, code, "3");
    ESP_LOGI(TAG ,"Sending ARM AWAY command");
    ad2_send(msg);
}


/**
 * send a string to the AD2
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

    }
  } else {
        ESP_LOGE(TAG, "invalid handle in send_to_ad2");
        return;
  }
}