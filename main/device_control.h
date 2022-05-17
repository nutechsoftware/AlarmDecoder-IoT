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

#define GPIO_NOT_USED GPIO_NUM_MAX

// Defaults to ESP32 DEVKIT V4
//#define CONFIG_TARGET_OLIMEX_ESP32_EVB
//#define CONFIG_TARGET_WEMOS_D1_R32
#define CONFIG_TARGET_OLIMEX_ESP32_POE_ISO

#if defined(CONFIG_TARGET_WEMOS_D1_R32)
#define GPIO_INPUT_BUTTON GPIO_NOT_USED
#define GPIO_MAINLED GPIO_NOT_USED
#define GPIO_MAINLED_0 GPIO_NOT_USED /* use as current sink */
#define GPIO_SWITCH_A GPIO_NOT_USED
#define GPIO_SWITCH_B GPIO_NOT_USED
#elif defined(CONFIG_TARGET_OLIMEX_ESP32_EVB)
#define GPIO_INPUT_BUTTON GPIO_NOT_USED
#define GPIO_MAINLED GPIO_NOT_USED
#define GPIO_MAINLED_0 GPIO_NOT_USED /* use as current sink */
#define GPIO_SWITCH_A GPIO_NOT_USED
#define GPIO_SWITCH_B GPIO_NOT_USED
#elif defined(CONFIG_TARGET_OLIMEX_ESP32_POE_ISO)
#define GPIO_INPUT_BUTTON 0
#define GPIO_MAINLED GPIO_NOT_USED
#define GPIO_MAINLED_0 GPIO_NOT_USED /* use as current sink */
#define GPIO_SWITCH_A GPIO_NOT_USED
#define GPIO_SWITCH_B GPIO_NOT_USED
#else // ESP32_DEVKITC_V4
#define GPIO_INPUT_BUTTON GPIO_NOT_USED
#define GPIO_MAINLED GPIO_NOT_USED
#define GPIO_MAINLED_0 GPIO_NOT_USED /* use as current sink */
#define GPIO_SWITCH_A GPIO_NOT_USED
#define GPIO_SWITCH_B GPIO_NOT_USED
#endif

// AD2IoT - ESP32-POE-ISO-V1-ADAPTER
/// AD2pHat Reset pin assert LOW to reboot.
#define GPIO_AD2_RESET GPIO_NUM_32
/// AD2pHat heartbeat monitor pin
#define GPIO_AD2_MONITOR GPIO_NUM_33
/// ESP32 TX PIN -> AD2_RX
#define GPIO_AD2_RX GPIO_NUM_4
/// ESP32 RX PIN <- AD2_TX
#define GPIO_AD2_TX GPIO_NUM_36

// OLIMEX ESP-POE*
/// uSD
#define GPIO_uSD_CLK GPIO_NUM_14   // CLK/SCLK - HS2_CLK
#define GPIO_uSD_CMD GPIO_NUM_15   // CMD/DI - MOSI
#define GPIO_uSD_D0 GPIO_NUM_2     // DAT0/DO - HS2_DATA0 MISO
#define GPIO_uSD_CS GPIO_NOT_USED  // N/A tie to open pin for now :(
/// LAN8710A
#define GPIO_ETH_PHY_POWER GPIO_NUM_12
#define GPIO_ETH_PHY_MODE0 GPIO_NUM_25
#define GPIO_ETH_PHY_MODE1 GPIO_NUM_26
#define GPIO_ETH_PHY_MODE2 GPIO_NUM_27
#define GPIO_ETH_PHY_EMAC_TX_EN GPIO_NUM_21
#define GPIO_ETH_PHY_EMAC_TXD0 GPIO_NUM_19
#define GPIO_ETH_PHY_EMAC_TXD1 GPIO_NUM_22
#define GPIO_ETH_PHY_EMAC_CLK GPIO_NUM_17
#define GPIO_ETH_PHY_MDC GPIO_NUM_23
#define GPIO_ETH_PHY_MDIO GPIO_NUM_18



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
void hal_factory_reset();
void hal_init_network_stack();
void hal_init_wifi(std::string &args);
void hal_init_eth(std::string &args);
void hal_set_wifi_hostname(const char *);
void hal_set_eth_hostname(const char *);
void hal_host_uart_init();
void hal_ad2_reset();
bool hal_get_netif_started();
bool hal_get_network_connected();
void hal_ota_do_update(const char *);
void hal_set_network_connected(bool);
bool hal_init_sd_card();
void hal_get_socket_client_ip(int sockfd, std::string& IP);
void hal_get_socket_local_ip(int sockfd, std::string& IP);
void hal_set_log_mode(char m);
void hal_dump_hw_info();
void hal_init_persistent_storage();
#ifdef __cplusplus
}
#endif
#endif /* _DEVICE_CONTROL_H */

