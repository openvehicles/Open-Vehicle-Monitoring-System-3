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
;    (C) 2017       Gregory Dolkas
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

#ifndef __OBD2ECU_H__
#define __OBD2ECU_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcp.h"
#include "can.h"
#include "ovms_metrics.h"

class obd2pid
  {
  public:
    typedef enum
      {
      Unimplemented,
      Internal,
      Metric,
      Script
      } pid_t;

  public:
    obd2pid(int pid, pid_t type=Unimplemented, OvmsMetric* metric=NULL);
    ~obd2pid();

  public:
    int GetPid();
    pid_t GetType();
    const char* GetTypeString();
    OvmsMetric* GetMetric();
    void SetType(pid_t type);
    void SetMetric(OvmsMetric* metric);
    void LoadScript(std::string path);
    float Execute();

  public:
    float InternalPid();

  protected:
    int m_pid;
    pid_t m_type;
    char* m_script;
    OvmsMetric* m_metric;
  };

typedef std::map<int, obd2pid*> PidMap;

class obd2ecu : public pcp, public InternalRamAllocated
  {
  public:
    obd2ecu(const char* name, canbus* can);
    ~obd2ecu();

  public:
    virtual void SetPowerMode(PowerMode powermode);

  public:
    canbus* m_can;
    QueueHandle_t m_rxqueue;
    TaskHandle_t m_task;
    time_t m_starttime;
    PidMap m_pidmap;
    uint32_t m_supported_01_20;  // bitmap of PIDs configured 0x01 through 0x20
    uint32_t m_supported_21_40;  // bitmap of PIDs configured 0x21 through 0x40

  public:
    void NotifyStartup();
    void NotifyShutdown();

    void IncomingFrame(CAN_frame_t* p_frame);
    void LoadMap();
    void ClearMap();
    void Addpid(uint8_t pid);

  protected:
    void FillFrame(CAN_frame_t *frame,int reply,uint8_t pid,float data,uint8_t format);
    void ECURxCallback(const CAN_frame_t* frame, bool success);
  };
  
class obd2ecuInit
  {
  public:
    obd2ecuInit();
    void AutoInit();
  };

extern class obd2ecuInit obd2ecuInit;


#define REQUEST_PID  0x7df
#define RESPONSE_PID 0x7e8
#define FLOWCONTROL_PID 0x7e0
#define REQUEST_EXT_PID  0x18db33f1
#define RESPONSE_EXT_PID 0x18daf10e
#define FLOWCONTROL_EXT_PID 0x18da0ef1


#endif //#ifndef __OBD2ECU_H__
