/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th of November 2025
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2024-2025  Raphael Mack
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

#ifndef __VEHICLE_FIATEDOBLO_H__
#define __VEHICLE_FIATEDOBLO_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleFiatEDoblo : public OvmsVehicle
{
public:
  OvmsVehicleFiatEDoblo();
  ~OvmsVehicleFiatEDoblo();
public:
  void IncomingFrameCan1(CAN_frame_t* p_frame) override;
  void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
  void Ticker60(uint32_t ticker) override;
    
protected:
  void IncomingBatteryPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);
  void IncomingVCUPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);
  void IncomingVINPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);

  // Inline functions to handle the different Poll states.
  inline int PollGetState()
  {
    return m_poll_state;
  }
  inline void PollState_Off()
  {
    PollSetState(0);
  }
  inline bool IsPollState_Off()
  {
    return m_poll_state == 0;
  }

  inline void PollState_Running()
  {
    PollSetState(1);
  }
  inline bool IsPollState_Running()
  {
    return m_poll_state == 1;
  }
    
  inline void PollState_Charging()
  {
    PollSetState(2);
  }
  inline bool IsPollState_Charging()
  {
    return m_poll_state == 2;
  }

  inline void PollState_Ping()
  {
    PollSetState(3);
  }
    
protected:
  uint32_t m_lastCanFrameTime = 0;
  uint32_t m_last305FrameTime = 0;
  
  std::string m_vin;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // --------------------------------------------------------------------------
  // Webserver subsystem
public:
  void GetDashboardConfig(DashboardConfig& cfg);
#endif //CONFIG_OVMS_COMP_WEBSERVER
    
};

#endif //#ifndef __VEHICLE_FIATEDOBLO_H__
