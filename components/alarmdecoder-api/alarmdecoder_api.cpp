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
#if defined(IDF_VER)
#include "esp_log.h"
static const char *TAG = "AD2API";
#endif

#define ZONE_TIMEOUT 60
#define FIRE_TIMEOUT 30
#define BEEPS_TIMEOUT 30

// nostate
AD2PartitionState *nostate = nullptr;

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
#if defined(IDF_VER)
    ESP_LOGI(TAG, "updateVersion %s",arg);
#endif
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
 * @brief find value by name in query string config data
 *    param1=val1&param2=val2
 *
 * @arg [in]qry_string std::string * to scan for NV data.
 * @arg [in]key char * key to search for.
 * @arg [in]val std::string &. Result buffer for value.
 * @arg [in]val_size size of output buffer.
 *
 * @return int results. < -2 error, -1 not found, >= 0 result length
 *
 * @note val is cleared after param check.
 */
int AlarmDecoderParser::query_key_value_string(std::string  &qry_str, const char *key, std::string &val)
{

    /* Test parmeters. */
    if ( !qry_str.length() || key == NULL) {
        return -2;
    }

    /* Clear val first. */
    val = "";

    /* Init state machine args and get raw pointer to our query string. */
    int keylen = strlen(key);
    const char * qry_ptr = qry_str.c_str();

    /* Process until we reach null terminator on the query string. */
    while ( *qry_ptr ) {
        const char *tp = qry_ptr;
        int len = 0;
        static char chr;

        /* Scan the KEY looking for the next terminator. */
        while ( (chr = *tp) != 0 ) {
            if (chr == '=' || chr == '&') {
                break;
            }
            len++;
            tp++;
        }

        /* Test key for a match. */
        if ( len && len == keylen ) {
            if ( strncasecmp (key, qry_ptr, keylen) == 0 ) {

                /* move the index */
                len++;
                tp++;

                /* Test for null value. Still valid just return empty string. */
                if ( !chr || chr == '&' ) {
                    return val.length();
                }

                /* Save the value. */
                while ( ( chr = *tp ) != 0 ) {
                    if ( chr == '=' || chr == '&' ) {
                        break;
                    }
                    val += chr;
                    tp++;
                }
                return val.length();
            }
        }

        /* End of string and key not found. We are done. */
        if ( !chr ) {
            return -1;
        }

        /* Keep looking skip last terminator. */
        len++;

        /* Scan till we start the next set. */
        qry_ptr += len;
        while ( chr && chr != '&' ) {
            chr = *qry_ptr++;
        }

        /* End of string and not found. We are done. */
        if ( !chr ) {
            return -1;
        }
    }
    return -1;
}

/**
 * @brief Subscribe to a EVENT type.
 *
 * @param [in]ev ad2_event_t event TYPE
 *   ex. ON_ALPHA_MESSAGE
 * @param [in]fn Callback pointer function type AD2ParserCallback_sub_t.
 * @param [in]arg pointer to argument to pass to subscriber on event.
 */
void AlarmDecoderParser::subscribeTo(ad2_event_t ev, AD2SubScriber::AD2ParserCallback_sub_t fn, void *arg)
{
    subscribers_t& v = AD2Subscribers[ev];
    v.push_back(AD2SubScriber(fn, arg));
}

/**
 * @brief Subscribe to a RAW RX DATA events.
 *
 * @param [in]fn Callback pointer function type AD2ParserCallbackRawRXData_sub_t.
 * @param [in]arg pointer to argument to pass to subscriber on event.
 */
void AlarmDecoderParser::subscribeTo(AD2SubScriber::AD2ParserCallbackRawRXData_sub_t fn, void *arg)
{
    subscribers_t& v = AD2Subscribers[ON_RAW_RX_DATA];
    v.push_back(AD2SubScriber(fn, arg));
}

/**
 * @brief Sequentially call each subscriber function in the list.
 *
 * @param [in]data pointer to bytes from AD2*.
 * @param [in]len data buffer size.
 *
 */
void AlarmDecoderParser::notifyRawDataSubscribers(uint8_t *data, size_t len)
{
    // notify any direct subscribers to this event type(ON_RAW_RX_DATA).
    for ( subscribers_t::iterator i = AD2Subscribers[ON_RAW_RX_DATA].begin(); i != AD2Subscribers[ON_RAW_RX_DATA].end(); ++i ) {
        ((AD2SubScriber::AD2ParserCallbackRawRXData_sub_t)i->fn)(data, len, i->varg);
    }
}

/**
 * @brief Sequentially call each subscriber function in the list.
 *
 * @param [in]ev event class.
 * @param [in]msg message that generated event.
 * @param [in]pstate partition state
 *
 */
void AlarmDecoderParser::notifySubscribers(ad2_event_t ev, std::string &msg, AD2PartitionState *pstate)
{
    // Build a human readable string of the event and send
    // as the msg to notifySearchSubscribers with a message type of "EVENT".
    // notifySearchSubscribers will check all search subscribers and look
    // for any matches calling the search call back if a match is found.

    // convert event to human readable string and state OPEN/CLOSE/TROUBLE
    std::string emsg;
    if (event_str.find((int)ev) == event_str.end()) {
        emsg = "EVENT ID " + std::to_string(ev);
    } else {
        emsg = event_str[(int)ev];
    }

    // build a simple event string that can be used by search.
    switch ((int)ev) {
    case ON_DISARM:
        break;
    case ON_ARM:
        if (pstate) {
            if (pstate->armed_stay) {
                emsg += " STAY";
            }
            if (pstate->armed_away) {
                emsg += " AWAY";
            }
        }
        break;
    case ON_POWER_CHANGE:
        if(pstate) {
            if(pstate->ac_power) {
                emsg += " AC";
            } else {
                emsg += " BATTERY";
            }
        }
        break;
    case ON_READY_CHANGE:
        if(pstate) {
            if(!pstate->ready) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_ALARM_CHANGE:
        if(pstate) {
            if(pstate->alarm_sounding) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_FIRE_CHANGE:
        if(pstate) {
            if(pstate->fire_alarm) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_CHIME_CHANGE:
        if(pstate) {
            if(pstate->chime_on) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_EXIT_CHANGE:
        if(pstate) {
            if(pstate->exit_now) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_PROGRAMMING_CHANGE:
        if(pstate) {
            if(pstate->programming) {
                emsg += " ON";
            } else {
                emsg += " OFF";
            }
        }
        break;
    case ON_ZONE_CHANGE:
        // lookup the zone state.
        if(pstate) {
            if(pstate->zone_states[pstate->zone].state() == AD2_STATE_TROUBLE) {
                emsg += " TROUBLE ";
            } else if(pstate->zone_states[pstate->zone].state() == AD2_STATE_OPEN) {
                emsg += " OPEN ";
            } else if(pstate->zone_states[pstate->zone].state() == AD2_STATE_CLOSED) {
                emsg += " CLOSE ";
            }
            // zero pad 3 digit zone number string
            char zstr[4];
            snprintf(zstr, sizeof(zstr), "%03d", (int)pstate->zone);
            emsg += zstr;
        }
        break;
    default:
        emsg += " " + msg;
    }

    // save results of human readable event message.
    if (pstate) {
        pstate->last_event_message = emsg;
    }

    // notify any direct subscribers to this event type.
    for ( subscribers_t::iterator i = AD2Subscribers[ev].begin(); i != AD2Subscribers[ev].end(); ++i ) {
        ((AD2SubScriber::AD2ParserCallback_sub_t)i->fn)(&msg, pstate, i->varg);
    }

    // notify any search subscribers that are watching for the "EVENT" type. Provide
    // a human readable event description.
    // TODO: Document event messages
    notifySearchSubscribers(EVENT_MESSAGE_TYPE, emsg, pstate);
}

/**
 * @brief Subscribe to a message using a REGEX expression.
 *
 * @param [in]fn Callback pointer function type AD2ParserCallback_sub_t.
 * @param [in]regex_search regex search structure.
 * @param [in]arg pointer to argument to pass to subscriber on event.
 */
void AlarmDecoderParser::subscribeTo(AD2SubScriber::AD2ParserCallback_sub_t fn, AD2EventSearch *event_search)
{
    subscribers_t& v = AD2Subscribers[ON_SEARCH_MATCH];
    v.push_back(AD2SubScriber(fn, event_search));
}

/**
 * @brief Sequentially call each subscriber function in the list.
 *
 * @param [in]mt message type.
 * @param [in]msg message that generated event.
 * @param [in]s partition state. May be nullptr only valid for ALPHA messages.
 */
void AlarmDecoderParser::notifySearchSubscribers(ad2_message_t mt, std::string &msg, AD2PartitionState *pstate)
{
    for ( subscribers_t::iterator i = AD2Subscribers[ON_SEARCH_MATCH].begin(); i != AD2Subscribers[ON_SEARCH_MATCH].end(); ++i ) {
        if (i->varg) {
            AD2EventSearch *eSearch = (AD2EventSearch*)i->varg;
            bool done = false;

            // test reset time if set and restore state to default if true.
            // FIXME: For now only TRUE/FALSE no actual time tracked.
            if (eSearch->getResetTime()) {
                eSearch->setState(eSearch->getDefaultState());
            }

            int savedstate = eSearch->getState();
            std::string outformat;

            // Pre filter tests for message type.
            std::vector<ad2_message_t> *fmt = &eSearch->PRE_FILTER_MESAGE_TYPE;
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
            std::string pfregex = eSearch->PRE_FILTER_REGEX;
            /// only test if supplied.
            if (pfregex.length()) {
                std::smatch m;
                std::regex re(pfregex);
                const auto& tmsg = msg;
                std::match_results< std::string::const_iterator > mr;
                bool res = std::regex_search(tmsg, m, re);
                if (!res) {
                    // no match next subscriber.
                    continue;
                }
            }

            // Test CLOSED REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && eSearch->CLOSE_REGEX_LIST.size()) {
                for(auto &regexstr : eSearch->CLOSE_REGEX_LIST) {
                    try {
                        std::smatch m;
                        std::regex re(regexstr);
                        const auto& tmsg = msg;
                        std::match_results< std::string::const_iterator > mr;
                        bool res = std::regex_search(tmsg, m, re);
                        if (res) {
                            // regex match
                            eSearch->setState(AD2_STATE_CLOSED);
                            outformat = eSearch->CLOSE_OUTPUT_FORMAT;
                            // Clear last output results before we collect new.
                            eSearch->RESULT_GROUPS.clear();
                            // save the regex group results if any.
                            for(auto idx : m) {
                                eSearch->RESULT_GROUPS.push_back(idx);
                            }
                            done = true;
                            break;
                        }
                    } catch (std::exception const& e) { // catch (std::regex_error& e) {
#if defined(IDF_VER)
                        ESP_LOGE(TAG, "!ERR: regex error: '%s' '%s' '%s'", e.what(), regexstr.c_str(), msg.c_str());
#endif
                        done = true;
                        break;
                    }
                }
            }

            // Test OPEN REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && eSearch->OPEN_REGEX_LIST.size()) {
                for(auto &regexstr : eSearch->OPEN_REGEX_LIST) {
                    std::smatch m;
                    std::regex e(regexstr);
                    const auto& tmsg = msg;
                    bool res = std::regex_search(tmsg, m, e);
                    if (res) {
                        // regex match
                        eSearch->setState(AD2_STATE_OPEN);
                        outformat = eSearch->OPEN_OUTPUT_FORMAT;
                        // Clear last output results before we collect new.
                        eSearch->RESULT_GROUPS.clear();
                        // save the regex group results if any.
                        for(auto idx : m) {
                            eSearch->RESULT_GROUPS.push_back(idx);
                        }
                        done = true;
                        break;
                    }
                }
            }

            // Test TROUBLE REGEX list stop on first matching statement.
            /// only test if a list is supplied.
            if (!done && eSearch->TROUBLE_REGEX_LIST.size()) {
                for(auto &regexstr : eSearch->TROUBLE_REGEX_LIST) {
                    std::smatch m;
                    std::regex e(regexstr);
                    const auto& tmsg = msg;
                    std::match_results< std::string::const_iterator > mr;
                    bool res = std::regex_search(tmsg, m, e);
                    if (res) {
                        // regex match
                        outformat = eSearch->TROUBLE_OUTPUT_FORMAT;
                        eSearch->setState(AD2_STATE_TROUBLE);
                        // Clear last output results before we collect new.
                        eSearch->RESULT_GROUPS.clear();
                        // save the regex group results if any.
                        for(auto idx : m) {
                            eSearch->RESULT_GROUPS.push_back(idx);
                        }
                        done = true;
                        break;
                    }
                }
            }

            // Match found and state changed. Call the callback routine.
            if (savedstate != eSearch->getState()) {
                eSearch->last_message = msg;
                eSearch->out_message = outformat; //FIXME do the formatting macro magic stuff.
                ((AD2SubScriber::AD2ParserCallback_sub_t)i->fn)(&msg, pstate, i->varg);
            }

            // All done with this subscriber. Next.
        }
    }
}

/**
 * @brief Return a partition state structure by 8bit keypad address 0-31(Ademco) or partition # 1-8(DSC).
 * 0 is reserved for system partition.
 *
 * @param [in]address 8bit signed in partition or keypad address.
 * @param [in]update if true update mask adding new bits.
 *
 */
AD2PartitionState * AlarmDecoderParser::getAD2PState(int address, bool update)
{
    uint32_t amask = 1;
    amask <<= address;
    return getAD2PState(&amask, update);
}

/**
 * @brief Return a partition state structure by 32bit keypad/partition mask.
 * LSB is address 1 on Ademco & partition 1 on DSC.
 * Mask 0x00000000 is reserved for system partition state.
 *
 * @param [in]amask pointer to 32bit mask to search for.
 * @param [in]update if true update mask adding new bits.
 *
 */
AD2PartitionState * AlarmDecoderParser::getAD2PState(uint32_t *amask, bool update)
{
    // Create or return a pointer to our partition storage class.
    AD2PartitionState *ad2ps = nullptr;

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
            ad2ps = AD2PStates[*amask] = new AD2PartitionState;
            ad2ps->partition = AD2PStates.size();
            ad2ps->primary_address = 0;
#if defined(IDF_VER)
            ESP_LOGI(TAG, "AD2PStates[%08x] not found adding partition ID(%i)", *amask, ad2ps->partition);
#endif
        }

    } else {
        // Found. Grab the state class ptr.
        ad2ps = AD2PStates[*amask];
    }
    return ad2ps;
}


/**
 * @brief Return a alpha description of a zone state. Use the AD2ZoneAlpha string if found
 * or the standard 'ZONE XXX' template if not.
 *
 * @param [in]zone int value for the zone from 1-N
 * @param [out]alpha std::string * alpha for the zone.
 *
 */
bool AlarmDecoderParser::getZoneString(uint8_t zone, std::string &alpha)
{
    if (AD2ZoneAlpha.count(zone)) {
        alpha = AD2ZoneAlpha[zone];
        return true;
    } else {
        alpha = "ZONE ";
        // zero pad 3 digit zone number string
        char zstr[4];
        snprintf(zstr, sizeof(zstr), "%03d", (int)zone);
        alpha += zstr;
        return false;
    }
}

/**
 * @brief Set the alpha description of a zone.
 *
 * @param [in]zone int value for the zone from 1-N
 *
 * @return const char * alpha for the zone.
 *
 */
void AlarmDecoderParser::setZoneString(uint8_t zone, const char* alpha)
{
    AD2ZoneAlpha[zone] = alpha;
}

/**
 * @brief Return a type of a zone state. Use the AD2ZoneType string if found
 * or the standard 'motion' template if not.
 *
 * @param [in]zone int value for the zone from 1-N
 * @param [out]type std::string * type for the zone.
 *
 */
bool AlarmDecoderParser::getZoneType(uint8_t zone, std::string &type)
{
    if (AD2ZoneType.count(zone)) {
        type = AD2ZoneType[zone];
        return true;
    }
    return false;
}

/**
 * @brief Set the type of a zone.
 *
 * @param [in]zone int value for the zone from 1-N
 *
 * @return const char * type for the zone.
 *
 */
void AlarmDecoderParser::setZoneType(uint8_t zone, const char* type)
{
    AD2ZoneType[zone] = type;
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

    // call ON_RAW_RX_DATA callback if enabled.
    // For now this is done first but I may move it after parsing below.
    notifyRawDataSubscribers(buff, (size_t)len);

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

#if MONITOR_PARSER_TIMING && defined(IDF_VER)
                // monitor processing time.
                int64_t xStart, xEnd, xDifference;
                xStart = esp_timer_get_time();
#endif
                // state mask
                AD2PartitionState *ad2ps = nullptr;

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
                        // DSC Zone Tracking use EXP messages and convert to zones.
                        if (panel_type == 'D') {
                            uint8_t exp_addr = atoi(msg.substr(5,2).c_str());
                            uint8_t exp_chan = atoi(msg.substr(8,2).c_str());
                            uint8_t zone = (exp_addr * 8) + exp_chan;
                            uint8_t value = atoi(msg.substr(11,2).c_str());

                            // default to nostate object.
                            ad2ps = nostate;

                            // Find the state based upon the zone.
                            std::map<uint32_t, AD2PartitionState *>::iterator part_it = AD2PStates.begin();

                            // Look at each partition for a zone list match
                            // and send a notification for every matching partition.
                            bool _zone_found = false;
                            while (part_it != AD2PStates.end()) {
                                // If zone is in the known zone list then use this partition.
                                std::list<uint8_t> *zl = &part_it->second->zone_list;
                                if ( find (zl->begin(), zl->end(), zone) != zl->end()) {
                                    _zone_found = true;
                                    // Found a match. Get pointer to partition state that matches this zone
                                    ad2ps = part_it->second;
                                    // Update the zone state object No timeout needed for DSC
                                    ad2ps->zone_states[zone].state(value > 0 ? AD2_STATE_OPEN : AD2_STATE_CLOSED);
                                    // Set the effected zone for the partition state.
                                    ad2ps->zone = zone;
                                    // Send zone change notification with partition state if found
                                    notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                                    // Done. Zone can only be mapped to one partition.
                                    break;
                                }
                                part_it++;
                            }
                            // If not found then use default system partition for state storage.
                            if (!_zone_found) {
                                uint32_t amask = 0;
                                ad2ps = getAD2PState(&amask, true);
                                // Update the zone state object No timeout needed for DSC
                                ad2ps->zone_states[zone].state(value > 0 ? AD2_STATE_OPEN : AD2_STATE_CLOSED);
                                // Set the effected zone for the partition state.
                                ad2ps->zone = zone;
                                // Send zone change notification with partition state if found
                                notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                            }
                        }
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
                        // save the AlarmDecoder firmware version string if change.
                        std::string _new = msg.substr(5);
                        if ( _new.compare(ad2_version_string) != 0 ) {
                            // save new value
                            ad2_version_string = _new;
                            // call ON_VER callback if enabled.
                            MESSAGE_TYPE = VER_MESSAGE_TYPE;
                            notifySubscribers(ON_VER, msg, nostate);
                        }
                    } else if (msg.find("!ERR:") == 0) {
                        // call ON_ERR callback if enabled.
                        MESSAGE_TYPE = ERR_MESSAGE_TYPE;
                        notifySubscribers(ON_ERR, msg, nostate);
                    } else if (msg.find("!CONFIG>") == 0) {
                        // save the AlarmDecoder firmware configuration string if change.
                        std::string _new = msg.substr(8);
                        if ( _new.compare(ad2_config_string) != 0 ) {
                            // save new value
                            ad2_config_string = _new;
                            // Early update AlarmDecoder panel mode.
                            std::string mode;
                            if (query_key_value_string(ad2_config_string, "MODE", mode) >= 0 ) {
                                panel_type = mode[0];
                            }
                            // call ON_CFG callback if enabled.
                            MESSAGE_TYPE = CFG_MESSAGE_TYPE;
                            notifySubscribers(ON_CFG, msg, nostate);
                        }
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

                            // Convert to host order LSB is address 1 on Ademco & partition 1 on DSC
                            // 0x00000000 is reserved for system partition state.
                            amask = AD2_NTOHL(amask);

                            // Create or return a pointer to our partition storage class.
                            ad2ps = getAD2PState(&amask, true);

                            // track message event count
                            ad2ps->count++;

                            // we should not need to test the validity of ad2ps with update=true
                            // the function will return a value.

                            // store key internal for easy use.
                            ad2ps->address_mask_filter |= amask;

                            // Update the partition state based upon the new status message.
                            // get the panel type first
                            ad2ps->panel_type = msg[PANEL_TYPE_BYTE];

                            // Update the parser panel mode.
                            panel_type = ad2ps->panel_type;

                            // Numeric field section #2 used in logic.
                            std::string numeric_message = msg.substr(SECTION_2_START, 3);

                            // event triggers
                            bool SEND_FIRE_CHANGE  = false;
                            bool SEND_READY_CHANGE = false;
                            bool SEND_ARMED_CHANGE = false;
                            bool SEND_CHIME_CHANGE = false;
                            bool SEND_PROGRAMMING_CHANGE = false;
                            bool SEND_POWER_CHANGE = false;
                            bool SEND_BATTERY_CHANGE = false;
                            bool SEND_ALARM_CHANGE = false;
                            bool SEND_ZONE_BYPASSED_CHANGE = false;
                            bool SEND_EXIT_CHANGE = false;
                            bool SEND_BEEPS_CHANGE = false;

                            // state change tracking
                            bool ARMED_STAY = is_bit_set(ARMED_STAY_BYTE, msg.c_str());
                            bool ARMED_AWAY = is_bit_set(ARMED_AWAY_BYTE, msg.c_str());
                            bool PERIMETER_ONLY = is_bit_set(PERIMETERONLY_BYTE, msg.c_str());
                            bool ENTRY_DELAY = is_bit_set(ENTRYDELAY_BYTE, msg.c_str());
                            bool READY = is_bit_set(READY_BYTE, msg.c_str());
                            bool CHIME_ON = is_bit_set(CHIME_BYTE, msg.c_str());
                            uint8_t BEEPS = msg[BEEPMODE_BYTE] - '0';
                            bool PROGRAMMING = is_bit_set(PROGMODE_BYTE, msg.c_str());
                            bool FIRE_ALARM = is_bit_set(FIRE_BYTE, msg.c_str());
                            bool AC_POWER = is_bit_set(ACPOWER_BYTE, msg.c_str());
                            bool LOW_BATTERY = is_bit_set(LOWBATTERY_BYTE, msg.c_str());
                            bool ALARM_BELL = is_bit_set(ALARM_BYTE, msg.c_str());
                            bool ALARM_STICKY = is_bit_set(ALARMSTICKY_BYTE, msg.c_str());
                            bool ZONE_BYPASSED = is_bit_set(BYPASS_BYTE, msg.c_str());
                            uint8_t extra_sys_1 = (uint8_t) strtol(msg.substr(ADEMCO_EXTRA_SYSB1, 2).c_str(), 0, 16);
                            uint8_t extra_sys_2 = (uint8_t) strtol(msg.substr(ADEMCO_EXTRA_SYSB2, 2).c_str(), 0, 16);
                            uint8_t extra_sys_3 = (uint8_t) strtol(msg.substr(ADEMCO_EXTRA_SYSB3, 2).c_str(), 0, 16);
                            uint8_t extra_sys_4 = (uint8_t) strtol(msg.substr(ADEMCO_EXTRA_SYSB4, 2).c_str(), 0, 16);

                            // virtual bit restore current state by default.
                            bool EXIT_NOW = ad2ps->exit_now;

                            // Get section #4 alpha message and upper case for later searching
                            string ALPHAMSG = msg.substr(SECTION_4_START, 32);
                            transform(ALPHAMSG.begin(), ALPHAMSG.end(), ALPHAMSG.begin(), ::toupper);

                            // Ademco QUIRK system messages ignore some bits.
                            bool ADEMCO_SYS_MESSAGE = false;
                            if (ad2ps->panel_type == ADEMCO_PANEL) {
                                if(ALPHAMSG.find("SYSTEM") == 0) {
                                    ADEMCO_SYS_MESSAGE = true;
                                }

                                // Restore battery state only track when it is a system message.
                                // TODO: Can Zone battery state can be tracked for non system messages?
                                if (ADEMCO_SYS_MESSAGE) {
                                    // SM20210623 Vista 50PUL battery toggle quirk. Ignore battery messages
                                    // on system partition messages at mask 0x00000000.
                                    // Only trust it on partition messages. This needs more testing.
                                    if (ad2ps->address_mask_filter == 0x00000000) {
                                        LOW_BATTERY = ad2ps->battery_low;
                                    }
                                } else {
                                    // not system message so restore last state.
                                    LOW_BATTERY = ad2ps->battery_low;
                                }
                            }

                            // If we are armed we may be in exit mode
                            if (ARMED_STAY || ARMED_AWAY) {
                                switch (ad2ps->panel_type) {
                                // "ARMED ***STAY***                "
                                // "ARMED ***AWAY***You may exit now"
                                case ADEMCO_PANEL:
                                    if ( !ADEMCO_SYS_MESSAGE ) {
                                        if( ALPHAMSG.find("ARMED") == 0 ) {
                                            if( ALPHAMSG.find("MAY EXIT NOW") != string::npos ) {
                                                // on state change update and notify subscribers.
                                                if (!ad2ps->exit_now) {
                                                    // trigger notify subscribers.
                                                    EXIT_NOW = true;
                                                    SEND_EXIT_CHANGE = true;
                                                }
                                            } else {
                                                // on state change update and notify subscribers.
                                                if (ad2ps->exit_now) {
                                                    EXIT_NOW = false;
                                                    SEND_EXIT_CHANGE = true;
                                                }
                                            }
                                        }
                                        break;
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

                            // If this is the first state update then ONLY send READY state as a SYNC is is
                            // necessary to at minimum subscribe to READY to be sure to stay in sync at startup.
                            if ( ad2ps->unknown_state ) {
                                SEND_READY_CHANGE = true;
                                ad2ps->unknown_state = false;
                            } else {
                                // fire state set on message
                                // prevent bouncing of alarms from unexpected messages.
                                // only timeout or on_ready will clear a fire.
                                if ( FIRE_ALARM ) {
                                    // fire bit set. Extend timeout.
                                    ad2ps->fire_timeout = monotonicTime()+FIRE_TIMEOUT;
                                    if (!ad2ps->fire_alarm) {
                                        // trigger notify subscribers.
                                        SEND_FIRE_CHANGE = true;
                                    }
                                } else {
                                    // restore current fire bit and clear
                                    // on timeout.
                                    if (ad2ps->fire_alarm) {
                                        if (ad2ps->fire_timeout < monotonicTime()) {
                                            // Clear state. Fire timeout.
                                            FIRE_ALARM = false;
                                            SEND_FIRE_CHANGE = true;
                                            ad2ps->fire_timeout = 0;
                                        } else {
                                            // Restore state. Fire timer still active.
                                            FIRE_ALARM = true;
                                        }
                                    }
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

                                // programming state change send
                                if ( ad2ps->programming != PROGRAMMING ) {
                                    SEND_PROGRAMMING_CHANGE = true;
                                }

                                // ac_power state change send
                                if ( ad2ps->ac_power != AC_POWER ) {
                                    SEND_POWER_CHANGE = true;
                                }

                                // low battery state change send
                                if ( ad2ps->battery_low != LOW_BATTERY ) {
                                    SEND_BATTERY_CHANGE = true;
                                }

                                // ALARM_BELL state change send
                                if ( ad2ps->alarm_sounding != ALARM_BELL ) {
                                    // TODO: Test on DSC
                                    // skip messages with Alarm sticky bit off unless clearing the event
                                    if (ALARM_STICKY && !ALARM_BELL) {
                                        // restore current ignore change
                                        ALARM_BELL = ad2ps->alarm_sounding;
                                    } else {
                                        SEND_ALARM_CHANGE = true;
                                    }
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

                            // Beep state set on message
                            if ( BEEPS ) {
                                // if different than current state notify.
                                if ( ad2ps->beeps != BEEPS ) {
                                    SEND_BEEPS_CHANGE = true;
                                }
                                // set timeout.
                                ad2ps->beeps_timeout = monotonicTime()+BEEPS_TIMEOUT;
                            } else {
                                if ( ad2ps->beeps ) {
                                    // restore state
                                    BEEPS = ad2ps->beeps;
                                    if ( ad2ps->beeps_timeout < monotonicTime() ) {
                                        BEEPS = 0;
                                        SEND_BEEPS_CHANGE = true;
                                    }
                                }
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
                            ad2ps->programming = is_bit_set(PROGMODE_BYTE, msg.c_str());
                            ad2ps->alarm_event_occurred = is_bit_set(ALARMSTICKY_BYTE, msg.c_str());
                            ad2ps->system_issue = is_bit_set(SYSISSUE_BYTE, msg.c_str());
                            ad2ps->system_specific = (uint8_t) (msg[SYSSPECIFIC_BYTE] - '0') & 0xff;
                            ad2ps->beeps = BEEPS;

                            // Extract the numeric value from section #2 HEX & DEC mix keep as string.
                            ad2ps->last_numeric_message = numeric_message;

                            // Extract the 32 char Alpha message from section #4.
                            ad2ps->last_alpha_message = msg.substr(SECTION_4_START, 32);

                            // Extract the cursor location and type from section #3
                            ad2ps->display_cursor_type = (uint8_t) strtol(msg.substr(CURSOR_TYPE_POS, 2).c_str(), 0, 16);
                            ad2ps->display_cursor_location = (uint8_t) strtol(msg.substr(CURSOR_POS, 2).c_str(), 0, 16);


                            // Debugging / testing output
#if defined(IDF_VER)
                            ESP_LOGD(TAG, "!DBG: SSIZE(%i) PID(%i) MASK(%08X) Ready(%i) Armed[Away(%i) Stay(%i)] Bypassed(%i) Exit(%i)",
                                     AD2PStates.size(),ad2ps->partition,amask,ad2ps->ready,ad2ps->armed_away,ad2ps->armed_stay,ad2ps->zone_bypassed,ad2ps->exit_now);
#endif

                            // Call ON_ALPHA_MESSAGE callback if enabled.
                            notifySubscribers(ON_ALPHA_MESSAGE, msg, ad2ps);

                            // Send event if FIRE state changed
                            if ( SEND_FIRE_CHANGE ) {
                                notifySubscribers(ON_FIRE_CHANGE, msg, ad2ps);
                            }

                            // Send event if ready state changed
                            if ( SEND_READY_CHANGE ) {
                                notifySubscribers(ON_READY_CHANGE, msg, ad2ps);
                            }

                            // Update zone tracking if Ademco panel zone list report
                            if (ad2ps->panel_type == ADEMCO_PANEL && !ad2ps->programming) {
                                // Restore all faulted zones ON_READY.
                                if (SEND_READY_CHANGE && ad2ps->ready) {
                                    for (std::pair<uint8_t, AD2ZoneState> e : ad2ps->zone_states) {
                                        // If zone(e.first) is currently OPEN then CLOSE it and notify subscribers.
                                        if (ad2ps->zone_states[e.first].state() != AD2_STATE_CLOSED) {
                                            // Update the zone state object and set timeout
                                            ad2ps->zone_states[e.first].state(AD2_STATE_CLOSED, monotonicTime()+ZONE_TIMEOUT);
                                            // Set the effected zone for the partition state.
                                            ad2ps->zone = e.first;
                                            // Send zone change notification with partition state if found
                                            notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                                        }
                                    }
                                } else
                                    // _not_ ready _not_ special cases.
                                    if (!ADEMCO_SYS_MESSAGE // not system message
                                            && ad2ps->system_specific == 0
                                            && extra_sys_4 != 0xff // avoid special flag
                                            && !ad2ps->exit_now) { // not exit countdown

                                        // get the numeric section and use as a zone #.
                                        // convert base 16 to base 10 if needed.
                                        bool _ishex = std::count_if(ad2ps->last_numeric_message.begin(),
                                                                    ad2ps->last_numeric_message.end(),
                                        [](unsigned char c) {
                                            return std::isalpha(c);
                                        }) > 0;
                                        uint8_t _zone = 0;
                                        if (_ishex) {
                                            _zone = (uint8_t) strtol(ad2ps->last_numeric_message.c_str(), 0, 16);
                                        } else {
                                            _zone = (uint8_t) strtol(ad2ps->last_numeric_message.c_str(), 0, 10);
                                        }

                                        // Flag as system if HEX value.
                                        ad2ps->zone_states[_zone].is_system(_ishex);

                                        bool _send_event = false;

                                        // this message is part of the zone low battery report
                                        // [00000011000100000A--],023,[f70600ef1023004018020000000000],"LOBAT 23                        "
                                        if (ad2ps->battery_low) {
                                            // Update the low_battery object and set timeout
                                            if (ad2ps->zone_states[_zone].low_battery() == false) {
                                                _send_event = true;
                                            }
                                            ad2ps->zone_states[_zone].low_battery(monotonicTime()+ZONE_TIMEOUT);
                                        }

                                        // standard zone fault report.
                                        // [00000011000000000A--],002,[f70600ef1002000018020000000000],"FAULT 02                        "
                                        // alarm zone(alarm_event_occurred) report. Set for zone alarm report entry.
                                        // [00110001111000010A--],011,[f70200ff101110802b020000000000],"ALARM 11 GARAGE DOOR            "
                                        // check zone(system_issue) set for zone trouble report entry.
                                        // [00000401000000100A--],009,[f700001f1009040208020000000000],"CHECK 09                        "
                                        if (ad2ps->system_issue || ad2ps->alarm_event_occurred) {
                                            // Update the zone state object and set timeout
                                            if (ad2ps->zone_states[_zone].state() != AD2_STATE_TROUBLE) {
                                                _send_event = true;
                                            }
                                            ad2ps->zone_states[_zone].state(AD2_STATE_TROUBLE, monotonicTime()+ZONE_TIMEOUT);
                                        } else {
                                            // Update the zone state object and set timeout
                                            if (ad2ps->zone_states[_zone].state() != AD2_STATE_OPEN) {
                                                _send_event = true;
                                            }
                                            ad2ps->zone_states[_zone].state(AD2_STATE_OPEN, monotonicTime()+ZONE_TIMEOUT);
                                        }

                                        // Send event notification if needed.
                                        if (_send_event) {
                                            // Set the effected zone for the partition state.
                                            ad2ps->zone = _zone;
                                            // Send zone change notification with partition state if found
                                            notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                                        }

                                    }
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

                            // Send event if beeps state changed
                            if ( SEND_BEEPS_CHANGE ) {
                                notifySubscribers(ON_BEEPS_CHANGE, msg, ad2ps);
                            }

                            // Send event if programming state changed
                            if ( SEND_PROGRAMMING_CHANGE ) {
                                notifySubscribers(ON_PROGRAMMING_CHANGE, msg, ad2ps);
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

                            // Zone tracking timeouts.
                            // TODO: Add to external periodic call. If we dont get messages this wont run.
                            // Not a problem in most cases but in some cases such as DEDUPLICATE setting on the AD2*
                            // this could be a problem.
                            if (ad2ps->panel_type == ADEMCO_PANEL && !ad2ps->programming) {
                                checkZoneTimeout();
                            }
                        }
                    } else {
                        //TODO: Error statistics tracking
#if defined(IDF_VER)
                        ESP_LOGE(TAG, "!ERR: BAD PROTOCOL PREFIX. '%s'", msg.c_str());
#endif
                    }
                }

                // call Search callback subscribers if a match is found for this message type.
                notifySearchSubscribers(MESSAGE_TYPE, msg, ad2ps);

#if defined(MONITOR_PARSER_TIMING) && defined(IDF_VER)
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
 * @brief Check zones for timeouts and send notifications.
 */
void AlarmDecoderParser::checkZoneTimeout()
{
    AD2PartitionState *ad2ps = nullptr;
    // Find the state based upon the zone.
    std::map<uint32_t, AD2PartitionState *>::iterator part_it = AD2PStates.begin();

    // Look at each partition zone list for state timeouts.
    // and send a notification for every matching partition.
    while (part_it != AD2PStates.end()) {
        ad2ps = part_it->second;
        if (ad2ps) {
            std::string msg = "ZONE_CHECK";
            for (std::pair<uint8_t, AD2ZoneState> e : ad2ps->zone_states) {
                // If zone is OPEN and the reset time has expired restore and notify subscribers.
                if (ad2ps->zone_states[e.first].state() != AD2_STATE_CLOSED) {
                    unsigned long _reset_time = ad2ps->zone_states[e.first].state_reset_time();
                    if (_reset_time && _reset_time < monotonicTime()) {
                        ad2ps->zone_states[e.first].state(AD2_STATE_CLOSED);
                        ad2ps->zone = e.first;
                        notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                    }
                }
                // If zone Battery is faulted and reset time has expired restore and notify subscribers.
                if (ad2ps->zone_states[e.first].low_battery()) {
                    unsigned long _reset_time = ad2ps->zone_states[e.first].battery_reset_time();
                    if (_reset_time && _reset_time < monotonicTime()) {
                        ad2ps->zone_states[e.first].low_battery(false);
                        ad2ps->zone = e.first;
                        notifySubscribers(ON_ZONE_CHANGE, msg, ad2ps);
                    }
                }
            }
        }
        part_it++;
    }
}

/**
 * @brief FIXME test code
 */
void AlarmDecoderParser::test()
{
    for (int x = 0; x < 10000; x++) {
        AD2PStates[1] = new AD2PartitionState;
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
