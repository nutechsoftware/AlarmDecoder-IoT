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
#include "cJSON.h"

static const char *TAG = "TWILIO";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"

// Disable via sdkconfig
#if CONFIG_AD2IOT_TWILIO_CLIENT

// specific includes
#include "twilio.h"

// mbedtls
#include "mbedtls/base64.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
#include "mbedtls/ssl_cache.h"
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
#include "esp_crt_bundle.h"
#endif

QueueHandle_t  sendQ_tw=NULL;
mbedtls_entropy_context twilio_entropy;
mbedtls_ctr_drbg_context twilio_ctr_drbg;
mbedtls_ssl_config twilio_conf;
mbedtls_ssl_context twilio_ssl;
mbedtls_x509_crt api_twilio_com_cacert;
mbedtls_ssl_config sendgrid_conf;
mbedtls_ssl_context sendgrid_ssl;
mbedtls_x509_crt api_sendgrid_com_cacert;
mbedtls_net_context twilio_server_fd;
#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
mbedtls_ssl_cache_context cache;
#endif

std::vector<AD2EventSearch *> twilio_AD2EventSearches;


#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
/**
 * @brief Root cert for api.twilio.com and api.sendgrid.com
 *   The PEM file was extracted from the output of this command:
 *  openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null
 *  The CA root cert is the last cert given in the chain of certs.
 *  To embed it in the app binary, the PEM file is named
 *  in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */
extern const uint8_t twilio_root_pem_start[]   asm("_binary_twilio_root_pem_start");
extern const uint8_t twilio_root_pem_end[]     asm("_binary_twilio_root_pem_end");
extern const uint8_t sendgrid_root_pem_start[] asm("_binary_sendgrid_root_pem_start");
extern const uint8_t sendgrid_root_pem_end[]   asm("_binary_sendgrid_root_pem_end");
#endif

/**
 * build a request string for the twilio api.
 */
std::string build_twilio_request_string(std::string sid,
                                        std::string token, std::string body, char type)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    std::string apiPath;

    if (type == TWILIO_NOTIFY_CALL[0]) {
        apiPath = "Calls.json";
    }
    if (type == TWILIO_NOTIFY_MESSAGE[0]) {
        apiPath = "Messages.json";
    }

    std::string auth_header = ad2_make_basic_auth_header(sid, token);
    std::string http_request =
        "POST /" + std::string(TWILIO_API_VERSION) + "/Accounts/" + sid + "/" + apiPath + " HTTP/1.0\r\n" +
        "User-Agent: esp-idf/1.0 esp32(v" + ad2_to_string(chip_info.revision) + ")\r\n" +
        auth_header + "\r\n" +
        "Host: " + TWILIO_API_SERVER + "\r\n" +
        "Cache-control: no-cache\r\n" +
        "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n" +
        "Content-Length: " + ad2_to_string(body.length()) + "\r\n" +
        "Connection: close\r\n" +
        "\r\n" + body + "\r\n";
    return http_request;
}

/**
 * build a body string for the sendgrid api.()
 */
std::string build_sendgrid_body(std::string from, std::string to, std::string arg)
{
    // object: root
    cJSON *_root = cJSON_CreateObject();

    // array: personalizations
    cJSON *_personalizations = cJSON_CreateArray();
    cJSON *_pitem = cJSON_CreateObject();
    cJSON_AddItemToArray(_personalizations, _pitem);

    // object: from
    cJSON *_from = cJSON_CreateObject();
    cJSON_AddStringToObject(_from, "email", from.c_str());

    // array: content
    std::string message = arg;
    cJSON *_content = cJSON_CreateArray();
    cJSON *_text = cJSON_CreateObject();
    cJSON_AddItemToArray(_content, _text);
    cJSON_AddStringToObject(_text, "type", "text/plain");
    cJSON_AddStringToObject(_text, "value", message.c_str());

    // add top level items to root
    cJSON_AddItemToObject(_root, "personalizations", _personalizations);
    cJSON_AddItemToObject(_root, "from", _from);
    cJSON_AddItemToObject(_root, "content", _content);


    // _personalizations -> subject
    cJSON_AddStringToObject(_pitem, "subject", ad2_string_printf("AD2 ALERT '%s'", arg.c_str()).c_str());

    // _personalizations -> to[]
    cJSON *_to = cJSON_CreateArray();
    cJSON_AddItemToObject(_pitem, "to", _to);
    cJSON *_email = cJSON_CreateObject();
    cJSON_AddItemToArray(_to, _email);
    cJSON_AddStringToObject(_email, "email", to.c_str());


    char *json = NULL;
    json = cJSON_Print(_root);

    std::string ret = json;
    free (json);
    cJSON_Delete(_root);

    return ret;
}

/**
 * build_sendgrid_request_string()
 */
std::string build_sendgrid_request_string(std::string apikey, std::string body)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    std::string auth_header = ad2_make_bearer_auth_header(apikey);
    std::string http_request = "POST https://api.sendgrid.com/v3/mail/send HTTP/1.1\r\n";
    http_request +=
        "User-Agent: esp-idf/1.0 esp32(v" + ad2_to_string(chip_info.revision) + ")\r\n" +
        auth_header + "\r\n" +
        "Host: " + std::string(SENDGRID_API_SERVER) + "\r\n" +
        "Cache-control: no-cache\r\n" +
        "Content-Type: application/json; charset=utf-8\r\n" +
        "Content-Length: " + ad2_to_string(body.length()) + "\r\n" +
        "Connection: close\r\n" +
        "\r\n" + body + "\r\n";
    return http_request;
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * twilio_add_queue()
 */
void twilio_add_queue(std::string &sid, std::string &token, std::string &from, std::string &to, char type, std::string &arg)
{
    if (sendQ_tw) {
        twilio_message_data_t *message_data = NULL;
        message_data = (twilio_message_data_t *)malloc(sizeof(twilio_message_data_t));
        message_data->sid = strdup(sid.c_str());
        message_data->token = strdup(token.c_str());
        message_data->from =  strdup(from.c_str());
        message_data->to =  strdup(to.c_str());
        message_data->type = type;
        message_data->arg = strdup(arg.c_str());
        xQueueSend(sendQ_tw,(void *)&message_data,(TickType_t )0);
    } else {
        ESP_LOGE(TAG, "Invalid queue handle");
    }
}

/**
 * Background task to watch queue and synchronously send notifications.
 * Note:
 *   [X] Only runs one task at a time.
 *   [X] Delivery rate limited.
 *   [ ] Limit max tasks in queue. How many? High water local audible alert?.
 */
void twilio_consumer_task(void *pvParameter)
{
    ESP_LOGI(TAG,"queue consumer loop start");
    while(1) {
        if(sendQ_tw == NULL) {
            ESP_LOGW(TAG, "sendQ is not ready task ending");
            break;
        }
        twilio_message_data_t *message_data = NULL;
        if ( xQueueReceive(sendQ_tw,&message_data,portMAX_DELAY) ) {
            ESP_LOGI(TAG, "Calling twilio notification.");
            twilio_send_task((void *)message_data);
            ESP_LOGI(TAG, "Completed requests stack free %d", uxTaskGetStackHighWaterMark(NULL));
            vTaskDelay(TWILIO_RATE_LIMIT/portTICK_PERIOD_MS);
        } else {
            // sleep for a bit then check the queue again.
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }
    // Notification queue consumer task stopping.
    vTaskDelete(NULL);
}

/**
 * cleanup memory
 */
void twilio_free()
{
    mbedtls_ssl_free( &twilio_ssl );
    mbedtls_x509_crt_free( &api_twilio_com_cacert );
    mbedtls_ssl_config_free( &twilio_conf );

    mbedtls_ssl_free( &sendgrid_ssl );
    mbedtls_x509_crt_free( &api_sendgrid_com_cacert );
    mbedtls_ssl_config_free( &sendgrid_conf );

    mbedtls_ctr_drbg_free( &twilio_ctr_drbg);
    mbedtls_entropy_free( &twilio_entropy );
}

/**
 * @brief Background task to send a message to twilio.
 *
 * @param [in] pvParameters void * argment from task creation.
 */
void twilio_send_task(void *pvParameters)
{
    ESP_LOGI(TAG, "twilio send task start stack free %d", esp_get_free_heap_size());

    twilio_message_data_t *message_data = (twilio_message_data_t *)pvParameters;

    char buf[512];
    int ret, flags, len;
    size_t written_bytes = 0;
    std::string http_request;
    std::string body;
    char * reqp = NULL;
    mbedtls_ssl_context *_ssl = nullptr;

    // Response parsing
    std::string response;
    std::string respLine;
    std::vector<std::string> respLines;
    std::stringstream responseStream;


    // should never happen sanity check.
    if (message_data == NULL) {
        ESP_LOGE(TAG, "error null message_data aborting task.");
        return;
    }
    // load message data local for ease of access
    std::string sid = message_data->sid;
    std::string token = message_data->token;
    std::string to = message_data->to;
    std::string from = message_data->from;
    char type = message_data->type;
    std::string arg = message_data->arg;

    // free we are done with the pointers.
    free(message_data->sid);
    free(message_data->token);
    free(message_data->from);
    free(message_data->to);
    free(message_data->arg);
    free(message_data);

    /* Build the HTTPS POST request. */
    const char * _api_server_port = nullptr;
    const char * _api_server_address = nullptr;

    switch(type) {
    // Twilio Messages api
    case TWILIO_NOTIFY_MESSAGE[0]:
        _api_server_address = TWILIO_API_SERVER;
        _api_server_port = TWILIO_API_PORT;
        body = "To=" + ad2_urlencode(to) + "&From=" + ad2_urlencode(from) + \
               "&Body=" + ad2_urlencode(arg);
        // build request string including basic auth headers
        http_request = build_twilio_request_string(sid, token, body, TWILIO_NOTIFY_MESSAGE[0]);
        _ssl = &twilio_ssl;
        break;

    // Twilio Call api
    case TWILIO_NOTIFY_CALL[0]:
        _api_server_address = TWILIO_API_SERVER;
        _api_server_port = TWILIO_API_PORT;
        body = "To=" + ad2_urlencode(to) + "&From=" + ad2_urlencode(from) + \
               "&Twiml=" + ad2_urlencode(arg);
        // build request string including basic auth headers
        http_request = build_twilio_request_string(sid, token, body, TWILIO_NOTIFY_CALL[0]);
        _ssl = &twilio_ssl;
        break;

    // SendGrid Email api.
    case TWILIO_NOTIFY_EMAIL[0]:
        _api_server_address = SENDGRID_API_SERVER;
        _api_server_port = SENDGRID_API_PORT;

        body = build_sendgrid_body(from, to, arg);

        // build request string including basic auth headers
        http_request = build_sendgrid_request_string(token, body);
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "SENDING '%s'", http_request.c_str());
#endif
        _ssl = &sendgrid_ssl;
        break;

    default:
        ESP_LOGW(TAG, "Unknown message type '%c' aborting task.", type);
        return;
    }

    ESP_LOGI(TAG, "Connecting to %s:%s...", _api_server_address, _api_server_port);

    // clean up and ready for new request
    mbedtls_ssl_session_reset(_ssl);
    mbedtls_net_free(&twilio_server_fd);

    if ((ret = mbedtls_net_connect(&twilio_server_fd, _api_server_address,
                                   _api_server_port, MBEDTLS_NET_PROTO_TCP)) != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
        goto exit;
    }

    ESP_LOGI(TAG, "Connected.");

    mbedtls_ssl_set_bio(_ssl, &twilio_server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
            goto exit;
        }
    }

    ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

    if ((flags = mbedtls_ssl_get_verify_result(_ssl)) != 0) {
        /* In real life, we probably want to close connection if ret != 0 */
        ESP_LOGW(TAG, "Failed to verify peer certificate!");
        bzero(buf, sizeof(buf));
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
        ESP_LOGW(TAG, "verification info: %s", buf);
    } else {
        ESP_LOGI(TAG, "Certificate verified.");
    }

    ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(_ssl));

    reqp = (char *)http_request.c_str();
#if defined(DEBUG_TWILIO)
    ESP_LOGI(TAG, "sending message to twilio\r\n%s\r\n",http_request.c_str());
#endif


    /* Send HTTPS POST request. */
    ESP_LOGI(TAG, "Writing HTTP request...");
    do {
        ret = mbedtls_ssl_write(_ssl,
                                (const unsigned char *)reqp + written_bytes,
                                strlen(reqp) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_WANT_READ) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
            goto exit;
        }
    } while(written_bytes < strlen(reqp));

    ESP_LOGI(TAG, "Reading HTTP response...");

    do {
        len = sizeof(buf) - 1;
        bzero(buf, sizeof(buf));
        ret = mbedtls_ssl_read(_ssl, (unsigned char *)buf, len);

        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            ret = 0;
            break;
        }

        if(ret < 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
            break;
        }

        if(ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        // Build response buffer. If over N bytes reject and finish.
        if (response.length()+ret > 1024 * 4) {
            ESP_LOGI(TAG, "Response buffer full %i.", response.length());
            break;
        }
        response.append(buf, ret);

        // TODO: parse response error logging for easier debugging.
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "HTTPRX: %s", buf);
#endif
        ESP_LOGD(TAG, "%d bytes read", ret);
    } while(1);

    mbedtls_ssl_close_notify(_ssl);

    // Simple HTTP response parsing.
    responseStream.str(response);
    while(std::getline(responseStream >> std::ws, respLine, '\r')) {
#if defined(DEBUG_TWILIO)
        ESP_LOGI(TAG, "Response: '%s'", respLine.c_str());
#endif
        respLines.push_back(respLine);
    }

    // First line is response code.
    if (respLines.size()) {
        if (respLines[0].find("HTTP/1.1 20") == 0) {
            ESP_LOGI(TAG, "Response OK");
        } else {
            ESP_LOGI(TAG, "Response error code: '%s'", respLines[0].c_str());
            ESP_LOGI(TAG, "Response error body: '%s'", respLines[respLines.size()-1].c_str());
        }
    } else {
        ESP_LOGI(TAG, "No response");
    }

exit:

    if(ret != 0) {
        mbedtls_strerror(ret, buf, 100);
        ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
    }
}

/**
 * @brief Build and queue a twilio request.
 *
 * @param [in]msg raw panel message that caused event.
 * @param [in]s virtual partition state.
 * @param [in]arg string pointer event class string.
 *
 */
void ad2_event_cb_twilio(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if (!s || (defs && s->partition == defs->partition)) {
        ESP_LOGI(TAG, "twilio ad2_event_cb_twilio '%i'", (int)arg);

        std::string body;
        if (AD2Parse.event_str.find((int)arg) == AD2Parse.event_str.end()) {
            body = ad2_string_printf("EVENT ID %d",(int)arg);
        } else {
            body = AD2Parse.event_str[(int)arg];
        }

        if (g_ad2_network_state == AD2_CONNECTED) {
            // load our settings for this event type.
            std::string key;

            std::string sid;
            key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_SID_SUBCMD);
            ad2_get_nv_slot_key_string(key.c_str(), AD2_DEFAULT_TWILIO_SLOT, nullptr, sid);

            std::string token;
            key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TOKEN_SUBCMD);
            ad2_get_nv_slot_key_string(key.c_str(), AD2_DEFAULT_TWILIO_SLOT, nullptr, token);

            std::string from;
            key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_FROM_SUBCMD);
            ad2_get_nv_slot_key_string(key.c_str(), AD2_DEFAULT_TWILIO_SLOT, nullptr, from);

            std::string to;
            key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TO_SUBCMD);
            ad2_get_nv_slot_key_string(key.c_str(), AD2_DEFAULT_TWILIO_SLOT, nullptr, to);

            std::string type;
            key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TYPE_SUBCMD);
            ad2_get_nv_slot_key_string(key.c_str(), AD2_DEFAULT_TWILIO_SLOT, nullptr, type);
            type.resize(1);

            // sanity check see if type(char) is in our list of types
            if (std::string(TWILIO_NOTIFY_MESSAGE TWILIO_NOTIFY_CALL TWILIO_NOTIFY_EMAIL).find(type) != string::npos) {
                // add to the queue
                ESP_LOGI(TAG, "Adding task to twilio send queue");
                twilio_add_queue(sid, token, from, to, type[0], body);
            } else {
                ESP_LOGI(TAG, "Unknown or missing type '%s' check settings. Not adding to delivery queue.", type.c_str());
            }
        }
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
void on_search_match_cb_tw(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    AD2EventSearch *es = (AD2EventSearch *)arg;
    ESP_LOGI(TAG, "ON_SEARCH_MATCH_CB: '%s' -> '%s' notify slot #%02i", msg->c_str(), es->out_message.c_str(), es->INT_ARG);
    if (g_ad2_network_state == AD2_CONNECTED) {
        std::string body = es->out_message;
        // load our settings for this event type.
        std::string key;

        std::string sid;
        key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_SID_SUBCMD);
        ad2_get_nv_slot_key_string(key.c_str(), es->INT_ARG, nullptr, sid);

        std::string token;
        key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TOKEN_SUBCMD);
        ad2_get_nv_slot_key_string(key.c_str(), es->INT_ARG, nullptr, token);

        std::string from;
        key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_FROM_SUBCMD);
        ad2_get_nv_slot_key_string(key.c_str(), es->INT_ARG, nullptr, from);

        std::string to;
        key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TO_SUBCMD);
        ad2_get_nv_slot_key_string(key.c_str(), es->INT_ARG, nullptr, to);

        std::string type;
        key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_TYPE_SUBCMD);
        ad2_get_nv_slot_key_string(key.c_str(), es->INT_ARG, nullptr, type);
        type.resize(1);

        // sanity check see if type(char) is in our list of types
        if (std::string(TWILIO_NOTIFY_MESSAGE TWILIO_NOTIFY_CALL TWILIO_NOTIFY_EMAIL).find(type) != string::npos) {
            // add to the queue
            ESP_LOGI(TAG, "Adding task to twilio send queue");
            twilio_add_queue(sid, token, from, to, type[0], body);
        } else {
            ESP_LOGI(TAG, "Unknown or missing type '%s' check settings. Not adding to delivery queue.", type.c_str());
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

            ad2_printf_host("Twilio SmartSwitch #%i report\r\n", slot);
            // sub key suffix.
            std::string sk;
            for(char& c : args) {
                std::string sk = ad2_string_printf("%c", c);
                switch(c) {
                case SK_NOTIFY_SLOT[0]:
                    i = 0; // Default 0
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set notification slot [%c] to #%i.\r\n", c, i);
                    ad2_printf_host("%s %s %i %c %i\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i);
                    break;
                case SK_DEFAULT_STATE[0]:
                    i = 0; // Default CLOSED
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set default virtual switch state [%c] to '%s'(%i)\r\n", c, AD2Parse.state_str[i].c_str(), i);
                    ad2_printf_host("%s %s %i %c %i\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i);
                    break;
                case SK_AUTO_RESET[0]:
                    i = 0; // Defaut 0 or disabled
                    ad2_get_nv_slot_key_int(key.c_str(), slot, sk.c_str(), &i);
                    ad2_printf_host("# Set auto reset time in ms [%c] to '%s'\r\n", c, (i > 0) ? ad2_to_string(i).c_str() : "DISABLED");
                    ad2_printf_host("%s %s %i %c %i\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i);
                    break;
                case SK_TYPE_LIST[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    ad2_printf_host("# Set message type list [%c]\r\n", c);
                    if (out.length()) {
                        ad2_printf_host("%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
                    } else {
                        ad2_printf_host("# disabled\r\n");
                    }
                    break;
                case SK_PREFILTER_REGEX[0]:
                    out = "";
                    ad2_get_nv_slot_key_string(key.c_str(), slot, sk.c_str(), out);
                    if (out.length()) {
                        ad2_printf_host("# Set pre filter REGEX [%c]\r\n", c);
                        ad2_printf_host("%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
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
                            ad2_printf_host("%s %s %i %c %i %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, i, out.c_str());
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
                        ad2_printf_host("%s %s %i %c %s\r\n", TWILIO_COMMAND, TWILIO_SWITCH_SUBCMD, slot, c, out.c_str());
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
            ad2_printf_host("What?\r\n");
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
        "### Configuration for Twilio message notifications.\r\n\r\n"
        "  - Sets the 'SID' for a given notification slot. Multiple slots allow for multiple twilio accounts.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_SID_SUBCMD " {slot} {hash}```\r\n\r\n"
        "    - {slot}: [N]\r\n"
        "      - For default use 0.\r\n"
        "    - {hash}: Twilio 'Account SID'.\r\n\r\n"
        "    Example: " TWILIO_COMMAND " " TWILIO_SID_SUBCMD " 0 aabbccdd112233..\r\n\r\n"
        "  - Sets the 'Auth Token' for a given notification slot.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_TOKEN_SUBCMD " {slot} {hash}```\r\n\r\n"
        "    - {slot}: [N]\r\n"
        "      - For default use 0.\r\n\r\n"
        "    - {hash}: Twilio 'Auth Token'.\r\n\r\n"
        "    Example: " TWILIO_COMMAND " " TWILIO_TOKEN_SUBCMD " 0 aabbccdd112233..\r\n\r\n"
        "  - Sets the 'From' info for a given notification slot.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_FROM_SUBCMD " {slot} {phone|email}```\r\n\r\n"
        "    - {phone|email}: [NXXXYYYZZZZ|user@example.com]\r\n\r\n"
        "  - Sets the 'To' info for a given notification slot.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_TO_SUBCMD " {slot} {phone|email}```\r\n\r\n"
        "    - {phone|email}: [NXXXYYYZZZZ|user@example.com]\r\n\r\n"
        "  - Sets the 'Type' for a given notification slot.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_TYPE_SUBCMD " {slot} {type}```\r\n\r\n"
        "    - {type}: [M|C|E]\r\n"
        "      - Notification type [M]essage, [C]all, [E]mail.\r\n"
        "    Example: " TWILIO_COMMAND " " TWILIO_TYPE_SUBCMD " 0 M\r\n\r\n"
        "  - Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.\r\n\r\n"
        "    ```" TWILIO_COMMAND " " TWILIO_SWITCH_SUBCMD " {slot} {setting} {arg1} [arg2]```\r\n\r\n"
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
        "      - [f] Fault output format string.\r\n\r\n", _cli_cmd_twilio_command_router
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
    int res = 0;

#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
    mbedtls_ssl_cache_init( &cache );
#endif
    // init server_fd
    mbedtls_net_init(&twilio_server_fd);

    // init api.twilio.com ssl connection
    mbedtls_ssl_init(&twilio_ssl);
    // init api.twilio.com SSL conf
    mbedtls_ssl_config_init(&twilio_conf);
    // init cert for api.twilio.com
    mbedtls_x509_crt_init(&api_twilio_com_cacert);

    // init api.sendgrid.com ssl connection
    mbedtls_ssl_init(&sendgrid_ssl);
    // init api.sendgrid.com SSL conf
    mbedtls_ssl_config_init(&sendgrid_conf);
    // init cert for api.sendgrid.com
    mbedtls_x509_crt_init(&api_sendgrid_com_cacert);

    // init entropy
    mbedtls_entropy_init(&twilio_entropy);

    // init drbg
    mbedtls_ctr_drbg_init(&twilio_ctr_drbg);
#if defined(DEBUG_TWILIO_TLS)
    mbedtls_debug_set_threshold( DEBUG_LEVEL );
#endif

    ESP_LOGI(TAG, "Seeding the random number generator");
    if((res = mbedtls_ctr_drbg_seed(&twilio_ctr_drbg, mbedtls_entropy_func, &twilio_entropy,
                                    NULL, 0)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", res);
        twilio_free();
        return;
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
    /**
     * api.twilio.com SSL setup
     */
    ESP_LOGI(TAG, "Loading public api.twilio.com CA root certificate...");
    res = mbedtls_x509_crt_parse(&api_twilio_com_cacert, twilio_root_pem_start,
                                 twilio_root_pem_end-twilio_root_pem_start);
    if(res < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }
#else
    res = esp_crt_bundle_attach(&twilio_conf);

    if(res < 0) {
        ESP_LOGE(TAG, "esp_crt_bundle_attach for api.twilio.com returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }
#endif
    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.
       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&twilio_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&twilio_conf, &api_twilio_com_cacert, NULL);
    mbedtls_ssl_conf_rng(&twilio_conf, mbedtls_ctr_drbg_random, &twilio_ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&twilio_conf, 4);
#endif

#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
    mbedtls_ssl_conf_session_cache( &twilio_conf, &cache,
                                    mbedtls_ssl_cache_get,
                                    mbedtls_ssl_cache_set );
#endif

    ESP_LOGI(TAG, "Setting api.twilio.com hostname for TLS session...");
    /* Hostname set here should match CN in server certificate */
    if((res = mbedtls_ssl_set_hostname(&twilio_ssl, TWILIO_API_SERVER)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Setting up the api.twilio.com SSL/TLS structure...");
    if((res = mbedtls_ssl_config_defaults(&twilio_conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", res);
        twilio_free();
        return;
    }

    if ((res = mbedtls_ssl_setup(&twilio_ssl, &twilio_conf)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
    /**
     * api.sendgrid.com SSL setup
     */
    ESP_LOGI(TAG, "Loading public api.sendgrid.com CA root certificate...");
    res = mbedtls_x509_crt_parse(&api_sendgrid_com_cacert, sendgrid_root_pem_start,
                                 sendgrid_root_pem_end-sendgrid_root_pem_start);
    if(res < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }
#else
    res = esp_crt_bundle_attach(&sendgrid_conf);

    if(res < 0) {
        ESP_LOGE(TAG, "esp_crt_bundle_attach for api.sendgrid.com returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }
#endif

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.
       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&sendgrid_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&sendgrid_conf, &api_sendgrid_com_cacert, NULL);
    mbedtls_ssl_conf_rng(&sendgrid_conf, mbedtls_ctr_drbg_random, &twilio_ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&sendgrid_conf, 4);
#endif

#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
    mbedtls_ssl_conf_session_cache( &sendgrid_conf, &cache,
                                    mbedtls_ssl_cache_get,
                                    mbedtls_ssl_cache_set );
#endif

    ESP_LOGI(TAG, "Setting api.sendgrid.com hostname for TLS session...");
    /* Hostname set here should match CN in server certificate */
    if((res = mbedtls_ssl_set_hostname(&sendgrid_ssl, SENDGRID_API_SERVER)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Setting up the api.sendgrid.com SSL/TLS structure...");
    if((res = mbedtls_ssl_config_defaults(&sendgrid_conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", res);
        twilio_free();
        return;
    }

    if ((res = mbedtls_ssl_setup(&sendgrid_ssl, &sendgrid_conf)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Starting twilio queue consumer task");
    sendQ_tw = xQueueCreate(TWILIO_QUEUE_SIZE,sizeof(struct twilio_message_data *));
    if( sendQ_tw == 0 ) {
        ESP_LOGE(TAG, "Failed to create twilio queue.");
    } else {
        xTaskCreate(&twilio_consumer_task, "twilio_consumer_task", 1024*6, NULL, tskIDLE_PRIORITY+1, NULL);
    }

    // TODO configure to all selection on events to notify on.

    // Register callbacks for a few static events for testing.
    //AD2Parse.subscribeTo(ON_LRR, ad2_event_cb_twilio, (void*)ON_LRR);
    //AD2Parse.subscribeTo(ON_ARM, ad2_event_cb_twilio, (void*)ON_ARM);
    //AD2Parse.subscribeTo(ON_DISARM, ad2_event_cb_twilio, (void*)ON_DISARM);
    //AD2Parse.subscribeTo(ON_FIRE, ad2_event_cb_twilio, (void*)ON_FIRE);
    //AD2Parse.subscribeTo(ON_ALARM_CHANGE, ad2_event_cb_twilio, (void*)ON_ALARM_CHANGE);

    // Register search based virtual switches.
    std::string key = std::string(TWILIO_CFG_PREFIX) + std::string(TWILIO_SWITCH_SUBCMD);
    int subscribers = 0;
    for (int i = 1; i < 99; i++) {
        int notification_slot = -1;
        ad2_get_nv_slot_key_int(key.c_str(), i, SK_NOTIFY_SLOT, &notification_slot);
        if (notification_slot >= 0) {
            AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);

            // Save the notification slot. That is all we need to complete the task.
            es1->INT_ARG = notification_slot;
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
                twilio_AD2EventSearches.push_back(es1);

                // subscribe to the callback for events.
                AD2Parse.subscribeTo(on_search_match_cb_tw, es1);

                // keep track of how many for user feedback.
                subscribers++;
            }
        }
    }

    ESP_LOGI(TAG, "Found and configured %i virtual switches.", subscribers);

#if 0 /* TESTING FIXME */
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, ad2_event_cb_twilio, (void*)ON_CHIME_CHANGE);
#endif
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_AD2IOT_TWILIO_CLIENT */

