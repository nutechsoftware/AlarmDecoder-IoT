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

#define AD2_CMD_REBOOT   "restart"
#define AD2_CMD_NETMODE  "netmode"
#define AD2_CMD_SWITCH   "switch"
#define AD2_CMD_ZONE     "zone"
#define AD2_CMD_CODE     "code"
#define AD2_CMD_PART     "partition"
#define AD2_CMD_SOURCE   "ad2source"
#define AD2_CMD_CONFIG   "ad2config"
#define AD2_CMD_TERM     "ad2term"
#define AD2_CMD_LOGMODE  "logmode"
#define AD2_CMD_FACTORY  "factory-reset"
#define AD2_CMD_TOP      "top"
#define AD2_CMD_UNLOCKER "teaser :c)"

void register_ad2_cli_cmd(void);

#endif /* _AD2_CLI_CMD_H_ */
