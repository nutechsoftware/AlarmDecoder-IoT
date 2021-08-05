/**
*  @file    alarmdecoder_main.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    02/20/2020
*  @version 1.0.3
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_tls.h"
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

// ser2sockd support
#if CONFIG_AD2IOT_SER2SOCKD
#include "ser2sock.h"
#endif

// twilio support
#if CONFIG_AD2IOT_TWILIO_CLIENT
#include "twilio.h"
#endif

// twilio support
#if CONFIG_AD2IOT_PUSHOVER_CLIENT
#include "pushover.h"
#endif

// web server UI support
#if CONFIG_AD2IOT_WEBSERVER_UI
#include "webUI.h"
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
*  0 : running the main function.
*  1 : halted waiting for timeout to auto resume.
*  2 : halted.
*/
int g_StopMainTask = 0;

portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// global AlarmDecoder parser class instance
AlarmDecoderParser AD2Parse;

// global AD2 device connection fd/id <socket or uart id>
int g_ad2_client_handle = -1;

// global ad2 connection mode ['S'ocket | 'C'om port]
uint8_t g_ad2_mode = 0;

// global ad2 network EventGroup
EventGroupHandle_t g_ad2_net_event_group = nullptr;

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
    ESP_LOGI(TAG, "RAM left %d min %d maxblk %d", esp_get_free_heap_size(),esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
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
    ESP_LOGI(TAG, "ON_READY_CHANGE: READY(%i) EXIT(%i) STAY(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_stay, s->armed_away);
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
    ESP_LOGI(TAG, "ON_ARM: READY(%i) EXIT(%i) STAY(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_stay, s->armed_away);
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
 * @brief ON_LOW_BATTERY
 * Called when a low battery state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear LOW BATTERY ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 */
void my_ON_LOW_BATTERY_CB(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_LOW_BATTERY_CB: BATTERY(%i)", s->battery_low);
}

#if defined(CONFIG_AD2IOT_SER2SOCKD)
/**
 * @brief ON_RAW_RX_DATA
 * Called when data is sent into the parser.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2VirtualPartitionState updated partition state for message.
 *
 * @note this is done before parsing.
 */
void SER2SOCKD_ON_RAW_RX_DATA(uint8_t *buffer, size_t s, void *arg)
{
    ser2sockd_sendall(buffer, s);
}
#endif

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
        // do not process if main halted or network disconnected.
        if (!g_StopMainTask && hal_get_network_connected()) {
            memset(rx_buffer, 0, AD2_UART_RX_BUFF_SIZE);

            // Read data from the UART
            int len = uart_read_bytes((uart_port_t)g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);
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
        if (hal_get_network_connected()) {
            // load settings from NVS
            // host stored in slot 1
            std::string buf;
            ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                                       AD2MODE_CONFIG_ARG_SLOT, nullptr, buf);

            std::vector<std::string> out;
            ad2_tokenize(buf, ':', out);

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
                ESP_LOGE(TAG, "Error parsing host:port from settings '%s'", buf.c_str());
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
                if (!hal_get_network_connected()) {
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
 * @brief Start ser2sock client task
 */
void init_ser2sock_client()
{
    xTaskCreate(ser2sock_client_task, "ser2sock_client", 4096, (void*)AF_INET, tskIDLE_PRIORITY+2, NULL);
}

/**
 *  @brief Initialize the uart connected to the AD2 device
 */
void init_ad2_uart_client()
{
    // load settings from NVS
    // host stored in slot 1
    std::string port_pins;
    ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                               AD2MODE_CONFIG_ARG_SLOT, nullptr, port_pins);
    g_ad2_client_handle = UART_NUM_2;
    std::vector<std::string> out;
    ad2_tokenize(port_pins, ':', out);
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

    // esp_restart causes issues with UART2 don't set config if reset switch pushed.
    // https://github.com/espressif/esp-idf/issues/5274
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
    if (esp_reset_reason() != ESP_RST_SW) {
#endif
        uart_param_config((uart_port_t)g_ad2_client_handle, uart_config);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
    }
#endif

    uart_set_pin((uart_port_t)g_ad2_client_handle,
                 tx_pin,   // TX
                 rx_pin,   // RX
                 UART_PIN_NO_CHANGE, // RTS
                 UART_PIN_NO_CHANGE);// CTS

    uart_driver_install((uart_port_t)g_ad2_client_handle, MAX_UART_LINE_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);
    free(uart_config);

    xTaskCreate(ad2uart_client_task, "ad2uart_client", 4096, (void *)AF_INET, tskIDLE_PRIORITY + 2, NULL);

}

/**
 * @brief main() app main entrypoint
 */
void app_main()
{

    //// AlarmDecoder App main

    // start with log level error.
    esp_log_level_set("*", ESP_LOG_NONE);        // set all components to ERROR level

    // init the AD2IoT gpio
    hal_gpio_init();

    // init host(USB) uart port
    hal_host_uart_init();

    ad2_printf_host(AD2_SIGNON, FIRMWARE_VERSION);

    ESP_ERROR_CHECK(esp_tls_init_global_ca_store());

    // Dump hardware info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ad2_printf_host("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
                    chip_info.cores,
                    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ad2_printf_host("silicon revision %d, ", chip_info.revision);

    ad2_printf_host("%dMB %s flash\r\n", spi_flash_get_chip_size() / (1024 * 1024),
                    (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // Initialize nvs partition for key value storage.
    ad2_printf_host("Initialize NVS subsystem start.");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ad2_printf_host(" Not found or error clearing flash.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ad2_printf_host(" Done.\r\n");
    ESP_ERROR_CHECK( err );

    // Example of nvs_get_stats() to get the number of used entries and free entries:
    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);
    ad2_printf_host("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\r\n",
                    nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);

    // load and set the logging level.
    ad2_set_log_mode(ad2_log_mode());

    // create event group
    g_ad2_net_event_group = xEventGroupCreate();

    // init the virtual partition database from NV storage
    // see iot_cli_cmd::vpart
    // Address 0 is reserved for system partition
    for (int n = 0; n <= AD2_MAX_VPARTITION; n++) {
        int x = -1;
        ad2_get_nv_slot_key_int(VPART_CONFIG_KEY, n, 0, &x);
        // if we found a NV record then initialize the AD2PState for the mask.
        if (x != -1) {
            AD2Parse.getAD2PState(x, true);
            ESP_LOGI(TAG, "init vpart slot %i mask address %i", n, x);
        }
    }

#if CONFIG_STDK_IOT_CORE
    // Register STSDK CLI commands.
    stsdk_register_cmds();
#endif

#if CONFIG_AD2IOT_SER2SOCKD
    // Register ser2sock daemon CLI commands.
    ser2sockd_register_cmds();
#endif

#if CONFIG_AD2IOT_TWILIO_CLIENT
    // Register TWILIO CLI commands.
    twilio_register_cmds();
#endif

#if CONFIG_AD2IOT_PUSHOVER_CLIENT
    // Register PUSHOVER CLI commands.
    pushover_register_cmds();
#endif

    // Register AD2 CLI commands.
    register_ad2_cli_cmd();

    // Load AD2IoT operating mode [Socket|UART] and argument
    std::string ad2_mode;
    ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY,
                               AD2MODE_CONFIG_MODE_SLOT, nullptr, ad2_mode);

    // Load AD2 connection type Com|Socket
    g_ad2_mode = ad2_mode[0];
    // If the hardware is local UART start it now.
    if (g_ad2_mode == 'C') {
        init_ad2_uart_client();
    }

    // Start the CLI.
    // Press "..."" to halt startup and stay if a safe mode command line only.
    uart_cli_main();

    if (g_ad2_mode == 'S') {
        ad2_printf_host("Delaying start of ad2source SOCKET after network is up.\r\n");
    } else if(g_ad2_mode != 'C') {
        ESP_LOGI(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
        ad2_printf_host("AlarmDecoder protocol source mode NOT configured. Configure using ad2source command.\r\n");
    }

#if CONFIG_STDK_IOT_CORE
    int stEN = -1;
    ad2_get_nv_slot_key_int(STSDK_ENABLE, 0, nullptr, &stEN);
    if (stEN == -1) {
        // Enable STSDK if no setting found.
        ESP_LOGI(TAG,"STSDK enable setting not found. Saving new enabled by default.");
        ad2_set_nv_slot_key_int(STSDK_ENABLE, 0, nullptr, 1);
    }
#endif

    // get the network mode set default mode to 'N'
    std::string netmode_args;
    char net_mode = ad2_network_mode(netmode_args);
    ad2_printf_host("AD2IoT 'netmode' set to '%c'.\r\n", net_mode);

    /**
     * Start the network TCP/IP driver stack if Ethernet or Wifi enabled.
     */
    if ( net_mode != 'N') {
        hal_init_network_stack();
    }

    /**
     * Start the network hardware driver(s)
     *
     * FIXME: SmartThings needs to manage the Wifi during adopting.
     * For now just one interface more is much more complex.
     */
#if CONFIG_AD2IOT_USE_ETHERNET
    if ( net_mode == 'E') {
        // Init the hardware ethernet driver and hadware
        hal_init_eth(netmode_args);
    }
#endif
#if CONFIG_AD2IOT_USE_WIFI
    if ( net_mode == 'W') {
        // Init the wifi driver and hardware
        hal_init_wifi(netmode_args);
    }
#endif

#if 1 // FIXME add build switch for release builds.
    // AlarmDecoder callback wire up for testing.
    AD2Parse.subscribeTo(ON_MESSAGE, my_ON_MESSAGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_LRR, my_ON_LRR_CB, nullptr);
    AD2Parse.subscribeTo(ON_ARM, my_ON_ARM_CB, nullptr);
    AD2Parse.subscribeTo(ON_DISARM, my_ON_DISARM_CB, nullptr);
    AD2Parse.subscribeTo(ON_READY_CHANGE, my_ON_READY_CHANGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, my_ON_CHIME_CHANGE_CB, nullptr);
    AD2Parse.subscribeTo(ON_FIRE, my_ON_FIRE_CB, nullptr);
    AD2Parse.subscribeTo(ON_LOW_BATTERY, my_ON_LOW_BATTERY_CB, nullptr);
#endif

    // Start components
#if CONFIG_STDK_IOT_CORE
    // Disable stsdk if network mode is not N
    if ( net_mode == 'N') {
        /**
         * Initialize SmartThings SDK
         *
         *  WARNING: If enabled it will consume the esp_event_loop_init and
         * only one task can create this.
         */
        stsdk_init();
        // Kick off a connect to SmartThings server to get things started.
        stsdk_connection_start();
    } else {
        ESP_LOGI(TAG, "'netmode' <> 'N' disabling SmartThings.");
        ad2_printf_host("'netmode' <> 'N' disabling SmartThings.\r\n");
    }
#endif
#if CONFIG_AD2IOT_TWILIO_CLIENT
    // Initialize twilio client
    twilio_init();
#endif
#if CONFIG_AD2IOT_PUSHOVER_CLIENT
    // Initialize pushover client
    pushover_init();
#endif
#if CONFIG_AD2IOT_WEBSERVER_UI
    // Initialize WEB SEVER USER INTERFACE
    webUI_init();
#endif

    // Sleep for another 5 seconds. Hopefully wifi is up before we continue connecting the AD2*.
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Start main AlarmDecoder IoT app task
    xTaskCreate(ad2_app_main_task, "ad2_app_main_task", 4096, NULL, tskIDLE_PRIORITY+1, NULL);

    // Start firmware update task
    ota_init();

    // If the AD2* is a socket connection we can hopefully start it now.
    if (g_ad2_mode == 'S') {
        init_ser2sock_client();
    }

#if defined(CONFIG_AD2IOT_SER2SOCKD)
    // init ser2sock server
    ser2sockd_init();
    AD2Parse.subscribeTo(SER2SOCKD_ON_RAW_RX_DATA, nullptr);
#endif

}

#ifdef __cplusplus
} // extern "C"
#endif

