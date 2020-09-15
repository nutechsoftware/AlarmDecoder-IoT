/**
 *  @file    twilio.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/12/2020
 *  @version 1.0
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
#ifndef twilio_h
#define twilio_h

#define DEBUG_TWILIO
#define TWILIO_QUEUE_SIZE 20
#define TWILIO_CONFIG_TOKEN "TW"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "api.twilio.com"
#define WEB_PORT "443"
#define API_VERSION "2010-04-01"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct twilio_message_data {
    char *sid;
    char *token;
    char *from;
    char *to;
    uint8_t type; // 'M' Message 'R' Redirect 'T' Twiml url
    char *arg;
} twilio_message_data_t;

void twilio_init();
void twilio_send_task(void *pvParameters);
void twilio_add_queue(char *from, char *to, uint8_t type, char *arg);

#ifdef __cplusplus
}
#endif

#endif