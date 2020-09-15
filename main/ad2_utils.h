/**
 *  @file    ad2_utils.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *  @version 1.0
 *
 *  @brief AlarmDecoder IoT embedded network appliance for SmartThings
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
#ifndef ad2_utils_h
#define ad2_utils_h

#ifdef __cplusplus
extern "C" {
#endif
int  ad2_copy_nth_arg(char* dest, char* src, int size, int n);
void ad2_get_nv_arg(const char *key, char *arg, size_t size);
void ad2_set_nv_arg(const char *key, char *arg);
void ad2_set_nv_slot_key_int(const char *key, int slot, int value);
void ad2_get_nv_slot_key_int(const char *key, int slot, int *value);
void ad2_set_nv_slot_key_string(const char *key, int slot, char *value);
void ad2_get_nv_slot_key_string(const char *key, int slot, char *value, size_t size);
#ifdef __cplusplus
}
#endif

#endif
