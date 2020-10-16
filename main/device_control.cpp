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

#include "device_control.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"

static const char *TAG = "HAL";

#ifdef __cplusplus
extern "C" {
#endif

const int WIFI_STA_START_BIT 		= BIT0;
const int WIFI_STA_CONNECT_BIT		= BIT1;
const int WIFI_STA_DISCONNECT_BIT	= BIT2;
const int WIFI_AP_START_BIT 		= BIT3;
const int WIFI_AP_STOP_BIT 			= BIT4;

const int WIFI_EVENT_BIT_ALL = BIT0|BIT1|BIT2|BIT3|BIT4;

static int HAL_WIFI_INITIALIZED = false;
static EventGroupHandle_t wifi_event_group;


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
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED,MAINLED_GPIO_OFF);
    } else {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED, MAINLED_GPIO_ON);
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
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED, MAINLED_GPIO_OFF);
    } else {
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_MAINLED, MAINLED_GPIO_ON);
    }
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
 * @brief blocking call flashes the LED at a given delay for
 * a given count.
 *
 * @param [in]switch_state int starting SWITCH_OFF/ON
 */
void hal_led_blink(int switch_state, int delay, int count)
{
    for (int i = 0; i < count; i++) {
        vTaskDelay(delay / portTICK_PERIOD_MS);
        hal_change_switch_a_state(1 - switch_state);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        hal_change_switch_a_state(switch_state);
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
            hal_change_switch_a_state(led_state);
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
            hal_change_switch_a_state(led_state);
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
static esp_err_t hal_event_handler(void *dummy, system_event_t *event)
{
    wifi_ap_record_t ap_info;
    memset(&ap_info, 0x0, sizeof(wifi_ap_record_t));

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xEventGroupSetBits(wifi_event_group, WIFI_STA_START_BIT);
        esp_wifi_connect();
        g_ad2_network_state = AD2_OFFLINE;
        break;

    case SYSTEM_EVENT_STA_STOP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");
        xEventGroupClearBits(wifi_event_group, WIFI_EVENT_BIT_ALL);
        g_ad2_network_state = AD2_OFFLINE;
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnect reason : %d", event->event_info.disconnected.reason);
        xEventGroupSetBits(wifi_event_group, WIFI_STA_DISCONNECT_BIT);
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECT_BIT);
        g_ad2_network_state = AD2_OFFLINE;
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        esp_wifi_sta_get_ap_info(&ap_info);
        ESP_LOGI(TAG, "got ip:%s rssi:%ddBm",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip), ap_info.rssi);
        xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECT_BIT);
        xEventGroupClearBits(wifi_event_group, WIFI_STA_DISCONNECT_BIT);
        g_ad2_network_state = AD2_CONNECTED;
        break;

    case SYSTEM_EVENT_AP_START:
        xEventGroupClearBits(wifi_event_group, WIFI_EVENT_BIT_ALL);
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START");
        xEventGroupSetBits(wifi_event_group, WIFI_AP_START_BIT);
        break;

    case SYSTEM_EVENT_AP_STOP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STOP");
        xEventGroupSetBits(wifi_event_group, WIFI_AP_STOP_BIT);
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);

        xEventGroupSetBits(wifi_event_group, WIFI_AP_STOP_BIT);
        break;

    default:
        ESP_LOGI(TAG, "hal_event_handler = %d", event->event_id);
        break;
    }

    return ESP_OK;
}

/**
 * @brief initialize wifi driver
 */
void hal_init_wifi()
{
    ESP_LOGI(TAG, "Wifi hardware init");

    esp_err_t esp_ret;
    if(HAL_WIFI_INITIALIZED) {
        return;
    }

    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    esp_ret = esp_event_loop_init(hal_event_handler, NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_init failed err=[%d]", esp_ret);
        return;
    }

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
    std::string args;
    ad2_network_mode(args);

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
    ESP_ERROR_CHECK( esp_wifi_start() );

    HAL_WIFI_INITIALIZED = true;
    ESP_LOGI(TAG, "[esp32] iot_bsp_wifi_init done");

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

#ifdef __cplusplus
} // extern "C"
#endif
