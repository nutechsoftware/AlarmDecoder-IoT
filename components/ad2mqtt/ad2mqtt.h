/**
 *  @file    ad2mqtt.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    07/31/2021
 *
 *  @brief Connect to an MQTT broker and publish state updates as
 *  well as subscribe to a command topic to listen for verbs.
 *
 *  @copyright Copyright (C) 2021 Nu Tech Software Solutions, Inc.
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
#ifndef _MQTT_H
#define _MQTT_H
#if CONFIG_AD2IOT_MQTT_CLIENT

/* Constants that aren't configurable in menuconfig */
#define MQTT_COMMAND        "mqtt"
#define MQTT_PREFIX         "mq"
#define MQTT_URL_CFGKEY     "url"
#define MQTT_SAS_CFGKEY     "switch"

#define MAX_SEARCH_KEYS 9

// NV storage sub key values for virtual search switch
#define SK_NOTIFY_SLOT       "N"
#define SK_DEFAULT_STATE     "D"
#define SK_AUTO_RESET        "R"
#define SK_TYPE_LIST         "T"
#define SK_PREFILTER_REGEX   "P"
#define SK_OPEN_REGEX_LIST   "O"
#define SK_CLOSED_REGEX_LIST "C"
#define SK_FAULT_REGEX_LIST  "F"
#define SK_OPEN_OUTPUT_FMT   "o"
#define SK_CLOSED_OUTPUT_FMT "c"
#define SK_FAULT_OUTPUT_FMT  "f"

#define MQTT_TOPIC_PREFIX "ad2iot"

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_register_cmds();
void mqtt_init();

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_AD2IOT_MQTT_CLIENT */
#endif /* _MQTT_H */
