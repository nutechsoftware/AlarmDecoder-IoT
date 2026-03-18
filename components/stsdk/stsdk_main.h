/**
 *  @file    stsdk_main.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *
 *  @brief SmartThings Direct Attached Device using STSDK
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

#ifndef _STSDK_MAIN_H
#define _STSDK_MAIN_H


#include "st_dev.h"
#include "device_control.h"

// device types
#include "caps_refresh.h"
#include "caps_switch.h"
#include "caps_securitySystem.h"
#include "caps_button.h"
#include "caps_momentary.h"
#include "caps_smokeDetector.h"
#include "caps_tamperAlert.h"
#include "caps_contactSensor.h"
#include "caps_carbonMonoxideDetector.h"
#include "caps_powerSource.h"
#include "caps_battery.h"

#define STSDK_COMMAND        "smartthings"
#define STSDK_SUBCMD_ENABLE  "enable"
#define STSDK_SUBCMD_CLEANUP "cleanup"
#define STSDK_SUBCMD_SERIAL  "serial"
#define STSDK_SUBCMD_PUBKEY  "publickey"
#define STSDK_SUBCMD_PRIVKEY "privatekey"

#define STSDK_CONFIG_SECTION "smartthings"


//#define SET_PIN_NUMBER_CONFRIM
#ifdef __cplusplus
extern "C" {
#endif
void stsdk_register_cmds(void);
void stsdk_init(void);
void button_event(IOT_CAP_HANDLE *handle, int type, int count);
void iot_noti_cb(iot_noti_data_t *noti_data, void *noti_usr_data);
void stsdk_connection_start(void);
void capability_init();
void cap_current_version_init_cb(IOT_CAP_HANDLE *handle, void *usr_data);
void update_firmware_cmd_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data);
void cap_health_check_init_cb(IOT_CAP_HANDLE *handle, void *usr_data);
void ping_cmd_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data);
void refresh_cmd_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data);
int stsdk_get_switch_b_state();
int stsdk_get_switch_b_state();

extern iot_status_t g_iot_status;
extern iot_stat_lv_t g_iot_stat_lv;

extern IOT_CAP_HANDLE *ota_cap_handle;
extern IOT_CAP_HANDLE *healthCheck_cap_handle;
extern IOT_CAP_HANDLE *refresh_cap_handle;
#if 0 // TODO/FIXME
extern caps_securitySystem_data_t *cap_securitySystem_data;
extern caps_button_data_t *cap_buttonMainSecurityActions_data;
#endif
extern caps_powerSource_data_t *cap_powerSource_data;
extern caps_battery_data_t *cap_battery_data;
extern caps_contactSensor_data_t *cap_contactSensor_data_chime;
extern caps_momentary_data_t *cap_momentary_data_chime;
extern caps_smokeDetector_data_t *cap_smokeDetector_data;
extern caps_momentary_data_t *cap_momentary_data_fire;
extern caps_contactSensor_data_t *cap_alarm_bell_data;
extern caps_momentary_data_t *cap_momentary_data_panic_alarm;
extern caps_momentary_data_t *cap_momentary_data_aux_alarm;
extern caps_contactSensor_data_t *cap_contactSensor_data_arm_stay;
extern caps_momentary_data_t *cap_momentary_data_arm_stay;
extern caps_contactSensor_data_t *cap_contactSensor_data_arm_away;
extern caps_momentary_data_t *cap_momentary_data_arm_away;
extern caps_contactSensor_data_t *cap_contactSensor_data_bypass;
#if 0 // TODO/FIXME
extern caps_switch_data_t *cap_switch_a_data;
extern caps_switch_data_t *cap_switch_b_data;
#endif
extern caps_momentary_data_t *cap_momentary_data_disarm;





extern IOT_CAP_HANDLE *ota_cap_handle;
extern IOT_CAP_HANDLE *healthCheck_cap_handle;
extern IOT_CAP_HANDLE *refresh_cap_handle;

extern int noti_led_mode;
#ifdef __cplusplus
} // extern "C"
#endif
#endif /* _STSDK_MAIN_H */
