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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// esp includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"
static const char *TAG = "AD2UTIL";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"

#include "ad2_uart_cli.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct cli_command_list *cli_cmd_list;

static void cli_cmd_help(char *string);

static struct cli_command help_cmd = {
    (char*)"help", (char*)"print command list", cli_cmd_help
};

/**
 * @brief Check if a command is registered.
 *
 * @param [in]input_string string containing the command to check.
 *
 * @return cli_cmd_t * the command structure pointer or NULL
 * if not found
 */
static cli_cmd_t* cli_find_command (char* input_string)
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
        printf("command not found. please check 'help'\n");
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
    char buf[20]; // buffer to hold help argument

    if (ad2_copy_nth_arg(buf, cmd, sizeof(buf), 1) >= 0) {
        cli_cmd_t *command;
        command = cli_find_command(buf);
        if (command != NULL) {
            printf("Help for command '%s'\n%s\n", command->command, command->help_string);
            showhelp = false;
        } else {
            printf("Command not found '%s'\n", cmd);
        }
    }

    if (showhelp) {
        printf("Available AD2IoT terminal commands\n  [");
        while (now) {
            if (!now->cmd) {
                continue;
            }
            printf("%s",now->cmd->command);
            now = now->next;
            if (now) {
                printf(", ");
            }
        }
        printf("]\n\nType help <command> for details on each command.\n");
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
        portENTER_CRITICAL(&spinlock);
        if (g_StopMainTask != 1) {
            portEXIT_CRITICAL(&spinlock);
            break;
        }
        portEXIT_CRITICAL(&spinlock);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    portENTER_CRITICAL(&spinlock);
    if (g_StopMainTask == 1) {
        // When there is no input("\n") for a set time, main task will be executed...
        g_StopMainTask = 0;
    }
    portEXIT_CRITICAL(&spinlock);

    if (g_StopMainTask != 0) {
        while (1) {
            portENTER_CRITICAL(&spinlock);
            if (g_StopMainTask == 0) {
                portEXIT_CRITICAL(&spinlock);
                break;
            }
            portEXIT_CRITICAL(&spinlock);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief UART0 settings
 */
static void esp_uart_init()
{

    // Configure parameters of an UART driver,
    uart_config_t* uart_config = (uart_config_t*)calloc(sizeof(uart_config_t), 1);

    uart_config->baud_rate = 115200;
    uart_config->data_bits = UART_DATA_8_BITS;
    uart_config->parity    = UART_PARITY_DISABLE;
    uart_config->stop_bits = UART_STOP_BITS_1;
    uart_config->flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    uart_param_config(UART_NUM_0, uart_config);
    uart_driver_install(UART_NUM_0, MAX_UART_LINE_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);

    free(uart_config);
}

/**
 * @brief UART CLI task
 * Echo back valid data and allow for simple left/right cursor navigation.
 *   Port: UART0
 *   Receive (Rx) buffer: on
 *   Receive (tx) buffer: off
 *   Flow control: off
 *   Event queue: off
 *
 * @param [in]pvParameters void * to parameters.
 *
 */
static void esp_uart_cli_task(void *pvParameters)
{

    // Configure a temporary buffer for the incoming data
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];
    uint8_t line[MAX_UART_LINE_SIZE];
    uint8_t prev_line[MAX_UART_LINE_SIZE];
    memset(line, 0, MAX_UART_LINE_SIZE);
    memset(prev_line, 0, MAX_UART_LINE_SIZE);
    int line_len = 0;

    cli_register_command(&help_cmd);

    while (1) {
        memset(rx_buffer, 0, AD2_UART_RX_BUFF_SIZE);

        // Read data from the UART
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 20 / portTICK_RATE_MS);
        for (int i = 0; i < len; i++) {
            switch(rx_buffer[i]) {
            case '\r':
            case '\n':
                portENTER_CRITICAL(&spinlock);
                if (g_StopMainTask == 1) {
                    // when there is a user input("\n") within a given timeout, this value will be chaned into 2.
                    // but, if there is no user input within a given timeout, this value will be changed into 0 in order to run the main function
                    g_StopMainTask = 2;
                }
                portEXIT_CRITICAL(&spinlock);

                uart_write_bytes(UART_NUM_0, "\r\n", 2);
                if (line_len) {
                    cli_process_command((char *)line);
                    memcpy(prev_line, line, MAX_UART_LINE_SIZE);
                    memset(line, 0, MAX_UART_LINE_SIZE);
                    line_len = 0;
                }
                uart_write_bytes(UART_NUM_0, PROMPT_STRING, sizeof(PROMPT_STRING));
                break;

            case '\b':
                //backspace
                if (line_len > 0) {
                    uart_write_bytes(UART_NUM_0, "\b \b", 3);
                    line[--line_len] = '\0';
                }
                break;

            case 0x03: //Ctrl + C
                uart_write_bytes(UART_NUM_0, "^C\n", 3);
                memset(line, 0, MAX_UART_LINE_SIZE);
                line_len = 0;
                uart_write_bytes(UART_NUM_0, PROMPT_STRING, sizeof(PROMPT_STRING));
                break;

            case 0x1B: //arrow keys : 0x1B 0x5B 0x41~44
                if ( rx_buffer[i+1] == 0x5B ) {
                    switch (rx_buffer[i+2]) {
                    case 0x41: //UP
                        memcpy(line, prev_line, MAX_UART_LINE_SIZE);
                        line_len = strlen((char*)line);
                        uart_write_bytes(UART_NUM_0, (const char *)&rx_buffer[i+1], 2);
                        uart_write_bytes(UART_NUM_0, "\r\n", 2);
                        uart_write_bytes(UART_NUM_0, PROMPT_STRING, sizeof(PROMPT_STRING));
                        uart_write_bytes(UART_NUM_0, (const char *)line, line_len);
                        i+=3;
                        break;
                    case 0x42: //DOWN - ignore
                        i+=3;
                        break;
                    case 0x43: //right
                        if (line[line_len+1] != '\0') {
                            line_len += 1;
                            uart_write_bytes(UART_NUM_0, (const char *)&rx_buffer[i], 3);
                        }
                        i+=3;
                        break;
                    case 0x44: //left
                        if (line_len > 0) {
                            line_len -= 1;
                            uart_write_bytes(UART_NUM_0, (const char *)&rx_buffer[i], 3);
                        }
                        i+=3;
                        break;
                    default:
                        break;
                    }
                }
                break;

            default:
                //check whether character is valid
                if ((rx_buffer[i] >= ' ') && (rx_buffer[i] <= '~')) {
                    if (line_len >= MAX_UART_LINE_SIZE - 2) {
                        break;
                    }

                    // print character back
                    uart_write_bytes(UART_NUM_0, (const char *) &rx_buffer[i], 1);

                    line[line_len++] = rx_buffer[i];
                }
            } // switch rx_buffer[i]
        } //buf while loop
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

    esp_uart_init();

    xTaskCreate(esp_uart_cli_task, "uart_cli_task", CLI_TASK_SIZE, NULL, CLI_TASK_PRIORITY, NULL);

    // Press \n to halt further processing and just enable CLI processing.
    printf("Press enter to stop init.");
    fflush(stdout);
    _cli_util_wait_for_user_input(5000);
    printf(" Starting main task.\n");
}

#ifdef __cplusplus
} // extern "C"
#endif

