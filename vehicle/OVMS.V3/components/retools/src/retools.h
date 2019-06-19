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

#ifndef __RETOOLS_H__
#define __RETOOLS_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string>
#include <map>
#include "can.h"
#include "canformat.h"
#include "dbc.h"
#include "pcp.h"
#include "ovms.h"
#include "ovms_mutex.h"
#include "ovms_netmanager.h"

typedef struct
  {
  CAN_frame_t last;
  uint32_t rxcount;
  struct __attribute__((__packed__))
    {
    struct {
      uint8_t Ignore:1;     // 0x01
      uint8_t Changed:1;    // 0x02
      uint8_t Discovered:1; // 0x04
      uint8_t :1;           // 0x08
      uint8_t :1;           // 0x10
      uint8_t :1;           // 0x20
      uint8_t :1;           // 0x40
      uint8_t :1;           // 0x80
      } b;
    uint8_t dc;             // Data bytes changed
    uint8_t dd;             // Data bytes discovered
    uint8_t spare;
    } attr;
  } re_record_t;

typedef std::map<std::string, re_record_t*> re_record_map_t;

enum REMode { Serve, Analyse, Discover };
enum REServeMode { Ignore, Simulate, Transmit };

class re : public pcp, public ExternalRamAllocated
  {
  public:
    re(const char* name);
    ~re();

  public:
    virtual void SetPowerMode(PowerMode powermode);

  public:
    void Task();
    void Clear();
    std::string GetKey(CAN_frame_t* frame);

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  public:
    void MongooseHandler(struct mg_connection *nc, int ev, void *p);

  public:
    typedef std::map<mg_connection*, uint8_t> re_serve_map_t;
    re_serve_map_t m_smap;
    OvmsMutex m_smapmutex;
#endif // #ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

  protected:
    void DoAnalyse(CAN_frame_t* frame);
    void DoServe(CAN_log_message_t* message);

  protected:
    TaskHandle_t m_task;
    QueueHandle_t m_rxqueue;

  public:
    OvmsMutex m_mutex;
    REMode m_mode;
    REServeMode m_servemode;
    re_record_map_t m_rmap;
    canformat* m_serveformat_in;
    canformat* m_serveformat_out;
    uint32_t m_obdii_std_min;
    uint32_t m_obdii_std_max;
    uint32_t m_obdii_ext_min;
    uint32_t m_obdii_ext_max;
    uint32_t m_started;
    uint32_t m_finished;
  };

#endif //#ifndef __RETOOLS_H__
