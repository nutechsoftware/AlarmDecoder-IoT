/**
*  @file    alarmdecoder_main.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    02/20/2020
*  @version 1.0.1
*
*  @brief AlarmDecoder IoT embedded network appliance
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


/**
 * AlarmDecoder Arduino library.
 * https://github.com/nutechsoftware/ArduinoAlarmDecoder
 */
#include <alarmdecoder_api.h>

// esp includes
#include "driver/uart.h"
#include <lwip/netdb.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

// OTA updates
#include "ota_util.h"

// UART CLI
#include "ad2_uart_cli.h"
#include "ad2_cli_cmd.h"

// Common settings
#include "ad2_settings.h"

// HAL
#include "device_control.h"

// AD2IoT include
#include "alarmdecoder_main.h"

// common utils
#include "ad2_utils.h"

// SmartThings direct attached device support
#if CONFIG_STDK_IOT_CORE
#include "stsdk_main.h"
#endif

// twilio support
#if CONFIG_TWILIO_CLIENT
#include "twilio.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constants / Static / Extern
 */
static const char *TAG = "AD2_IoT";

/**
* Control main task processing
*  0 : running the main function
*  1 : stop for a timeout
*  2 : stop before selecting the go_main function.
*/
int g_StopMainTask = 0;

portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// global AlarmDecoder parser class instance
AlarmDecoderParser AD2Parse;

// global AD2 device connection fd/id <socket or uart id>
int g_ad2_client_handle = -1;

// global ad2 connection mode ['S'ocket | 'C'om port]
uint8_t g_ad2_mode = 0;

// global network connection state
int g_ad2_network_state = AD2_OFFLINE;

// Device LED mode
int noti_led_mode = LED_ANIMATION_MODE_IDLE;

/**
 * AlarmDecoder callbacks.
 * As the AlarmDecoder receives data via put() data is validated.
 * When a complete messages is received or a specific stream of
 * bytes is received event(s) will be called.
 */

/**
 * @brief ON_MESSAGE
 * Called when a full standard alarm state message is received
 * but before it is parsed so the virtual state will be based upon
 * the last message parsed.
 *
 * @note WARNING the message may be invalid.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_MESSAGE_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "MESSAGE_CB: '%s'", msg->c_str());
    ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());
}

/**
 * @brief ON_LRR
 * Called when a LRR message is received.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_LRR_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "LRR_CB: %s",msg->c_str());
}

/**
 * @brief ON_READY_CHANGE
 * Called when the READY state change event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_READY_CHANGE_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_READY_CHANGE: READY(%i) EXIT(%i) HOME(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_home, s->armed_away);
}

/**
 * @brief ON_ARM
 * Called when a ARM event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_ARM_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_ARM: READY(%i) EXIT(%i) HOME(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_home, s->armed_away);
}

/**
 * @brief ON_DISARM
 * Called when a DISARM event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_DISARM_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_DISARM: READY(%i)", s->ready);
}

/**
 * @brief ON_CHIME_CHANGE
 * Called when CHIME state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear Chime ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_CHIME_CHANGE_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_CHIME_CHANGE: CHIME(%i)", s->chime_on);
}

/**
 * @brief ON_FIRE
 * Called when FIRE state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear FIRE ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_FIRE_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_FIRE_CB: FIRE(%i)", s->fire_alarm);
}

/**
 * @brief Main task to monitor physical button(s) and update state led(s).
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ad2_app_main_task(void *pvParameters)
{
    int button_event_type;
    int button_event_count;

    for (;;) {
        // do not process if main halted.
        if (!g_StopMainTask) {
            if (hal_get_button_event(&button_event_type, &button_event_count)) {
                // FIXME: update stsdk virtual button state
            }
            if (noti_led_mode != LED_ANIMATION_MODE_IDLE) {
                hal_change_led_mode(noti_led_mode);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

/**
 * @brief ad2uart client task
 * process messages from local UART connected to the AD2*
 * device.
 *
 * @note Hardware uart may have noise during flashing or rebooting.
 * To help we will send down a bunch of line breaks to force the AD2*
 * into run mode. This is not necessary with socket connected AD2*.
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ad2uart_client_task(void *pvParameters)
{
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];

    // send break to AD2* be sure we are in run mode.
    std::string breakline = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";
    uart_write_bytes((uart_port_t)g_ad2_client_handle, breakline.c_str(), breakline.length());

    while (1) {
        // do not process if main halted.
        if (!g_StopMainTask) {
            memset(rx_buffer, 0, AD2_UART_RX_BUFF_SIZE);

            // Read data from the UART
            int len = uart_read_bytes((uart_port_t)g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 10 / portTICK_RATE_MS);
            if (len == -1) {
                // An error happend. Sleep for a bit and try again?
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
            if (len>0) {
                AD2Parse.put(rx_buffer, len);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/**
 * @brief ser2sock client task
 * Connects and stays connected to ser2sock server to receive
 * AD2* protocol messages from an alarm system.
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ser2sock_client_task(void *pvParameters)
{

    while (1) {
        if (g_ad2_network_state == AD2_CONNECTED) {
            // load settings from NVS
            // host stored in slot 1
            char buf[AD2_MAX_MODE_ARG_SIZE] = {0};
            ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                       AD2MODE_CONFIG_ARG_SLOT, buf, sizeof(buf));

            std::vector<std::string> out;
            ad2_tokenize(std::string(buf), ':', out);

            // data sanity tests
            bool connectok = false;
            if (out[0].length() > 0) {
                connectok = true;
            }
            int port = atoi(out[1].c_str());
            if (port > 0) {
                connectok = true;
            }
            std::string host = out[0];

            // looks good. connect.
            if (connectok) {
                ESP_LOGI(TAG, "Connecting to ser2sock host %s:%i", host.c_str(), port);
            } else {
                ESP_LOGE(TAG, "Error parsing host:port from settings '%s'", buf);
            }

            int addr_family = 0;
            int ip_protocol = 0;

#if defined(CONFIG_AD2IOT_SER2SOCK_IPV4)
            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(host.c_str());
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_AD2IOT_SER2SOCK_IPV6)
            struct sockaddr_in6 dest_addr = { 0 };
            inet6_aton(host.c_str(), &dest_addr.sin6_addr);
            dest_addr.sin6_family = AF_INET6;
            dest_addr.sin6_port = htons(port);
            dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
            addr_family = AF_INET6;
            ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
            struct sockaddr_in6 dest_addr = { 0 };
            ESP_ERROR_CHECK(get_addr_from_stdin(port, SOCK_STREAM, &ip_protocol, &addr_family, &dest_addr));
#endif
            g_ad2_client_handle =  socket(addr_family, SOCK_STREAM, ip_protocol);
            if (g_ad2_client_handle < 0) {
                ESP_LOGE(TAG, "ser2sock client unable to create socket: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "ser2sock client socket created, connecting to %s:%d", host.c_str(), port);

            int err = connect(g_ad2_client_handle, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
            if (err != 0) {
                ESP_LOGE(TAG, "ser2sock client socket unable to connect: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "ser2sock client successfully connected");

            // set socket non blocking.
            fcntl(g_ad2_client_handle, F_SETFL, O_NONBLOCK);

            while (1) {
                // do not process if main halted.
                if (!g_StopMainTask) {
                    uint8_t rx_buffer[128];
                    int len = recv(g_ad2_client_handle, rx_buffer, sizeof(rx_buffer) - 1, 0);
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
                        AD2Parse.put(rx_buffer, len);
                    }
                }
                if (g_ad2_network_state != AD2_CONNECTED) {
                    break;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            if (g_ad2_client_handle != -1) {
                ESP_LOGE(TAG, "ser2sock client shutting down socket and restarting in 3 seconds.");
                shutdown(g_ad2_client_handle, 0);
                close(g_ad2_client_handle);
                g_ad2_client_handle = -1;
                vTaskDelay(3000 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/**
 * @brief helper do_retransmit for ser2sock_server_task
 *
 * @param [in]sock socket fd
 */
inline void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[128];

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "ser2sock server error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "ser2sock server connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "ser2sock server received %d bytes: %s", len, rx_buffer);

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0) {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "ser2sock server error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}

/**
 * @brief ser2sock server task
 *
 * @param [in]pvParameters currently not used NULL.
 */
#ifdef SER2SOCK_SERVER
static void ser2sock_server_task(void *pvParameters)
{
    char addr_str[128];
    // int addr_family = (int)pvParameters;
    int addr_family = AF_INET;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    ESP_LOGI(TAG, "ser2sock server task starting.");

    for (;;) {
        if (g_ad2_network_state == AD2_CONNECTED) {
            ESP_LOGI(TAG, "ser2sock server starting.");
            if (addr_family == AF_INET) {
                struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
                dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
                dest_addr_ip4->sin_family = AF_INET;
                dest_addr_ip4->sin_port = htons(SER2SOCK_SERVER_PORT);
                ip_protocol = IPPROTO_IP;
            } else if (addr_family == AF_INET6) {
                bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
                dest_addr.sin6_family = AF_INET6;
                dest_addr.sin6_port = htons(SER2SOCK_SERVER_PORT);
                ip_protocol = IPPROTO_IPV6;
            }

            int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
            if (listen_sock < 0) {
                ESP_LOGE(TAG, "ser2sock server unable to create socket: errno %d", errno);
                vTaskDelete(NULL);
                return;
            }
#if defined(SER2SOCK_IPV4) && defined(SER2SOCK_IPV6)
            // Note that by default IPV6 binds to both protocols, it is must be disabled
            // if both protocols used at the same time (used in CI)
            int opt = 1;
            setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

            ESP_LOGI(TAG, "ser2sock server socket created");

            int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err != 0) {
                ESP_LOGE(TAG, "ser2sock server socket unable to bind: errno %d", errno);
                ESP_LOGE(TAG, "ser2sock server IPPROTO: %d", addr_family);
                goto CLEAN_UP;
            }
            ESP_LOGI(TAG, "ser2sock server socket bound, port %d", SER2SOCK_SERVER_PORT);

            err = listen(listen_sock, 1);
            if (err != 0) {
                ESP_LOGE(TAG, "ser2sock server error occurred during listen: errno %d", errno);
                goto CLEAN_UP;
            }

            while (1) {

                ESP_LOGI(TAG, "ser2sock server socket listening");

                struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
                uint addr_len = sizeof(source_addr);
                int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
                if (sock < 0) {
                    ESP_LOGE(TAG, "ser2sock server unable to accept connection: errno %d", errno);
                    break;
                }

                // Convert ip address to string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }
                ESP_LOGI(TAG, "ser2sock server socket accepted ip address: %s", addr_str);

                do_retransmit(sock);

                shutdown(sock, 0);
                close(sock);
            }

CLEAN_UP:
            close(listen_sock);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif

/**
 * @brief Start ser2sock client task
 */
void init_ser2sock_client()
{
    xTaskCreate(ser2sock_client_task, "ser2sock_client", 4096, (void*)AF_INET, tskIDLE_PRIORITY+2, NULL);
}

/**
 * @brief Start ser2sock server task
 */
#if defined(AD2_SER2SOCK_SERVER)
void init_ser2sock_server()
{
    xTaskCreate(ser2sock_server_task, "ser2sock_server", 4096, (void*)AF_INET, tskIDLE_PRIORITY+2, NULL);
}
#endif

/**
 *  @brief Initialize the uart connected to the AD2 device
 */
void init_ad2_uart_client()
{
    // load settings from NVS
    // host stored in slot 1
    char port_pins[20];
    ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                               AD2MODE_CONFIG_ARG_SLOT, port_pins, sizeof(port_pins));
    g_ad2_client_handle = UART_NUM_2;
    std::vector<std::string> out;
    ad2_tokenize(std::string(port_pins), ':', out);
    ESP_LOGI(TAG, "Initialize AD2 UART client using txpin(%s) rxpin(%s)", out[0].c_str(),out[1].c_str());
    int tx_pin = atoi(out[0].c_str());
    int rx_pin = atoi(out[1].c_str());

    // Configure parameters of an UART driver,
    uart_config_t* uart_config = (uart_config_t*)calloc(sizeof(uart_config_t), 1);

    uart_config->baud_rate = 115200;
    uart_config->data_bits = UART_DATA_8_BITS;
    uart_config->parity    = UART_PARITY_DISABLE;
    uart_config->stop_bits = UART_STOP_BITS_1;
    uart_config->flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config->rx_flow_ctrl_thresh = 1;

    uart_param_config((uart_port_t)g_ad2_client_handle, uart_config);

    uart_set_pin((uart_port_t)g_ad2_client_handle,
                 tx_pin,   // TX
                 rx_pin,   // RX
                 UART_PIN_NO_CHANGE, // RTS
                 UART_PIN_NO_CHANGE);// CTS

    uart_driver_install((uart_port_t)g_ad2_client_handle, AD2_UART_RX_BUFF_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);
    free(uart_config);

    xTaskCreate(ad2uart_client_task, "ad2uart_client", 4096, (void *)AF_INET, tskIDLE_PRIORITY + 2, NULL);

}

/**
 * @brief main() app main entrypoint
 */
void app_main()
{

    //// AlarmDecoder App main

    // init the AD2IoT gpio
    hal_gpio_init();

    // Dump hardware info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // Initialize nvs partition for key value storage.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "truncating nvs partition");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
        ESP_LOGI(TAG, "nvs_flash_init done");
    }
    ESP_ERROR_CHECK( err );

    // Register and start the AD2IOT cli early so we can stop init by hitting enter.
    register_ad2_cli_cmd();

    /**
     * FIXME: SmartThings needs to manage the Wifi during adopting.
     * FIXME: For now just one interface more is much more complex.
     */
#if CONFIG_AD2IOT_ETHERNET
    // Init the hardware ethernet module
    init_eth();
#endif
#if CONFIG_AD2IOT_WIFI
    // Init the wifi module
    init_wifi();
#endif

    // init the virtual partition database from NV storage
    // see iot_cli_cmd::vpaddr
    for (int n = 0; n <= AD2_MAX_VPARTITION; n++) {
        int x = -1;
        ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, n, &x);
        // if we found a NV record then initialize the AD2PState for the mask.
        if (x != -1) {
            uint32_t amask = 1;
            amask <<= x-1;
            AD2Parse.getAD2PState(&amask, true);
            ESP_LOGI(TAG, "init vpaddr slot %i mask %i", n, x);
        }
    }

#if 1
    // AlarmDecoder callback wire up for testing.
    AD2Parse.subscribeTo(ON_MESSAGE, my_ON_MESSAGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_LRR, my_ON_LRR_CB, nullptr);
    AD2Parse.subscribeTo(ON_ARM, my_ON_ARM_CB, nullptr);
    AD2Parse.subscribeTo(ON_DISARM, my_ON_DISARM_CB, nullptr);
    AD2Parse.subscribeTo(ON_READY_CHANGE, my_ON_READY_CHANGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, my_ON_CHIME_CHANGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_FIRE, my_ON_FIRE_CB, nullptr);
#endif

    // Load AD2IoT operating mode [Socket|UART] and argument
    // get the mode
    char mode[2];
    ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                               AD2MODE_CONFIG_MODE_SLOT, mode, sizeof(mode));
    g_ad2_mode = mode[0];

    // init the AlarmDecoder UART
    if (g_ad2_mode == 'C') {
        init_ad2_uart_client();
    } else if (g_ad2_mode == 'S') {
        init_ser2sock_client();
    } else {
        ESP_LOGW(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
        printf("AD2IoT operating mode configured. Configure using ad2source command.\n");
    }

#if defined(AD2_SER2SOCK_SERVER)
    // init ser2sock server
    init_ser2sock_server();
#endif

#if CONFIG_TWILIO_CLIENT
    // Initialize twilio client
    twilio_init();
#endif

#if CONFIG_STDK_IOT_CORE
    // Initialize SmartThings SDK
    stsdk_init();
#endif

    // Start the CLI.
    // Press ENTER to halt and stay in CLI only.
    uart_cli_main();

    // Start main AlarmDecoder IoT app task
    xTaskCreate(ad2_app_main_task, "ad2_app_main_task", 4096, NULL, tskIDLE_PRIORITY+1, NULL);

    // Start firmware update task
    ota_init();

#if CONFIG_STDK_IOT_CORE
    // connect to SmartThings server
    connection_start();
#endif
}

#ifdef __cplusplus
} // extern "C"
#endif

