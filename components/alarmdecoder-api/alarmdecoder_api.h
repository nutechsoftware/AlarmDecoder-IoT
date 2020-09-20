/**
 *  @file    alarmdecoder_api.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/15/2020
 *
 *  @brief AlarmDecoder embedded state machine and parser
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
#ifndef alarmdecoder_api_h
#define alarmdecoder_api_h

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <bits/stdc++.h>
#include <map>
#include "esp_log.h"
using namespace std;

// types and defines

enum AD2_PARSER_STATES {
  AD2_PARSER_RESET            = 0,
  AD2_PARSER_SCANNING_START   = 1,
  AD2_PARSER_SCANNING_EOL     = 2,
  AD2_PARSER_PROCESSING       = 3
};

// The actual max is ~90 but leave some room for future.
// WARN: use int8_t so must be <= 127 for overflow protection.
#define ALARMDECODER_MAX_MESSAGE_SIZE 120

#define BIT_ON '1'
#define BIT_OFF '0'
#define BIT_UNDEFINED '-'

/** example Keypad Message.
[10000001000000003A--],008,[f70600ff1008001c08020000000000],"****DISARMED****  Ready to Arm  "
*/
// KPI
#define SECTION_1_START     0
#define SECTION_2_START    23
#define SECTION_3_START    27
#define SECTION_4_START    61
#define AMASK_START        30
#define AMASK_END          38
#define CURSOR_TYPE_POS    SECTION_3_START+19
#define CURSOR_POS         SECTION_3_START+21

// BIT/DATA OFFSETS
#define READY_BYTE          1
#define ARMED_AWAY_BYTE     2
#define ARMED_HOME_BYTE     3
#define BACKLIGHT_BYTE      4
#define PROGMODE_BYTE       5
#define BEEPMODE_BYTE       6
#define BYPASS_BYTE         7
#define ACPOWER_BYTE        8
#define CHIME_BYTE          9
#define ALARMSTICKY_BYTE   10
#define ALARM_BYTE         11
#define LOWBATTERY_BYTE    12
#define ENTRYDELAY_BYTE    13
#define FIRE_BYTE          14
#define SYSISSUE_BYTE      15
#define PERIMETERONLY_BYTE 16
#define SYSSPECIFIC_BYTE   17
#define PANEL_TYPE_BYTE    18
#define UNUSED_1_BYTE      19
#define UNUSED_2_BYTE      20

#define ADEMCO_PANEL       'A'
#define DSC_PANEL          'D'



/**
 * Utility functions/macros.
 */

#define AD2_NTOHL(x) ((((x) & 0xff000000UL) >> 24) | \
                    (((x) & 0x00ff0000UL) >>  8) | \
                    (((x) & 0x0000ff00UL) <<  8) | \
                    (((x) & 0x000000ffUL) << 24))

/**
 * Data structure for each Virtual partition state.
 *
 * https://www.alarmdecoder.com/wiki/index.php/Protocol
 *
 * The AlarmDecoder Alphanumeric Keypad Message has 4 sections.
 * 1) Bit Fields   - [00110011000000003A--]
 * 2) Numeric data - 010
 * 3) Raw Data     - [f70200010010808c18020000000000]
 *                      ^^^^^^^^ <- Address bytes
 * 4) Alpha Data   - "ARMED ***STAY** ZONE BYPASSED "
 *
 * The Partition/Address data is kept in section #3
 * bytes 2-9
 *  https://www.alarmdecoder.com/wiki/index.php/Protocol#Raw_data
 *
 * This 32 bit Little Endian word uses each bit to represent a single
 * address(Ademco) or partition(DSC) from 0 to 31.
 *
 * Ademco example
 *  0x02000100 = This message was for keypads at address 1 and 16.
 *
 * DSC example
 *  0x02000000 = This message was for partition #1
 *
 * To prevent mixing data between partitions a separate instance of this
 * storage structure will be used for each unique mask received from the
 * AlarmDecoder using the 32 bit address mask as a unique key.
 *
 * To determin the actual partition for each structure on Ademco it would be
 * necessary to have a translation between keypad address and partition.
 *
 */
class AD2VirtualPartitionState
{
  public:

  // Address mask filter for this partition
  uint32_t address_mask_filter;

  // Partition number(external lookup required for Ademco)
  uint8_t partition;

  // Calculated from section #3(Raw)
  uint8_t display_cursor_type = 0;     // 0[OFF] 1[UNDERLINE] 2[INVERT]
  uint8_t display_cursor_location = 0; // 1-32

  // SECTION #1 data
  //  https://www.alarmdecoder.com/wiki/index.php/Protocol#Bit_field
  bool unknown_state = true;
  bool ready = false;
  bool armed_away = false;
  bool armed_home = false;
  bool backlight_on = false;
  bool programming_mode = false;
  bool zone_bypassed = false;
  bool ac_power = false;
  bool chime_on = false;
  bool alarm_event_occurred = false;
  bool alarm_sounding = false;
  bool battery_low = false;
  bool entry_delay_off = false;
  bool fire_alarm = false;
  bool system_issue = false;
  bool perimeter_only = false;
  bool exit_now = false;
  bool system_specific = false;
  uint8_t beeps = 0;
  char panel_type = '?';
  bool unused1 = false;
  bool unused2 = false;

  std::string last_alpha_message = "";
  std::string last_numeric_message = "";

};

typedef std::map<uint32_t, AD2VirtualPartitionState *> ad2pstates_t;
typedef void (*AD2ParserCallback_msg_t)(std::string*, AD2VirtualPartitionState*);

/**
 * AlarmDecoder protocol parser.
 * 1) Able to receive partial messages with subsequent calls to complete full
 *   messages.
 * 2) consume all data received and if not complete preserve for sub.
 * 2) upon receiving a full message parse and call any events that trigger.
 * 3) continue processing data
 */
class AlarmDecoderParser
{
  public:

    AlarmDecoderParser();

    // Subscribe to callbacks.
    void setCB_ON_RAW_MESSAGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_ARM(AD2ParserCallback_msg_t cb);
    void setCB_ON_DISARM(AD2ParserCallback_msg_t cb);
    void setCB_ON_POWER_CHANGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_READY_CHANGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_ALARM(AD2ParserCallback_msg_t cb);
    void setCB_ON_ALARM_RESTORED(AD2ParserCallback_msg_t cb);
    void setCB_ON_FIRE(AD2ParserCallback_msg_t cb);
    void setCB_ON_BYPASS(AD2ParserCallback_msg_t cb);
    void setCB_ON_BOOT(AD2ParserCallback_msg_t cb);
    void setCB_ON_CONFIG_RECEIVED(AD2ParserCallback_msg_t cb);
    void setCB_ON_ZONE_FAULT(AD2ParserCallback_msg_t cb);
    void setCB_ON_ZONE_RESTORE(AD2ParserCallback_msg_t cb);
    void setCB_ON_LOW_BATTERY(AD2ParserCallback_msg_t cb);
    void setCB_ON_PANIC(AD2ParserCallback_msg_t cb);
    void setCB_ON_RELAY_CHANGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_CHIME_CHANGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_MESSAGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_EXPANDER_MESSAGE(AD2ParserCallback_msg_t cb);
    void setCB_ON_LRR(AD2ParserCallback_msg_t cb);
    void setCB_ON_RFX(AD2ParserCallback_msg_t cb);
    void setCB_ON_SENDING_RECEIVED(AD2ParserCallback_msg_t cb);
    void setCB_ON_AUI(AD2ParserCallback_msg_t cb);
    void setCB_ON_KPM(AD2ParserCallback_msg_t cb);
    void setCB_ON_KPE(AD2ParserCallback_msg_t cb);
    void setCB_ON_CRC(AD2ParserCallback_msg_t cb);
    void setCB_ON_VER(AD2ParserCallback_msg_t cb);
    void setCB_ON_ERR(AD2ParserCallback_msg_t cb);

    // Callback functino pointers.
    AD2ParserCallback_msg_t ON_RAW_MESSAGE_CB;
    AD2ParserCallback_msg_t ON_ARM_CB;
    AD2ParserCallback_msg_t ON_DISARM_CB;
    AD2ParserCallback_msg_t ON_POWER_CHANGE_CB;
    AD2ParserCallback_msg_t ON_READY_CHANGE_CB;
    AD2ParserCallback_msg_t ON_ALARM_CB;
    AD2ParserCallback_msg_t ON_ALARM_RESTORED_CB;
    AD2ParserCallback_msg_t ON_FIRE_CB;
    AD2ParserCallback_msg_t ON_BYPASS_CB;
    AD2ParserCallback_msg_t ON_BOOT_CB;
    AD2ParserCallback_msg_t ON_CONFIG_RECEIVED_CB;
    AD2ParserCallback_msg_t ON_ZONE_FAULT_CB;
    AD2ParserCallback_msg_t ON_ZONE_RESTORE_CB;
    AD2ParserCallback_msg_t ON_LOW_BATTERY_CB;
    AD2ParserCallback_msg_t ON_PANIC_CB;
    AD2ParserCallback_msg_t ON_RELAY_CHANGE_CB;
    AD2ParserCallback_msg_t ON_CHIME_CHANGE_CB;
    AD2ParserCallback_msg_t ON_MESSAGE_CB;
    AD2ParserCallback_msg_t ON_EXPANDER_MESSAGE_CB;
    AD2ParserCallback_msg_t ON_LRR_CB;
    AD2ParserCallback_msg_t ON_RFX_CB;
    AD2ParserCallback_msg_t ON_SENDING_RECEIVED_CB;
    AD2ParserCallback_msg_t ON_AUI_CB;
    AD2ParserCallback_msg_t ON_KPM_CB;
    AD2ParserCallback_msg_t ON_KPE_CB;
    AD2ParserCallback_msg_t ON_CRC_CB;
    AD2ParserCallback_msg_t ON_VER_CB;
    AD2ParserCallback_msg_t ON_ERR_CB;


    // Push data into state machine. Events fire if a complete message is
    // received.
    bool put(uint8_t *buf, int8_t len);

    // Reset the parser state machine.
    void reset_parser();

    // get AD2PPState by mask create if flag is set and no match found.
    AD2VirtualPartitionState * getAD2PState(uint32_t *mask, bool update=false);

    void test();




  protected:
    // Track all panel states in separate class.
    ad2pstates_t AD2PStates;

    // Parser state control starts out as AD2_PARSER_RESET.
    int AD2_Parser_State;

    // ring buffer must be < 128 bytes using signed int8 for pointer variable.
    uint8_t ring_buffer[ALARMDECODER_MAX_MESSAGE_SIZE];
    int8_t ring_qin_position, ring_qout_position;
    uint16_t ring_error_count;

};



// Forward decls.
void ad2test(void);

// Utility functions.
bool is_bit_set(int pos, const char * bitStr);

#endif // alarmdecoder_api_h
