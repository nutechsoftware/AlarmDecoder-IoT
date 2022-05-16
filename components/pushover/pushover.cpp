/**
*  @file    pushover.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    01/06/2021
*  @version 1.0.1
*
*  @brief Simple commands to post to api.pushover.net
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
#if CONFIG_AD2IOT_PUSHOVER_CLIENT
static const char *TAG = "PUSHOVER";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// specific includes

//#define DEBUG_PUSHOVER
#define AD2_DEFAULT_PUSHOVER_SLOT 0

/* Constants that aren't configurable in menuconfig */
#define PUSHOVER_API_VERSION "1"
const char PUSHOVER_URL[] = "https://api.pushover.net/" PUSHOVER_API_VERSION "/messages.json";

#define PUSHOVER_COMMAND        "pushover"
#define PUSHOVER_TOKEN_SUBCMD   "apptoken"
#define PUSHOVER_USERKEY_SUBCMD "userkey"
#define PUSHOVER_SWITCH_SUBCMD  "switch"

#define PUSHOVER_CONFIG_SECTION "pushover"

#define PUSHOVER_CONFIG_SWITCH_SUFFIX_NOTIFY "notify"
#define PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN "open"
#define PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE "close"
#define PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE "trouble"
#define PUSHOVER_CONFIG_SWITCH_VERBS ( \
    PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN " " \
    PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE " " \
    PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE)


static std::vector<AD2EventSearch *> pushover_AD2EventSearches;


// forward decl

/**
 * @brief class that will be stored in the sendQ for each request.
 */
class request_message
{
public:
    request_message()
    {
        config_client = (esp_http_client_config_t *)calloc(1, sizeof(esp_http_client_config_t));
    }
    ~request_message()
    {
        if (config_client) {
            free(config_client);
        }
    }

    // client_config used the the http_sendQ
    esp_http_client_config_t* config_client;

    // Application specific
    std::string token;
    std::string userkey;
    std::string message;
    std::string post;
    std::string results;
};

/**
 * @brief ad2_http_sendQ callback before esp_http_client_perform() is called.
 * Add headers content etc.
 *
 */
static void _sendQ_ready_handler(esp_http_client_handle_t client, esp_http_client_config_t *config)
{
    esp_err_t err;
    // if perform failed this can be NULL
    if (client) {
        request_message *r = (request_message*) config->user_data;

        // Pushover message API
        //   https://pushover.net/api
        r->post = std::string("token=" + ad2_urlencode(r->token) + "&user=" + ad2_urlencode(r->userkey) + \
                              "&message=" + ad2_urlencode(r->message));

        // does not copy data just a pointer so we have to maintain memory.
        err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());

    }
}

/**
 * @brief ad2_http_sendQ callback when esp_http_client_perform() is finished.
 * Preform any final cleanup here.
 *
 *  @param [in]esp_err_t res - Results of esp_http_client_preform()
 *  @param [in]evt esp_http_client_event_t
 *  @param [in]config esp_http_client_config_t *
 *
 * @return bool TRUE: The connection done and allow sendQ worker to delete it.
 *              FALSE: The connection expects a new URL and will call preform() again.
 */
static bool _sendQ_done_handler(esp_err_t res, esp_http_client_handle_t client, esp_http_client_config_t *config)
{
    request_message *r = (request_message*) config->user_data;

    ESP_LOGI(TAG, "perform results = %d HTTP Status = %d, response length = %d response = '%s'", res,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client), r->results.c_str());

    // free message_data class will also delete the client_config in the distructor.
    delete r;

    // Report back this client request is all done and is ready to close.
    return true;
}

/**
 * @brief esp_http_client event callback.
 *
 * HTTP_EVENT_ON_DATA to capture server response message.
 * Also use for diagnostics of response headers etc.
 *
 *  @param [in]evt esp_http_client_event_t *
 */
esp_err_t _pushover_http_event_handler(esp_http_client_event_t *evt)
{
    request_message *r;
    size_t len;

    switch(evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
#endif
        break;
    case HTTP_EVENT_ERROR:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
#endif
        break;
    case HTTP_EVENT_ON_CONNECTED:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
#endif
        break;
    case HTTP_EVENT_HEADERS_SENT:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
#endif
        break;
    case HTTP_EVENT_ON_FINISH:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
#endif
        break;
    case HTTP_EVENT_DISCONNECTED:
#if defined(DEBUG_PUSHOVER)
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
#endif
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len) {
            // Save the results into our message_data structure.
            r = (request_message *)evt->user_data;
            // Limit size.
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
            len = MIN(1024, evt->data_len);
            r->results = std::string((char*)evt->data, len);
        }
        break;
    }

    return ESP_OK;
}

/**
 * @brief SmartSwitch match callback.
 * Called when the current message matches a AD2EventSearch test.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 * @note No full queue handler
 */
void on_search_match_cb_pushover(std::string *msg, AD2PartitionState *s, void *arg)
{

    AD2EventSearch *es = (AD2EventSearch *)arg;

    // es->PTR_ARG is the notification slots std::list for this notification.
    std::list<uint8_t> *notify_list = (std::list<uint8_t>*)es->PTR_ARG;
    for (uint8_t const& notify_slot : *notify_list) {

        // Container to store details needed for delivery.
        request_message *r = new request_message();

        // load our settings for this event type.
        // get the pushover user key
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION, PUSHOVER_USERKEY_SUBCMD, r->userkey, notify_slot);

        // get the pushover api token
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION, PUSHOVER_TOKEN_SUBCMD, r->token, notify_slot);

        // save the message
        r->message = es->out_message;

        // Settings specific for http_client_config
        r->config_client->url = PUSHOVER_URL;
        // set request type
        r->config_client->method = HTTP_METHOD_POST;

        // optional define an internal event handler
        r->config_client->event_handler = _pushover_http_event_handler;

        // required save internal class to user_data to be used in callback.
        r->config_client->user_data = (void *)r; // Definition of grok.. see grok.

        // Fails, but not needed to work.
        // config_client->use_global_ca_store = true;

        // Add client config to the http_sendQ for processing.
        bool res = ad2_add_http_sendQ(r->config_client, _sendQ_ready_handler, _sendQ_done_handler);
        if (!res) {
            ESP_LOGE(TAG,"Error adding HTTP request to ad2_add_http_sendQ.");
            // destroy storage class if we fail to add to the sendQ
            delete r;
        }
    }
}

/**
 * Command support values.
 */
enum {
    PUSHOVER_TOKEN_SUBCMD_ID = 0,
    PUSHOVER_USERKEY_SUBCMD_ID,
    PUSHOVER_SWITCH_SUBCMD_ID
};
char * PUSHOVER_SUBCMD [] = {
    (char*)PUSHOVER_TOKEN_SUBCMD,
    (char*)PUSHOVER_USERKEY_SUBCMD,
    (char*)PUSHOVER_SWITCH_SUBCMD,
    0 // EOF
};

/**
 * Component generic command event processing
 *
 * Usage: pushover (apptoken|userkey) <acid> [<arg>]
 */
static void _cli_cmd_pushover_event_generic(std::string &subcmd, const char *string)
{
    int accountId = -1;
    std::string buf;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    // get the accountID 1-8
    if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
        accountId = strtol(buf.c_str(), NULL, 10);
    }
    // Allowed accountId 1-8
    if (accountId > 0 && accountId < 9) {
        // <arg>
        if (ad2_copy_nth_arg(buf, string, 3) >= 0) {
            ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, subcmd.c_str(), buf.c_str(), accountId);
            ad2_printf_host(false, "Setting <acid> #%02i '%s' value '%s' finished.\r\n", accountId, subcmd.c_str(), buf.c_str());
        } else {
            buf = "";
            ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION, subcmd.c_str(), buf, accountId);
            ad2_printf_host(false, "Current <acid> #%02i '%s' value '%s'\r\n", accountId, subcmd.c_str(), buf.length() ? buf.c_str() : "EMPTY");
        }
    } else {
        ad2_printf_host(false, "Missing or invalid <acid> [1-8].\r\n");
    }
}

/**
 * Component SmartSwitch command event processing
 *
 *  Usage: pushover switch <swid> [delete|-|notify|open|close|trouble] [<arg>]
 */
static void _cli_cmd_pushover_smart_alert_switch(std::string &subcmd, const char *instring)
{
    int swID = 0;
    std::string scmd;
    std::string tbuf;
    std::string arg;

    // sub key "switch N AAAAA"
    std::string key = std::string(AD2SWITCH_CONFIG_SECTION);

    // get the sub command value validation
    if (ad2_copy_nth_arg(tbuf, instring, 2) >= 0) {
        swID = std::atoi (tbuf.c_str());
    }
    // Switch ID to the key.
    if (swID > 0 && swID <= AD2_MAX_SWITCHES) {

        // sub command
        if (ad2_copy_nth_arg(scmd, instring, 3) >= 0) {

            // load remaining arg data
            ad2_copy_nth_arg(arg, instring, 4, true);

            // sub key test.
            if (scmd.compare(AD2SWITCH_SK_DELETE1) == 0 || scmd.compare(AD2SWITCH_SK_DELETE2) == 0) {
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), NULL, swID, PUSHOVER_CONFIG_SWITCH_SUFFIX_NOTIFY, true);
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), NULL, swID, PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN, true);
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), NULL, swID, PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE, true);
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), NULL, swID, PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE, true);
                ad2_printf_host(false, "Removing switch #%i settings from pushover config.\r\n", swID);
            } else if (scmd.compare(PUSHOVER_CONFIG_SWITCH_SUFFIX_NOTIFY) == 0) {
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), arg.c_str(), swID, scmd.c_str());
                ad2_printf_host(false, "Setting switch #%i %s string to '%s'.\r\n", swID, scmd.c_str(), arg.c_str());
            } else if (scmd.compare(PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN) == 0 ||
                       scmd.compare(PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE) == 0 ||
                       scmd.compare(PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE) == 0) {
                ad2_set_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), arg.c_str(), swID, scmd.c_str());
                ad2_printf_host(false, "Setting switch #%i output string for state '%s' to '%s'.\r\n", swID, scmd.c_str(), arg.c_str());
            } else if (scmd.compare(PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN) == 0) {
                ESP_LOGW(TAG, "Unknown sub command setting '%s' ignored.", scmd.c_str());
                return;
            }
        } else {
            // check all settings verbs against provided verb and proform setting logic
            std::stringstream ss(
                PUSHOVER_CONFIG_SWITCH_SUFFIX_NOTIFY " "
                PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN " "
                PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE " "
                PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE);

            std::string sk;
            bool command_found = false;
            ad2_printf_host(false, "## [pushover] switch %i configuration.\r\n", swID);
            while (ss >> sk) {
                tbuf = "";
                ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION, key.c_str(), tbuf, swID, sk.c_str());
                if (tbuf.length()) {
                    ad2_printf_host(false, "%s = %s\r\n", sk.c_str(), tbuf.c_str());
                } else {
                    ad2_printf_host(false, "# %s = \r\n", sk.c_str());
                }
            }
            // dump finished, all done.
            return;
        }
    } else {
        ad2_printf_host(false, "Missing or invalid switch <id> 1-255\r\n");
        // TODO: DUMP all when swID is 0 or > AD2_MAX_SWITCHES
    }
}

/**
 * Component command router
 */
static void _cli_cmd_pushover_command_router(const char *string)
{
    int i;
    std::string subcmd;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    for(i = 0;; ++i) {
        if (PUSHOVER_SUBCMD[i] == 0) {
            ad2_printf_host(false, "What?\r\n");
            break;
        }
        if(subcmd.compare(PUSHOVER_SUBCMD[i]) == 0) {
            switch(i) {
            case PUSHOVER_TOKEN_SUBCMD_ID:   // 'apptoken' sub command
            case PUSHOVER_USERKEY_SUBCMD_ID: // 'userkey' sub command
                _cli_cmd_pushover_event_generic(subcmd, string);
                break;
            case PUSHOVER_SWITCH_SUBCMD_ID:
                _cli_cmd_pushover_smart_alert_switch(subcmd, string);
                break;
            }
            // all done
            break;
        }
    }
}

/**
 * @brief command list for component
 *
 */
static struct cli_command pushover_cmd_list[] = {
    {
        // ### Pushover.net notification component
        (char*)PUSHOVER_COMMAND,(char*)
        "Usage: pushover (apptoken|userkey) <acid> [<arg>]\r\n"
        "Usage: pushover switch <swid> [delete|-|notify|open|close|trouble] [<arg>]\r\n"
        "\r\n"
        "    Configuration tool for Pushover.net notification\r\n"
        "Commands:\r\n"
        "    apptoken acid [hash]    Application token/key HASH\r\n"
        "    userkey acid [hash]     User Auth Token HASH\r\n"
        "    switch swid SCMD [ARG]  Configure virtual switches\r\n"
        "Sub-Commands:\r\n"
        "    delete | -              Clear switch notification settings\r\n"
        "    notify <acid>,...       List of accounts [1-8] to use for notification\r\n"
        "    open <message>          Send <message> for OPEN events\r\n"
        "    close <message>         Send <message> for CLOSE events\r\n"
        "    trouble <message>       Send <message> for TROUBLE events\r\n"
        "Options:\r\n"
        "    acid                    Account storage location 1-8\r\n"
        "    swid                    ad2iot virtual switch ID 1-255.\r\n"
        "                            See ```switch``` command\r\n"
        "    message                 Message to send for this notification\r\n"
        , _cli_cmd_pushover_command_router
    },
};

/**
 * Register cli commands
 */
void pushover_register_cmds()
{
    // Register pushover CLI commands
    for (int i = 0; i < ARRAY_SIZE(pushover_cmd_list); i++) {
        cli_register_command(&pushover_cmd_list[i]);
    }
}

/**
 * Initialize component
 */
void pushover_init()
{

    // Register search based virtual switches if enabled.
    // [switch N]
    int subscribers = 0;
    for (int swID = 1; swID < AD2_MAX_SWITCHES; swID++) {
        // load switch settings for 'swID' and test if found
        std::string open_output_format;
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  open_output_format,
                                  swID,
                                  PUSHOVER_CONFIG_SWITCH_SUFFIX_OPEN);
        std::string close_output_format;
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  close_output_format,
                                  swID,
                                  PUSHOVER_CONFIG_SWITCH_SUFFIX_CLOSE);
        std::string trouble_output_format;
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  trouble_output_format,
                                  swID,
                                  PUSHOVER_CONFIG_SWITCH_SUFFIX_TROUBLE);
        std::string notify_slots_string;
        ad2_get_config_key_string(PUSHOVER_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  notify_slots_string,
                                  swID,
                                  PUSHOVER_CONFIG_SWITCH_SUFFIX_NOTIFY);

        // Only process entries with notify list and at least one output string.
        if ( notify_slots_string.length() &&
                ( open_output_format.length() ||
                  close_output_format.length() ||
                  trouble_output_format.length() )
           ) {
            // key switch N
            std::string key = std::string(AD2SWITCH_CONFIG_SECTION);
            key += " ";
            key += std::to_string(swID);

            // construct our search object.
            AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);

            // store validated output formats.
            es1->OPEN_OUTPUT_FORMAT = open_output_format;
            es1->CLOSE_OUTPUT_FORMAT = close_output_format;
            es1->TROUBLE_OUTPUT_FORMAT = trouble_output_format;

            // save notify list to PTR_ARG.
            std::list<uint8_t> *pslots = new std::list<uint8_t>;
            std::vector<std::string> vres;
            ad2_tokenize(notify_slots_string, ",", vres);
            for (auto &slotstring : vres) {
                uint8_t s = std::atoi(slotstring.c_str());
                pslots->push_front((uint8_t)s & 0xff);
            }
            es1->PTR_ARG = pslots;

            // save the NVS Virtual SWITCH ID so we can read the data back later.
            es1->INT_ARG = swID;

            // switch filter settings
            std::string filter;

            // Get the optional switch types to listen for.
            std::string types = "";
            std::vector<std::string> notify_types_v;
            ad2_get_config_key_string(key.c_str(), AD2SWITCH_SK_TYPES, types);
            ad2_tokenize(types, ", ", notify_types_v);
            for (auto &sztype : notify_types_v) {
                ad2_trim(sztype);
                auto x = AD2Parse.message_type_id.find(sztype);
                if(x != std::end(AD2Parse.message_type_id)) {
                    ad2_message_t mt = (ad2_message_t)AD2Parse.message_type_id.at(sztype);
                    es1->PRE_FILTER_MESAGE_TYPE.push_back(mt);
                }
            }

            // load [switch N] required regex match settings
            std::string prefilter_regex;
            ad2_get_config_key_string(key.c_str(), AD2SWITCH_SK_FILTER, prefilter_regex);

            // Load all regex search patterns for open, close, and trouble sub keys.
            std::string regex_sk_list = AD2SWITCH_SK_OPEN " "
                                        AD2SWITCH_SK_CLOSE " "
                                        AD2SWITCH_SK_TROUBLE;

            std::stringstream ss(regex_sk_list);
            std::string sk;
            int sk_index = 0;
            while (ss >> sk) {
                for ( int a = 1; a < AD2_MAX_SWITCH_SEARCH_KEYS; a++) {
                    std::string out = "";
                    ad2_get_config_key_string(key.c_str(), sk.c_str(), out, a);

                    if ( out.length()) {
                        if (sk_index == 0) {
                            es1->OPEN_REGEX_LIST.push_back(out);
                        }
                        if (sk_index == 1) {
                            es1->CLOSE_REGEX_LIST.push_back(out);
                        }
                        if (sk_index == 2) {
                            es1->TROUBLE_REGEX_LIST.push_back(out);
                        }
                    }
                }
                sk_index++;
            }

            // Must provide at least one states or it will be skipped.
            if (es1->OPEN_REGEX_LIST.size() ||
                    es1->CLOSE_REGEX_LIST.size() ||
                    es1->TROUBLE_REGEX_LIST.size()) {
                // Save the search to a list for management.
                pushover_AD2EventSearches.push_back(es1);

                // subscribe to the callback for events.
                AD2Parse.subscribeTo(on_search_match_cb_pushover, es1);

                // keep track of how many for user feedback.
                subscribers++;

            } else {
                // incomplete switch so delete it and supporting pointers.
                delete pslots;
                delete es1;
                ESP_LOGE(TAG, "Error in config section [switch %i]. Missing required open, close, or fault filter expressions.", swID);
            }
        } else {
            if (open_output_format.length() || close_output_format.length()
                    || trouble_output_format.length()) {
                ESP_LOGE(TAG, "Error in config for switch [switch %i]. Missing on or more required open,close, or fault output expressions.", swID);
            }
        }
    }

    ad2_printf_host(true, "%s: Init done. Found and configured %i virtual switches.", TAG, subscribers);

}

/**
 * component memory cleanup
 */
void pushover_free()
{
}

#endif /*  CONFIG_AD2IOT_PUSHOVER_CLIENT */

