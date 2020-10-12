/**
 *  @file    ad2_cli_cmd.c
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

// common includes
// stdc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>

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
static const char *TAG = "AD2CLICMD";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"
#include "device_control.h"

// specific includes
#include "ad2_cli_cmd.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the USER code for a given slot.
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: code <slot> <code>
 *   Valid slots are from 0 to AD2_MAX_CODE
 *   where slot 0 is the default user.
 *
 *    example.
 *      AD2IOT # code 1 1234
 *
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
 * @brief Set the Virtual Partition address code for a given slot.
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: vpaddr <slot> <address>
 *   Valid slots are from 0 to AD2_MAX_VPARTITION
 *   where slot 0 is the default.
 *
 *   example.
 *     AD2IOT # vpaddr c 2
 *     AD2IOT # vpaddr s 192.168.1.2:10000
 *
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
        ESP_LOGE(TAG, "%s: Error (args) invalid slot # (0-%i).", __func__, AD2_MAX_VPARTITION);
    }
}

/**
 * @brief Configure the AD2IoT connection to the AlarmDecoder device
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: ad2source <mode> <arg>
 *   examples.
 *     AD2IOT # ad2source c 17:16
 *                          [TX PIN:RX PIN]
 *     AD2IOT # ad2source s 192.168.1.2:10000
 *                          [HOST:PORT]
 */
static void _cli_cmd_ad2source_event(char *string)
{
    char modesz[2]; // just a single byte and null term
    char arg[AD2_MAX_MODE_ARG_SIZE]; // Big enough for a long(ish) host name

    if (ad2_copy_nth_arg(modesz, string, sizeof(modesz), 1) >= 0) {
        // upper case it all
        uint8_t mode = toupper((int)modesz[0]);
        modesz[0] = mode;
        modesz[1] = 0;
        if (ad2_copy_nth_arg(arg, string, sizeof(arg), 2) >= 0) {
            switch (mode) {
            case 'S':
            case 'C':
                // save mode in slot 0
                ad2_set_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                           AD2MODE_CONFIG_MODE_SLOT, modesz);
                // save arg in slot 1
                ad2_set_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                           AD2MODE_CONFIG_ARG_SLOT, arg);
                break;
            default:
                printf("Invalid mode selected must be [S]ocket or [C]OM\n");
            }
        } else {
            printf("Missing <arg>\n");
        }
    } else {
        // get mode in slot 0
        ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                   AD2MODE_CONFIG_MODE_SLOT, modesz, sizeof(modesz));

        // get arg in slot 1
        ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                   AD2MODE_CONFIG_ARG_SLOT, arg, sizeof(arg));

        printf("Current %s '%s'\n", (modesz[0]=='C'?"COM TXPIN:RXPIN":"SOCKET AUTHORITY"), arg);
    }
}

/**
 * @brief Configure the AD2IoT connection to the AlarmDecoder device
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: ad2term
 *   Note) To exit press '.' three times fast.
 *
 */
static void _cli_cmd_ad2term_event(char *string)
{
    printf("Locking main threads. Send '.' 3 times to break out and return.\n");
    portENTER_CRITICAL(&spinlock);
    g_StopMainTask = 2;
    portEXIT_CRITICAL(&spinlock);

    // proccess data from AD2* and send to UART0
    // any data on UART0 send to AD2*
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];

    while (1) {

        // UART source to host
        if (g_ad2_mode == 'C') {
            // Read data from the UART
            int len = uart_read_bytes((uart_port_t)g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 20 / portTICK_PERIOD_MS);
            if (len == -1) {
                // An error happend. Sleep for a bit and try again?
                ESP_LOGE(TAG, "Error reading for UART aborting task.");
                break;
            }
            if (len>0) {
                rx_buffer[len] = 0;
                printf((char*)rx_buffer);
                fflush(stdout);
            }

            // Socket source
        } else if (g_ad2_mode == 'S') {
            if (g_ad2_network_state == AD2_CONNECTED) {
                int len = recv(g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 0);

                // test if error occurred
                if (len < 0) {
                    if ( errno != EAGAIN && errno == EWOULDBLOCK ) {
                        ESP_LOGE(TAG, "ser2sock client recv failed: errno %d", errno);
                        break;
                    }
                }
                // Data received
                else {
                    // Parse data from AD2* and report back to host.
                    rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                    printf((char*)rx_buffer);
                    fflush(stdout);
                }
            }

            // should not happen
        } else {
            ESP_LOGW(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
            printf("AD2IoT operating mode configured. Configure using ad2source command.\n");
            break;
        }

        // Host to AD2*
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len == -1) {
            // An error happend. Sleep for a bit and try again?
            ESP_LOGE(TAG, "Error reading for UART aborting task.");
            break;
        }
        if (len>0) {

            // Detect "..." break sequence.
            static uint8_t break_count = 0;
            if (rx_buffer[0] == '.') { // note perfect peek at first byte.
                break_count++;
                if (break_count > 2) {
                    break_count = 0;
                    break;
                }
            } else {
                break_count = 0;
            }

            // null terminate and send the message to the AD2*
            rx_buffer[len] = 0;
            ad2_send((char*)rx_buffer);
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    printf("Resuming main threads.\n");
    portENTER_CRITICAL(&spinlock);
    g_StopMainTask = 0;
    portEXIT_CRITICAL(&spinlock);
}

/**
 * @brief event handler for reboot command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_reboot_event(char *string)
{
    ESP_LOGE(TAG, "%s: rebooting now.", __func__);
    printf("Restarting now\n");
    hal_restart();
}

/**
 * @brief event handler for netmode command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_netmode_event(char *string)
{
    ESP_LOGI(TAG, "%s: Setting network mode (%s).", __func__, string);
    char arg[200];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        char mode = toupper((int)arg[0]);
        switch(mode) {
        case 'N':
        case 'W':
        case 'E':
            ad2_set_nv_slot_key_int(NETMODE_CONFIG_KEY, 0, mode);
            ad2_copy_nth_arg(arg, string, sizeof(arg), 2);
            ad2_set_nv_slot_key_string(NETMODE_CONFIG_KEY, 1, arg);
            break;
        default:
            printf("Unknown network mode('%c') error.\n", mode);
            break;
        }
    }
    // show current mode.
    ad2_get_nv_slot_key_string(NETMODE_CONFIG_KEY, 1, arg, sizeof(arg));
    std::string args;
    printf("The current network mode is '%c' with args '%s'\n", ad2_network_mode(args), arg);
}

/**
 * @brief virtual button press event.
 *
 * @param [in]string command buffer pointer.
 *
 */
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
    //FIXME: button_event(ctx, type, count);
}

/**
 * @brief command list for module
 */
static struct cli_command cmd_list[] = {
    {
        (char*)AD2_REBOOT,(char*)
        "- Reboot the device.\n\n"
        "  ```" AD2_REBOOT "```\n\n", _cli_cmd_reboot_event
    },
    {
        (char*)AD2_NETMODE,(char*)
        "- Manage network connection type.\n\n"
        "  ```" AD2_NETMODE " {mode} [args]```\n\n"
        "  - {mode}\n"
        "    - [N]one: (default) Do not enable any network let component(s) manage the networking.\n"
        "    - [W]iFi: Enable WiFi network driver.\n"
        "    - [E]thernet: Enable ethernet network driver.\n"
        "  - [arg]\n"
        "    - Argument string name value pairs sepearted by &.\n"
        "      - Keys: MODE,IP,MASK,GW,DNS1,DNS2,SID,PASSWORD\n\n"
        "  Examples\n"
        "    - WiFi DHCP with SID and password.\n"
        "      - netmode W mode=d&sid=example&password=somethingsecret\n"
        "    - Ethernet DHCP DNS2 override.\n"
        "      - netmode E mode=d&dns2=4.2.2.2\n"
        "    - Ethernet Static IPv4 address\n"
        "      - netmode E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8\n"
        , _cli_cmd_netmode_event
    },
    {
        (char*)AD2_BUTTON,(char*)
        "- Simulate a button press event.\n\n"
        "  ```" AD2_BUTTON " {count} {type}```\n\n"
        "  - {count}\n"
        "    - Number of times the button was pushed.\n"
        "  - {type}\n"
        "    - The type of event 'short' or 'long'.\n\n"
        "  Examples\n"
        "    - Send a single LONG button press.\n"
        "      - " AD2_BUTTON " 1 long\n", _cli_cmd_butten_event
    },
    {
        (char*)AD2_CODE,(char*)
        "- Manage user codes.\n\n"
        "  ```" AD2_CODE " {id} [value]```\n\n"
        "  - {id}\n"
        "    - Index of code to evaluate. 0 is default.\n"
        "  - [value]\n"
        "    - A valid alarm code or -1 to remove.\n\n"
        "  Examples\n"
        "    - Set default code to 1234\n"
        "      - " AD2_CODE " 0 1234\n"
        "    - Set alarm code for slot 1\n"
        "      - " AD2_CODE " 1 1234\n"
        "    - Show code in slot #3\n"
        "      - " AD2_CODE " 3\n"
        "    - Remove code for slot 2\n"
        "      - " AD2_CODE " 2 -1\n\n"
        "    Note: value -1 will remove an entry.\n", _cli_cmd_code_event
    },
    {
        (char*)AD2_VPADDR,(char*)
        "- Manage virtual partitions\n\n"
        "  ```" AD2_VPADDR " {id} {value}```\n\n"
        "  - {id}\n"
        "    - The virtual partition ID. 0 is the default.\n"
        "  - [value]\n"
        "    - (Ademco)Keypad address or (DSC)Partion #. -1 to delete.\n\n"
        "  Examples\n"
        "    - Set default address mask to 18 for an Ademco system.\n"
        "      - " AD2_VPADDR " 0 18\n"
        "    - Set default send partition to 1 for a DSC system.\n"
        "      - " AD2_VPADDR " 0 1\n"
        "    - Show address for partition 2\n"
        "      - " AD2_VPADDR " 2\n"
        "    - Remove virtual partition in slot 2\n"
        "      - " AD2_VPADDR " 2 -1\n\n"
        "    Note: address -1 will remove an entry.\n", _cli_cmd_vpaddr_event
    },
    {
        (char*)AD2_SOURCE,(char*)
        "- Manage AlarmDecoder protocol source.\n\n"
        "  ```" AD2_SOURCE " [{mode} {arg}]```\n\n"
        "  - {mode}\n"
        "    - [S]ocket: Use ser2sock server over tcp for AD2* messages.\n"
        "    - [C]om port: Use local UART for AD2* messages.\n"
        "  - {arg}\n"
        "    - [S]ocket arg: {HOST:PORT}\n"
        "    - [C]om arg: {TXPIN:RXPIN}.\n\n"
        "  Examples:\n"
        "    - Show current mode\n"
        "      - " AD2_SOURCE "\n"
        "    - Set source to ser2sock client at address and port\n"
        "      - " AD2_SOURCE " SOCK 192.168.1.2:10000\n"
        "    - Set source to local attached uart with TX on GPIP 17 and RX on GPIO 16\n"
        "      - " AD2_SOURCE " COM 17:16\n", _cli_cmd_ad2source_event
    },
    {
        (char*)AD2_TERM,(char*)
        "- Connect directly to the AD2* source and halt processing.\n\n"
        "  ```" AD2_TERM "```\n\n"
        "  Note: To exit press ... three times fast.\n", _cli_cmd_ad2term_event
    }
};

/**
 * @brief Register ad2 CLI commands.
 *
 */
void register_ad2_cli_cmd(void)
{
    for (int i = 0; i < ARRAY_SIZE(cmd_list); i++) {
        cli_register_command(&cmd_list[i]);
    }
}

#ifdef __cplusplus
} // extern "C"
#endif
