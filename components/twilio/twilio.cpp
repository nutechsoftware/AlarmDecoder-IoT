/**
*  @file    twilio.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    09/12/2020
*  @version 1.0.3
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
static const char *TAG = "TWILIO";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"

// Disable via sdkconfig
#if CONFIG_TWILIO_CLIENT

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

QueueHandle_t  sendQ=NULL;
mbedtls_ssl_config conf;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_x509_crt api_twilio_com_cacert;
mbedtls_x509_crt api_sendgrid_com_cacert;
mbedtls_net_context server_fd;
#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
mbedtls_ssl_cache_context cache;
#endif

std::vector<AD2EventSearch *> AD2EventSearches;


/**
 * @brief Root cert for api.twilio.com and api.sendgrid.com
 *   The PEM file was extracted from the output of this command:
 *  openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null
 *  The CA root cert is the last cert given in the chain of certs.
 *  To embed it in the app binary, the PEM file is named
 *  in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */
extern const uint8_t twilio_root_pem_start[] asm("_binary_twilio_root_pem_start");
extern const uint8_t twilio_root_pem_end[]   asm("_binary_twilio_root_pem_end");
extern const uint8_t sendgrid_root_pem_start[] asm("_binary_sendgrid_root_pem_start");
extern const uint8_t sendgrid_root_pem_end[]   asm("_binary_sendgrid_root_pem_end");

/**
 * url_encode
 */
std::string urlencode(std::string str)
{
    std::string encoded = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str[i];
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

/**
 * build auth string from user and pass
 */
std::string get_auth_header(const std::string& user, const std::string& password)
{

    size_t toencodeLen = user.length() + password.length() + 2;
    size_t out_len = 0;
    char toencode[toencodeLen];
    unsigned char outbuffer[(toencodeLen + 2 - ((toencodeLen + 2) % 3)) / 3 * 4 + 1];

    memset(toencode, 0, toencodeLen);

    snprintf(
        toencode,
        toencodeLen,
        "%s:%s",
        user.c_str(),
        password.c_str()
    );

    mbedtls_base64_encode(outbuffer,sizeof(outbuffer),&out_len,(unsigned char*)toencode, toencodeLen-1);
    outbuffer[out_len] = '\0';

    std::string encoded_string = std::string((char *)outbuffer);
    return "Authorization: Basic " + encoded_string;
}

/**
 * build_request_string()
 */
std::string build_request_string(std::string sid,
                                 std::string token, std::string body)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    std::string auth_header = get_auth_header(sid, token);
    std::string http_request =
        "POST /" + std::string(API_VERSION) + "/Accounts/" + sid + "/Messages HTTP/1.0\r\n" +
        "User-Agent: esp-idf/1.0 esp32(v" + ad2_to_string(chip_info.revision) + ")\r\n" +
        auth_header + "\r\n" +
        "Host: " + WEB_SERVER + "\r\n" +
        "Cache-control: no-cache\r\n" +
        "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n" +
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
    if (sendQ) {
        twilio_message_data_t *message_data = NULL;
        message_data = (twilio_message_data_t *)malloc(sizeof(twilio_message_data_t));
        message_data->sid = strdup(sid.c_str());
        message_data->token = strdup(token.c_str());
        message_data->from =  strdup(from.c_str());
        message_data->to =  strdup(to.c_str());
        message_data->type = type;
        message_data->arg = strdup(arg.c_str());
        xQueueSend(sendQ,(void *)&message_data,(TickType_t )0);
    } else {
        ESP_LOGE(TAG, "Invalid queue handle");
    }
}

/**
 * Background task to watch queue and synchronously spawn tasks.
 * Note:
 *   1) Only runs one task at a time.
 *   2) Monitor/limit max tasks in queue.
 */
void twilio_consumer_task(void *pvParameter)
{
    while(1) {
        ESP_LOGI(TAG,"queue consumer loop start");
        if(sendQ == NULL) {
            ESP_LOGW(TAG, "sendQ is not ready task ending");
            break;
        }
        twilio_message_data_t *message_data = NULL;
        if ( xQueueReceive(sendQ,&message_data,portMAX_DELAY) ) {
            ESP_LOGI(TAG, "creating task for twilio notification");
            // process the message
            BaseType_t xReturned = xTaskCreate(&twilio_send_task, "twilio_send_task", 1024*5, (void *)message_data, tskIDLE_PRIORITY+5, NULL);
            if (xReturned != pdPASS) {
                ESP_LOGE(TAG, "failed to create twilio task.");
            }
            // Sleep after spawingin a task.
            vTaskDelay(TWILIO_RATE_LIMIT/portTICK_PERIOD_MS); //wait for 500 ms
        }
    }
    vTaskDelete(NULL);
}

/**
 * cleanup memory
 */
void twilio_free()
{
    mbedtls_ssl_free( &ssl );
    mbedtls_x509_crt_free( &api_twilio_com_cacert );
    mbedtls_x509_crt_free( &api_sendgrid_com_cacert );
    mbedtls_ctr_drbg_free( &ctr_drbg);
    mbedtls_ssl_config_free( &conf );
    mbedtls_entropy_free( &entropy );
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


    // should never happen sanity check.
    if (message_data == NULL) {
        ESP_LOGE(TAG, "error null message_data aborting task.");
        vTaskDelete(NULL);
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
    switch(type) {
    case 'M': // Messages
        body = "To=" + urlencode(to) + "&From=" + urlencode(from) + \
               "&Body=" + urlencode(arg);
        break;
    case 'R': // Redirect
        break;
    case 'T': // Twiml URL
        body = "To=" + urlencode(to) + "&From=" + urlencode(from) + \
               "&Url=" + urlencode(arg);
        break;
    default:
        ESP_LOGW(TAG, "Unknown message type '%c' aborting task.", type);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

    // clean up and ready for new request
    mbedtls_ssl_session_reset(&ssl);
    mbedtls_net_free(&server_fd);

    if ((ret = mbedtls_net_connect(&server_fd, WEB_SERVER,
                                   WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
        goto exit;
    }

    ESP_LOGI(TAG, "Connected.");

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
            goto exit;
        }
    }

    ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

    if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
        /* In real life, we probably want to close connection if ret != 0 */
        ESP_LOGW(TAG, "Failed to verify peer certificate!");
        bzero(buf, sizeof(buf));
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
        ESP_LOGW(TAG, "verification info: %s", buf);
    } else {
        ESP_LOGI(TAG, "Certificate verified.");
    }

    ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));

    // build request string including basic auth headers
    http_request = build_request_string(sid, token, body);
    reqp = (char *)http_request.c_str();
#if defined(TWILIO_DEBUG)
    ad2_printf_host("sending message to twilio\r\n%s\r\n",http_request.c_str());
#endif
    /* Send HTTPS POST request. */
    ESP_LOGI(TAG, "Writing HTTP request...");
    do {
        ret = mbedtls_ssl_write(&ssl,
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
        ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);

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
        // TODO: parse response error logging for easier debugging.

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);
    } while(1);

    mbedtls_ssl_close_notify(&ssl);

exit:

    if(ret != 0) {
        mbedtls_strerror(ret, buf, 100);
        ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
    }
    ESP_LOGI(TAG, "Completed requests stack free %d", uxTaskGetStackHighWaterMark(NULL));

    vTaskDelete(NULL);
}

char * TWILIO_SETTINGS [] = {
    (char*)TWILIO_SID_CFGKEY,
    (char*)TWILIO_TOKEN_CFGKEY,
    (char*)TWILIO_TYPE_CFGKEY,
    (char*)TWILIO_TO_CFGKEY,
    (char*)TWILIO_FROM_CFGKEY,
    (char*)TWILIO_BODY_CFGKEY,
    0 // EOF
};

/**
 * @brief Build and queue a twilio request.
 *
 * @param [in]msg raw panel message that caused event.
 * @param [in]s virtual partition state.
 * @param [in]arg string pointer event class string.
 *
 */
void ad2_event_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if (!s || (defs && s->partition == defs->partition)) {
        ESP_LOGI(TAG, "twilio ad2_event_cb '%i'", (int)arg);

        std::string body;
        if (AD2Parse.event_str.find((int)arg) == AD2Parse.event_str.end()) {
            body = ad2_string_printf("EVENT ID %d",(int)arg);
        } else {
            body = AD2Parse.event_str[(int)arg];
        }

        if (g_ad2_network_state == AD2_CONNECTED) {
            // load our settings for this event type.
            std::string sid;
            ad2_get_nv_slot_key_string(TWILIO_SID_CFGKEY, AD2_DEFAULT_TWILIO_SLOT, nullptr, sid);

            std::string token;
            ad2_get_nv_slot_key_string(TWILIO_TOKEN_CFGKEY, AD2_DEFAULT_TWILIO_SLOT, nullptr, token);

            std::string from;
            ad2_get_nv_slot_key_string(TWILIO_FROM_CFGKEY, AD2_DEFAULT_TWILIO_SLOT, nullptr, from);

            std::string to;
            ad2_get_nv_slot_key_string(TWILIO_TO_CFGKEY, AD2_DEFAULT_TWILIO_SLOT, nullptr, to);

            std::string type;
            ad2_get_nv_slot_key_string(TWILIO_TYPE_CFGKEY, AD2_DEFAULT_TWILIO_SLOT, nullptr, type);
            type.resize(1);

            // sanity check see if type(char) is in our list of types
            if (std::string("MRT").find(type) != string::npos) {
                // add to the queue
                ESP_LOGI(TAG, "Adding task to twilio send queue");
                twilio_add_queue(sid, token, from, to, type[0], body);
            } else {
                ESP_LOGI(TAG, "Unknown or missing twtype '%s' check settings. Not adding to delivery queue.", type.c_str());
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
void on_search_match_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    AD2EventSearch *es = (AD2EventSearch *)arg;
    ESP_LOGI(TAG, "ON_SEARCH_MATCH_CB: '%s' -> '%s'", msg->c_str(), es->out_message.c_str());
    for( std::size_t idx = 1; idx < es->RESULT_GROUPS.size(); ++idx ) {
        ESP_LOGI(TAG, "----- '%s'", es->RESULT_GROUPS[idx].c_str());
    }
}

/**
 * Twilio generic command event processing
 *  command: [COMMAND] <id> <arg>
 * ex.
 *   [COMMAND] 0 arg...
 */
static void _cli_cmd_twilio_event(char *string)
{
    int slot = -1;
    std::string buf;
    std::string key;

    // key value validation
    ad2_copy_nth_arg(key, string, 0);
    ad2_lcase(key);

    int i;
    for(i = 0;; ++i) {
        if (TWILIO_SETTINGS[i] == 0) {
            ad2_printf_host("What?\r\n");
            break;
        }
        if(key.compare(TWILIO_SETTINGS[i]) == 0) {
            if (ad2_copy_nth_arg(buf, string, 1) >= 0) {
                slot = strtol(buf.c_str(), NULL, 10);
            }
            if (slot >= 0) {
                if (ad2_copy_nth_arg(buf, string, 2) >= 0) {
                    ad2_set_nv_slot_key_string(key.c_str(), slot, nullptr, buf.c_str());
                    ad2_printf_host("Setting %s value finished.\r\n", key.c_str());
                } else {
                    ad2_get_nv_slot_key_string(key.c_str(), slot, nullptr, buf);
                    ad2_printf_host("Current slot #%02i '%s' value '%s'\r\n", slot, key.c_str(), buf.c_str());
                }
            } else {
                ad2_printf_host("Missing <slot>\r\n");
            }
            // found match search done
            break;
        }
    }
}

/**
 * Twilio smart alert switch command event processing
 *  command: [COMMAND] <id> <setting> <arg1> <arg2>
 * ex. Switch #0 [N]otification slot 0
 *   [COMMAND] 0 N 0
 */
static void _cli_cmd_twilio_smart_alert_switch(char *string)
{
    int i;
    int slot = -1;
    std::string buf;
    std::string key;
    std::string arg1;
    std::string arg2;
    std::string tmpsz;

    // key value validation
    ad2_copy_nth_arg(key, string, 0);

    if (ad2_copy_nth_arg(buf, string, 1) >= 0) {
        slot = std::atoi (buf.c_str());
    }
    // add the slot# to the key max 99 slots.
    if (slot > 0 && slot < 100) {

        if (ad2_copy_nth_arg(buf, string, 2) >= 0) {

            // sub key suffix.
            std::string sk = ad2_string_printf("%c", buf[0]);

            // get the args if any
            ad2_copy_nth_arg(arg1, string, 3);
            ad2_copy_nth_arg(arg2, string, 4);

            /* setting */
            switch(buf[0]) {
            case 'N': // Notification slot
                i = std::atoi (arg1.c_str());
                tmpsz = i < 0 ? "Clearing" : "Setting";
                ad2_set_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), i);
                ad2_printf_host("%s smartswitch #%i to use notification settings from slot #%i.\r\n", tmpsz.c_str(), slot, i);
                break;
            case 'D': // Default state
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), i);
                ad2_printf_host("Setting smartswitch #%i to use default state '%s' %i.\r\n", slot, AD2Parse.state_str[i].c_str(), i);
                break;
            case 'R': // Auto Reset
                i = std::atoi (arg1.c_str());
                ad2_set_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), i);
                ad2_printf_host("Setting smartswitch #%i auto reset value %i.\r\n", slot, i);
                break;
            case 'T': // Message type filter list
                ad2_set_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), arg1.c_str());
                ad2_printf_host("Setting smartswitch #%i message type filter list to '%s'.\r\n", slot, arg1.c_str());
                break;
            case 'P': // Pre filter REGEX
                ad2_set_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), arg1.c_str());
                ad2_printf_host("Setting smartswitch #%i pre filter regex to '%s'.\r\n", slot, arg1.c_str());
                break;

            case 'O': // Open state REGEX list editor
            case 'C': // Close state REGEX list editor
            case 'F': // Fault state REGEX list editor
                // Add index to file name to track N number of elements.
                i = std::atoi (arg1.c_str());
                if ( i > 0 && i < 9) {
                    std::string op = arg2.length() > 0 ? "Setting" : "Clearing";
                    sk+=ad2_string_printf("%02i", i);
                    if (buf[0] == 'O') {
                        tmpsz = "OPEN";
                    }
                    if (buf[0] == 'C') {
                        tmpsz = "CLOSED";
                    }
                    if (buf[0] == 'F') {
                        tmpsz = "FAULT";
                    }
                    ad2_set_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), arg2.c_str());
                    ad2_printf_host("%s smartswitch #%i REGEX filter #%02i for state '%s' to '%s'.\r\n", op.c_str(), slot, i, tmpsz.c_str(), arg2.c_str());
                } else {
                    ad2_printf_host("Error invalid index %i. Valid values are 1-8.\r\n", slot, i);
                }
                break;

            case 'o': // Open state output format string
            case 'c': // Close state output format string
            case 'f': // Fault state output format string
                if (buf[0] == 'o') {
                    tmpsz = "OPEN";
                }
                if (buf[0] == 'c') {
                    tmpsz = "CLOSED";
                }
                if (buf[0] == 'f') {
                    tmpsz = "FAULT";
                }
                ad2_set_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), arg1.c_str());
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
            std::string args = "NDRTPOCFocf";
            ad2_printf_host("Twilio SmartSwitch #%i report\r\n", slot);
            // sub key suffix.
            std::string sk;
            for(char& c : args) {
                std::string sk = ad2_string_printf("%c", c);
                switch(c) {
                case 'N':
                    i = 0; // Default 0
                    ad2_get_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), &i);
                    ad2_printf_host("  Using notification settings from slot #%i.\r\n", i);
                    break;
                case 'D':
                    i = 0; // Default CLOSED
                    ad2_get_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), &i);
                    ad2_printf_host("  Switch default state is '%s' (%i).\r\n", AD2Parse.state_str[i].c_str(), i);
                    break;
                case 'R':
                    i = 0; // Defaut 0 or disabled
                    ad2_get_nv_slot_key_int(TWILIO_SAS_CFGKEY, slot, sk.c_str(), &i);
                    ad2_printf_host("  Auto reset time in ms is '%s'.\r\n", (i > 0) ? ad2_to_string(i).c_str() : "DISABLED");
                    break;
                case 'T':
                    out = "";
                    ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), out);
                    ad2_printf_host("  Message type list is '%s'.\r\n", out.c_str());
                    break;
                case 'P':
                    out = "";
                    ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), out);
                    ad2_printf_host("  Pre filter REGEX is '%s'.\r\n", out.c_str());
                    break;
                case 'O':
                case 'C':
                case 'F':
                    // * Find all sub keys and show info.
                    if (c == 'O') {
                        tmpsz = "OPEN";
                    }
                    if (c == 'C') {
                        tmpsz = "CLOSED";
                    }
                    if (c == 'F') {
                        tmpsz = "FAULT";
                    }
                    for ( i = 1; i < 9; i++ ) {
                        out = "";
                        std::string tsk = sk + ad2_string_printf("%02i", i);
                        ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, tsk.c_str(), out);
                        if (out.length()) {
                            ad2_printf_host("  '%s' state REGEX Filter #%02i is '%s'.\r\n", tmpsz.c_str(), i, out.c_str());
                        }
                    }
                    break;
                case 'o':
                case 'c':
                case 'f':
                    if (c == 'o') {
                        tmpsz = "OPEN";
                    }
                    if (c == 'c') {
                        tmpsz = "CLOSED";
                    }
                    if (c == 'f') {
                        tmpsz = "FAULT";
                    }
                    out = "";
                    ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, slot, sk.c_str(), out);
                    ad2_printf_host("  Output format string for '%s' state is '%s'.\r\n", tmpsz.c_str(), out.c_str());
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
 * @brief command list for module
 */
static struct cli_command twilio_cmd_list[] = {
    {
        (char*)TWILIO_TOKEN_CFGKEY,(char*)
        "- Sets the 'User Auth Token' for notifications.\r\n\r\n"
        "  ```" TWILIO_TOKEN_CFGKEY " {slot} {hash}```\r\n\r\n"
        "  - {slot}: [N]\r\n"
        "    - For default use 0. Support multiple accounts.\r\n"
        "  - {hash}: Twilio 'User Auth Token'\r\n\r\n"
        "  Example: " TWILIO_TOKEN_CFGKEY " 0 aabbccdd112233..\r\n\r\n", _cli_cmd_twilio_event
    },
    {
        (char*)TWILIO_SID_CFGKEY,(char*)
        "- Sets the 'Account SID' for notification.\r\n\r\n"
        "  ```" TWILIO_SID_CFGKEY " {slot} {hash}```\r\n\r\n"
        "  - {slot}: [N]\r\n"
        "    - For default use 0. Support multiple accounts.\r\n"
        "  - {hash}: Twilio 'Account SID'\r\n\r\n"
        "  Example: " TWILIO_SID_CFGKEY " 0 aabbccdd112233..\r\n\r\n", _cli_cmd_twilio_event
    },
    {
        (char*)TWILIO_FROM_CFGKEY,(char*)
        "- Sets the 'From' address for notification.\r\n\r\n"
        "  ```" TWILIO_FROM_CFGKEY " {slot} {phone} ```\r\n\r\n"
        "  - {slot}: [N]\r\n"
        "    - For default use 0. Support multiple accounts.\r\n"
        "  - {phone|email}: [NXXXYYYZZZZ]\r\n"
        "    - From phone #\r\n\r\n"
        "  Example: " TWILIO_FROM_CFGKEY " 0 13115552368\r\n\r\n", _cli_cmd_twilio_event
    },
    {
        (char*)TWILIO_TO_CFGKEY,(char*)
        "- Sets the 'To' address for notification.\r\n\r\n"
        "  ```" TWILIO_TO_CFGKEY " {slot} {phone}```\r\n\r\n"
        "  - {slot}: [N]\r\n"
        "    - For default use 0. Support multiple accounts.\r\n"
        "  - {phone|email}: [NXXXYYYZZZZ]\r\n"
        "    - To phone #\r\n\r\n"
        "  Example: " TWILIO_TO_CFGKEY " 0 13115552368\r\n\r\n", _cli_cmd_twilio_event
    },
    {
        (char*)TWILIO_TYPE_CFGKEY,(char*)
        "- Sets the 'Type' for notification.\r\n\r\n"
        "  ```" TWILIO_TYPE_CFGKEY " {slot} {type}```\r\n\r\n"
        "  - {slot}\r\n"
        "    - For default use 0. Support multiple accounts.\r\n"
        "  - {type}: [M|C|E]\r\n"
        "    - Notification type [M]essage, [C]all, [E]mail.\r\n\r\n"
        "  Example: " TWILIO_TYPE_CFGKEY " 0 M\r\n\r\n", _cli_cmd_twilio_event
    },
    {
        (char*)TWILIO_SAS_CFGKEY,(char*)
        "- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.\r\n\r\n"
        "  ```" TWILIO_SAS_CFGKEY " {slot} {setting} {arg1} [arg2]```\r\n\r\n"
        "  - {slot}\r\n"
        "    - 1-99 : Supports multiple virtual smart alert switches.\r\n"
        "  - {setting}\r\n"
        "    - [N] Notification slot\r\n"
        "      -  Notification settings slot to use for sending this events.\r\n"
        "    - [D] Default state\r\n"
        "      - {arg1}: [0]CLOSE [1]OPEN\r\n"
        "    - [R] AUTO Reset.\r\n"
        "      - {arg1}:  time in ms 0 to disable\r\n"
        "    - [T] Message type filter.\r\n"
        "      - {arg1}: Message type list seperated by ',' or empty to disables filter.\r\n"
        "        - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR]\r\n"
        "    - [P] Pre filter REGEX or empty to disable.\r\n"
        "    - [O] Open state regex search string list management.\r\n"
        "      - {arg1}: Index # 1-8\r\n"
        "      - {arg2}: Regex string for this slot or empty string  to clear\r\n"
        "    - [C] Close state regex search string list management.\r\n"
        "      - {arg1}: Index # 1-8\r\n"
        "      - {arg2}: Regex string for this slot or empty string to clear\r\n"
        "    - [F] Fault state regex search string list management.\r\n"
        "      - {arg1}: Index # 1-8\r\n"
        "      - {arg2}: Regex string for this slot or empty  string to clear\r\n"
        "    - [o] Open output format string.\r\n"
        "    - [c] Close output format string.\r\n"
        "    - [f] Fault output format string.\r\n\r\n", _cli_cmd_twilio_smart_alert_switch
    },
};

/**
 * Initialize queue and SSL
 */
void twilio_init()
{
    int res = 0;

    // Register twilio CLI commands
    for (int i = 0; i < ARRAY_SIZE(twilio_cmd_list); i++) {
        cli_register_command(&twilio_cmd_list[i]);
    }

    // init server_fd
    mbedtls_net_init(&server_fd);
    // init ssl
    mbedtls_ssl_init(&ssl);
    // init SSL conf
    mbedtls_ssl_config_init(&conf);
#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
    mbedtls_ssl_cache_init( &cache );
#endif
    // init cert for api.twilio.com
    mbedtls_x509_crt_init(&api_twilio_com_cacert);
    // init cert for api.sendgrid.com
    mbedtls_x509_crt_init(&api_sendgrid_com_cacert);
    // init entropy
    mbedtls_entropy_init(&entropy);
    // init drbg
    mbedtls_ctr_drbg_init(&ctr_drbg);
#if defined(DEBUG_TWILIO)
    mbedtls_debug_set_threshold( DEBUG_LEVEL );
#endif

    ESP_LOGI(TAG, "Loading api.twilio.com CA root certificate...");
    res = mbedtls_x509_crt_parse(&api_twilio_com_cacert, twilio_root_pem_start,
                                 twilio_root_pem_end-twilio_root_pem_start);
    if(res < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Loading api.sendgrid.com CA root certificate...");
    res = mbedtls_x509_crt_parse(&api_sendgrid_com_cacert, sendgrid_root_pem_start,
                                 sendgrid_root_pem_end-sendgrid_root_pem_start);
    if(res < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Seeding the random number generator");
    if((res = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", res);
        twilio_free();
        return;
    }

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.
       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &api_twilio_com_cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf, 4);
#endif

#if defined(MBEDTLS_SSL_CACHE_C_BROKEN)
    mbedtls_ssl_conf_session_cache( &conf, &cache,
                                    mbedtls_ssl_cache_get,
                                    mbedtls_ssl_cache_set );
#endif

    ESP_LOGI(TAG, "Setting hostname for TLS session...");
    /* Hostname set here should match CN in server certificate */
    if((res = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");
    if((res = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", res);
        twilio_free();
        return;
    }

    if ((res = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -res);
        twilio_free();
        return;
    }

    ESP_LOGI(TAG, "Starting twilio queue consumer task");
    sendQ = xQueueCreate(TWILIO_QUEUE_SIZE,sizeof(struct twilio_message_data *));
    if( sendQ == 0 ) {
        ESP_LOGE(TAG, "Failed to create twilio queue.");
    } else {
        xTaskCreate(&twilio_consumer_task, "twilio_consumer_task", 2048, NULL, tskIDLE_PRIORITY+1, NULL);
    }

    // TODO configure to all selection on events to notify on.

    // Register callbacks for a few static events for testing.
    AD2Parse.subscribeTo(ON_LRR, ad2_event_cb, (void*)ON_LRR);
    AD2Parse.subscribeTo(ON_ARM, ad2_event_cb, (void*)ON_ARM);
    AD2Parse.subscribeTo(ON_DISARM, ad2_event_cb, (void*)ON_DISARM);
    AD2Parse.subscribeTo(ON_FIRE, ad2_event_cb, (void*)ON_FIRE);
    AD2Parse.subscribeTo(ON_ALARM_CHANGE, ad2_event_cb, (void*)ON_ALARM_CHANGE);

    // Register search based virtual switches.
    for (int i = 1; i < 99; i++) {
        int notification_slot = -1;
        ad2_get_nv_slot_key_int(TWILIO_SAS_CFGKEY, i, "N", &notification_slot);
        if (notification_slot >= 0) {
            AD2EventSearch *es1 = new AD2EventSearch(AD2_STATE_CLOSED, 0);
            // We at least need some output format or skip
            ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, i, "o", es1->OPEN_OUTPUT_FORMAT);
            ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, i, "c", es1->CLOSED_OUTPUT_FORMAT);
            ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, i, "f", es1->FAULT_OUTPUT_FORMAT);
            if ( es1->OPEN_OUTPUT_FORMAT.length()
                    || es1->CLOSED_OUTPUT_FORMAT.length()
                    || es1->FAULT_OUTPUT_FORMAT.length() ) {
                ESP_LOGI(TAG,"FIXME: adding subscriber slot#%02i notification id %02i", i, notification_slot);
                std::string notify_types_sz = "";
                std::vector<std::string> notify_types_v;
                ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, i, "T", notify_types_sz);
                ad2_tokenize(notify_types_sz, ',', notify_types_v);
                for (auto &sztype : notify_types_v) {
                    ad2_trim(sztype);
                    auto x = AD2Parse.message_type_id.find(sztype);
                    if(x != std::end(AD2Parse.message_type_id)) {
                        ad2_message_t mt = (ad2_message_t)AD2Parse.message_type_id.at(sztype);
                        ESP_LOGI(TAG,"FIXME: adding type '%s' -> %i", sztype.c_str(), (int)mt);
                        es1->PRE_FILTER_MESAGE_TYPE.push_back(mt);
                    }
                }
                ad2_get_nv_slot_key_string(TWILIO_SAS_CFGKEY, i, "P", es1->PRE_FILTER_REGEX);

                // FIXME
                es1->OPEN_REGEX_LIST.push_back("RFX:0041594,1.......");
                es1->CLOSED_REGEX_LIST.push_back("RFX:0041594,0.......");
                es1->FAULT_REGEX_LIST.push_back("RFX:0041594,......1.");

                AD2EventSearches.push_back(es1);
                AD2Parse.subscribeTo(on_search_match_cb, es1);
            }
        }
    }



#if 1 /* TESTING FIXME */
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, ad2_event_cb, (void*)ON_CHIME_CHANGE);
#endif
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_TWILIO_CLIENT */

