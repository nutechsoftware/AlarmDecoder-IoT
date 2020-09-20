/**
 *  @file    iot_cli_cmd.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief CLI interface for AD2IoT
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
#include "iot_bsp_system.h"
#include "device_control.h"

#include "st_dev.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ad2_utils.h"
#include "ad2_settings.h"
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
static void _cli_cmd_code_event(char *string)
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
                        ad2_set_nv_slot_key_string(CODES_CONFIG_KEY, slot, buf);
                    } else {
                        printf("Setting code in slot %i to '%s'...\n", slot, buf);
                        ad2_set_nv_slot_key_string(CODES_CONFIG_KEY, slot, buf);
                    }
            }
        } else {
            // show contents of this slot
            memset(buf, 0, sizeof(buf));
            ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, slot, buf, sizeof(buf));
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
static void _cli_cmd_vpaddr_event(char *string)
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
                    ad2_set_nv_slot_key_int(VPADDR_CONFIG_KEY, slot, address);
            } else {
                    // delete entry
                    printf("Deleting vpaddr in slot %i...\n", slot);
                    ad2_set_nv_slot_key_int(VPADDR_CONFIG_KEY, slot, -1);
            }
        } else {
            // show contents of this slot
            ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, slot, &address);
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
static void _cli_cmd_ad2source_event(char *string)
{
    char modesz[2]; // just a single byte and null term
    char arg[AD2_MAX_MODE_ARG_SIZE]; // Big enough for a long(ish) host name

    if (ad2_copy_nth_arg(modesz, string, sizeof(modesz), 1) >= 0) {
        // upper case it all
        uint8_t mode = toupper((int)modesz[0]); modesz[0] = mode; modesz[1] = 0;
        if (ad2_copy_nth_arg(arg, string, sizeof(arg), 2) >= 0) {
            switch (mode)
            {
                case 'S':
                case 'C':
                    // save mode in slot 0
                    ad2_set_nv_slot_key_string(AD2MODE_CONFIG_KEY, 0, modesz);
                    // save arg in slot 1
                    ad2_set_nv_slot_key_string(AD2MODE_CONFIG_KEY, 1, arg);
                    break;
                default:
                    printf("Invalid mode selected must be [S]ocket or [C]OM\n");
            }
        } else {
            printf("Missing <arg>\n");
        }
    } else {
        // get mode in slot 0
        ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY, 0, modesz, sizeof(modesz));

        // get arg in slot 1
        ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY, 1, arg, sizeof(arg));

        printf("Current %s '%s'\n", (modesz[0]=='C'?"UART#":"AUTHORITY"), arg);
    }
}

#if defined(CONFIG_STDK_IOT_CORE_SUPPORT_STNV_PARTITION)
/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stserial <serialNumber>
 * ex.
 *   stserial AaBbCcDdEeFf...
 */
static void _cli_cmd_stserial_event(char *string)
{
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "SerialNum", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <serialNumber>\n");
    }
}

/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stpublickey <publicKey>
 * ex.
 *   stpublickey AaBbCcDdEeFf...
 */
static void _cli_cmd_stpublickey_event(char *string)
{
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "PublicKey", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <publicKey>\n");
    }
}

/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stprivatekey <privateKey>
 * ex.
 *   stprivatekey AaBbCcDdEeFf...
 */
static void _cli_cmd_stprivatekey_event(char *string)
{
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "PrivateKey", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <privateKey>\n");
    }
}
#endif



static void _cli_cmd_reboot_event(char *string)
{
    ESP_LOGE(TAG, "%s: rebooting now.", __func__);
    iot_bsp_system_reboot(ctx, true);
}

static void _cli_cmd_cleanup_event(char *string)
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
        "reboot this microcontroller\n", _cli_cmd_reboot_event},
    {"cleanup",
        "Cleanup NV data with reboot option\n"
        "  Syntax: cleanup\n", _cli_cmd_cleanup_event},
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
        "    Note: value -1 will remove an entry.\n", _cli_cmd_code_event},
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
        "  Note: address -1 will remove an entry.\n", _cli_cmd_vpaddr_event},
    {"ad2source",
        "Manage AlarmDecoder protocol source.\n"
        "  Syntax: ad2source <[S]OCK|[C]OM> <AUTHORITY|UART#>\n"
        "  Examples:\n"
        "    Show current mode\n"
        "      ad2source"
        "    Set source to ser2sock client at address and port\n"
        "      ad2source SOCK 192.168.1.2:10000\n"
        "    Set source to local attached uart #2\n"
        "      ad2source COM 2\n", _cli_cmd_ad2source_event},
#if defined(CONFIG_STDK_IOT_CORE_SUPPORT_STNV_PARTITION)
    {"stserial",
        "Sets the SmartThings device_info serialNumber.\n"
        "  Syntax: stserial <serialNumber>\n"
        "  Example: stserial AaBbCcDdEeFfGg...\n", _cli_cmd_stserial_event},
    {"stpublickey",
        "Sets the SmartThings device_info publicKey.\n"
        "  Syntax: stpublickey <publicKey>\n"
        "  Example: stpublickey AaBbCcDdEeFfGg...\n", _cli_cmd_stpublickey_event},
    {"stprivatekey",
        "Sets the SmartThings device_info privateKey.\n"
        "  Syntax: stprivatekey <privateKey>\n"
        "  Example: stprivatekey AaBbCcDdEeFfGg...\n", _cli_cmd_stprivatekey_event},
#endif
};

void register_iot_cli_cmd(void) {
    for (int i = 0; i < ARRAY_SIZE(cmd_list); i++)
        cli_register_command(&cmd_list[i]);
}
