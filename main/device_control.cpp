/**
 *  @file    device_control.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief Hardware abstraction
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


#include <string.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_idf_version.h"
#include "device_control.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
#include "tcpip_adapter.h"
#else
#include "esp_netif.h"
#endif

#if CONFIG_AD2IOT_ETH_PHY_LAN8720
//#include "eth_phy/phy_lan8720.h"
//#define DEFAULT_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config
#endif

#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "device_control.h"

static const char *TAG = "HAL";

#ifdef __cplusplus
extern "C" {
#endif

void _eth_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
void _got_ip_event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data);

static int HAL_NET_INITIALIZED = false;

// WiFi event state bits
const int WIFI_STA_START_BIT 		= BIT0;
const int WIFI_STA_CONNECT_BIT		= BIT1;
const int WIFI_STA_DISCONNECT_BIT	= BIT2;
const int WIFI_AP_START_BIT 		= BIT3;
const int WIFI_AP_STOP_BIT 			= BIT4;
const int WIFI_EVENT_BIT_ALL = BIT0|BIT1|BIT2|BIT3|BIT4;

static EventGroupHandle_t net_event_group;

static bool switchAState = SWITCH_OFF;
static bool switchBState = SWITCH_OFF;


/**
 * @brief Change SWITCH/RELAY state.
 *   In some designs this may be a virtual SWITCH visible via the
 *   CLI or remotely via some API.
 *
 * @param [in]switch_state int [SWITCH_ON|SWITCH_OFF]
 *
 */
void hal_change_switch_a_state(int switch_state)
{
    if (switch_state == SWITCH_OFF) {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_SWITCH_A, SWITCH_OFF);
        switchAState = SWITCH_OFF;
    } else {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_SWITCH_A, SWITCH_ON);
        switchAState = SWITCH_ON;
    }
}

/**
 * @brief Change SWITCH/RELAY state.
 *   In some designs this may be a virtual SWITCH visible via the
 *   CLI or remotely via some API.
 *
 * @param [in]switch_state int [SWITCH_ON|SWITCH_OFF]
 *
 */
void hal_change_switch_b_state(int switch_state)
{
    if (switch_state == SWITCH_OFF) {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_SWITCH_B, MAINLED_GPIO_OFF);
        switchBState = SWITCH_OFF;
    } else {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_SWITCH_B, MAINLED_GPIO_ON);
        switchBState = SWITCH_ON;
    }
}

/**
 * @brief Get SWITCH/RELAY A state.
 *
 * @param [in]switch_state int [SWITCH_ON|SWITCH_OFF]
 *
 * @return bool current state.
 */
bool hal_get_switch_a_state()
{
    return switchAState;
}

/**
 * @brief Get SWITCH/RELAY B state.
 *
 * @param [in]switch_state int [SWITCH_ON|SWITCH_OFF]
 *
 * @return bool current state.
 */
bool hal_get_switch_b_state()
{
    return switchBState;
}

/**
 * @brief Button state tracking call periodically.
 *
 * @details State machine called frequently to watch for physical
 * button state changes. If changes are detected it will return
 * true and place the details about the event into the provided
 * int pointers.
 *
 * @param [in]button_event_type int pointer to event_type.
 * @param [in]button_event_count int pointer to event_count.
 *
 * @return int [true|false]
 */
int hal_get_button_event(int* button_event_type, int* button_event_count)
{
    static uint32_t button_count = 0;
    static uint32_t button_last_state = BUTTON_GPIO_RELEASED;
    static TimeOut_t button_timeout;
    static TickType_t long_press_tick = pdMS_TO_TICKS(BUTTON_LONG_THRESHOLD_MS);
    static TickType_t button_delay_tick = pdMS_TO_TICKS(BUTTON_DELAY_MS);

    uint32_t gpio_level = 0;

    gpio_level = gpio_get_level((gpio_num_t)GPIO_INPUT_BUTTON);
    if (button_last_state != gpio_level) {
        /* wait debounce time to ignore small ripple of currunt */
        vTaskDelay( pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS) );
        gpio_level = gpio_get_level((gpio_num_t)GPIO_INPUT_BUTTON);
        if (button_last_state != gpio_level) {
            ESP_LOGI(TAG, "%s: Button event, val: %d, tick: %u", __func__, gpio_level, (uint32_t)xTaskGetTickCount());
            button_last_state = gpio_level;
            if (gpio_level == BUTTON_GPIO_PRESSED) {
                button_count++;
            }
            vTaskSetTimeOutState(&button_timeout);
            button_delay_tick = pdMS_TO_TICKS(BUTTON_DELAY_MS);
            long_press_tick = pdMS_TO_TICKS(BUTTON_LONG_THRESHOLD_MS);
        }
    } else if (button_count > 0) {
        if ((gpio_level == BUTTON_GPIO_PRESSED)
                && (xTaskCheckForTimeOut(&button_timeout, &long_press_tick ) != pdFALSE)) {
            *button_event_type = BUTTON_LONG_PRESS;
            *button_event_count = 1;
            button_count = 0;
            return true;
        } else if ((gpio_level == BUTTON_GPIO_RELEASED)
                   && (xTaskCheckForTimeOut(&button_timeout, &button_delay_tick ) != pdFALSE)) {
            *button_event_type = BUTTON_SHORT_PRESS;
            *button_event_count = button_count;
            button_count = 0;
            return true;
        }
    }

    return false;
}

/**
 * @brief set the LED state
 *
 */
void hal_change_led_state(int state)
{
    gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED, state);
}

/**
 * @brief blocking call flashes the LED at a given delay for
 * a given count.
 *
 * @param [in]switch_state int starting SWITCH_OFF/ON
 */
void hal_led_blink(int switch_state, int delay, int count)
{
    for (int i = 0; i < count; i++) {
        vTaskDelay(delay / portTICK_PERIOD_MS);
        hal_change_led_state(SWITCH_OFF);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        hal_change_led_state(SWITCH_ON);
    }
}

/**
 * @brief change the LED mode
 *
 * @param [in]mode sets the mode for the indicator LED.
 *   In some designs this may be a virtual LED visible via the
 *   CLI or remotely via some API.
 */
void hal_change_led_mode(int mode)
{
    static TimeOut_t led_timeout;
    static TickType_t led_tick = -1;
    static int last_led_mode = -1;
    static int led_state = SWITCH_OFF;

    if (last_led_mode != mode) {
        last_led_mode = mode;
        vTaskSetTimeOutState(&led_timeout);
        led_tick = 0;
    }

    switch (mode) {
    case LED_ANIMATION_MODE_IDLE:
        break;
    case LED_ANIMATION_MODE_SLOW:
        if (xTaskCheckForTimeOut(&led_timeout, &led_tick ) != pdFALSE) {
            led_state = 1 - led_state;
            hal_change_led_state(led_state);
            vTaskSetTimeOutState(&led_timeout);
            if (led_state == SWITCH_ON) {
                led_tick = pdMS_TO_TICKS(200);
            } else {
                led_tick = pdMS_TO_TICKS(800);
            }
        }
        break;
    case LED_ANIMATION_MODE_FAST:
        if (xTaskCheckForTimeOut(&led_timeout, &led_tick ) != pdFALSE) {
            led_state = 1 - led_state;
            hal_change_led_state(led_state);
            vTaskSetTimeOutState(&led_timeout);
            led_tick = pdMS_TO_TICKS(100);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Initialize the hardware.
 */
void hal_gpio_init(void)
{
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (gpio_num_t) 1 << GPIO_OUTPUT_MAINLED;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (gpio_num_t) 1 << GPIO_OUTPUT_MAINLED_0;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = 1 << GPIO_OUTPUT_NOUSE1;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (gpio_num_t)1 <<GPIO_OUTPUT_NOUSE2;
    gpio_config(&io_conf);


    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1 << GPIO_INPUT_BUTTON;
    io_conf.pull_down_en = (BUTTON_GPIO_RELEASED == 0 ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE);
    io_conf.pull_up_en = (BUTTON_GPIO_RELEASED == 1 ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE);
    gpio_config(&io_conf);

    gpio_set_intr_type((gpio_num_t)GPIO_INPUT_BUTTON, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(0);

    gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED, MAINLED_GPIO_ON);
    gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED_0, 0);
}

/**
 * @brief restart the hardware
 */
void hal_restart()
{
    ESP_LOGE(TAG, "%s: rebooting now.", __func__);
    ad2_printf_host("Restarting now\r\n");
    esp_restart();
}

/**
 * @brief event handler for WiFi and Ethernet
 */
void _wifi_event_handler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data)
{
    wifi_ap_record_t ap_info;
    int ret;

    memset(&ap_info, 0x0, sizeof(wifi_ap_record_t));
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;

    switch(event_id) {
    case WIFI_EVENT_STA_START:
        xEventGroupSetBits(net_event_group, WIFI_STA_START_BIT);
        esp_wifi_connect();
        g_ad2_network_state = AD2_OFFLINE;
        break;

    case WIFI_EVENT_STA_STOP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");
        xEventGroupClearBits(net_event_group, WIFI_EVENT_BIT_ALL);
        g_ad2_network_state = AD2_OFFLINE;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        // Set hostname
        ret = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA,"ad2iot");
        if(ret != ESP_OK ) {
            ESP_LOGE(TAG,"failed to set hostname: %i %i '%s'", ret, errno, strerror(errno));
        }
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        xEventGroupSetBits(net_event_group, WIFI_STA_DISCONNECT_BIT);
        esp_wifi_connect();
        xEventGroupClearBits(net_event_group, WIFI_STA_CONNECT_BIT);
        g_ad2_network_state = AD2_OFFLINE;
        break;

    case WIFI_EVENT_AP_START:
        // Set hostname
        ret = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,"ad2iot");
        if(ret != ESP_OK ) {
            ESP_LOGE(TAG,"failed to set hostname: %i %i '%s'", ret, errno, strerror(errno));
        }
        xEventGroupClearBits(net_event_group, WIFI_EVENT_BIT_ALL);
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START");
        xEventGroupSetBits(net_event_group, WIFI_AP_START_BIT);
        break;

    case WIFI_EVENT_AP_STOP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STOP");
        xEventGroupSetBits(net_event_group, WIFI_AP_STOP_BIT);
        break;
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->mac),
                 event->aid);
        break;

    case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " leave, AID=%d",
                 MAC2STR(event->mac),
                 event->aid);

        xEventGroupSetBits(net_event_group, WIFI_AP_STOP_BIT);
        break;

    default:
        ESP_LOGI(TAG, "wifi_event_handler = %d", event_id);
        break;
    }

}

/**
 * @brief initialize network TCP/IP stack driver
 */
void hal_init_network_stack()
{

    if(HAL_NET_INITIALIZED) {
        ESP_LOGE(TAG, "network TCP/IP stack already initialized");
        return;
    }

    ESP_LOGI(TAG, "network TCP/IP stack init start");

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
    tcpip_adapter_init();
#else
    esp_netif_init();
#endif

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_loop_init(nullptr, nullptr));
    ESP_ERROR_CHECK(tcpip_adapter_set_default_eth_handlers());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &_got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_got_ip_event_handler, NULL));

    // create event group
    net_event_group = xEventGroupCreate();

    HAL_NET_INITIALIZED = true;

    ESP_LOGI(TAG, "network TCP/IP stack init finish");
    return;
}

/**
 * @brief initialize wifi hardware and driver
 */
void hal_init_wifi(std::string &args)
{
    esp_err_t esp_ret;
    ESP_LOGI(TAG, "WiFi hardware init start");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&cfg);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed err=[%d]", esp_ret);
        return;
    }

    esp_ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_storage failed err=[%d]", esp_ret);
        return;
    }

    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed err=[%d]", esp_ret);
        return;
    }

    int res = 0;
    std::string value;

    wifi_config_t sta_config = { };

    res = ad2_query_key_value(args, "SID", value);
    if (res <= 0) {
        ESP_LOGE(TAG, "Error loading SID value from netmode arg");
    }
    strcpy((char*)sta_config.sta.ssid, value.c_str());

    res = ad2_query_key_value(args, "PASSWORD", value);
    if (res <= 0) {
        ESP_LOGE(TAG, "Error loading PASSWORD value from netmode arg");
    }
    strcpy((char*)sta_config.sta.password, value.c_str());

    sta_config.sta.bssid_set = false;

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );


    // test DHCP or Static configuration
    res = ad2_query_key_value(args, "MODE", value);
    ad2_ucase(value);
    bool dhcp = true;
    if (res > 0) {
        // Static IP mode
        if (value[0] == 'S') {
            dhcp = false;
        }
    }

    if (!dhcp) {
        // init vars
        std::string ip;
        std::string netmask;
        std::string gateway;
        std::string dns1;
        std::string dns2;
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_dns_info_t dns_info;
        memset(&ip_info, 0, sizeof(ip_info));
        memset(&dns_info, 0, sizeof(dns_info));

        // Parse name value pair settings from args string
        res = ad2_query_key_value(args, "IP", ip);
        res = ad2_query_key_value(args, "MASK", netmask);
        res = ad2_query_key_value(args, "GW", gateway);
        res = ad2_query_key_value(args, "DNS1", dns1);
        res = ad2_query_key_value(args, "DNS2", dns2);

        // stop dhcp client service on interface.
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));

        // if gateway defined set it.
        if (gateway.length()) {
            ip4addr_aton(gateway.c_str(), &ip_info.gw);
        }

        // Assign static ip/netmask to interface
        ip4addr_aton(ip.c_str(), &ip_info.ip);
        ip4addr_aton(netmask.c_str(), &ip_info.netmask);
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

        // assign DNS1 if avail.
        if (dns1.length()) {
            ipaddr_aton(dns1.c_str(), &dns_info.ip);
            ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns_info));
        }
        // assign DNS2 if avail.
        if (dns2.length()) {
            ipaddr_aton(dns2.c_str(), &dns_info.ip);
            ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_BACKUP, &dns_info));
        }
    } else {
        ESP_LOGI(TAG, "DHCP Mode selected");
    }

    ESP_ERROR_CHECK( esp_wifi_start() );

    ESP_LOGI(TAG, "WiFi hardware init done");

}

/**
 * @brief initialize the ethernet driver
 */
void hal_init_eth(std::string &args)
{
    ESP_LOGI(TAG, "ETH hardware init start");
    int res = 0;
    std::string value;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_AD2IOT_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_AD2IOT_ETH_PHY_RST_GPIO;
    phy_config.reset_timeout_ms = 100;
    gpio_pad_select_gpio((gpio_num_t)CONFIG_AD2IOT_ETH_PHY_POWER_GPIO);
    gpio_set_direction((gpio_num_t)CONFIG_AD2IOT_ETH_PHY_POWER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)CONFIG_AD2IOT_ETH_PHY_POWER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

#if CONFIG_AD2IOT_USE_INTERNAL_ETHERNET
    mac_config.smi_mdc_gpio_num = (gpio_num_t)CONFIG_AD2IOT_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = (gpio_num_t)CONFIG_AD2IOT_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#ifdef CONFIG_AD2IOT_ETH_PHY_LAN8720
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#endif
#endif

    /* Configure the hardware */
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    // test DHCP or Static configuration
    res = ad2_query_key_value(args, "MODE", value);
    ad2_ucase(value);
    bool dhcp = true;
    if (res > 0) {
        // Static IP mode
        if (value[0] == 'S') {
            dhcp = false;
        }
    }

    if (!dhcp) {
        ESP_LOGI(TAG, "Static IP Mode selected");

        // init vars
        std::string ip;
        std::string netmask;
        std::string gateway;
        std::string dns1;
        std::string dns2;
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_dns_info_t dns_info;
        memset(&ip_info, 0, sizeof(ip_info));
        memset(&dns_info, 0, sizeof(dns_info));

        // Parse name value pair settings from args string
        res = ad2_query_key_value(args, "IP", ip);
        res = ad2_query_key_value(args, "MASK", netmask);
        res = ad2_query_key_value(args, "GW", gateway);
        res = ad2_query_key_value(args, "DNS1", dns1);
        res = ad2_query_key_value(args, "DNS2", dns2);

        // stop dhcp client service on interface.
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH));

        // if gateway defined set it.
        if (gateway.length()) {
            ip4addr_aton(gateway.c_str(), &ip_info.gw);
        }

        // Assign static ip/netmask to interface
        ip4addr_aton(ip.c_str(), &ip_info.ip);
        ip4addr_aton(netmask.c_str(), &ip_info.netmask);
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &ip_info));

        // assign DNS1 if avail.
        if (dns1.length()) {
            ipaddr_aton(dns1.c_str(), &dns_info.ip);
            ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &dns_info));
        }
        // assign DNS2 if avail.
        if (dns2.length()) {
            ipaddr_aton(dns2.c_str(), &dns_info.ip);
            ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_BACKUP, &dns_info));
        }
    } else {
        ESP_LOGI(TAG, "DHCP Mode selected");
    }
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "ETH hardware init finish");

    return;
}

/**
 * @brief Start host uart
 */
void hal_host_uart_init()
{

    // Configure parameters of an UART driver,
    uart_config_t* uart_config = (uart_config_t*)calloc(sizeof(uart_config_t), 1);

    uart_config->baud_rate = 115200;
    uart_config->data_bits = UART_DATA_8_BITS;
    uart_config->parity    = UART_PARITY_DISABLE;
    uart_config->stop_bits = UART_STOP_BITS_1;
    uart_config->flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    uart_param_config(UART_NUM_0, uart_config);
    uart_driver_install(UART_NUM_0, MAX_UART_LINE_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);

    free(uart_config);
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
void _got_ip_event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const tcpip_adapter_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    xEventGroupSetBits(net_event_group, WIFI_STA_CONNECT_BIT);
    xEventGroupClearBits(net_event_group, WIFI_STA_DISCONNECT_BIT);
    g_ad2_network_state = AD2_CONNECTED;

}

/** Event handler for Ethernet events */
void _eth_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    esp_err_t ret;
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        g_ad2_network_state = AD2_OFFLINE;
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        // Set hostname
        ret = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH,"ad2iot");
        if(ret != ESP_OK ) {
            ESP_LOGE(TAG,"failed to set hostname: %i %i '%s'", ret, errno, strerror(errno));
        }
        break;
    case ETHERNET_EVENT_STOP:
        g_ad2_network_state = AD2_OFFLINE;
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/**
 * @brief assert reset pin on AD2pHAT board long enough to reboot the uP.
 * TODO: First release of the carrier uses GPIO 32. This may move on the final product.
 */
void hal_ad2_reset()
{
    gpio_pad_select_gpio(GPIO_NUM_32);
    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_32, 1);
}

#ifdef __cplusplus
} // extern "C"
#endif
