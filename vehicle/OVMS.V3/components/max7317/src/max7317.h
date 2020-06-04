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

#include <stdint.h>
#include "pcp.h"
#include "spi.h"
#include <bitset>
#include "ovms_timer.h"
#include "ovms_semaphore.h"
#include "ovms_mutex.h"

#ifndef __MAX7317_H__
#define __MAX7317_H__

class max7317 : public pcp, public InternalRamAllocated
  {
  public:
    max7317(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin);
    ~max7317();

  public:
    void Output(uint8_t port, uint8_t level);
    std::bitset<10> OutputState();
    uint8_t Input(uint8_t port);
    std::bitset<10> Inputs(std::bitset<10> ports);
    std::bitset<10> InputState();
    bool Monitor(std::bitset<10> ports, bool enable);
    bool Monitor(uint8_t port, bool enable);
    std::bitset<10> MonitorState();

  public:
    std::bitset<10> GetConfigMonitorPorts();
    void AutoInit();
    bool CheckMonitor();
    void MonitorTask();

  protected:
    spi* m_spibus;
    spi_nodma_device_interface_config_t m_devcfg;
    spi_nodma_host_device_t m_host;
    int m_clockspeed;
    int m_cspin;
    spi_nodma_device_handle_t m_spi;
    std::bitset<10> m_outputstate;
    std::bitset<10> m_inputstate;
    std::bitset<10> m_monitor_ports;
    OvmsInterval* m_monitor_timer;
    OvmsSemaphore m_monitor_semaphore;
    OvmsMutex m_monitor_mutex;
    TaskHandle_t m_monitor_task;
  };

#endif //#ifndef __MAX7317_H__
