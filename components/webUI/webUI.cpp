/**
 *  @file    webUI.cpp
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
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Disable via sdkconfig
#if CONFIG_AD2IOT_WEBSERVER_UI
static const char *TAG = "WEBUI";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "esp_http_server.h"

// specific includes

/* Constants that aren't configurable in menuconfig */
//#define DEBUG_WEBUI
#define PORT 10000
#define MAX_CLIENTS 4
#define MAX_FIFO_BUFFERS 30
#define MAXCONNECTIONS MAX_CLIENTS+1
#define WEBUI_DOC_ROOT         "/www"
#define WEBUI_COMMAND          "webui"
#define WEBUI_SUBCMD_ENABLE    "enable"
#define WEBUI_SUBCMD_ACL       "acl"

#define WEBUI_CONFIG_SECTION  "webui"

#define WEBUI_DEFAULT_ACL "0.0.0.0/0"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (255)

/* helper macro MIN */
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* helper macro to test for extenions */
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

// Global handle to httpd server
httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
httpd_handle_t server = nullptr;

/* ACL control */
ad2_acl_check webui_acl;

/**
 * WebUI command list and enum.
 */
char * WEBUI_SUBCMD [] = {
    (char*)WEBUI_SUBCMD_ENABLE,
    (char*)WEBUI_SUBCMD_ACL,
    0 // EOF
};

enum {
    WEBUI_SUBCMD_ENABLE_ID = 0,
    WEBUI_SUBCMD_ACL_ID,
};

/**
 * @brief websocket session storage structure.
 */
struct ws_session_storage {
    int partID;
    int codeID;
};

// C++

// Include template engine from
//   https://github.com/full-stack-ex/tiny-template-engine-arduino
// Currently not functional with esp-idf development platform only Arduino so some mods were needed.
#include "TinyTemplateEngine.h"
#include "TinyTemplateEngineFileReader.h"

/**
 * generate uptime string
 */
void uptimeString(std::string &tstring)
{
    // 64bit milliseconds since boot
    int64_t ms = hal_uptime_us() / 1000;
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
    int wsfd = (int)arg;
    if (wsfd) {
        if (server && hal_get_network_connected()) {
            // lookup session context from socket id.
            struct ws_session_storage *sess = (ws_session_storage *)httpd_sess_get_ctx(server, wsfd);
            if (sess) {
                // get the partition state based upon the partition ID on the AD2IoT firmware.
                AD2PartitionState *s = ad2_get_partition_state(sess->partID);
                if (s) {
                    // build the standard json AD2IoT device and alarm state object.
                    cJSON *root = ad2_get_partition_state_json(s);
                    cJSON *zone_alerts = ad2_get_partition_zone_alerts_json(s);
                    cJSON_AddStringToObject(root, "event", "SYNC");
                    cJSON_AddItemToObject(root, "zone_alerts", zone_alerts);
                    char *sys_info = cJSON_Print(root);
                    cJSON_Minify(sys_info);
                    httpd_ws_frame_t ws_pkt;
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.payload = (uint8_t*)sys_info;
                    ws_pkt.len = strlen(sys_info);
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                    httpd_ws_send_frame_async(server, wsfd, &ws_pkt);
                    cJSON_free(sys_info);
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
            ad2_tokenize(args, ", ", args_v);
            int codeID = atoi(args_v[1].c_str());
            int partID = atoi(args_v[0].c_str());
            ((ws_session_storage *)req->sess_ctx)->codeID = codeID;
            ((ws_session_storage *)req->sess_ctx)->partID = partID;

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
            int partID = ((ws_session_storage *)req->sess_ctx)->partID;

            if (sendbuf.rfind("<DISARM>", 0) == 0) {
                ad2_disarm(codeID, partID);
            } else if (sendbuf.rfind("<STAY>", 0) == 0) {
                ad2_arm_stay(codeID, partID);
            } else if (sendbuf.rfind("<AWAY>", 0) == 0) {
                ad2_arm_away(codeID, partID);
            } else if (sendbuf.rfind("<EXIT>", 0) == 0) {
                ad2_exit_now(partID);
            } else if (sendbuf.rfind("<AUX_ALARM>", 0) == 0) {
                ad2_aux_alarm(partID);
            } else if (sendbuf.rfind("<PANIC_ALARM>", 0) == 0) {
                ad2_panic_alarm(partID);
            } else if (sendbuf.rfind("<FIRE_ALARM>", 0) == 0) {
                ad2_fire_alarm(partID);
            } else if (sendbuf.rfind("<BYPASS>", 0) == 0) {
                // <BYPASS>XX
                std::string zone = sendbuf.substr(8, string::npos);
                ad2_bypass_zone(codeID, partID, std::atoi(zone.c_str()));
            } else {
                ESP_LOGW(TAG, "Unknown websocket command '%s'", sendbuf.c_str());
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

    // extract the full file path using AD2_USD_MOUNT_POINT as the root.
    char temppath[FILE_PATH_MAX];
    const char *filename = get_path_from_uri(temppath,  "/" AD2_USD_MOUNT_POINT WEBUI_DOC_ROOT,
                           req->uri, sizeof(temppath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL; // close socket
    }

    // copy string into something more flexible.
    std::string filepath(temppath);

    // Switch / with /index.html
    if (filepath.back() == '/') {
        filepath += "index.html";
    }

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
            filepath = "/" AD2_USD_MOUNT_POINT WEBUI_DOC_ROOT "/404.html";
            if (stat(filepath.c_str(), &file_stat) != 0) {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found.<br>Connect a uSD card with a FAT32 partition and the html content in the root directory before the device starts.");
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
        int sockfd = httpd_req_to_sockfd(req);

        // build standard template values FIXME: function dynamic.
        // Version MACRO ${0}
        std::string szVersion = "1.0";

        // Time MACRO ${1}
        std::string szTime;
        uptimeString(szTime);

        // socket Local address string MACRO ${2}
        std::string szLocalIP;
        hal_get_socket_local_ip(sockfd, szLocalIP);

        // socket Client address string MACRO ${3}
        std::string szClientIP;
        hal_get_socket_client_ip(sockfd, szClientIP);

        // Request protocol MACRO ${4}
        std::string szProt = "HTTP"; // (req->isSecure() ? "HTTPS" : "HTTP");

        // UUID MACRO ${5}
        std::string szUUID;
        ad2_genUUID(0x0, szUUID);

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
 * @param [in]s AD2PartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void webui_on_state_change(std::string *msg, AD2PartitionState *s, void *arg)
{
#if CONFIG_HTTPD_WS_SUPPORT
#if defined(DEBUG_WEBUI)
    ESP_LOGI(TAG, "webui_on_state_change partition(%i) event(%s) message('%s')", s->partition, AD2Parse.event_str[(int)arg].c_str(), msg->c_str());
#endif
    size_t fds = server_config.max_open_sockets;
    int client_fds[fds];
    if (server && hal_get_network_connected()) {
        httpd_get_client_list(server, &fds, client_fds);
        for (int i=0; i<fds; i++) {
            if (httpd_ws_get_fd_info(server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                struct ws_session_storage *sess = (ws_session_storage *)httpd_sess_get_ctx(server, client_fds[i]);
                if (sess) {
                    // get the partition state based upon the partition requested.
                    AD2PartitionState *temps = ad2_get_partition_state(sess->partID);
                    if (temps && s->partition == temps->partition) {
                        cJSON *root = ad2_get_partition_state_json(s);
                        cJSON *zone_alerts = ad2_get_partition_zone_alerts_json(s);
                        cJSON_AddStringToObject(root, "event", AD2Parse.event_str[(int)arg].c_str());
                        cJSON_AddItemToObject(root, "zone_alerts", zone_alerts);
                        char *sys_info = cJSON_Print(root);
                        cJSON_Minify(sys_info);
                        httpd_ws_frame_t ws_pkt;
                        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                        ws_pkt.payload = (uint8_t*)sys_info;
                        ws_pkt.len = strlen(sys_info);
                        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                        httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
                        cJSON_free(sys_info);
                        cJSON_Delete(root);
                    }
                }
            }
        }
    }
#endif
}

/**
 * @brief callback after accept before any R/W on a client.
 * Test if we want this connection return ESP_OK to keep ESP_FAIL to reject.
 */
esp_err_t http_acl_test(httpd_handle_t hd, int sockfd)
{
    std::string IP;
    // Get a string for the client address connected to this IP.
    hal_get_socket_client_ip(sockfd, IP);
    /* ACL test */
    if (!webui_acl.find(IP)) {
        ESP_LOGW(TAG, "Rejecting client connection from '%s'", IP.c_str());
        // FIXME: Not able to make it close clean. If I dont send something the the browser
        // will try again. Sending here is a problem because it will close before sending yet it helps?
        std::string Err = "HTTP/1.0 403 Forbidden\r\n\r\n\r\nAccess denied check ACL list\r\n";
        send(sockfd, Err.c_str(), Err.length(), 0);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief webui server task
 *
 * @param [in]pvParameters currently not used NULL.
 */
void webui_server_task(void *pvParameters)
{
    esp_err_t err;
#if defined(FTPD_DEBUG)
     ESP_LOGI(TAG, "%s waiting for network layer to start.", TAG);
#endif
    while (1) {
        if (!hal_get_netif_started()) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        } else {
            break;
        }
    }
    ESP_LOGI(TAG, "Network layer OK. %s daemon service starting.", TAG);

    // Configure the web server and handlers.
    server_config.uri_match_fn = httpd_uri_match_wildcard;
    server_config.open_fn = http_acl_test;
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

    for (;;) {
        if (hal_get_network_connected() && server==nullptr) {

            // Start the httpd server and handlers.
            if ((err = httpd_start(&server, &server_config)) == ESP_OK) {
                // Set URI handlers
#if CONFIG_HTTPD_WS_SUPPORT
                httpd_register_uri_handler(server, &ad2ws_server);
#endif
                httpd_register_uri_handler(server, &file_server);
            } else {
                // error long 10s sleep.
                ESP_LOGW(TAG, "Error calling httpd_start [%s]", esp_err_to_name(err));
                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
        } else {
            // network down
            if (!hal_get_network_connected() && server!=nullptr) {
                taskENTER_CRITICAL(&spinlock);
                httpd_handle_t ts = server;
                server = nullptr;
                taskEXIT_CRITICAL(&spinlock);
                err = httpd_stop(ts);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Error calling httpd_start [%s]", esp_err_to_name(err));
                }
            }
            // short 1s sleep
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

/**
 * WebUI generic command event processing
 *  command: [COMMAND] <id> <arg>
 * ex.
 *   [COMMAND] 0 arg...
 */
static void _cli_cmd_webui_event(const char *string)
{

    // key value validation
    std::string cmd;
    ad2_copy_nth_arg(cmd, string, 0);
    ad2_lcase(cmd);

    if(cmd.compare(WEBUI_COMMAND) != 0) {
        ad2_printf_host(false, "What?\r\n");
        return;;
    }

    // key value validation
    std::string subcmd;
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    int i;
    for(i = 0;; ++i) {
        if (WEBUI_SUBCMD[i] == 0) {
            ad2_printf_host(false, "What?\r\n");
            break;
        }
        if(subcmd.compare(WEBUI_SUBCMD[i]) == 0) {
            std::string arg;
            std::string acl;
            switch(i) {
            /**
             * Enable/Disable WebUI daemon.
             */
            case WEBUI_SUBCMD_ENABLE_ID:
                if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
                    ad2_set_config_key_bool(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ENABLE, (arg[0] == 'Y' || arg[0] ==  'y'));
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                }

                {
                    // show contents of this slot
                    bool en = false;
                    ad2_get_config_key_bool(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ENABLE, &en);
                    ad2_printf_host(false, "WebUI daemon is '%s'.\r\n", (en ? "Enabled" : "Disabled"));
                }
                break;
            /**
             * WebUI daemon IP/CIDR ACL list.
             */
            case WEBUI_SUBCMD_ACL_ID:
                // If no arg then return ACL list
                if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
                    webui_acl.clear();
                    int res = webui_acl.add(arg);
                    if (res == webui_acl.ACL_FORMAT_OK) {
                        ad2_set_config_key_string(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ACL, arg.c_str());
                    } else {
                        ad2_printf_host(false, "Error parsing ACL string. Check ACL format. Not saved.\r\n");
                    }
                }
                // show contents of this slot set default to allow all
                acl = WEBUI_DEFAULT_ACL;
                ad2_get_config_key_string(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ACL, acl);
                ad2_printf_host(false, WEBUI_COMMAND " 'acl' set to '%s'.\r\n", acl.c_str());
                break;
            default:
                break;
            }
            break;
        }
    }
}

/**
 * @brief command list for module
 */
static struct cli_command webui_cmd_list[] = {
    {
        (char*)WEBUI_COMMAND,(char*)
        "Usage: webui <command> [arg]\r\n"
        "\r\n"
        "    Configuration tool for WebUI server\r\n"
        "Commands:\r\n"
        "    enable [Y|N]            Set or get enable flag\r\n"
        "    acl [aclString|-]       Set or get ACL CIDR CSV list\r\n"
        "                            use - to delete\r\n"
        "Examples:\r\n"
        "    ```webui enable Y```\r\n"
        "    ```webui acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4```\r\n"
        , _cli_cmd_webui_event
    }
};

/**
 * @brief Register componet cli commands.
 */
void webui_register_cmds()
{
    // Register webui CLI commands
    for (int i = 0; i < ARRAY_SIZE(webui_cmd_list); i++) {
        cli_register_command(&webui_cmd_list[i]);
    }
}

/**
 * @brief AD2IoT Component webUI init
 *
 */
void webui_init(void)
{
    bool en = -1;
    ad2_get_config_key_bool(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ENABLE, &en);

    // nothing more needs to be done once commands are set if not enabled.
    if (!en) {
        ad2_printf_host(true, "%s: daemon disabled.", TAG);
        return;
    }

    // load and parse ACL if set or set default to allow all.
    std::string acl = WEBUI_DEFAULT_ACL;

    ad2_get_config_key_string(WEBUI_CONFIG_SECTION, WEBUI_SUBCMD_ACL, acl);
    if (acl.length()) {
        int res = webui_acl.add(acl);
        if (res != webui_acl.ACL_FORMAT_OK) {
            ESP_LOGW(TAG, "ACL parse error %i for '%s'", res, acl.c_str());
        }
    }

    // Subscribe to AlarmDecoder events
    AD2Parse.subscribeTo(ON_ARM, webui_on_state_change, (void *)ON_ARM);
    AD2Parse.subscribeTo(ON_DISARM, webui_on_state_change, (void *)ON_DISARM);
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, webui_on_state_change, (void *)ON_CHIME_CHANGE);
    AD2Parse.subscribeTo(ON_BEEPS_CHANGE, webui_on_state_change, (void *)ON_BEEPS_CHANGE);
    AD2Parse.subscribeTo(ON_FIRE_CHANGE, webui_on_state_change, (void *)ON_FIRE_CHANGE);
    AD2Parse.subscribeTo(ON_POWER_CHANGE, webui_on_state_change, (void *)ON_POWER_CHANGE);
    AD2Parse.subscribeTo(ON_READY_CHANGE, webui_on_state_change, (void *)ON_READY_CHANGE);
    AD2Parse.subscribeTo(ON_LOW_BATTERY, webui_on_state_change, (void *)ON_LOW_BATTERY);
    AD2Parse.subscribeTo(ON_ALARM_CHANGE, webui_on_state_change, (void *)ON_ALARM_CHANGE);
    AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, webui_on_state_change, (void *)ON_ZONE_BYPASSED_CHANGE);
    AD2Parse.subscribeTo(ON_EXIT_CHANGE, webui_on_state_change, (void *)ON_EXIT_CHANGE);
    // SUbscribe to ON_ZONE_CHANGE events
    AD2Parse.subscribeTo(ON_ZONE_CHANGE, webui_on_state_change, (void *)ON_ZONE_CHANGE);

    ad2_printf_host(true, "%s: Init done, daemon starting.", TAG);
    xTaskCreate(&webui_server_task, "AD2 webUI", 1024*5, NULL, tskIDLE_PRIORITY+1, NULL);
}

#endif /*  CONFIG_AD2IOT_WEBSERVER_UI */
