/* ***************************************************************************
 *
 * Copyright 2019-2020 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"

// Disable componet via sdkconfig
#if CONFIG_STDK_IOT_CORE
static const char *TAG = "CAPS_SECS";

#include "st_dev.h"
#include "caps_securitySystem.h"

static const char *caps_securitySystem_get_alarm_value(caps_securitySystem_data_t *caps_data)
{
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: caps_data is NULL", __func__);
        return NULL;
    }
    return caps_data->alarm_value;
}

static void caps_securitySystem_set_alarm_value(caps_securitySystem_data_t *caps_data, const char *value)
{
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: caps_data is NULL", __func__);
        return;
    }
    if (caps_data->alarm_value) {
        free(caps_data->alarm_value);
    }
    caps_data->alarm_value = strdup(value);
}

static void caps_securitySystem_attr_alarm_send(caps_securitySystem_data_t *caps_data)
{
    int sequence_no = -1;

    if (!caps_data || !caps_data->handle) {
        ESP_LOGE(TAG, "%s: fail to get handle", __func__);
        return;
    }
    if (!caps_data->alarm_value) {
        ESP_LOGE(TAG, "%s: value is NULL", __func__);
        return;
    }

    ST_CAP_SEND_ATTR_STRING(caps_data->handle,
            (char *)caps_helper_securitySystem.attr_alarm.name,
            caps_data->alarm_value,
            NULL,
            NULL,
            sequence_no);

    if (sequence_no < 0)
        ESP_LOGE(TAG, "%s: fail to send alarm value", __func__);
    else
        ESP_LOGI(TAG, "%s: Sequence number return : %d", __func__, sequence_no);
}


static int caps_securitySystem_attr_securitySystemStatus_str2idx(const char *value)
{
    int index;

    for (index = 0; index < CAP_ENUM_SECURITYSYSTEM_SECURITYSYSTEMSTATUS_VALUE_MAX; index++) {
        if (!strcmp(value, caps_helper_securitySystem.attr_securitySystemStatus.values[index])) {
            return index;
        }
    }
    return -1;
}

static const char *caps_securitySystem_get_securitySystemStatus_value(caps_securitySystem_data_t *caps_data)
{
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: caps_data is NULL", __func__);
        return NULL;
    }
    return caps_data->securitySystemStatus_value;
}

static void caps_securitySystem_set_securitySystemStatus_value(caps_securitySystem_data_t *caps_data, const char *value)
{
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: caps_data is NULL", __func__);
        return;
    }
    if (caps_data->securitySystemStatus_value) {
        free(caps_data->securitySystemStatus_value);
    }
    caps_data->securitySystemStatus_value = strdup(value);
}

static void caps_securitySystem_attr_securitySystemStatus_send(caps_securitySystem_data_t *caps_data)
{
    int sequence_no = -1;

    if (!caps_data || !caps_data->handle) {
        ESP_LOGE(TAG, "%s: fail to get handle", __func__);
        return;
    }
    if (!caps_data->securitySystemStatus_value) {
        ESP_LOGE(TAG, "%s: value is NULL", __func__);
        return;
    }

    ST_CAP_SEND_ATTR_STRING(caps_data->handle,
            (char *)caps_helper_securitySystem.attr_securitySystemStatus.name,
            caps_data->securitySystemStatus_value,
            NULL,
            NULL,
            sequence_no);

    if (sequence_no < 0)
        ESP_LOGE(TAG, "%s: fail to send securitySystemStatus value", __func__);
    else
        ESP_LOGI(TAG, "%s: Sequence number return : %d", __func__, sequence_no);

}


static void caps_securitySystem_cmd_armStay_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    caps_securitySystem_data_t *caps_data = (caps_securitySystem_data_t *)usr_data;
    const char* value = caps_helper_securitySystem.attr_securitySystemStatus.values[CAP_ENUM_SECURITYSYSTEM_SECURITYSYSTEMSTATUS_VALUE_ARMEDSTAY];

    ESP_LOGI(TAG, "%s: num_args:%u", __func__, cmd_data->num_args);

    caps_securitySystem_set_securitySystemStatus_value(caps_data, value);
    if (caps_data && caps_data->cmd_armStay_usr_cb)
        caps_data->cmd_armStay_usr_cb(caps_data);
    caps_securitySystem_attr_securitySystemStatus_send(caps_data);
}

static void caps_securitySystem_cmd_disarm_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    caps_securitySystem_data_t *caps_data = (caps_securitySystem_data_t *)usr_data;
    const char* value = caps_helper_securitySystem.attr_securitySystemStatus.values[CAP_ENUM_SECURITYSYSTEM_SECURITYSYSTEMSTATUS_VALUE_DISARMED];

    ESP_LOGI(TAG, "%s: num_args:%u", __func__, cmd_data->num_args);

    caps_securitySystem_set_securitySystemStatus_value(caps_data, value);
    if (caps_data && caps_data->cmd_disarm_usr_cb)
        caps_data->cmd_disarm_usr_cb(caps_data);
    caps_securitySystem_attr_securitySystemStatus_send(caps_data);
}

static void caps_securitySystem_cmd_armAway_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    caps_securitySystem_data_t *caps_data = (caps_securitySystem_data_t *)usr_data;
    const char* value = caps_helper_securitySystem.attr_securitySystemStatus.values[CAP_ENUM_SECURITYSYSTEM_SECURITYSYSTEMSTATUS_VALUE_ARMEDAWAY];

    ESP_LOGI(TAG, "%s: num_args:%u", __func__, cmd_data->num_args);

    caps_securitySystem_set_securitySystemStatus_value(caps_data, value);
    if (caps_data && caps_data->cmd_armAway_usr_cb)
        caps_data->cmd_armAway_usr_cb(caps_data);
    caps_securitySystem_attr_securitySystemStatus_send(caps_data);
}

static void caps_securitySystem_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    caps_securitySystem_data_t *caps_data = usr_data;
    if (caps_data && caps_data->init_usr_cb)
        caps_data->init_usr_cb(caps_data);
    caps_securitySystem_attr_alarm_send(caps_data);
    caps_securitySystem_attr_securitySystemStatus_send(caps_data);
}

caps_securitySystem_data_t *caps_securitySystem_initialize(IOT_CTX *ctx, const char *component, void *init_usr_cb, void *usr_data)
{
    caps_securitySystem_data_t *caps_data = NULL;
    int err;

    caps_data = malloc(sizeof(caps_securitySystem_data_t));
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: fail to malloc for caps_securitySystem_data", __func__);
        return NULL;
    }

    memset(caps_data, 0, sizeof(caps_securitySystem_data_t));

    caps_data->init_usr_cb = init_usr_cb;
    caps_data->usr_data = usr_data;

    caps_data->get_alarm_value = caps_securitySystem_get_alarm_value;
    caps_data->set_alarm_value = caps_securitySystem_set_alarm_value;
    caps_data->attr_alarm_send = caps_securitySystem_attr_alarm_send;
    caps_data->get_securitySystemStatus_value = caps_securitySystem_get_securitySystemStatus_value;
    caps_data->set_securitySystemStatus_value = caps_securitySystem_set_securitySystemStatus_value;
    caps_data->attr_securitySystemStatus_str2idx = caps_securitySystem_attr_securitySystemStatus_str2idx;
    caps_data->attr_securitySystemStatus_send = caps_securitySystem_attr_securitySystemStatus_send;
    if (ctx) {
        caps_data->handle = st_cap_handle_init(ctx, component, caps_helper_securitySystem.id, caps_securitySystem_init_cb, caps_data);
    }
    if (caps_data->handle) {
        err = st_cap_cmd_set_cb(caps_data->handle, caps_helper_securitySystem.cmd_armStay.name, caps_securitySystem_cmd_armStay_cb, caps_data);
        if (err) {
            ESP_LOGE(TAG, "%s: fail to set cmd_cb for armStay of securitySystem", __func__);
    }
        err = st_cap_cmd_set_cb(caps_data->handle, caps_helper_securitySystem.cmd_disarm.name, caps_securitySystem_cmd_disarm_cb, caps_data);
        if (err) {
            ESP_LOGE(TAG, "%s: fail to set cmd_cb for disarm of securitySystem", __func__);
    }
        err = st_cap_cmd_set_cb(caps_data->handle, caps_helper_securitySystem.cmd_armAway.name, caps_securitySystem_cmd_armAway_cb, caps_data);
        if (err) {
            ESP_LOGE(TAG, "%s: fail to set cmd_cb for armAway of securitySystem", __func__);
    }
    } else {
        ESP_LOGE(TAG, "%s: fail to init securitySystem handle", __func__);
    }

    return caps_data;
}

#endif /* CONFIG_STDK_IOT_CORE */

