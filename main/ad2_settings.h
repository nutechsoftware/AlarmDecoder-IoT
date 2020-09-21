/**
 *  @file    ad2_settings.h
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief Settings common to everyone
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

// @brief MAX address slots
#define AD2_MAX_ADDRESS       99

// @brief MAX virtual partition slots
#define AD2_MAX_VPARTITION    9

// @brief MAX code slots
#define AD2_MAX_CODE          99

// @brief Default code to use if none specified
#define AD2_DEFAULT_CODE_SLOT 0

// @brief Default Address to use if none specified
#define AD2_DEFAULT_VPA_SLOT  0

// @brief Max connection mode string argument size(host:port|UART#)
#define AD2_MAX_MODE_ARG_SIZE 80

// @brief Parent Key to store virtual partition slots
#define VPADDR_CONFIG_KEY     "vpa"

// @brief Parent Key to store code slots
#define CODES_CONFIG_KEY      "codes"

// @brief Parent Key to store ad2 source configuration values
#define AD2MODE_CONFIG_KEY    "ad2source"
