/**
*  @file    ftpd.h
*  @author  Sean Mathews <coder@f34r.com>
*  @date    03/5/2022
*  @version 1.0.0
*
*  @brief Simple ftp daemon for updates to uSD on AD2IoT
*  based upon work by kolban. If you don't have his book get it. I liked
*  it so much I purchased the electrons twice!
*   https://github.com/nkolban/esp32-snippets/tree/master/networking/FTPServer
*
*  @copyright Copyright (C) 2021 Nu Tech Software Solutions, Inc.
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
#ifndef _FTPD_H
#define _FTPD_H
#if CONFIG_AD2IOT_FTP_DAEMON
void ftpd_register_cmds();
void ftpd_init();
#endif /* CONFIG_AD2IOT_FTP_DAEMON */
#endif /* _FTPD_H */
