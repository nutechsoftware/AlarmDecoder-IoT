/**
 *  @file    twilio.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/12/2020
 *
 *  @brief Simple commands to post to api.twilio.com
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
#ifndef _TWILIO_H
#define _TWILIO_H
#if CONFIG_TWILIO_CLIENT
//#define DEBUG_TWILIO
//#define DEBUG_TWILIO_TLS
#define TWILIO_QUEUE_SIZE 20
#define AD2_DEFAULT_TWILIO_SLOT 0

/* Constants that aren't configurable in menuconfig */
#define SENDGRID_API_SERVER "api.sendgrid.com"
#define SENDGRID_API_PORT "443"

#define TWILIO_API_SERVER "api.twilio.com"
#define TWILIO_API_PORT "443"
#define TWILIO_API_VERSION "2010-04-01"
#define TWILIO_RATE_LIMIT 2000

#define TWILIO_SID_CFGKEY     "twsid"
#define TWILIO_TOKEN_CFGKEY   "twtoken"
#define TWILIO_TYPE_CFGKEY    "twtype"
#define TWILIO_TO_CFGKEY      "twto"
#define TWILIO_FROM_CFGKEY    "twfrom"
#define TWILIO_SAS_CFGKEY     "twsas"

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

// Notification types
#define TWILIO_NOTIFY_MESSAGE "M"
#define TWILIO_NOTIFY_CALL    "C"
#define TWILIO_NOTIFY_EMAIL   "E"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct twilio_message_data {
    char *sid;
    char *token;
    char *from;
    char *to;
    char type; // 'M' Message 'R' Redirect 'T' Twiml url
    char *arg;
} twilio_message_data_t;

void twilio_register_cmds();
void twilio_init();
void twilio_send_task(void *pvParameters);
void twilio_add_queue(std::string &sid, std::string &token, std::string &from, std::string &to, char type, std::string &arg);

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_TWILIO_CLIENT */
#endif /* _TWILIO_H */

