/**
 * Project:      Open Vehicle Monitor System
 * Module:       Poll Reply Helper
 * 
 * (c) 2020 Soko
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __POLL_REPLY_HELPER_H__
#define __POLL_REPLY_HELPER_H__

#include <string>
#include <cmath>
#include <cfloat>
#include <iomanip>
#include <sstream>

using namespace std;

class PollReplyHelper
{
public:
    bool AddNewData(uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain);

    bool FromUint8(const std::string &info, float &value, uint8_t shiftLeft = 0);
    bool FromUint16(const std::string &info, float &value, uint8_t shiftLeft = 0);
    bool FromInt32(const std::string &info, float &value, uint8_t shiftLeft = 0);

private:
    string Store;
    uint16_t LastPid = 0;
    uint16_t LastRemain = 0;    
};

#endif //#ifndef __POLL_REPLY_HELPER_H__
