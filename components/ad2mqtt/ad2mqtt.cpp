/**
*  @file    ad2mqtt.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    07/31/2021
*  @version 1.0.0
*
*  @brief Connect to an MQTT broker and publish state updates as
*  well as subscribe to a command topic to listen for verbs.
*
*  @copyright Copyright (C) 2021 Nu Tech Software Solutions, Inc.
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
#include "mqtt_client.h"
#include "esp_tls.h"

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"
#include "device_control.h"

// Disable via sdkconfig
#if CONFIG_AD2IOT_MQTT_CLIENT
static const char *TAG = "MQTT";

// specific includes
#include "ad2mqtt.h"

// mbedtls
#include "mbedtls/base64.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

std::vector<AD2EventSearch *> mqtt_AD2EventSearches;

#define EXAMPLE_BROKER_URI "mqtt://mqtt.eclipseprojects.io"
esp_mqtt_client_handle_t mqtt_client = nullptr;

std::string mqttclient_UUID;



#ifdef __cplusplus
extern "C" {
#endif

#if 0
/**
 * @brief custom outbox handler
 *  see: MQTT_CUSTOM_OUTBOX
 *
 */
esp_err_t outbox_set_pending(outbox_handle_t outbox, int msg_id, pending_state_t pending)
{
    outbox_item_handle_t item = outbox_get(outbox, msg_id);
    if (item) {
        item->pending = pending;
        return ESP_OK;
    }
    return ESP_FAIL;
}
#endif

/**
 * @brief mqtt event callback handler.
 *
 * @param [in]handler_args void *
 * @param [in]base esp_event_base_t
 * @param [in]event_id int32_t
 * @param [in]event_data void *
 *
 */
static esp_err_t ad2_mqtt_event_handler(esp_mqtt_event_handle_t event_data)
{
    int msg_id;
    std::string topic;

    //ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_client_handle_t client = event_data->client;
    // your_context_t *context = event->context;
    switch (event_data->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Subscribe to command inputs
        topic = MQTT_TOPIC_PREFIX "/";
        topic+=mqttclient_UUID;
        topic+="/commands";
        msg_id = esp_mqtt_client_subscribe(client, topic.c_str(), 0);
        ESP_LOGI(TAG, "sent subscribe successful for topic/commands, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event_data->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event_data->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event_data->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event_data->topic_len, event_data->topic);
        printf("DATA=%.*s\r\n", event_data->data_len, event_data->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event_data->event_id);
        break;
    }

    return ESP_OK;
}

/**
 * @brief Generic callback for all AlarmDecoder API event subscriptions.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2VirtualPartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void mqtt_on_state_change(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    int msg_id;
    if (mqtt_client != nullptr && s) {
        ESP_LOGE(TAG, "mqtt_on_state_change partition(%i) event(%s) message('%s')", s->partition, AD2Parse.event_str[(int)arg].c_str(), msg->c_str());
        std::string sTopic = MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/state/";
        sTopic+=ad2_to_string(s->partition);
        cJSON *root = ad2_get_partition_state_json(s);
        cJSON_AddStringToObject(root, "event", AD2Parse.event_str[(int)arg].c_str());
        char *sys_info = cJSON_Print(root);
        cJSON_Minify(sys_info);
        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client, sTopic.c_str(), sys_info, 0, 1, 0, 0);
        ESP_LOGI(TAG, "queue result/message id: %i", msg_id);
        cJSON_Delete(root);
    }
}

/**
 * cleanup memory
 */
void mqtt_free()
{
    __attribute__((__unused__)) esp_err_t err;
    if (mqtt_client) {
        err = esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
}

/**
 * @brief Search match callback.
 * Called when the current message matches a AD2EventSearch test.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_search_match_cb_mqtt(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    AD2EventSearch *es = (AD2EventSearch *)arg;
    ESP_LOGI(TAG, "ON_SEARCH_MATCH_CB: '%s' -> '%s'", msg->c_str(), es->out_message.c_str());
    if (hal_get_network_connected()) {
        std::string message = es->out_message;
        // publishing event
        ESP_LOGI(TAG, "Publishing to MQTT broker '%s'", message.c_str());
    }
}


/**
 * Command support values.
 */
enum {
    MQTT_URL_CFGKEY_ID = 0,
    MQTT_SAS_CFGKEY_ID
};
char * MQTT_SETTINGS [] = {
    (char*)MQTT_URL_CFGKEY,
    (char*)MQTT_SAS_CFGKEY,
    0 // EOF
};

/**
 * MQTT generic command event processing
 *  command: [COMMAND] [SUB] <id> <arg>
 * ex.
 *   [COMMAND] [SUB] 0 arg...
 */
static void _cli_cmd_mqtt_event_generic(std::string &subcmd, char *string)
{
    int slot = -1;
    std::string buf;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    // build the NV storage key concat prefix to sub command. Take human readable and make it less so.
    std::string key = std::string(MQTT_PREFIX) + subcmd.c_str();
    if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
        slot = strtol(buf.c_str(), NULL, 10);
    }
    if (slot >= 0) {
        if (ad2_copy_nth_arg(buf, string, 3) >= 0) {
            ad2_set_nv_slot_key_string(key.c_str(), slot, nullptr, buf.c_str());
            ad2_printf_host("Setting '%s' value finished.\r\n", subcmd.c_str());
        } else {
            buf = "";
            ad2_get_nv_slot_key_string(key.c_str(), slot, nullptr, buf);
            ad2_printf_host("Current slot #%02i '%s' value '%s'\r\n", slot, subcmd.c_str(), buf.length() ? buf.c_str() : "EMPTY");
        }
    } else {
        ad2_printf_host("Missing <slot>\r\n");
    }
}

/**
 * MQTT smart alert switch command event processing
 *  command: [COMMAND] [SUB] <id> <setting> <arg1> <arg2>
 * ex. Switch #0 [N]otification slot 0
 *   [COMMAND] [SUB] 0 N 0
 */
static void _cli_cmd_mqtt_smart_alert_switch(std::string &subcmd, char *instring)
{
    int i;
    int slot = -1;
    std::string buf;
    std::string arg1;
    std::string arg2;
    std::string tmpsz;

    // get the sub command value validation
    std::string key = std::string(MQTT_PREFIX) + subcmd.c_str();

    if (ad2_copy_nth_arg(buf, instring, 2) >= 0) {
        slot = std::atoi (buf.c_str());
    }
    // add the slot# to the key max 99 slots.
    if (slot > 0 && slot < 100) {

        if (ad2_copy_nth_arg(buf, instring, 3) >= 0) {

            // sub key suffix.
            std::string sk = ad2_string_printf("%c", buf[0]);

            /* setting */
            switch(buf[0]) {
            case SK_NOTIFY_SLOT[0]: // Notification slot
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                tmpsz = i < 0 ? "Clearing" : "Setting";
                ad2_set_nv_slot_key_int(key.c_str(), slot, sk.c_str(), i);
                ad2_printf_host("%s smartswitch #%i to use notification settings from slot #%i.\r\n", tmpsz.c_str(), slot, i);
                break;
            case SK_DEFAULT_STATE[0]: // Default state
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(key.c_str(), slot, sk.c_str(), i);
                ad2_printf_host("Setting smartswitch #%i to use default state '%s' %i.\r\n", slot, AD2Parse.state_str[i].c_str(), i);
                break;
            case SK_AUTO_RESET[0]: // Auto Reset
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(key.c_str(), slot, sk.c_str(), i);
                ad2_printf_host("Setting smartswitch #%i auto reset value %i.\r\n", slot, i);
                break;
            case SK_TYPE_LIST[0]: // Message type filter list
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host("Setting smartswitch #%i message type filter list to '%s'.\r\n", slot, arg1.c_str());
                break;
            case SK_PREFILTER_REGEX[0]: // Pre filter REGEX
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host("Setting smartswitch #%i pre filter regex to '%s'.\r\n", slot, arg1.c_str());
                break;

            case SK_OPEN_REGEX_LIST[0]: // Open state REGEX list editor
            case SK_CLOSED_REGEX_LIST[0]: // Closed state REGEX list editor
            case SK_FAULT_REGEX_LIST[0]: // Fault state REGEX list editor
                // Add index to file name to track N number of elements.
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                if ( i > 0 && i < MAX_SEARCH_KEYS) {
                    // consume the arge and to EOL
                    ad2_copy_nth_arg(arg2, instring, 5, true);
                    std::string op = arg2.length() > 0 ? "Setting" : "Clearing";
                    sk+=ad2_string_printf("%02i", i);
                    if (buf[0] == SK_OPEN_REGEX_LIST[0]) {
                        tmpsz = "OPEN";
                    }
                    if (buf[0] == SK_CLOSED_REGEX_LIST[0]) {
                        tmpsz = "CLOSED";
                    }
                    if (buf[0] == SK_FAULT_REGEX_LIST[0]) {
                        tmpsz = "FAULT";
                    }
                    ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg2.c_str());
                    ad2_printf_host("%s smartswitch #%i REGEX filter #%02i for state '%s' to '%s'.\r\n", op.c_str(), slot, i, tmpsz.c_str(), arg2.c_str());
                } else {
                    ad2_printf_host("Error invalid index %i. Valid values are 1-8.\r\n", slot, i);
                }
                break;

            case SK_OPEN_OUTPUT_FMT[0]: // Open state output format string
            case SK_CLOSED_OUTPUT_FMT[0]: // Closed state output format string
            case SK_FAULT_OUTPUT_FMT[0]: // Fault state output format string
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                if (buf[0] == SK_OPEN_OUTPUT_FMT[0]) {
                    tmpsz = "OPEN";
                }
                if (buf[0] == SK_CLOSED_OUTPUT_FMT[0]) {
                    tmpsz = "CLOSED";
                }
                if (buf[0] == SK_FAULT_OUTPUT_FMT[0]) {
                    tmpsz = "FAULT";
                }
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host("Setting smartswitch #%i output format string for '%s' state to '%s'.\r\n", slot, tmpsz.c_str(), arg1.c_str());
                break;

            default:
                ESP_LOGW(TAG, "Unknown setting '%c' ignored.", buf[0]);
                return;
            }
        } else {
            // query show contents.
            int i;
            std::string out;
            std::string args = SK_NOTIFY_SLOT
                               SK_DEFAULT_STATE
                               SK_AUTO_RESET
                               SK_TYPE_LIST
                               SK_PREFILTER_REGEX
                               SK_OPEN_REGEX_LIST
                               SK_CLOSED_REGEX_LIST
                               SK_FAULT_REGEX_LIST
                               SK_OPEN_OUTPUT_FMT
                               SK_CLOSED_OUTPUT_FMT
                               SK_FAULT_OUTPUT_FMT;

            ad2_printf_host("MQTT SmartSwitch #%i report\r\n", slot);
            // sub key suffix.
            std::string sk;
            for(char& c : args) {
                std::string sk = ad2_string_printf("%c", c);
                switch(c) {
                case SK_NOTIFY_SLOT[0]:
                    i = 0; // Default 0
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set notification slot [%c] to #%i.\r\n", c, i);
                    ad2_printf_host("%s %s %i %c %i\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i);
                    break;
                case SK_DEFAULT_STATE[0]:
                    i = 0; // Default CLOSED
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set default virtual switch state [%c] to '%s'(%i)\r\n", c, AD2Parse.state_str[i].c_str(), i);
                    ad2_printf_host("%s %s %i %c %i\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i);
                    break;
                case SK_AUTO_RESET[0]:
                    i = 0; // Defaut 0 or disabled
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set auto reset time in ms [%c] to '%s'\r\n", c, (i > 0) ? ad2_to_string(i).c_str() : "DISABLED");
                    ad2_printf_host("%s %s %i %c %i\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i);
                    break;
                case SK_TYPE_LIST[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host("# Set message type list [%c]\r\n", c);
                    if (out.length()) {
                        ad2_printf_host("%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    } else {
                        ad2_printf_host("# disabled\r\n");
                    }
                    break;
                case SK_PREFILTER_REGEX[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host("# Set pre filter REGEX [%c]\r\n", c);
                        ad2_printf_host("%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    }
                    break;
                case SK_OPEN_REGEX_LIST[0]:
                case SK_CLOSED_REGEX_LIST[0]:
                case SK_FAULT_REGEX_LIST[0]:
                    // * Find all sub keys and show info.
                    if (c == SK_OPEN_REGEX_LIST[0]) {
                        tmpsz = "OPEN";
                    }
                    if (c == SK_CLOSED_REGEX_LIST[0]) {
                        tmpsz = "CLOSED";
                    }
                    if (c == SK_FAULT_REGEX_LIST[0]) {
                        tmpsz = "FAULT";
                    }
                    for ( i = 1; i < MAX_SEARCH_KEYS; i++ ) {
                        out = "";
                        std::string tsk = sk + ad2_string_printf("%02i", i);
                        ad2_get_nv_slot_key_string(key.c_str(), slot, tsk.c_str(), out);
                        if (out.length()) {
                            ad2_printf_host("# Set '%s' state REGEX Filter [%c] #%02i.\r\n", tmpsz.c_str(), c, i);
                            ad2_printf_host("%s %s %i %c %i %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i, out.c_str());
                        }
                    }
                    break;
                case SK_OPEN_OUTPUT_FMT[0]:
                case SK_CLOSED_OUTPUT_FMT[0]:
                case SK_FAULT_OUTPUT_FMT[0]:
                    if (c == SK_OPEN_OUTPUT_FMT[0]) {
                        tmpsz = "OPEN";
                    }
                    if (c == SK_CLOSED_OUTPUT_FMT[0]) {
                        tmpsz = "CLOSED";
                    }
                    if (c == SK_FAULT_OUTPUT_FMT[0]) {
                        tmpsz = "FAULT";
                    }
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host("# Set output format string for '%s' state [%c].\r\n", tmpsz.c_str(), c);
                        ad2_printf_host("%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    }
                    break;
                }
            }
        }
    } else {
        ad2_printf_host("Missing or invalid <slot> 1-99\r\n");
        // TODO: DUMP when slot is 0 or 100
    }
}

/**
 * MQTT command router.
 */
static void _cli_cmd_mqtt_command_router(char *string)
{
    int i;
    std::string subcmd;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    for(i = 0;; ++i) {
        if (MQTT_SETTINGS[i] == 0) {
            ad2_printf_host("What?\r\n");
            break;
        }
        if(subcmd.compare(MQTT_SETTINGS[i]) == 0) {
            switch(i) {
            case MQTT_URL_CFGKEY_ID:   // 'url' sub command
                _cli_cmd_mqtt_event_generic(subcmd, string);
                break;
            case MQTT_SAS_CFGKEY_ID:
                _cli_cmd_mqtt_smart_alert_switch(subcmd, string);
                break;
            }
            // all done
            break;
        }
    }
}

/**
 * @brief command list for module
 */
static struct cli_command mqtt_cmd_list[] = {
    {
        (char*)MQTT_COMMAND,(char*)
        "### Configuration for MQTT message notifications.\r\n\r\n"
        "   - Will publish the virtual partition state using the following topic pattern.\r\n\r\n"
        "       - ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/state/Y\r\n\r\n"
        "       - X: The unique id using the ESP32 WiFi mac address.\r\n\r\n"
        "       - Y: The virtual partition ID 1-9\r\n\r\n"
        "  - Sets the URL to the MQTT broker.\r\n\r\n"
        "    ```" MQTT_COMMAND " " MQTT_URL_CFGKEY " {url}```\r\n\r\n"
        "    - {url}: MQTT broker URL.\r\n\r\n"
        "    Example: " MQTT_COMMAND " " MQTT_URL_CFGKEY " mqtt://mqtt.eclipseprojects.io\r\n\r\n"
        "  - Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.\r\n\r\n"
        "    ```" MQTT_COMMAND " " MQTT_SAS_CFGKEY " {slot} {setting} {arg1} [arg2]```\r\n\r\n"
        "    - {slot}\r\n"
        "      - 1-99 : Supports multiple virtual smart alert switches.\r\n\r\n"
        "    - {setting}\r\n"
        "      - [N] Notification slot\r\n"
        "        -  Notification settings slot to use for sending this events.\r\n"
        "      - [D] Default state\r\n"
        "        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)\r\n"
        "      - [R] AUTO Reset.\r\n"
        "        - {arg1}:  time in ms 0 to disable\r\n"
        "      - [T] Message type filter.\r\n"
        "        - {arg1}: Message type list seperated by ',' or empty to disables filter.\r\n"
        "          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]\r\n"
        "            - For EVENT type the message will be generated by the API and not the AD2\r\n"
        "      - [P] Pre filter REGEX or empty to disable.\r\n"
        "      - [O] Open(ON) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string  to clear\r\n"
        "      - [C] Close(OFF) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string to clear\r\n"
        "      - [F] Fault state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty  string to clear\r\n"
        "      - [o] Open output format string.\r\n"
        "      - [c] Close output format string.\r\n"
        "      - [f] Fault output format string.\r\n\r\n", _cli_cmd_mqtt_command_router
    },
};

/**
 * Register cli commands
 */
void mqtt_register_cmds()
{
    // Register MQTT CLI commands
    for (int i = 0; i < ARRAY_SIZE(mqtt_cmd_list); i++) {
        cli_register_command(&mqtt_cmd_list[i]);
    }
}

/**
 * Initialize queue and SSL
 */
void mqtt_init()
{

#if 1 //FIXME debug logging settings.
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
#endif

    // generate our client's unique user id. UUID.
    ad2_genUUID(0x10, mqttclient_UUID);
    ESP_LOGI(TAG,"MQTT init UUID: %s", mqttclient_UUID.c_str());

    // configure and start MQTT client
    esp_err_t err;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = ad2_mqtt_event_handler,
        .uri = EXAMPLE_BROKER_URI,
        .client_id = mqttclient_UUID.c_str()
    };
#pragma GCC diagnostic pop

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start return error: %s.", esp_err_to_name(err));
    }

    // Register search based virtual switches.
    std::string key = std::string(MQTT_PREFIX) + std::string(MQTT_SAS_CFGKEY);
    int subscribers = 0;
    for (int i = 1; i < 99; i++) {
        AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);

        // Save the notification slot. That is all we need to complete the task.
        es1->PTR_ARG = nullptr;

        // We at least need some output format or skip
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_OPEN_OUTPUT_FMT, es1->OPEN_OUTPUT_FORMAT);
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_CLOSED_OUTPUT_FMT, es1->CLOSED_OUTPUT_FORMAT);
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_FAULT_OUTPUT_FMT, es1->FAULT_OUTPUT_FORMAT);
        if ( es1->OPEN_OUTPUT_FORMAT.length()
                || es1->CLOSED_OUTPUT_FORMAT.length()
                || es1->FAULT_OUTPUT_FORMAT.length() ) {

            std::string notify_types_sz = "";
            std::vector<std::string> notify_types_v;
            ad2_get_nv_slot_key_string(key.c_str(), i, SK_TYPE_LIST, notify_types_sz);
            ad2_tokenize(notify_types_sz, ',', notify_types_v);
            for (auto &sztype : notify_types_v) {
                ad2_trim(sztype);
                auto x = AD2Parse.message_type_id.find(sztype);
                if(x != std::end(AD2Parse.message_type_id)) {
                    ad2_message_t mt = (ad2_message_t)AD2Parse.message_type_id.at(sztype);
                    es1->PRE_FILTER_MESAGE_TYPE.push_back(mt);
                }
            }
            ad2_get_nv_slot_key_string(key.c_str(), i, SK_PREFILTER_REGEX, es1->PRE_FILTER_REGEX);

            // Load all regex search patterns for OPEN,CLOSE and FAULT sub keys.
            std::string regex_sk_list = SK_FAULT_REGEX_LIST SK_CLOSED_REGEX_LIST SK_OPEN_REGEX_LIST;
            for(char& c : regex_sk_list) {
                std::string sk = ad2_string_printf("%c", c);
                for ( int a = 1; a < MAX_SEARCH_KEYS; a++) {
                    std::string out = "";
                    std::string tsk = sk + ad2_string_printf("%02i", a);
                    ad2_get_nv_slot_key_string(key.c_str(), i, tsk.c_str(), out);
                    if ( out.length()) {
                        if (c == SK_OPEN_REGEX_LIST[0]) {
                            es1->OPEN_REGEX_LIST.push_back(out);
                        }
                        if (c == SK_CLOSED_REGEX_LIST[0]) {
                            es1->CLOSED_REGEX_LIST.push_back(out);
                        }
                        if (c == SK_FAULT_REGEX_LIST[0]) {
                            es1->FAULT_REGEX_LIST.push_back(out);
                        }
                    }
                }
            }

            // Save the search to a list for management.
            mqtt_AD2EventSearches.push_back(es1);

            // subscribe to the callback for events.
            AD2Parse.subscribeTo(on_search_match_cb_mqtt, es1);

            // keep track of how many for user feedback.
            subscribers++;
        }
    }

    ESP_LOGI(TAG, "Found and configured %i virtual switches.", subscribers);

    // Subscribe standard AlarmDecoder events
    AD2Parse.subscribeTo(ON_ARM, mqtt_on_state_change, (void *)ON_ARM);
    AD2Parse.subscribeTo(ON_DISARM, mqtt_on_state_change, (void *)ON_DISARM);
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, mqtt_on_state_change, (void *)ON_CHIME_CHANGE);
    AD2Parse.subscribeTo(ON_FIRE, mqtt_on_state_change, (void *)ON_FIRE);
    AD2Parse.subscribeTo(ON_POWER_CHANGE, mqtt_on_state_change, (void *)ON_POWER_CHANGE);
    AD2Parse.subscribeTo(ON_READY_CHANGE, mqtt_on_state_change, (void *)ON_READY_CHANGE);
    AD2Parse.subscribeTo(ON_LOW_BATTERY, mqtt_on_state_change, (void *)ON_LOW_BATTERY);
    AD2Parse.subscribeTo(ON_ALARM_CHANGE, mqtt_on_state_change, (void *)ON_ALARM_CHANGE);
    AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, mqtt_on_state_change, (void *)ON_ZONE_BYPASSED_CHANGE);
    AD2Parse.subscribeTo(ON_EXIT_CHANGE, mqtt_on_state_change, (void *)ON_EXIT_CHANGE);
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_AD2IOT_MQTT_CLIENT */
