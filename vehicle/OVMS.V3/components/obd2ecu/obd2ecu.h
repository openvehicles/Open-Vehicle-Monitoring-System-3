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

class obd2ecu : public pcp
  {
  public:
    obd2ecu(std::string name, canbus* can);
    ~obd2ecu();

  public:
    virtual void SetPowerMode(PowerMode powermode);

  public:
    canbus* m_can;
    QueueHandle_t m_rxqueue;
    TaskHandle_t m_task;
    time_t m_starttime;
    uint8_t m_private;

  public:
    void IncomingFrame(CAN_frame_t* p_frame);
 
  protected:
    void FillFrame(CAN_frame_t *frame,int reply,uint8_t pid,float data,uint8_t format);
  };
  
      
#define REQUEST_PID  0x7df
#define RESPONSE_PID 0x7e8
#define FLOWCONTROL_PID 0x7e0
#define REQUEST_EXT_PID  0x98db33f1
#define RESPONSE_EXT_PID 0x98daf10e
#define FLOWCONTROL_EXT_PID 0x98da0ef1
  
#define verbose 0

#endif //#ifndef __OBD2ECU_H__
