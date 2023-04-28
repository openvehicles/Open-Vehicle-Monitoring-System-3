/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
;
;    Changes:
;    0.0.1  Initial release
;
;    (C) 2021       Didier Ernotte
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

#ifndef __VEHICLE_JAGUARIPACE_H__
#define __VEHICLE_JAGUARIPACE_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleJaguarIpace : public OvmsVehicle {
  public:
    OvmsVehicleJaguarIpace();
    ~OvmsVehicleJaguarIpace();

  protected:
    void IncomingPollReply(
      canbus* bus, uint32_t moduleidsent, uint32_t moduleid, uint16_t type, uint16_t pid,
      const uint8_t* data, uint16_t mloffset, uint8_t length, uint16_t mlremain, uint16_t mlframe,
      const OvmsPoller::poll_pid_t &pollentry) override;
    void IncomingFrameCan1(const CAN_frame_t* p_frame) override;
    //void IncomingFrameCan2(const CAN_frame_t* p_frame) override;
    //void IncomingFrameCan3(const CAN_frame_t* p_frame) override;
    //void IncomingFrameCan4(const CAN_frame_t* p_frame) override;

    char m_vin[18];
    uint8_t m_localization[10];
    
    OvmsCommand *cmd_xrt;
    OvmsMetricFloat *xji_v_bat_capacity_soh_min ;
    OvmsMetricFloat *xji_v_bat_capacity_soh_max ;
    OvmsMetricFloat *xji_v_bat_power_soh;
    OvmsMetricFloat *xji_v_bat_power_soh_min;
    OvmsMetricFloat *xji_v_bat_power_soh_max;
    OvmsMetricFloat *xji_v_bat_max_regen;


  private:
    void IncomingPollFrame(const CAN_frame_t* frame);
    void IncomingBecmPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain);
    void IncomingHvacPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain);
    void IncomingBcmPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain);
    void IncomingTpmsPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain);
    void IncomingTcuPoll(uint16_t pid, const uint8_t* data, uint8_t length, uint16_t remain);

    bool SendPollMessage(canbus* bus, uint16_t id, uint8_t type, uint16_t pid);

};

#endif //#ifndef __VEHICLE_JAGUARIPACE_H__
