/**
 *  @file    ad2_settings.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief Settings common to everyone
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
// @brief Define for task stack reporting for tuning.
//#define AD2_STACK_REPORT

// @brief Firmware version string.
#define FIRMWARE_VERSION      "AD2IOT-1102"

#if defined(CONFIG_STDK_IOT_CORE)
#define FIRMWARE_BUILDFLAGS "stsdk"
// Default netmode N disabled
#define AD2_DEFAULT_NETMODE_STRING "N"
#else
#define FIRMWARE_BUILDFLAGS "webui"
// Default netmode Ethernet dhcp
#define AD2_DEFAULT_NETMODE_STRING "E mode=d"
#endif

// @brief MAX address slots
#define AD2_MAX_ADDRESS       99

// @brief MAX partition slots
#define AD2_MAX_PARTITION      8

// @brief MAX zones
#define AD2_MAX_ZONES        255

// @brief MAX code slots
#define AD2_MAX_CODE         128

// @brief MAX virtual switches
#define AD2_MAX_SWITCHES      255
#define AD2_MAX_SWITCH_SEARCH_KEYS 9

// @brief Default code to use if none specified
#define AD2_DEFAULT_CODE_SLOT 1

// @brief Default Address to use if none specified
#define AD2_DEFAULT_VPA_SLOT  1

// @brief Max connection mode string argument size(host:port|UART#)
#define AD2_MAX_MODE_ARG_SIZE 80

// @brief Main config section
#define AD2MAIN_CONFIG_SECTION ""

// @brief partition config section
#define AD2PART_CONFIG_SECTION "partition"
#define PART_CONFIG_ADDRESS "address"
#define PART_CONFIG_ZONES "zones"

// @brief [zone N] config section
#define AD2ZONE_CONFIG_SECTION  "zone"
#define ZONE_CONFIG_DESCRIPTION  "description"

// @brief code config section
#define AD2CODES_CONFIG_SECTION "code"

// @brief [switch N] confoig section
#define AD2SWITCH_CONFIG_SECTION "switch"
#define AD2SWITCH_SK_DELETE1 "-"
#define AD2SWITCH_SK_DELETE2 "delete"
#define AD2SWITCH_SK_DEFAULT "default"
#define AD2SWITCH_SK_RESET "reset"
#define AD2SWITCH_SK_TYPES "types"
#define AD2SWITCH_SK_FILTER "filter"
#define AD2SWITCH_SK_OPEN "open"
#define AD2SWITCH_SK_CLOSE "close"
#define AD2SWITCH_SK_TROUBLE "trouble"

// @brief netmode settings key under main section
#define NETMODE_CONFIG_KEY    "netmode"

// @brief ad2source settings key under main section
#define AD2MODE_CONFIG_KEY    "ad2source"

// @brief ad2config settings key under main section
#define AD2CONFIG_CONFIG_KEY    "ad2config"

// @brief logmode setting key under main section
#define LOGMODE_CONFIG_KEY    "logmode"

// UART RX buffer size
#define AD2_UART_RX_BUFF_SIZE  100
#define MAX_UART_CMD_SIZE    (1024)

// NV
#define AD2_MAX_VALUE_SIZE 1024

// Message prefix
#define AD2PFX "!IOT: "

// Signon message
#define AD2_SIGNON "%s: Starting AlarmDecoder AD2IoT network appliance version (%s) build flag (%s)"

// The virtual mount prefix for all file operations.
#define AD2_USD_MOUNT_POINT "sdcard"
#define AD2_SPIFFS_MOUNT_POINT "spiffs"
#define AD2_CONFIG_FILE "/ad2iot.ini"

// Console LOCK timeout
#define AD2_CONSOLE_LOCK_TIME 500