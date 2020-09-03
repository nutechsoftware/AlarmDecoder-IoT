/**
 *  @file    alarmdecoder_stsdk_app_esp32.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *  @version 1.0
 *
 *  @brief AlarmDecoder IoT embedded network appliance for SmartThings
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
#ifndef AlarmDecoder_app_h
#define AlarmDecoder_app_h

#ifdef __cplusplus
extern "C" {
#endif

void alarmdecoder_start(iot_status_t *, IOT_CTX**);
void button_event(IOT_CAP_HANDLE *handle, int type, int count);
static void iot_noti_cb(iot_noti_data_t *noti_data, void *noti_usr_data);
static void connection_start(void);
static void capability_init();
#ifdef __cplusplus
}
#endif

#endif