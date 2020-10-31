/**
 *  @file    alarmdecoder_api.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/15/2020
 *  @version 1.0.3
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
AD2VirtualPartitionState *nostate = nullptr;

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
 * @brief build bit string from binary pointer
 *
 * @param [in]size size_t bytes to read
 * @param [in]ptr pointer to buffer of bytes to read
 *
 * @return std::string
 *
 */
std::string AlarmDecoderParser::bin_to_binsz(size_t const size, void const * const ptr)
{
    std::string out;
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            out += '0'+byte;
        }
    }
    return out;
}

/**
 * @brief build bit string from hex string
 *
 * @param [in]ptr pointer to buffer of bytes to read
 *
 * @return std::string
 *
 */
std::string AlarmDecoderParser::hex_to_binsz(void const * const ptr)
{
    std::string out;
    unsigned char *b = (unsigned char*) ptr;
    int i = 0, j, bitval;
    while (b[i]) {
        char c = b[i];
        int value = 0;
        if(c >= '0' && c <= '9') {
            value = (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            value = (10 + (c - 'A'));
        } else if (c >= 'a' && c <= 'f') {
            value = (10 + (c - 'a'));
        } else {
            // invalid
            value = 0x00;
        }
        for (j = 3; j >= 0; j--) {
            bitval = (value >> j) & 1;
            out += '0'+bitval;
        }
        i++;
    }
    return out;
}


/**
 * @brief Subscribe to a EVENT type.
 *
 * @param [in]ev ad2_event_t event TYPE
 *   ex. ON_MESSAGE
 * @param [in]fn Callback pointer function type AD2ParserCallback_sub_t.
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
 * @brief Subscribe to a message using a REGEX expression.
 *
 * @param [in]fn Callback pointer function type AD2ParserCallback_sub_t.
 * @param [in]regex_search regex search structure.
 * @param [in]arg pointer to argument to pass to subscriber on event.
 */
void AlarmDecoderParser::subscribeTo(AD2ParserCallback_sub_t fn, AD2EventSearch *event_search)
{
    subscribers_t& v = AD2Subscribers[ON_SEARCH_MATCH];
    v.push_back(AD2SubScriber(fn, event_search));
}

/**
 * @brief Sequentially call each subscriber function in the list.
 *
 * @param [in]mt message type.
 * @param [in]msg message that generated event.
 * @param [in]s virtual partition state. May be nullptr only valid for ALPHA messages.
 */
void AlarmDecoderParser::notifySearchSubscribers(ad2_message_t mt, std::string &msg, AD2VirtualPartitionState *pstate)
{
    for ( subscribers_t::iterator i = AD2Subscribers[ON_SEARCH_MATCH].begin(); i != AD2Subscribers[ON_SEARCH_MATCH].end(); ++i ) {
        if (i->eSearch) {
            bool done = false;
            int savedstate = i->eSearch->getState();
            std::string outformat;

            // Pre filter tests for message type.
            std::vector<ad2_message_t> *fmt = &i->eSearch->PRE_FILTER_MESAGE_TYPE;
            /// only test if a list is supplied.
            if (fmt->size()) {
                bool res = false;
                if(std::find(fmt->begin(), fmt->end(), mt)!=fmt->end()) {
                    // Found the item
                    res = true;
                }
                if (!res) {
                    // no match next subscriber.
                    continue;
                }
            }

            // Pre filter tests for message REGEX match.
            std::string pfregex = i->eSearch->PRE_FILTER_REGEX;
            /// only test if supplied.
            if (pfregex.length()) {
                std::smatch m;
                std::regex e(pfregex);
                const auto& tmsg = msg;
                std::match_results< std::string::const_iterator > mr;
                bool res = std::regex_search(tmsg, m, e);
                if (!res) {
                    // no match next subscriber.
                    continue;
                }
            }

            // Test CLOSED REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && i->eSearch->CLOSED_REGEX_LIST.size()) {
                for(auto &regexstr : i->eSearch->CLOSED_REGEX_LIST) {
                    std::smatch m;
                    std::regex e(regexstr);
                    const auto& tmsg = msg;
                    std::match_results< std::string::const_iterator > mr;
                    bool res = std::regex_search(tmsg, m, e);
                    if (res) {
                        // no match next subscriber.
                        i->eSearch->setState(AD2_STATE_CLOSED);
                        outformat = i->eSearch->CLOSED_OUTPUT_FORMAT;
                        // Clear last output results before we collect new.
                        i->eSearch->RESULT_GROUPS.clear();
                        // save the regex group results if any.
                        for(auto idx : m) {
                            i->eSearch->RESULT_GROUPS.push_back(idx);
                        }
                        done = true;
                        break;
                    }
                }
            }

            // Test OPEN REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && i->eSearch->OPEN_REGEX_LIST.size()) {
                for(auto &regexstr : i->eSearch->OPEN_REGEX_LIST) {
                    std::smatch m;
                    std::regex e(regexstr);
                    const auto& tmsg = msg;
                    bool res = std::regex_search(tmsg, m, e);
                    if (res) {
                        // no match next subscriber.
                        i->eSearch->setState(AD2_STATE_OPEN);
                        outformat = i->eSearch->OPEN_OUTPUT_FORMAT;
                        // Clear last output results before we collect new.
                        i->eSearch->RESULT_GROUPS.clear();
                        // save the regex group results if any.
                        for(auto idx : m) {
                            i->eSearch->RESULT_GROUPS.push_back(idx);
                        }
                        done = true;
                        break;
                    }
                }
            }

            // Test FAULT REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && i->eSearch->FAULT_REGEX_LIST.size()) {
                for(auto &regexstr : i->eSearch->FAULT_REGEX_LIST) {
                    std::smatch m;
                    std::regex e(regexstr);
                    const auto& tmsg = msg;
                    std::match_results< std::string::const_iterator > mr;
                    bool res = std::regex_search(tmsg, m, e);
                    if (res) {
                        // no match next subscriber.
                        outformat = i->eSearch->FAULT_OUTPUT_FORMAT;
                        i->eSearch->setState(AD2_STATE_FAULT);
                        // Clear last output results before we collect new.
                        i->eSearch->RESULT_GROUPS.clear();
                        // save the regex group results if any.
                        for(auto idx : m) {
                            i->eSearch->RESULT_GROUPS.push_back(idx);
                        }
                        done = true;
                        break;
                    }
                }
            }

            // Match found and state changed. Call the callback routine.
            if (savedstate != i->eSearch->getState()) {
                i->eSearch->last_message = msg;
                i->eSearch->out_message = outformat; //FIXME do the formatting macro magic stuff.
                (i->fn)(&msg, pstate, i->eSearch);
            }

            // All done with this subscriber. Next.
        }
    }
}

/**
 * @brief Return a partition state structure by 8bit keypad address 0-31(Ademco) or partition #(DSC).
 *
 * @param [in]address 8bit signed in partition or keypad address.
 * @param [in]update if true update mask adding new bits.
 *
 */
AD2VirtualPartitionState * AlarmDecoderParser::getAD2PState(int address, bool update)
{
    uint32_t amask = 1;
    amask <<= address;
    return getAD2PState(&amask, update);
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

#if MONITOR_PARSER_TIMING
                // monitor processing time.
                int64_t xStart, xEnd, xDifference;
                xStart = esp_timer_get_time();
#endif
                // state mask
                AD2VirtualPartitionState *ad2ps = nullptr;

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

                ad2_message_t MESSAGE_TYPE = UNKOWN_MESSAGE_TYPE;

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
                        MESSAGE_TYPE = LRR_MESSAGE_TYPE;
                        notifySubscribers(ON_LRR, msg, nostate);
                    } else if (msg.find("!REL:") == 0) {
                        // call ON_EXPANDER_MESSAGE callback if enabled.
                        MESSAGE_TYPE = REL_MESSAGE_TYPE;
                        notifySubscribers(ON_REL, msg, nostate);
                    } else if (msg.find("!EXP:") == 0) {
                        MESSAGE_TYPE = EXP_MESSAGE_TYPE;
                        notifySubscribers(ON_EXP, msg, nostate);
                    } else if (msg.find("!RFX:") == 0) {
                        MESSAGE_TYPE = RFX_MESSAGE_TYPE;
                        // Expand the HEX value to a bit string for easy pattern matching.
                        // RFX:012345,80 -> !RFX:012345,10000000
                        std::string save = msg;
                        std::regex rx( "!RFX:(.*),(.*)" );
                        std::match_results< std::string::const_iterator > mr;
                        bool res = std::regex_search( msg, mr, rx );
                        if (res && mr.size() == 3) {
                            std::string bits = hex_to_binsz(mr.str(2).c_str());
                            msg = "!RFX:" + mr.str(1) + "," + bits;
                        }
                        // call ON_RFX callback if enabled.
                        notifySubscribers(ON_RFX, msg, nostate);

                    } else if (msg.find("!AUI:") == 0) {
                        // call ON_AUI callback if enabled.
                        MESSAGE_TYPE = AUI_MESSAGE_TYPE;
                        notifySubscribers(ON_AUI, msg, nostate);
                    } else if (msg.find("!KPM:") == 0) {
                        // FIXME: move parser below to function so it can be called here.
                        // call ON_KPM callback if enabled.
                        MESSAGE_TYPE = KPM_MESSAGE_TYPE;
                        notifySubscribers(ON_KPM, msg, nostate);
                    } else if (msg.find("!KPE:") == 0) {
                        // call ON_KPE callback if enabled.
                        MESSAGE_TYPE = KPE_MESSAGE_TYPE;
                        notifySubscribers(ON_KPE, msg, nostate);
                    } else if (msg.find("!CRC:") == 0) {
                        // call ON_CRC callback if enabled.
                        MESSAGE_TYPE = CRC_MESSAGE_TYPE;
                        notifySubscribers(ON_CRC, msg, nostate);
                    } else if (msg.find("!VER:") == 0) {
                        // Parse the version string.
                        // call ON_VER callback if enabled.
                        MESSAGE_TYPE = VER_MESSAGE_TYPE;
                        notifySubscribers(ON_VER, msg, nostate);
                    } else if (msg.find("!ERR:") == 0) {
                        // call ON_ERR callback if enabled.
                        MESSAGE_TYPE = ERR_MESSAGE_TYPE;
                        notifySubscribers(ON_ERR, msg, nostate);
                    }
                } else {
                    // http://www.alarmdecoder.com/wiki/index.php/Protocol#Keypad
                    if (msg[0] == '[') {
                        MESSAGE_TYPE = ALPHA_MESSAGE_TYPE;
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
                            ad2ps = getAD2PState(&amask, true);

                            // we should not need to test the validity of ad2ps with update=true
                            // the function will return a value.

                            // store key internal for easy use.
                            ad2ps->address_mask_filter = amask;

                            // Update the partition state based upon the new status message.
                            // get the panel type first
                            ad2ps->panel_type = msg[PANEL_TYPE_BYTE];

                            // triggers
                            bool SEND_FIRE_CHANGE  = false;
                            bool SEND_READY_CHANGE = false;
                            bool SEND_ARMED_CHANGE = false;
                            bool SEND_CHIME_CHANGE = false;
                            bool SEND_POWER_CHANGE = false;
                            bool SEND_BATTERY_CHANGE = false;
                            bool SEND_ALARM_CHANGE = false;
                            bool SEND_ZONE_BYPASSED_CHANGE = false;
                            bool SEND_EXIT_CHANGE = false;

                            // state change tracking
                            bool ARMED_STAY = is_bit_set(ARMED_STAY_BYTE, msg.c_str());
                            bool ARMED_AWAY = is_bit_set(ARMED_AWAY_BYTE, msg.c_str());
                            bool PERIMETER_ONLY = is_bit_set(PERIMETERONLY_BYTE, msg.c_str());
                            bool ENTRY_DELAY = is_bit_set(ENTRYDELAY_BYTE, msg.c_str());
                            bool READY = is_bit_set(READY_BYTE, msg.c_str());
                            bool CHIME_ON = is_bit_set(CHIME_BYTE, msg.c_str());
                            bool EXIT_NOW = false;
                            bool FIRE_ALARM = is_bit_set(FIRE_BYTE, msg.c_str());
                            bool AC_POWER = is_bit_set(ACPOWER_BYTE, msg.c_str());
                            bool LOW_BATTERY = is_bit_set(LOWBATTERY_BYTE, msg.c_str());
                            bool ALARM_BELL = is_bit_set(ALARM_BYTE, msg.c_str());
                            bool ZONE_BYPASSED = is_bit_set(BYPASS_BYTE, msg.c_str());

                            // Get section #4 alpha message and upper case for later searching
                            string ALPHAMSG = msg.substr(SECTION_4_START,32);
                            transform(ALPHAMSG.begin(), ALPHAMSG.end(), ALPHAMSG.begin(), ::toupper);

                            // Ademco QUIRK system messages ignore some bits.
                            bool ADEMCO_SYS_MESSAGE = false;
                            if (ad2ps->panel_type == ADEMCO_PANEL) {
                                if( ALPHAMSG.find("CHECK") != string::npos ||
                                        ALPHAMSG.find("SYSTEM") != string::npos) {
                                    ADEMCO_SYS_MESSAGE = true;
                                }

                                if ( ADEMCO_SYS_MESSAGE ) {
                                    // Skip fire bit restore to current so we dont trip an event.
                                    FIRE_ALARM = ad2ps->fire_alarm;
                                }
                            }

                            // If we are armed we may be in exit mode
                            if (ARMED_STAY || ARMED_AWAY) {
                                switch (ad2ps->panel_type) {
                                case ADEMCO_PANEL:
                                    if ( !ADEMCO_SYS_MESSAGE ) {
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
                            if ( ad2ps->unknown_state ) {
                                SEND_FIRE_CHANGE = true;
                                SEND_READY_CHANGE = true;
                                SEND_ARMED_CHANGE = true;
                                SEND_CHIME_CHANGE = true;
                                SEND_POWER_CHANGE = true;
                                SEND_BATTERY_CHANGE = true;
                                SEND_ALARM_CHANGE = true;
                                SEND_ZONE_BYPASSED_CHANGE = true;
                                SEND_EXIT_CHANGE = true;
                                ad2ps->unknown_state = false;
                            } else {
                                // fire state change
                                if ( ad2ps->fire_alarm != FIRE_ALARM ) {
                                    SEND_FIRE_CHANGE = true;
                                }

                                // ready state change
                                if ( ad2ps->ready != READY ) {
                                    SEND_READY_CHANGE = true;
                                }

                                // armed_state change send notification
                                if ( ad2ps->armed_stay != ARMED_STAY ||
                                        ad2ps->armed_away != ARMED_AWAY) {
                                    SEND_ARMED_CHANGE = true;
                                }

                                // chime_on state change send
                                if ( ad2ps->chime_on != CHIME_ON ) {
                                    SEND_CHIME_CHANGE = true;
                                }

                                // ac_power state change send
                                if ( ad2ps->ac_power != AC_POWER ) {
                                    SEND_POWER_CHANGE = true;
                                }

                                // ac_power state change send
                                if ( ad2ps->battery_low != LOW_BATTERY ) {
                                    SEND_BATTERY_CHANGE = true;
                                }

                                // ALARM_BELL state change send
                                if ( ad2ps->alarm_sounding != ALARM_BELL ) {
                                    SEND_ALARM_CHANGE = true;
                                }

                                // BYPASSED state change
                                if ( ad2ps->zone_bypassed != ZONE_BYPASSED) {
                                    SEND_ZONE_BYPASSED_CHANGE = true;
                                }
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

                            // exit_now state change send
                            if ( ad2ps->exit_now != EXIT_NOW ) {
                                SEND_EXIT_CHANGE = true;
                            }

                            // Save states for event tracked changes
                            ad2ps->armed_away = ARMED_AWAY;
                            ad2ps->armed_stay = ARMED_STAY;
                            ad2ps->entry_delay_off = ENTRY_DELAY;
                            ad2ps->perimeter_only = PERIMETER_ONLY;
                            ad2ps->ready = READY;
                            ad2ps->exit_now = EXIT_NOW;
                            ad2ps->chime_on = CHIME_ON;
                            ad2ps->fire_alarm = FIRE_ALARM;
                            ad2ps->ac_power = AC_POWER;
                            ad2ps->battery_low = LOW_BATTERY;
                            ad2ps->alarm_sounding = ALARM_BELL;
                            ad2ps->zone_bypassed = ZONE_BYPASSED;

                            // Save states for non even tracked changes
                            ad2ps->backlight_on  = is_bit_set(BACKLIGHT_BYTE, msg.c_str());
                            ad2ps->programming_mode = is_bit_set(PROGMODE_BYTE, msg.c_str());
                            ad2ps->alarm_event_occurred = is_bit_set(ALARMSTICKY_BYTE, msg.c_str());
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
                            ESP_LOGD(TAG, "!DBG: SSIZE(%i) PID(%i) MASK(%08X) Ready(%i) Armed[Away(%i) Stay(%i)] Bypassed(%i) Exit(%i)",
                                     AD2PStates.size(),ad2ps->partition,amask,ad2ps->ready,ad2ps->armed_away,ad2ps->armed_stay,ad2ps->zone_bypassed,ad2ps->exit_now);

                            // Call ON_MESSAGE callback if enabled.
                            notifySubscribers(ON_MESSAGE, msg, ad2ps);

                            // Send event if FIRE state changed
                            if ( SEND_FIRE_CHANGE ) {
                                notifySubscribers(ON_FIRE, msg, ad2ps);
                            }

                            // Send event if ready state changed
                            if ( SEND_READY_CHANGE ) {
                                notifySubscribers(ON_READY_CHANGE, msg, ad2ps);
                            }

                            // Send armed/disarm event
                            if ( SEND_ARMED_CHANGE ) {
                                if ( ad2ps->armed_stay || ad2ps->armed_away) {
                                    notifySubscribers(ON_ARM, msg, ad2ps);
                                } else {
                                    notifySubscribers(ON_DISARM, msg, ad2ps);
                                }
                            }

                            // Send event if chime_on state changed
                            if ( SEND_CHIME_CHANGE ) {
                                notifySubscribers(ON_CHIME_CHANGE, msg, ad2ps);
                            }

                            // Send event if ac_power state changed
                            if ( SEND_POWER_CHANGE ) {
                                notifySubscribers(ON_POWER_CHANGE, msg, ad2ps);
                            }

                            // Send event if battery_low state changed
                            if ( SEND_BATTERY_CHANGE ) {
                                notifySubscribers(ON_LOW_BATTERY, msg, ad2ps);
                            }

                            // Send event if alarm_sounding state changed
                            if ( SEND_ALARM_CHANGE ) {
                                notifySubscribers(ON_ALARM_CHANGE, msg, ad2ps);
                            }

                            // Send event if zone_bypassed state changed
                            if ( SEND_ZONE_BYPASSED_CHANGE ) {
                                notifySubscribers(ON_ZONE_BYPASSED_CHANGE, msg, ad2ps);
                            }

                            // Send event if EXIT state changed
                            if ( SEND_EXIT_CHANGE ) {
                                notifySubscribers(ON_EXIT_CHANGE, msg, ad2ps);
                            }

                        }
                    } else {
                        //TODO: Error statistics tracking
                        ESP_LOGE(TAG, "!ERR: BAD PROTOCOL PREFIX.");
                    }
                }

                // call Search callback subscribers if a match is found.
                notifySearchSubscribers(MESSAGE_TYPE, msg, ad2ps);

#ifdef MONITOR_PARSER_TIMING
                xEnd = esp_timer_get_time();
                xDifference = xEnd - xStart;
                ESP_LOGI(TAG, "message processing time: %lldus", xDifference );
#endif
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
