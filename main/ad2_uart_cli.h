/**
 *  @file    ad2_uart_cli.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief UART command line interface for direct access configuration
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

#ifndef _AD2_UART_CLI_H_
#define _AD2_UART_CLI_H_

#define PROMPT_STRING "AD2IOT # "
#define AD2_HELP_CMD "help"
#define CLI_TASK_PRIORITY (tskIDLE_PRIORITY+2)
#define CLI_TASK_SIZE    (8192)

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* command_function_t)(char *string);

typedef struct cli_command {
    char *command;
    char *help_string;
    command_function_t command_fn;
} cli_cmd_t;

typedef struct cli_command_list {
    cli_cmd_t* cmd;
    struct cli_command_list* next;
} cli_cmd_list_t;

void uart_cli_main();
void cli_register_command(cli_cmd_t* cmd);

#ifdef __cplusplus
}
#endif
#endif /* _AD2_UART_CLI_H_ */
