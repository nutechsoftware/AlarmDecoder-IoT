/**
 *  @file    ad2_utils.h
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
#ifndef _AD2_UTILS_H
#define _AD2_UTILS_H


// Helper to find array storage size.
#define ARRAY_SIZE(x) (int)(sizeof(x)/sizeof(x[0]))

// Debugging config
//#define DEBUG_CONFIG

// Communication with AD2* device / host
void ad2_arm_away(std::string &code, int partId);
void ad2_arm_away(int codeId, int partId);
void ad2_arm_stay(std::string &code, int partId);
void ad2_arm_stay(int codeId, int partId);
void ad2_disarm(std::string &code, int partId);
void ad2_disarm(int codeId, int partId);
void ad2_chime_toggle(std::string &code, int partId);
void ad2_chime_toggle(int codeId, int partId);
void ad2_fire_alarm(int partId);
void ad2_panic_alarm(int partId);
void ad2_aux_alarm(int partId);
void ad2_exit_now(int partId);
void ad2_bypass_zone(std::string &code, int partId, uint8_t zone);
void ad2_bypass_zone(int codeId, int partId, uint8_t zone);
void ad2_send(std::string &buf);
AD2PartitionState *ad2_get_partition_state(int partId);
cJSON *ad2_get_ad2iot_device_info_json();
cJSON *ad2_get_partition_state_json(AD2PartitionState *);
cJSON *ad2_get_partition_zone_alerts_json(AD2PartitionState *);
int ad2_log_vprintf_host(const char *fmt, va_list args);
void ad2_printf_host(bool prefix, const char *format, ...);
void ad2_snprintf_host(const char *fmt, size_t size, ...);
int ad2_take_host_console(void *owner, int timeout);
int ad2_give_host_console(void *owner);
int ad2_is_host_last(void *owner);
unsigned long ad2_host_last_lock_time();
char ad2_get_network_mode(std::string &args);
char ad2_get_log_mode();

// string utils
std::string ad2_string_printf(const char *fmt, ...);
std::string ad2_string_vaprintf(const char *fmt, va_list args);
std::string ad2_string_vasnprintf(const char *fmt, size_t size, va_list args);
int ad2_copy_nth_arg(std::string &dest, const char* src, int n, bool remaining = false);
void ad2_tokenize(std::string const &str, const char* delim, std::vector<std::string> &out);
std::string ad2_make_basic_auth_string(const std::string& user, const std::string& password);
std::string ad2_urlencode(const std::string str);
void ad2_genUUID(uint8_t n, std::string& ret);
void ad2_lcase(std::string &str);
void ad2_ucase(std::string &str);
bool ad2_replace_all(std::string& inStr, const char *findStr, const char *replaceStr);
void ad2_ltrim(std::string &s);
void ad2_rtrim(std::string &s);
void ad2_trim(std::string &s);
void ad2_remove_ws(std::string &s);

/* @brief container for IP addresses */
struct ad2_addr {
    u8_t  u8_addr[16];
    u32_t padding;
};

/**
 *  @brief ACL parser and test class
 *
 *  @details Simple parser for ACL strings and testing for matches given
 *  an IP string or the uint32_t value of an IP.
 *     Example ACL String: '192.168.0.0/24, 192.168.1.1-192.168.1.2'
 */
class ad2_acl_check
{
public:
    int add(std::string& acl);
    bool find(std::string szaddr);
    bool find(ad2_addr& addr);
    void clear()
    {
        allowed_networks.clear();
    };

    enum {
        ACL_FORMAT_OK = 0,
        ACL_ERR_BADFORMAT_CIDR = -1,
        ACL_ERR_BADFORMAT_IP = -2
    };

    // @brief debugging
    void dump(ad2_addr& addr);

private:
    // @brief check if addr is between(inclusive) s and e
    bool _in_addr_BETWEEN(ad2_addr& addr, ad2_addr& start, ad2_addr& end);

    // @brief 128bit left shift.
    // @note modifies addr
    void _addr_SHIFT_LEFT(ad2_addr& addr, int cnt);

    // @brief 128bit NOT(~).
    // @note modifies addr
    void _addr_NOT(ad2_addr& addr);

    // @brief 128bit AND.
    void _addr_AND(ad2_addr& out, ad2_addr& addrA, ad2_addr& addrB);

    // @brief 128bit OR.
    void _addr_OR(ad2_addr& out, ad2_addr& addrA, ad2_addr& addrB);

    // @brief parse IPv4/IPv6 address string to ip6_addr value.
    // @return bool ok = true;
    bool _szIPaddrParse(std::string& szaddr, ad2_addr& addr, bool& is_ipv4);

    // @brief list of allowed network start and end addresses.
    vector<pair<ad2_addr,ad2_addr>> allowed_networks;
};

// Configuration Storage utilities
void ad2_get_config_key_bool(
    const char * section, const char *key,
    bool *vout,
    int index = -1, const char *suffix = NULL
);
void ad2_set_config_key_bool(
    const char * section, const char *key,
    bool vin,
    int index = -1, const char *suffix = NULL,
    bool remove = false
);
void ad2_get_config_key_int(
    const char * section, const char *key,
    int *vout,
    int index = -1, const char *suffix = NULL
);
void ad2_set_config_key_int(
    const char * section, const char *key,
    int vin,
    int index = -1, const char *suffix = NULL,
    bool remove = false
);
void ad2_get_config_key_string(
    const char * section, const char *key,
    std::string &vout,
    int index = -1, const char *suffix = NULL
);
void ad2_set_config_key_string(
    const char * section, const char *key,
    const char *vin,
    int index = -1, const char *suffix = NULL,
    bool remove = false
);

#define CFG_SECTION_MAIN ""

// persistent configuration load/save
void ad2_load_persistent_config();
void ad2_save_persistent_config();

// ASYNC serialized http request api for components.

/// ad2_http async http request callback. Called before esp_http_client_perform()
typedef void (*ad2_http_sendQ_ready_cb_t)(esp_http_client_handle_t, esp_http_client_config_t*);
/// ad2_http async http request callback. Called before after esp_http_client_cleanup()
typedef bool (*ad2_http_sendQ_done_cb_t)(esp_err_t, esp_http_client_handle_t, esp_http_client_config_t*);
void ad2_init_http_sendQ();
bool ad2_add_http_sendQ(esp_http_client_config_t*, ad2_http_sendQ_ready_cb_t, ad2_http_sendQ_done_cb_t);


#endif /* _AD2_UTILS_H */

