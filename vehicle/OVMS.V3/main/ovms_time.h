/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#ifndef __OVMS_TIME_H__
#define __OVMS_TIME_H__

#include <time.h>
#include <sys/time.h>
#include <functional>
#include <map>
#include <string>
#include "ovms_utils.h"

using namespace std;

class OvmsTimeProvider
  {
  public:
    OvmsTimeProvider(const char* provider, int stratum=0, time_t tim=0);
    ~OvmsTimeProvider();

  public:
    const char* m_provider;
    time_t m_time;
    uint32_t m_lastreport;
    int m_stratum;

  public:
    void Set(int stratum, time_t tim);
  };

typedef std::map<const char*, OvmsTimeProvider*, CmpStrOp> OvmsTimeProviderMap_t;

class OvmsTime
  {
  public:
    OvmsTime();
    ~OvmsTime();

  public:
    OvmsTimeProviderMap_t m_providers;
    OvmsTimeProvider* m_current;
    struct timezone m_tz;

  public:
    void Set(const char* provider, int stratum, bool trusted, time_t tim, suseconds_t timu=0);
    void Elect();

  public:
    void EventConfigChanged(std::string event, void* data);
    void EventSystemStart(std::string event, void* data);
  };

extern OvmsTime MyTime;

#endif //#ifndef __OVMS_TIME_H__
