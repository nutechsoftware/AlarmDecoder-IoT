/**
*  @file    twilio.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    09/12/2020
*  @version 1.0.4
*
*  @brief Simple commands to post to api.twilio.com
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

// Disable via sdkconfig
#if CONFIG_AD2IOT_TWILIO_CLIENT
static const char *TAG = "TWILIO";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// specific includes
#include <fmt/core.h>

//#define DEBUG_TWILIO

/* Constants that aren't configurable in menuconfig */
#define TWILIO_API_VERSION "2010-04-01"
#define TWILIO_URL_FMT "https://api.twilio.com/" TWILIO_API_VERSION "/Accounts/%s/%s"
#define SENDGRID_API_VERSION "v3"
#define SENDGRID_URL "https://api.sendgrid.com/" SENDGRID_API_VERSION "/mail/send"

#define TWILIO_COMMAND        "twilio"
#define TWILIO_DISABLE_SUBCMD "disable"
#define TWILIO_SID_SUBCMD     "sid"
#define TWILIO_TOKEN_SUBCMD   "token"
#define TWILIO_FROM_SUBCMD    "from"
#define TWILIO_TO_SUBCMD      "to"
#define TWILIO_TYPE_SUBCMD    "type"
#define TWILIO_FORMAT_SUBCMD  "format"
#define TWILIO_SWITCH_SUBCMD  "switch"

#define TWILIO_CONFIG_SECTION "twilio"

#define TWILIO_CONFIG_SWITCH_SUFFIX_NOTIFY "notify"
#define TWILIO_CONFIG_SWITCH_SUFFIX_OPEN "open"
#define TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE "close"
#define TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE "trouble"
#define TWILIO_CONFIG_SWITCH_VERBS ( \
    TWILIO_CONFIG_SWITCH_SUFFIX_OPEN " " \
    TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE " " \
    TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE)

// Notification types
#define TWILIO_NOTIFY_MESSAGE "M"
#define TWILIO_NOTIFY_CALL    "C"
#define TWILIO_NOTIFY_EMAIL   "E"

std::vector<AD2EventSearch *> twilio_AD2EventSearches;


// forward decl

enum {
    TWILIO_NEXT_STATE_DONE=0,
    TWILIO_NEXT_STATE_GET,
};

/**
 * @brief class that will be stored in the sendQ for each request.
 */
class tw_request_message
{
public:
    tw_request_message()
    {
        config_client = (esp_http_client_config_t *)calloc(1, sizeof(esp_http_client_config_t));
        state = TWILIO_NEXT_STATE_DONE;
    }
    ~tw_request_message()
    {
        if (config_client) {
            free(config_client);
        }
    }

    // client_config used the the http_sendQ
    esp_http_client_config_t* config_client;

    // Application specific
    int notify_slot;
    std::string message;
    std::string url;
    std::string post;
    std::string results;
    int state;
};

/**
 * Build the json POST body for a twilio Call API call
 *   https://www.twilio.com/docs/sms/api/message-resource
 *   https://stackoverflow.com/questions/48898162/interactive-voice-menu-on-twilio-twiml
 */
static void _build_twilio_message_post(esp_http_client_handle_t client, tw_request_message *r)
{
    esp_err_t err;
    std::string auth_header;

    // get token for this notification slot from config.
    std::string sidString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_SID_SUBCMD, sidString, r->notify_slot);

    // get sid for this notification slot from config.
    std::string tokenString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TOKEN_SUBCMD, tokenString, r->notify_slot);

    // Set the Authorization header
    auth_header = "Basic " + ad2_make_basic_auth_string(sidString, tokenString);
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // get from for this notification slot from config.
    std::string fromString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_FROM_SUBCMD, fromString, r->notify_slot);

    // get to for this notification slot from config.
    std::string toString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TO_SUBCMD, toString, r->notify_slot);

    // Set post body
    r->post = "To=" + ad2_urlencode(toString) + "&From=" + ad2_urlencode(fromString) + \
              "&Body=" + ad2_urlencode(r->message);

    // does not copy data just a pointer so we have to maintain memory.
    err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());
}

/**
 * Build the json POST body for a twilio Message API call
 *   https://www.twilio.com/docs/voice/api/sip-making-calls
 */
static void _build_twilio_call_post(esp_http_client_handle_t client, tw_request_message *r)
{
    esp_err_t err;
    std::string auth_header;

    // get sid for this notification slot from config.
    std::string sidString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_SID_SUBCMD, sidString, r->notify_slot);

    // get token for this notification slot from config.
    std::string tokenString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TOKEN_SUBCMD, tokenString, r->notify_slot);

    // Set the Authorization header
    auth_header = "Basic " + ad2_make_basic_auth_string(sidString, tokenString);
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // Build the Twiml message using the format and message as the arg
    // TODO: Multiple args by splitting r->message using , or |

    // get format template string for this notification slot from config.
    std::string formatString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_FORMAT_SUBCMD, formatString, r->notify_slot);

    // get from for this notification slot from config.
    std::string fromString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_FROM_SUBCMD, fromString, r->notify_slot);

    // get to for this notification slot from config.
    std::string toString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TO_SUBCMD, toString, r->notify_slot);

    std::string twiml = fmt::format(formatString, r->message);
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "Sending Twiml message: %s", twiml.c_str());
#endif

    // Set post body
    r->post = "To=" + ad2_urlencode(toString) + "&From=" + ad2_urlencode(fromString) + \
              "&Twiml=" + ad2_urlencode(twiml);

    // does not copy data just a pointer so we have to maintain memory.
    err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());
}

/**
 * Build the json POST body for a sendgrid message API call
 */
static void _build_sendgrid_post(esp_http_client_handle_t client, tw_request_message *r)
{
    esp_err_t err;

    // get sid for this notification slot from config.
    std::string tokenString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TOKEN_SUBCMD, tokenString, r->notify_slot);

    // Set the Authorization header
    std::string auth_header = "Bearer " + tokenString;
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // object: root
    cJSON *_root = cJSON_CreateObject();

    // array: personalizations
    cJSON *_personalizations = cJSON_CreateArray();

    // get from for this notification slot from config.
    std::string fromString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_FROM_SUBCMD, fromString, r->notify_slot);

    // object: from
    cJSON *_from = cJSON_CreateObject();
    cJSON_AddStringToObject(_from, "email", fromString.c_str());

    // array: content
    cJSON *_content = cJSON_CreateArray();
    cJSON *_text = cJSON_CreateObject();
    cJSON_AddItemToArray(_content, _text);
    cJSON_AddStringToObject(_text, "type", "text/plain");
    cJSON_AddStringToObject(_text, "value", r->message.c_str());

    // add top level items to root
    cJSON_AddStringToObject(_root, "subject", ad2_string_printf("AD2 ALERT '%s'", r->message.c_str()).c_str());
    cJSON_AddItemToObject(_root, "from", _from);
    cJSON_AddItemToObject(_root, "personalizations", _personalizations);
    cJSON_AddItemToObject(_root, "content", _content);

    // get from for this notification slot from config.
    std::string toString;
    ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TO_SUBCMD, toString, r->notify_slot);

    // _personalizations -> to[]
    std::vector<std::string> to_list;
    ad2_tokenize(toString, ", ", to_list);
    for (auto &szto : to_list) {
        cJSON *_to = cJSON_CreateArray();
        cJSON *_pitem = cJSON_CreateObject();
        cJSON_AddItemToArray(_personalizations, _pitem);
        cJSON_AddItemToObject(_pitem, "to", _to);
        cJSON *_email = cJSON_CreateObject();
        cJSON_AddItemToArray(_to, _email);
        cJSON_AddStringToObject(_email, "email", szto.c_str());
    }

    // Build and save final json string and cleanup
    char *json = NULL;
    json = cJSON_Print(_root);
    r->post = json;

    // does not copy data just a pointer so we have to maintain memory.
    err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());

    // set content type to json
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");

    cJSON_free(json);
    cJSON_Delete(_root);

}

/**
 * @brief ad2_http_sendQ callback before esp_http_client_perform() is called.
 * Add headers content etc.
 *
 */
static void _sendQ_ready_handler(esp_http_client_handle_t client, esp_http_client_config_t *config)
{
    // if perform failed this can be NULL
    if (client) {
        tw_request_message *r = (tw_request_message*) config->user_data;

        // get twilio [type] : Used to determine delivery settings using SendGrid or twilio servers.
        std::string type;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TYPE_SUBCMD, type, r->notify_slot);
        type.resize(1);

        // Build POST based upon notification type
        switch(type[0]) {

        // Twilio Call api
        //     https://www.twilio.com/docs/voice/api/sip-making-calls
        case TWILIO_NOTIFY_CALL[0]:
            _build_twilio_call_post(client, r);
            break;

        // Twilio Messages api
        //     https://www.twilio.com/docs/sms/api/message-resource
        case TWILIO_NOTIFY_MESSAGE[0]:
            _build_twilio_message_post(client, r);
            break;

        // SendGrid Email api.
        //     https://docs.sendgrid.com/api-reference/mail-send/mail-send
        case TWILIO_NOTIFY_EMAIL[0]:
            _build_sendgrid_post(client, r);
            break;

        }
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
    tw_request_message *r = (tw_request_message*) config->user_data;
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "perform results = %d HTTP Status = %d, response length = %d response = '%s'", res,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client), r->results.c_str());
#endif

    // parse some data from json response if one exists.
    std::string szStatus;
    std::string szMessage;
    std::string notify_url;

    cJSON * root = cJSON_Parse(r->results.c_str());
    if (root) {
        // check for exceptions and report
        // https://www.twilio.com/docs/usage/twilios-response#response-formats-exceptions
        cJSON * code = cJSON_GetObjectItem(root, "code");
        if (code) {
            ESP_LOGI(TAG,"parse exception code json");
            szStatus = "Exception";
            int ret_code = code->valueint;

            std::string ret_message;
            cJSON * jso_message = cJSON_GetObjectItem(root, "message");
            if (jso_message && jso_message->valuestring) {
                ret_message = jso_message->valuestring;
            }

            std::string ret_more_info;
            cJSON * jso_more_info = cJSON_GetObjectItem(root, "more_info");
            if (jso_more_info && jso_more_info->valuestring) {
                ret_more_info = jso_more_info->valuestring;
            }

            int ret_status = -1;
            cJSON * jso_status = cJSON_GetObjectItem(root, "status");
            if (jso_status) {
                ret_status = jso_status->valueint;
            }

            szMessage = ad2_string_printf("code: %i, message: '%s', more_info: '%s', status: %i",
                                          ret_code, ret_message.c_str(), ret_more_info.c_str(), ret_status);
        }

        // If the request contains a subresource_uris and Notifications.json then request it next else we are done.
        cJSON * subr_uris = cJSON_GetObjectItem(root, "subresource_uris");
        if (subr_uris) {
            ESP_LOGI(TAG,"parse subresource_uris json");
            szStatus = "subresource_uris";
            // { "subresource_uris" : { "notifications" : "FOO", ...}}
            cJSON * notifications_url = cJSON_GetObjectItem(subr_uris, "notifications");
            if (notifications_url && notifications_url->valuestring) {
                notify_url = notifications_url->valuestring;
            }
        }

        // json error
        cJSON * errors = cJSON_GetObjectItem(root, "errors");
        if (errors) {
            ESP_LOGI(TAG,"parse error json");
            szStatus = "error";
            cJSON * error = cJSON_GetArrayItem(errors, 0);
            if (error) {
                cJSON * message = cJSON_GetObjectItem(error, "message");
                if (message && message->valuestring) {
                    szMessage = message->valuestring;
                }
            }
        }

        cJSON_Delete(root);
    }

    ESP_LOGI(TAG,"Notify slot #%i response code: '%i' status: '%s' message: '%s'", r->notify_slot, esp_http_client_get_status_code(client), szStatus.c_str(), szMessage.c_str());

    // If first request was OK and we are scheduled to make GET for details.
    if (res == ESP_OK && r->state == TWILIO_NEXT_STATE_GET) {

        // Last thing we will do and we are done.
        r->state = TWILIO_NEXT_STATE_DONE;

        // empty the results string for next response.
        r->results = "";

        if (notify_url.length()) {
            // set the next url to the notifications url
            r->url = notify_url;
            r->config_client->url = r->url.c_str();
            // GET request for status url
            esp_http_client_set_method(client, HTTP_METHOD_GET);
            esp_http_client_set_url(client,  r->config_client->url);
            return false;
        }
    }

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
esp_err_t _twilio_http_event_handler(esp_http_client_event_t *evt)
{
    tw_request_message *r;

    switch(evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
#endif
        break;
    case HTTP_EVENT_ERROR:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
#endif
        break;
    case HTTP_EVENT_ON_CONNECTED:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
#endif
        break;
    case HTTP_EVENT_HEADERS_SENT:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
#endif
        break;
    case HTTP_EVENT_ON_FINISH:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
#endif
        break;
    case HTTP_EVENT_DISCONNECTED:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
#endif
        break;
    case HTTP_EVENT_ON_DATA:
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%i", evt->data_len);
#endif
        if (evt->data_len) {
            // Save the results into our message_data structure.
            r = (tw_request_message *)evt->user_data;
            r->results.append((char*)evt->data, evt->data_len);
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
void on_search_match_cb_tw(std::string *msg, AD2PartitionState *s, void *arg)
{

    AD2EventSearch *es = (AD2EventSearch *)arg;
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "ON_SEARCH_MATCH_CB: '%s' -> '%s' notify slot #%02i", msg->c_str(), es->out_message.c_str(), es->INT_ARG);
#endif
    // es->PTR_ARG is the notification slots std::list for this notification.
    std::list<uint8_t> *notify_list = (std::list<uint8_t>*)es->PTR_ARG;
    for (uint8_t const& notify_slot : *notify_list) {

        // skip if this notification slot if disabled.
        // cli example: twilio disable 1 true
        bool bDiabled = false;
        ad2_get_config_key_bool(TWILIO_CONFIG_SECTION, TWILIO_DISABLE_SUBCMD, &bDiabled, notify_slot);
        if (bDiabled) {
            continue;
        }

        // Container to store details needed for delivery.
        tw_request_message *r = new tw_request_message();

        // save the Account storage ID for the notification.
        r->notify_slot = notify_slot;

        // save the message
        r->message = es->out_message;

        // get sid for this notification slot from config.
        std::string sidString;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_SID_SUBCMD, sidString, r->notify_slot);

        // get twilio [type] : Used to determine delivery settings using SendGrid or twilio servers.
        std::string type;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION, TWILIO_TYPE_SUBCMD, type, notify_slot);
        type.resize(1);

        // Settings specific for http_client_config

        // Configure the URL based upon the request type.
        switch(type[0]) {

        // Twilio Call api
        //     https://www.twilio.com/docs/voice/api/sip-making-calls
        case TWILIO_NOTIFY_CALL[0]:
            // Build dynamic URL and set pointer to config_client->url;
            r->url = ad2_string_printf(TWILIO_URL_FMT, sidString.c_str(), "Calls.json");
            r->config_client->url = r->url.c_str();
            // POST and parse resutls for url to query.
            r->state = TWILIO_NEXT_STATE_GET;
            break;

        // Twilio Messages api
        //     https://www.twilio.com/docs/sms/api/message-resource
        case TWILIO_NOTIFY_MESSAGE[0]:
            // Build dynamic URL and set pointer to config_client->url;
            r->url = ad2_string_printf(TWILIO_URL_FMT, sidString.c_str(), "Messages.json");
            r->config_client->url = r->url.c_str();
            // Single POST request then done
            r->state = TWILIO_NEXT_STATE_DONE;
            break;

        // SendGrid Email api.
        //     https://docs.sendgrid.com/api-reference/mail-send/mail-send
        case TWILIO_NOTIFY_EMAIL[0]:
            // constant URL just set pointer to string.
            r->config_client->url = SENDGRID_URL;
            // Single POST request then done
            r->state = TWILIO_NEXT_STATE_DONE;
            break;

        // Unknown.
        default:
            ESP_LOGW(TAG, "Unknown message type '%c' aborting adding to sendQ.", type[0]);
            // destroy storage class if we fail to add to the sendQ
            delete r;
            return;
        }

        // set request type
        r->config_client->method = HTTP_METHOD_POST;

        // optional define an internal event handler
        r->config_client->event_handler = _twilio_http_event_handler;

        // required save internal class to user_data to be used in callback.
        r->config_client->user_data = (void *)r; // Definition of grok.. see grok.

        // Add client config to the http_sendQ for processing.
        bool res = ad2_add_http_sendQ(r->config_client, _sendQ_ready_handler, _sendQ_done_handler);
        if (res) {
            ESP_LOGI(TAG,"Switch #%i match message '%s'. Sending '%s' to acid #%i", es->INT_ARG, msg->c_str(), es->out_message.c_str(), notify_slot);
        } else {
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
    TWILIO_SID_SUBCMD_ID = 0,
    TWILIO_TOKEN_SUBCMD_ID,
    TWILIO_FROM_SUBCMD_ID,
    TWILIO_TO_SUBCMD_ID,
    TWILIO_TYPE_SUBCMD_ID,
    TWILIO_FORMAT_SUBCMD_ID,
    TWILIO_DISABLE_SUBCMD_ID,
    TWILIO_SWITCH_SUBCMD_ID
};
char * TWILIO_SUBCMDS [] = {
    (char*)TWILIO_SID_SUBCMD,
    (char*)TWILIO_TOKEN_SUBCMD,
    (char*)TWILIO_FROM_SUBCMD,
    (char*)TWILIO_TO_SUBCMD,
    (char*)TWILIO_TYPE_SUBCMD,
    (char*)TWILIO_FORMAT_SUBCMD,
    (char*)TWILIO_DISABLE_SUBCMD,
    (char*)TWILIO_SWITCH_SUBCMD,
    0 // EOF
};

/**
 * Component generic command event processing
 *
 * Usage: twilio (sid|token|from|to|type|format) <acid> [<arg>]
 */
static void _cli_cmd_twilio_event_generic(std::string &subcmd, const char *string)
{
    int accountId = -1;
    std::string buf;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    // get the accountID 1-999
    if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
        accountId = strtol(buf.c_str(), NULL, 10);
    }
    // Allowed accountId 1-999
    if (accountId > 0 && accountId < 1000) {
        // <arg>
        if (ad2_copy_nth_arg(buf, string, 3, true) >= 0) {

            // don't remove ws from format string.
            if (subcmd.compare(TWILIO_FORMAT_SUBCMD) != 0) {
                ad2_remove_ws(buf);
            }

            // disabled is bool all others are string.
            if(subcmd.compare(TWILIO_DISABLE_SUBCMD) == 0) {
                ad2_set_config_key_bool(TWILIO_CONFIG_SECTION, subcmd.c_str(), (buf[0] == 'Y' || buf[0] ==  'y'), accountId);
            } else {
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, subcmd.c_str(), buf.c_str(), accountId);
            }
            ad2_printf_host(false, "Setting '%s' value '%s' finished.\r\n", subcmd.c_str(), buf.c_str());
        } else {
            buf = "";
            ad2_get_config_key_string(TWILIO_CONFIG_SECTION, subcmd.c_str(), buf, accountId);
            if(subcmd.compare(TWILIO_DISABLE_SUBCMD) == 0) {
                if (buf.compare("true") == 0) {
                    buf = "Y";
                }
            }
            ad2_printf_host(false, "Current acid #%02i '%s' value '%s'\r\n", accountId, subcmd.c_str(), buf.length() ? buf.c_str() : "EMPTY");
        }
    } else {
        ad2_printf_host(false, "Missing or invalid <acid> [1-999].\r\n");
    }
}

/**
 * Component SmartSwitch command event processing
 *
 * Usage: twilio switch <swid> [delete|-|notify|open|close|trouble] [<arg>]
 */
static void _cli_cmd_twilio_smart_alert_switch(std::string &subcmd, const char *instring)
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
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), NULL, swID, TWILIO_CONFIG_SWITCH_SUFFIX_NOTIFY, true);
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), NULL, swID, TWILIO_CONFIG_SWITCH_SUFFIX_OPEN, true);
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), NULL, swID, TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE, true);
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), NULL, swID, TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE, true);
                ad2_printf_host(false, "Removing switch #%i settings from twilio config.\r\n", swID);
            } else if (scmd.compare(TWILIO_CONFIG_SWITCH_SUFFIX_NOTIFY) == 0) {
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), arg.c_str(), swID, scmd.c_str());
                ad2_printf_host(false, "Setting switch #%i %s string to '%s'.\r\n", swID, scmd.c_str(), arg.c_str());
            } else if (scmd.compare(TWILIO_CONFIG_SWITCH_SUFFIX_OPEN) == 0 ||
                       scmd.compare(TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE) == 0 ||
                       scmd.compare(TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE) == 0) {
                ad2_set_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), arg.c_str(), swID, scmd.c_str());
                ad2_printf_host(false, "Setting switch #%i output string for state '%s' to '%s'.\r\n", swID, scmd.c_str(), arg.c_str());
            } else if (scmd.compare(TWILIO_CONFIG_SWITCH_SUFFIX_OPEN) == 0) {
                ESP_LOGW(TAG, "Unknown sub command setting '%s' ignored.", scmd.c_str());
                return;
            }
        } else {
            // check all settings verbs against provided verb and proform setting logic
            std::stringstream ss(
                TWILIO_CONFIG_SWITCH_SUFFIX_NOTIFY " "
                TWILIO_CONFIG_SWITCH_SUFFIX_OPEN " "
                TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE " "
                TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE);

            std::string sk;
            ad2_printf_host(false, "## [twilio] switch %i configuration.\r\n", swID);
            while (ss >> sk) {
                tbuf = "";
                ad2_get_config_key_string(TWILIO_CONFIG_SECTION, key.c_str(), tbuf, swID, sk.c_str());
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
static void _cli_cmd_twilio_command_router(const char *string)
{
    int i;
    std::string subcmd;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    for(i = 0;; ++i) {
        if (TWILIO_SUBCMDS[i] == 0) {
            ad2_printf_host(false, "What?\r\n");
            break;
        }
        if(subcmd.compare(TWILIO_SUBCMDS[i]) == 0) {
            switch(i) {
            case TWILIO_SID_SUBCMD_ID:   // 'sid' sub command
            case TWILIO_TOKEN_SUBCMD_ID: // 'token' sub command
            case TWILIO_FROM_SUBCMD_ID:  // 'from' sub command
            case TWILIO_TO_SUBCMD_ID:  // 'to' sub command
            case TWILIO_TYPE_SUBCMD_ID:  // 'type' sub command
            case TWILIO_FORMAT_SUBCMD_ID:  // 'format' sub command
            case TWILIO_DISABLE_SUBCMD_ID:  // 'disable' sub command
                _cli_cmd_twilio_event_generic(subcmd, string);
                break;
            case TWILIO_SWITCH_SUBCMD_ID:
                _cli_cmd_twilio_smart_alert_switch(subcmd, string);
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
static struct cli_command twilio_cmd_list[] = {
    {
        // ### Twilio notification component
        (char*)TWILIO_COMMAND,(char*)
        "Usage: twilio (disable|sid|token|from|to|type|format) <acid> [<arg>]\r\n"
        "Usage: twilio switch <swid> [delete|-|notify|open|close|trouble] [<arg>]\r\n"
        "\r\n"
        "    Configuration tool for Twilio + SendGrid notifications\r\n"
        "Commands:\r\n"
        "    disable acid [Y|N]      Disable notification account(acid)\r\n"
        "    sid acid [hash]         Twilio String Identifier(SID)\r\n"
        "    token acid [hash]       Twilio Auth Token\r\n"
        "    from acid [address]     Validated Email or Phone #\r\n"
        "    to acid [address]       Email or Phone #\r\n"
        "    type acid [M|C|E]       Notification type Mail, Call, EMail\r\n"
        "    format acid [format]    Output format string\r\n"
        "    switch swid SCMD [ARG]  Configure switches\r\n"
        "Sub-Commands: switch\r\n"
        "    delete | -              Clear switch notification settings\r\n"
        "    notify <acid>,...       List of accounts [1-999] to use for notification\r\n"
        "    open <message>          Send <message> for OPEN events\r\n"
        "    close <message>         Send <message> for CLOSE events\r\n"
        "    trouble <message>       Send <message> for TROUBLE events\r\n"
        "Options:\r\n"
        "    acid                    Account storage location 1-999\r\n"
        "    swid                    ad2iot virtual switch ID 1-255.\r\n"
        "                            See ```switch``` command\r\n"
        "    message                 Message to send for this notification\r\n"
        "    address                 EMail or Phone # depending on type\r\n"
        "    format                  Template format string\r\n"
        , _cli_cmd_twilio_command_router
    },
};

/**
 * Register cli commands
 */
void twilio_register_cmds()
{
    // Register twilio CLI commands
    for (int i = 0; i < ARRAY_SIZE(twilio_cmd_list); i++) {
        cli_register_command(&twilio_cmd_list[i]);
    }
}

/**
 * Initialize component
 */
void twilio_init()
{

    // Register search based virtual switches if enabled.
    // [switch N]
    int subscribers = 0;
    for (int swID = 1; swID < AD2_MAX_SWITCHES; swID++) {
        // load switch settings for 'swID' and test if found
        std::string open_output_format;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  open_output_format,
                                  swID,
                                  TWILIO_CONFIG_SWITCH_SUFFIX_OPEN);
        std::string close_output_format;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  close_output_format,
                                  swID,
                                  TWILIO_CONFIG_SWITCH_SUFFIX_CLOSE);
        std::string trouble_output_format;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  trouble_output_format,
                                  swID,
                                  TWILIO_CONFIG_SWITCH_SUFFIX_TROUBLE);
        std::string notify_slots_string;
        ad2_get_config_key_string(TWILIO_CONFIG_SECTION,
                                  AD2SWITCH_CONFIG_SECTION,
                                  notify_slots_string,
                                  swID,
                                  TWILIO_CONFIG_SWITCH_SUFFIX_NOTIFY);

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

            // Default switch state from global switch settings
            int defaultState = AD2_STATE_UNKNOWN; // default default to unknown.
            ad2_get_config_key_int(key.c_str(),
                                   AD2SWITCH_SK_DEFAULT,
                                   &defaultState);

            // Auto reset time from global switch settings
            int autoReset = 0;
            ad2_get_config_key_int(key.c_str(),
                                   AD2SWITCH_SK_RESET,
                                   &autoReset);

            // construct our search object.
            AD2EventSearch *es1 = new AD2EventSearch((AD2_CMD_ZONE_state_t)defaultState, autoReset);

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
            es1->PRE_FILTER_REGEX = prefilter_regex;

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
                twilio_AD2EventSearches.push_back(es1);

                // subscribe to the callback for events.
                AD2Parse.subscribeTo(on_search_match_cb_tw, es1);

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
void twilio_free()
{
}

#endif /*  CONFIG_AD2IOT_TWILIO_CLIENT */

