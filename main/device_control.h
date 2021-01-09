/**
 *  @file    device_control.h
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
#ifndef _DEVICE_CONTROL_H
#define _DEVICE_CONTROL_H

// Defaults to ESP32 DEVKIT V4
//#define CONFIG_TARGET_OLIMEX_ESP32_EVB
//#define CONFIG_TARGET_WEMOS_D1_R32
#ifdef CONFIG_TARGET_WEMOS_D1_R32
#define GPIO_INPUT_BUTTON 18
#define GPIO_OUTPUT_MAINLED 16
#define GPIO_OUTPUT_MAINLED_0 26 /* use as ground */
#define GPIO_OUTPUT_NOUSE1 17
#define GPIO_OUTPUT_NOUSE2 25
#define GPIO_OUTPUT_SWITCH_A GPIO_NUM_MAX
#define GPIO_OUTPUT_SWITCH_B GPIO_NUM_MAX
#elif CONFIG_TARGET_OLIMEX_ESP32_EVB
#define GPIO_INPUT_BUTTON 0
#define GPIO_OUTPUT_MAINLED 12
#define GPIO_OUTPUT_MAINLED_0 26 /* use as ground */
#define GPIO_OUTPUT_NOUSE1 14
#define GPIO_OUTPUT_NOUSE2 27
#define GPIO_OUTPUT_SWITCH_A GPIO_NUM_MAX
#define GPIO_OUTPUT_SWITCH_B GPIO_NUM_MAX
#else // ESP32_DEVKITC_V4
#define GPIO_INPUT_BUTTON 0
#define GPIO_OUTPUT_MAINLED 12
#define GPIO_OUTPUT_MAINLED_0 26 /* use as ground */
#define GPIO_OUTPUT_NOUSE1 14
#define GPIO_OUTPUT_NOUSE2 27
#define GPIO_OUTPUT_SWITCH_A GPIO_NUM_MAX
#define GPIO_OUTPUT_SWITCH_B GPIO_NUM_MAX
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum switch_onoff_state {
    SWITCH_OFF = 0,
    SWITCH_ON = 1,
};

enum main_led_gpio_state {
    MAINLED_GPIO_ON = 1,
    MAINLED_GPIO_OFF = 0,
};

enum led_animation_mode_list {
    LED_ANIMATION_MODE_IDLE = 0,
    LED_ANIMATION_MODE_FAST,
    LED_ANIMATION_MODE_SLOW,
};

enum button_gpio_state {
    BUTTON_GPIO_RELEASED = 1,
    BUTTON_GPIO_PRESSED = 0,
};

#define BUTTON_DEBOUNCE_TIME_MS 20
#define BUTTON_LONG_THRESHOLD_MS 5000
#define BUTTON_DELAY_MS 300

enum button_event_type {
    BUTTON_LONG_PRESS = 0,
    BUTTON_SHORT_PRESS = 1,
};

int hal_get_button_event(int* button_event_type, int* button_event_count);
void hal_change_switch_a_state(int switch_state);
void hal_change_switch_b_state(int switch_state);
bool hal_get_switch_a_state();
bool hal_get_switch_b_state();
void hal_led_blink(int switch_state, int delay, int count);
void hal_change_led_mode(int noti_led_mode);
void hal_gpio_init(void);
void hal_restart();
void hal_init_network_stack();
void hal_init_wifi(std::string &args);
void hal_init_eth(std::string &args);
void hal_host_uart_init();
void hal_ad2_reset();
#ifdef __cplusplus
}
#endif
#endif /* _DEVICE_CONTROL_H */

