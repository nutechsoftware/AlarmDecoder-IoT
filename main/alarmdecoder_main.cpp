/**
*  @file    alarmdecoder_main.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    02/20/2020
*  @version 1.0.3
*
*  @brief AlarmDecoder IoT embedded network appliance
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

static const char *TAG = "AD2_IoT";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "driver/uart.h"
#include "nvs_flash.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
#include "tcpip_adapter.h"
#else
#include "esp_netif.h"
#include "esp_tls.h"
#endif
// specific includes

// SimpleIni
#include <SimpleIni.h>

// OTA updates
#include "ota_util.h"

// Command line interface
#include "ad2_cli_cmd.h"

// SmartThings direct attached device support
#if CONFIG_STDK_IOT_CORE
#include "stsdk_main.h"
#endif

// ser2sockd support
#if CONFIG_AD2IOT_SER2SOCKD
#include "ser2sock.h"
#endif

// twilio support
#if CONFIG_AD2IOT_TWILIO_CLIENT
#include "twilio.h"
#endif

// twilio support
#if CONFIG_AD2IOT_PUSHOVER_CLIENT
#include "pushover.h"
#endif

// web server UI support
#if CONFIG_AD2IOT_WEBSERVER_UI
#include "webUI.h"
#endif

// MQTT client support
#if CONFIG_AD2IOT_MQTT_CLIENT
#include "ad2mqtt.h"
#endif

// FTP daemon support
#if CONFIG_AD2IOT_FTP_DAEMON
#include "ftpd.h"
#endif

/**
* Control main task processing
*  0 : running the main function.
*  1 : halted waiting for timeout to auto resume.
*  2 : halted.
*/
int g_StopMainTask = 0;

// all module init have finished no more calls to AlarmDecoderParser::subscribeTo
int g_init_done = 0;

// Critical section spin lock.
portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// global host console access mutex
SemaphoreHandle_t g_ad2_console_mutex = nullptr;

// global AlarmDecoder parser class instance
AlarmDecoderParser AD2Parse;

// global AD2 device connection fd/id <socket or uart id>
int g_ad2_client_handle = -1;

// global ad2 connection mode ['S'ocket | 'C'om port]
uint8_t g_ad2_mode = 0;

// global ad2 network EventGroup
EventGroupHandle_t g_ad2_net_event_group = nullptr;

// uSD card mounted
bool g_uSD_mounted = false;

// Device LED mode
int noti_led_mode = LED_ANIMATION_MODE_IDLE;

/**
 * AlarmDecoder callbacks.
 * As the AlarmDecoder receives data via put() data is validated.
 * When a complete messages is received or a specific stream of
 * bytes is received event(s) will be called.
 */

/**
 * @brief ON_ALPHA_MESSAGE
 * Called when a full standard alarm state message is received
 * but before it is parsed so the virtual state will be based upon
 * the last message parsed.
 *
 * @note WARNING the message may be invalid.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_ALPHA_MESSAGE_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    // match "Press *  to show faults" or "Hit * for faults" and send * to get zone list.
    // FIXME: Multi Language support.
    static unsigned long _last_faults_alert = 0;
    if (s && s->panel_type == ADEMCO_PANEL) {
        unsigned long _now = AD2Parse.monotonicTime();
        if (msg->find("Hit * for faults") != std::string::npos ||
                msg->find("Press *  to show faults") != std::string::npos) {
            // Some other system may initiate the report so wait a bit first.
            if (_last_faults_alert) {
                if ( _now - _last_faults_alert > 5) {
                    _last_faults_alert = 0;
                    std::string msg = ad2_string_printf("K%02i%s", s->primary_address, "*");
                    ad2_printf_host(true, "Sending '*' for zone report using address %i", s->primary_address);
                    ad2_send(msg);
                }
            } else {
                _last_faults_alert = _now;
            }
        }
        // clear alert if timeout.
        if ( _now - _last_faults_alert > 30) {
            _last_faults_alert = 0;
        }

    }
#if 0
#define ON_MESS_EXTRA_INFO_EVERY 10
    static int extra_info = ON_MESS_EXTRA_INFO_EVERY;
    if(!--extra_info) {
        extra_info = ON_MESS_EXTRA_INFO_EVERY;
        ESP_LOGI(TAG, "RAM left %d min %d maxblk %d", esp_get_free_heap_size(),esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        ESP_LOGI(TAG, "AlarmDecoder parser stack free %d", uxTaskGetStackHighWaterMark(NULL));
    }
#endif
}

/**
 * @brief ON_ZONE_CHANGE
 * Called when a ON_ZONE_CHANGE event occures.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_ZONE_CHANGE_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_ZONE_CHANGE_CB: EVSTR(%s)", (s ? s->last_event_message.c_str() : "UNKNOWN"));
}


/**
 * @brief ON_LRR
 * Called when a LRR message is received.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_LRR_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "LRR_CB: %s",msg->c_str());
}

/**
 * @brief ON_READY_CHANGE
 * Called when the READY state change event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_READY_CHANGE_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_READY_CHANGE: READY(%i) EXIT(%i) STAY(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_stay, s->armed_away);
}

/**
 * @brief ON_ARM
 * Called when a ARM event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_ARM_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_ARM: READY(%i) EXIT(%i) STAY(%i) AWAY(%i)", s->ready, s->exit_now, s->armed_stay, s->armed_away);
}

/**
 * @brief ON_DISARM
 * Called when a DISARM event is triggered.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_DISARM_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_DISARM: READY(%i)", s->ready);
}

/**
 * @brief ON_CHIME_CHANGE
 * Called when CHIME state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear Chime ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_CHIME_CHANGE_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_CHIME_CHANGE: CHIME(%i)", s->chime_on);
}

/**
 * @brief ON_FIRE_CHANGE
 * Called when FIRE state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear FIRE ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_FIRE_CHANGE_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_FIRE_CHANGE_CB: FIRE(%i)", s->fire_alarm);
}

/**
 * @brief ON_LOW_BATTERY
 * Called when a low battery state change event is triggered.
 * Contact sensor shows ACTIVE(LED ON) on APP when 'Open' so reverse logic
 * to make it clear LOW BATTERY ON = Contact LED ON
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 */
void my_ON_LOW_BATTERY_CB(std::string *msg, AD2PartitionState *s, void *arg)
{
    ESP_LOGI(TAG, "ON_LOW_BATTERY_CB: BATTERY(%i)", s->battery_low);
}

#if defined(CONFIG_AD2IOT_SER2SOCKD)
/**
 * @brief ON_RAW_RX_DATA
 * Called when data is sent into the parser.
 *
 * @param [in]msg std::string full AD2* message that triggered the event.
 * @param [in]s AD2PartitionState updated partition state for message.
 *
 * @note this is done before parsing.
 */
void SER2SOCKD_ON_RAW_RX_DATA(uint8_t *buffer, size_t s, void *arg)
{
    ser2sockd_sendall(buffer, s);
}
#endif

/**
 * @brief Generic callback for all AlarmDecoder API event subscriptions.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2PartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void ad2_on_state_change(std::string *msg, AD2PartitionState *s, void *arg)
{
    int msg_id;
    if (s) {
        cJSON *root = ad2_get_partition_state_json(s);
        cJSON_AddStringToObject(root, "event", AD2Parse.event_str[(int)arg].c_str());
        char *state = cJSON_Print(root);
        cJSON_Minify(state);

        // Notify CLI of the new state for easy console diagnostics of panel.
        ad2_printf_host(true, "%s: %s", TAG, state);

        cJSON_free(state);
        cJSON_Delete(root);
    }
}

/**
 * @brief Callback for config report from AD2*
 * Test all ad2config settings are correct on AD2* and if not
 * attempt to update. Potential loop of failure so force limits
 * on how many attempts to avoid the update failure loop.
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2PartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void ad2_on_cfg(std::string *msg, AD2PartitionState *s, void *arg)
{
    // For now allow only once per boot. Better would be to track pass/fail in NV
    // just in case we reboot loop config loop...
    static bool protectMode = false;

    std::string config;
    ESP_LOGI(TAG, "AD2* config string received. '%s'", AD2Parse.ad2_config_string.c_str());
    ad2_get_config_key_string(AD2MAIN_CONFIG_SECTION, AD2CONFIG_CONFIG_KEY, config);
    if (config.length() && AD2Parse.ad2_config_string.length()) {
        // find each NV pair in local config and test with AD2* config string.
        // Update reply string with updates for AD2* using 'C' command.
        // Test for known keys
        // MODE, ADDRESS, CONFIGBITS, LRR, COM, EXP, REL, MASK, DEDUPLICATE
        std::string updateConfig = "C";

        std::stringstream ss("MODE ADDRESS CONFIGBITS LRR COM EXP REL MASK DEDUPLICATE");
        std::string sk;
        bool sendUpdate = false;
        while (ss >> sk) {
            std::string _iotcfgval;
            if (AD2Parse.query_key_value_string(config, sk.c_str(), _iotcfgval) >= 0) {
                // force upper case for no case compare
                ad2_ucase(_iotcfgval);
                std::string _ad2cfgval;
                if (AD2Parse.query_key_value_string(AD2Parse.ad2_config_string, sk.c_str(), _ad2cfgval)) {
                    // force upper case for no case compare
                    ad2_ucase(_ad2cfgval);
                    if (_iotcfgval.compare(_ad2cfgval) != 0) {
                        sendUpdate=true;
                        updateConfig += sk + "=";
                        updateConfig += _iotcfgval;
                        updateConfig += "&";
                    }
                }
            }
        }
        if (sendUpdate) {
            if (!protectMode) {
                ESP_LOGI(TAG, "Sending '%s' to AlarmDecoder sync settings.", updateConfig.c_str());
                // finish command with line terminator and send
                updateConfig+="\r\n";
                ad2_send(updateConfig);
                // only allow this once. No fighting with others
                // over config settings if we can avoid it.
                protectMode = true;
            } else {
                ESP_LOGW(TAG, "Protect mode triggered. Unable to send '%s' to AlarmDecoder sync settings.", updateConfig.c_str());
            }
        }
    }
}

/**
 * @brief Callback for version report from AD2*
 *
 * @param [in]msg std::string panel message.
 * @param [in]s AD2PartitionState *.
 * @param [in]arg cast as int for event type (ON_ARM,,,).
 *
 */
void ad2_on_ver(std::string *msg, AD2PartitionState *s, void *arg)
{
}

#if CONFIG_AD2IOT_TOP
// Task state cache object
struct taskStats {
    taskStats() : last(0), total(0), lastBusy(0.0) {}
    unsigned int last;
    uint64_t total;
    double lastBusy;
};

// static storage for task counters and stats.
std::map<unsigned int, taskStats> task_times_cache;

// used to track exact time since last test.
static uint64_t uLastTime = 0;

/**
 * @brief Periodic task to update stats from FreeRTOS 32 bit counters
 * into 64bit counters. This will likely not be needed and can be
 * simplified in the future when esp32 build tools support 64bit time.
 */
void _update_top_task_stats()
{
    // get task count for memory storage size
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

    // make buffer for results
    TaskStatus_t *pxTSArray = (TaskStatus_t *)malloc(uxArraySize * sizeof( TaskStatus_t ));

    // sanity test did we get the memory.
    if (pxTSArray != NULL) {

        // init static uLastTime 64bit time
        if (!uLastTime) {
            uLastTime = hal_uptime_us();
        }

        // send null for 32 bit total time. We will use our own 64bit counter.
        uxArraySize = uxTaskGetSystemState( pxTSArray, uxArraySize, NULL );

        // 64bit microsecond system time overflows every 1.84467440737096E+019. A Very long time!
        uint64_t uTotalTime = hal_uptime_us();

        // sanity tests to avoid div zero etc.
        if( uxArraySize && uTotalTime > 0 ) {

            // calculate time since last called for current process CPU busy calculations.
            uint64_t uDeltaTime = uTotalTime - uLastTime;
            uLastTime = uTotalTime;

            // update cache for each process.
            taskENTER_CRITICAL(&spinlock);
            for( int x = 0; x < uxArraySize; x++ ) {

                // keep some data on local stack avoid array expansion multiple times.
                unsigned int _cur_TaskID = (unsigned int)pxTSArray[x].xTaskNumber;
                uint32_t _cur_RunTimeCounter = pxTSArray[x].ulRunTimeCounter;

                // find the task in the cache
                map<unsigned int, taskStats>::iterator it;
                it = task_times_cache.find(_cur_TaskID);

                // if found update
                if (it != task_times_cache.end()) {
                    // overflow handled using unsigned values
                    uint32_t tdelta  = 0;
                    if (_cur_RunTimeCounter < it->second.last) {
                        // overflow
                        tdelta = ((uint32_t)0xffffffff - it->second.total) + _cur_RunTimeCounter;
                    } else {
                        // no overflow
                        tdelta = _cur_RunTimeCounter - it->second.last;
                    }

                    // update task state
                    it->second.total += tdelta;
                    it->second.last = _cur_RunTimeCounter;
                    // calculate the process CPU usage since last update.
                    it->second.lastBusy = (double)((tdelta*100.0)/uDeltaTime);
                } else {
                    // not found first time init state
                    task_times_cache[_cur_TaskID].total = _cur_RunTimeCounter;
                    task_times_cache[_cur_TaskID].last = _cur_RunTimeCounter;
                }

            }
            taskEXIT_CRITICAL(&spinlock);
        }
        // all done release memory
        free(pxTSArray);

        // inform cli task data is ready if top is running
        // if it is not it will do nothing.
        cli_task_notify();
    }
}

/**
 * @brief Dump report of current tasks sorted by lowest stack free watermark.
 */
bool _show_pretty_process_list()
{
    // Format string for data columns.
    // "Name", "ID", "State", "Priority", "Stack", "CPU#", "TIME", "%BUSY, %CUR"
#define TABBED_HEADER_FMT "%-15s %-3s %-5s %-8s %-5s %-4s %-20s %-6s %-6s\r\n"
#define TABBED_LINE_FMT "%-15s %3u %-5s %8u %5u %4u %20llu %6.2f %6.2f\r\n"

    // Log time for human readable
    std::chrono::milliseconds ms(esp_log_timestamp());
    uint32_t days = std::chrono::duration_cast<std::chrono::seconds>(ms).count()/(60*60*24);

    // memory stats
    size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t heap_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    // how many tasks.
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

    // 64bit microsecond system time overflows every 1.84467440737096E+019. A Very long time!
    uint64_t uTotalTime = hal_uptime_us();

    // clear screen home cursor and print report header
    ad2_printf_host(false, "\033[H\033[2J\033[3J");
    ad2_printf_host(false, "top - %s up %i days TS: %llu Tasks: %-2lu\r\n", esp_log_system_timestamp(), days, uTotalTime, uxArraySize);
    ad2_printf_host(false, "Mem: %lu total, %lu free, %lu min free\r\n\r\n", heap_total, heap_free, heap_min_free);

    ad2_printf_host(false, "\033[7m");
    ad2_printf_host(false, TABBED_HEADER_FMT, "Name", "ID", "State", "Priority", "Stack", "CPU#", "Time", "%TBusy", "%Busy");
    ad2_printf_host(false, "\033[m");

    // make buffer for results
    TaskStatus_t *pxTSArray = (TaskStatus_t *)malloc(uxArraySize * sizeof( TaskStatus_t ));

    // sanity test did we get the memory.
    if (pxTSArray != NULL) {

        // send null for 32 bit total time. We will use our own 64bit counter.
        uxArraySize = uxTaskGetSystemState( pxTSArray, uxArraySize, NULL );

        // avoid div zero and other state checks
        if( uxArraySize && uTotalTime > 0 ) {

            // sort on stack high water mark lowest first.
            std::list<int> sort_list;
            std::sort(pxTSArray, pxTSArray+uxArraySize,
            [] (TaskStatus_t a, TaskStatus_t b) -> bool {
                return ((a.usStackHighWaterMark < b.usStackHighWaterMark));
            });

            // format each process for top list.
            for( int x = 0; x < uxArraySize; x++ ) {

                // get local task ID for easy use
                unsigned int _cur_TaskID = (unsigned int)pxTSArray[x].xTaskNumber;

                // format human readable state
                std::string state;
                switch (pxTSArray[x].eCurrentState) {
                case eRunning:
                case eReady  :
                    state = "R";
                    break;
                case eBlocked:
                    state = "B";
                    break;
                case eSuspended:
                    state = "S";
                    break;
                case eDeleted:
                    state = "D";
                    break;
                case eInvalid:
                    state = "?";
                    break;
                }

                // fetch this task stats object from cache
                // and store results local to minimize lock time.
                uint64_t uTaskTime = 0;
                double dlastBusy = 0;
                map<unsigned int, taskStats>::iterator it;
                taskENTER_CRITICAL(&spinlock);
                it = task_times_cache.find(_cur_TaskID);

                // if found.
                if (it != task_times_cache.end()) {
                    uTaskTime = it->second.total;
                    dlastBusy = it->second.lastBusy;
                } else {
                    uTaskTime =  pxTSArray[x].ulRunTimeCounter;
                }
                taskEXIT_CRITICAL(&spinlock);

                // send process detail line to host
                ad2_printf_host(false, TABBED_LINE_FMT,
                                pxTSArray[x].pcTaskName,
                                (unsigned int)_cur_TaskID,
                                state.c_str(),
                                (unsigned int)pxTSArray[x].uxCurrentPriority,
                                (unsigned int)pxTSArray[x].usStackHighWaterMark,
                                (unsigned int)pxTSArray[x].xCoreID,
                                (uint64_t)uTaskTime,
                                (double)((uTaskTime*100.00)/uTotalTime),
                                (double)dlastBusy);
            }
        }
        // all done release memory
        free(pxTSArray);
    }

    // print report footer
    ad2_printf_host(false,
                    "\r\n"
                    "   State legend\r\n"
                    "    'B'locked 'R'eady 'D'eleted 'S'uspended\r\n"
                    "   Column legend\r\n"
                    "    Stack: Minimum stack free bytes, CPU#: CPU affinity\r\n"
                    "    TBusy: %% busy total, Busy: %% busy now\r\n"
                   );
    return true;
}

/**
 * @brief event handler for top command
 *
 * @param [in]string command buffer pointer.
 *
 */
static void _cli_cmd_top_event(const char *string)
{
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];
    while(1) {
        if (ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(100)) != 0) {
            if (!_show_pretty_process_list()) {
                break;
            }
        }
        // Check if host sent any data exit command if true.
        int len = uart_read_bytes(UART_NUM_0, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);
        if (len == -1) {
            // An error happend. Sleep for a bit and try again?
            ESP_LOGE(TAG, "Error reading for UART aborting task.");
            break;
        }
        if (len>0) {
            break;
        }
    }
}
#endif

/**
 * @brief Main task to monitor physical button(s) and update state led(s).
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ad2_app_main_task(void *pvParameters)
{
    int button_event_type;
    int button_event_count;

    for (;;) {
        // do not process if main halted.
        if (!g_StopMainTask) {
            if (hal_get_button_event(&button_event_type, &button_event_count)) {
                // FIXME: update stsdk virtual button state
            }
#if (GPIO_MAINLED != GPIO_NOT_USED)
            if (noti_led_mode != LED_ANIMATION_MODE_IDLE) {
                hal_change_led_mode(noti_led_mode);
            }
#endif
        }
#if CONFIG_AD2IOT_TOP
        _update_top_task_stats();
#endif
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

/**
 * @brief ad2uart client task
 * process messages from local UART connected to the AD2*
 * device.
 *
 * @note Hardware uart may have noise during flashing or rebooting.
 * To help we will send down a bunch of line breaks to force the AD2*
 * into run mode. This is not necessary with socket connected AD2*.
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ad2uart_client_task(void *pvParameters)
{
    uint8_t rx_buffer[AD2_UART_RX_BUFF_SIZE];

    // send break to AD2* be sure we are in run mode.
    std::string breakline = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";
    uart_write_bytes((uart_port_t)g_ad2_client_handle, breakline.c_str(), breakline.length());

    // send a 'V" and a 'C' command to get version and configuration from the AD2*.
    std::string cmdline = "V\r\n\r\nC\r\n\r\n\r\n";
    uart_write_bytes((uart_port_t)g_ad2_client_handle, cmdline.c_str(), cmdline.length());

    while (1) {
        // do not process if main halted or network disconnected.
        // TODO: Cleanup continue to make it less network dependent.
        if (g_init_done && !g_StopMainTask && hal_get_network_connected()) {
            memset(rx_buffer, 0, AD2_UART_RX_BUFF_SIZE);

            // Read data from the UART
            int len = uart_read_bytes((uart_port_t)g_ad2_client_handle, rx_buffer, AD2_UART_RX_BUFF_SIZE - 1, 5 / portTICK_PERIOD_MS);
            if (len == -1) {
                // An error happend. Sleep for a bit and try again?
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
            if (len>0) {
                AD2Parse.put(rx_buffer, len);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/**
 * ser2sock_client_task private helper.
 *
 */
bool _ser2sock_client_connect(const char *args)
{
    // load the host and port params from the mode args.
    std::string buf = args;
    int res;

    // Storage for parsed Host & Port to connect to.
    int port = -1;
    std::string host;
    int addr_family = 0;
    int size;
#if CONFIG_LWIP_IPV6
    bool isv6 = false;
    struct sockaddr_in6 dest_addr6 = {};
    dest_addr6.sin6_family = AF_INET6;
#endif
    struct sockaddr_in dest_addr = {};

    std::regex rgx;
    std::smatch matches;
#if CONFIG_LWIP_IPV6
    // test for IPv6 host:port RFC 3986, section 3.2.2: Host. Must be surrounded by square braces.
    rgx = "^\\[(.*)\\]:(.*)$";
    if (std::regex_search(buf, matches, rgx)) {
        host = matches[1].str();
        port = std::atoi(matches[2].str().c_str());
        isv6 = true;
    } else
#endif
    {
        // Test for IPv4:PORT
        rgx = "^(.*):(.*)$";
        if (std::regex_search(buf, matches, rgx)) {
            host = matches[1].str();
            port = std::atoi(matches[2].str().c_str());
        }
    }

    // no valid address parsed sleep and restart.
    if (port == -1) {
        ESP_LOGE(TAG, "Error parsing host:port from settings '%s'. Sleeping for 30 seconds.", buf.c_str());
        // sleep a long time maybe the config will be updated live.
        vTaskDelay(30000 / portTICK_PERIOD_MS);
        return false;
    }

    ESP_LOGI(TAG, "Connecting to ser2sock host '%s' on port %i", host.c_str(), port);

    // detect IPv4 vs IPv6 host address see RFC 3986, section 3.2.2: Host
#if CONFIG_LWIP_IPV6
    if (isv6) {
        //res = inet_pton(AF_INET6, host.c_str(), &dest_addr6.sin6_addr);
        dest_addr6.sin6_port = htons(port);
        addr_family = AF_INET6;
        size = sizeof(struct sockaddr_in6);
    } else
#endif
    {
        dest_addr.sin_addr.s_addr = inet_addr(host.c_str());
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        addr_family = AF_INET;
        size = sizeof(struct sockaddr_in);
    }

    g_ad2_client_handle =  socket(addr_family, SOCK_STREAM, IPPROTO_TCP);
    if (g_ad2_client_handle < 0) {
        ESP_LOGE(TAG, "ser2sock client unable to create socket: errno %d", errno);
        return false;
    }
    ESP_LOGI(TAG, "ser2sock client socket created, connecting to host %s on port %d", host.c_str(), port);

#if CONFIG_LWIP_IPV6
    if (isv6) {
        res = connect(g_ad2_client_handle, (struct sockaddr *)&dest_addr6, size);
    } else
#endif
    {
        res = connect(g_ad2_client_handle, (struct sockaddr *)&dest_addr, size);
    }

    if (res != 0) {
        ESP_LOGE(TAG, "ser2sock client socket unable to connect: errno %d", errno);
        close(g_ad2_client_handle);
        g_ad2_client_handle = -1;
        return false;
    }
    ESP_LOGI(TAG, "ser2sock client successfully connected");

    // set socket non blocking.
    fcntl(g_ad2_client_handle, F_SETFL, O_NONBLOCK);

    // send break to AD2* be sure we are in run mode.
    buf = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";
    send((uart_port_t)g_ad2_client_handle, buf.c_str(), buf.length(), 0);

    // send a 'V" and a 'C' command to get version and configuration from the AD2*.
    buf = "V\r\n\r\nC\r\n\r\n\r\n";
    send((uart_port_t)g_ad2_client_handle, buf.c_str(), buf.length(), 0);

    return true;
}


/**
 * @brief ser2sock client task
 * Connects and stays connected to ser2sock server to receive
 * AD2* protocol messages from an alarm system.
 *
 * @param [in]pvParameters currently not used NULL.
 */
static void ser2sock_client_task(void *pvParameters)
{

    while (1) {
        if (hal_get_network_connected()) {

            if (_ser2sock_client_connect((const char *)pvParameters)) {
                while (1) {
                    // do not process if main halted.
                    if (g_init_done && !g_StopMainTask) {
                        uint8_t rx_buffer[128];
                        int len = recv(g_ad2_client_handle, rx_buffer, sizeof(rx_buffer) - 1, 0);
                        // test if error occurred
                        if (len < 0) {
                            if ( errno != EAGAIN ) {
                                ESP_LOGE(TAG, "ser2sock client recv failed: errno %d", errno);
                                break;
                            }
                        }
                        // Data received
                        else {
                            // Parse data from AD2* and report back to host.
                            rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                            AD2Parse.put(rx_buffer, len);
                        }
                    }
                    if (!hal_get_network_connected()) {
                        break;
                    }
                    vTaskDelay(10 / portTICK_PERIOD_MS);
#if defined(AD2_STACK_REPORT)
#define S2S_EXTRA_INFO_EVERY 1000
                    static int extra_info = S2S_EXTRA_INFO_EVERY;
                    if(!--extra_info) {
                        extra_info = S2S_EXTRA_INFO_EVERY;
                        ESP_LOGI(TAG, "ser2sock_client stack free %d", uxTaskGetStackHighWaterMark(NULL));
                    }
#endif
                }
            }

            ESP_LOGE(TAG, "ser2sock client shutting down socket and restarting in 3 seconds.");
            if (g_ad2_client_handle != -1) {
                shutdown(g_ad2_client_handle, 0);
                close(g_ad2_client_handle);
                g_ad2_client_handle = -1;
            }
#if defined(AD2_STACK_REPORT)
            ESP_LOGI(TAG, "ser2sock_client stack free %d", uxTaskGetStackHighWaterMark(NULL));
#endif
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/**
 * @brief Start ser2sock client task
 */
void init_ser2sock_client(const char *args)
{
    xTaskCreate(ser2sock_client_task, "AD2 ser2sock RX", 1024*8, (void*)strdup(args), tskIDLE_PRIORITY+2, NULL);
}

/**
 *  @brief Initialize the uart connected to the AD2 device
 */
void init_ad2_uart_client(const char *args)
{
    // load the GPIO pin TX:RX from the mode args.
    std::string port_pins = args;

    g_ad2_client_handle = UART_NUM_2;
    std::vector<std::string> out;
    ad2_tokenize(port_pins, ":", out);
    ad2_printf_host(true, "%s: Initialize AD2 UART client using txpin(%s) rxpin(%s)", TAG, out[0].c_str(),out[1].c_str());
    int tx_pin = atoi(out[0].c_str());
    int rx_pin = atoi(out[1].c_str());

    // Configure parameters of an UART driver,
    uart_config_t* uart_config = (uart_config_t*)calloc(sizeof(uart_config_t), 1);

    uart_config->baud_rate = 115200;
    uart_config->data_bits = UART_DATA_8_BITS;
    uart_config->parity    = UART_PARITY_DISABLE;
    uart_config->stop_bits = UART_STOP_BITS_1;
    uart_config->flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config->rx_flow_ctrl_thresh = 1;

    // esp_restart causes issues with UART2 don't set config if reset switch pushed.
    // https://github.com/espressif/esp-idf/issues/5274
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
    if (esp_reset_reason() != ESP_RST_SW) {
#endif
        uart_param_config((uart_port_t)g_ad2_client_handle, uart_config);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,1,0)
    }
#endif

    uart_set_pin((uart_port_t)g_ad2_client_handle,
                 tx_pin,   // TX
                 rx_pin,   // RX
                 UART_PIN_NO_CHANGE, // RTS
                 UART_PIN_NO_CHANGE);// CTS

    uart_driver_install((uart_port_t)g_ad2_client_handle, MAX_UART_CMD_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_LOWMED);
    free(uart_config);

    // Main AlarmDecoderParser:
    // 20210815SM: 1220 bytes stack free.
    // 20211201SM: expand to 8k. Main task for everything.
    xTaskCreate(ad2uart_client_task, "AD2 GPIO COM RX", 1024*8, (void *)AF_INET, tskIDLE_PRIORITY+2, NULL);

}


/**
 * @brief dump memory stats for debugging and tuning.
 *
 */
void dump_mem_stats()
{
    float Total = heap_caps_get_total_size(MALLOC_CAP_32BIT);
    float Free_Size = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    float DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    float IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ad2_printf_host(true, "Total Heap Size %.2f Kb\nFree Space %.2f Kb\nDRAM  %.2f Kb\nIRAM  %.2f Kb\n",Total/1024,Free_Size/1024,DRam/1024,IRam/1024);
}

// external call from FreeRTOS is CDECL
extern "C" {
    /**
     * @brief AlarmDecoder App main
     */
    void app_main()
    {

        // Create console access mutex.
        g_ad2_console_mutex = xSemaphoreCreateMutex();

        // Redirect ESP-IDF log to our own handler.
        esp_log_set_vprintf(&ad2_log_vprintf_host);

        // start with log level error.
        esp_log_level_set("*", ESP_LOG_NONE);        // set all components to ERROR level

        // init the AD2IoT gpio
        hal_gpio_init();

        // init host(USB) uart port
        hal_host_uart_init();
        ad2_printf_host(false, "\r\n");
        ad2_printf_host(true, AD2_SIGNON, TAG, FIRMWARE_VERSION, FIRMWARE_BUILDFLAGS);

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        ESP_ERROR_CHECK(esp_tls_init_global_ca_store());
#endif

        // dump the hardware info to the console
        hal_dump_hw_info();

        // initialize storage for config settings.
        hal_init_persistent_storage();

        // Initialize attached uSD card.
        if (hal_init_sd_card()) {
            g_uSD_mounted = true;
        }

        // Load persistent configuration ini
        size_t mem_a = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        ad2_load_persistent_config();
        size_t mem_b = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        ad2_printf_host(true, "%s: Approximate Configuration memory usage: %d B", TAG, mem_a - mem_b);

        // load and set the logging level.
        hal_set_log_mode(ad2_get_log_mode());

        // create event group
        g_ad2_net_event_group = xEventGroupCreate();

        // init the partition database from config storage
        // see ad2_cli_cmd::part
        // partition 1 is the default partition for some notifications.
        for (int n = 1; n <= AD2_MAX_PARTITION; n++) {
            int x = -1;
            std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(n);
            ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &x);
            // if we found a NV record then initialize the AD2PState for the mask.
            if (x != -1) {
                // Init AD2PState and set primary address
                AD2PartitionState *s = AD2Parse.getAD2PState(x, true);
                s->primary_address = x;

                // If a zone list is provided then parse it and save in the zone_list.
                std::string zlist;
                ad2_get_config_key_string(_section.c_str(), PART_CONFIG_ZONES, zlist);
                ad2_trim(zlist);
                if (zlist.length()) {
                    std::vector<std::string> vres;
                    ad2_tokenize(zlist, ",", vres);
                    for (auto &zonestring : vres) {
                        uint8_t z = std::atoi(zonestring.c_str());
                        s->zone_list.push_front((uint8_t)z & 0xff);
                    }
                }
                ad2_printf_host(true, "%s: init partition slot %i address %i zones '%s'", TAG, n, x, zlist.c_str());
            }
        }
        // Load Zone config "description" json string parse and save to AD2Parse class.
        std::string config;
        for (int n = 1; n <= AD2_MAX_ZONES; n++) {
            config = "";
            std::string _section = std::string(AD2ZONE_CONFIG_SECTION " ") + std::to_string(n);
            ad2_get_config_key_string(_section.c_str(), ZONE_CONFIG_DESCRIPTION, config);
            if (config.length()) {
                // Parse JSON string extract "alpha" and "type" strings.
                cJSON *json = cJSON_Parse(config.c_str());
                if (json) {
                    cJSON *jsonDesc = cJSON_GetObjectItem(json, "alpha");
                    if (cJSON_IsString(jsonDesc)) {
                        AD2Parse.setZoneString(n, jsonDesc->valuestring);
                    }
                    cJSON *jsonType = cJSON_GetObjectItem(json, "type");
                    if (cJSON_IsString(jsonType)) {
                        AD2Parse.setZoneType(n, jsonType->valuestring);
                    }
                }
            }
        }

#if CONFIG_AD2IOT_TOP
        static struct cli_command top_cmd = {
            (char*)AD2_CMD_TOP,(char*)
            "Usage: top"
            "\r\n"
            "    Provides a dynamic real-time view of the running system\r\n"
            "    Press any key to exit\r\n"
            , _cli_cmd_top_event
        };

        cli_register_command(&top_cmd);
#endif

#if CONFIG_STDK_IOT_CORE
        // Register STSDK CLI commands.
        stsdk_register_cmds();
#endif

#if CONFIG_AD2IOT_SER2SOCKD
        // Register ser2sock daemon CLI commands.
        ser2sockd_register_cmds();
#endif

#if CONFIG_AD2IOT_TWILIO_CLIENT
        // Register TWILIO CLI commands.
        twilio_register_cmds();
#endif

#if CONFIG_AD2IOT_PUSHOVER_CLIENT
        // Register PUSHOVER CLI commands.
        pushover_register_cmds();
#endif

#if CONFIG_AD2IOT_WEBSERVER_UI
        // Initialize WEB SEVER USER INTERFACE
        webui_register_cmds();
#endif

#if CONFIG_AD2IOT_MQTT_CLIENT
        // Register MQTT CLI commands.
        mqtt_register_cmds();
#endif

#if CONFIG_AD2IOT_FTP_DAEMON
        // Register FTPD DAEMON commands.
        ftpd_register_cmds();
#endif

        // Register AD2 CLI commands.
        register_ad2_cli_cmd();

        // Load AD2IoT operating mode [Socket|UART] and argument
        std::string ad2_mode_string = "";
        ad2_get_config_key_string(AD2MAIN_CONFIG_SECTION, AD2MODE_CONFIG_KEY, ad2_mode_string);


        // Create stream for parsing.
        std::istringstream ss(ad2_mode_string);

        // Load AD2 connection type Com|Socket from mode string
        std::string temp_mode;
        std::getline(ss, temp_mode, ' ');
        g_ad2_mode = temp_mode[0];

        // Load the connection args from the stream.
        std::string ad2_mode_args;
        std::getline(ss, ad2_mode_args, ' ');

        // If the hardware is local UART start it now.
        if (g_ad2_mode == 'C') {
            init_ad2_uart_client(ad2_mode_args.c_str());
        }

        // Start the CLI.
        // Press "..."" to halt startup and stay if a safe mode command line only.
        cli_main();

        if (g_ad2_mode == 'S') {
            ad2_printf_host(true, "Delaying start of ad2source SOCKET after network is up.");
        } else if(g_ad2_mode != 'C') {
            ESP_LOGI(TAG, "Unknown ad2source mode '%c'", g_ad2_mode);
            ad2_printf_host(true, "AlarmDecoder protocol source mode NOT configured. Configure using ad2source command.");
        }

#if CONFIG_STDK_IOT_CORE
        // Enable STSDK if no setting found.
        bool stEN = true;
        ad2_get_config_key_bool(STSDK_CONFIG_SECTION, STSDK_SUBCMD_ENABLE, &stEN);
#endif

        // get the network mode set default mode to 'N'
        std::string netmode_args;
        char net_mode = ad2_get_network_mode(netmode_args);
        ad2_printf_host(true, "%s: 'netmode' set to '%c'.", TAG, net_mode);

        /**
         * Start the network TCP/IP driver stack if Ethernet or Wifi enabled.
         */
        if ( net_mode != 'N') {
            hal_init_network_stack();
        }

        /**
         * Start the network hardware driver(s)
         *
         * FIXME: SmartThings needs to manage the Wifi during adopting.
         * For now just one interface more is much more complex.
         */
#if CONFIG_AD2IOT_USE_ETHERNET
        if ( net_mode == 'E') {
            // Init the hardware ethernet driver and hadware
            hal_init_eth(netmode_args);
        }
#endif
#if CONFIG_AD2IOT_USE_WIFI
        if ( net_mode == 'W') {
            // Init the wifi driver and hardware
            hal_init_wifi(netmode_args);
        }
#endif

#if 0 // FIXME add build switch for release builds.
        // AlarmDecoder callback wire up for testing.
        AD2Parse.subscribeTo(ON_ZONE_CHANGE, my_ON_ZONE_CHANGE_CB, nullptr);
        AD2Parse.subscribeTo(ON_LRR, my_ON_LRR_CB, nullptr);
        AD2Parse.subscribeTo(ON_ARM, my_ON_ARM_CB, nullptr);
        AD2Parse.subscribeTo(ON_DISARM, my_ON_DISARM_CB, nullptr);
        AD2Parse.subscribeTo(ON_READY_CHANGE, my_ON_READY_CHANGE_CB, nullptr);
        AD2Parse.subscribeTo(ON_CHIME_CHANGE, my_ON_CHIME_CHANGE_CB, nullptr);
        AD2Parse.subscribeTo(ON_FIRE_CHANGE, my_ON_FIRE_CHANGE_CB, nullptr);
        AD2Parse.subscribeTo(ON_LOW_BATTERY, my_ON_LOW_BATTERY_CB, nullptr);
#else
        // Subscribe standard AlarmDecoder events
        AD2Parse.subscribeTo(ON_ALPHA_MESSAGE, my_ON_ALPHA_MESSAGE_CB, nullptr);
        AD2Parse.subscribeTo(ON_ARM, ad2_on_state_change, (void *)ON_ARM);
        AD2Parse.subscribeTo(ON_DISARM, ad2_on_state_change, (void *)ON_DISARM);
        AD2Parse.subscribeTo(ON_CHIME_CHANGE, ad2_on_state_change, (void *)ON_CHIME_CHANGE);
        AD2Parse.subscribeTo(ON_BEEPS_CHANGE, ad2_on_state_change, (void *)ON_BEEPS_CHANGE);
        AD2Parse.subscribeTo(ON_FIRE_CHANGE, ad2_on_state_change, (void *)ON_FIRE_CHANGE);
        AD2Parse.subscribeTo(ON_POWER_CHANGE, ad2_on_state_change, (void *)ON_POWER_CHANGE);
        AD2Parse.subscribeTo(ON_READY_CHANGE, ad2_on_state_change, (void *)ON_READY_CHANGE);
        AD2Parse.subscribeTo(ON_LOW_BATTERY, ad2_on_state_change, (void *)ON_LOW_BATTERY);
        AD2Parse.subscribeTo(ON_ALARM_CHANGE, ad2_on_state_change, (void *)ON_ALARM_CHANGE);
        AD2Parse.subscribeTo(ON_ZONE_BYPASSED_CHANGE, ad2_on_state_change, (void *)ON_ZONE_BYPASSED_CHANGE);
        AD2Parse.subscribeTo(ON_EXIT_CHANGE, ad2_on_state_change, (void *)ON_EXIT_CHANGE);
        AD2Parse.subscribeTo(ON_CFG, ad2_on_cfg, (void *)ON_CFG);
        AD2Parse.subscribeTo(ON_VER, ad2_on_ver, (void *)ON_VER);
#endif

        // Start components

        // Initialize ad2 HTTP request sendQ and consumer task.
        ad2_init_http_sendQ();

#if CONFIG_AD2IOT_TWILIO_CLIENT
        // Initialize twilio client
        twilio_init();
#endif
#if CONFIG_AD2IOT_PUSHOVER_CLIENT
        // Initialize pushover client
        pushover_init();
#endif
#if CONFIG_AD2IOT_WEBSERVER_UI
        // Initialize WEB SEVER USER INTERFACE
        webui_init();
#endif
#if CONFIG_AD2IOT_MQTT_CLIENT
        // Initialize MQTT client
        mqtt_init();
#endif
#if CONFIG_AD2IOT_FTP_DAEMON
        // Initialize FTP daemon
        ftpd_init();
#endif

        // Sleep for another 5 seconds. Hopefully wifi is up before we continue connecting the AD2*.
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        // Start main AlarmDecoder IoT app task
        xTaskCreate(ad2_app_main_task, "AD2 main", 1024*4, NULL, tskIDLE_PRIORITY+1, NULL);

        // Start firmware update task
        ota_init();

        // If the AD2* is a socket connection we can hopefully start it now.
        if (g_ad2_mode == 'S') {
            init_ser2sock_client(ad2_mode_args.c_str());
        }

#if CONFIG_AD2IOT_SER2SOCKD
        // init ser2sock server
        ser2sockd_init();
        AD2Parse.subscribeTo(SER2SOCKD_ON_RAW_RX_DATA, nullptr);
#endif

#if CONFIG_STDK_IOT_CORE
        // Disable stsdk if network mode is not N
        if ( net_mode == 'N') {
            /**
             * Initialize SmartThings SDK
             *
             *  WARNING: If enabled it will consume the esp_event_loop_init and
             * only one task can create this.
             */
            stsdk_init();

            // Kick off a connect to SmartThings server to get things started.
            // Warning: Blocking until completion of onboarding
            stsdk_connection_start();
        } else {
            ESP_LOGI(TAG, "'netmode' <> 'N' disabling SmartThings.");
            ad2_printf_host(true, "'netmode' <> 'N' disabling SmartThings.");
        }
#endif

        // Init finished parsing data from the AD2* can now safely start.
        g_init_done = true;

    }

} // extern "C"
