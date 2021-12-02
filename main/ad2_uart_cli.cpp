/**
 *  @file    ad2_uart_cli.c
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
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "UARTCLI";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "driver/uart.h"

// specific includes
#include "ad2_cli_cmd.h"

//#define DEBUG_CLI

#ifdef __cplusplus
extern "C" {
#endif

static struct cli_command_list *cli_cmd_list;

static void cli_cmd_help(char *string);

/**
 * @brief command list for module
 */
static struct cli_command help_cmd = {
    (char*)AD2_HELP_CMD, (char*)"- Show the list of commands or give more detail on a specific command.\r\n"
    "  ```" AD2_HELP_CMD " [command]```\r\n\r\n", cli_cmd_help
};

/**
 * @brief Check if a command is registered.
 *
 * @param [in]input_string string containing the command to check.
 *
 * @return cli_cmd_t * the command structure pointer or NULL
 * if not found
 */
static cli_cmd_t* cli_find_command (const char* input_string)
{
    cli_cmd_list_t* now = cli_cmd_list;

    while (now) {
        if (!now->cmd) {
            continue;
        }

        if (strncmp(input_string, now->cmd->command, strlen(now->cmd->command)) == 0) {
            switch (input_string[strlen(now->cmd->command)]) {
            case ' ':
            case '\r':
            case '\n':
            case '\0':
                return now->cmd;
            }
        }
        now = now->next;
    }

    return NULL;
}

/**
 * @brief process a command line event.
 *
 * @param [in]input_string command string to test for.
 */
static void cli_process_command(char* input_string)
{
    cli_cmd_t *command;

    command = cli_find_command(input_string);

    if (command == NULL) {
        ad2_printf_host(false, "command not found. please check 'help'\r\n");
        return;
    }

    command->command_fn(input_string);
}

/**
 * @brief Register cli commands with the CLI engine.
 *
 * @param [in]cli_cmd_t * pointer to command structure to register.
 */
void cli_register_command(cli_cmd_t* cmd)
{
    cli_cmd_list_t* now;


    if ( (!cmd) || (!cmd->command) ) {
        ESP_LOGE(TAG, "%s: register fail : cmd is invalid.", __func__);
        return;
    }

    if (cli_find_command(cmd->command)) {
        ESP_LOGE(TAG, "%s: register fail : same cmd already exists.", __func__);
        return;
    }

    if (!cli_cmd_list) {
        cli_cmd_list = (cli_cmd_list_t*) malloc(sizeof(struct cli_command_list));
        cli_cmd_list->next = NULL;
        cli_cmd_list->cmd = cmd;
    } else {
        now = cli_cmd_list;
        while (now->next) {
            now = now->next;
        }
        now->next = (cli_cmd_list_t*) malloc(sizeof(struct cli_command_list));

        now = now->next;
        now->next = NULL;
        now->cmd = cmd;
    }
}

/**
 * @brief command event processor for the 'help' command.
 *
 * @param [in]cmd char * command string with args if any.
 */
static void cli_cmd_help(char *cmd)
{
    bool showhelp = true;
    cli_cmd_list_t* now = cli_cmd_list;
    std::string buf; // buffer to hold help argument

    ad2_printf_host(false, "\n");
    if (ad2_copy_nth_arg(buf, cmd, 1) >= 0) {
        cli_cmd_t *command;
        command = cli_find_command(buf.c_str());
        if (command != NULL) {
            ad2_printf_host(false, "Help for command '%s'\r\n\r\n", command->command);
            // spool bytes until we reach the null term.
            char *help_string = command->help_string;
            while (*help_string) {
                uart_write_bytes(UART_NUM_0, help_string, 1);
                uart_wait_tx_done(UART_NUM_0, 100);
                help_string++;
            }
            ad2_printf_host(false, "\r\n");
            showhelp = false;
        } else {
            ad2_printf_host(false, ", Command not found '%s'\r\n", cmd);
        }
    }

    if (showhelp) {
        int x = 0;
        bool sendpfx = false;
        bool sendsfx = false;
        ad2_printf_host(false, "Available AD2IoT terminal commands\r\n  [");
        while (now) {
            if (!now->cmd) {
                continue;
            }
            if (sendpfx && now->next) {
                sendpfx = false;
                ad2_printf_host(false, ",\r\n   ");
            }
            if (sendsfx) {
                sendsfx = false;
                ad2_printf_host(false, ", ");
            }
            ad2_printf_host(false, "%s",now->cmd->command);
            x++;
            now = now->next;
            if (now) {
                sendsfx = true;
            }
            if (x > 5) {
                sendpfx = true;
                sendsfx = false;
                x = 0;
            }
        }
        ad2_printf_host(false, "]\r\n\r\nType help <command> for details on each command.\r\n\r\n");
    }
}

/**
 * @brief If there is user input("\n") within a given timeout,
 * the main function will be suspended.
 *
 *  @param [in]timeout_ms int timeout in ms to wait for input before
 * continuing. If no \n is received it will allow main task to resume.
 */
static void _cli_util_wait_for_user_input(unsigned int timeout_ms)
{
    TickType_t cur = xTaskGetTickCount();
    TickType_t end = cur + pdMS_TO_TICKS(timeout_ms);
    while (xTaskGetTickCount() < end) {
        taskENTER_CRITICAL(&spinlock);
        if (g_StopMainTask != 1) {
            taskEXIT_CRITICAL(&spinlock);
            break;
        }
        taskEXIT_CRITICAL(&spinlock);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    taskENTER_CRITICAL(&spinlock);
    if (g_StopMainTask == 1) {
        // When there is no input("\n") for a set time, main task will be executed...
        g_StopMainTask = 0;
    }
    taskEXIT_CRITICAL(&spinlock);

    if (g_StopMainTask != 0) {
        while (1) {
            taskENTER_CRITICAL(&spinlock);
            if (g_StopMainTask == 0) {
                taskEXIT_CRITICAL(&spinlock);
                break;
            }
            taskEXIT_CRITICAL(&spinlock);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief UART command line interface CLI task.
 * Consume host uart data and provide a simple terminal interface to this device.
 *
 * Terminal navigation support:
 *    UP/DOWN last command/current command navigation.
 *    BACKSPACE command line edit.
 *    CTL+C Clear command start new.
 *
 *    Echo back valid data and allow for simple left/right cursor navigation.
 *
 * @param [in]pvParameters void * to parameters.
 *
 */
static void esp_uart_cli_task(void *pvParameters)
{

    // Configure a temporary buffer for the incoming data
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];
    uint8_t prev_cmd[MAX_UART_CMD_SIZE];
    uint8_t _cmd_buffer[MAX_UART_CMD_SIZE];
    int _cmd_len = 0;
    memset(_cmd_buffer, 0, MAX_UART_CMD_SIZE);
    memset(prev_cmd, 0, MAX_UART_CMD_SIZE);

    cli_register_command(&help_cmd);

    // break sequence state
    static uint8_t break_count = 0;
    while (1) {

        // Read data from the UART
        memset(rx_buffer, 0, AD2_UART_RX_BUFF_SIZE);
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);

        if (len < 0) {
            ESP_LOGE(TAG, "%s: uart cli read error.", __func__);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // if not last then reset prompt and show current command buffer
        int last_console_owner = false;
        taskENTER_CRITICAL(&spinlock);
        if (ad2_is_host_last((void *)xTaskGetCurrentTaskHandle())) {
            last_console_owner = true;
        }
        taskEXIT_CRITICAL(&spinlock);

        // if the console line was taken and its been idle redraw current command.
        int last_console_time = AD2Parse.monotonicTime() - ad2_host_last_lock_time();
        if (!last_console_owner && last_console_time > 1) {
            ad2_printf_host(false, "\r\n", 2);
            ad2_printf_host(false, PROMPT_STRING);
            if (_cmd_len) {
                ad2_snprintf_host( (const char *)_cmd_buffer, _cmd_len);
            }
        }

        if (len) {
            // lock the console as we spool out the message
            ad2_take_host_console((void *)xTaskGetCurrentTaskHandle(), AD2_CONSOLE_LOCK_TIME);

            for (int i = 0; i < len; i++) {
#if defined(DEBUG_CLI)
                ESP_LOGI(TAG, "%s: chr(%.2x)", __func__, rx_buffer[i]);
#endif
                switch(rx_buffer[i]) {
                case '\n':
                    // silently ignore LF only respond to CR
                    break_count = 0;
                    break;
                case '\r':
                    break_count = 0;
                    ad2_printf_host(false, "\r\n");
                    if (_cmd_len) {
                        cli_process_command((char *)_cmd_buffer);
                        memcpy(prev_cmd, _cmd_buffer, MAX_UART_CMD_SIZE);
                        memset(_cmd_buffer, 0, MAX_UART_CMD_SIZE);
                        _cmd_len = 0;
                    }
                    ad2_printf_host(false, PROMPT_STRING);
                    break;

                case '\b': //backspace
                case 0x7f: //delete
                    break_count = 0;
                    //backspace
                    if (_cmd_len > 0) {
                        ad2_printf_host( false, "\b \b");
                        _cmd_buffer[--_cmd_len] = '\0';
                    }
                    break;

                case 0x03: //Ctrl + C
                    break_count = 0;
                    ad2_printf_host( false, "^C\r\n");
                    memset(_cmd_buffer, 0, MAX_UART_CMD_SIZE);
                    _cmd_len = 0;
                    ad2_printf_host( false, PROMPT_STRING);
                    break;

                case 0x1B: //arrow keys : 0x1B 0x5B 0x41~44
                    break_count = 0;
                    if ( rx_buffer[i+1] == 0x5B ) {
                        switch (rx_buffer[i+2]) {
                        case 0x41: //UP
                            memcpy(_cmd_buffer, prev_cmd, MAX_UART_CMD_SIZE);
                            _cmd_len = strlen((char*)_cmd_buffer);
                            ad2_snprintf_host( (const char *)&rx_buffer[i+1], 2);
                            ad2_printf_host( false, "\r\n");
                            ad2_printf_host( false, PROMPT_STRING);
                            ad2_snprintf_host( (const char *)_cmd_buffer, _cmd_len);
                            i+=3;
                            break;
                        case 0x42: //DOWN - ignore
                            i+=3;
                            break;
                        case 0x43: //right
                            if (_cmd_buffer[_cmd_len+1] != '\0') {
                                _cmd_len += 1;
                                ad2_snprintf_host( (const char *)&rx_buffer[i], 3);
                            }
                            i+=3;
                            break;
                        case 0x44: //left
                            if (_cmd_len > 0) {
                                _cmd_len -= 1;
                                ad2_snprintf_host( (const char *)&rx_buffer[i], 3);
                            }
                            i+=3;
                            break;
                        default:
                            break;
                        }
                    }
                    break;

                default:
                    // detect a break sequence '...'
                    if (g_StopMainTask == 1 && rx_buffer[i] == '.') {
                        break_count++;
                        // clear break state and exit while if break detected
                        if (break_count > 2) {
                            // break detected block main task from running and continue.
                            taskENTER_CRITICAL(&spinlock);
                            g_StopMainTask = 2;
                            taskEXIT_CRITICAL(&spinlock);
                            ad2_printf_host( false, "Startup halted. Use the 'restart' command when finished to start normally.\r\n");
                            ad2_printf_host( false, PROMPT_STRING);
                            break_count = 0;
                        }
                    } else {
                        break_count = 0;
                    }
                    //check whether character is valid
                    if ((rx_buffer[i] >= ' ') && (rx_buffer[i] <= '~')) {
                        if (_cmd_len >= MAX_UART_CMD_SIZE - 2) {
                            break;
                        }

                        // print character back
                        ad2_snprintf_host( (const char *) &rx_buffer[i], 1);

                        _cmd_buffer[_cmd_len++] = rx_buffer[i];
                    }
                } // switch rx_buffer[i]
            } //buf while loop

            // release the console
            ad2_give_host_console((void *)xTaskGetCurrentTaskHandle());
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
#if defined(AD2_STACK_REPORT)
#define EXTRA_INFO_EVERY 1000
        static int extra_info = EXTRA_INFO_EVERY;
        if(!--extra_info) {
            extra_info = EXTRA_INFO_EVERY;
            ESP_LOGI(TAG, "esp_uart_cli_task stack free %d", uxTaskGetStackHighWaterMark(NULL));
        }
#endif
    } //main loop


}

/**
 * @brief Start the CLI task after a few second pause to allow the user
 * to halt any further processing and stay in CLI mode.
 */
void uart_cli_main()
{
    /* to decide whether the main function is running or not by user action... */
    g_StopMainTask = 1;    //default value is 1;  stop for a timeout

    // uart cli worker thread
    // 20210815SM: 1404 bytes stack free after first connection.
    xTaskCreate(esp_uart_cli_task, "uart_cli_task", 1024*5, NULL, tskIDLE_PRIORITY+2, NULL);

    // Press \n to halt further processing and just enable CLI processing.
    ad2_printf_host(true, "Send '.' three times in the next 5 seconds to stop the init.");
    fflush(stdout);
    _cli_util_wait_for_user_input(5000);
    ad2_printf_host(true, "Starting main task.");

}

#ifdef __cplusplus
} // extern "C"
#endif

