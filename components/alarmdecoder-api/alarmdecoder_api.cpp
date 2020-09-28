/**
 *  @file    alarmdecoder_api.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/15/2020
 *  @version 1.0.1
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

#include "alarmdecoder_api.h"

static const char *TAG = "AD2API";

// nostate
AD2VirtualPartitionState *nostate = 0;

/**
 * @brief constructor
 */
AlarmDecoderParser::AlarmDecoderParser()
{

    // Reset the parser on init.
    reset_parser();

}

/**
 * @brief Reset parser processing state.
 */
void AlarmDecoderParser::reset_parser()
{
    // Initialize parser state machine state.
    AD2_Parser_State = AD2_PARSER_RESET;
}

/**
 * @brief Update the available version and trigger any
 * subscribers that are watching.
 *
 * @param [in]arg char * version string
 */
void AlarmDecoderParser::updateVersion(char *arg)
{
    std::string version = arg;
    ESP_LOGI(TAG, "updateVersion %s",arg);
    notifySubscribers(ON_FIRMWARE_VERSION, version, nullptr);
}

/**
 * @brief Subscribe to a EVENT type.
 *
 * @param [in]ev ad2_event_t event TYPE
 *   ex. ON_MESSAGE
 * @param [in]arg pointer to argument to pass to subscriber on event.
 */
void AlarmDecoderParser::subscribeTo(ad2_event_t ev, AD2ParserCallback_sub_t fn, void *arg)
{
    subscribers_t& v = AD2Subscribers[ev];
    v.push_back(AD2SubScriber(fn, arg));
}

/**
 * @brief Sequentially call each subscriber function in the list.
 *
 * @param [in]ev event class.
 * @param [in]msg message that generated event.
 * @param [in]pstate virtual partition state
 *
 */
void AlarmDecoderParser::notifySubscribers(ad2_event_t ev, std::string &msg, AD2VirtualPartitionState *pstate)
{
    for ( subscribers_t::iterator i = AD2Subscribers[ev].begin(); i != AD2Subscribers[ev].end(); ++i ) {
        (i->fn)(&msg, pstate, i->arg);
    }
}

/**
 * @brief Return a partition state structure by 32bit keypad/partition mask.
 *
 * @param [in]amask pointer to 32bit mask to search for.
 * @param [in]update if true update mask adding new bits.
 *
 */
AD2VirtualPartitionState * AlarmDecoderParser::getAD2PState(uint32_t *amask, bool update)
{
    // Create or return a pointer to our partition storage class.
    AD2VirtualPartitionState *ad2ps = nullptr;

    // look for an exact match.
    if ( AD2PStates.find(*amask) == AD2PStates.end()) {

        // Not found create or find a mask that has at least
        // one bit in common and update its mask to include the new
        // bits. System partition mask is 0 and will skip this logic.
        if (*amask) {
            uint32_t foundkey = *amask;
            for (auto const& x : AD2PStates) {
                // Mask has matching bits use it instead.
                if (x.first & *amask) {
                    ad2ps = AD2PStates[x.first];
                    foundkey = x.first;
                    break;
                }
            }

            // Not necessary but seems reasonable.
            // Update key to include new mask if it changed.
            if (foundkey != *amask && update) {
                // remove the old map entry.
                AD2PStates.erase(foundkey);
                // Add new one with mask of original + new.
                *amask |= foundkey;
                AD2PStates[*amask] = ad2ps;
            }
        }

        // Did not find entry. Make new.
        if (!ad2ps && update) {
            ad2ps = AD2PStates[*amask] = new AD2VirtualPartitionState;
            ad2ps->partition = AD2PStates.size();
        }

    } else {
        // Found. Grab the state class ptr.
        ad2ps = AD2PStates[*amask];
    }
    return ad2ps;
}

/**
 * @brief Consume bytes from an AlarmDecoder stream into a small ring
 * buffer for processing.
 *
 * @param [in]buff byte buffer to process.
 * @param [in]len length of data in buff
 *
 * @note Parse all of the data firing off events upon parsing a full message.
 *   Continue parsing data until all is consumed.
 */
bool AlarmDecoderParser::put(uint8_t *buff, int8_t len)
{

    // All AlarmDecoder messages are '\n' terminated.
    // "!boot.....done" is the only state exists that needs notification
    // before the terminator in order to enter the boot loader.
    // If KPM config bit is set then all messages will start with '!'
    // If KPM config bit is not set(the default) then standard keypad state
    // messages start with '['.

    int8_t bytes_left = len;
    uint8_t *bp = buff;

    // Sanity check.
    if (len<=0) {
        return false;
    }

    // Consume all the bytes.
    while (bytes_left>0) {

        uint8_t ch;

        // Update state machine.
        switch (AD2_Parser_State) {

        // Reset the ring buffer state.
        case AD2_PARSER_RESET:

            ring_qin_position = 0;
            ring_qout_position = 0;

            // Initialize AlarmDecoder stream parsing ring buffer memory to 0s.
            memset(ring_buffer, 0, sizeof(ring_buffer));
            AD2_Parser_State = AD2_PARSER_SCANNING_START;
            break;

        // Wait for ALPHA/NUMERIC after an EOL
        case AD2_PARSER_SCANNING_START:
            // store local.
            ch = *bp;

            // start scanning for EOL as soon as we have a printable character.
            if (ch > 31 && ch < 127) {
                AD2_Parser_State = AD2_PARSER_SCANNING_EOL;
            } else {
                // Dump the byte.
                // Update remaining bytes counter and move ptr.
                bp++;
                bytes_left--;
            }
            break;

        // Consume bytes looking for terminator.
        case AD2_PARSER_SCANNING_EOL:
            // store local.
            ch = *bp;

            // Protection from external leaks or faults. Very unlikely to happen.
            // Do not consume the byte and try again after a reset.
            if (ring_qin_position > ALARMDECODER_MAX_MESSAGE_SIZE) {

                // Next state is RESET.
                AD2_Parser_State = AD2_PARSER_RESET;

                // Done.
                break;
            }

            // Protect from corrupt data skip and reset.
            // All bytes must be CR/LF or printable characters only.
            if ( ch != '\r' && ch != '\n' ) {
                if ( ch < 32 || ch > 126 ) {

                    // Update remaining bytes counter and move ptr.
                    bp++;
                    bytes_left--;

                    // Next state is RESET.
                    AD2_Parser_State = AD2_PARSER_RESET;

                    // Done.
                    break;
                }
            }

            // Update remaining bytes counter and move ptr.
            bp++;
            bytes_left--;

            // Process full messages on CR or LF
            if ( ch == '\n' || ch == '\r') {

                // Next wait for start of next message
                AD2_Parser_State = AD2_PARSER_SCANNING_START;

                // FIXME: Make it better faster less cpu mem etc. String :(
                // Consume all of the data for this message from the ring buffer.
                string msg = "";
                while (ring_qout_position != ring_qin_position) {
                    msg += (char)ring_buffer[ring_qout_position];
                    // Reset to 0 at the end of the ring.
                    if ( ++ring_qout_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
                        ring_qout_position = 0;
                    }
                }

                // call ON_RAW_MESSAGE callback if enabled.
                notifySubscribers(ON_RAW_MESSAGE, msg, nostate);

                // Detect message type or error.
                // 1) Starts with !
                //     !boot, !EXP, !REL, etc, etc.
                // 2) Starts with [
                //    Standard status message.
                // All other cases are invalid
                //
                if (msg[0] == '!') {
                    if (msg.find("!LRR:") == 0) {
                        // call ON_LRR callback if enabled.
                        notifySubscribers(ON_LRR, msg, nostate);
                    } else if (msg.find("!REL:") == 0 || msg.find("!EXP:") == 0) {
                        // call ON_EXPANDER_MESSAGE callback if enabled.
                        notifySubscribers(ON_EXP, msg, nostate);
                    } else if (msg.find("!RFX:") == 0) {
                        // call ON_RFX callback if enabled.
                        notifySubscribers(ON_RFX, msg, nostate);
                    } else if (msg.find("!AUI:") == 0) {
                        // call ON_AUI callback if enabled.
                        notifySubscribers(ON_AUI, msg, nostate);
                    } else if (msg.find("!KPM:") == 0) {
                        // FIXME: move parser below to function so it can be called here.
                        // call ON_KPM callback if enabled.
                        notifySubscribers(ON_KPM, msg, nostate);
                    } else if (msg.find("!KPE:") == 0) {
                        // call ON_KPE callback if enabled.
                        notifySubscribers(ON_KPE, msg, nostate);
                    } else if (msg.find("!CRC:") == 0) {
                        // call ON_CRC callback if enabled.
                        notifySubscribers(ON_CRC, msg, nostate);
                    } else if (msg.find("!VER:") == 0) {
                        // Parse the version string.
                        // call ON_VER callback if enabled.
                        notifySubscribers(ON_VER, msg, nostate);
                    } else if (msg.find("!ERR:") == 0) {
                        // call ON_ERR callback if enabled.
                        notifySubscribers(ON_ERR, msg, nostate);
                    }
                } else {
                    // http://www.alarmdecoder.com/wiki/index.php/Protocol#Keypad
                    if (msg[0] == '[') {

                        // Excessive sanity check. Test a few static characters.
                        // Length should be 94 bytes and end with ".
                        // [00110011000000003A--],010,[f70700000010808c18020000000000],"ARMED ***STAY** ZONE BYPASSED "
                        if (msg.length() == 94 && msg[93]=='"' && msg[22] == ',') {

                            // First extract the 32 bit address mask from section #3
                            // to use it as a storage key for the state.
                            uint32_t amask = strtol(msg.substr(AMASK_START, AMASK_END-AMASK_START).c_str(), nullptr, 16);

                            amask = AD2_NTOHL(amask);
                            // Ademco/DSC: MASK 00000000 = System
                            // Ademco 00000001 is keypad address 0
                            // Ademco 00000002 is keypad address 1
                            // DSC    00000002 is partition 1
                            // Ademco 40000000 is keypad address 30

                            // Create or return a pointer to our partition storage class.
                            AD2VirtualPartitionState *ad2ps = getAD2PState(&amask, true);

                            // we should not need to test the validity of ad2ps with update=true
                            // the function will return a value.

                            // store key internal for easy use.
                            ad2ps->address_mask_filter = amask;

                            // Update the partition state based upon the new status message.
                            // get the panel type first
                            ad2ps->panel_type = msg[PANEL_TYPE_BYTE];

                            // triggers
                            bool SEND_READY_CHANGE = false;
                            bool SEND_ARMED_CHANGE = false;
                            bool SEND_CHIME_CHANGE = false;

                            // state change tracking
                            bool ARMED_HOME = is_bit_set(ARMED_HOME_BYTE, msg.c_str());
                            bool ARMED_AWAY = is_bit_set(ARMED_AWAY_BYTE, msg.c_str());
                            bool PERIMETER_ONLY = is_bit_set(PERIMETERONLY_BYTE, msg.c_str());
                            bool ENTRY_DELAY = is_bit_set(ENTRYDELAY_BYTE, msg.c_str());
                            bool READY = is_bit_set(READY_BYTE, msg.c_str());
                            bool CHIME_ON = is_bit_set(CHIME_BYTE, msg.c_str());
                            bool EXIT_NOW = false;

                            // Get section #4 alpha message and upper case for later searching
                            string ALPHAMSG = msg.substr(SECTION_4_START,32);
                            transform(ALPHAMSG.begin(), ALPHAMSG.end(), ALPHAMSG.begin(), ::toupper);

                            // If we are armed we may be in exit mode
                            if (ARMED_HOME || ARMED_AWAY) {
                                switch (ad2ps->panel_type) {
                                case ADEMCO_PANEL:
                                    if( ALPHAMSG.find("CHECK") == string::npos &&
                                            ALPHAMSG.find("SYSTEM") == string::npos) {
                                        if( ALPHAMSG.find("MAY EXIT NOW") != string::npos ) {
                                            EXIT_NOW = true;
                                        }
                                        break;
                                    } else {
                                        // QUIRK: system etc message ignore state change restore current
                                        EXIT_NOW = ad2ps->exit_now;
                                    }
                                    break;
                                case DSC_PANEL:
                                    if (ALPHAMSG.find("QUICK EXIT") != string::npos ||
                                            ALPHAMSG.find("EXIT DELAY") != string::npos) {
                                        EXIT_NOW = true;
                                    }
                                    break;
                                default:
                                    break;
                                }
                            }

                            // if this is the first state update then send ARMED/READY states
                            if (ad2ps->unknown_state ) {
                                SEND_ARMED_CHANGE = true;
                                SEND_READY_CHANGE = true;
                                SEND_CHIME_CHANGE = true;
                                ad2ps->unknown_state = false;
                            }

                            // armed_state change send notification
                            if ( ad2ps->armed_home != ARMED_HOME ||
                                    ad2ps->armed_away != ARMED_AWAY) {
                                SEND_ARMED_CHANGE = true;
                            }

                            // entry_delay bit is expected to change only when the ARMED bit changes.
                            // But just in case watch for it to change
                            if ( ad2ps->entry_delay_off != ENTRY_DELAY ) {
                                SEND_READY_CHANGE = true;
                            }

                            // perimeter_only bit can change after the armed bit is set
                            // this will treat it like AWAY/Stay transition as an additional
                            // arming event.
                            if ( ad2ps->perimeter_only != PERIMETER_ONLY ) {
                                SEND_READY_CHANGE = true;
                            }

                            // ready state change
                            if ( ad2ps->ready != READY ) {
                                SEND_READY_CHANGE = true;
                            }

                            // exit_now state change send
                            if ( ad2ps->exit_now != EXIT_NOW ) {
                                SEND_READY_CHANGE = true;
                            }

                            // chime_on state change send
                            if ( ad2ps->chime_on != CHIME_ON ) {
                                SEND_CHIME_CHANGE = true;
                            }

                            // Save states for event tracked changes
                            ad2ps->armed_away = ARMED_AWAY;
                            ad2ps->armed_home = ARMED_HOME;
                            ad2ps->entry_delay_off = ENTRY_DELAY;
                            ad2ps->perimeter_only = PERIMETER_ONLY;
                            ad2ps->ready = READY;
                            ad2ps->exit_now = EXIT_NOW;
                            ad2ps->chime_on = CHIME_ON;

                            // Save states for non even tracked changes
                            ad2ps->backlight_on  = is_bit_set(BACKLIGHT_BYTE, msg.c_str());
                            ad2ps->programming_mode = is_bit_set(PROGMODE_BYTE, msg.c_str());
                            ad2ps->zone_bypassed = is_bit_set(BYPASS_BYTE, msg.c_str());
                            ad2ps->ac_power = is_bit_set(ACPOWER_BYTE, msg.c_str());
                            ad2ps->alarm_event_occurred = is_bit_set(ALARMSTICKY_BYTE, msg.c_str());
                            ad2ps->alarm_sounding = is_bit_set(ALARM_BYTE, msg.c_str());
                            ad2ps->battery_low = is_bit_set(LOWBATTERY_BYTE, msg.c_str());
                            ad2ps->fire_alarm = is_bit_set(FIRE_BYTE, msg.c_str());
                            ad2ps->system_issue = is_bit_set(SYSISSUE_BYTE, msg.c_str());
                            ad2ps->system_specific = is_bit_set(SYSSPECIFIC_BYTE, msg.c_str());
                            ad2ps->beeps = msg[BEEPMODE_BYTE];

                            // Extract the numeric value from section #2
                            ad2ps->last_numeric_message = strtol(msg.substr(SECTION_2_START, SECTION_2_START+3).c_str(), 0, 10);

                            // Extract the 32 char Alpha message from section #4.
                            ad2ps->last_alpha_message = msg.substr(SECTION_4_START, SECTION_4_START+32);

                            // Extract the cursor location and type from section #3
                            ad2ps->display_cursor_type = (uint8_t) strtol(msg.substr(CURSOR_TYPE_POS, CURSOR_TYPE_POS+2).c_str(), 0, 16);
                            ad2ps->display_cursor_location = (uint8_t) strtol(msg.substr(CURSOR_POS, CURSOR_POS+2).c_str(), 0, 16);


                            // Debugging / testing output
                            ESP_LOGD(TAG, "!DBG: SSIZE(%i) PID(%i) MASK(%08X) Ready(%i) Armed[Away(%i) Home(%i)] Bypassed(%i) Exit(%i)",
                                     AD2PStates.size(),ad2ps->partition,amask,ad2ps->ready,ad2ps->armed_away,ad2ps->armed_home,ad2ps->zone_bypassed,ad2ps->exit_now);

                            // Call ON_MESSAGE callback if enabled.
                            notifySubscribers(ON_MESSAGE, msg, ad2ps);

                            // Send event if ready state changed
                            if ( SEND_READY_CHANGE ) {
                                notifySubscribers(ON_READY_CHANGE, msg, ad2ps);
                            }

                            // Send armed/disarm event
                            if ( SEND_ARMED_CHANGE ) {
                                if ( ad2ps->armed_home || ad2ps->armed_away) {
                                    notifySubscribers(ON_ARM, msg, ad2ps);
                                } else {
                                    notifySubscribers(ON_DISARM, msg, ad2ps);
                                }
                            }

                            // Send event if chime_on state changed
                            if ( SEND_CHIME_CHANGE ) {
                                notifySubscribers(ON_CHIME_CHANGE, msg, ad2ps);
                            }

                        }
                    } else {
                        //TODO: Error statistics tracking
                        ESP_LOGE(TAG, "!ERR: BAD PROTOCOL PREFIX.");
                    }
                }

                // Do not save EOL into the ring. We are done for now.
                break;

            } // switch(AD2_Parser_State)

            // Still receiving a message.
            // Save this byte to our ring buffer and parse and keep waiting for EOL.
            ring_buffer[ring_qin_position] = ch;

            // Reset to 0 at the end of the ring
            if ( ++ring_qin_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
                ring_qin_position = 0;
            }

            // If next qin address is qout then loose a byte and report error.
            if (ring_qin_position == ring_qout_position) {
                ring_error_count++;
                if ( ++ring_qout_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
                    ring_qout_position = 0;
                }
            }

            break;
        }
    }

    return true;
}

/**
 * @brief FIXME test code
 */
void AlarmDecoderParser::test()
{
    for (int x = 0; x < 10000; x++) {
        AD2PStates[1] = new AD2VirtualPartitionState;
        AD2PStates[1]->ready = false;
        delete AD2PStates[1];
    }
}

/**
 * @brief parse AlarmDecoder section #1 protocol "bits".
 * each byte position has will contain [0, 1, -].
 *
 * @param [in]pos position to look at in buffer.
 * @param [in]bitStr char * to test
 *
 * @note Typical section # string '[00100001100000003A--]'
 *
 * @return int 0 or 1
 */
bool is_bit_set(int pos, const char * bitStr)
{
    int set;
    set = 0;

    if( bitStr[pos] == '0' || bitStr[pos] == BIT_UNDEFINED) {
        set = 0;
    }
    if( bitStr[pos] == BIT_ON ) {
        set = 1;
    }

    return set;
}
