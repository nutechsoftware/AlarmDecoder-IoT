/**
 *  @file    alarmdecoder_main.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief AlarmDecoder IoT embedded network appliance
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
#ifndef _ALARMDECODER_MAIN_H
#define _ALARMDECODER_MAIN_H

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// ESP-IDF includes
#include "esp_log.h"
#include "cJSON.h"
#include <lwip/netdb.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
#include "esp_crt_bundle.h"
#endif

#include "esp_http_client.h"

/**
 * AlarmDecoder Arduino library.
 * https://github.com/nutechsoftware/ArduinoAlarmDecoder
 */
#include "alarmdecoder_api.h"

// Common settings
#include "ad2_settings.h"

// Common utils
#include "ad2_utils.h"

// HAL
#include "device_control.h"

#include "ad2_uart_cli.h"

#ifdef __cplusplus
extern "C" {
#endif

// global thread control
extern int g_StopMainTask;

// global critical section handle
extern portMUX_TYPE spinlock;

// global host console access mutex
extern SemaphoreHandle_t g_ad2_console_mutex;

// global AlarmDecoder parser class instance
extern AlarmDecoderParser AD2Parse;

// global AD2 device connection fd/id <socket or uart id>
extern int g_ad2_client_handle;

// global ad2 connection mode ['S'ocket | 'C'om port]
extern uint8_t g_ad2_mode;

// global ad2 connection mode args
extern std::string g_ad2_mode_args;


// global uSD card mounted flag
extern bool g_uSD_mounted;

// global host console access mutex
extern SemaphoreHandle_t g_ad2_console_mutex;

// global eventgroup for network sync and control
extern EventGroupHandle_t g_ad2_net_event_group;

#ifdef __cplusplus
}
#endif

#endif /* _ALARMDECODER_MAIN_H */

