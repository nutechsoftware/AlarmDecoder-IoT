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

/**
 * ser2sock command list and enum.
 */
char * S2SD_SETTINGS [] = {
    (char*)S2SD_ENABLE,
    (char*)S2SD_ACL_LIST,
    0 // EOF
};

enum {
    S2SD_ENABLE_ID = 0,
    S2SD_ACL_LIST_ID,
};

/**
 * ser2sockd generic command event processing
 *  command: [COMMAND] <id> <arg>
 * ex.
 *   [COMMAND] 0 arg...
 */
static void _cli_cmd_ser2sockd_event(char *string){
    std::string buf;
    std::string key;

    // key value validation
    ad2_copy_nth_arg(key, string, 0);
    ad2_lcase(key);

    int i;
    for(i = 0;; ++i) {
        if (S2SD_SETTINGS[i] == 0) {
            ad2_printf_host("What?\r\n");
            break;
        }
        if(key.compare(S2SD_SETTINGS[i]) == 0) {
            std::string arg;
            switch(i) {
                /**
                 * Enable/Disable ser2sock daemon.
                 */
                case S2SD_ENABLE_ID:
                    ESP_LOGI(TAG, "%s: enable/disable SER2SOCKD", __func__);
                    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
                        ad2_set_nv_slot_key_int(S2SD_ENABLE, 0, nullptr, (arg[0] == 'Y' || arg[0] ==  'y'));
                        ad2_printf_host("Success setting value. Restart required to take effect.\r\n");
                    }

                    // show contents of this slot
                    int i;
                    ad2_get_nv_slot_key_int(S2SD_ENABLE, 0, nullptr, &i);
                    ad2_printf_host("ser2sock daemon is '%s'.\r\n", (i ? "Enabled" : "Disabled"));
                    break;
                /**
                 * ser2sock daemon IP/CIDR ACL list.
                 */
                case S2SD_ACL_LIST_ID:
                    ad2_printf_host("Not yet implemented.\r\n");
                    break;
                default:
                    break;
            }
            break;
        }
    }
}

/**
 * @brief command list for module
 */
static struct cli_command ser2sockd_cmd_list[] = {
    {
        (char*)S2SD_ENABLE,(char*)
        "- Enable ser2sock daemon component\r\n\r\n"
        "  ```" S2SD_ENABLE " {bool}```\r\n\r\n"
        "  - {bool}: [Y]es/[N]o\n\n", _cli_cmd_ser2sockd_event
    },
    {
        (char*)S2SD_ACL_LIST,(char*)
        "- Sets a list of ip addresses or networks allowed to connect in CIDR format.\r\n\r\n"
        "  ```" S2SD_ACL_LIST " {CIDR list}```\r\n\r\n"
        "  - {CIDR list}: empty\r\n"
        "    - Leave empty string for no ACL.\r\n"
        "  Example: " S2SD_ACL_LIST " 192.168.123/24,10.10.10.0/24\r\n\r\n", _cli_cmd_ser2sockd_event
    }
};

/**
 * @brief Register componet cli commands.
 */
void ser2sockd_register_cmds()
{
    // Register ser2sock CLI commands
    for (int i = 0; i < ARRAY_SIZE(ser2sockd_cmd_list); i++) {
        cli_register_command(&ser2sockd_cmd_list[i]);
    }
}

/**
 * @brief Initialize the ser2sock daemon
 */
void ser2sockd_init(void)
{
    int enabled = 0;
    ad2_get_nv_slot_key_int(S2SD_ENABLE, 0, nullptr, &enabled);

    // nothing more needs to be done once commands are set if not enabled.
    if (!enabled) {
        ESP_LOGI(TAG, "ser2sockd disabled");
        return;
    }

    ESP_LOGI(TAG, "Starting ser2sockd");

    // TODO:
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_SER2SOCKD */
