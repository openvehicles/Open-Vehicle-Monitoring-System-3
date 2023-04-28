/*
;    Project:       Open Vehicle Monitor System for Toyota RAV4 EV (2012-2014)
;    Date:          22nd June 2021
;
;    Changes:
;    0.1.0  Trial code
;    0.2.0  Code initially submitted to master branch
;    0.3.0  Implemented v_on and v_charging states with some metrics only set/cleared depending on these states
;    0.4.0  Changed v_on to be gated on BMS_status instead of BMS_CtrSet to eliminate the overlap with AC charging state, 
;           drop SOCui, added gear letter metric.
;
;    (C) 2021-2022 Mike Iimura
;
;   Adapted from Tesla Model S by:
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

#ifndef __VEHICLE_TOYOTARAV4EV_H__
#define __VEHICLE_TOYOTARAV4EV_H__

#include "vehicle.h"
#include "ovms_metrics.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

#define TS_CANDATA_TIMEOUT 10
#define BMS_CTRSET_OPEN 0x00
#define BMS_CTRSET_PRECHARGE 0x01
#define BMS_CTRSET_CLOSED 0x02
#define BMS_STANDBY 0x00
#define BMS_DRIVE 0x01
#define BMS_CHARGER 0x03

class OvmsVehicleToyotaRav4Ev: public OvmsVehicle
  {
  public:
    OvmsVehicleToyotaRav4Ev();
    ~OvmsVehicleToyotaRav4Ev();

  public:
    void IncomingFrameCan1(const CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(const CAN_frame_t* p_frame) override;
//    void IncomingFrameCan3(const CAN_frame_t* p_frame) override;
//    void GetDashboardConfig(DashboardConfig& cfg);

  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();
    virtual void NotifyBmsAlerts();

/*
#ifdef CONFIG_OVMS_COMP_TPMS
  public:
    virtual bool TPMSRead(std::vector<uint32_t> *tpms);
    virtual bool TPMSWrite(std::vector<uint32_t> &tpms);

  protected:
    typedef enum
      {
      Idle = 0,
      Reading,
      DoneReading,
      ReadFailed,
      Writing,
      DoneWriting,
      Writefailed
    } tpms_command_t;
    tpms_command_t m_tpms_cmd;
    uint8_t m_tpms_data[16];
    int m_tpms_pos;
    uint16_t m_tpms_uds_seed;
#endif // #ifdef CONFIG_OVMS_COMP_TPMS
*/

  protected:
    char m_vin[18];
    char m_type[5];
    // uint16_t m_charge_w;
    unsigned int m_candata_timer;
    float fPackVolts;
    float fPackAmps;
    uint16_t iPackAmps;
    bool v_on;
    bool v_charging;
    bool v_dc_charging;
    uint16_t iFrameCountCan1;
    uint16_t iFrameCountCan2;
    uint32_t iNegPowerCount;

//    OvmsMetricFloat *m_v_bat_socui;
    OvmsMetricFloat *m_v_bat_cool_in_temp;
    OvmsMetricFloat *m_v_bat_cool_out_temp;
    OvmsMetricString *m_v_env_gear_letter;
    OvmsMetricFloat *m_v_mot_cool_in_temp;
    OvmsMetricFloat *m_v_mot_cool_out_temp;
    OvmsMetricFloat *m_v_bat_energy_avail;
    OvmsMetricInt *m_v_bat_cool_pump_1;
    OvmsMetricInt *m_v_bat_cool_pump_2;
    OvmsMetricInt *m_v_mot_cool_pump;
    OvmsMetricInt *m_v_chg_pilot_cur;
  };

#endif //#ifndef __VEHICLE_TOYOTARAV4EV_H__
