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

// No active network IP layer
#define AD2_OFFLINE (1 << 0)

// Active network IP layer
#define AD2_CONNECTED (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

// Communication with AD2* device / host

void ad2_arm_away(int codeId, int addressId);
void ad2_arm_stay(int codeId, int addressId);
void ad2_disarm(int codeId, int addressId);
void ad2_chime_toggle(int codeId, int addressId);
void ad2_fire_alarm(int codeId, int addressId);
void ad2_send(std::string &buf);
AD2VirtualPartitionState *ad2_get_partition_state(int address_slot);
void ad2_printf_host(const char *format, ...);
void ad2_snprintf_host(const char *fmt, size_t size, ...);
char ad2_network_mode(std::string &args);
char ad2_log_mode();
void ad2_set_log_mode(char m);

// string utils

std::string ad2_string_printf(const char *fmt, ...);
std::string ad2_string_vaprintf(const char *fmt, va_list args);
std::string ad2_string_vasnprintf(const char *fmt, size_t size, va_list args);
int  ad2_copy_nth_arg(std::string &dest, char* src, int n);
void ad2_tokenize(std::string const &str, const char delim, std::vector<std::string> &out);
std::string ad2_to_string(int n);
int ad2_query_key_value(std::string &qry_str, const char *key, std::string &val);
void ad2_lcase(std::string &str);
void ad2_ucase(std::string &str);
bool ad2_replace_all(std::string& inStr, const char *findStr, const char *replaceStr);

// NV Storage utilities

void ad2_get_nv_arg(const char *key, std::string &value);
void ad2_set_nv_arg(const char *key, const char *value);
void ad2_set_nv_slot_key_int(const char *key, int slot, int value);
void ad2_get_nv_slot_key_int(const char *key, int slot, int *value);
void ad2_set_nv_slot_key_string(const char *key, int slot, const char *value);
void ad2_get_nv_slot_key_string(const char *key, int slot, std::string &value);

#ifdef __cplusplus
}
#endif

#endif /* _AD2_UTILS_H */

