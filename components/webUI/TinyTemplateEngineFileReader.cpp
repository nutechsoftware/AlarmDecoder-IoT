/**
 *  @file    TinyTemplateEngineFileReader.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    07/21/2021
 *
 *  @brief Custom Line reader for TinyTemplateEngine
 *           https://github.com/full-stack-ex/tiny-template-engine-arduino
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

#include "TinyTemplateEngineFileReader.h"
#include <cstring>
#include <string.h>
#include <stdio.h>

TinyTemplateEngineFileReader::TinyTemplateEngineFileReader(FILE *fd):
    Reader(),
    _fd(fd),
    _buffer({0}) {}

void TinyTemplateEngineFileReader::reset()
{
    fseek(_fd, 0, SEEK_SET);
}

TinyTemplateEngine::Line TinyTemplateEngineFileReader::nextLine()
{
    // consume bytes looking for \n or if > 200 any non template character.
    uint8_t i = 0;
    int8_t c;
    do {
        c = fgetc(_fd);
        if(c == EOF) {
            // send a NULL back and it will be flagged as EOF.
            return TinyTemplateEngine::Line(0, 0);
        }

        _buffer[i++] = c;
        if (c == '\n') {
            break;
        }

        if ( i > 200 ) {
            if (c != '{' && c != '}' && c != '$' && (c <= '0' || c >='9')) {
                break;
            }
        }

        // guard
        if ( i == sizeof(_buffer)-1) {
            break;
        }
    } while(!feof(_fd));
    return TinyTemplateEngine::Line(_buffer, i);
}

