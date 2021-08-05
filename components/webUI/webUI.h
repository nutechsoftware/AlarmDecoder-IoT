/**
 *  @file    webUI.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    07/17/2021
 *
 *  @brief WEB server for user interface to alarm system
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
#ifndef _WEBUI_H
#define _WEBUI_H
#if CONFIG_AD2IOT_WEBSERVER_UI

#ifdef __cplusplus
extern "C" {
#endif

void webUI_init();

#ifdef __cplusplus
}
#endif
#endif /* CONFIG_AD2IOT_WEBSERVER_UI */
#endif /* _WEBUI_H */