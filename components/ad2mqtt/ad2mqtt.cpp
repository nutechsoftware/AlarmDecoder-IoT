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
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Disable via sdkconfig
#if CONFIG_AD2IOT_MQTT_CLIENT
static const char *TAG = "MQTT";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "mqtt_client.h"

/* Constants that aren't configurable in menuconfig */
#define MQTT_COMMAND        "mqtt"
#define MQTT_PREFIX         "mq"
#define MQTT_ENABLE_CFGKEY  "enable"
#define MQTT_URL_CFGKEY     "url"
#define MQTT_CMDEN_CFGKEY   "commands"
#define MQTT_TPREFIX_CFGKEY "tprefix"
#define MQTT_DPREFIX_CFGKEY "dprefix"
#define MQTT_SAS_CFGKEY     "switch"

#define MAX_SEARCH_KEYS 9

// NV storage sub key values for virtual search switch
#define SK_NOTIFY_TOPIC      "N"
#define SK_DEFAULT_STATE     "D"
#define SK_AUTO_RESET        "R"
#define SK_TYPE_LIST         "T"
#define SK_PREFILTER_REGEX   "P"
#define SK_OPEN_REGEX_LIST   "O"
#define SK_CLOSED_REGEX_LIST "C"
#define SK_TROUBLE_REGEX_LIST  "F"
#define SK_OPEN_OUTPUT_FMT   "o"
#define SK_CLOSED_OUTPUT_FMT "c"
#define SK_TROUBLE_OUTPUT_FMT  "f"

#define MQTT_TOPIC_PREFIX "ad2iot"
#define MQTT_LWT_TOPIC_SUFFIX "will"
#define MQTT_LWT_MESSAGE "offline"
#define MQTT_COMMANDS_TOPIC "commands"
#define MQTT_COMMAND_MAX_DATA_LEN 256

#define EXAMPLE_BROKER_URI "mqtt://mqtt.eclipseprojects.io"

static esp_mqtt_client_handle_t mqtt_client = nullptr;
static std::string mqttclient_UUID;
static std::string mqttclient_TPREFIX = "";
static std::string mqttclient_DPREFIX = "";
static std::vector<AD2EventSearch *> mqtt_AD2EventSearches;
static int commands_enabled = 0;

// prefix name lines to identy the source. User can change.
#define NAME_PREFIX "AD2IoT"

// Default MQTT message settings excluding LWT.
#define MQTT_DEF_QOS    1 // AT Least Once
#define MQTT_DEF_RETAIN 1 // Retain
#define MQTT_DEF_STORE  0 // No local storage

// LOG settings
//#define MQTT_EVENT_LOGGING

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
 * @brief helper to send config json for auto discovery.
 *
 * @param [in]device_type - const char * - ex. 'binary_sensor'
 * @param [in]device_class - const char * - ex. 'smoke'
 * @param [in]type - const char * - ex. 'zone'
 * @param [in]id - uint8_t - unique ID 000 to 255 for type
 * @param [in]id_append_id - bool - append id to [unique|object]_id fields
 * @param [in]name - const char * - name
 * @param [in]name_append_id - bool - append id to name field
 * @param [in]pairs - std::map<std::string,std::string> - attributes to add to config.
 */
void mqtt_publish_device_config(const char *device_type, const char *device_class,
                                const char *ad2type, uint8_t id, bool id_append_id,
                                const char* name, bool name_append_id,
                                std::map<std::string, std::string> pairs)
{
    cJSON *root = cJSON_CreateObject();

    // if id > 0 append id else no ID
    std::string szid;
    if (id_append_id) {
        szid = ad2_to_string(id);
    }

    // Name
    std::string value = name;
    if (name_append_id) {
        value += ad2_to_string(id);
    }
    cJSON_AddStringToObject(root, "name", value.c_str());

    // unique_id ad2iot_{ad2type}[_{id}]
    // object_id ad2iot_{ad2type}[_{id}]
    // ex. ad2iot_zone_1
    value  = "ad2iot_";
    value += ad2type;
    if (id) {
        value += szid;
    }
    cJSON_AddStringToObject(root, "unique_id", value.c_str());
    cJSON_AddStringToObject(root, "object_id", value.c_str());

    // set the device class
    cJSON_AddStringToObject(root, "device_class", device_class);

    // add the name value pairs
    for (const auto& kv : pairs) {
        cJSON_AddStringToObject(root, kv.first.c_str(), kv.second.c_str());
    }

    // compress json and convert to char *
    char *szjson = cJSON_Print(root);
    cJSON_Minify(szjson);

    // set the correct topic for the config document
    std::string topic = "";
    if (mqttclient_DPREFIX.length()) {
        topic = mqttclient_DPREFIX;
    }
    topic += device_type;
    topic += "/";
    topic += mqttclient_UUID;
    topic += "/";
    topic += ad2type;
    if (id) {
        topic += szid;
    }
    topic += "/config";

    // non blocking publish
    esp_mqtt_client_enqueue(mqtt_client,
                            topic.c_str(),
                            szjson,
                            0,
                            MQTT_DEF_QOS,
                            MQTT_DEF_RETAIN,
                            MQTT_DEF_STORE);

    // cleanup free memory
    cJSON_free(szjson);
    cJSON_Delete(root);
}

/**
 * @brief helper to send config json for a given partition.
 *
 */
void mqtt_send_partition_config(AD2VirtualPartitionState *s)
{

    // Partiting number string.
    std::string szpid = ad2_to_string(s->partition);

    // Base topic for device
    std::string topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;

    // alarm_control_panel
    mqtt_publish_device_config("alarm_control_panel", "alarm_control_panel", "p",
                               s->partition, true,
    NAME_PREFIX " Partition #", true, {{
            { "state_topic", topic+"/partitions/"+szpid },
            { "value_template", "{% if value_json.alarm_sounding == true or value_json.alarm_event_occurred == true %}triggered{% elif value_json.armed_stay == true %}{% if value_json.entry_delay_off == true %}armed_night{% else %}armed_home{% endif %}{% elif value_json.armed_away == true %}{% if value_json.entry_delay_off == true %}armed_vacation{% elif value_json.entry_delay_off == false %}armed_away{% endif %}{% else %}disarmed{% endif %}" },
            { "command_topic", topic+"/commands"},
            { "command_template", "{ \"vpart\": 0, \"action\": \"{{ action }}\", \"code\": \"{{ code }}\"}"},
            { "availability_topic", topic+"/status"},
            { "code", "REMOTE_CODE"},
            { "payload_arm_home", "ARM_STAY"},
            { "payload_trigger", "PANIC_ALARM"},
            { "icon", "mdi:shield-home"},
            { "sw_version", FIRMWARE_VERSION}
        }
    });


    // ac_power
    mqtt_publish_device_config("binary_sensor", "power", "ac_power",
                               0, false,
    NAME_PREFIX " AC Power", false, {{
            { "state_topic", topic+"/partitions/"+szpid },
            { "value_template", "{% if value_json.ac_power == true %}ON{% else %}OFF{% endif %}" },
            { "availability_topic", topic+"/status"}
        }
    });

    // partition fire
    mqtt_publish_device_config("binary_sensor", "smoke", "fire_p",
                               s->partition, true,
    NAME_PREFIX " Fire Partition #", true, {{
            { "state_topic", topic+"/partitions/"+szpid },
            { "value_template", "{% if value_json.fire_alarm == true %}ON{% else %}OFF{% endif %}" },
            { "availability_topic", topic+"/status"}
        }
    });

    // partition chime
    mqtt_publish_device_config("binary_sensor", "running", "chime_p",
                               s->partition, true,
    NAME_PREFIX " Chime Mode Partition #", true, {{
            { "state_topic", topic+"/partitions/"+szpid },
            { "value_template", "{% if value_json.chime_on == true %}ON{% else %}OFF{% endif %}" },
            { "availability_topic", topic+"/status"}
        }
    });

}

/**
 * @brief helper to send config json for every partition zones.
 *
 */
void mqtt_send_fw_version(const char *available_version)
{
    int msg_id;
    if (mqtt_client != nullptr) {
        std::string sTopic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/fw_version";

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "installed", FIRMWARE_VERSION);
        cJSON_AddStringToObject(root, "available", available_version);

        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client,
                                         sTopic.c_str(),
                                         state,
                                         0,
                                         MQTT_DEF_QOS,
                                         MQTT_DEF_RETAIN,
                                         MQTT_DEF_STORE);

        cJSON_free(state);
        cJSON_Delete(root);
    }
}

/**
 * @brief helper to send config json for every partition zones.
 *
 * TODO: This and other mqtt_client_enqueue calls will stack up
 * a lot of memory for larger systems. This should instead be
 * put into an efficient task list to be processed over time
 * as resources are available. In this design it saves all of the
 * JSON data and more for each enqueue.
 */
void mqtt_send_partition_zone_configs(AD2VirtualPartitionState *s)
{
    // Send panel_power_0 config
    std::string topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;

    for (uint8_t zn : s->zone_list) {

        std::string _type;
        AD2Parse.getZoneType(zn, _type);

        std::string _alpha;
        AD2Parse.getZoneString(zn, _alpha);

        mqtt_publish_device_config("binary_sensor", _type.c_str(), "zone_",
                                   zn, true,
        _alpha.c_str(), false, {{
                { "state_topic", topic+"/zones/"+ad2_to_string(zn) },
                { "value_template", "{% if value_json.state == 'CLOSE' %}OFF{% else %}ON{% endif %}" },
                { "availability_topic", topic+"/status"}
            }
        });
    }
}

/**
 * @brief Callback for MQTT_EVENT_CONNECTED event.
 * Preform subscribe to commands and initial publish to status and info.
 *
 * @param [in]client esp_mqtt_client_handle_t
 */
void mqtt_on_connect(esp_mqtt_client_handle_t client)
{
    std::string topic;

    // Subscribe to command inputs for remote control if enabled.
    if (commands_enabled) {
        ESP_LOGI(TAG, "Warning! MQTT commands subscription enabled. Not sure on public servers.");
        topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        topic += mqttclient_UUID;
        topic += "/" MQTT_COMMANDS_TOPIC;
        esp_mqtt_client_subscribe(client,
                                  topic.c_str(),
                                  MQTT_DEF_QOS);
    }

    // Publish we are Online
    topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;
    topic += "/status";

    // non blocking.
    esp_mqtt_client_enqueue(mqtt_client,
                            topic.c_str(),
                            "online",
                            0,
                            MQTT_DEF_QOS,
                            MQTT_DEF_RETAIN,
                            MQTT_DEF_STORE);

    // Publish our device HW/FW info.
    cJSON *root = ad2_get_ad2iot_device_info_json();
    topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;
    topic += "/info";
    char *state = cJSON_Print(root);
    cJSON_Minify(state);

    // non blocking.
    esp_mqtt_client_enqueue(client,
                            topic.c_str(),
                            state,
                            0,
                            MQTT_DEF_QOS,
                            MQTT_DEF_RETAIN,
                            MQTT_DEF_STORE);
    cJSON_free(state);
    cJSON_Delete(root);

    // set available version to current for now. Will be updated if new version available.
    mqtt_send_fw_version(FIRMWARE_VERSION);

    // Send firmware_update config
    topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;
    mqtt_publish_device_config("binary_sensor", "update", "fw_version",
                               0, false,
    NAME_PREFIX " Firmware", false, {{
            { "state_topic", topic+"/fw_version" },
            { "value_template", "{% if value_json.installed != value_json.available %}ON{% else %}OFF{% endif %}" },
            { "availability_topic", topic+"/status"}
        }
    });


    mqtt_publish_device_config("button", "update", "fw_update",
                               0, false,
    NAME_PREFIX " Start firmware update", false, {{
            { "availability_topic", topic+"/fw_version" },
            { "availability_template", "{% if value_json.installed != value_json.available %}online{% else %}offline{% endif %}" },
            { "command_topic", topic+"/commands" },
            { "payload_press", "{\"action\": \"FW_UPDATE\"}" }
        }
    });

    // Publish panel config info for home assistant or others for discovery
    // for each partition that is configured with 'vpart' command.
    for (int n = 0; n <= AD2_MAX_VPARTITION; n++) {
        AD2VirtualPartitionState *s = ad2_get_partition_state(n);
        if (s) {
            mqtt_send_partition_config(s);
            mqtt_send_partition_zone_configs(s);
        }
    }

    // Send virtual switches in mqtt_AD2EventSearches
    topic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    topic += mqttclient_UUID;
    for (auto &sw : mqtt_AD2EventSearches) {
        // Grab the topic using the virtusal switch ID pre saved into INT_ARG
        std::string name = "NA";
        std::string key = std::string(MQTT_PREFIX) + std::string(MQTT_SAS_CFGKEY);
        ad2_get_nv_slot_key_string(key.c_str(), sw->INT_ARG, SK_NOTIFY_TOPIC, name);
        ad2_trim(name);
        std::string _type = "door";

        mqtt_publish_device_config(
            "binary_sensor", _type.c_str(), "switch_",
            sw->INT_ARG, true,
        name.c_str(), false, {{
                { "state_topic", topic+"/switches/"+ad2_to_string(sw->INT_ARG) },
                { "value_template", "{{value_json.state}}" },
                { "availability_topic", topic+"/status" }
            }
        });
    }

}

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

    esp_mqtt_client_handle_t client = event_data->client;
    // your_context_t *context = event->context;
    switch (event_data->event_id) {
    case MQTT_EVENT_CONNECTED:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
#endif
        mqtt_on_connect(client);
        break;
    case MQTT_EVENT_DISCONNECTED:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
#endif
        break;
    case MQTT_EVENT_SUBSCRIBED:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event_data->msg_id);
#endif
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event_data->msg_id);
#endif
        break;
    case MQTT_EVENT_PUBLISHED:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event_data->msg_id);
#endif
        break;
    case MQTT_EVENT_DATA:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event_data->topic_len, event_data->topic);
        printf("DATA=%.*s\r\n", event_data->data_len, event_data->data);
#endif
        // test if commands subscription is enabled
        if ( commands_enabled ) {
            // Sanity test topic is the size of ```commands``` topic name.
            // Topic pattern to confirm command
            std::string topic_path;
            topic_path = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
            topic_path += mqttclient_UUID;
            topic_path += "/" MQTT_COMMANDS_TOPIC;

            if ( event_data->topic_len == topic_path.length() ) {
                std::string topic( event_data->topic, event_data->topic_len );
                if ( topic.compare(topic_path) == 0 ) {

                    // We only want fresh messages no recordings.
                    // Check for retain flag skip if true.
                    if ( event_data->retain ) {
                        break;
                    }

                    // sanity check the event_data->data_len
                    if ( event_data->data_len < MQTT_COMMAND_MAX_DATA_LEN ) {
                        std::string command( event_data->data, event_data->data_len );
                        // grab the json buffer
                        // {
                        //   vpart: {{ number virtual partition ID see ```vpart``` command. }},
                        //   code: '{{ string code }}',
                        //   action: '{{ string action }}',
                        //   arg: '{{ string argument }}'
                        // }
                        cJSON * root   = cJSON_Parse( command.c_str() );
                        if (root) {
                            cJSON *ovpart = NULL;
                            cJSON *ocode = NULL;
                            cJSON *oaction = NULL;
                            cJSON *oarg = NULL;
                            ovpart = cJSON_GetObjectItemCaseSensitive(root, "vpart");
                            ocode = cJSON_GetObjectItemCaseSensitive(root, "code");
                            oaction = cJSON_GetObjectItemCaseSensitive(root, "action");
                            oarg = cJSON_GetObjectItemCaseSensitive(root, "arg");

                            int vpart = 0; // default partition
                            std::string code = "";
                            std::string action = "";
                            std::string arg = "";

                            if ( cJSON_IsNumber(ovpart) ) {
                                vpart = ovpart->valuedouble;
                            }
                            if ( cJSON_IsString(ocode) ) {
                                code = ocode->valuestring;
                            }
                            if ( cJSON_IsString(oaction) && (oaction->valuestring != NULL) ) {
                                action = oaction->valuestring;
                            }
                            if ( cJSON_IsString(oarg) && (oarg->valuestring != NULL) ) {
                                arg = oarg->valuestring;
                            }

                            ESP_LOGI(TAG, "vpart: %i, code: '%s', action: %s, arg: %s", vpart, code.c_str(), action.c_str(), arg.c_str());

                            if ( action.compare("DISARM") == 0 ) {
                                ad2_disarm(code, vpart);
                            } else if ( action.compare("ARM_STAY") == 0 ) {
                                ad2_arm_stay(code, vpart);
                            } else if ( action.compare("ARM_AWAY") == 0 ) {
                                ad2_arm_away(code, vpart);
                            } else if ( action.compare("EXIT") == 0 ) {
                                ad2_exit_now(vpart);
                            } else if ( action.compare("CHIME_TOGGLE") == 0 ) {
                                ad2_chime_toggle(code, vpart);
                            } else if ( action.compare("AUX_ALARM") == 0 ) {
                                ad2_aux_alarm(vpart);
                            } else if ( action.compare("PANIC_ALARM") == 0 ) {
                                ad2_panic_alarm(vpart);
                            } else if ( action.compare("FIRE_ALARM") == 0 ) {
                                ad2_fire_alarm(vpart);
                            } else if ( action.compare("BYPASS") == 0 ) {
                                ad2_bypass_zone(code, vpart, std::atoi(arg.c_str()));
                            } else if ( action.compare("SEND_RAW") == 0 ) {
                                ad2_send(arg);
                            } else if ( action.compare("FW_UPDATE") == 0 ) {
                                hal_ota_do_update("");
                            } else {
                                // unknown command
                            }
                            cJSON_Delete(root);
                        } else {
                            // JSON parse error
                            ESP_LOGI(TAG, "json parse error '%s'", command.c_str());
                        }
                    } else {
                        ESP_LOGI(TAG, "invalid data len");
                    }
                } else {
                    ESP_LOGI(TAG, "invalid topic path");
                }
            } else {
                ESP_LOGI(TAG, "invalid topic len");
            }
        }
        break;
    case MQTT_EVENT_ERROR:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
#endif
        break;
    default:
#if defined(MQTT_EVENT_LOGGING)
        ESP_LOGI(TAG, "Other event id:%d", event_data->event_id);
#endif
        break;
    }

    return ESP_OK;
}

/**
 * @brief ON_LRR callback for all AlarmDecoder API event subscriptions.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2VirtualPartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void mqtt_on_lrr(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    int msg_id;
    if (mqtt_client != nullptr) {
        std::string sTopic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/cid";

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "event_message", msg->c_str());

        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client,
                                         sTopic.c_str(),
                                         state,
                                         0,
                                         MQTT_DEF_QOS,
                                         MQTT_DEF_RETAIN,
                                         MQTT_DEF_STORE);

        cJSON_free(state);
        cJSON_Delete(root);
    }
}

/**
 * @brief ON_FIRMWARE_VERSION
 * Called when a new firmware version is available.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_new_firmware_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    mqtt_send_fw_version(msg->c_str());
}

/**
 * @brief ON_ZONE_CHANGE callback for all AlarmDecoder API event subscriptions.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2VirtualPartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void mqtt_on_zone_change(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    int msg_id;
    if (mqtt_client != nullptr && s) {
        std::string sTopic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/zones/";

        // Append the zone to the topic string
        sTopic+=ad2_to_string((int)s->zone);

        cJSON *root = cJSON_CreateObject();
        std::string buf;
        // grab the verb(FOO) 'ZONE FOO 001'
        ad2_copy_nth_arg(buf, (char *)s->last_event_message.c_str(), 1);
        cJSON_AddStringToObject(root, "state", buf.c_str());
        cJSON_AddNumberToObject(root, "partition", s->partition);
        cJSON_AddNumberToObject(root, "mask", s->address_mask_filter);
        cJSON_AddBoolToObject(root, "system", s->zone_states[s->zone].is_system());
        std::string zalpha;
        AD2Parse.getZoneString((int)s->zone, zalpha);
        cJSON_AddStringToObject(root, "name", zalpha.c_str());
        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client,
                                         sTopic.c_str(),
                                         state,
                                         0,
                                         MQTT_DEF_QOS,
                                         MQTT_DEF_RETAIN,
                                         MQTT_DEF_STORE);
        cJSON_free(state);
        cJSON_Delete(root);
    }
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
        std::string sTopic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/partitions/";
        sTopic+=ad2_to_string(s->partition);
        cJSON *root = ad2_get_partition_state_json(s);
        cJSON_AddStringToObject(root, "event", AD2Parse.event_str[(int)arg].c_str());
        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client,
                                         sTopic.c_str(),
                                         state,
                                         0,
                                         MQTT_DEF_QOS,
                                         MQTT_DEF_RETAIN,
                                         MQTT_DEF_STORE);
        if (msg_id == -1) {
            ESP_LOGE(TAG, "esp_mqtt_client_enqueue failed.");
        }
        cJSON_free(state);
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

    std::string message = es->out_message;

    // Grab the topic using the virtual switch ID pre saved into INT_ARG
    // publishing event
    int msg_id;
    if (mqtt_client != nullptr) {
        std::string sTopic = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
        sTopic+=mqttclient_UUID;
        sTopic+="/switches/";
        sTopic+=ad2_to_string(es->INT_ARG);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "state", es->out_message.c_str());
        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Non blocking. We must not block AlarmDecoderParser
        msg_id = esp_mqtt_client_enqueue(mqtt_client,
                                         sTopic.c_str(),
                                         state,
                                         0,
                                         MQTT_DEF_QOS,
                                         MQTT_DEF_RETAIN,
                                         MQTT_DEF_STORE);
        cJSON_free(state);
        cJSON_Delete(root);
    }

}


/**
 * Command support values.
 */
enum {
    MQTT_ENABLE_CFGKEY_ID = 0,
    MQTT_URL_CFGKEY_ID,
    MQTT_CMDEN_CFGKEY_ID,
    MQTT_TPREFIX_CFGKEY_ID,
    MQTT_DPREFIX_CFGKEY_ID,
    MQTT_SAS_CFGKEY_ID
};
char * MQTT_SUBCMD [] = {
    (char*)MQTT_ENABLE_CFGKEY,
    (char*)MQTT_URL_CFGKEY,
    (char*)MQTT_CMDEN_CFGKEY,
    (char*)MQTT_TPREFIX_CFGKEY,
    (char*)MQTT_DPREFIX_CFGKEY,
    (char*)MQTT_SAS_CFGKEY,
    0 // EOF
};

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
            case '-': // Remove entry
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_NOTIFY_TOPIC, NULL);
                ad2_set_nv_slot_key_int(key.c_str(), slot, SK_DEFAULT_STATE, -1);
                ad2_set_nv_slot_key_int(key.c_str(), slot, SK_AUTO_RESET, -1);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_TYPE_LIST, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_PREFILTER_REGEX, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_OPEN_REGEX_LIST, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_CLOSED_REGEX_LIST, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_TROUBLE_REGEX_LIST, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_OPEN_OUTPUT_FMT, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_CLOSED_OUTPUT_FMT, NULL);
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_TROUBLE_OUTPUT_FMT, NULL);
                ad2_printf_host(false, "Deleteing smartswitch #%i.\r\n", slot);
                break;
            case SK_NOTIFY_TOPIC[0]: // MQTT Topic
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host(false, "Setting smartswitch #%i MQTT topic to '%s'.\r\n", slot, arg1.c_str());
                break;
            case SK_DEFAULT_STATE[0]: // Default state
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(key.c_str(), slot, sk.c_str(), i);
                ad2_printf_host(false, "Setting smartswitch #%i to use default state '%s' %i.\r\n", slot, AD2Parse.state_str[i].c_str(), i);
                break;
            case SK_AUTO_RESET[0]: // Auto Reset
                ad2_copy_nth_arg(arg1, instring, 4);
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(key.c_str(), slot, sk.c_str(), i);
                ad2_printf_host(false, "Setting smartswitch #%i auto reset value %i.\r\n", slot, i);
                break;
            case SK_TYPE_LIST[0]: // Message type filter list
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host(false, "Setting smartswitch #%i message type filter list to '%s'.\r\n", slot, arg1.c_str());
                break;
            case SK_PREFILTER_REGEX[0]: // Pre filter REGEX
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host(false, "Setting smartswitch #%i pre filter regex to '%s'.\r\n", slot, arg1.c_str());
                break;

            case SK_OPEN_REGEX_LIST[0]: // Open state REGEX list editor
            case SK_CLOSED_REGEX_LIST[0]: // Closed state REGEX list editor
            case SK_TROUBLE_REGEX_LIST[0]: // Trouble state REGEX list editor
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
                    if (buf[0] == SK_TROUBLE_REGEX_LIST[0]) {
                        tmpsz = "TROUBLE";
                    }
                    ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg2.c_str());
                    ad2_printf_host(false, "%s smartswitch #%i REGEX filter #%02i for state '%s' to '%s'.\r\n", op.c_str(), slot, i, tmpsz.c_str(), arg2.c_str());
                } else {
                    ad2_printf_host(false, "Error invalid index %i. Valid values are 1-8.\r\n", slot, i);
                }
                break;

            case SK_OPEN_OUTPUT_FMT[0]: // Open state output format string
            case SK_CLOSED_OUTPUT_FMT[0]: // Closed state output format string
            case SK_TROUBLE_OUTPUT_FMT[0]: // Trouble state output format string
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg1, instring, 4, true);
                if (buf[0] == SK_OPEN_OUTPUT_FMT[0]) {
                    tmpsz = "OPEN";
                }
                if (buf[0] == SK_CLOSED_OUTPUT_FMT[0]) {
                    tmpsz = "CLOSED";
                }
                if (buf[0] == SK_TROUBLE_OUTPUT_FMT[0]) {
                    tmpsz = "TROUBLE";
                }
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg1.c_str());
                ad2_printf_host(false, "Setting smartswitch #%i output format string for '%s' state to '%s'.\r\n", slot, tmpsz.c_str(), arg1.c_str());
                break;

            default:
                ESP_LOGW(TAG, "Unknown setting '%c' ignored.", buf[0]);
                return;
            }
        } else {
            // query show contents.
            int i;
            std::string out;
            std::string args = SK_NOTIFY_TOPIC
                               SK_DEFAULT_STATE
                               SK_AUTO_RESET
                               SK_TYPE_LIST
                               SK_PREFILTER_REGEX
                               SK_OPEN_REGEX_LIST
                               SK_CLOSED_REGEX_LIST
                               SK_TROUBLE_REGEX_LIST
                               SK_OPEN_OUTPUT_FMT
                               SK_CLOSED_OUTPUT_FMT
                               SK_TROUBLE_OUTPUT_FMT;

            ad2_printf_host(false, "MQTT SmartSwitch #%i report\r\n", slot);
            // sub key suffix.
            std::string sk;
            for(char& c : args) {
                std::string sk = ad2_string_printf("%c", c);
                switch(c) {
                case SK_NOTIFY_TOPIC[0]:
                    out = ""; // Default 0
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host(false, "# Set MQTT topic [%c] to '%s'.\r\n", c, out.c_str());
                    ad2_printf_host(false, "%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    break;
                case SK_DEFAULT_STATE[0]:
                    i = 0; // Default CLOSED
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host(false, "# Set default virtual switch state [%c] to '%s'(%i)\r\n", c, AD2Parse.state_str[i].c_str(), i);
                    ad2_printf_host(false, "%s %s %i %c %i\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i);
                    break;
                case SK_AUTO_RESET[0]:
                    i = 0; // Defaut 0 or disabled
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host(false, "# Set auto reset time in ms [%c] to '%s'\r\n", c, (i > 0) ? ad2_to_string(i).c_str() : "DISABLED");
                    ad2_printf_host(false, "%s %s %i %c %i\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i);
                    break;
                case SK_TYPE_LIST[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host(false, "# Set message type list [%c]\r\n", c);
                    if (out.length()) {
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    } else {
                        ad2_printf_host(false, "# disabled\r\n");
                    }
                    break;
                case SK_PREFILTER_REGEX[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host(false, "# Set pre filter REGEX [%c]\r\n", c);
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    }
                    break;
                case SK_OPEN_REGEX_LIST[0]:
                case SK_CLOSED_REGEX_LIST[0]:
                case SK_TROUBLE_REGEX_LIST[0]:
                    // * Find all sub keys and show info.
                    if (c == SK_OPEN_REGEX_LIST[0]) {
                        tmpsz = "OPEN";
                    }
                    if (c == SK_CLOSED_REGEX_LIST[0]) {
                        tmpsz = "CLOSED";
                    }
                    if (c == SK_TROUBLE_REGEX_LIST[0]) {
                        tmpsz = "TROUBLE";
                    }
                    for ( i = 1; i < MAX_SEARCH_KEYS; i++ ) {
                        out = "";
                        std::string tsk = sk + ad2_string_printf("%02i", i);
                        ad2_get_nv_slot_key_string(key.c_str(), slot, tsk.c_str(), out);
                        if (out.length()) {
                            ad2_printf_host(false, "# Set '%s' state REGEX Filter [%c] #%02i.\r\n", tmpsz.c_str(), c, i);
                            ad2_printf_host(false, "%s %s %i %c %i %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, i, out.c_str());
                        }
                    }
                    break;
                case SK_OPEN_OUTPUT_FMT[0]:
                case SK_CLOSED_OUTPUT_FMT[0]:
                case SK_TROUBLE_OUTPUT_FMT[0]:
                    if (c == SK_OPEN_OUTPUT_FMT[0]) {
                        tmpsz = "OPEN";
                    }
                    if (c == SK_CLOSED_OUTPUT_FMT[0]) {
                        tmpsz = "CLOSED";
                    }
                    if (c == SK_TROUBLE_OUTPUT_FMT[0]) {
                        tmpsz = "TROUBLE";
                    }
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host(false, "# Set output format string for '%s' state [%c].\r\n", tmpsz.c_str(), c);
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", MQTT_COMMAND, MQTT_SAS_CFGKEY, slot, c, out.c_str());
                    }
                    break;
                }
            }
        }
    } else {
        ad2_printf_host(false, "Missing or invalid <slot> 1-99\r\n");
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
    std::string arg;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    for(i = 0;; ++i) {
        if (MQTT_SUBCMD[i] == 0) {
            ad2_printf_host(false, "What?\r\n");
            break;
        }
        if(subcmd.compare(MQTT_SUBCMD[i]) == 0) {
            switch(i) {
            /**
             * MQTT Enable / Disable
             */
            case MQTT_ENABLE_CFGKEY_ID:   // 'enable' sub command
                arg = "";
                if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
                    ad2_set_nv_slot_key_int(MQTT_COMMAND, MQTT_ENABLE_CFGKEY_ID, nullptr, (arg[0] == 'Y' || arg[0] ==  'y'));
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                }

                // show contents of this setting
                i=0;
                ad2_get_nv_slot_key_int(MQTT_COMMAND, MQTT_ENABLE_CFGKEY_ID, nullptr, &i);
                ad2_printf_host(false, "MQTT client is '%s'.\r\n", (i ? "Enabled" : "Disabled"));
                break;

            /**
             * MQTT Broker URL
             */
            case MQTT_URL_CFGKEY_ID:   // 'url' sub command
                // If arg provided then save.
                if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
                    ad2_set_nv_slot_key_string(MQTT_COMMAND, MQTT_URL_CFGKEY_ID, nullptr, arg.c_str());
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                } else {
                    // show contents of this setting
                    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_URL_CFGKEY_ID, nullptr, arg);
                    ad2_printf_host(false, "MQTT Broker 'url' set to '%s'.\r\n", arg.c_str());
                }
                break;

            /**
             * MQTT commands enable / disable
             */
            case MQTT_CMDEN_CFGKEY_ID:   // 'commands' sub command
                arg = "";
                if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
                    ad2_set_nv_slot_key_int(MQTT_COMMAND, MQTT_CMDEN_CFGKEY_ID, nullptr, (arg[0] == 'Y' || arg[0] ==  'y'));
                    if ((arg[0] == 'Y' || arg[0] ==  'y')) {
                        ad2_printf_host(false, "Warning! Enabling commands on a public sever will allow anyone to send commands to the panel. Be sure this is only enabled on private servers or servers with publish permissions.\r\n");
                    }
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                }

                // show contents of this setting
                i = 0;
                ad2_get_nv_slot_key_int(MQTT_COMMAND, MQTT_CMDEN_CFGKEY_ID, nullptr, &i);
                ad2_printf_host(false, "MQTT command subscription is '%s'.\r\n", (i ? "Enabled" : "Disabled"));
                break;

            /**
             * MQTT topic prefix
             */
            case MQTT_TPREFIX_CFGKEY_ID:   // 'prefix' sub command
                // If arg provided then save.
                if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
                    ad2_remove_ws(arg);
                    ad2_set_nv_slot_key_string(MQTT_COMMAND, MQTT_TPREFIX_CFGKEY_ID, nullptr, arg.c_str());
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                } else {
                    // show contents of this setting
                    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_TPREFIX_CFGKEY_ID, nullptr, arg);
                    ad2_printf_host(false, "MQTT topic prefix set to '%s'.\r\n", arg.c_str());
                }
                break;

            /**
             * MQTT auto discovery topic prefix
             */
            case MQTT_DPREFIX_CFGKEY_ID:   // 'dprefix' sub command
                // If arg provided then save.
                if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
                    ad2_remove_ws(arg);
                    ad2_set_nv_slot_key_string(MQTT_COMMAND, MQTT_DPREFIX_CFGKEY_ID, nullptr, arg.c_str());
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                } else {
                    // show contents of this setting
                    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_DPREFIX_CFGKEY_ID, nullptr, arg);
                    ad2_printf_host(false, "MQTT discovery topic prefix set to '%s'.\r\n", arg.c_str());
                }
                break;

            /**
             * MQTT Virtual switch command
             */
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
        "#### Configuration for MQTT message notifications\r\n"
        "- Publishes the virtual partition state using the following topic pattern.\r\n"
        "  - ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/partitions/Y\r\n"
        "  - X: The unique id using the ESP32 WiFi mac address.\r\n"
        "  - Y: The virtual partition ID 1-9 or a Virtual switch sub topic.\r\n"
        "- [" MQTT_ENABLE_CFGKEY "] Enable / Disable MQTT client\r\n"
        "  -  {arg1}: [Y]es [N]o\r\n"
        "    - [N] Default state\r\n"
        "  - Example: ```" MQTT_COMMAND " " MQTT_ENABLE_CFGKEY " Y```\r\n"
        "- Sets the URL to the MQTT broker.\r\n"
        "  - ```" MQTT_COMMAND " " MQTT_URL_CFGKEY " {url}```\r\n"
        "    - {url}: MQTT broker URL.\r\n"
        "  - Example: ```" MQTT_COMMAND " " MQTT_URL_CFGKEY " mqtt://user@pass:mqtt.example.com```\r\n"
        "- Topic prefix. Prefix to be used on publish topics.\r\n"
        "  - ```" MQTT_COMMAND " " MQTT_TPREFIX_CFGKEY " {prefix}```\r\n"
        "  -  {prefix}: Topic prefix.\r\n"
        "  - Example: ```" MQTT_COMMAND " " MQTT_TPREFIX_CFGKEY " somepath```\r\n"
        "- Enable/Disable command subscription. Do not enable on public MQTT servers!\r\n"
        "  - ```" MQTT_COMMAND " " MQTT_CMDEN_CFGKEY " [Y/N]```\r\n"
        "  -  {arg1}: [Y]es [N]o\r\n"
        "  - Example: ```" MQTT_COMMAND " " MQTT_CMDEN_CFGKEY " Y```\r\n"
        "- MQTT auto discovery prefix for topic to publish config documents.\r\n"
        "  - ```" MQTT_COMMAND " " MQTT_DPREFIX_CFGKEY " {prefix}```\r\n"
        "  -  {prefix}: MQTT auto discovery topic root.\r\n"
        "  - Example: ```" MQTT_COMMAND " " MQTT_DPREFIX_CFGKEY " homeassistant```\r\n"
        "- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.\r\n"
        "  - ```" MQTT_COMMAND " " MQTT_SAS_CFGKEY " {slot} {setting} {arg1} [arg2]```\r\n"
        "    - {slot}\r\n"
        "      - 1-99 : Supports multiple virtual smart alert switches.\r\n"
        "        - full topic will be ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/switches/{slot}\r\n"
        "    - {setting}\r\n"
        "      - [-] Delete switch\r\n"
        "      - [N] Notification device name\r\n"
        "        -  Example: ```TEST``` - {\"name\": \"TEST\"}\r\n"
        "      - [D] Default state\r\n"
        "        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)\r\n"
        "      - [R] AUTO Reset.\r\n"
        "        - {arg1}:  time in ms 0 to disable\r\n"
        "      - [T] Message type filter.\r\n"
        "        - {arg1}: Message type list separated by ',' or empty to disables filter.\r\n"
        "          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]\r\n"
        "            - For EVENT type the message will be generated by the API and not the AD2\r\n"
        "      - [P] Pre filter REGEX or empty to disable.\r\n"
        "      - [O] Open(ON) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string  to clear\r\n"
        "      - [C] Close(OFF) state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty string to clear\r\n"
        "      - [F] Trouble state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty  string to clear\r\n"
        "      - [o] Open output format string.\r\n"
        "      - [c] Close output format string.\r\n"
        "      - [f] Trouble output format string.\r\n\r\n", _cli_cmd_mqtt_command_router
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

#if 0 // debug logging settings.
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
#endif

    int enabled = 0;
    ad2_get_nv_slot_key_int(MQTT_COMMAND, MQTT_ENABLE_CFGKEY_ID, nullptr, &enabled);

    // nothing more needs to be done once commands are set if not enabled.
    if (!enabled) {
        ad2_printf_host(true, "%s client disabled", TAG);
        return;
    }

    // load commands subscription enable/disable setting
    ad2_get_nv_slot_key_int(MQTT_COMMAND, MQTT_CMDEN_CFGKEY_ID, nullptr, &commands_enabled);

    // generate our client's unique user id. UUID.
    ad2_genUUID(0x10, mqttclient_UUID);
    ad2_printf_host(true, "%s init UUID: %s", TAG, mqttclient_UUID.c_str());

    // configure and start MQTT client
    esp_err_t err;

    // load topic prefix setting
    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_TPREFIX_CFGKEY_ID, nullptr, mqttclient_TPREFIX);
    if (mqttclient_TPREFIX.length()) {
        // add a slash
        mqttclient_TPREFIX += "/";
    }

    // load discovery topic prefix setting
    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_DPREFIX_CFGKEY_ID, nullptr, mqttclient_DPREFIX);
    if (mqttclient_DPREFIX.length()) {
        // add a slash
        mqttclient_DPREFIX += "/";
    }

    // load and parse the Broker URL if set.
    std::string brokerURL;
    ad2_get_nv_slot_key_string(MQTT_COMMAND, MQTT_URL_CFGKEY_ID, nullptr, brokerURL);
    if (!brokerURL.length()) {
        // set default
        brokerURL = EXAMPLE_BROKER_URI;
    }

    // Last Will topic
    std::string LWT_TOPIC = mqttclient_TPREFIX + MQTT_TOPIC_PREFIX "/";
    LWT_TOPIC+=mqttclient_UUID;
    LWT_TOPIC+="/status";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = ad2_mqtt_event_handler,
        .uri = brokerURL.c_str(),
        .client_id = mqttclient_UUID.c_str(),
        .lwt_topic = LWT_TOPIC.c_str(),
        .lwt_msg =  "offline",
        .lwt_qos = 1,    // For LWT use QOS1
        .lwt_retain = 1  // FOr LWT use Retain
    };
#pragma GCC diagnostic pop

    // Create and start the client.
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start return error: %s.", esp_err_to_name(err));
    }

    // Subscribe standard AlarmDecoder events
    AD2Parse.subscribeTo(ON_ARM, mqtt_on_state_change, (void *)ON_ARM);
    AD2Parse.subscribeTo(ON_DISARM, mqtt_on_state_change, (void *)ON_DISARM);
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, mqtt_on_state_change, (void *)ON_CHIME_CHANGE);
    AD2Parse.subscribeTo(ON_BEEPS_CHANGE, mqtt_on_state_change, (void *)ON_BEEPS_CHANGE);
    AD2Parse.subscribeTo(ON_FIRE_CHANGE, mqtt_on_state_change, (void *)ON_FIRE_CHANGE);
    AD2Parse.subscribeTo(ON_POWER_CHANGE, mqtt_on_state_change, (void *)ON_POWER_CHANGE);
    AD2Parse.subscribeTo(ON_READY_CHANGE, mqtt_on_state_change, (void *)ON_READY_CHANGE);
    AD2Parse.subscribeTo(ON_LOW_BATTERY, mqtt_on_state_change, (void *)ON_LOW_BATTERY);
    AD2Parse.subscribeTo(ON_ALARM_CHANGE, mqtt_on_state_change, (void *)ON_ALARM_CHANGE);
    AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, mqtt_on_state_change, (void *)ON_ZONE_BYPASSED_CHANGE);
    AD2Parse.subscribeTo(ON_EXIT_CHANGE, mqtt_on_state_change, (void *)ON_EXIT_CHANGE);
    AD2Parse.subscribeTo(ON_LRR, mqtt_on_lrr, (void *)ON_LRR);
    // SUbscribe to ON_ZONE_CHANGE events
    AD2Parse.subscribeTo(ON_ZONE_CHANGE, mqtt_on_zone_change, (void *)ON_ZONE_CHANGE);

    // subscribe to firmware updates available events.
    AD2Parse.subscribeTo(ON_FIRMWARE_VERSION, on_new_firmware_cb, nullptr);

    // Register search based virtual switches.
    std::string key = std::string(MQTT_PREFIX) + std::string(MQTT_SAS_CFGKEY);
    int subscribers = 0;
    for (int i = 1; i < 99; i++) {
        AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);

        // Not used.
        es1->PTR_ARG = nullptr;

        // save the NVS Virtual SWITCH ID so we can read the data base later.
        es1->INT_ARG = i;

        // We at least need some output format or skip
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_OPEN_OUTPUT_FMT, es1->OPEN_OUTPUT_FORMAT);
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_CLOSED_OUTPUT_FMT, es1->CLOSED_OUTPUT_FORMAT);
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_TROUBLE_OUTPUT_FMT, es1->TROUBLE_OUTPUT_FORMAT);
        if ( es1->OPEN_OUTPUT_FORMAT.length()
                || es1->CLOSED_OUTPUT_FORMAT.length()
                || es1->TROUBLE_OUTPUT_FORMAT.length() ) {

            std::string notify_types_sz = "";
            std::vector<std::string> notify_types_v;
            ad2_get_nv_slot_key_string(key.c_str(), i, SK_TYPE_LIST, notify_types_sz);
            ad2_tokenize(notify_types_sz, ", ", notify_types_v);
            for (auto &sztype : notify_types_v) {
                ad2_trim(sztype);
                auto x = AD2Parse.message_type_id.find(sztype);
                if(x != std::end(AD2Parse.message_type_id)) {
                    ad2_message_t mt = (ad2_message_t)AD2Parse.message_type_id.at(sztype);
                    es1->PRE_FILTER_MESAGE_TYPE.push_back(mt);
                }
            }
            ad2_get_nv_slot_key_string(key.c_str(), i, SK_PREFILTER_REGEX, es1->PRE_FILTER_REGEX);

            // Load all regex search patterns for OPEN,CLOSE and TROUBLE sub keys.
            std::string regex_sk_list = SK_TROUBLE_REGEX_LIST SK_CLOSED_REGEX_LIST SK_OPEN_REGEX_LIST;
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
                        if (c == SK_TROUBLE_REGEX_LIST[0]) {
                            es1->TROUBLE_REGEX_LIST.push_back(out);
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
        } else {
            // incomplete switch call distructor.
            es1->~AD2EventSearch();
        }
    }

    ad2_printf_host(true, "%s: Init done. Found and configured %i virtual switches.", TAG, subscribers);

}

#endif /*  CONFIG_AD2IOT_MQTT_CLIENT */

