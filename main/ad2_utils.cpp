/**
 *  @file    ad2_utils.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief AD2IOT common utils shared between main and components
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
#include "freertos/task.h"

static const char *TAG = "AD2UTIL";

// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// esp component includes
#include "driver/uart.h"

// specific includes
#include "ad2_utils.h"

// esp includes
#include "nvs_flash.h"
#include "mbedtls/base64.h"

#include <SimpleIni.h>
// ini config class
static CSimpleIniA _ad2ini;

// auto save and cache states.
static bool _config_autosave = false;
static bool _config_dirty = false;
static bool _uSD_config = false;

/**
 * @brief  ini file error string helper
 *
 */
const char * _ini_file_error(SI_Error e)
{
    if (e == SI_NOMEM) {
        return "out of memory.";
    }
    return (const char *)strerror(errno);
}

/**
 * @brief  ad2_save_persistent_config
 *
 */
void ad2_save_persistent_config()
{
    if (!_config_autosave && _config_dirty) {
        SI_Error rc;
        if (_uSD_config) {
            rc = _ad2ini.SaveFile("/" AD2_USD_MOUNT_POINT AD2_CONFIG_FILE);
        } else {
            rc = _ad2ini.SaveFile("/" AD2_SPIFFS_MOUNT_POINT AD2_CONFIG_FILE);
        }
        if (rc < 0) {
            ESP_LOGE(TAG, "%s: Error (%i) ini saveFile.", __func__, rc);
        } else {
            _config_dirty = false;
        }
    }
}

/**
 * @brief  ad2_load_persistent_config
 *
 */
void ad2_load_persistent_config()
{
    // Set config storage format to UTF-8.
    _ad2ini.SetUnicode();

    // Enable multi line values.
    _ad2ini.SetMultiLine();

    // See if a config exists on the uSD card and use if found.
    ad2_printf_host(true, "%s: Attempting to load config file: " AD2_USD_MOUNT_POINT AD2_CONFIG_FILE, TAG);
    SI_Error rc = _ad2ini.LoadFile("/" AD2_USD_MOUNT_POINT AD2_CONFIG_FILE);

    if (rc < 0) {
        // USD config load failed show why.
        ad2_printf_host( false, " failed(");
        ad2_printf_host( false, _ini_file_error(rc));
        ad2_printf_host( false, ")");

        // See if a config exists on the SPIFFS and use if found.
        ad2_printf_host(true, "%s: Attempting to load config file: " AD2_SPIFFS_MOUNT_POINT AD2_CONFIG_FILE, TAG);
        // load from internal SPIFFS storage
        rc = _ad2ini.LoadFile("/" AD2_SPIFFS_MOUNT_POINT AD2_CONFIG_FILE);
        if (rc < 0) {
            // USD config load failed show why.
            ad2_printf_host( false, " failed(");
            ad2_printf_host( false, _ini_file_error(rc));
            ad2_printf_host( false, ")");

            // last option create a new one with factory reset and reboot.
            hal_factory_reset();

        } else {
            ad2_printf_host(false, " success.");
        }
    } else {
        ad2_printf_host(false, " success.");
        _uSD_config = true;
    }
}

/**
 * @brief Parse an acl string and add to our list of networks.
 *
 * @details Split ACL string on ',' then test if each section is a CIDR '/XX', range '-'
 * or single IP. Calculate the appropriate uint32_t Start and End address values and append it to
 * allowed_networks vector.
 *
 * Split on '/' for CIDR '-' for IP range or just a single IP.
 *   Example ACL string: "192.168.0.0/24, 192.168.1.1-192.168.1.3, 192.168.2.1"
 *
 * @arg [in]acl std::string * Pointer to std::string with ACL list.
 *
 * @return int results.   0 ad2_acl_check.ACL_FORMAT_OK
 *                       -1 ad2_acl_check.ACL_ERR_BADFORMAT_CIDR
 *                       -2 ad2_acl_check.ACL_ERR_BADFORMAT_IP
 */
int ad2_acl_check::add(std::string &acl)
{
    bool is_ipv4;

    std::vector<std::string> tokens;
    // Remove all white space.
    ad2_remove_ws(acl);

    // Split on commas.
    // A, B, C
    ad2_tokenize(acl, ",", tokens);
    for (auto &token : tokens) {

        /// Test for CIDR notation '\' 192.168.0.0/24
        if (token.find('/') != std::string::npos) {
            // convert string after '/' to a small int 0-255 sufficient to hold a CIDR value 1-128
            // Allow for /0 match all special case 0.0.0.0/0
            uint8_t iCIDR = std::atoi(token.substr(token.find("/") + 1).c_str());
            if (iCIDR > 128) {
                return this->ACL_ERR_BADFORMAT_CIDR;
            }

            // Load IP address string into addr. If it is IPv4 pad left bits with 1's
            std::string ip = token.substr(0, token.find("/"));
            ad2_addr addr = {};
            if (!this->_szIPaddrParse(ip, addr, is_ipv4)) {
                return ACL_ERR_BADFORMAT_IP;
            }

            // CIDR to 128bit mask.
            // The address and mask is in Network Byte Order(Big-endian)
            ad2_addr mask = {};
            memset((uint8_t *)&mask.u8_addr[0], 0xff, sizeof(((struct ad2_addr *)0)->u8_addr));
            this->_addr_SHIFT_LEFT(mask, (is_ipv4 ? 32 : 128) - iCIDR);

            // 128bit Start address prefill with 1's
            ad2_addr addr_start = {};
            memset((uint8_t *)&addr_start.u8_addr[0], 0xff, sizeof(((struct ad2_addr *)0)->u8_addr));

            // 128 bit End address prefill with 0's
            ad2_addr addr_end = {};
            memset((uint8_t *)&addr_end.u8_addr[0], 0x0, sizeof(((struct ad2_addr *)0)->u8_addr));

            // 32 bit version (start = addr & mask; end = addr | (~mask))
            this->_addr_AND(addr_start, addr, mask);
            this->_addr_NOT(mask);
            this->_addr_OR(addr_end, addr, mask);

            // Save the allowed ACL range.
            allowed_networks.push_back({addr_start, addr_end});
        } else {

            // Test for RANGE notation '-' 192.168.0.10-192.168.0.100'
            if (token.find('-') != std::string::npos) {
                // Start address
                std::string szstart = token.substr(0, token.find("-"));
                ad2_addr saddr = {};
                if (!this->_szIPaddrParse(szstart, saddr, is_ipv4)) {
                    return this->ACL_ERR_BADFORMAT_IP;
                }
                // End address
                std::string szend = token.substr(token.find("-") + 1);
                ad2_addr eaddr = {};
                if (!this->_szIPaddrParse(szend, eaddr, is_ipv4)) {
                    return this->ACL_ERR_BADFORMAT_IP;
                }
                // Currently start needs to be less than end.
                // FIXME if (saddr <= eaddr) {
                allowed_networks.push_back({saddr, eaddr});
            } else {

                // Single IP just use it for the Start and End..
                ad2_addr addr = {};
                if (!this->_szIPaddrParse(token, addr, is_ipv4)) {
                    return this->ACL_ERR_BADFORMAT_IP;
                }
                allowed_networks.push_back({addr, addr});
            }
        }
    }
    return this->ACL_FORMAT_OK;
}

// @brief test if an IP string is inside of any of the know network ranges.
bool ad2_acl_check::find(std::string szaddr)
{
    bool is_ipv4;

    // If no ACLs exist then skip ACL testing and everything passes.
    if (!allowed_networks.size()) {
        return true;
    }
    ad2_remove_ws(szaddr);
    // storage for IPv4 or IPv6
    ad2_addr addr = {};
    if (!this->_szIPaddrParse(szaddr, addr, is_ipv4)) {
        return false;
    }
    return find(addr);
}

// @brief test if an IP value is inside of any of the know network ranges.
// Addresses are 16 byte or 4 32bit words. For IPv4 then only one word is used.
bool ad2_acl_check::find(ad2_addr &addr)
{
    // If no ACLs exist then skip ACL testing and everything passes.
    if (!allowed_networks.size()) {
        return true;
    }
    for (auto acl : allowed_networks) {
        if (_in_addr_BETWEEN(addr, acl.first, acl.second)) {
            return true;
        }
    }
    return false;
}

// @brief parse IPv4/IPv6 address string to ip6_addr value.
// @return bool ok = true;
bool ad2_acl_check::_szIPaddrParse(std::string &szaddr, ad2_addr &addr, bool &is_ipv4)
{
    is_ipv4 = false;

    // IPv4 has '.'
    if (szaddr.find('.') != std::string::npos) {
        is_ipv4 = true;
        // prefix all bits with 1's ::FFFF: to keep a 32 bit IPv4 in a 128bit IPv6 capable container.
        memset((uint8_t *)&addr.u8_addr[0], 0xff, sizeof(((struct ad2_addr *)0)->u8_addr));
        // 32bit IPv4 stored in the last 4 bytes.
        if (!inet_pton(AF_INET, szaddr.c_str(), &addr.u8_addr[12])) {
            // Allow all IPv4 pattern 0.0.0.0/0
            if (szaddr.compare("0.0.0.0/0") != 0) {
                // reject anything that does not parse or match allow all.
                return false;
            }
        }
#if CONFIG_LWIP_IPV6
    } else
        // IPv6 has ':'
        if (szaddr.find(':') != std::string::npos) {
            if (!inet_pton(AF_INET6, szaddr.c_str(), &addr.u8_addr[0])) {
                // reject anything that does not parse.
                return false;
            }
#endif
        } else {
            // reject anything that does not parse.
            return false;
        }
    return true;
};

// @brief IPv6 or IPv4 math.
bool ad2_acl_check::_in_addr_BETWEEN(ad2_addr &test, ad2_addr &start, ad2_addr &end)
{
    for (int n = 0; n < sizeof(((struct ad2_addr *)0)->u8_addr); n++) {
        if (test.u8_addr[n] < start.u8_addr[n] || test.u8_addr[n] > end.u8_addr[n]) {
            return false;
        }
    }
    return true;
}

// @brief shift 128bit big endian value to the left cnt times.
void ad2_acl_check::_addr_SHIFT_LEFT(ad2_addr &addr, int cnt)
{
    int carry_bit = 0, last_carry_bit = 0;

    while (cnt > 0) {
        for (int i = sizeof(((struct ad2_addr *)0)->u8_addr) - 1; i >= 0; i--) {
            // save top bit for carry
            carry_bit = addr.u8_addr[i] & 0x80 ? 0x01 : 0x00;
            addr.u8_addr[i] <<= 1;
            if (i < sizeof(((struct ad2_addr *)0)->u8_addr) - 1) {
                // carry top bit to next int.
                addr.u8_addr[i] |= last_carry_bit;
            }
            last_carry_bit = carry_bit;
        }
        cnt--;
    }
}

// @brief NOT 128bit big endian value.
void ad2_acl_check::_addr_NOT(ad2_addr &addr)
{
    for (int i = sizeof(((struct ad2_addr *)0)->u8_addr) - 1; i >= 0; i--) {
        addr.u8_addr[i] = ~addr.u8_addr[i];
    }
}

// @brief AND 128bit big endian values.
void ad2_acl_check::_addr_AND(ad2_addr &out, ad2_addr &inA, ad2_addr &inB)
{
    for (int i = sizeof(((struct ad2_addr *)0)->u8_addr) - 1; i >= 0; i--) {
        out.u8_addr[i] = inA.u8_addr[i] & inB.u8_addr[i];
    }
}

// @brief OR 128bit big endian values.
void ad2_acl_check::_addr_OR(ad2_addr &out, ad2_addr &inA, ad2_addr &inB)
{
    for (int i = sizeof(((struct ad2_addr *)0)->u8_addr) - 1; i >= 0; i--) {
        out.u8_addr[i] = inA.u8_addr[i] | inB.u8_addr[i];
    }
}

/**
 * @brief build auth string from user and pass.
 *
 * @arg [in]user std::string * to user.
 * @arg [in]password std::string * to password.
 *
 * @return std::string basic auth string
 *
 */
std::string ad2_make_basic_auth_string(const std::string &user, const std::string &password)
{

    size_t toencodeLen = user.length() + password.length() + 2;
    size_t out_len = 0;
    char toencode[toencodeLen];
    unsigned char outbuffer[(toencodeLen + 2 - ((toencodeLen + 2) % 3)) / 3 * 4 + 1];

    memset(toencode, 0, toencodeLen);

    snprintf(
        toencode,
        toencodeLen,
        "%s:%s",
        user.c_str(),
        password.c_str());

    mbedtls_base64_encode(outbuffer, sizeof(outbuffer), &out_len, (unsigned char *)toencode, toencodeLen - 1);
    outbuffer[out_len] = '\0';

    std::string encoded_string = std::string((char *)outbuffer);
    return encoded_string;
}

/**
 * @brief url encode a string making it safe for http protocols.
 *
 * @arg [in]str std::string to url encode.
 *
 * @return std::string url encoded string
 *
 */
std::string ad2_urlencode(const std::string str)
{
    std::string encoded = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str[i];
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

/**
 * @brief Generate a UUID based upon the ESP32 wifi hardware mac address.
 *
 * @arg [in]uint32_t Sub ID.
 * @arg [in]ret std::string& to the result.
 *
 * UUID
 *  Length fixed 36 characters
 *  format args
 *   AD2EMBEDXYYYYYY
 *   X: app specific ID
 *   Y: unique 32 bit value from WIFI MAC address.
 *
 */
#define AD2IOT_UUID_FORMAT "41443245-4d42-4544-44%02x-%02x%02x%02x%02x%02x%02x"

void ad2_genUUID(uint8_t n, std::string &ret)
{
    static uint8_t chipid[6] = {0, 0, 0, 0, 0, 0};
    // assuming first byte wont never be 0x00
    if (chipid[0] == 0x00) {
        esp_read_mac(chipid, ESP_MAC_WIFI_STA);
    }

    char _uuid[37];
    snprintf(_uuid, sizeof(_uuid), AD2IOT_UUID_FORMAT,
             (uint16_t)((n)&0xff),
             (uint16_t)((chipid[0]) & 0xff),
             (uint16_t)((chipid[1]) & 0xff),
             (uint16_t)((chipid[2]) & 0xff),
             (uint16_t)((chipid[3]) & 0xff),
             (uint16_t)((chipid[4]) & 0xff),
             (uint16_t)((chipid[5]) & 0xff));
    ret = _uuid;
}

/**
 * @brief conver bytes in a std::string to upper case.
 *
 * @param [in]str std::string & to modify
 */
void ad2_ucase(std::string &str)
{
    for (std::string::size_type i = 0; i < str.length(); ++i) {
        str[i] = std::toupper(str[i]);
    }
}

/**
 * @brief conver bytes in a std::string to lower case.
 *
 * @param [in]str std::string & to modify
 */
void ad2_lcase(std::string &str)
{
    for (std::string::size_type i = 0; i < str.length(); ++i) {
        str[i] = std::tolower(str[i]);
    }
}

/**
 * @brief split string to vector on token
 *
 * @param [in]str std::string input string
 * @param [in]delim char * delimeters ex. ", " comma and space.
 * @param [in]out pointer to output std::vector of std:strings
 *
 */
void ad2_tokenize(std::string const &str, const char *delimiters,
                  std::vector<std::string> &out)
{
    char *_str = strdup(str.c_str());
    char *token = std::strtok(_str, delimiters);
    while (token) {
        out.push_back(std::string(token));
        token = std::strtok(nullptr, delimiters);
    }
    free(_str);
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]size size_t buffer limits.
 * @param [in]va_list variable args list.
 *
 * @return new std::string
 */
std::string ad2_string_vasnprintf(const char *fmt, size_t size, va_list args)
{
    size_t len = size + 1; // Add room for null
    std::string out = "";
    char temp[len];
    vsnprintf(temp, len, fmt, args);
    out = temp;
    return out;
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]va_list variable args list.
 *
 * @return new std::string
 */
std::string ad2_string_vaprintf(const char *fmt, va_list args)
{
    std::string out = "";
    int len = vsnprintf(NULL, 0, fmt, args);
    if (len < 0) {
        return out;
    }
    len++; // account for null after tests.
    char temp[len];
    len = vsnprintf(temp, len, fmt, args);

    if (len) {
        out = temp;
    }

    return out;
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]... variable args.
 *
 * @return new std::string
 */
std::string ad2_string_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string out = ad2_string_vaprintf(fmt, args);
    va_end(args);
    return out;
}

/**
 * @brief replace all occurrences of a string in another string.
 *
 * @param [in]inStr std::string source.
 * @param [in]findStr std::string & what to look for.
 * @param [in]replaceStr std::string & what to change it to.
 *
 * @return bool true|false
 *
 */
bool ad2_replace_all(std::string &inStr, const char *findStr, const char *replaceStr)
{
    int findLen = strlen(findStr);
    if (!findLen) {
        return false;
    }

    size_t i = inStr.find(findStr);
    if (i == std::string::npos) {
        return false;
    }

    int replaceLen = strlen(replaceStr);

    while (i != std::string::npos) {
        inStr.replace(i, findLen, replaceStr);
        i = inStr.find(findStr, replaceLen + i);
    }

    return true;
}

/**
 * @brief left trim.
 *
 * @param [in]s std::string &.
 */
void ad2_ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
}

/**
 * @brief right trim.
 *
 * @param [in]s std::string &.
 */
void ad2_rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace)))
            .base(),
            s.end());
}

/**
 * @brief all trim.
 *
 * @param [in]s std::string &.
 */
void ad2_trim(std::string &s)
{
    ad2_ltrim(s);
    ad2_rtrim(s);
}

/**
 * @brief Remove all white space from a string.
 *
 * @param [in]s std::string &.
 */
void ad2_remove_ws(std::string &s)
{
    std::string str_no_ws;
    for (char c : s)
        if (!std::isspace(c)) {
            str_no_ws += c;
        }
    s = str_no_ws;
}

/**
 * @brief Get bool configuration value by section and key.
 *  Optional default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[out] vout bool* to store result.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 *
 */
void ad2_get_config_key_bool(
    const char *section, const char *key,
    bool *vout,
    int index, const char *suffix)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }

    // If config from uSD is loaded then use it else use NVM
    *vout = _ad2ini.GetBoolValue(section, tkey.c_str(), *vout);
#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: s(%s) k(%s) v(%i)", __func__, section, tkey.c_str(), *vout);
#endif
}

/**
 * @brief Set bool configuration value by section and key.
 *  Optional remove, default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[in] vin bool value to store.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 * @param[in] remove=false remove entry.
 *
 */
void ad2_set_config_key_bool(
    const char *section, const char *key,
    bool vin,
    int index, const char *suffix, bool remove)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }
    // If config from persistent storage
#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: Set|Delete bool key(%s)", __func__, tkey.c_str());
#endif
    bool done;
    if (remove) {
        if (_ad2ini.KeyExists(section, tkey.c_str())) {
            done = _ad2ini.Delete(section, tkey.c_str(), false);
        } else {
            // asked to delete but does not exist. Still consider done.
            done = true;
        }
    } else {
        done = _ad2ini.SetBoolValue(section, tkey.c_str(), vin);
    }
    if (!done) {
        ESP_LOGE(TAG, "%s: fail ini Set|Delete(%s).", __func__, tkey.c_str());
    } else {
        _config_dirty = true;
    }
    if (_config_autosave && _config_dirty) {
        SI_Error rc = _ad2ini.SaveFile("/" AD2_USD_MOUNT_POINT AD2_CONFIG_FILE);
        if (rc < 0) {
            ESP_LOGE(TAG, "%s: Error (%i) ini saveFile.", __func__, rc);
        }
    }
}

/**
 * @brief Get int configuration value by section and key.
 *  Optional default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[out] vout int* to store result.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 *
 */
void ad2_get_config_key_int(
    const char *section, const char *key,
    int *vout,
    int index, const char *suffix)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }

    *vout = (int)_ad2ini.GetLongValue(section, tkey.c_str(), *vout);
#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: s(%s) k(%s) v(%i)", __func__, section, tkey.c_str(), *vout);
#endif
}

/**
 * @brief Set int configuration value by section and key.
 *  Optional remove, default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[in] vin int value to store.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 * @param[in] remove=false remove entry.
 *
 */
void ad2_set_config_key_int(
    const char *section, const char *key,
    int vin,
    int index, const char *suffix, bool remove)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }

#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: Set|Delete bool key(%s)", __func__, tkey.c_str());
#endif
    bool done;
    if (remove) {
        if (_ad2ini.KeyExists(section, tkey.c_str())) {
            done = _ad2ini.Delete(section, tkey.c_str(), false);
        } else {
            // asked to delete but does not exist. Still consider done.
            done = true;
        }
    } else {
        done = _ad2ini.SetLongValue(section, tkey.c_str(), vin);
    }
    if (!done) {
        ESP_LOGE(TAG, "%s: fail ini Set|Delete(%s).", __func__, tkey.c_str());
    } else {
        _config_dirty = true;
    }
    if (_config_autosave && _config_dirty) {
        SI_Error rc = _ad2ini.SaveFile("/" AD2_USD_MOUNT_POINT AD2_CONFIG_FILE);
        if (rc < 0) {
            ESP_LOGE(TAG, "%s: Error (%i) ini saveFile.", __func__, rc);
        }
    }
}

/**
 * @brief Get std::string configuration value by section and key.
 *  Optional default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[out] vout std::string& to store result.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 *
 */
void ad2_get_config_key_string(
    const char *section, const char *key,
    std::string &vout,
    int index, const char *suffix)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }

    vout = _ad2ini.GetValue(section, tkey.c_str(), vout.c_str());
#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: s(%s) k(%s) v(%s)", __func__, section, tkey.c_str(), vout.c_str());
#endif

}

/**
 * @brief Set std::string configuration value by section and key.
 *  Optional remove, default value, index(0-999), and suffix helpers.
 *
 * @param[in] section config section.
 * @param[in] key config key in section.
 * @param[in] vin const char * value to store.
 * @param[in] index=-1 index of key when multiple exist. Use -1 to disable.
 * @param[in] suffix=NULL char * string to add to the key if not NULL.
 * @param[in] remove=false remove entry.
 *
 */
void ad2_set_config_key_string(
    const char *section, const char *key,
    const char *vin,
    int index, const char *suffix, bool remove)
{
    std::string tkey = (key == nullptr ? "" : key);
    if (index > -1) {
        if (tkey.length()) {
            tkey += " ";
        }
        tkey += std::to_string(index);
        if (suffix != nullptr) {
            tkey += " ";
            tkey += suffix;
        }
    }

#ifdef DEBUG_CONFIG
    ESP_LOGI(TAG, "%s: Set|Delete bool key(%s)", __func__, tkey.c_str());
#endif
    bool done;
    if (remove) {
        if (_ad2ini.KeyExists(section, tkey.c_str())) {
            done = _ad2ini.Delete(section, tkey.c_str(), false);
        } else {
            // asked to delete but does not exist. Still consider done.
            done = true;
        }
    } else {
        done = _ad2ini.SetValue(section, tkey.c_str(), vin);
    }
    if (!done) {
        ESP_LOGE(TAG, "%s: fail ini Set|Delete(%s).", __func__, tkey.c_str());
    } else {
        _config_dirty = true;
    }
    if (_config_autosave && _config_dirty) {
        SI_Error rc = _ad2ini.SaveFile("/" AD2_USD_MOUNT_POINT AD2_CONFIG_FILE);
        if (rc < 0) {
            ESP_LOGE(TAG, "%s: Error (%i) ini saveFile.", __func__, rc);
        }
    }
    return;
}

/**
 * @brief Copy the Nth space separated word from a string.
 *
 * @param dest pointer to std::string for output
 * @param [in]src pointer to input bytes
 * @param [in]n argument number to return if possible
 * @param [in]remaining Default false. If true will consume remaining string as the final result.
 *
 * @return 0 on success -1 on failure
 */
int ad2_copy_nth_arg(std::string &dest, const char *src, int n, bool remaining)
{
    int start = 0, end = -1;
    int i = 0, word_index = 0;
    int len;

    for (i = 0; src[i] != '\0'; i++) {
        if ((src[i] == ' ') && (src[i + 1] != ' ') && (src[i + 1] != '\0')) {
            // start check
            word_index++;
            if (word_index == n) {
                start = i + 1;
            }
        } else if ((src[i] != ' ') && ((src[i + 1] == ' ') || (src[i + 1] == '\0'))) {
            // end check
            if (word_index == n) {
                if (remaining) {
                    // Fast forward to last char
                    end = strlen(src) - 1;
                } else {
                    end = i;
                }
                break;
            }
        }
    }

    if (end <= -1) {
        ESP_LOGD(TAG, "%s: Fail to find %dth arg", __func__, n);
        return -1;
    }

    len = end - start + 1;

    dest = std::string(&src[start], len);
    return 0;
}

/**
 * @brief Update the AD2IoT AD2* config string.
 *
 * @details FIXME TODO Update the AD2IoT config string that is kept
 * in sync with the AD2* attached to the AD2IoT device.
 *
 * @param [in]arg const char * args
 *
 */
void ad2_config_update(const char *arg)
{

}

/**
 * @brief Update the firmware for the attached AD2* device
 *
 * @details Preform an update using /sdcard/ad2firmware.hex
 * or OTA using firmware URL
 *
 * @param [in]arg const char * args
 *
 */
void ad2_fw_update(const char *arg)
{

}

/**
 * @brief Send the ARM AWAY command to the alarm panel.
 *
 * @details using the given code and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]code std::string &
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_arm_away(std::string &code, int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "2");
        } else if (s->panel_type == DSC_PANEL) {
            msg = ad2_string_printf("K%01i1<S5>", address);
        }

        ESP_LOGI(TAG, "Sending ARM AWAY command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the ARM AWAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_arm_away(int codeId, int partId)
{

    // Get user code
    std::string code;
    ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, NULL, code, codeId);

    ad2_arm_away(code, partId);
}

/**
 * @brief Send the ARM STAY command to the alarm panel.
 *
 * @details using the given code and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]code std::string &
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_arm_stay(std::string &code, int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "3");
        } else if (s->panel_type == DSC_PANEL) {
            msg = ad2_string_printf("K%01i1<S4>", address);
        }
        ESP_LOGI(TAG, "Sending ARM STAY command to address %i using code '%s'", address, code.c_str());
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the ARM STAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_arm_stay(int codeId, int partId)
{
    // Get user code
    std::string code;
    ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, NULL, code, codeId);

    ad2_arm_stay(code, partId);
}

/**
 * @brief Send the DISARM command to the alarm panel.
 *
 * @details using a given code and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]code std::string &
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_disarm(std::string &code, int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "1");
        } else if (s->panel_type == DSC_PANEL) {
            // QUIRK: For DSC don't disarm if already disarmed. Unlike Ademoc no specific command AFAIK exists to disarm just the code. If I find one I will change this.
            if (s->armed_away || s->armed_stay) {
                msg = ad2_string_printf("K%01i1%s", address, code.c_str());
            } else {
                ESP_LOGI(TAG, "DSC: Already DISARMED not sending DISARM command");
            }
        }
        ESP_LOGI(TAG, "Sending DISARM command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the DISARM command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_disarm(int codeId, int partId)
{
    // Get user code
    std::string code;
    ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, NULL, code, codeId);

    ad2_disarm(code, partId);
}

/**
 * @brief Toggle Chime mode.
 *
 * @details using a given code and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]code std:string &
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_chime_toggle(std::string &code, int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "9");
        } else if (s->panel_type == DSC_PANEL) {
            msg = ad2_string_printf("K%01i1<S6>", address);
        }

        ESP_LOGI(TAG, "Sending CHIME toggle command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Toggle Chime mode.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_chime_toggle(int codeId, int partId)
{

    // Get user code
    std::string code;
    ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, NULL, code, codeId);

    ad2_chime_toggle(code, partId);
}

/**
 * @brief Send the FIRE PANIC command to the alarm panel.
 *
 * @details using the address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_fire_alarm(int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        msg = ad2_string_printf("K%02i<S1>", address);

        ESP_LOGI(TAG, "Sending FIRE PANIC button command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the PANIC command to the alarm panel.
 *
 * @details using the address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_panic_alarm(int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        msg = ad2_string_printf("K%02i<S2>", address);

        ESP_LOGI(TAG, "Sending PANIC button command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the AUX(medical) PANIC command to the alarm panel.
 *
 * @details using the address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_aux_alarm(int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        msg = ad2_string_printf("K%02i<S3>", address);

        ESP_LOGI(TAG, "Sending AUX PANIC button command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the EXIT now command to the alarm panel.
 *
 * @details using the address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 *
 */
void ad2_exit_now(int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s", address, "*");
        } else if (s->panel_type == DSC_PANEL) {
            msg = ad2_string_printf("K%01i1<S8>", address);
        }

        ESP_LOGI(TAG, "Sending EXIT NOW command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send a zone bypass command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]code std::string &
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 * @param [in]zone uint8_t[1-255] zone number
 *
 * FIXME: larger panels have 3 digit zones. Detect?
 *
 */
void ad2_bypass_zone(std::string &code, int partId, uint8_t zone)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    // @brief get the partition state
    AD2PartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        std::string msg;
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s6%02i*", address, code.c_str(), zone);
        } else if (s->panel_type == DSC_PANEL) {
            msg = ad2_string_printf("K%01i1*1%02i#", address, zone);
        }

        ESP_LOGI(TAG, "Sending BYPASS ZONE command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send a zone bypass command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot partId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]partId int [0 - AD2_MAX_PARTITION]
 * @param [in]zone uint8_t[1-255] zone number
 *
 * FIXME: larger panels have 3 digit zones. Detect?
 *
 */
void ad2_bypass_zone(int codeId, int partId, uint8_t zone)
{
    // Get user code
    std::string code;
    ad2_get_config_key_string(AD2CODES_CONFIG_SECTION, NULL, code, codeId);

    ad2_bypass_zone(code, partId, zone);
}

/**
 * @brief Send string to the AD2 devices after macro translation.
 *
 * @param [in]buf Pointer to string to send to AD2 devices.
 *
 * @note Macros <SX> for sending panel specific special keys.
 *       http://www.alarmdecoder.com/wiki/index.php/Protocol#Special_Keys
 * This makes it more simple to send complex sequences with a simple human
 * readable macro.
 */
void ad2_send(std::string &buf)
{
    if (g_ad2_client_handle > -1) {

        /* replace macros <S1>-<S8> with real values */
        for (int x = 1; x < 9; x++) {
            std::string key = ad2_string_printf("<S%01i>", x);
            std::string out;
            out.append(3, (char)x);
            ad2_replace_all(buf, key.c_str(), out.c_str());
        }

        ESP_LOGD(TAG, "sending '%s' to AD2*", buf.c_str());

        if (g_ad2_mode == 'C') {
            uart_write_bytes((uart_port_t)g_ad2_client_handle, buf.c_str(), buf.length());
        } else if (g_ad2_mode == 'S') {
            // the handle is a socket fd use send()
            send(g_ad2_client_handle, buf.c_str(), buf.length(), 0);
        } else {
            ESP_LOGE(TAG, "invalid ad2 connection mode");
        }
    } else {
        ESP_LOGE(TAG, "invalid handle in send_to_ad2");
        return;
    }
}

/**
 * @brief Format and send bytes to the host uart with !XXX prefix to be AD2 protcol compatible.
 *
 * @param [in]format const char * format string.
 * @param [in]... variable args
 *
 * @return result
 */
// Track if the terminal line is busy
static bool line_clear = false;
int ad2_log_vprintf_host(const char *fmt, va_list args)
{
    // wait 500ms for access to the console.
    if (!ad2_take_host_console((void *)xTaskGetCurrentTaskHandle(), AD2_CONSOLE_LOCK_TIME)) {
        return 0;
    }

    // calculate size and make buffer
    int len = vsnprintf(NULL, 0, fmt, args);
    if (len) {
        char *tbuf = nullptr;
        tbuf = (char *)malloc(len + 1);
        len = vsnprintf(tbuf, len + 1, fmt, args);
        if (len) {
            // don't log blank lines or send out \r\n.
            tbuf[strcspn(tbuf, "\r\n")] = 0;
            len = strlen(tbuf);
            if (!len) {
                if (!line_clear) {
                    line_clear = false;
                }
                goto clean_up;
            }
            if (!line_clear) {
                // check if continuation of log. Must start with "[I,W,E,D] ("
                if ((len > 3) && (strchr("IWED", tbuf[0]) != NULL) && (tbuf[1] == ' ' && tbuf[2] == '(')) {
                    uart_write_bytes(UART_NUM_0, "\r\n", 2);
                    uart_write_bytes(UART_NUM_0, AD2PFX, sizeof(AD2PFX) - 1);
                }
            }
            int pos = 0;
            char ch;
            while (pos < len) {
                ch = tbuf[pos];
                if (ch == '\n') {
                    line_clear = true;
                    uart_write_bytes(UART_NUM_0, "\r\n", 2);
                } else {
                    // avoid nulls and non binary data
                    if (ch > 31 && ch < 127) {
                        if (line_clear) {
                            line_clear = false;
                            uart_write_bytes(UART_NUM_0, AD2PFX, sizeof(AD2PFX) - 1);
                        }
                        uart_write_bytes(UART_NUM_0, &ch, 1);
                    }
                }
                pos++;
            }
        }
clean_up:
        if (tbuf) {
            free(tbuf);
        }
    }

    // release the console
    ad2_give_host_console((void *)xTaskGetCurrentTaskHandle());

    return len;
}

/**
 * @brief Format and send bytes to the host uart.
 * @param [in]bool prefix
 * @param [in]format const char * format string.
 * @param [in]... variable args
 */
void ad2_printf_host(bool prefix, const char *fmt, ...)
{
    // wait 500ms for access to the console.
    if (!ad2_take_host_console((void *)xTaskGetCurrentTaskHandle(), AD2_CONSOLE_LOCK_TIME)) {
        return;
    }
    if (prefix) {
        if (!line_clear) {
            // move to the next line.
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
        }
        line_clear = false;
        // write prefix
        std::string pfx = AD2PFX;
        pfx += "N (";
        pfx += std::to_string(esp_log_timestamp());
        pfx += ") ";
        uart_write_bytes(UART_NUM_0, pfx.c_str(), pfx.length());
    }

    va_list args;
    va_start(args, fmt);
    std::string out = ad2_string_vaprintf(fmt, args);
    va_end(args);
    uart_write_bytes(UART_NUM_0, out.c_str(), out.length());
    // release the console
    ad2_give_host_console((void *)xTaskGetCurrentTaskHandle());
}

/**
 * @brief Format and send bytes to the host uart with sized buffer.
 *
 * @param [in]format const char * format string.
 * @param [in]size size_t buffer size limiter.
 * @param [in]... variable args
 */
void ad2_snprintf_host(const char *fmt, size_t size, ...)
{
    // wait 500ms for access to the console.
    if (!ad2_take_host_console((void *)xTaskGetCurrentTaskHandle(), AD2_CONSOLE_LOCK_TIME)) {
        return;
    }

    va_list args;
    va_start(args, size);
    std::string out = ad2_string_vasnprintf(fmt, size, args);
    va_end(args);
    uart_write_bytes(UART_NUM_0, out.c_str(), out.length());
    // release the console
    ad2_give_host_console((void *)xTaskGetCurrentTaskHandle());
}

/**
 * @brief Get partition state by partitition ID
 *
 * @param [in]partId Address slot for address to use for
 * returning partition info. The AlarmDecoderParser class tracks
 * every message and parses each into a status by partition.
 * Each state is stored by an address mask. To fetch the state of
 * a partition all that is needed is an address that is known to
 * be on that partition. For DSC panels the address is the partition.
 *
 */
AD2PartitionState *ad2_get_partition_state(int partId)
{
    // Get the address/partition mask for multi partition support.
    int address = -1;
    std::string _section = std::string(AD2PART_CONFIG_SECTION " ") + std::to_string(partId);
    ad2_get_config_key_int(_section.c_str(), PART_CONFIG_ADDRESS, &address);

    AD2PartitionState *s = nullptr;

    // if we found a config record then initialize the AD2PState for the mask.
    if (address != -1) {
        s = AD2Parse.getAD2PState(address, false);
    }
    return s;
}

/**
 * @brief Generate a standardized JSON string for the AD2IoT device details.
 *
 * @return cJSON*
 *
 */
cJSON *ad2_get_ad2iot_device_info_json()
{
    cJSON *root = cJSON_CreateObject();

    // Add this boards info to the object
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddNumberToObject(root, "cpu_model", chip_info.model);
    cJSON_AddNumberToObject(root, "cpu_revision", chip_info.revision);
    cJSON_AddNumberToObject(root, "cpu_cores", chip_info.cores);
    cJSON *cjson_cpu_features = cJSON_CreateArray();
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString("WiFi"));
    }
    if (chip_info.features & CHIP_FEATURE_BLE) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString("BLE"));
    }
    if (chip_info.features & CHIP_FEATURE_BT) {
        cJSON_AddItemToArray(cjson_cpu_features, cJSON_CreateString("BT"));
    }
    cJSON_AddItemToObject(root, "cpu_features", cjson_cpu_features);
    cJSON_AddNumberToObject(root, "cpu_flash_size", spi_flash_get_chip_size());
    std::string flash_type = (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external";
    cJSON_AddStringToObject(root, "cpu_flash_type", flash_type.c_str());

    cJSON_AddStringToObject(root, "ad2_version_string", AD2Parse.ad2_version_string.c_str());
    cJSON_AddStringToObject(root, "ad2_config_string", AD2Parse.ad2_config_string.c_str());

    return root;
}

/**
 * @brief Generate a standardized JSON string for the given AD2PartitionState pointer.
 *
 * @param [in]AD2PartitionState * to use for json object.
 *
 * @return cJSON*
 *
 */
cJSON *ad2_get_partition_state_json(AD2PartitionState *s)
{
    cJSON *root = cJSON_CreateObject();
    if (s && !s->unknown_state) {
        cJSON_AddBoolToObject(root, "ready", s->ready);
        cJSON_AddBoolToObject(root, "armed_away", s->armed_away);
        cJSON_AddBoolToObject(root, "armed_stay", s->armed_stay);
        cJSON_AddBoolToObject(root, "backlight_on", s->backlight_on);
        cJSON_AddBoolToObject(root, "programming", s->programming);
        cJSON_AddBoolToObject(root, "zone_bypassed", s->zone_bypassed);
        cJSON_AddBoolToObject(root, "ac_power", s->ac_power);
        cJSON_AddBoolToObject(root, "chime_on", s->chime_on);
        cJSON_AddBoolToObject(root, "alarm_event_occurred", s->alarm_event_occurred);
        cJSON_AddBoolToObject(root, "alarm_sounding", s->alarm_sounding);
        cJSON_AddBoolToObject(root, "battery_low", s->battery_low);
        cJSON_AddBoolToObject(root, "entry_delay_off", s->entry_delay_off);
        cJSON_AddBoolToObject(root, "fire_alarm", s->fire_alarm);
        cJSON_AddBoolToObject(root, "system_issue", s->system_issue);
        cJSON_AddBoolToObject(root, "perimeter_only", s->perimeter_only);
        cJSON_AddBoolToObject(root, "exit_now", s->exit_now);
        cJSON_AddNumberToObject(root, "system_specific", s->system_specific);
        cJSON_AddNumberToObject(root, "beeps", s->beeps);
        cJSON_AddStringToObject(root, "panel_type", std::string(1, s->panel_type).c_str());
        cJSON_AddStringToObject(root, "last_alpha_message", s->last_alpha_message.c_str());
        cJSON_AddStringToObject(root, "last_numeric_messages", s->last_numeric_message.c_str()); // Can have HEX digits ex. 'FC'.
        cJSON_AddNumberToObject(root, "mask", s->address_mask_filter);
    } else {
        cJSON_AddStringToObject(root, "last_alpha_message", "Unknown");
    }
    return root;
}

/**
 * @brief Generate a standardized JSON string for s->zone_states.
 *
 * @param [in]AD2PartitionState * to use for json object.
 *
 * @return cJSON*
 *
 */
cJSON *ad2_get_partition_zone_alerts_json(AD2PartitionState *s)
{
    // OPEN zones.
    cJSON *_zone_alerts = cJSON_CreateArray();
    if (s) {
        for (std::pair<uint8_t, AD2ZoneState> e : s->zone_states) {
            if (s->zone_states[e.first].state() != AD2_STATE_CLOSED) {
                cJSON *zone = cJSON_CreateObject();
                std::string _state_string = AD2Parse.state_str[s->zone_states[e.first].state()];
                // grab the verb(FOO) 'ZONE FOO 001'
                cJSON_AddNumberToObject(zone, "zone", e.first);
                cJSON_AddNumberToObject(zone, "partition", s->partition);
                cJSON_AddNumberToObject(zone, "mask", s->address_mask_filter);
                cJSON_AddStringToObject(zone, "state", _state_string.c_str());
                std::string zalpha;
                AD2Parse.getZoneString((int)s->zone, zalpha);
                cJSON_AddStringToObject(zone, "name", zalpha.c_str());
                cJSON_AddItemToArray(_zone_alerts, zone);
            }
        }
    }
    return _zone_alerts;
}

#define HTTP_SEND_QUEUE_SIZE 20  // More? Less?
#define HTTP_SEND_RATE_LIMIT 200 // 5/s seems like a reasonable value to start with.
static QueueHandle_t _http_sendQ = NULL;

typedef struct sendQ_event_data {
    esp_http_client_config_t *client_config;
    ad2_http_sendQ_ready_cb_t ready;
    ad2_http_sendQ_done_cb_t done;
} sendQ_event_data_t;

/**
 * @brief HTTP sendQ consumer
 *
 * @param [in]pvParameters void *
 */
static void _http_sendQ_consumer_task(void *pvParameters)
{
    esp_err_t err;

    while (1) {
        if (_http_sendQ == NULL) {
            break;
        }
        if (!g_StopMainTask && hal_get_network_connected()) {
            sendQ_event_data_t event_data;
            if (xQueueReceive(_http_sendQ, &event_data, portMAX_DELAY)) {
#if defined(AD2_STACK_REPORT)
                ESP_LOGI(TAG, "_http_sendQ_consumer_task stack free %d", uxTaskGetStackHighWaterMark(NULL));
#endif

                // Initialize a new http client using the client_config..
                esp_http_client_handle_t http_client;
                http_client = esp_http_client_init(event_data.client_config);

                // Set the user agent.
                esp_chip_info_t chip_info;
                esp_chip_info(&chip_info);

                // Set user agent no including version info.
                std::string ua = "AD2IoT-HTTP-Client/NOPE (ESP32-r" + std::to_string(chip_info.revision) + ")";
                esp_http_client_set_header(http_client, "User-Agent", ua.c_str());

                // notify compoenet we are about to send and allow to
                // update connection details including post data etc.
                event_data.ready(http_client, event_data.client_config);

                // Allow for multiple requests on a single connection.
                // TODO: sanity checking. Put back in sendQ for others to get some time? Memory.
                do {
                    // start the connection
                    err = esp_http_client_perform(http_client);

                    // Notify client the request finished and the results.
                    // If it wants to preform again it will return false;
                    if (event_data.done(err, http_client, event_data.client_config)) {
                        break;
                    }
                } while (err == ESP_OK);

                // free the client
                esp_http_client_cleanup(http_client);

                // sleep our rate limit. Not exactly the rate not factoring in
                // client connection time but close enough for now.
                vTaskDelay(HTTP_SEND_RATE_LIMIT / portTICK_PERIOD_MS);
            } else {
                // sleep for a bit then check the queue again.
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
        // sleep for a bit then check the queue again.
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    ESP_LOGW(TAG, "http sendQ ending. HTTP request delivery halted.");
    vTaskDelete(NULL);
}

/**
 * @brief Initialize and start the HTTP request send queue.
 * Allows for components to POST requests to server ASYNC and serialized with each other.
 * This prevents memory exhaustion at the cost of a potential slower delivery time.
 *
 * @note TODO: Add retry logic. Will take some brain time on this. It would not be good
 * to deliver a message out of order yet we need to let other components deliver and not be
 * blocked. Need a queue per component. Maybe have each component manage an internal queue
 * and if one delivery fails let the component decide how to deal with it.
 *
 */
void ad2_init_http_sendQ()
{
    // Init the queue.
    if (_http_sendQ == NULL) {
        _http_sendQ = xQueueCreate(HTTP_SEND_QUEUE_SIZE, sizeof(sendQ_event_data_t));
    }

    // Start the queue consumer task. Keep the stack as small as possible.
    // 20210815SM: 1444 bytes stack free
    xTaskCreate(_http_sendQ_consumer_task, "AD2 sendQ", 1024 * 8, NULL, tskIDLE_PRIORITY + 1, NULL);
}

/**
 * @brief Add a http client config to the queue
 *
 * @param [in]client_config esp_http_client_config_t *
 * @param [in]ready_cb ad2_http_sendQ_ready_cb_t: Called before esp_http_client_perform()
 * @param [in]done_cb ad2_http_sendQ_done_cb_t: Called before esp_http_client_cleanup()
 *
 */
bool ad2_add_http_sendQ(esp_http_client_config_t *client_config, ad2_http_sendQ_ready_cb_t ready_cb, ad2_http_sendQ_done_cb_t done_cb)
{
    // Save queue data into a structure for storage in the sendQ
    sendQ_event_data_t event_data = {
        .client_config = client_config,
        .ready = ready_cb,
        .done = done_cb
    };

    if (_http_sendQ) {
        // Copy the container into the sendQ
        if (xQueueSend(_http_sendQ, (void *)&event_data, (TickType_t)0) != pdPASS) {
            return false;
        }
        return true;
    } else {
        ESP_LOGE(TAG, "Invalid queue handle");
        return false;
    }
}

/**
 * @brief return the ad2 configured network mode value
 *
 * @return char mode
 */
char ad2_get_network_mode(std::string &args)
{
    std::string mode;

    // default to ethernet dhcp on first boot
    std::string modestring = AD2_DEFAULT_NETMODE_STRING;

    ad2_get_config_key_string(CFG_SECTION_MAIN, NETMODE_CONFIG_KEY, modestring);
    ad2_copy_nth_arg(mode, modestring.c_str(), 0);

    switch (mode[0]) {
    case 'W':
    case 'E':
        ad2_copy_nth_arg(args, modestring.c_str(), 1, true);
        break;
    case 'N':
    default:
        args = "";
        mode = 'N'; // default to N on parse error.
        break;
    }
    return (char)mode[0] & 0xff;
}

/**
 * @brief return the current ad2 log mode value
 *
 * @return char mode
 */
char ad2_get_log_mode()
{
    std::string mode = "N";

    ad2_get_config_key_string(CFG_SECTION_MAIN, LOGMODE_CONFIG_KEY, mode);

    switch (mode[0]) {
    case 'I':
    case 'D':
    case 'N':
    case 'V':
        break;
    default:
        mode = 'N';
        break;
    }
    return mode[0];
}

/**
 * @brief Console access control globals
 */
static void *LAST_OWNER = nullptr;
static int console_locked = false;
static unsigned long last_lock_time = 0;

/**
 * @brief return the last time in seconds the console was updated.
 *
 * @return unsigned long Monotonic time of last console udpate.
 */
unsigned long ad2_host_last_lock_time()
{
    return last_lock_time;
}

/**
 * @brief Check if the given owner was the last owner
 * of the host console.
 *
 * @param [in]owner void *
 *
 * @return int result
 */
int ad2_is_host_last(void *owner)
{
    int res = false;
    res = LAST_OWNER == owner;
    return res;
}

/**
 * @brief Take owenrship of the host console.
 *
 * @param [in]owner void *
 * @param [in]wait int time in ms to wait for an exclusive lock.
 *
 * @return int result
 */
int ad2_take_host_console(void *owner, int wait)
{
    int res = false;

    taskENTER_CRITICAL(&spinlock);
    // if already locked by same owner return success.
    if (console_locked && LAST_OWNER == owner) {
        res = true;
        taskEXIT_CRITICAL(&spinlock);
        goto done;
    }
    taskEXIT_CRITICAL(&spinlock);

    res = xSemaphoreTake(g_ad2_console_mutex, pdMS_TO_TICKS(wait));
    if (res == pdTRUE) {
        LAST_OWNER = owner;
        console_locked = true;
    }
done:
    last_lock_time = AD2Parse.monotonicTime();
    return res;
}

/**
 * @brief Release ownership of the host console.
 *
 * @param [in]owner void *
 *
 * @return int result
 */
int ad2_give_host_console(void *owner)
{
    taskENTER_CRITICAL(&spinlock);
    int res = xSemaphoreGive(g_ad2_console_mutex);
    console_locked = false;
    taskEXIT_CRITICAL(&spinlock);
    return res;
}
