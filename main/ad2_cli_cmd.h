/**
 *  @file    ad2_cli_cmd.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief CLI interface for AD2IoT
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

#ifndef _AD2_CLI_CMD_H_
#define _AD2_CLI_CMD_H_

#define AD2_REBOOT   "restart"
#define AD2_NETMODE  "netmode"
#define AD2_BUTTON   "button"
#define AD2_CODE     "code"
#define AD2_VPADDR   "vpaddr"
#define AD2_SOURCE   "ad2source"
#define AD2_TERM     "ad2term"
#define AD2_LOGMODE  "logmode"
#define AD2_UNLOCKER "teaser :c)"

#ifdef __cplusplus
extern "C" {
#endif

void register_ad2_cli_cmd(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _AD2_CLI_CMD_H_ */

