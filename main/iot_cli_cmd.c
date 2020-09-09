/**
 *  @file    iot_cli_cmd.c
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
#include <stdarg.h>
#include <ctype.h>

#include "iot_uart_cli.h"
#include "device_control.h"

#include "st_dev.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ad2_utils.h"

#include "esp_log.h"
static const char *TAG = "CLI_CMD";

extern IOT_CTX *ctx;


/**
 * Set the USER code for a given slot.
 *
 * command: code <slot> <code>
 *
 * Valid slots are from 0 to AD2_MAX_CODE
 * where slot 0 is the default user.
 *
 * ex.
 *   STDK # code 1 1234
 */
static void _cli_cmd_code(char *string)
{
    char buf[7]; // MAX 6 digit code null terminated
    int slot = 0;

    if (ad2_copy_nth_arg(buf, string, sizeof(buf), 1) >= 0) {
        slot = strtol(buf, NULL, 10);
    }

    if (slot >= 0 && slot <= AD2_MAX_CODE) {
        if (ad2_copy_nth_arg(buf, string, sizeof(buf), 2) >= 0) {
            if (strlen(buf)) {
                    if (atoi(buf) == -1) {
                        printf("Removing code in slot %i...\n", slot);
                        ad2_set_nv_code(slot, NULL);
                    } else {
                        printf("Setting code in slot %i to '%s'...\n", slot, buf);
                        ad2_set_nv_code(slot, buf);
                    }
            }
        } else {
            // show contents of this slot
            memset(buf, 0, sizeof(buf));
            ad2_get_nv_code(slot, buf, sizeof(buf));
            printf("The code in slot %i is '%s'\n", slot, buf);
        }
    } else {
        ESP_LOGE(TAG, "%s: Error (args) invalid slot # (0-%i).", __func__, AD2_MAX_CODE);
    }
}

/**
 * Set the Virtual Partition address code for a given slot.
 *
 * command: vpaddr <slot> <code>
 *
 * Valid slots are from 0 to AD2_MAX_VPARTITION
 * where slot 0 is the default.
 *
 * ex. Set virtual parition 0 to use address 18 for TX/RX
 *   and address 16 for TX/RX on virtual partition 1.
 *   STDK # vpaddr 0 18
 *   STDK # vpaddr 1 16
 */
static void _cli_cmd_vpaddr(char *string)
{
    char buf[3]; // MAX 2 digit numbers
    int slot = 0;
    int address = 0;

    if (ad2_copy_nth_arg(buf, string, sizeof(buf), 1) >= 0) {
        slot = strtol(buf, NULL, 10);
    }

    if (slot >= 0 && slot <= AD2_MAX_VPARTITION) {
        if (ad2_copy_nth_arg(buf, string, sizeof(buf), 2) >= 0) {
            int address = strtol(buf, NULL, 10);
            if (address>=0 && address < AD2_MAX_ADDRESS) {
                    printf("Setting vpaddr in slot %i to '%i'...\n", slot, address);
                    ad2_set_nv_vpaddr(slot, address);
            } else {
                    // delete entry
                    address = -1;
                    printf("Deleting vpaddr in slot %i...\n", slot);
                    ad2_set_nv_vpaddr(slot, address);
            }
        } else {
            // show contents of this slot
            ad2_get_nv_vpaddr(slot, &address);
            printf("The vpaddr in slot %i is %i\n", slot, address);
        }
    } else {
        ESP_LOGE(TAG, "%s: Error (args) invalid slot # (0-%i).", __func__, AD2_MAX_ADDRESS);
    }
}

/**
 * Configure the AD2IoT connection to the AlarmDecoder device
 *
 *  command: ad2source <mode> <arg>
 * ex.
 *   ad2source c 2
 *   ad2source s 192.168.1.2:10000
 */
static void _cli_cmd_ad2source(char *string)
{
    char arg[AD2_MAX_MODE_ARG_SIZE]; // Big enough for a long(ish) host name
    uint8_t mode = 0;

    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        mode = toupper((int)arg[0]);
        if (ad2_copy_nth_arg(arg, string, sizeof(arg), 2) >= 0) {
            switch (mode)
            {
                case 'S':
                case 'C':
                    ad2_set_nv_mode_arg(mode, arg);
                    break;
                default:
                    printf("Invalid mode selected must be [S]ocket or [C]OM\n");
            }
        } else {
            printf("Missing <arg>\n");
        }
    } else {
        ad2_get_nv_mode_arg(&mode, arg, sizeof(arg));
        printf("Current %s '%s'\n", (mode=='C'?"UART#":"AUTHORITY"), arg);
    }
}

static void _cli_cmd_reboot(char *string)
{
    ESP_LOGE(TAG, "%s: rebooting now.", __func__);
    st_conn_cleanup(ctx, true);
}

static void _cli_cmd_cleanup(char *string)
{
    ESP_LOGI(TAG, "%s: clean-up data with reboot option", __func__);
    st_conn_cleanup(ctx, true);
}

extern void button_event(IOT_CAP_HANDLE *handle, int type, int count);
static void _cli_cmd_butten_event(char *string)
{
    char buf[10];
    int count = 1;
    int type = BUTTON_SHORT_PRESS;

    if (ad2_copy_nth_arg(buf, string, sizeof(buf), 1) >= 0) {
        count = strtol(buf, NULL, 10);
    }
    if (ad2_copy_nth_arg(buf, string, sizeof(buf), 2) >= 0) {
        if (strncmp(buf, "long", 4) == 0) {
            type = BUTTON_LONG_PRESS;
        }
    }

    ESP_LOGI(TAG, "%s: button_event : count %d, type %d", __func__, count, type);
    button_event(ctx, type, count);
}

static struct cli_command cmd_list[] = {
    {"reboot",
        "reboot this microcontroller\n", _cli_cmd_reboot},
    {"cleanup",
        "Cleanup NV data with reboot option\n"
        "  Syntax: cleanup\n", _cli_cmd_cleanup},
    {"button",
        "Simulate a button press event\n"
        "  Syntax: button <count> <type>\n"
        "  Example: button 5 / button 1 long\n", _cli_cmd_butten_event},
    {"code",
        "Manage user codes\n"
        "  Syntax: code <id> <value>\n"
        "  Examples:\n"
        "    Set default code to 1234\n"
        "      code 0 1234\n"
        "    Set alarm code for slot 1\n"
        "      code 1 1234\n"
        "    Show code in slot #3\n"
        "      code 3\n"
        "    Remove code for slot 2\n"
        "      code 2 -1\n"
        "    Note: value -1 will remove an entry.\n", _cli_cmd_code},
    {"vpaddr",
        "Manage virtual partitions\n"
        "  Syntax: vpaddr <partition> <address>\n"
        "  Examples:\n"
        "    Set default send address to 18\n"
        "      vpaddr 0 18\n"
        "    Show address for partition 2\n"
        "      vpaddr 2\n"
        "    Remove virtual partition in slot 2\n"
        "      vpaddr 2 -1\n"
        "  Note: address -1 will remove an entry.\n", _cli_cmd_vpaddr},
    {"ad2source",
        "Manage AlarmDecoder protocol source.\n"
        "  Syntax: ad2source <[S]OCK|[C]OM> <AUTHORITY|UART#>\n"
        "  Examples:\n"
        "    Show current mode\n"
        "      ad2source"
        "    Set source to ser2sock client at address and port\n"
        "      ad2source SOCK 192.168.1.2:10000\n"
        "    Set source to local attached uart #2\n"
        "      ad2source COM 2\n", _cli_cmd_ad2source},
};

void register_iot_cli_cmd(void) {
    for (int i = 0; i < ARRAY_SIZE(cmd_list); i++)
        cli_register_command(&cmd_list[i]);
}
