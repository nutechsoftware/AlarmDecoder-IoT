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
#ifndef twilio_h
#define twilio_h
#if CONFIG_TWILIO_CLIENT

//#define DEBUG_TWILIO
#define TWILIO_QUEUE_SIZE 20
#define AD2_DEFAULT_TWILIO_SLOT 0

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "api.twilio.com"
#define WEB_PORT "443"
#define API_VERSION "2010-04-01"

#define TWILIO_SID   "twsid"
#define TWILIO_TOKEN "twtoken"
#define TWILIO_TYPE  "twtype"
#define TWILIO_TO    "twto"
#define TWILIO_FROM  "twfrom"
#define TWILIO_BODY  "twbody"

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

void twilio_init();
void twilio_send_task(void *pvParameters);
void twilio_add_queue(const char *sid, const char *token, const char *from, const char *to, char type, const char *arg);

#ifdef __cplusplus
}
#endif
#endif // CONFIG_TWILIO_CLIENT
#endif // twilio_h