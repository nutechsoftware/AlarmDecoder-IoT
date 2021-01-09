/**
 *  @file    pushover.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/06/2021
 *
 *  @brief Simple commands to post to api.pushover.net
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
#ifndef _PUSHOVER_H
#define _PUSHOVER_H
#if CONFIG_PUSHOVER_CLIENT
//#define DEBUG_PUSHOVER
//#define DEBUG_PUSHOVER_TLS
#define PUSHOVER_QUEUE_SIZE 20
#define AD2_DEFAULT_PUSHOVER_SLOT 0

/* Constants that aren't configurable in menuconfig */
#define PUSHOVER_API_SERVER "api.pushover.net"
#define PUSHOVER_API_PORT "443"

#define PUSHOVER_API_VERSION "1"
#define PUSHOVER_RATE_LIMIT 2000

#define PUSHOVER_COMMAND        "pushover"
#define PUSHOVER_PREFIX         "po"
#define PUSHOVER_TOKEN_CFGKEY   "apptoken"
#define PUSHOVER_USERKEY_CFGKEY "userkey"
#define PUSHOVER_SAS_CFGKEY     "switch"

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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pushover_message_data {
    char *token;
    char *userkey;
    char *message;
} pushover_message_data_t;

void pushover_register_cmds();
void pushover_init();
void pushover_send_task(void *pvParameters);
void pushover_add_queue(std::string &userkey, std::string &token, std::string &message);

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_PUSHOVER_CLIENT */
#endif /* _PUSHOVER_H */
