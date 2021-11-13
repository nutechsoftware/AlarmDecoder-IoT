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

//#define DEBUG_TWILIO
#define AD2_DEFAULT_TWILIO_SLOT 0

/* Constants that aren't configurable in menuconfig */
#define TWILIO_API_VERSION "2010-04-01"
#define TWILIO_URL_FMT "https://api.twilio.com/" TWILIO_API_VERSION "/Accounts/%s/%s"
#define SENDGRID_API_VERSION "v3"
#define SENDGRID_URL "https://api.sendgrid.com/" SENDGRID_API_VERSION "/mail/send"

#define TWILIO_COMMAND        "twilio"
#define TWILIO_CFG_PREFIX     "tw"
#define TWILIO_SID_SUBCMD     "sid"
#define TWILIO_TOKEN_SUBCMD   "token"
#define TWILIO_FROM_SUBCMD    "from"
#define TWILIO_TO_SUBCMD      "to"
#define TWILIO_TYPE_SUBCMD    "type"
#define TWILIO_SWITCH_SUBCMD  "switch"

#define MAX_SEARCH_KEYS 9

// NV storage sub key values for virtual search switch
#define SK_NOTIFY_SLOT       "N"
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

// Notification types
#define TWILIO_NOTIFY_MESSAGE "M"
#define TWILIO_NOTIFY_CALL    "C"
#define TWILIO_NOTIFY_EMAIL   "E"

std::vector<AD2EventSearch *> twilio_AD2EventSearches;


#ifdef __cplusplus
extern "C" {
#endif

/* forward decl */

enum {
    TWILIO_NEXT_STATE_DONE=0,
    TWILIO_NEXT_STATE_GET,
};

/**
 * @brief class that will be stored in the sendQ for each request.
 */
class request_message
{
public:
    request_message()
    {
        config_client = (esp_http_client_config_t *)calloc(1, sizeof(esp_http_client_config_t));
        state = TWILIO_NEXT_STATE_DONE;
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
    std::string type;
    std::string sid;
    std::string token;
    std::string from;
    std::string to;
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
static void _build_twilio_message_post(esp_http_client_handle_t client, request_message *r)
{
    esp_err_t err;
    std::string auth_header;

    // Set the Authorization header
    auth_header = "Basic " + ad2_make_basic_auth_string(r->sid, r->token);
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // Set post body
    r->post = "To=" + ad2_urlencode(r->to) + "&From=" + ad2_urlencode(r->from) + \
              "&Body=" + ad2_urlencode(r->message);

    // does not copy data just a pointer so we have to maintain memory.
    err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());
}

/**
 * Build the json POST body for a twilio Message API call
 *   https://www.twilio.com/docs/voice/api/sip-making-calls
 */
static void _build_twilio_call_post(esp_http_client_handle_t client, request_message *r)
{
    esp_err_t err;
    std::string auth_header;

    // Set the Authorization header
    auth_header = "Basic " + ad2_make_basic_auth_string(r->sid, r->token);
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // Set post body
    r->post = "To=" + ad2_urlencode(r->to) + "&From=" + ad2_urlencode(r->from) + \
              "&Twiml=" + ad2_urlencode(r->message);

    // does not copy data just a pointer so we have to maintain memory.
    err = esp_http_client_set_post_field(client, r->post.c_str(), r->post.length());
}

/**
 * Build the json POST body for a sendgrid message API call
 */
static void _build_sendgrid_post(esp_http_client_handle_t client, request_message *r)
{
    esp_err_t err;

    // Set the Authorization header
    std::string auth_header = "Bearer " + r->token;
    esp_http_client_set_header(client, "Authorization", auth_header.c_str());

    // object: root
    cJSON *_root = cJSON_CreateObject();

    // array: personalizations
    cJSON *_personalizations = cJSON_CreateArray();

    // object: from
    cJSON *_from = cJSON_CreateObject();
    cJSON_AddStringToObject(_from, "email", r->from.c_str());

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

    // _personalizations -> to[]
    std::vector<std::string> to_list;
    ad2_tokenize(r->to, ", ", to_list);
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
        request_message *r = (request_message*) config->user_data;

        // Build POST based upon notification type
        switch(r->type[0]) {

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
    request_message *r = (request_message*) config->user_data;
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "perform results = %d HTTP Status = %d, response length = %d response = '%s'", res,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client), r->results.c_str());
#endif
    // If first request was OK and we are scheduled to make GET for details.
    if (res == ESP_OK && r->state == TWILIO_NEXT_STATE_GET) {

        // If the request contains a subresource_uris and Notifications.json then request it next else we are done.
        cJSON * root   = cJSON_Parse(r->results.c_str());
        cJSON * subr_uris = cJSON_GetObjectItem(root, "subresource_uris");
        // { "subresource_uris" : { "notifications" : "FOO", ...}}
        cJSON * notifications_url = cJSON_GetObjectItem(subr_uris, "notifications");
        std::string notify_url;
        if (notifications_url->valuestring) {
            notify_url = notifications_url->valuestring;
        }
        cJSON_Delete(root);

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
    // connection is finished return done.
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
    request_message *r;
    size_t len;

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
            r = (request_message *)evt->user_data;
            r->results.append((char*)evt->data, evt->data_len);
        }
        break;
    }

    return ESP_OK;
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
void on_search_match_cb_tw(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    std::string nvkey;

    AD2EventSearch *es = (AD2EventSearch *)arg;
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "ON_SEARCH_MATCH_CB: '%s' -> '%s' notify slot #%02i", msg->c_str(), es->out_message.c_str(), es->INT_ARG);
#endif
    // es->PTR_ARG is the notification slots std::list for this notification.
    std::list<uint8_t> *notify_list = (std::list<uint8_t>*)es->PTR_ARG;
    for (uint8_t const& notify_slot : *notify_list) {

        // Container to store details needed for delivery.
        request_message *r = new request_message();

        // load our settings for this event type.

        // save the [type] : Used to determin delivery using sendgrid or twilio servers.
        nvkey = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TYPE_SUBCMD);
        ad2_get_nv_slot_key_string(nvkey.c_str(), notify_slot, nullptr, r->type);
        r->type.resize(1);

        // save the {sid} : twilio api sid
        nvkey = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_SID_SUBCMD);
        ad2_get_nv_slot_key_string(nvkey.c_str(), notify_slot, nullptr, r->sid);

        // save the {token} : twilio api token
        nvkey = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TOKEN_SUBCMD);
        ad2_get_nv_slot_key_string(nvkey.c_str(), notify_slot, nullptr, r->token);

        // save the {from} : twilio api from
        nvkey = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_FROM_SUBCMD);
        ad2_get_nv_slot_key_string(nvkey.c_str(), notify_slot, nullptr, r->from);

        // save the {to} : twilio api to
        nvkey = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TO_SUBCMD);
        ad2_get_nv_slot_key_string(nvkey.c_str(), notify_slot, nullptr, r->to);

        // save the message
        r->message = es->out_message;

        // Settings specific for http_client_config

        // Configure the URL based upon the request type.
        switch(r->type[0]) {

        // Twilio Call api
        //     https://www.twilio.com/docs/voice/api/sip-making-calls
        case TWILIO_NOTIFY_CALL[0]:
            // Build dynamic URL and set pointer to config_client->url;
            r->url = ad2_string_printf(TWILIO_URL_FMT, r->sid.c_str(), "Calls.json");
            r->config_client->url = r->url.c_str();
            // POST and parse resutls for url to query.
            r->state = TWILIO_NEXT_STATE_GET;
            break;

        // Twilio Messages api
        //     https://www.twilio.com/docs/sms/api/message-resource
        case TWILIO_NOTIFY_MESSAGE[0]:
            // Build dynamic URL and set pointer to config_client->url;
            r->url = ad2_string_printf(TWILIO_URL_FMT, r->sid.c_str(), "Messages.json");
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
            ESP_LOGW(TAG, "Unknown message type '%c' aborting adding to sendQ.", r->type[0]);
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
#if defined(DEBUG_TWILIO)
            ESP_LOGI(TAG,"Adding HTTP request to ad2_add_http_sendQ");
#endif
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
    TWILIO_SWITCH_SUBCMD_ID
};
char * TWILIO_SUBCMDS [] = {
    (char*)TWILIO_SID_SUBCMD,
    (char*)TWILIO_TOKEN_SUBCMD,
    (char*)TWILIO_FROM_SUBCMD,
    (char*)TWILIO_TO_SUBCMD,
    (char*)TWILIO_TYPE_SUBCMD,
    (char*)TWILIO_SWITCH_SUBCMD,
    0 // EOF
};

/**
 * Twilio generic command event processing
 *  command: [COMMAND] [SUB] <id> <arg>
 * ex.
 *   [COMMAND] [SUB] 0 arg...
 */
static void _cli_cmd_twilio_event_generic(std::string &subcmd, char *string)
{
    int slot = -1;
    std::string buf;

    // get the sub command value validation
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    // build the NV storage key concat prefix to sub command. Take human readable and make it less so.
    std::string key = std::string(TWILIO_CFG_PREFIX) + subcmd.c_str();
    if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
        slot = strtol(buf.c_str(), NULL, 10);
    }
    if (slot >= 0) {
        if (ad2_copy_nth_arg(buf, string, 3, true) >= 0) {
            ad2_remove_ws(buf);
            ad2_set_nv_slot_key_string(key.c_str(), slot, nullptr, buf.c_str());
            ad2_printf_host(false, "Setting '%s' value '%s' finished.\r\n", subcmd.c_str(), buf.c_str());
        } else {
            buf = "";
            ad2_get_nv_slot_key_string(key.c_str(), slot, nullptr, buf);
            ad2_printf_host(false, "Current slot #%02i '%s' value '%s'\r\n", slot, subcmd.c_str(), buf.length() ? buf.c_str() : "EMPTY");
        }
    } else {
        ad2_printf_host(false, "Missing <slot>\r\n");
    }
}

/**
 * Twilio smart alert switch command event processing
 *  command: [COMMAND] <id> <setting> <arg1> <arg2>
 * ex. Switch #0 [N]otification slot 0
 *   [COMMAND] 0 N 0
 */
static void _cli_cmd_twilio_smart_alert_switch(std::string &subcmd, char *instring)
{
    int i;
    int slot = -1;
    std::string buf;
    std::string arg1;
    std::string arg2;
    std::string tmpsz;

    // get the sub command value validation
    std::string key = std::string(TWILIO_CFG_PREFIX) + subcmd.c_str();

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
                ad2_set_nv_slot_key_string(key.c_str(), slot, SK_NOTIFY_SLOT, NULL);
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
            case SK_NOTIFY_SLOT[0]: // Notification slot
                // consume the arge and to EOL
                ad2_copy_nth_arg(arg2, instring, 4, true);
                ad2_trim(arg2);
                tmpsz = arg2.length() == 0 ? "Clearing" : "Setting";
                ad2_set_nv_slot_key_string(key.c_str(), slot, sk.c_str(), arg2.length() ? arg2.c_str() : nullptr);
                ad2_printf_host(false, "%s smartswitch #%i to use notification settings from slots #%s.\r\n", tmpsz.c_str(), slot, arg2.c_str());
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
            std::string args = SK_NOTIFY_SLOT
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

            ad2_printf_host(false, "Twilio SmartSwitch #%i report\r\n", slot);
            // sub key suffix.
            std::string sk;
            for(char& c : args) {
                std::string sk = ad2_string_printf("%c", c);
                switch(c) {
                case SK_NOTIFY_SLOT[0]:
                    i = 0; // Default 0
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host(false, "# Set notification slots [%c] to #%s.\r\n", c, out.c_str());
                    ad2_printf_host(false, "%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
                    break;
                case SK_DEFAULT_STATE[0]:
                    i = 0; // Default CLOSED
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host(false, "# Set default virtual switch state [%c] to '%s'(%i)\r\n", c, AD2Parse.state_str[i].c_str(), i);
                    ad2_printf_host(false, "%s %s %i %c %i\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i);
                    break;
                case SK_AUTO_RESET[0]:
                    i = 0; // Defaut 0 or disabled
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host(false, "# Set auto reset time in ms [%c] to '%s'\r\n", c, (i > 0) ? ad2_to_string(i).c_str() : "DISABLED");
                    ad2_printf_host(false, "%s %s %i %c %i\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i);
                    break;
                case SK_TYPE_LIST[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host(false, "# Set message type list [%c]\r\n", c);
                    if (out.length()) {
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
                    } else {
                        ad2_printf_host(false, "# disabled\r\n");
                    }
                    break;
                case SK_PREFILTER_REGEX[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host(false, "# Set pre filter REGEX [%c]\r\n", c);
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
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
                            ad2_printf_host(false, "%s %s %i %c %i %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i, out.c_str());
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
                        ad2_printf_host(false, "%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
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
 * Twilio command router.
 */
static void _cli_cmd_twilio_command_router(char *string)
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
            case TWILIO_TYPE_SUBCMD_ID:  // 'to' sub command
            case TWILIO_FROM_SUBCMD_ID:  // 'type' sub command
            case TWILIO_TO_SUBCMD_ID:  // 'from' sub command
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
 * @brief command list for module
 */
static struct cli_command twilio_cmd_list[] = {
    {
        (char*)TWILIO_COMMAND,(char*)
        "#### Configuration for Twilio notifications\r\n"
        "- Sets the 'SID' for a given notification slot. Multiple slots allow for multiple twilio accounts.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_SID_SUBCMD " {slot} {hash}```\r\n"
        "    - {slot}: [N]\r\n"
        "      - For default use 0.\r\n"
        "    - {hash}: Twilio ```Account SID```.\r\n"
        "  - Example: ```" TWILIO_COMMAND " " TWILIO_SID_SUBCMD " 0 aabbccdd112233..```\r\n"
        "- Sets the ```Auth Token``` for a given notification slot.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_TOKEN_SUBCMD " {slot} {hash}```\r\n"
        "    - {slot}: [N]\r\n"
        "      - For default use 0.\r\n"
        "    - {hash}: Twilio ```Auth Token```.\r\n"
        "    -  Example: ```" TWILIO_COMMAND " " TWILIO_TOKEN_SUBCMD " 0 aabbccdd112233..```\r\n"
        "- Sets the 'From' info for a given notification slot.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_FROM_SUBCMD " {slot} {phone|email}```\r\n"
        "    - {phone|email}: [NXXXYYYZZZZ|user@example.com]\r\n"
        "- Sets the 'To' info for a given notification slot.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_TO_SUBCMD " {slot} {phone|email}```\r\n"
        "    - {phone|email}: [NXXXYYYZZZZ|user@example.com, user2@example.com]\r\n"
        "- Sets the 'Type' for a given notification slot.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_TYPE_SUBCMD " {slot} {type}```\r\n"
        "    - {type}: [M|C|E]\r\n"
        "      - Notification type [M]essage, [C]all, [E]mail.\r\n"
        "  - Example: ```" TWILIO_COMMAND " " TWILIO_TYPE_SUBCMD " 0 M```\r\n"
        "- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.\r\n"
        "  - ```" TWILIO_COMMAND " " TWILIO_SWITCH_SUBCMD " {slot} {setting} {arg1} [arg2]```\r\n"
        "    - {slot}\r\n"
        "      - 1-99 : Supports multiple virtual smart alert switches.\r\n"
        "    - {setting}\r\n"
        "      - [-] Delete switch\r\n"
        "      - [N] Notification slots\r\n"
        "        -  Comma seperated list of notification slots to use for sending this events.\r\n"
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
        "      - [F] Trouble state regex search string list management.\r\n"
        "        - {arg1}: Index # 1-8\r\n"
        "        - {arg2}: Regex string for this slot or empty  string to clear\r\n"
        "      - [o] Open output format string.\r\n"
        "      - [c] Close output format string.\r\n"
        "      - [f] Trouble output format string.\r\n\r\n", _cli_cmd_twilio_command_router
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
 * Initialize queue and SSL
 */
void twilio_init()
{

    // Register search based virtual switches with the AlarmDecoderParser.
    std::string key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_SWITCH_SUBCMD);
    int subscribers = 0;
    for (int i = 1; i < 99; i++) {
        std::string slots;
        ad2_get_nv_slot_key_string(key.c_str(), i, SK_NOTIFY_SLOT, slots);
        if (slots.length()) {
            AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);

            // read notification slot list into std::list
            std::list<uint8_t> *pslots = new std::list<uint8_t>;
            std::vector<std::string> vres;
            ad2_tokenize(slots, ",", vres);
            for (auto &slotstring : vres) {
                uint8_t s = std::atoi(slotstring.c_str());
                pslots->push_front((uint8_t)s & 0xff);
            }

            // Save the notification slot. That is all we need to complete the task.
            es1->INT_ARG = 0;
            es1->PTR_ARG = pslots;

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
                twilio_AD2EventSearches.push_back(es1);

                // subscribe the AD2EventSearch virtual switch callback.
                AD2Parse.subscribeTo(on_search_match_cb_tw, es1);

                // keep track of how many for user feedback.
                subscribers++;
            }
        }
    }

    ad2_printf_host(true, "%s: Init done. Found and configured %i virtual switches.", TAG, subscribers);

}

/**
 * cleanup memory
 */
void twilio_free()
{
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_AD2IOT_TWILIO_CLIENT */

