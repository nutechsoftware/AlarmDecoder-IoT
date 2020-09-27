/**
 *  @file    alarmdecoder_main.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief AlarmDecoder IoT embedded network appliance
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
#ifndef _ALARMDECODER_MAIN_H
#define _ALARMDECODER_MAIN_H

#include "alarmdecoder_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// global thread control
extern int g_StopMainTask;
extern portMUX_TYPE spinlock;

// global AlarmDecoder parser class instance
extern AlarmDecoderParser AD2Parse;

// global AD2 device connection fd/id <socket or uart id>
extern int g_ad2_client_handle;

// global ad2 connection mode ['S'ocket | 'C'om port]
extern uint8_t g_ad2_mode;

// global network connection state
extern int g_ad2_network_state;

#ifdef __cplusplus
}
#endif

#endif /* _ALARMDECODER_MAIN_H */

