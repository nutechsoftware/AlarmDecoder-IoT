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

#define AD2_MAX_ADDRESS 99
#define AD2_MAX_VPARTITION 9
#define AD2_MAX_CODE 127

#define AD2_DEFAULT_CODE_SLOT 0
#define AD2_DEFAULT_VPA_SLOT 0

#define AD2_MAX_MODE_ARG_SIZE 80

#ifdef __cplusplus
extern "C" {
#endif
void ad2_set_nv_code(int slot, char *code);
void ad2_get_nv_code(int slot, char *code, size_t size);
void ad2_set_nv_vpaddr(int slot, int32_t address);
void ad2_get_nv_vpaddr(int slot, int32_t *address);
void ad2_set_nv_mode_arg(uint8_t mode, char *arg);
void ad2_get_nv_mode_arg(uint8_t *mode, char *arg, size_t size);
int  ad2_copy_nth_arg(char* dest, char* src, int size, int n);
void ad2_get_nv_arg(char *key, char *arg, size_t size);
void ad2_set_nv_arg(char *key, char *arg);
#ifdef __cplusplus
}
#endif

#endif