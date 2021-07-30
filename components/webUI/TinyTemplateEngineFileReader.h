/**
 *  @file    TinyTemplateEngineFileReader.h
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

#ifndef FILE_READER_H_INCLUDED
#define FILE_READER_H_INCLUDED

#include "TinyTemplateEngine.h"

class TinyTemplateEngineFileReader: public TinyTemplateEngine::Reader
{
public:
    TinyTemplateEngineFileReader(FILE *);
    virtual ~TinyTemplateEngineFileReader() {}
    virtual TinyTemplateEngine::Line nextLine() override;
    virtual void reset() override;
private:
    FILE* _fd;
    char _buffer[255];
};

#endif
