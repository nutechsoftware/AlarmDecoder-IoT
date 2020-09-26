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

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
static const char *TAG = "HAL";

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
    esp_restart();
}
