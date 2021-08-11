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
#if CONFIG_AD2IOT_PUSHOVER_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

void pushover_register_cmds();
void pushover_init();

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_AD2IOT_PUSHOVER_CLIENT */
#endif /* _PUSHOVER_H */
