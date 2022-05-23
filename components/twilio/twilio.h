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
#if CONFIG_AD2IOT_TWILIO_CLIENT

void twilio_register_cmds();
void twilio_init();
void twilio_add_queue(std::string &sid, std::string &token, std::string &from, std::string &to, char type, std::string &arg);

#endif /* CONFIG_AD2IOT_TWILIO_CLIENT */
#endif /* _TWILIO_H */

