/*
MIT License

Copyright (c) 2019 full-stack-ex

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Changes
//  Sean Mathews <coder@f34r.com>
//
// Original project
//    https://raw.githubusercontent.com/full-stack-ex/tiny-template-engine-arduino/master/src/TinyTemplateEngine.cpp
//
// Required some minor changes to work with platform.io and esp-idf.
// This is a Arduino plugin and I have not found a way to include the project directly.
// 1. Remove yeild() and deal with sleeping a few levels up.
// 2. Change includes remove Arduino.h and add needed string includes.
// 3. Add custom Line reader(s).

#include <cstring>
#include <string.h>
#include "TinyTemplateEngine.h"

TinyTemplateEngine::TinyTemplateEngine(Reader& reader): _reader(reader), _buffer(0), _values(0)
{
}

TinyTemplateEngine::~TinyTemplateEngine()
{
    reset();
}

void TinyTemplateEngine::start(const char * const * values)
{
    reset();
    _reader.reset();
    _values = values;
}

const char *TinyTemplateEngine::nextLine()
{
    deleteBuffer();
    Line line = _reader.nextLine();
    if (line.eof()) {
        return 0;
    }
    size_t outSize = calculateOutLength(line);
    allocateBuffer(outSize+1);
    expandToBuffer(line);
    return _buffer;
}

void TinyTemplateEngine::end()
{
    reset();
}

void TinyTemplateEngine::reset()
{
    deleteBuffer();
    _values = 0;
}


class LineTraverser
{

public:

    LineTraverser(const char* const* values): _values(values) {}
    virtual ~LineTraverser() {}

    virtual void traverse(
        const TinyTemplateEngine::Line& line
    )
    {
        const size_t MAX_INDEX_LEN = 5;
        if (! *line.text) {
            return;
        }
        bool insideBrackets = false;
        bool buckHit = false;
        char parameterIndexS[MAX_INDEX_LEN+1];
        char* parameterIndexPos;
        for (size_t i=0; i<line.length; i++) {
            //yield(); // not needed in this use case. Already done up a few levels in the stack.
            if (! insideBrackets && i<line.length-1 && line.text[i] == '$' && line.text[i+1] == '{') {
                buckHit = true;
            } else if (buckHit && ! insideBrackets && line.text[i] == '{') {
                insideBrackets = true;
                buckHit = false;
                parameterIndexPos = parameterIndexS;
            } else if (insideBrackets && line.text[i] != '}') {
                if (parameterIndexPos - parameterIndexS < MAX_INDEX_LEN) {
                    *parameterIndexPos++ = line.text[i];
                }
            } else if (insideBrackets && line.text[i] == '}') {
                insideBrackets = false;
                *parameterIndexPos = 0;
                int parameterIndex = atoi(parameterIndexS);
                bool valuePassed = false;
                if (_values) {
                    valuePassed = true;
                    for (int i=0; i<=parameterIndex; i++) {
                        if (! _values[i]) {
                            valuePassed = false;
                            break;
                        }
                    }
                }
                if (valuePassed) {
                    onParameterIndexRead(_values, parameterIndex);
                }
            } else {
                onNextCharacter(line.text, i);
            }
        }
    }

protected:

    virtual void onParameterIndexRead(const char * const * values, int parameterIndex) = 0;

    virtual void onNextCharacter(const char* text, size_t position) = 0;

private:

    const char* const* _values;

};

class OutLengthCalculator: public LineTraverser
{

public:

    OutLengthCalculator(const char* const* values, size_t& outSizeP): LineTraverser(values), _outSize(outSizeP) {}

    virtual void onParameterIndexRead(const char * const * values, int parameterIndex) override
    {
        _outSize += strlen(values[parameterIndex]);
    }

    virtual void onNextCharacter(const char* text, size_t position) override
    {
        _outSize++;
    }

private:

    size_t& _outSize;

};

size_t TinyTemplateEngine::calculateOutLength(const Line& line) const
{
    if (line.eof()) {
        return 0;
    }
    size_t outSize = 0;
    OutLengthCalculator calc(_values, outSize);
    calc.traverse(line);
    return outSize;
}

void TinyTemplateEngine::allocateBuffer(size_t size)
{
    deleteBuffer();
    _buffer = new char[size];
}

class LineExpander: public LineTraverser
{

public:

    LineExpander(const char* const* values, char*& outPos): LineTraverser(values), _outPos(outPos) {}

    virtual void onParameterIndexRead(const char * const * values, int parameterIndex) override
    {
        memcpy(_outPos, values[parameterIndex], strlen(values[parameterIndex]));
        _outPos += strlen(values[parameterIndex]);
    }

    virtual void onNextCharacter(const char* text, size_t position) override
    {
        *_outPos++ = text[position];
    }

private:

    char*& _outPos;

};

void TinyTemplateEngine::expandToBuffer(const Line& line)
{
    if (line.eof()) {
        return;
    }
    char* outPos = _buffer;
    LineExpander expander(_values, outPos);
    expander.traverse(line);
    *outPos = 0;
}


void TinyTemplateEngine::deleteBuffer()
{
    if (_buffer) {
        delete _buffer;
        _buffer = 0;
    }
}

