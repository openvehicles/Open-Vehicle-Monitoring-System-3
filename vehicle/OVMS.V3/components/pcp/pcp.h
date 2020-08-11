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

#ifndef __PCP_H__
#define __PCP_H__

#include <sys/types.h>
#include <string>
#include <map>
#include "ovms_command.h"

enum PowerMode { Undefined, On, Sleep, DeepSleep, Off, Devel };

class pcp
  {
  public:
    pcp(const char* name);
    virtual ~pcp();

  public:
    virtual void SetPowerMode(PowerMode powermode);
    const char* GetName();
    PowerMode GetPowerMode();

  protected:
    const char* m_name;
    PowerMode m_powermode;
  };

typedef std::map<const char*, pcp*, CompareCharPtr> pcpmap_t;
typedef std::map<const char*, PowerMode, CompareCharPtr> pcpmappm_t;

class pcpapp
  {
  public:
    pcpapp();
    virtual ~pcpapp();

  public:
    void Register(const char* name, pcp* device);
    void Deregister(const char* name);
    pcp* FindDeviceByName(const char* name);
    PowerMode FindPowerModeByName(const char* name);
    const char* FindPowerModeByType(PowerMode mode);

  public:
    pcpmap_t m_map;
    pcpmappm_t m_mappm;
  };

extern pcpapp MyPcpApp;

#endif //#ifndef __PCP_H__
