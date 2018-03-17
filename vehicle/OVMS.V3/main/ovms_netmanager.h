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

#ifndef __OVMS_NETMANAGER_H__
#define __OVMS_NETMANAGER_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ovms_events.h"

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
#define MG_LOCALS 1
#include "mongoose.h"
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

class OvmsNetManager
  {
  public:
    OvmsNetManager();
    ~OvmsNetManager();

  public:
    void WifiUpSTA(std::string event, void* data);
    void WifiDownSTA(std::string event, void* data);
    void WifiUpAP(std::string event, void* data);
    void WifiDownAP(std::string event, void* data);
    void ModemUp(std::string event, void* data);
    void ModemDown(std::string event, void* data);
    void InterfaceUp(std::string event, void* data);

  public:
    void SetInterfacePriority();

  public:
    bool m_connected_wifi;
    bool m_connected_modem;
    bool m_connected_any;
    bool m_wifi_ap;

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  protected:
    void StartMongooseTask();
    void StopMongooseTask();

  protected:
    TaskHandle_t m_mongoose_task;
    struct mg_mgr m_mongoose_mgr;
    bool m_mongoose_running;

  public:
    void MongooseTask();
    TaskHandle_t GetMongooseTaskHandle() { return m_mongoose_task; }
    struct mg_mgr* GetMongooseMgr();
    bool MongooseRunning();

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  };

extern OvmsNetManager MyNetManager;

#endif //#ifndef __OVMS_NETMANAGER_H__
