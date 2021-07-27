/**
 *  @file    webUI.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    07/17/2021
 *
 *  @brief WEB server for user interface to alarm system
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
// AlarmDecoder std includes
#include "alarmdecoder_main.h"
#include "ad2_settings.h"
#include "ad2_utils.h"

// esp networking etc.
#include <lwip/netdb.h>
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

// cJSON component (esp-idf REQUIRES json)
#include "cJSON.h"

// AlarmDecoder IoT hardware settings.
#include "device_control.h"

// Disable via sdkconfig
#if CONFIG_AD2IOT_WEBSERVER_UI
static const char *TAG = "WEBUI";

// Define the virtual mount prefix for all file operations.
#define MOUNT_POINT "/sdcard"

/**
 * SSDP SECRETS/SETTINGS
 */
// UUID
// Length fixed 36 characters
// format args
// 1-3: esp32 chip id
// 4-7: unique 32 bit value
#define AD2IOT_UUID_FORMAT   "41443245-4d42-4544-44%02x-%02x%02x%02x%02x%02x%02x"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (255)

/* helper macro to test for extenions */
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

// Global handle to httpd server
httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
httpd_handle_t server = NULL;


/**
 * @brief websocket session storage structure.
 */
struct ws_session_storage {
    int vpartID;
    int codeID;
};

// C++

// extern C
#ifdef __cplusplus
extern "C" {
#endif

// Include template engine from
//   https://github.com/full-stack-ex/tiny-template-engine-arduino
// Currently not functional with esp-idf development platform only Arduino so some mods were needed.
#include "TinyTemplateEngine.h"
#include "TinyTemplateEngineFileReader.h"

/**
 * generate AD2* uuid
 */
void genUUID(uint32_t n, std::string &ret)
{
    uint8_t chipid[6];
    esp_read_mac(chipid, ESP_MAC_WIFI_STA);

    char _uuid[37];
    snprintf(_uuid, sizeof(_uuid), AD2IOT_UUID_FORMAT,
             (uint16_t) ((chipid[0]) & 0xff),
             (uint16_t) ((chipid[1]) & 0xff),
             (uint16_t) ((chipid[2]) & 0xff),
             (uint16_t) ((n      >> 24) & 0xff),
             (uint16_t) ((n      >> 16) & 0xff),
             (uint16_t) ((n      >>  8) & 0xff),
             (uint16_t) ((n           ) & 0xff));
    ret = _uuid;
}

/**
 * generate uptime string
 */
void uptimeString(std::string &tstring)
{
    // 64bit milliseconds since boot
    int64_t ms = esp_timer_get_time() / 1000;
    // seconds
    int32_t s = ms / 1000;
    // days
    int32_t d = s / 86400;
    s %= 86400;
    // hours
    uint8_t h = s / 3600;
    s %= 3600;
    // minutes
    uint8_t m = s / 60;
    s %= 60;
    char fbuff[255];
    snprintf(fbuff, sizeof(fbuff), "%04dd:%02dh:%02dm:%02ds", d, h, m, s);
    tstring = fbuff;
}

/**
 * @brief Given a file name set the request session response type.
 *
 * @param [in]httpd_req_t *req
 * @param [in]const char *filename
 *
 * @return esp_err_t
 *
 */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    /* Limited set of types hard coded here */
    if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "application/javascript");
    } else if (IS_FILE_EXT(filename, ".json")) {
        return httpd_resp_set_type(req, "application/json");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".svg")) {
        return httpd_resp_set_type(req, "image/svg+xml");
    } else if (IS_FILE_EXT(filename, ".gz")) {
        return httpd_resp_set_type(req, "application/x-gzip");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/**
 * @brief Copies the full path into destination buffer and returns
 * pointer to relative path after the base_path.
 *
 * @param [in]char *dest
 * @param [in]const char *base_path
 * @param [in]const char *uri
 * @param [in]size_t destsize
 *
 * @return stat const char*
 *
 */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/**
 * @brief Free session context memory.
 *
 * @param [in]void *ctx.
 *
 */
void free_ws_session_storage(void *ctx)
{
    // sanity check.
    if (ctx) {
        free(ctx);
    }
}

/**
 * @brief Generate a JSON string for the given AD2VirtualPartitionState pointer.
 *
 * @param [in]AD2VirtualPartitionState * to use for json object.
 *
 * @return cJSON*
 *
 */
cJSON *get_alarmdecoder_state_json(AD2VirtualPartitionState *s)
{
    cJSON *root = cJSON_CreateObject();

    // Add this boards info to the object
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddNumberToObject(root, "cpu_model", chip_info.model);
    cJSON_AddNumberToObject(root, "cpu_revision", chip_info.revision);
    cJSON_AddNumberToObject(root, "cpu_cores", chip_info.cores);
    cJSON *cjson_cpu_features = cJSON_CreateArray();
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString( "WiFi" ));
    }
    if (chip_info.features & CHIP_FEATURE_BLE) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString( "BLE" ));
    }
    if (chip_info.features & CHIP_FEATURE_BT) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString( "BT" ));
    }
    cJSON_AddItemToObject(root, "cpu_features", cjson_cpu_features);
    cJSON_AddNumberToObject(root, "cpu_flash_size", spi_flash_get_chip_size());
    std::string flash_type = (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external";
    cJSON_AddStringToObject(root, "cpu_flash_type", flash_type.c_str());

    if (s && !s->unknown_state) {
        cJSON_AddBoolToObject(root, "ready", s->ready);
        cJSON_AddBoolToObject(root, "armed_away", s->armed_away);
        cJSON_AddBoolToObject(root, "armed_stay", s->armed_stay);
        cJSON_AddBoolToObject(root, "backlight_on", s->backlight_on);
        cJSON_AddBoolToObject(root, "programming_mode", s->programming_mode);
        cJSON_AddBoolToObject(root, "zone_bypassed", s->zone_bypassed);
        cJSON_AddBoolToObject(root, "ac_power", s->ac_power);
        cJSON_AddBoolToObject(root, "chime_on", s->chime_on);
        cJSON_AddBoolToObject(root, "alarm_event_occurred", s->alarm_event_occurred);
        cJSON_AddBoolToObject(root, "alarm_sounding", s->alarm_sounding);
        cJSON_AddBoolToObject(root, "battery_low", s->battery_low);
        cJSON_AddBoolToObject(root, "entry_delay_off", s->entry_delay_off);
        cJSON_AddBoolToObject(root, "fire_alarm", s->fire_alarm);
        cJSON_AddBoolToObject(root, "system_issue", s->system_issue);
        cJSON_AddBoolToObject(root, "perimeter_only", s->perimeter_only);
        cJSON_AddBoolToObject(root, "exit_now", s->exit_now);
        cJSON_AddNumberToObject(root, "system_specific", s->system_specific);
        cJSON_AddNumberToObject(root, "beeps", s->beeps);
        cJSON_AddStringToObject(root, "panel_type", std::string(1, s->panel_type).c_str());
        cJSON_AddStringToObject(root, "last_alpha_messages", s->last_alpha_message.c_str());
        cJSON_AddStringToObject(root, "last_numeric_messages", s->last_numeric_message.c_str()); // Can have HEX digits ex. 'FC'.
    }
    return root;
}

#if CONFIG_HTTPD_WS_SUPPORT
/**
 * @brief Send current alarm state to web socket connection. Lookup
 * ws_session_storage for this connection from user_ctx
 *
 * @param [in]void * socket fd for this connection cast as void *.
 *
 */
static void ws_alarmstate_async_send(void *arg)
{
    ESP_LOGI(TAG, "ws_alarmstate_async_send: %i", (int)arg);
    int wsfd = (int)arg;
    if (wsfd) {
        // lookup session context from socket id.
        struct ws_session_storage *sess = (ws_session_storage *)httpd_sess_get_ctx(server, wsfd);
        if (sess) {
            // get the partition state based upon the virtual partition ID on the AD2IoT firmware.
            AD2VirtualPartitionState *s = ad2_get_partition_state(sess->vpartID);
            if (s) {
                // build the standard json AD2IoT device and alarm state object.
                cJSON *root = get_alarmdecoder_state_json(s);
                if (root) {
                    ESP_LOGI(TAG, "sending alarm panel state for virtual partition: %i", sess->vpartID);
                    cJSON_AddStringToObject(root, "event", "SYNC");
                    const char *sys_info = cJSON_Print(root);
                    httpd_ws_frame_t ws_pkt;
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.payload = (uint8_t*)sys_info;
                    ws_pkt.len = strlen(sys_info);
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                    httpd_ws_send_frame_async(server, wsfd, &ws_pkt);
                    free((void *)sys_info);
                    cJSON_Delete(root);
                }
            }
        }
    }
}

/**
 * @brief HTTP GET websocket handler for api access.
 *
 * @param [in]httpd_req_t *
 *
 * @return esp_err_t
 *
 */
esp_err_t ad2ws_handler(httpd_req_t *req)
{
    uint8_t rx_buf[128] = { 0 };
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = rx_buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "websocket packet with message: %s and type %d", ws_pkt.payload, ws_pkt.type);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {

        // Register and return current state.
        std::string key_sync = "!SYNC:";
        if(strncmp((char*)ws_pkt.payload, key_sync.c_str(), key_sync.length()) == 0) {
            /* Create session's context if not already available */
            if (!req->sess_ctx) {
                req->sess_ctx = malloc(sizeof(ws_session_storage));  /*!< Pointer to context data */
                req->free_ctx = free_ws_session_storage;             /*!< Function to free context data */
            }
            std::string args((char *)&ws_pkt.payload[key_sync.length()]);
            std::vector<std::string> args_v;
            ad2_tokenize(args, ',', args_v);
            int codeID = atoi(args_v[1].c_str());
            int vpartID = atoi(args_v[0].c_str());
            ((ws_session_storage *)req->sess_ctx)->codeID = codeID;
            ((ws_session_storage *)req->sess_ctx)->vpartID = vpartID;
            ESP_LOGI(TAG, "Got !SYNC request triggering to send current alarm state for virtual partition %i.", vpartID);

            // trigger an async send using httpd_queue_work
            return httpd_queue_work(req->handle, ws_alarmstate_async_send, (void *)httpd_req_to_sockfd(req));
        }

        std::string key_ping = "!PING:";
        if(strncmp((char*)ws_pkt.payload, key_ping.c_str(), key_ping.length()) == 0) {
            // send back a !PONG reply
            std::string pong = "!PONG:00000000";
            ws_pkt.payload = (uint8_t *)pong.c_str();
            ws_pkt.len = pong.length();
            ret = httpd_ws_send_frame(req, &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
            }
        }

        std::string key_send = "!SEND:";
        if(strncmp((char*)ws_pkt.payload, key_send.c_str(), key_send.length()) == 0) {
            std::string sendbuf = (char *)&ws_pkt.payload[key_send.length()];
            int codeID = ((ws_session_storage *)req->sess_ctx)->codeID;
            int vpartID = ((ws_session_storage *)req->sess_ctx)->vpartID;

            if (sendbuf.rfind("<DISARM>", 0) == 0) {
                ad2_disarm(codeID, vpartID);
            } else if (sendbuf.rfind("<STAY>", 0) == 0) {
                ad2_arm_stay(codeID, vpartID);
            } else if (sendbuf.rfind("<AWAY>", 0) == 0) {
                ad2_arm_away(codeID, vpartID);
            } else if (sendbuf.rfind("<EXIT>", 0) == 0) {
                ad2_exit_now(vpartID);
            } else {
                ESP_LOGI(TAG, "Unknown websocket command '%s'", sendbuf.c_str());
            }
        }
    }
    return ret;
}
#endif

/**
 * @brief HTTP GET handler for downloading files from uSD card.
 *
 * @param [in]httpd_req_t *
 *
 * @return esp_err_t
 *
 */
esp_err_t file_get_handler(httpd_req_t *req)
{

    // state: send raw file or process as template
    bool apply_template = false;
    bool apply_gzip = false;

    struct stat file_stat;

    // extract the full file path using MOUNT_POINT as the root.
    char temppath[FILE_PATH_MAX];
    const char *filename = get_path_from_uri(temppath,  MOUNT_POINT,
                           req->uri, sizeof(temppath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL; // close socket
    }

    // copy string into something more flexible.
    std::string filepath(temppath);

    // Special case check for pre compressed files.
    std::string gzfile = filepath + ".gz";
    if (stat(gzfile.c_str(), &file_stat) == 0) {
        // Add content encoding so the client knows how to pre process the stream.
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        // change file adding gz so we spool out the pre compressed file.
        filepath = gzfile;
        apply_gzip = true;
    } else {
        // Check if _NOT_ exists swap for 404.html
        if (stat(filepath.c_str(), &file_stat) != 0) {
            // not found check if we have a custom 404.html file in our document root.
            filepath = MOUNT_POINT "/404.html";
            if (stat(filepath.c_str(), &file_stat) != 0) {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
                return ESP_FAIL; // close socket
            }
            // set status and continue processing 404.html file
            httpd_resp_set_status(req, "404 Not found");
        } else {
            // Check if a flag file with the same name with .tpl extension exist.
            std::string tplfile = filepath+".tpl";
            if (!apply_gzip && stat(tplfile.c_str(), &file_stat) == 0) {
                apply_template = true; //FIXME disable for testing.
            }
        }
    }

    // set the content type based upon the extension.
    set_content_type_from_file(req, filepath.c_str());

    // inform the client we prefer they disconnect when the page is delivered.
    httpd_resp_set_hdr(req, "Connection", "close");

    // Open the file and spool it to the client.
    ESP_LOGD(TAG, "opening file : %s", filepath.c_str());
    FILE *f = fopen(filepath.c_str(), "r");
    if (f == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL; // close socket
    }

    // return state
    int result = ESP_OK;

    // FIXME: add function will use it more than 1 time.
    // apply template if set
    if (apply_template) {
        char ipstr[INET6_ADDRSTRLEN];
        int sockfd = httpd_req_to_sockfd(req);
        struct sockaddr_in6 addr;   // esp_http_server uses IPv6 addressing
        socklen_t addr_size = sizeof(addr);

        // build standard template values FIXME: function dynamic.
        // Version MACRO ${0}
        std::string szVersion = "1.0";

        // Time MACRO ${1}
        std::string szTime;
        uptimeString(szTime);

        // Local IP MACRO ${2}
        if (getsockname(sockfd, (struct sockaddr *)&addr, &addr_size) < 0) {
            ESP_LOGE(TAG, "Error getting local IP");
        }
        /// Convert to IPv4 string
        inet_ntop(AF_INET, &addr.sin6_addr.un.u32_addr[3], ipstr, sizeof(ipstr));
        std::string szLocalIP = ipstr; //req->getClientIP().toString();

        // Client IP MACRO ${3}
        if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_size) < 0) {
            ESP_LOGE(TAG, "Error getting client IP");
        }
        /// Convert to IPv4 string
        inet_ntop(AF_INET, &addr.sin6_addr.un.u32_addr[3], ipstr, sizeof(ipstr));
        std::string szClientIP = ipstr; //req->getClientIP().toString();

        // Request protocol MACRO ${4}
        std::string szProt = "HTTP"; // (req->isSecure() ? "HTTPS" : "HTTP");

        // UUID MACRO ${5}
        std::string szUUID;
        genUUID(0, szUUID);

        const char* values[] = {
            szVersion.c_str(), // match ${0}
            szTime.c_str(),    // match ${1}
            szLocalIP.c_str(), // match ${2}
            szClientIP.c_str(),// match ${3}
            szProt.c_str(),    // match ${4}
            szUUID.c_str(),    // match ${5}
            0 // guard
        };

        // init template engine
        TinyTemplateEngineFileReader reader(f);
        reader.keepLineEnds(true);
        TinyTemplateEngine engine(reader);

        // process and send
        engine.start(values);

        // Send content
        int len = 0;
        do {
            len = 0;
            const char* line = engine.nextLine();
            if (line) {
                len = strlen(line);
                if (len && httpd_resp_send_chunk(req, line, len) != ESP_OK) {
                    result = ESP_FAIL;
                    break;
                }
            }
            // Yield to other tasks.
            vTaskDelay(1);
        } while (len);
        engine.end();
    } else {
        /* Read file in chunks (relaxes any constraint due to large file sizes)
        * and send HTTP response in chunked encoding */
        char   chunk[1024];
        size_t chunksize;
        do {
            chunksize = fread(chunk, 1, sizeof(chunk), f);
            if (chunksize && httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                result = ESP_FAIL;
                break;
            }
        } while (chunksize != 0);
    }

    // all done sending the file so close it.
    fclose(f);

    // FIXME: close socket
    if (httpd_resp_send_chunk(req, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "error calling httpd_resp_send_chunk with NULL to close.");
        result = ESP_FAIL; // close socket.
    }
    // finally force the socket to be closed.
    httpd_sess_trigger_close(req->handle, httpd_req_to_sockfd(req));

    return result;
}

/**
 * @brief Generic callback for all AlarmDecoder API event subscriptions.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2VirtualPartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void on_state_change(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
#if CONFIG_HTTPD_WS_SUPPORT
    ESP_LOGI(TAG, "on_state_change partition(%i) event(%s) message('%s')", s->partition, AD2Parse.event_str[(int)arg].c_str(), msg->c_str());
    size_t fds = server_config.max_open_sockets;
    int client_fds[fds];
    httpd_handle_t hd = server;
    httpd_get_client_list(hd, &fds, client_fds);
    for (int i=0; i<fds; i++) {
        if (httpd_ws_get_fd_info(hd, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            struct ws_session_storage *sess = (ws_session_storage *)httpd_sess_get_ctx(server, client_fds[i]);
            if (sess) {
                // get the partition state based upon the virtual partition requested.
                AD2VirtualPartitionState *temps = ad2_get_partition_state(sess->vpartID);
                if (temps && s->partition == temps->partition) {
                    cJSON *root = get_alarmdecoder_state_json(s);
                    cJSON_AddStringToObject(root, "event", AD2Parse.event_str[(int)arg].c_str());
                    const char *sys_info = cJSON_Print(root);

                    httpd_ws_frame_t ws_pkt;
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.payload = (uint8_t*)sys_info;
                    ws_pkt.len = strlen(sys_info);
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                    httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
                    free((void *)sys_info);
                    cJSON_Delete(root);
                }
            }
        }
    }
#endif
}

/**
 * @brief AD2IoT Component webUI init
 *
 */
void webUI_init(void)
{
    esp_err_t err;

    // Setup for Mount of uSD over SPI on the OLIMEX ESP-POE-ISO that is wired for a 1 bit data bus.
    const char mount_point[] = MOUNT_POINT;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

    gpio_set_pull_mode(GPIO_uSD_CMD, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_uSD_D0, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_uSD_CLK, GPIO_FLOATING);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;

    ESP_LOGI(TAG, "Mounting uSD card");
    sdmmc_card_t *card = NULL;
    err = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Mounting uSD card err: 0x%x (%s).", err, esp_err_to_name(err));
        return;
    }

    // Configure the web server and handlers.
    server_config.uri_match_fn = httpd_uri_match_wildcard;

#if CONFIG_HTTPD_WS_SUPPORT
    httpd_uri_t ad2ws_server = {
        .uri       = "/ad2ws",
        .method    = HTTP_GET,
        .handler   = ad2ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
    };
#endif
    httpd_uri_t file_server = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = file_get_handler,
        .user_ctx  = NULL,
#if CONFIG_HTTPD_WS_SUPPORT
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = nullptr
#endif
    };

    // Start the httpd server and handlers.
    ESP_LOGI(TAG, "Starting web services on port: %d", server_config.server_port);
    if (httpd_start(&server, &server_config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
#if CONFIG_HTTPD_WS_SUPPORT
        httpd_register_uri_handler(server, &ad2ws_server);
#endif
        httpd_register_uri_handler(server, &file_server);
        // Subscribe to AlarmDecoder events
        AD2Parse.subscribeTo(ON_ARM, on_state_change, (void *)ON_ARM);
        AD2Parse.subscribeTo(ON_DISARM, on_state_change, (void *)ON_DISARM);
        AD2Parse.subscribeTo(ON_CHIME_CHANGE, on_state_change, (void *)ON_CHIME_CHANGE);
        AD2Parse.subscribeTo(ON_FIRE, on_state_change, (void *)ON_FIRE);
        AD2Parse.subscribeTo(ON_POWER_CHANGE, on_state_change, (void *)ON_POWER_CHANGE);
        AD2Parse.subscribeTo(ON_READY_CHANGE, on_state_change, (void *)ON_READY_CHANGE);
        AD2Parse.subscribeTo(ON_LOW_BATTERY, on_state_change, (void *)ON_LOW_BATTERY);
        AD2Parse.subscribeTo(ON_ALARM_CHANGE, on_state_change, (void *)ON_ALARM_CHANGE);
        AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, on_state_change, (void *)ON_ZONE_BYPASSED_CHANGE);
        AD2Parse.subscribeTo(ON_EXIT_CHANGE, on_state_change, (void *)ON_EXIT_CHANGE);
        // All good return.
        return;
    }

    ESP_LOGI(TAG, "Error starting server!");
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_AD2IOT_WEBSERVER_UI */