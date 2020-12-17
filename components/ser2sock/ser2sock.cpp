/**
*  @file    ser2sock.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    12/17/2020
*  @version 1.0.0
*
*  @brief ser2sock server daemon
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

// common includes
// stdc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>

// stdc++
#include <string>
#include <sstream>

// esp includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "SER2SOCKD";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"

// Disable via sdkconfig
#if CONFIG_SER2SOCKD

// specific includes
#include "ser2sock.h"
#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_SER2SOCKD */
