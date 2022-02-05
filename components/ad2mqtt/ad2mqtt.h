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

void mqtt_register_cmds();
void mqtt_init();

#endif /* CONFIG_AD2IOT_MQTT_CLIENT */
#endif /* _MQTT_H */
