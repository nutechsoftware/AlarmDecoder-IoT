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
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "AD2CLICMD";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "driver/uart.h"

// specific includes
#include "ad2_cli_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the alpha descriptor for a given zone.
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: zone <number> <string>
 *   Valid zones are from 1 to AD2_MAX_ZONES
 *
 *    example.
 *      AD2IOT # zone 1 TESTING ZONE ALPHA
 *
 */
static void _cli_cmd_zone_event(const char *string)
{
    std::string arg;
    int zone = 0;

    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        zone = strtol(arg.c_str(), NULL, 10);
    }

    if (zone > 0 && zone <= AD2_MAX_ZONES) {
        std::string _section = std::string(AD2ZONE_CONFIG_SECTION " ") + std::to_string(zone);
        if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
            if (arg.length()) {
                if (arg[0] == '-') {
                    ad2_printf_host(false, "Removing settings string for zone %i...\r\n", zone);
                    ad2_set_config_key_string(_section.c_str(), ZONE_CONFIG_DESCRIPTION, arg.c_str(), true);
                } else {
                    ad2_printf_host(false, "Saving settings string for zone %i to '%s'\r\n", zone, arg.c_str());
                    ad2_set_config_key_string(_section.c_str(), ZONE_CONFIG_DESCRIPTION, arg.c_str());
                }
            }
        } else {
            // show contents of this slot
            std::string buf;
            ad2_get_config_key_string(_section.c_str(), ZONE_CONFIG_DESCRIPTION, buf);
            ad2_printf_host(false, "The settings for zone %i is '%s'\r\n", zone, buf.c_str());
        }
    } else {
        ad2_printf_host(false, "Error invalid zone zone %i\r\n", zone);
    }
}

/**
 * @brief Set the alarm code for a given code slot/id.
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
static void _cli_cmd_code_event(const char *string)
{
    std::string arg;
    int slot = 0;

    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        slot = strtol(arg.c_str(), NULL, 10);
    }

    if (slot >= 0 && slot <= AD2_MAX_CODE) {
        if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
            if (arg.length()) {
                if (arg[0] == '-') {
                    ad2_printf_host(false, "Removing code in slot %i...\r\n", slot);
                    ad2_set_config_key_string(AD2CODES_CONFIG_SECTION, nullptr, nullptr, true, slot);
                } else {
                    ad2_printf_host(false, "Setting code in slot %i to '%s'...\r\n", slot, arg.c_str());
                    ad2_set_config_key_string(AD2CODES_CONFIG_SECTION, nullptr, arg.c_str(), false, slot);
                }
            }
        } else {
            // show contents of this slot
            std::string buf;
            ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, nullptr, buf, slot);
            ad2_printf_host(false, "The code in slot %i is '%s'\r\n", slot, buf.c_str());
        }
    } else {
        ESP_LOGE(TAG, "%s: Error (args) invalid slot # (0-%i).", __func__, AD2_MAX_CODE);
    }
}

/**
 * @brief Set the Address for a given partition slot/id.
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: part <slot> <address> <zone list>
 *   Valid slots are from 0 to AD2_MAX_PARTITION
 *   where slot 0 is the default.
 *
 *   example.
 *     AD2IOT # part 0 18 2,3,4,5
 *     AD2IOT # part 1 19 6,7
 *
 */
static void _cli_cmd_part_event(const char *string)
{
    std::string buf;
    int part = 0;
    int address = 0;

    if (ad2_copy_nth_arg(buf, string, 1) >= 0) {
        part = strtol(buf.c_str(), NULL, 10);
    }

    if (part >= 0 && part <= AD2_MAX_PARTITION) {
        std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(part);
        if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
            int address = strtol(buf.c_str(), NULL, 10);
            if (address>=0 && address < AD2_MAX_ADDRESS) {
                ad2_set_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, address);
                ad2_copy_nth_arg(buf, string, 3, true);
                ad2_set_config_key_string(_section.c_str(), PART_CONFIG_ZONES, buf.c_str());
                ad2_printf_host(false, "Setting partition %i to address '%i' with zone list '%s'.\r\n", part, address, buf.c_str());
            } else {
                // delete entry
                ad2_printf_host(false, "Deleting partition %i...\r\n", part);
                ad2_set_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, -1, true);
                ad2_set_config_key_string(_section.c_str(), PART_CONFIG_ZONES, nullptr, true);
            }
        } else {
            // show contents of this partition
            ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);
            ad2_get_config_key_string(_section.c_str(), PART_CONFIG_ZONES, buf);
            ad2_printf_host(false, "The partition %i uses address %i with a zone list of '%s'\r\n", part, address, buf.c_str());
        }
    } else {
        ESP_LOGE(TAG, "%s: Error (args) invalid partition # (0-%i).", __func__, AD2_MAX_PARTITION);
    }
}

/**
 * @brief Configure the AD2IoT connection to the AlarmDecoder device
 *
 * @param [in]string command buffer pointer.
 *
 * @note command: ad2source <mode> <arg>
 *   examples.
 *     AD2IOT # ad2source c 4:36
 *                          [TX PIN:RX PIN]
 *     AD2IOT # ad2source s 192.168.1.2:10000
 *                          [HOST:PORT]
 */
static void _cli_cmd_ad2source_event(const char *string)
{
    std::string mode;
    std::string arg;
    std::string modestring;

    if (ad2_copy_nth_arg(mode, string, 1) >= 0) {

        // upper case it all
        ad2_ucase(mode);
        ad2_trim(mode);

        if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
            switch (mode[0]) {
            case 'S':
            case 'C':
                ad2_copy_nth_arg(arg, string, 2, true);
                modestring = mode + " " + arg;
                ad2_set_config_key_string(AD2MAIN_CONFIG_SECTION, AD2MODE_CONFIG_KEY, modestring.c_str());
                ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                break;
            default:
                ad2_printf_host(false, "Invalid mode selected must be [S]ocket or [C]OM\r\n");
            }
        } else {
            ad2_printf_host(false, "Missing <arg>\r\n");
        }
    } else {
        // Get current string
        ad2_get_config_key_string(AD2MAIN_CONFIG_SECTION, AD2MODE_CONFIG_KEY, modestring);
    }
    // get and show current mode string and arg.
    ad2_get_config_key_string(AD2MAIN_CONFIG_SECTION, AD2MODE_CONFIG_KEY, modestring);
    ad2_copy_nth_arg(arg, modestring.c_str(), 2, true);
    ad2_printf_host(false, "Current mode '%s %s'\r\n", (modestring[0]=='C'?"COM":"SOCKET"), arg.c_str());

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
static void _cli_cmd_ad2term_event(const char *string)
{
    ad2_printf_host(false, "Halting command line interface. Send '.' 3 times to break out and return.\r\n");
    taskENTER_CRITICAL(&spinlock);
    // save the main task state for later restore.
    int save_StopMainTask = g_StopMainTask;
    // set main task to halted.
    g_StopMainTask = 2;
    taskEXIT_CRITICAL(&spinlock);

    // let other tasks have time to stop.
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // if argument provided then assert reset pin on AD2pHAT board on GPIO
    std::string arg;
    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        hal_ad2_reset();
    }

    // proccess data from AD2* and send to UART0
    // any data on UART0 send to AD2*
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];

    // break sequence state
    static uint8_t break_count = 0;

    while (1) {

        // UART source to host
        if (g_ad2_mode == 'C') {
            // Read data from the UART
            int len = uart_read_bytes((uart_port_t)g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);
            if (len == -1) {
                // An error happend. Sleep for a bit and try again?
                ESP_LOGE(TAG, "Error reading for UART aborting task.");
                break;
            }
            if (len>0) {
                rx_buffer[len] = 0;
                ad2_printf_host(false, (char*)rx_buffer);
                fflush(stdout);
            }

            // Socket source
        } else if (g_ad2_mode == 'S') {
            if (hal_get_network_connected()) {
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
                    ad2_printf_host(false, (char*)rx_buffer);
                    fflush(stdout);
                }
            }

            // should not happen
        } else {
            ESP_LOGW(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
            ad2_printf_host(false, "AD2IoT operating mode configured. Configure using ad2source command.\r\n");
            break;
        }

        // Host to AD2*
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);
        if (len == -1) {
            // An error happend. Sleep for a bit and try again?
            ESP_LOGE(TAG, "Error reading for UART aborting task.");
            break;
        }
        if (len>0) {

            // Detect "..." break sequence in stream;
            for (int t = 0; t < len; t++) {
                if (rx_buffer[t] == '.') { // note perfect peek at first byte.
                    break_count++;
                    if (break_count > 2) {
                        // break detected we are done!
                        break;
                    }
                } else {
                    break_count = 0;
                }
            }
            // clear break state and exit while if break detected
            if (break_count > 2) {
                break_count = 0;
                break;
            }

            // null terminate and send the message to the AD2*
            rx_buffer[len] = 0;
            std::string temp = (char *)rx_buffer;
            ad2_send(temp);
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ad2_printf_host(false, "Resuming command line interface threads.\r\n");
    taskENTER_CRITICAL(&spinlock);
    // restore last state.
    g_StopMainTask = save_StopMainTask;
    taskEXIT_CRITICAL(&spinlock);
}

/**
 * @brief event handler for restart command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_restart_event(const char *string)
{
    // Flush/save persistent configuration ini
    ad2_save_persistent_config();

    hal_restart();
}

/**
 * @brief event handler for factory-reset command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_factory_reset_event(const char *string)
{
    hal_factory_reset();
    hal_restart();
}


/**
 * @brief event handler for netmode command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_netmode_event(const char *string)
{
    ESP_LOGD(TAG, "%s: Setting network mode (%s).", __func__, string);
    std::string mode;
    std::string arg;
    std::string modestring;

    if (ad2_copy_nth_arg(mode, string, 1) >= 0) {

        // upper case it all
        ad2_ucase(mode);

        switch(mode[0]) {
        case 'N':
        case 'W':
        case 'E':
            ad2_copy_nth_arg(arg, string, 2, true);
            modestring = mode + " " + arg;
            ad2_set_config_key_string(AD2MAIN_CONFIG_SECTION, NETMODE_CONFIG_KEY, modestring.c_str());
            ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
            break;
        default:
            ad2_printf_host(false, "Unknown network mode('%c') error.\r\n", mode[0]);
            break;
        }
    }
    // show current mode.
    char cmode = ad2_get_network_mode(arg);
    ad2_printf_host(false, "The current network mode is '%c' with args '%s'.\r\n", cmode, arg.c_str());
}

/**
 * @brief event handler for logging command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_ad2logmode_event(const char *string)
{
    ESP_LOGD(TAG, "%s: Setting logging mode (%s).", __func__, string);
    std::string mode;
    std::string arg;
    std::string modestring;

    if (ad2_copy_nth_arg(mode, string, 1) >= 0) {

        // upper case it all
        ad2_ucase(mode);

        switch(mode[0]) {
        case 'N':
        case 'D':
        case 'I':
        case 'V':
            ad2_set_config_key_string(AD2MAIN_CONFIG_SECTION, LOGMODE_CONFIG_KEY, mode.c_str());

            break;
        default:
            ad2_printf_host(false, "Unknown logging mode('%c') error.\r\n", mode[0]);
            break;
        }
    }
    // show current mode.
    char cmode = ad2_get_log_mode();
    ad2_printf_host(false, "The current logging mode mode is '%c'.\r\n", cmode);
}


/**
 * @brief virtual switch command event.
 *  ex)
 *    switch 1 open 1 regex_string
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_switch_event(const char *command_string)
{
    int sId;
    std::string setting;
    std::string arg;
    std::string sztmp;
    int itmp;
    int sk_index = 0;
    std::string sk;

    // get the switch Id from the command string
    sztmp = "-1"; // default if not found
    ad2_copy_nth_arg(sztmp, command_string, 1);
    // safe convert to int
    sId = std::atoi(sztmp.c_str());
    if (sId <= 0 || sId > AD2_MAX_SWITCHES) {
        ad2_printf_host(false, "Invalid switch id(%i) error.\r\n", sId);
        return;
    }

    // root config key for [switch N]
    std::string key = std::string(AD2SWITCH_CONFIG_SECTION);
    key += " ";
    key += std::to_string(sId);

    // get the switch setting from the command string
    ad2_copy_nth_arg(setting, command_string, 2);
    ad2_lcase(setting);
    ad2_trim(setting);


    // Helper to dump switch settings to user if no setting argument is given
    // ex. 'switch 1'
    if (!setting.length()) {
        // check all settings verbs against provided verb and proform setting logic
        std::stringstream ss(
            AD2SWITCH_SK_DEFAULT " " // 1
            AD2SWITCH_SK_RESET " "   // 2
            AD2SWITCH_SK_TYPES " "   // 3
            AD2SWITCH_SK_FILTER " "  // 4
            AD2SWITCH_SK_OPEN " "
            AD2SWITCH_SK_CLOSE " "
            AD2SWITCH_SK_TROUBLE);

        bool command_found = false;
        ad2_printf_host(false, "## switch %i global configuration.\r\n[%s]\r\n", sId, key.c_str());
        sk_index = 0;
        while (ss >> sk) {
            sk_index++; // 1 - N
            switch(sk_index) {
            case 1: // default
            case 2: // reset
                itmp = -1;
                ad2_get_config_key_int(key.c_str(), sk.c_str(), &itmp);
                if (itmp > -1) {
                    ad2_printf_host(false, "%s = %i\r\n", sk.c_str(), itmp);
                } else {
                    ad2_printf_host(false, "# %s = \r\n", sk.c_str());
                }
                break;
            case 3: // type
            case 4: // filter
                sztmp = "";
                ad2_get_config_key_string(key.c_str(), sk.c_str(), sztmp);
                if (sztmp.length()) {
                    ad2_printf_host(false, "%s = %s\r\n", sk.c_str(), sztmp.c_str());
                } else {
                    ad2_printf_host(false, "# %s = \r\n", sk.c_str());
                }
                break;
            case 5: // open
            case 6: // close
            case 7: // trouble
                // 1 - MAX_SEARCH_KEYS
                itmp = 0; // test for no settings
                for ( int i = 1; i < AD2_MAX_SWITCH_SEARCH_KEYS; i++ ) {
                    sztmp = "";
                    ad2_get_config_key_string(key.c_str(), sk.c_str(), sztmp, i);
                    if (sztmp.length()) {
                        itmp ++;
                        ad2_printf_host(false, "%s %i = %s\r\n", sk.c_str(), i, sztmp.c_str());
                    }
                }
                // no settings found for sub key
                if (!itmp) {
                    ad2_printf_host(false, "# %s [N] = \r\n", sk.c_str());
                }
                break;
            }
        }
        // dump finished, all done.
        return;
    }

    // get the switch command arg to end of string.
    ad2_copy_nth_arg(arg, command_string, 3, true);


    // check all settings verbs against provided verb and proform setting logic
    std::stringstream ss(AD2SWITCH_SK_DELETE1 " " AD2SWITCH_SK_DELETE2 " "
                         AD2SWITCH_SK_DEFAULT " "
                         AD2SWITCH_SK_RESET " "
                         AD2SWITCH_SK_TYPES " "
                         AD2SWITCH_SK_FILTER " "
                         AD2SWITCH_SK_OPEN " "
                         AD2SWITCH_SK_CLOSE " "
                         AD2SWITCH_SK_TROUBLE);

    sk_index = 0;
    bool command_found = false;
    while (ss >> sk) {
        sk_index++; // 1 - N
        if (sk.compare(setting) == 0) {
            command_found = true;
            switch(sk_index) {
            case 1: // -
            case 2: // delete
                // TODO: Delete topic [switch N]
                ad2_set_config_key_int(key.c_str(), AD2SWITCH_SK_DEFAULT, 0, true);
                ad2_set_config_key_int(key.c_str(), AD2SWITCH_SK_RESET, 0, true);
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_TYPES, NULL, true);
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_FILTER, NULL, true);
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_OPEN, NULL, true);
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_CLOSE, NULL, true);
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_TROUBLE, NULL, true);
                break;
            case 3: // default
                // TODO: validate
                ad2_set_config_key_int(key.c_str(), AD2SWITCH_SK_DEFAULT, std::atoi(arg.c_str()));
                break;
            case 4: // reset
                // TODO: validate
                ad2_set_config_key_int(key.c_str(), AD2SWITCH_SK_RESET, std::atoi(arg.c_str()));
                break;
            case 5: // type
                // TODO: validate
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_TYPES, arg.c_str());
                break;
            case 6: // filter
                // TODO: validate
                ad2_set_config_key_string(key.c_str(), AD2SWITCH_SK_FILTER, arg.c_str());
                break;
            case 7: // open
            case 8: // close
            case 9: // trouble
                // TODO: validate
                ad2_set_config_key_string(key.c_str(), sk.c_str(), arg.c_str());
                break;
            }
            // all done.
            break;
        }
    }
    if (!command_found) {
        ad2_printf_host(false, "Unknown setting argument '%s'.\r\n", setting.c_str());
    }
}

/**
 * @brief command list for module
 */
static struct cli_command cmd_list[] = {
    {
        (char*)AD2_CMD_REBOOT,(char*)
        "- Save config changes and restart the device.\r\n"
        "  - ```" AD2_CMD_REBOOT "```\r\n", _cli_cmd_restart_event
    },
    {
        (char*)AD2_CMD_NETMODE,(char*)
        "- Manage network connection type.\r\n"
        "  - ```" AD2_CMD_NETMODE " {mode} [args]```\r\n"
        "    - {mode}\r\n"
        "      - [N]one: (default) Do not enable any network let component(s) manage the networking.\r\n"
        "      - [W]iFi: Enable WiFi network driver.\r\n"
        "      - [E]thernet: Enable ethernet network driver.\r\n"
        "    - [arg]\r\n"
        "      - Argument string name value pairs sepearted by &.\r\n"
        "        - Keys: MODE,IP,MASK,GW,DNS1,DNS2,SID,PASSWORD\r\n"
        "  - Examples\r\n"
        "    - WiFi DHCP with SID and password.\r\n"
        "      - ```" AD2_CMD_NETMODE " W mode=d&sid=example&password=somethingsecret```\r\n"
        "    - Ethernet DHCP DNS2 override.\r\n"
        "      - ```" AD2_CMD_NETMODE " E mode=d&dns2=4.2.2.2```\r\n"
        "    - Ethernet Static IPv4 address.\r\n"
        "      - ```" AD2_CMD_NETMODE " E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8```\r\n\r\n"
        , _cli_cmd_netmode_event
    },
    {
        (char*)AD2_CMD_SWITCH,(char*)
        "- Manage virtual switch settings.\r\n"
        "  - ```" AD2_CMD_SWITCH " {id} {setting} {arg1} [arg2]```\r\n"
        "    - {id}\r\n"
        "      - 1-255 : Virtual switches ID.\r\n"
        "    - {setting}\r\n"
        "      - [-|delete] Delete switch\r\n"
        "      - [default] Default state\r\n"
        "        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)\r\n"
        "      - [reset] Auto reset.\r\n"
        "        - {arg1}:  time in ms 0 to disable\r\n"
        "      - [type] Message type filter.\r\n"
        "        - {arg1}: Message type list separated by ',' or empty to disables filter.\r\n"
        "          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]\r\n"
        "            - For EVENT type the message will be generated by the API and not the AD2\r\n"
        "      - [filter] Pre filter REGEX or empty to disable.\r\n"
        "      - [open] Open(ON) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string  to clear\r\n"
        "      - [close] Close(OFF) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string to clear\r\n"
        "      - [trouble] Trouble state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty  string to clear\r\n\r\n", _cli_cmd_switch_event
    },
    {
        (char*)AD2_CMD_ZONE,(char*)
        "- Manage zone description string.\r\n"
        "  - ```" AD2_CMD_ZONE " {id} [value]```\r\n"
        "    - {id}\r\n"
        "      - Zone number 1-255.\r\n"
        "    - [value]\r\n"
        "      - json zone description string for this zone.\r\n"
        "  - Examples\r\n"
        "    - Set zone 1 configuration string.\r\n"
        "      - ```" AD2_CMD_ZONE " 1 {\"type\": \"smoke\", \"alpha\": \"TESTING LAB SMOKE\"}```\r\n"
        "    - Remove zone 1 settings string.\r\n"
        "      - ```" AD2_CMD_ZONE " 1 -```\r\n\r\n", _cli_cmd_zone_event
    },
    {
        (char*)AD2_CMD_CODE,(char*)
        "- Manage user codes.\r\n"
        "  - ```" AD2_CMD_CODE " {id} [value]```\r\n"
        "    - {id}\r\n"
        "      - Index of code to evaluate. 0 is default.\r\n"
        "    - [value]\r\n"
        "      - A valid alarm code or -1 to remove.\r\n"
        "  - Examples\r\n"
        "    - Set default code to 1234.\r\n"
        "      - ```" AD2_CMD_CODE " 0 1234```\r\n"
        "    - Set alarm code for slot 1.\r\n"
        "      - ```" AD2_CMD_CODE " 1 1234```\r\n"
        "    - Show code in slot #3.\r\n"
        "      - ```" AD2_CMD_CODE " 3```\r\n"
        "    - Remove code for slot 2.\r\n"
        "      - ```" AD2_CMD_CODE " 2 -```\r\n"
        "        - Note: value -1 will remove an entry.\r\n\r\n", _cli_cmd_code_event
    },
    {
        (char*)AD2_CMD_PART,(char*)
        "- Manage partitions.\r\n"
        "  - ```" AD2_CMD_PART " {id} {value} [zone list]```\r\n"
        "    - {id}\r\n"
        "      - The partition ID. 0 is the default.\r\n"
        "    - {value}\r\n"
        "      - (Ademco)Keypad address or (DSC)Partion #. -1 to delete.\r\n"
        "    - [zone list]\r\n"
        "      - Comma separated list of zone numbers associated with this partition for tracking.\r\n"
        "  - Examples\r\n"
        "    - Set default address mask to 18 for an Ademco system with zones 2, 3, and 4.\r\n"
        "      - ```" AD2_CMD_PART " 0 18 2,3,4```\r\n"
        "    - Set default send partition to 1 for a DSC system.\r\n"
        "      - ```" AD2_CMD_PART " 0 1```\r\n"
        "    - Show address for partition 2.\r\n"
        "      - ```" AD2_CMD_PART " 2```\r\n"
        "    - Remove partition in slot 2.\r\n"
        "      - ```" AD2_CMD_PART " 2 -1```\r\n"
        "       - Note: address -1 will remove an entry.\r\n\r\n", _cli_cmd_part_event
    },
    {
        (char*)AD2_CMD_SOURCE,(char*)
        "- Manage AlarmDecoder protocol source.\r\n"
        "  - ```" AD2_CMD_SOURCE " [{mode} {arg}]```\r\n"
        "    - {mode}\r\n"
        "      - [S]ocket: Use ser2sock server over tcp for AD2* messages.\r\n"
        "      - [C]om port: Use local UART for AD2* messages.\r\n"
        "    - {arg}\r\n"
        "      - [S]ocket arg: {HOST:PORT}\r\n"
        "      - [C]om arg: {TXPIN:RXPIN}.\r\n"
        "  - Examples:\r\n"
        "    - Show current mode.\r\n"
        "      - ```" AD2_CMD_SOURCE "```\r\n"
        "    - Set source to ser2sock client at address and port.\r\n"
        "      - ```" AD2_CMD_SOURCE " SOCK 192.168.1.2:10000```\r\n"
        "    - Set source to local attached uart with TX on GPIO 17 and RX on GPIO 16.\r\n"
        "      - ```" AD2_CMD_SOURCE " COM 17:16```\r\n\r\n", _cli_cmd_ad2source_event
    },
    {
        (char*)AD2_CMD_TERM,(char*)
        "- Connect directly to the AD2* source and halt processing with option to hard reset AD2pHat.\r\n"
        "  - ```" AD2_CMD_TERM " [reset]```\r\n"
        "    - Note: To exit press ... three times fast.\r\n\r\n", _cli_cmd_ad2term_event
    },
    {
        (char*)AD2_CMD_LOGMODE,(char*)
        "- Set logging mode.\r\n"
        "  - ```" AD2_CMD_LOGMODE " {level}```\r\n"
        "    - {level}\r\n"
        "      - [I]nformational\r\n"
        "      - [V]Verbose\r\n"
        "      - [D]ebugging\r\n"
        "      - [N]one: (default) Warnings and errors only.\r\n"
        "  - Examples:\r\n"
        "    - Set logging mode to INFO.\r\n"
        "      - ```" AD2_CMD_LOGMODE " I```\r\n\r\n", _cli_cmd_ad2logmode_event
    },
    {
        (char*)AD2_CMD_FACTORY,(char*)
        "- Erase all NVS storage clearing all settings and reboot.\r\n"
        "  - ```" AD2_CMD_FACTORY "```\r\n\r\n, ", _cli_cmd_factory_reset_event
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
