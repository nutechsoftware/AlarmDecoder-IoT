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

#include "st_dev.h"
#include "caps_momentary.h"

static const char *TAG = "CAPS_MOME";

static void caps_momentary_cmd_push_cb(IOT_CAP_HANDLE *handle, iot_cap_cmd_data_t *cmd_data, void *usr_data)
{
    caps_momentary_data_t *caps_data = (caps_momentary_data_t *)usr_data;

    ESP_LOGI(TAG, "%s: num_args:%u", __func__, cmd_data->num_args);

    if (caps_data && caps_data->cmd_push_usr_cb)
        caps_data->cmd_push_usr_cb(caps_data);
}

static void caps_momentary_init_cb(IOT_CAP_HANDLE *handle, void *usr_data)
{
    caps_momentary_data_t *caps_data = usr_data;
    if (caps_data && caps_data->init_usr_cb)
        caps_data->init_usr_cb(caps_data);
}

caps_momentary_data_t *caps_momentary_initialize(IOT_CTX *ctx, const char *component, void *init_usr_cb, void *usr_data)
{
    caps_momentary_data_t *caps_data = NULL;
    int err;

    caps_data = malloc(sizeof(caps_momentary_data_t));
    if (!caps_data) {
        ESP_LOGE(TAG, "%s: fail to malloc for caps_momentary_data", __func__);
        return NULL;
    }

    memset(caps_data, 0, sizeof(caps_momentary_data_t));

    caps_data->init_usr_cb = init_usr_cb;
    caps_data->usr_data = usr_data;

    if (ctx) {
        caps_data->handle = st_cap_handle_init(ctx, component, caps_helper_momentary.id, caps_momentary_init_cb, caps_data);
    }
    if (caps_data->handle) {
        err = st_cap_cmd_set_cb(caps_data->handle, caps_helper_momentary.cmd_push.name, caps_momentary_cmd_push_cb, caps_data);
        if (err) {
            ESP_LOGE(TAG, "%s: fail to set cmd_cb for push of momentary", __func__);
    }
    } else {
        ESP_LOGE(TAG, "%s: fail to init momentary handle", __func__);
    }

    return caps_data;
}

#endif /* CONFIG_STDK_IOT_CORE */

