/**
 *  @file    stsdk_main.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *  @version 1.0
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"
static const char *TAG = "STSDK";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"

// specific includes
#include "stsdk_main.h"

#ifdef __cplusplus
extern "C" {
#endif


iot_status_t g_iot_status = IOT_STATUS_IDLE;
iot_stat_lv_t g_iot_stat_lv;
IOT_CTX* ctx = NULL;

//#define SET_PIN_NUMBER_CONFRIM

// onboarding_config_start is null-terminated string
extern const uint8_t onboarding_config_start[]  asm("_binary_onboarding_config_json_start");
extern const uint8_t onboarding_config_end[]    asm("_binary_onboarding_config_json_end");

// device_info_start is null-terminated string
extern const uint8_t device_info_start[]        asm("_binary_device_info_json_start");
extern const uint8_t device_info_end[]          asm("_binary_device_info_json_end");

caps_switch_data_t *cap_switch_data;
caps_contactSensor_data_t *cap_contactSensor_data_chime;
caps_momentary_data_t *cap_momentary_data_chime;

caps_securitySystem_data_t *cap_securitySystem_data;

caps_smokeDetector_data_t *cap_smokeDetector_data;
caps_momentary_data_t *cap_momentary_data_fire;

IOT_CAP_HANDLE *ota_cap_handle = NULL;
IOT_CAP_HANDLE *healthCheck_cap_handle = NULL;
IOT_CAP_HANDLE *refresh_cap_handle = NULL;

/**
 * Forget all STSDK state revert back to adoption mode.
 */
static void _cli_cmd_cleanup_event(char *string)
{
    ESP_LOGI(TAG, "%s: clean-up data with reboot option", __func__);
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
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "SerialNum", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <serialNumber>\n");
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
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "PublicKey", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <publicKey>\n");
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
    char arg[80];
    if (ad2_copy_nth_arg(arg, string, sizeof(arg), 1) >= 0) {
        nvs_handle my_handle;
        esp_err_t err;
        err = nvs_open_from_partition("stnv", "stdk", NVS_READWRITE, &my_handle);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_open_from_partition err %d", __func__, err);
        err = nvs_set_str(my_handle, "PrivateKey", arg);
        if ( err != ESP_OK)
            ESP_LOGE(TAG, "%s: nvs_set_str err %d", __func__, err);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        printf("Missing <privateKey>\n");
    }
}

char * STSDK_SETTINGS [] = {
  STSDK_CLEANUP,
  STSDK_SERIAL,
  STSDK_PUBKEY,
  STSDK_PRIVKEY,
  0 // EOF
};

static struct cli_command stsdk_cmd_list[] = {
    {STSDK_CLEANUP,
        "Cleanup NV data with reboot option\n"
        "  Syntax: " STSDK_CLEANUP "\n", _cli_cmd_cleanup_event},
    {STSDK_SERIAL,
        "Sets the SmartThings device_info serialNumber.\n"
        "  Syntax: " STSDK_SERIAL " <serialNumber>\n"
        "  Example: " STSDK_SERIAL " AaBbCcDdEeFfGg...\n", _cli_cmd_stserial_event},
    {STSDK_PUBKEY,
        "Sets the SmartThings device_info publicKey.\n"
        "  Syntax: " STSDK_PUBKEY " <publicKey>\n"
        "  Example: " STSDK_PUBKEY " AaBbCcDdEeFfGg...\n", _cli_cmd_stpublickey_event},
    {STSDK_PRIVKEY,
        "Sets the SmartThings device_info privateKey.\n"
        "  Syntax: " STSDK_PRIVKEY " <privateKey>\n"
        "  Example: " STSDK_PRIVKEY " AaBbCcDdEeFfGg...\n", _cli_cmd_stprivatekey_event},
};


int get_switch_state(void)
{
    const char* switch_value = cap_switch_data->get_switch_value(cap_switch_data);
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
 * callback from cloud to arm away. We modify to report back to the
 * cloud our current state not an armed away state. If/when arm away state
 * changes it will be sent.
 */
void cap_securitySystem_arm_away_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    const char *init_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
    cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, init_value);
    ad2_arm_away(AD2_DEFAULT_VPA_SLOT, AD2_DEFAULT_VPA_SLOT);
}

void cap_securitySystem_arm_stay_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    const char *init_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
    cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, init_value);
}

void cap_securitySystem_disarm_cmd_cb(struct caps_securitySystem_data *caps_data)
{
    const char *init_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
    cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, init_value);

}

void cap_securitySystem_chime_cmd_cb(struct caps_momentary_data *caps_data)
{
}

void cap_fire_cmd_cb(struct caps_momentary_data *caps_data)
{
}

void cap_switch_cmd_cb(struct caps_switch_data *caps_data)
{
    int switch_state = get_switch_state();
    change_switch_state(switch_state);
}

void stsdk_init(void) {
    unsigned char *onboarding_config = (unsigned char *) onboarding_config_start;
    unsigned int onboarding_config_len = onboarding_config_end - onboarding_config_start;
    unsigned char *device_info = (unsigned char *) device_info_start;
    unsigned int device_info_len = device_info_end - device_info_start;
    int iot_err;

    // register STSDK CLI commands
    for (int i = 0; i < ARRAY_SIZE(stsdk_cmd_list); i++)
        cli_register_command(&stsdk_cmd_list[i]);

    // create a iot context
    ctx = st_conn_init(onboarding_config, onboarding_config_len, device_info, device_info_len);
    if (ctx != NULL) {
        iot_err = st_conn_set_noti_cb(ctx, iot_noti_cb, NULL);
        if (iot_err)
            ESP_LOGE(TAG, "fail to set notification callback function");
    } else {
        ESP_LOGE(TAG, "fail to create the iot_context");
    }

    // Add AD2 capabilies
    capability_init();
}


/**
 * Initialize all SmartThings capabilities.
 */
void capability_init()
{
    ESP_LOGI(TAG, "capability_init");
    int iot_err;

    // FIXME: dummy switch tied to physical button on ESP32
    cap_switch_data = caps_switch_initialize(ctx, "trash", NULL, NULL);
    if (cap_switch_data) {
        // FIXME: const char *init_value = caps_helper_switch.attr_switch.value_on;
        cap_switch_data->cmd_on_usr_cb = cap_switch_cmd_cb;
        cap_switch_data->cmd_off_usr_cb = cap_switch_cmd_cb;
    }

    // refresh device type init
    refresh_cap_handle = st_cap_handle_init(ctx, "main", "refresh", NULL, NULL);
    if (refresh_cap_handle) {
		iot_err = st_cap_cmd_set_cb(refresh_cap_handle, "refresh", refresh_cmd_cb, NULL);
		if (iot_err)
            ESP_LOGE(TAG, "fail to set cmd_cb for refresh");
    }

    // healthCheck capabilities init
	healthCheck_cap_handle = st_cap_handle_init(ctx, "main", "healthCheck", cap_health_check_init_cb, NULL);
    if (healthCheck_cap_handle) {
		iot_err = st_cap_cmd_set_cb(healthCheck_cap_handle, "ping", refresh_cmd_cb, NULL);
		if (iot_err)
            ESP_LOGE(TAG, "fail to set cmd_cb for healthCheck");
    }

    // firmwareUpdate capabilities init
	ota_cap_handle = st_cap_handle_init(ctx, "main", "firmwareUpdate", cap_current_version_init_cb, NULL);
    if (ota_cap_handle) {
		iot_err = st_cap_cmd_set_cb(ota_cap_handle, "updateFirmware", update_firmware_cmd_cb, NULL);
		if (iot_err)
			ESP_LOGE(TAG, "fail to set cmd_cb for updateFirmware");
    }

    // securitySystem device type init
    cap_securitySystem_data = caps_securitySystem_initialize(ctx, "main", NULL, NULL);
    if (cap_securitySystem_data) {
        const char *init_value = caps_helper_securitySystem.attr_securitySystemStatus.value_disarmed;
        cap_securitySystem_data->set_securitySystemStatus_value(cap_securitySystem_data, init_value);
        cap_securitySystem_data->cmd_armAway_usr_cb = cap_securitySystem_arm_away_cmd_cb;
        cap_securitySystem_data->cmd_armStay_usr_cb = cap_securitySystem_arm_stay_cmd_cb;
        cap_securitySystem_data->cmd_disarm_usr_cb = cap_securitySystem_disarm_cmd_cb;
    }

    // Chime contact sensor
    cap_contactSensor_data_chime = caps_contactSensor_initialize(ctx, "chime", NULL, NULL);
    if (cap_contactSensor_data_chime) {
        const char *contact_init_value = caps_helper_contactSensor.attr_contact.value_open;
        cap_contactSensor_data_chime->set_contact_value(cap_contactSensor_data_chime, contact_init_value);
    }
    // Chime Momentary button
    cap_momentary_data_chime = caps_momentary_initialize(ctx, "chime", NULL, NULL);
    if (cap_momentary_data_chime) {
        cap_momentary_data_chime->cmd_push_usr_cb = cap_securitySystem_chime_cmd_cb;
    }

    // Smoke Detector
    cap_smokeDetector_data = caps_smokeDetector_initialize(ctx, "fire", NULL, NULL);
    if (cap_smokeDetector_data) {
        const char *smoke_init_value = caps_helper_smokeDetector.attr_smoke.value_clear;
        cap_smokeDetector_data->set_smoke_value(cap_smokeDetector_data, smoke_init_value);
    }

    // Fire Alarm Momentary button
    cap_momentary_data_fire = caps_momentary_initialize(ctx, "fire", NULL, NULL);
    if (cap_momentary_data_fire) {
        cap_momentary_data_fire->cmd_push_usr_cb = cap_fire_cmd_cb;
    }
}

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

    switch(status)
    {
        case IOT_STATUS_NEED_INTERACT:
            noti_led_mode = LED_ANIMATION_MODE_FAST;
            break;
        case IOT_STATUS_IDLE:
        case IOT_STATUS_CONNECTING:
            noti_led_mode = LED_ANIMATION_MODE_IDLE;
            // FIXME: send update on all caps to cloud
            change_switch_state(get_switch_state());
            break;
        default:
            break;
    }
}

#if defined(SET_PIN_NUMBER_CONFRIM)
void* pin_num_memcpy(void *dest, const void *src, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; i++)
        *((char*)dest + i) = *((char*)src + i);
    return dest;
}
#endif

void connection_start(void)
{
    iot_pin_t *pin_num = NULL;
    int err;

#if defined(SET_PIN_NUMBER_CONFRIM)
    pin_num = (iot_pin_t *) malloc(sizeof(iot_pin_t));
    if (!pin_num)
        ESP_LOGE(TAG, "failed to malloc for iot_pin_t");

    // to decide the pin confirmation number(ex. "12345678"). It will use for easysetup.
    //    pin confirmation number must be 8 digit number.
    pin_num_memcpy(pin_num, "12345678", sizeof(iot_pin_t));
#endif

    // process on-boarding procedure. There is nothing more to do on the app side than call the API.
    err = st_conn_start(ctx, (st_status_cb)&iot_status_cb, IOT_STATUS_ALL, NULL, pin_num);
    if (err) {
        ESP_LOGE(TAG, "fail to start connection. err:%d", err);
    }
    if (pin_num) {
        free(pin_num);
    }
}

void connection_start_task(void *arg)
{
    connection_start();
    vTaskDelete(NULL);
}

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

void button_event(IOT_CAP_HANDLE *handle, int type, int count)
{
    if (type == BUTTON_SHORT_PRESS) {
        ESP_LOGI(TAG, "Button short press, count: %d", count);
        switch(count) {
            case 1:
                if (g_iot_status == IOT_STATUS_NEED_INTERACT) {
                    st_conn_ownership_confirm(ctx, true);
                    noti_led_mode = LED_ANIMATION_MODE_IDLE;
                    change_switch_state(get_switch_state());
                } else {
                    if (get_switch_state() == SWITCH_ON) {
                        change_switch_state(SWITCH_OFF);
                        cap_switch_data->set_switch_value(cap_switch_data, caps_helper_switch.attr_switch.value_off);
                        cap_switch_data->attr_switch_send(cap_switch_data);
                    } else {
                        change_switch_state(SWITCH_ON);
                        cap_switch_data->set_switch_value(cap_switch_data, caps_helper_switch.attr_switch.value_on);
                        cap_switch_data->attr_switch_send(cap_switch_data);
                    }
                }
                break;
            case 5:
                /* clean-up provisioning & registered data with reboot option*/
                st_conn_cleanup(ctx, true);

                break;
            default:
                led_blink(get_switch_state(), 100, count);
                break;
        }
    } else if (type == BUTTON_LONG_PRESS) {
        ESP_LOGI(TAG, "Button long press, iot_status: %d", g_iot_status);
        led_blink(get_switch_state(), 100, 3);
        st_conn_cleanup(ctx, false);
        xTaskCreate(connection_start_task, "connection_task", 2048, NULL, tskIDLE_PRIORITY+2, NULL);
    }
}

#ifdef __cplusplus
} // extern "C"
#endif
