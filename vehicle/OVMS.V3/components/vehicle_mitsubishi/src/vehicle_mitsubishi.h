/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th Sept 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;    (C) 2018       Tamás Kovács
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

#ifndef __VEHICLE_MITSUBISHI_H__
#define __VEHICLE_MITSUBISHI_H__

#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

void xmi_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

class OvmsVehicleMitsubishi : public OvmsVehicle
  {
  public:
    OvmsVehicleMitsubishi();
    ~OvmsVehicleMitsubishi();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
    virtual void Ticker1(uint32_t ticker);
    void ConfigChanged(OvmsConfigParam* param);

  protected:
    char m_vin[18];
    OvmsCommand *cmd_xmi;

  public:
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);

    OvmsMetricFloat* v_b_power_min  = new OvmsMetricFloat("xmi.b.power.min", SM_STALE_MID, kW);
    OvmsMetricFloat* v_b_power_max  = new OvmsMetricFloat("xmi.b.power.max", SM_STALE_MID, kW);

    OvmsMetricBool*	m_v_env_lowbeam = MyMetrics.InitBool("xmi.e.lowbeam", 10, 0);
    OvmsMetricBool*	m_v_env_highbeam = MyMetrics.InitBool("xmi.e.highbeam", 10, 0);
    OvmsMetricBool*	m_v_env_frontfog = MyMetrics.InitBool("xmi.e.frontfog", 10, 0);
    OvmsMetricBool*	m_v_env_rearfog = MyMetrics.InitBool("xmi.e.rearfog", 10, 0);
    OvmsMetricBool*	m_v_env_blinker_right = MyMetrics.InitBool("xmi.e.rightblinker", 10, 0);
    OvmsMetricBool* m_v_env_blinker_left = MyMetrics.InitBool("xmi.e.leftblinker", 10, 0);
    OvmsMetricBool* m_v_env_warninglight = MyMetrics.InitBool("xmi.e.warninglight", 10, 0);
    OvmsMetricFloat*  m_v_charge_dc_kwh = MyMetrics.InitFloat("v.c.kwh.dc",SM_STALE_MID, kWh);
    OvmsMetricFloat*  m_v_charge_ac_kwh = MyMetrics.InitFloat("v.c.kwh.ac",SM_STALE_MID, kWh);
    OvmsMetricFloat*  v_c_efficiency = MyMetrics.InitFloat("v.c.efficiency",SM_STALE_HIGH, Percentage);
    OvmsMetricFloat*  v_c_power_ac = MyMetrics.InitFloat("v.c.power.ac",SM_STALE_MID, kW);
    OvmsMetricFloat*  v_c_power_dc = MyMetrics.InitFloat("v.c.power.dc",SM_STALE_MID, kW);

    OvmsMetricFloat*  m_v_env_heating_amp  = new OvmsMetricFloat("xmi.e.heating.amp", SM_STALE_MID, Amps);
    OvmsMetricFloat*  m_v_env_heating_watt  = new OvmsMetricFloat("xmi.e.heating.watt", SM_STALE_MID, Watts);
    OvmsMetricFloat*  m_v_env_heating_kwh = new OvmsMetricFloat("xmi.e.heating.kwh", SM_STALE_MID, kWh);
    OvmsMetricFloat*  m_v_env_heating_temp_return  = new OvmsMetricFloat("xmi.e.heating.temp.return", SM_STALE_MID, Celcius);
    OvmsMetricFloat*  m_v_env_heating_temp_flow  = new OvmsMetricFloat("xmi.e.heating.temp.flow", SM_STALE_MID, Celcius);
    OvmsMetricFloat*  m_v_env_ac_amp  = new OvmsMetricFloat("xmi.e.ac.amp", SM_STALE_MID, Amps);
    OvmsMetricFloat*  m_v_env_ac_watt  = new OvmsMetricFloat("xmi.e.ac.watt", SM_STALE_MID, Watts);
    OvmsMetricFloat*  m_v_env_ac_kwh = new OvmsMetricFloat("xmi.e.ac.kwh", SM_STALE_MID, kWh);
    OvmsMetricFloat*  m_v_trip_consumption1 = MyMetrics.InitFloat("xmi.v.trip.consumption.KWh/100km", 10, 0, Other);
    OvmsMetricFloat*  m_v_trip_consumption2 = MyMetrics.InitFloat("xmi.v.trip.consumption.km/kWh", 10, 0, Other);

    void vehicle_mitsubishi_car_on(bool isOn);

    float mi_trip_start_odo;
    int mi_start_time_utc;
    float mi_start_cdc;
    float mi_start_cc;
    //config variables
    bool cfg_heater_old;
    unsigned char  cfg_soh;
    //variables for QuickCharge
    unsigned int mi_est_range;
    unsigned char mi_QC;
    unsigned char mi_QC_counter;
    unsigned char mi_last_good_SOC;
    unsigned char mi_last_good_range;
    //charge variables
    float mi_chargekwh;


#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: mi_web.(h,cpp)
    //
  public:
    void WebInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);

#endif //CONFIG_OVMS_COMP_WEBSERVER

  };

#define POS_ODO			StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers)
#endif //#ifndef __VEHICLE_MITSUBISHI_H__
