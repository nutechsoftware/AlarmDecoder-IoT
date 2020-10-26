/**
 *  @file    stsdk_main.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *  @version 1.0.3
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

// common includes
// stdc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>

// esp includes
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"
#include "ota_util.h"

// Disable componet via sdkconfig
#if CONFIG_STDK_IOT_CORE
static const char *TAG = "STSDK";

// specific includes
#include "stsdk_main.h"

#ifdef __cplusplus
extern "C" {
#endif

// @brief global ST status tracking
iot_status_t g_iot_status = IOT_STATUS_IDLE;

// @brief global ST stat tracking
iot_stat_lv_t g_iot_stat_lv;

// @brief global ST context pointer
IOT_CTX* ctx = NULL;

// onboarding_config_start is null-terminated string
extern const uint8_t onboarding_config_start[]  asm("_binary_onboarding_config_json_start");
extern const uint8_t onboarding_config_end[]    asm("_binary_onboarding_config_json_end");

// device_info_start is null-terminated string
extern const uint8_t device_info_start[]        asm("_binary_device_info_json_start");
extern const uint8_t device_info_end[]          asm("_binary_device_info_json_end");

/**
 * @brief component: main
 */
// @brief OTA device capabilies
IOT_CAP_HANDLE *ota_cap_handle = NULL;
// @brief Health Check capabilities
IOT_CAP_HANDLE *healthCheck_cap_handle = NULL;
// @brief Refresh action capabilities
IOT_CAP_HANDLE *refresh_cap_handle = NULL;
#if 0 // TODO/FIXME
// @brief Security System capability
caps_securitySystem_data_t *cap_securitySystem_data;
// @brief Buttons in 'main' component for managing security modes.
caps_button_data_t *cap_buttonMainSecurityActions_data;
#endif
// @brief Power Source [UNKNOWN, AC, BATTERY]
caps_powerSource_data_t *cap_powerSource_data;
// @brief Battery [% int 0 - 100]
caps_battery_data_t *cap_battery_data;


/**
 * @brief component: chime
 * Chime management
 */
// @brief Chime virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_chime;
// @brief Chime momentary button
caps_momentary_data_t *cap_momentary_data_chime;


/**
 * @brief component: fire
 * Fire Alarm
 */
// @brief Smoke Detector capability
caps_smokeDetector_data_t *cap_smokeDetector_data;
// @brief Fire Alarm momentary panic button
caps_momentary_data_t *cap_momentary_data_fire;

/**
 * @brief component: alarm
 * Alarm Bell
 */
// @brief Alarm capability
caps_alarm_data_t *cap_alarm_bell_data;
// @brief Fire Alarm momentary panic button
caps_momentary_data_t *cap_momentary_data_panic_alarm;

/**
 * @brief component: auxalarm
 * AUX Medical alarm.
 */
// @brief AUX Alarm momentary panic button
caps_momentary_data_t *cap_momentary_data_aux_alarm;

/**
 * @brief component: armstay
 * Arm Stay
 */
// @brief Arm Stay virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_arm_stay;
// @brief Arm Stay momentary button
caps_momentary_data_t *cap_momentary_data_arm_stay;

/**
 * @brief component: armaway
 * Arm Away
 */
// @brief Arm Away virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_arm_away;
// @brief Arm Away momentary button
caps_momentary_data_t *cap_momentary_data_arm_away;

/**
 * @brief component: bypass
 * Bypass state & control
 */
// @brief Zone Bypassed virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_zone_bypassed;

/**
 * @brief component: ready
 * Ready to arm state
 */
// @brief Ready To Arm virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_ready_to_arm;

/**
 * @brief component: exit
 * Exit state. Zones disabled to allow entry / exit while armed.
 */
// @brief Exit Mode virtual contact sensor
caps_contactSensor_data_t *cap_contactSensor_data_exit_now;

#if 0 // TODO/FIXME
/**
 * @brief component: outputA
 */
// @brief Physical|Virtual Relay / Switch A
caps_switch_data_t *cap_switch_a_data;


/**
 * @brief component: outputB
 */
// @brief Physical|Virtual Relay / Switch B
caps_switch_data_t *cap_switch_b_data;
#endif

/**
 * @brief component: disarm
 * Disarm panel.
 */
// @brief Alarm Disarm momentary button
caps_momentary_data_t *cap_momentary_data_disarm;


/**
 * Enable/Disable STSDK component.
 */
static void _cli_cmd_enable_event(char *string)
{
    ESP_LOGI(TAG, "%s: enable/disable STSDK", __func__);
    std::string arg;
    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        ad2_set_nv_slot_key_int(STSDK_ENABLE, 0, (arg[0] == 'Y' || arg[0] ==  'y'));
        ad2_printf_host("Success setting value. Restart required to take effect.\r\n");
    }

    // show contents of this slot
    int i;
    ad2_get_nv_slot_key_int(STSDK_ENABLE, 0, &i);
    ad2_printf_host("SmartThings SDK driver is '%s'.\r\n", (i ? "Enabled" : "Disabled"));

}

/**
 * Forget all STSDK state revert back to adoption mode.
 */
static void _cli_cmd_cleanup_event(char *string)
{
    ESP_LOGI(TAG, "%s: clean-up data with restart option", __func__);
    ad2_printf_host("Calling clean-up for STSDK settings.\r\n");
    st_conn_cleanup(ctx, true);
}

/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stserial <serialNumber>
 * ex.
 *   stserial AaBbCcDdEeFf...
 */
static void _cli_cmd_stserial_event(char *string)
{
    std::string arg;
    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        }
        err = nvs_set_str(my_handle, "SerialNum", arg.c_str());
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
            ad2_printf_host("Failed setting value.\r\n");
        } else {
            ad2_printf_host("Success setting value. Restart required to take effect.\r\n");
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        ad2_printf_host("Missing <serialNumber>\r\n");
    }
}

/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stpublickey <publicKey>
 * ex.
 *   stpublickey AaBbCcDdEeFf...
 */
static void _cli_cmd_stpublickey_event(char *string)
{
    std::string arg;
    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        }
        err = nvs_set_str(my_handle, "PublicKey", arg.c_str());
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
            ad2_printf_host("Failed setting value.\r\n");
        } else {
            ad2_printf_host("Success setting value. Restart required to take effect.\r\n");
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        ad2_printf_host("Missing <publicKey>\r\n");
    }
}

/**
 * Configure the AD2IoT SmartThings device_info.json
 *  field stored in NV
 *
 *  command: stprivatekey <privateKey>
 * ex.
 *   stprivatekey AaBbCcDdEeFf...
 */
static void _cli_cmd_stprivatekey_event(char *string)
{
    std::string arg;
    if (ad2_copy_nth_arg(arg, string, 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        }
        err = nvs_set_str(my_handle, "PrivateKey", arg.c_str());
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
            ad2_printf_host("Failed setting value.\r\n");
        } else {
            ad2_printf_host("Success setting value. Restart required to take effect.\r\n");
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        ad2_printf_host("Missing <privateKey>\r\n");
    }
}

char * STSDK_SETTINGS [] = {
    (char*)STSDK_ENABLE,
    (char*)STSDK_CLEANUP,
    (char*)STSDK_SERIAL,
    (char*)STSDK_PUBKEY,
    (char*)STSDK_PRIVKEY,
    0 // EOF
};

/**
 * @brief command list for module
 */
static struct cli_command stsdk_cmd_list[] = {
    {
        (char*)STSDK_ENABLE,(char*)
        "- Enable SmartThings component\r\n\r\n"
        "  ```" STSDK_ENABLE " {bool}```\r\n\r\n"
        "  - {bool}: [Y]es/[N]o\n\n", _cli_cmd_enable_event
    },
    {
        (char*)STSDK_CLEANUP,(char*)
        "- Cleanup NV data with restart option\r\n\r\n"
        "  ```" STSDK_CLEANUP "```\r\n\r\n", _cli_cmd_cleanup_event
    },
    {
        (char*)STSDK_SERIAL,(char*)
        "- Sets the SmartThings device_info serialNumber.\r\n\r\n"
        "  ```" STSDK_SERIAL " {serialNumber}```\r\n\r\n"
        "  Example: " STSDK_SERIAL " AaBbCcDdEeFfGg...\r\n\r\n", _cli_cmd_stserial_event
    },
    {
        (char*)STSDK_PUBKEY,(char*)
        "- Sets the SmartThings device_info publicKey.\r\n\r\n"
        "  ```" STSDK_PUBKEY " {publicKey}```\r\n\r\n"
        "  Example: " STSDK_PUBKEY " AaBbCcDdEeFfGg...\r\n\r\n", _cli_cmd_stpublickey_event
    },
    {
        (char*)STSDK_PRIVKEY,(char*)
        "- Sets the SmartThings device_info privateKey.\r\n\r\n"
        "  ```" STSDK_PRIVKEY " {privateKey}```\r\n\r\n"
        "  Example: " STSDK_PRIVKEY " AaBbCcDdEeFfGg...\r\n\r\n", _cli_cmd_stprivatekey_event
    },
};

#if 0 // TODO/FIXME
/**
 * @brief switch/relay state A
 */
int stsdk_get_switch_a_state(void)
{
    const char* switch_value = cap_switch_a_data->get_switch_value(cap_switch_a_data);
    int switch_state = SWITCH_OFF;

    if (!switch_value) {
        return -1;
    }

    if (!strcmp(switch_value, caps_helper_switch.attr_switch.value_on)) {
        switch_state = SWITCH_ON;
    } else if (!strcmp(switch_value, caps_helper_switch.attr_switch.value_off)) {
        switch_state = SWITCH_OFF;
    }
    return switch_state;
}

/**
 * @brief get switch/relay state B
 */
int stsdk_get_switch_b_state(void)
{
    const char* switch_value = cap_switch_b_data->get_switch_value(cap_switch_b_data);
    int switch_state = SWITCH_OFF;

    if (!switch_value) {
        return -1;
    }

    if (!strcmp(switch_value, caps_helper_switch.attr_switch.value_on)) {
        switch_state = SWITCH_ON;
    } else if (!strcmp(switch_value, caps_helper_switch.attr_switch.value_off)) {
        switch_state = SWITCH_OFF;
    }
    return switch_state;
}
#endif

/**
 * @brief ST Cloud app arm away momentary button pushed
 * send panel disarm command.
 */
void cap_securitySystem_arm_away_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    // send ARM AWAY commands to the panel
    ad2_arm_away(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief ST Cloud app arm stay momentary button pushed
 * send panel disarm command.
 */
void cap_securitySystem_arm_stay_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    // send ARM STAY commands to the panel
    ad2_arm_stay(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief ST Cloud app disarm momentary button pushed
 * send panel disarm command.
 */
void cap_securitySystem_disarm_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    // send DISARM command to the panel
    ad2_disarm(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief ST Cloud app chime momentary button pushed
 * send panel toggle chime commands.
 */
void cap_securitySystem_chime_cmd_cb(struct caps_momentary_data *caps_data)
{
    // send CHIME toggle command to the panel.
    ad2_chime_toggle(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief Clear the fire trigger counter
 */
static int fire_trigger_count = 0;
static TimerHandle_t fire_trigger_timer = nullptr;
void fire_trigger_clear(TimerHandle_t *arg)
{
    xTimerDelete(fire_trigger_timer, 1000 / portTICK_PERIOD_MS);
    fire_trigger_count = 0;
    fire_trigger_timer = nullptr;
    ESP_LOGI(TAG, "Clearing fire panic counter.");
}

/**
 * @brief ST Cloud app fire alarm panic button pushed
 * trigger a fire alarm! Are you sure?
 */
void cap_fire_cmd_cb(struct caps_momentary_data *caps_data)
{
    fire_trigger_count++;
    ESP_LOGI(TAG, "Fire trigger count %i", fire_trigger_count);
    if (fire_trigger_count >= 3) {
        ESP_LOGI(TAG, "Sending fire signal to panel.");
        // send a fire panic F1 button to the panel.
        ad2_fire_alarm(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
    } else {
        if (fire_trigger_timer) {
            xTimerReset(fire_trigger_timer, 1000 / portTICK_PERIOD_MS);
        } else {
            fire_trigger_timer = xTimerCreate( "FIRE_TRIG",
                                               ( 5000 / portTICK_PERIOD_MS),
                                               pdFALSE,
                                               (void *) nullptr,
                                               (TimerCallbackFunction_t) fire_trigger_clear
                                             );
            if (fire_trigger_timer == nullptr) {
                ESP_LOGE(TAG, "Error creating fire trigger timer");
            } else {
                xTimerStart(fire_trigger_timer, 0);
                ESP_LOGI(TAG, "Starting fire trigger timer");
            }
        }
    }
}

/**
 * @brief Clear the panic trigger counter
 */
static int panic_trigger_count = 0;
static TimerHandle_t panic_trigger_timer = nullptr;
void panic_trigger_clear(TimerHandle_t *arg)
{
    xTimerDelete(panic_trigger_timer, 1000 / portTICK_PERIOD_MS);
    panic_trigger_count = 0;
    panic_trigger_timer = nullptr;
    ESP_LOGI(TAG, "Clearing panic alarm counter.");
}

/**
 * @brief ST Cloud app panic alarm momentary button pushed
 * trigger a panic alarm! Are you sure?
 */
void cap_panic_alarm_cmd_cb(struct caps_momentary_data *caps_data)
{
    panic_trigger_count++;
    ESP_LOGI(TAG, "Panic alarm trigger count %i", panic_trigger_count);
    if (panic_trigger_count >= 3) {
        ESP_LOGI(TAG, "Sending panic alarm signal to panel.");
        // send PANIC command to the panel.
        ad2_panic_alarm(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
    } else {
        if (panic_trigger_timer) {
            xTimerReset(panic_trigger_timer, 1000 / portTICK_PERIOD_MS);
        } else {
            panic_trigger_timer = xTimerCreate( "PANIC_TRIG",
                                                ( 5000 / portTICK_PERIOD_MS),
                                                pdFALSE,
                                                (void *) nullptr,
                                                (TimerCallbackFunction_t) panic_trigger_clear
                                              );
            if (panic_trigger_timer == nullptr) {
                ESP_LOGE(TAG, "Error creating panic trigger timer");
            } else {
                xTimerStart(panic_trigger_timer, 0);
                ESP_LOGI(TAG, "Starting panic trigger timer");
            }
        }
    }
}


/**
 * @brief Clear the aux trigger counter
 */
static int aux_trigger_count = 0;
static TimerHandle_t aux_trigger_timer = nullptr;
void aux_trigger_clear(TimerHandle_t *arg)
{
    xTimerDelete(aux_trigger_timer, 1000 / portTICK_PERIOD_MS);
    aux_trigger_count = 0;
    aux_trigger_timer = nullptr;
    ESP_LOGI(TAG, "Clearing aux alarm counter.");
}

/**
 * @brief ST Cloud app AUX(Medical) aux alarm momentary button pushed
 * send panel AUX aux commands.
 */
void cap_aux_alarm_cmd_cb(struct caps_momentary_data *caps_data)
{
    aux_trigger_count++;
    ESP_LOGI(TAG, "AUX alarm trigger count %i", aux_trigger_count);
    if (aux_trigger_count >= 3) {
        ESP_LOGI(TAG, "Sending aux alarm signal to panel.");
        // send AUX ALARM command to the panel.
        ad2_aux_alarm(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
    } else {
        if (aux_trigger_timer) {
            xTimerReset(aux_trigger_timer, 1000 / portTICK_PERIOD_MS);
        } else {
            aux_trigger_timer = xTimerCreate( "AUX_TRIG",
                                              ( 5000 / portTICK_PERIOD_MS),
                                              pdFALSE,
                                              (void *) nullptr,
                                              (TimerCallbackFunction_t) aux_trigger_clear
                                            );
            if (aux_trigger_timer == nullptr) {
                ESP_LOGE(TAG, "Error creating aux trigger timer");
            } else {
                xTimerStart(aux_trigger_timer, 0);
                ESP_LOGI(TAG, "Starting aux trigger timer");
            }
        }
    }
}

/**
 * @brief ST Cloud app Arm Stay momentary button pushed
 * send panel Arm Stay commands.
 */
void cap_arm_stay_cmd_cb(struct caps_momentary_data *caps_data)
{
    // send ARM stay command to the panel.
    ad2_arm_stay(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief ST Cloud app Arm Away momentary button pushed
 * send panel Arm Away commands.
 */
void cap_arm_away_cmd_cb(struct caps_momentary_data *caps_data)
{
    // send ARM AWAY command to the panel.
    ad2_arm_away(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * @brief ST Cloud app Exit Now momentary button pushed
 * send panel Exit Now commands.
 */
void cap_exit_now_cmd_cb(struct caps_momentary_data *caps_data)
{
    // send EXIT NOW command to the panel.
    ad2_exit_now(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

#if 0 // TODO/FIXME
/**
 * @brief callback for cloud update to switch A
 */
void cap_switch_a_cmd_cb(struct caps_switch_data *caps_data)
{
    int switch_state = stsdk_get_switch_a_state();
    hal_change_switch_a_state(switch_state);
}

/**
 * @brief callback for cloud update to switch B
 */
void cap_switch_b_cmd_cb(struct caps_switch_data *caps_data)
{
    caps_data->get_switch_value(caps_data);

    int switch_state = stsdk_get_switch_b_state();
    hal_change_switch_b_state(switch_state);
}
#endif

/**
 * @brief set available version ota capabilty.
 */
void cap_available_version_set(char *available_version)
{
    int32_t sequence_no = 0;

    if (!available_version) {
        ESP_LOGE(TAG, "invalid parameter");
        return;
    }

    /* Send avail version to ota cap handler */
    ST_CAP_SEND_ATTR_STRING(ota_cap_handle, (char*)"availableVersion", available_version, NULL, NULL, sequence_no);
    if (sequence_no < 0) {
        ESP_LOGE(TAG, "failed to send init_data");
    }

}

/**
 * @brief ST Cloud app disarm momentary button pushed
 * send panel disarm commands.
 */
void cap_disarm_cmd_cb(struct caps_momentary_data *caps_data)
{
    // send DISARM using defaults command to the panel.
    ad2_disarm(AD2_DEFAULT_CODE_SLOT, AD2_DEFAULT_VPA_SLOT);
}

/**
 * AlarmDecoder API subscriber callback functions.
 */

/**
 * @brief ON_FIRMWARE_VERSION
 * Called when a new firmware version is available.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_new_firmware_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "NEW FIRMWARE: '%s'", msg->c_str());
    cap_available_version_set((char*)msg->c_str());
}

/**
 * @brief ON_ARM.
 * Called when the alarm panel is armed.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_arm_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_ARM: '%s'", msg->c_str());
        if (s->armed_stay) {
#if 0 // TODO/FIXME
            cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, caps_helper_securitySystem.attr_securitySystemStatus.value_armedStay);
#endif
            cap_contactSensor_data_arm_stay->set_contact_value(cap_contactSensor_data_arm_stay, caps_helper_contactSensor.attr_contact.value_open);
            cap_contactSensor_data_arm_stay->attr_contact_send(cap_contactSensor_data_arm_stay);

            cap_contactSensor_data_arm_away->set_contact_value(cap_contactSensor_data_arm_away, caps_helper_contactSensor.attr_contact.value_closed);
            cap_contactSensor_data_arm_away->attr_contact_send(cap_contactSensor_data_arm_away);

        } else {
#if 0 // TODO/FIXME
            cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, caps_helper_securitySystem.attr_securitySystemStatus.value_armedAway);
#endif
            cap_contactSensor_data_arm_away->set_contact_value(cap_contactSensor_data_arm_away, caps_helper_contactSensor.attr_contact.value_open);
            cap_contactSensor_data_arm_away->attr_contact_send(cap_contactSensor_data_arm_away);

            cap_contactSensor_data_arm_stay->set_contact_value(cap_contactSensor_data_arm_stay, caps_helper_contactSensor.attr_contact.value_closed);
            cap_contactSensor_data_arm_stay->attr_contact_send(cap_contactSensor_data_arm_stay);

        }
#if 0 // TODO/FIXME
        cap_securitySystem_data->attr_securitySystemStatus_send(cap_securitySystem_data);
#endif
    }
}

/**
 * @brief ON_DISARM.
 * Called when the alarm panel is disarmed.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_disarm_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_DISARM: '%s'", msg->c_str());

#if 0 // TODO/FIXME
        cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed);
        cap_securitySystem_data->attr_securitySystemStatus_send(cap_securitySystem_data);
#endif
        // Turn both off.
        cap_contactSensor_data_arm_away->set_contact_value(cap_contactSensor_data_arm_away, caps_helper_contactSensor.attr_contact.value_closed);
        cap_contactSensor_data_arm_away->attr_contact_send(cap_contactSensor_data_arm_away);
        cap_contactSensor_data_arm_stay->set_contact_value(cap_contactSensor_data_arm_stay, caps_helper_contactSensor.attr_contact.value_closed);
        cap_contactSensor_data_arm_stay->attr_contact_send(cap_contactSensor_data_arm_stay);
    }
}

/**
 * @brief ON_CHIME_CHANGE.
 * Called when the alarm panel CHIME state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_chime_change_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_CHIME_CHANGE: '%s'", msg->c_str());
        if ( s->chime_on ) {
            cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, caps_helper_contactSensor.attr_contact.value_open);
        } else {
            cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, caps_helper_contactSensor.attr_contact.value_closed);
        }

        cap_contactSensor_data_chime->attr_contact_send(cap_contactSensor_data_chime);
    }
}

/**
 * @brief ON_FIRE.
 * Called when the alarm panel FIRE alarm state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_fire_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_FIRE: '%s'", msg->c_str());
        if ( s->fire_alarm ) {
            cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, caps_helper_smokeDetector.attr_smoke.value_detected);
        } else {
            cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, caps_helper_smokeDetector.attr_smoke.value_clear);
        }

        cap_smokeDetector_data->attr_smoke_send(cap_smokeDetector_data);
    }
}

/**
 * @brief ON_POWER_CHANGE.
 * Called when the alarm panel AC power stage changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_power_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_POWER_CHANGE: '%i'", s->ac_power);
        if ( s->ac_power ) {
            cap_powerSource_data->set_powerSource_value(cap_powerSource_data, caps_helper_powerSource.attr_powerSource.value_mains);
        } else {
            cap_powerSource_data->set_powerSource_value(cap_powerSource_data, caps_helper_powerSource.attr_powerSource.value_battery);
        }

        cap_powerSource_data->attr_powerSource_send(cap_powerSource_data);
    }
}

/**
 * @brief ON_LOW_BATTERY.
 * Called when the alarm panel BATTERY LOW event.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_low_battery_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_LOW_BATTERY: '%i'", s->battery_low);

        if ( s->battery_low ) {
            cap_battery_data->set_battery_value(cap_battery_data, 0);
        } else {
            cap_battery_data->set_battery_value(cap_battery_data, 100);
        }

        cap_battery_data->attr_battery_send(cap_battery_data);
    }
}

/**
 * @brief ON_ALARM_CHANGE.
 * Called when the alarm panel ALARM BELL state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_alarm_change_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_ALARM_CHANGE: '%i'", s->alarm_sounding);
        const char *alarm_value = caps_helper_alarm.attr_alarm.value_off;
        cap_alarm_bell_data->set_alarm_value(cap_alarm_bell_data, alarm_value);
        if ( s->alarm_sounding ) {
            alarm_value = caps_helper_alarm.attr_alarm.value_siren;
        }
        cap_alarm_bell_data->set_alarm_value(cap_alarm_bell_data, alarm_value);
        cap_alarm_bell_data->attr_alarm_send(cap_alarm_bell_data);
    }
}

/**
 * @brief ON_ZONE_BYPASSED_CHANGE.
 * Called when the alarm panel ZONE BYPASSED state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_zone_bypassed_change_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_ZONE_BYPASSED_CHANGE: '%i'", s->zone_bypassed);
        if ( s->zone_bypassed ) {
            cap_contactSensor_data_zone_bypassed->set_contact_value(cap_contactSensor_data_zone_bypassed, caps_helper_contactSensor.attr_contact.value_open);
        } else {
            cap_contactSensor_data_zone_bypassed->set_contact_value(cap_contactSensor_data_zone_bypassed, caps_helper_contactSensor.attr_contact.value_closed);
        }

        cap_contactSensor_data_zone_bypassed->attr_contact_send(cap_contactSensor_data_zone_bypassed);
    }
}

/**
 * @brief ON_EXIT_NOW_CHANGE.
 * Called when the alarm panel EXIT NOW state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_exit_now_change_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_EXIT_NOW_CHANGE: '%i'", s->exit_now);
        if ( s->exit_now ) {
            cap_contactSensor_data_exit_now->set_contact_value(cap_contactSensor_data_exit_now, caps_helper_contactSensor.attr_contact.value_open);
        } else {
            cap_contactSensor_data_exit_now->set_contact_value(cap_contactSensor_data_exit_now, caps_helper_contactSensor.attr_contact.value_closed);
        }

        cap_contactSensor_data_exit_now->attr_contact_send(cap_contactSensor_data_exit_now);
    }
}


/**
 * @brief ON_READY_CHANGE
 * Called when the alarm panel READY TO ARM state changes.
 *
 * @param [in]msg std::string new version string.
 * @param [in]s nullptr
 * @param [in]arg nullptr.
 *
 */
void on_ready_to_arm_change_cb(std::string *msg, AD2VirtualPartitionState *s, void *arg)
{
    // @brief Only listen to events for the default partition we are watching.
    AD2VirtualPartitionState *defs = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if ((s && defs) && s->partition == defs->partition) {
        ESP_LOGI(TAG, "ON_READY_CHANGE: '%i'", s->ready);
        if ( s->ready ) {
            cap_contactSensor_data_ready_to_arm->set_contact_value(cap_contactSensor_data_ready_to_arm, caps_helper_contactSensor.attr_contact.value_open);
        } else {
            cap_contactSensor_data_ready_to_arm->set_contact_value(cap_contactSensor_data_ready_to_arm, caps_helper_contactSensor.attr_contact.value_closed);
        }

        cap_contactSensor_data_ready_to_arm->attr_contact_send(cap_contactSensor_data_ready_to_arm);
    }
}


/**
 * @brief Initialize the STSDK engine
 */
void stsdk_init(void)
{
    unsigned char *onboarding_config = (unsigned char *) onboarding_config_start;
    unsigned int onboarding_config_len = onboarding_config_end - onboarding_config_start;
    unsigned char *device_info = (unsigned char *) device_info_start;
    unsigned int device_info_len = device_info_end - device_info_start;
    int iot_err;

    // register STSDK CLI commands
    for (int i = 0; i < ARRAY_SIZE(stsdk_cmd_list); i++) {
        cli_register_command(&stsdk_cmd_list[i]);
    }

    int enabled = 0;
    ad2_get_nv_slot_key_int(STSDK_ENABLE, 0, &enabled);

    // nothing more needs to be done once commands are set if not enabled.
    if (!enabled) {
        ESP_LOGI(TAG, "STSDK disabled");
        return;
    }

    // create a iot context
    ctx = st_conn_init(onboarding_config, onboarding_config_len, device_info, device_info_len);
    if (ctx != NULL) {
        iot_err = st_conn_set_noti_cb(ctx, iot_noti_cb, NULL);
        if (iot_err) {
            ESP_LOGE(TAG, "failed to set notification callback function");
        }
    } else {
        ESP_LOGE(TAG, "failed to create the iot_context");
    }

    // Add AD2 capabilies
    capability_init();

    // subscribe to firmware updates available events.
    AD2Parse.subscribeTo(ON_FIRMWARE_VERSION, on_new_firmware_cb, nullptr);

    // Subscribe to ARM/DISARM events to update cap_securitySystem
    AD2Parse.subscribeTo(ON_ARM, on_arm_cb, nullptr);

    // Subscribe to DISARM events to update cap_securitySystem
    AD2Parse.subscribeTo(ON_DISARM, on_disarm_cb, nullptr);

    // Subscribe to CHIME events to update the chime contact capability.
    AD2Parse.subscribeTo(ON_CHIME_CHANGE, on_chime_change_cb, nullptr);

    // Subscribe to FIRE events to update cap_smokeDetector capability.
    AD2Parse.subscribeTo(ON_FIRE, on_fire_cb, nullptr);

    // Subscribe to AC_POWER change events to update cap_powerSource capability.
    AD2Parse.subscribeTo(ON_POWER_CHANGE, on_power_cb, nullptr);

    // Subscribe to ON_LOW_BATTERY change events to update cap_battery capability.
    AD2Parse.subscribeTo(ON_LOW_BATTERY, on_low_battery_cb, nullptr);

    // Subscribe to ON_ALARM_CHANGE change events to update cap_alarm capability.
    AD2Parse.subscribeTo(ON_ALARM_CHANGE, on_alarm_change_cb, nullptr);

    // Subscribe to ON_ZONE_BYPASSED_CHANGE change events to update zone bypassed capability.
    AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, on_zone_bypassed_change_cb, nullptr);

    // Subscribe to ON_READY_CHANGE change events to update zone bypassed capability.
    AD2Parse.subscribeTo(ON_READY_CHANGE, on_ready_to_arm_change_cb, nullptr);

    // Subscribe to ON_EXIT_CHANGE change events to update zone bypassed capability.
    AD2Parse.subscribeTo(ON_EXIT_CHANGE, on_exit_now_change_cb, nullptr);
}

/**
 * Initialize all SmartThings capabilities.
 */
void capability_init()
{
    ESP_LOGI(TAG, "capability_init");
    int iot_err;

    /**
     * @brief component: main
     */

    // refresh device type init
    refresh_cap_handle = st_cap_handle_init(ctx, "main", "refresh", NULL, NULL);
    if (refresh_cap_handle) {
        iot_err = st_cap_cmd_set_cb(refresh_cap_handle, "refresh", refresh_cmd_cb, NULL);
        if (iot_err) {
            ESP_LOGE(TAG, "failed to set cmd_cb for refresh");
        }
    }
    // healthCheck capabilities init
    healthCheck_cap_handle = st_cap_handle_init(ctx, "main", "healthCheck", cap_health_check_init_cb, NULL);
    if (healthCheck_cap_handle) {
        iot_err = st_cap_cmd_set_cb(healthCheck_cap_handle, "ping", refresh_cmd_cb, NULL);
        if (iot_err) {
            ESP_LOGE(TAG, "failed to set cmd_cb for healthCheck");
        }
    }
    // firmwareUpdate capabilities init
    ota_cap_handle = st_cap_handle_init(ctx, "main", "firmwareUpdate", cap_current_version_init_cb, NULL);
    if (ota_cap_handle) {
        iot_err = st_cap_cmd_set_cb(ota_cap_handle, "updateFirmware", update_firmware_cmd_cb, NULL);
        if (iot_err) {
            ESP_LOGE(TAG, "failed to set cmd_cb for updateFirmware");
        }
    }

#if 0 // TODO/FIXME
    // securitySystem device type init
    // FIXME: Broken in the ST cloud...
    cap_securitySystem_data = caps_securitySystem_initialize(ctx, "main", NULL, NULL);
    if (cap_securitySystem_data) {
        const char *init_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
        cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, init_value);
        cap_securitySystem_data->cmd_armAway_usr_cb = cap_securitySystem_arm_away_cmd_cb;
        cap_securitySystem_data->cmd_armStay_usr_cb = cap_securitySystem_arm_stay_cmd_cb;
        cap_securitySystem_data->cmd_disarm_usr_cb = cap_securitySystem_disarm_cmd_cb;
    }
#endif

#if 0 // TODO/FIXME
    // Arm|Disarm|Exit button(s) device type init
    cap_buttonMainSecurityActions_data = caps_button_initialize(ctx, "main", NULL, NULL);
    if (cap_buttonMainSecurityActions_data) {
        cap_buttonMainSecurityActions_data->set_numberOfButtons_value(cap_buttonMainSecurityActions_data, 4);
        const char *values[] = { "pushed" };
        cap_buttonMainSecurityActions_data->set_supportedButtonValues_value(cap_buttonMainSecurityActions_data, values, ARRAY_SIZE(values));
    }
#endif

    // Power source init
    cap_powerSource_data = caps_powerSource_initialize(ctx, "main", NULL, NULL);
    if (cap_powerSource_data) {
        AD2VirtualPartitionState * s = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
        const char *init_value = caps_helper_powerSource.attr_powerSource.value_unknown;
        if (s != nullptr) {
            if (s->ac_power) {
                init_value = caps_helper_powerSource.attr_powerSource.value_mains;
            } else {
                init_value = caps_helper_powerSource.attr_powerSource.value_battery;
            }
        }
        cap_powerSource_data->set_powerSource_value(cap_powerSource_data, init_value);
    }
    // Battery init
    cap_battery_data = caps_battery_initialize(ctx, "main", NULL, NULL);
    if (cap_battery_data) {
        AD2VirtualPartitionState * s = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);

        /* @brief initial value of 100% battery as this is most likely the case at boot. */
        int init_value = 100;

        if (s != nullptr) {
            if (s->battery_low) {
                init_value = 0;
            }
        }
        cap_battery_data->set_battery_value(cap_battery_data, init_value);
    }


    /**
     * @brief component: chime
     */
    // Chime contact sensor ( visual for state )
    cap_contactSensor_data_chime = caps_contactSensor_initialize(ctx, "chime", NULL, NULL);
    if (cap_contactSensor_data_chime) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, contact_init_value);
    }
    // Chime Momentary button: toggle chime mode.
    cap_momentary_data_chime = caps_momentary_initialize(ctx, "chime", NULL, NULL);
    if (cap_momentary_data_chime) {
        cap_momentary_data_chime->cmd_push_usr_cb = cap_securitySystem_chime_cmd_cb;
    }

    /*
     * @brief component: fire
     */
    // Smoke Detector
    cap_smokeDetector_data = caps_smokeDetector_initialize(ctx, "fire", NULL, NULL);
    if (cap_smokeDetector_data) {
        const char *smoke_init_value = caps_helper_smokeDetector.attr_smoke.value_clear;
        cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, smoke_init_value);
    }
    // Fire Alarm panic Momentary button
    cap_momentary_data_fire = caps_momentary_initialize(ctx, "fire", NULL, NULL);
    if (cap_momentary_data_fire) {
        cap_momentary_data_fire->cmd_push_usr_cb = cap_fire_cmd_cb;
    }

    /**
     * @brief component: alarm
     */
    // Alarm panel Bell status.
    cap_alarm_bell_data = caps_alarm_initialize(ctx, "alarm", NULL, NULL);
    if (cap_alarm_bell_data) {
        const char *alarm_init_value = caps_helper_alarm.attr_alarm.value_off;
        cap_alarm_bell_data->set_alarm_value(cap_alarm_bell_data, alarm_init_value);
    }
    // Panic Alarm Momentary button
    cap_momentary_data_panic_alarm = caps_momentary_initialize(ctx, "alarm", NULL, NULL);
    if (cap_momentary_data_panic_alarm) {
        cap_momentary_data_panic_alarm->cmd_push_usr_cb = cap_panic_alarm_cmd_cb;
    }
    // TODO: carbon monoxide alarm. Will require LRR process and wont work on all systems.

    /**
     * @brief component: auxalarm
     * AUX Medical alarm.
     */
    // @brief AUX Alarm momentary panic button
    cap_momentary_data_aux_alarm = caps_momentary_initialize(ctx, "auxalarm", NULL, NULL);
    if (cap_momentary_data_aux_alarm) {
        cap_momentary_data_aux_alarm->cmd_push_usr_cb = cap_aux_alarm_cmd_cb;
    }

    /**
     * @brief component: armstay
     * Arm Stay control.
     */
    // Arm Stay contact sensor ( visual for state )
    cap_contactSensor_data_arm_stay = caps_contactSensor_initialize(ctx, "armstay", NULL, NULL);
    if (cap_contactSensor_data_arm_stay) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_arm_stay->set_contact_value(cap_contactSensor_data_arm_stay, contact_init_value);
    }
    // @brief Arm Stay momentary button
    cap_momentary_data_arm_stay = caps_momentary_initialize(ctx, "armstay", NULL, NULL);
    if (cap_momentary_data_arm_stay) {
        cap_momentary_data_arm_stay->cmd_push_usr_cb = cap_arm_stay_cmd_cb;
    }

    /**
     * @brief component: armaway
     * Arm Away control.
     */
    // Arm Stay contact sensor ( visual for state )
    cap_contactSensor_data_arm_away = caps_contactSensor_initialize(ctx, "armaway", NULL, NULL);
    if (cap_contactSensor_data_arm_away) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_arm_away->set_contact_value(cap_contactSensor_data_arm_away, contact_init_value);
    }
    // @brief Arm Away momentary button
    cap_momentary_data_arm_away = caps_momentary_initialize(ctx, "armaway", NULL, NULL);
    if (cap_momentary_data_arm_away) {
        cap_momentary_data_arm_away->cmd_push_usr_cb = cap_arm_away_cmd_cb;
    }

#if 0 // TODO
    /**
     * @brief component: outputA
     */
    // Output capabilies that can be tied to local Physical GPIO tied to a Relay.
    cap_switch_a_data = caps_switch_initialize(ctx, "outputA", NULL, NULL);
    if (cap_switch_a_data) {
        // FIXME: const char *init_value = caps_helper_switch.attr_switch.value_on;
        cap_switch_a_data->cmd_on_usr_cb = cap_switch_a_cmd_cb;
        cap_switch_a_data->cmd_off_usr_cb = cap_switch_a_cmd_cb;
        int val = hal_get_switch_a_state();
        const char *switch_init_value;
        if (val) {
            switch_init_value = caps_helper_switch.attr_switch.value_on;
        } else {
            switch_init_value = caps_helper_switch.attr_switch.value_off;
        }
        cap_switch_a_data->set_switch_value(cap_switch_a_data, switch_init_value);
    }

    /**
     * @brief component: outputB
     */
    // Output capabilies that can be tied to local Physical GPIO tied to a Relay.
    cap_switch_b_data = caps_switch_initialize(ctx, "outputB", NULL, NULL);
    if (cap_switch_b_data) {
        // FIXME: const char *init_value = caps_helper_switch.attr_switch.value_on;
        cap_switch_b_data->cmd_on_usr_cb = cap_switch_b_cmd_cb;
        cap_switch_b_data->cmd_off_usr_cb = cap_switch_b_cmd_cb;
        int val = hal_get_switch_b_state();
        const char *switch_init_value;
        if (val) {
            switch_init_value = caps_helper_switch.attr_switch.value_on;
        } else {
            switch_init_value = caps_helper_switch.attr_switch.value_off;
        }
        cap_switch_b_data->set_switch_value(cap_switch_b_data, switch_init_value);
    }
#endif

    /**
     * @brief component: bypass
     */
    // bypass contact sensor ( visual for state )
    cap_contactSensor_data_zone_bypassed = caps_contactSensor_initialize(ctx, "bypass", NULL, NULL);
    if (cap_contactSensor_data_zone_bypassed) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_zone_bypassed->set_contact_value(cap_contactSensor_data_zone_bypassed, contact_init_value);
    }

    /**
     * @brief component: ready
     */
    // ready contact sensor ( visual for state )
    cap_contactSensor_data_ready_to_arm = caps_contactSensor_initialize(ctx, "ready", NULL, NULL);
    if (cap_contactSensor_data_ready_to_arm) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_ready_to_arm->set_contact_value(cap_contactSensor_data_ready_to_arm, contact_init_value);
    }

    /**
     * @brief component: exit
     */
    // 'exit now' contact sensor ( visual for state )
    cap_contactSensor_data_exit_now = caps_contactSensor_initialize(ctx, "exit", NULL, NULL);
    if (cap_contactSensor_data_exit_now) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_exit_now->set_contact_value(cap_contactSensor_data_exit_now, contact_init_value);
    }
    // @brief Exit Now button
    cap_momentary_data_exit_now = caps_momentary_initialize(ctx, "exit", NULL, NULL);
    if (cap_momentary_data_exit_now) {
        cap_momentary_data_exit_now->cmd_push_usr_cb = cap_exit_now_cmd_cb;
    }

    /**
     * @brief component: disarm
     */
    // Disarm Alarm Momentary button
    cap_momentary_data_disarm = caps_momentary_initialize(ctx, "disarm", NULL, NULL);
    if (cap_momentary_data_disarm) {
        cap_momentary_data_disarm->cmd_push_usr_cb = cap_disarm_cmd_cb;
    }

}

/**
 * @brief status callback for STSDK api.
 */
void iot_status_cb(iot_status_t status,
                   iot_stat_lv_t stat_lv, void *usr_data)
{
    g_iot_status = status;
    g_iot_stat_lv = stat_lv;

    ESP_LOGI(TAG, "iot_status_cb %d, stat: %d", g_iot_status, g_iot_stat_lv);

    // Because ST takes control of the WiFi
    // let everyone know it is up and connected.
    if (status == IOT_STATUS_CONNECTING) {
        g_ad2_network_state = AD2_CONNECTED;
    } else {
        g_ad2_network_state = AD2_OFFLINE;
    }

    switch(status) {
    case IOT_STATUS_NEED_INTERACT:
        noti_led_mode = LED_ANIMATION_MODE_FAST;
        break;
    case IOT_STATUS_IDLE:
    case IOT_STATUS_CONNECTING:
        noti_led_mode = LED_ANIMATION_MODE_IDLE;

#if 0 // TODO/FIXME
        // Sync local hw switches to cloud states.
        hal_change_switch_a_state(stsdk_get_switch_a_state());
        hal_change_switch_b_state(stsdk_get_switch_b_state());
#endif
        break;
    default:
        break;
    }
}

#if defined(SET_PIN_NUMBER_CONFRIM)
void* pin_num_memcpy(void *dest, const void *src, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        *((char*)dest + i) = *((char*)src + i);
    }
    return dest;
}
#endif

/**
 * @brief start connection to SmartThings MQTT server
 */
void stsdk_connection_start(void)
{
    iot_pin_t *pin_num = NULL;
    int err;

#if defined(SET_PIN_NUMBER_CONFRIM)
    pin_num = (iot_pin_t *) malloc(sizeof(iot_pin_t));
    if (!pin_num) {
        ESP_LOGE(TAG, "failed to malloc for iot_pin_t");
    }

    // to decide the pin confirmation number(ex. "12345678"). It will use for easysetup.
    //    pin confirmation number must be 8 digit number.
    pin_num_memcpy(pin_num, "12345678", sizeof(iot_pin_t));
#endif

    // process on-boarding procedure. There is nothing more to do on the app side than call the API.
    err = st_conn_start(ctx, (st_status_cb)&iot_status_cb, IOT_STATUS_ALL, NULL, pin_num);
    if (err) {
        ESP_LOGE(TAG, "failed to start connection. err:%d", err);
    }
    if (pin_num) {
        free(pin_num);
    }
}

/**
 * @brief actual stsdk connection task.
 */
void stsdk_connection_start_task(void *arg)
{
    stsdk_connection_start();
    vTaskDelete(NULL);
}

/**
 * @brief STSDK notifications callback.
 */
void iot_noti_cb(iot_noti_data_t *noti_data, void *noti_usr_data)
{
    ESP_LOGI(TAG, "Notification message received");

    if (noti_data->type == IOT_NOTI_TYPE_DEV_DELETED) {
        ESP_LOGI(TAG, "[device deleted]");
    } else if (noti_data->type == IOT_NOTI_TYPE_RATE_LIMIT) {
        ESP_LOGI(TAG, "[rate limit] Remaining time:%d, sequence number:%d",
                 noti_data->raw.rate_limit.remainingTime, noti_data->raw.rate_limit.sequenceNumber);
    }
}


/**
 * @brief Callback on init of health check capability.
 */
void cap_health_check_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 0;

    ST_CAP_SEND_ATTR_NUMBER(handle, "checkInterval", 60, NULL, NULL, sequence_no);
    if (sequence_no < 0) {
        ESP_LOGE(TAG, "failed to send init_data");
    }

}

/**
 * @brief Callback on init of version capability.
 */
void cap_current_version_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    int32_t sequence_no = 0;

    /* Setup switch on state */
    // FIXME: get from device_info ctx->device_info->firmware_version
    // that is loaded from device_info.json

    /* Send avail version to ota cap handler */
    ST_CAP_SEND_ATTR_STRING(handle, (char*)"availableVersion", (char *)FIRMWARE_VERSION, NULL, NULL, sequence_no);
    if (sequence_no < 0) {
        ESP_LOGE(TAG, "failed to send init_data");
    }

}

/**
 * @brief refresh callback from ST refresh capability on app.
 * Will be called on refresh(swipe) on main device screen.
 */
void refresh_cmd_cb(IOT_CAP_HANDLE *handle,
                    iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    ESP_LOGI(TAG, "refresh_cmd_cb");

    // get the default partition state.
    // FIXME: using DEFAULT slot for now. Needs to be configurable
    // and if possible selectable from ST App so partition can be
    // selected from a list.
    AD2VirtualPartitionState * s = ad2_get_partition_state(AD2_DEFAULT_VPA_SLOT);
    if (s != nullptr) {
        std::string statestr = "REFRESH";
        if (s->armed_stay || s->armed_away) {
            on_arm_cb(&statestr, s, nullptr);
        } else {
            on_disarm_cb(&statestr, s, nullptr);
        }
        on_chime_change_cb(&statestr, s, nullptr);
    } else {
        ESP_LOGE(TAG, "vpaddr[%u] not found", AD2_DEFAULT_VPA_SLOT);
    }
}

void update_firmware_cmd_cb(IOT_CAP_HANDLE *handle,
                            iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    ota_do_update(nullptr);
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif /*  CONFIG_STDK_IOT_CORE */

