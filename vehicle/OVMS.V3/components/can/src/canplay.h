/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN logging framework
;    Date:          18th January 2018
;
;    (C) 2018       Michael Balzer
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

#ifndef __CANPLAY_H__
#define __CANPLAY_H__

#include "freertos/semphr.h"
#include "can.h"
#include "canformat.h"

/**
 * canplay is the general interface and base implementation for all can players.
 */
class canplay : public InternalRamAllocated
  {
  public:
    canplay(const char* type, std::string format, canformat::canformat_serve_mode_t mode=canformat::Simulate);
    virtual ~canplay();

  public:
    static void PlayTask(void* context);

  public:
    const char* GetType();
    const char* GetFormat();
    virtual std::string GetStats();
    void SetSpeed(uint32_t speed);

  public:
    // Methods expected to be implemented by sub-classes
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() = 0;
    virtual std::string GetInfo();
    virtual bool InputMsg(CAN_log_message_t* msg);

  public:
    virtual void SetFilter(canfilter* filter);
    virtual void ClearFilter();

  public:
    const char*         m_type;
    std::string         m_format;
    uint32_t            m_speed;
    canformat*          m_formatter;
    canfilter*          m_filter;

  public:
    TaskHandle_t        m_task;
    uint32_t            m_msgcount;
  };

#endif // __CANPLAY_H__
