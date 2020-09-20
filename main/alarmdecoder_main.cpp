 /**
 *  @file    alarmdecoder_main.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *  @version 1.0
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

#ifdef __cplusplus
extern "C" {
#endif

// esp includes
#include "driver/uart.h"
#include <lwip/netdb.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

// OTA updates
#include "ota_util.h"

// CLI interface
#include "iot_uart_cli.h"
#include "iot_cli_cmd.h"

// common settings
#include "ad2_settings.h"

// AD2IoT include
#include "alarmdecoder_main.h"

// common utils
#include "ad2_utils.h"

// SmartThings direct attached device support
#ifdef CONFIG_STDK_IOT_CORE
#include "stsdk_main.h"
#endif

// twilio support
#if CONFIG_TWILIO_CLIENT
#include "twilio.h"
#endif

/**
 * Constants / Static / Extern
 */
static const char *TAG = "AD2_IoT";

// global AlarmDecoder parser class instance
static AlarmDecoderParser AD2Parse;

// AD2 device connection settings
int g_ad2_client_handle = -1;
uint8_t g_ad2_mode = 0;

// Device LED mode
int noti_led_mode = LED_ANIMATION_MODE_IDLE;

/**
 * AlarmDecoder callbacks.
 * As the AlarmDecoder receives data via put() data is validated.
 * When a complete messages is received or a specific stream of
 * bytes is received event(s) will be called.
 */

/**
 * ON_MESSAGE
 * When a full standard alarm state message is received before it is parsed.
 * WARNING: It may be invalid.
 */
void my_ON_MESSAGE_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "MESSAGE_CB: '%s'", msg->c_str());
  ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());
}

/**
 * ON_LRR
 * When a LRR message is received.
 * WARNING: It may be invalid.
 */
void my_ON_LRR_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "LRR_CB: %s",msg->c_str());
}

/**
 * ON_READY_CHANGE
 * When the READY state change event is triggered.
 */
void my_ON_READY_CHANGE_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "ON_READY_CHANGE: READY(%i) EXIT(%i) HOME(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_home, s->armed_away);
}

/**
 * ON_ARM
 * When a ARM event is triggered.
 */
void my_ON_ARM_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "ON_ARM: READY(%i) EXIT(%i) HOME(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_home, s->armed_away);

#ifdef CONFIG_STDK_IOT_CORE
  if (s->armed_home)
    cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, caps_helper_securitySystem.attr_securitySystemStatus.value_armedStay);
  else
    cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, caps_helper_securitySystem.attr_securitySystemStatus.value_armedAway);

  cap_securitySystem_data->attr_securitySystemStatus_send(cap_securitySystem_data);
#endif
}

/**
 * ON_DISARM
 * When a DISARM event is triggered.
 */
void my_ON_DISARM_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "ON_DISARM: READY(%i)", s->ready);

#ifdef CONFIG_STDK_IOT_CORE
  const char *new_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
  cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, new_value);
  cap_securitySystem_data->attr_securitySystemStatus_send(cap_securitySystem_data);
#endif
}

/**
 * ON_CHIME_CHANGE
 * When CHIME state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear Chime ON = Contact LED ON
 */
void my_ON_CHIME_CHANGE_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "ON_CHIME_CHANGE: CHIME(%i)", s->chime_on);
#ifdef CONFIG_STDK_IOT_CORE
  if ( s->chime_on)
    cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, caps_helper_contactSensor.attr_contact.value_open);
  else
    cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, caps_helper_contactSensor.attr_contact.value_closed);

  cap_contactSensor_data_chime->attr_contact_send(cap_contactSensor_data_chime);
#endif

// FIXME: Testing.. todo: cleanup compress simplify
#if CONFIG_TWILIO_CLIENT
  if (g_iot_status == IOT_STATUS_CONNECTING) {
    // load our settings for this event type.
    char buf[80];

    buf[0]=0;
    ad2_get_nv_slot_key_string(TWILIO_SID, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    std::string sid = buf;

    buf[0]=0;
    ad2_get_nv_slot_key_string(TWILIO_TOKEN, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    std::string token = buf;

    buf[0]=0;
    ad2_get_nv_slot_key_string(TWILIO_FROM, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    std::string from = buf;

    buf[0]=0;
    ad2_get_nv_slot_key_string(TWILIO_TO, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    std::string to = buf;

    buf[0] = 'M'; // default to Messages
    ad2_get_nv_slot_key_string(TWILIO_TYPE, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    char type = buf[0];

    buf[0]=0;
    ad2_get_nv_slot_key_string(TWILIO_BODY, AD2_DEFAULT_TWILIO_SLOT, buf, sizeof(buf));
    std::string body = "TEST 123";

    // add to the queue
    twilio_add_queue(sid.c_str(), token.c_str(), from.c_str(), to.c_str(), type, body.c_str());
    ESP_LOGI(TAG, "Adding task to twilio send queue");
  }
#endif
}

/**
 * ON_FIRE
 * When FIRE state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear FIRE ON = Contact LED ON
 */
void my_ON_FIRE_CB(std::string *msg, AD2VirtualPartitionState *s) {
  ESP_LOGI(TAG, "ON_FIRE_CB: FIRE(%i)", s->fire_alarm);
#ifdef CONFIG_STDK_IOT_CORE
  if ( s->fire_alarm ) {
    cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, caps_helper_smokeDetector.attr_smoke.value_detected);
  } else {
    cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, caps_helper_smokeDetector.attr_smoke.value_clear);
  }

  cap_smokeDetector_data->attr_smoke_send(cap_smokeDetector_data);
#endif
}

/**
 * Main app task to monitor physical button(s) and update state led(s)
 */
static void ad2_app_main_task(void *arg)
{
    IOT_CAP_HANDLE *handle = (IOT_CAP_HANDLE *)arg;

    int button_event_type;
    int button_event_count;

    for (;;) {
        if (get_button_event(&button_event_type, &button_event_count)) {
            button_event(handle, button_event_type, button_event_count);
        }
        if (noti_led_mode != LED_ANIMATION_MODE_IDLE) {
            change_led_mode(noti_led_mode);
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}


/**
 * ser2sock client task
 *  Read from AD2* socket and process.
 */
static void ser2sock_client_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    char host[AD2_MAX_MODE_ARG_SIZE] = {0};
    int port = 0;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {
        if (g_iot_status == IOT_STATUS_CONNECTING) {

            // load settings from NVS
            // host stored in slot 1
            ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY, 1, host, sizeof(host));

            char tokens[] = ": ";
	        char *hostptr = strtok(host, tokens);
            char *portptr = NULL;
            bool connectok = true;
            if (hostptr != NULL) {
                portptr = strtok(NULL, tokens);
                if (portptr != NULL) {
                    connectok = true;
                }
            }

            if (connectok) {
                port = atoi(portptr);
            } else {
                ESP_LOGE(TAG, "Error parsing host:port from settings");
            }

            ESP_LOGI(TAG, "Connecting to ser2sock host %s:%i", hostptr, port);

#if defined(CONFIG_AD2IOT_SER2SOCK_IPV4)
            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(host);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_AD2IOT_SER2SOCK_IPV6)
            struct sockaddr_in6 dest_addr = { 0 };
            inet6_aton(host, &dest_addr.sin6_addr);
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
            ESP_LOGI(TAG, "ser2sock client socket created, connecting to %s:%d", host, port);

            int err = connect(g_ad2_client_handle, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
            if (err != 0) {
                ESP_LOGE(TAG, "ser2sock client socket unable to connect: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "ser2sock client successfully connected");

            while (1) {
                int len = recv(g_ad2_client_handle, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // Error occurred during receiving
                if (len < 0) {
                    ESP_LOGE(TAG, "ser2sock client recv failed: errno %d", errno);
                    break;
                }
                // Data received
                else {
                    // Parse data from AD2* and report back to host.
                    rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                    AD2Parse.put(rx_buffer, len);
                }

                if (g_iot_status != IOT_STATUS_CONNECTING)
                    break;
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
 * send a string to the AD2
 */
void send_to_ad2(char *buf)
{
  if(g_ad2_client_handle>-1) {
    if (g_ad2_mode == 'C') {
      uart_write_bytes((uart_port_t)g_ad2_client_handle, buf, strlen(buf));
    } else
    if (g_ad2_mode == 'S') {
      // the handle is a socket fd use send()
      send(g_ad2_client_handle, buf, strlen(buf), 0);
    } else {

    }
  } else {
        ESP_LOGE(TAG, "invalid handle in send_to_ad2");
        return;
  }
}

/**
 * do_retransmit for ser2sock_server_task
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
 * ser2sock server task
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

    for (;;)
    {
        if (g_iot_status == IOT_STATUS_CONNECTING) {
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
 * Initialize ser2sock client on port 10000
 */
void init_ser2sock_client() {
    xTaskCreate(ser2sock_client_task, "ser2sock_client", 4096, (void*)AF_INET, tskIDLE_PRIORITY+2, NULL);
}

/**
 * Initialize ser2sock server on port 10000
 */
#if defined(AD2_SER2SOCK_SERVER)
void init_ser2sock_server() {
    xTaskCreate(ser2sock_server_task, "ser2sock_server", 4096, (void*)AF_INET, tskIDLE_PRIORITY+2, NULL);
}
#endif


/**
 *  Initialize the uart connected to the AD2 device
 */
void init_ad2_uart_client() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = false
    };
    uart_param_config((uart_port_t)g_ad2_client_handle, &uart_config);
    uart_driver_install((uart_port_t)g_ad2_client_handle, 2048, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);
}

/**
 * init_ad2_gpio
 */
void init_ad2_gpio()
{
}

/**
 * app_main()
 */
void app_main()
{

    //// AlarmDecoder App main

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

    // init the AlarmDecoder IoT host uP GPIO
    init_ad2_gpio();

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

    // Load AD2IoT operating mode [Socket|UART] and argument

    // get the mode
    char mode[2];
    ad2_get_nv_slot_key_string(AD2MODE_CONFIG_KEY, 0, mode, sizeof(mode));
    g_ad2_mode = mode[0];

    // init the AlarmDecoder UART
    if (g_ad2_mode == 'C') {
      init_ad2_uart_client();
    } else
    if (g_ad2_mode == 'S') {
      init_ser2sock_client();
    } else {
      ESP_LOGW(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
      printf("AD2IoT operating mode configured. Configure using ad2source command.\n");
    }

#if defined(AD2_SER2SOCK_SERVER)
    // init ser2sock server
    init_ser2sock_server();
#endif

#if 0
    // init the hardware ethernet module
    init_eth();
#endif

#if CONFIG_TWILIO_CLIENT
    // Initialize twilio
    twilio_init();
#endif

    // AlarmDecoder callback wiring.
    AD2Parse.setCB_ON_MESSAGE(my_ON_MESSAGE_CB);
    AD2Parse.setCB_ON_LRR(my_ON_LRR_CB);
    AD2Parse.setCB_ON_ARM(my_ON_ARM_CB);
    AD2Parse.setCB_ON_DISARM(my_ON_DISARM_CB);
    AD2Parse.setCB_ON_READY_CHANGE(my_ON_READY_CHANGE_CB);
    AD2Parse.setCB_ON_CHIME_CHANGE(my_ON_CHIME_CHANGE_CB);
    AD2Parse.setCB_ON_FIRE(my_ON_FIRE_CB);

    //// SmartThings setup
    stsdk_init();

    // Init onboarding button and LED
    iot_gpio_init();

    // Register and start the STSDK cli
    register_iot_cli_cmd();
    uart_cli_main();

    // Start main AlarmDecoder IoT app task
    xTaskCreate(ad2_app_main_task, "ad2_app_main_task", 4096, NULL, tskIDLE_PRIORITY+1, NULL);

    // Firmware update task
    xTaskCreate(ota_polling_task_func, "ota_polling_task_func", 8096, NULL, tskIDLE_PRIORITY+1, NULL);

    // connect to SmartThings server
    connection_start();
}




/*
 * OTA update support
 */
void cap_available_version_set(char *available_version)
{
	IOT_EVENT *init_evt;
	uint8_t evt_num = 1;
	int32_t sequence_no;

	if (!available_version) {
		ESP_LOGE(TAG, "invalid parameter");
		return;
	}

	/* Setup switch on state */
	init_evt = st_cap_attr_create_string("availableVersion", available_version, NULL);

	/* Send avail version to ota cap handler */
	sequence_no = st_cap_attr_send(ota_cap_handle, evt_num, &init_evt);
	if (sequence_no < 0)
		ESP_LOGE(TAG, "fail to send init_data");

	st_cap_attr_free(init_evt);
}

void cap_health_check_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
	IOT_EVENT *init_evt;
	uint8_t evt_num = 1;
	int32_t sequence_no;

    init_evt = st_cap_attr_create_int("checkInterval", 60, NULL);
	sequence_no = st_cap_attr_send(handle, evt_num, &init_evt);
	if (sequence_no < 0)
		ESP_LOGE(TAG, "fail to send init_data");

	st_cap_attr_free(init_evt);

}

void cap_current_version_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
	IOT_EVENT *init_evt;
	uint8_t evt_num = 1;
	int32_t sequence_no;

	/* Setup switch on state */
    // FIXME: get from device_info ctx->device_info->firmware_version
    // that is loaded from device_info.json
    init_evt = st_cap_attr_create_string("currentVersion", (char *)OTA_FIRMWARE_VERSION, NULL);

	/* Send current version attribute */
	sequence_no = st_cap_attr_send(handle, evt_num, &init_evt);
	if (sequence_no < 0)
		ESP_LOGE(TAG, "fail to send init_data");

	st_cap_attr_free(init_evt);
}

void refresh_cmd_cb(IOT_CAP_HANDLE *handle,
			iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    int address = -1;
    uint32_t mask = 1;
    ESP_LOGI(TAG, "refresh_cmd_cb");

    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, AD2_DEFAULT_VPA_SLOT, &address);
    if (address == -1) {
        ESP_LOGE(TAG, "default virtual partition defined. see 'vpaddr' command");
        return;
    }

    mask <<= address-1;
    AD2VirtualPartitionState * s = AD2Parse.getAD2PState(&mask);

    if (s != nullptr) {
      if (s->armed_home || s->armed_away) {
        my_ON_ARM_CB(nullptr, s);
      } else {
        my_ON_DISARM_CB(nullptr, s);
      }

      my_ON_CHIME_CHANGE_CB(nullptr, s);
      my_ON_READY_CHANGE_CB(nullptr, s);
    } else {
      ESP_LOGE(TAG, "vpaddr[%u] not found", address);
    }
}

void update_firmware_cmd_cb(IOT_CAP_HANDLE *handle,
			iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
  do_ota_update();
}

static void ota_polling_task_func(void *arg)
{
	while (1) {

		vTaskDelay(30000 / portTICK_PERIOD_MS);

		ESP_LOGI(TAG, "Starting check new version");

		if (ota_task_handle != NULL) {
			ESP_LOGI(TAG, "Device is updating");
			continue;
		}

		if (g_iot_status != IOT_STATUS_CONNECTING) {
			ESP_LOGI(TAG, "Device is not connected to cloud");
			continue;
		}

		char *read_data = NULL;
		unsigned int read_data_len = 0;

		esp_err_t ret = ota_https_read_version_info(&read_data, &read_data_len);
		if (ret == ESP_OK) {
			char *available_version = NULL;

			esp_err_t err = ota_api_get_available_version(read_data, read_data_len, &available_version);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "ota_api_get_available_version is failed : %d", err);
				continue;
			}

			cap_available_version_set(available_version);

			if (available_version)
				free(available_version);
		}

		/* Set polling period */
		unsigned int polling_day = ota_get_polling_period_day();
		unsigned int task_delay_sec = polling_day * 24 * 3600;
		vTaskDelay(task_delay_sec * 1000 / portTICK_PERIOD_MS);
	}
}


#ifdef __cplusplus
} // extern "C"
#endif