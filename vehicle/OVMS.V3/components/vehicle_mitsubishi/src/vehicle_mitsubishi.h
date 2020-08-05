/*
;    Project:       Open Vehicle Monitor System
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011        Michael Stegen / Stegen Electronics
;    (C) 2011-2018   Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;    (C) 2018-2019   Tamás Kovács (KommyKT)
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

//void xmi_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_trip_since_parked(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_trip_since_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_vin(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
void xmi_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

class MI_Trip_Counter
	{
private:
  float odo_start;
  float odo;

public:
  MI_Trip_Counter();
  ~MI_Trip_Counter();
  void Reset(float odo);
  void Update(float odo);
  float GetDistance();
  bool Started();
	};

class OvmsVehicleMitsubishi : public OvmsVehicle
{
  public:
    OvmsVehicleMitsubishi();
    ~OvmsVehicleMitsubishi();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    char m_vin[18];

  protected:
    virtual void Ticker1(uint32_t ticker);
    void ConfigChanged(OvmsConfigParam* param);

  protected:

    OvmsCommand *cmd_xmi;

  public:
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);

    OvmsMetricFloat* v_b_power_min  = MyMetrics.InitFloat("xmi.b.power.min", 10, 0, kW);
    OvmsMetricFloat* v_b_power_max  = MyMetrics.InitFloat("xmi.b.power.max", 10, 0, kW);

    OvmsMetricBool*	ms_v_env_lowbeam = MyMetrics.InitBool("xmi.e.lowbeam", 10, 0);
    OvmsMetricBool*	ms_v_env_highbeam = MyMetrics.InitBool("xmi.e.highbeam", 10, 0);
    OvmsMetricBool*	ms_v_env_frontfog = MyMetrics.InitBool("xmi.e.frontfog", 10, 0);
    OvmsMetricBool*	ms_v_env_rearfog = MyMetrics.InitBool("xmi.e.rearfog", 10, 0);
    OvmsMetricBool*	ms_v_env_blinker_right = MyMetrics.InitBool("xmi.e.rightblinker", 10, 0);
    OvmsMetricBool* ms_v_env_blinker_left = MyMetrics.InitBool("xmi.e.leftblinker", 10, 0);
    OvmsMetricBool* ms_v_env_warninglight = MyMetrics.InitBool("xmi.e.warninglight", 10, 0);
    OvmsMetricFloat*  ms_v_charge_dc_kwh = MyMetrics.InitFloat("xmi.c.kwh.dc", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_charge_ac_kwh = MyMetrics.InitFloat("xmi.c.kwh.ac", 10, 0, kWh);
    OvmsMetricFloat*  v_c_efficiency = MyMetrics.InitFloat("xmi.c.efficiency", 10, 0, Percentage);
    OvmsMetricFloat*  v_c_power_ac = MyMetrics.InitFloat("xmi.c.power.ac", 10, 0, kW);
    OvmsMetricFloat*  v_c_power_dc = MyMetrics.InitFloat("xmi.c.power.dc", 10, 0, kW);
    OvmsMetricFloat*  v_c_time = MyMetrics.InitFloat("xmi.c.time", 10, 0, Seconds);
    OvmsMetricFloat*  v_c_soc_start = MyMetrics.InitFloat("xmi.c.soc.start", 10, 0, Percentage);
    OvmsMetricFloat*  v_c_soc_stop = MyMetrics.InitFloat("xmi.c.soc.stop", 10, 0, Percentage);

    OvmsMetricFloat*  ms_v_env_heating_amp = MyMetrics.InitFloat("xmi.e.heating.amp", 10, 0, Amps);
    OvmsMetricFloat*  ms_v_env_heating_watt  = MyMetrics.InitFloat("xmi.e.heating.watt", 10, 0, Watts);
    OvmsMetricFloat*  ms_v_env_heating_temp_return = MyMetrics.InitFloat("xmi.e.heating.temp.return", 10, 0, Celcius);
    OvmsMetricFloat*  ms_v_env_heating_temp_flow = MyMetrics.InitFloat("xmi.e.heating.temp.flow", 10, 0, Celcius);
    OvmsMetricFloat*  ms_v_env_ac_amp  = MyMetrics.InitFloat("xmi.e.ac.amp", 10, 0, Amps);
    OvmsMetricFloat*  ms_v_env_ac_watt = MyMetrics.InitFloat("xmi.e.ac.watt", 10, 0, Watts);

    OvmsMetricFloat*  ms_v_trip_consumption1 = MyMetrics.InitFloat("xmi.v.trip.consumption.KWh/100km", 10, 0, Other);
    OvmsMetricFloat*  ms_v_trip_consumption2 = MyMetrics.InitFloat("xmi.v.trip.consumption.km/kWh", 10, 0, Other);

    OvmsMetricFloat*  ms_v_pos_trip_park = MyMetrics.InitFloat("xmi.e.trip.park", 10, 0, Kilometers);
    OvmsMetricFloat*  ms_v_trip_park_energy_used = MyMetrics.InitFloat("xmi.e.trip.park.energy.used", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_park_energy_recd = MyMetrics.InitFloat("xmi.e.trip.park.energy.recuperated", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_park_heating_kwh = MyMetrics.InitFloat("xmi.e.trip.park.heating.kwh",10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_park_ac_kwh  = MyMetrics.InitFloat("xmi.e.trip.park.ac.kwh", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_park_soc_start = MyMetrics.InitFloat("xmi.e.trip.park.soc.start", 10, 0, Percentage);
    OvmsMetricFloat*  ms_v_trip_park_soc_stop = MyMetrics.InitFloat("xmi.e.trip.park.soc.stop", 10, 0, Percentage);
    OvmsMetricInt*    ms_v_trip_park_time_start = MyMetrics.InitInt("xmi.e.trip.park.time.start", 10, 0, Seconds);
    OvmsMetricInt*    ms_v_trip_park_time_stop = MyMetrics.InitInt("xmi.e.trip.park.time.stop", 10, 0, Seconds);

    OvmsMetricFloat*  ms_v_pos_trip_charge = MyMetrics.InitFloat("xmi.e.trip.charge", 10, 0, Kilometers);
    OvmsMetricFloat*  ms_v_trip_charge_energy_used = MyMetrics.InitFloat("xmi.e.trip.charge.energy.used", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_charge_energy_recd = MyMetrics.InitFloat("xmi.e.trip.charge.energy.recuperated", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_charge_heating_kwh = MyMetrics.InitFloat("xmi.e.trip.charge.heating.kwh", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_charge_ac_kwh  = MyMetrics.InitFloat("xmi.e.trip.charge.ac.kwh", 10, 0, kWh);
    OvmsMetricFloat*  ms_v_trip_charge_soc_start = MyMetrics.InitFloat("xmi.e.trip.charge.soc.start", 10, 0, Percentage);
    OvmsMetricFloat*  ms_v_trip_charge_soc_stop = MyMetrics.InitFloat("xmi.e.trip.charge.soc.stop", 10, 0, Percentage);

    OvmsMetricFloat* ms_v_trip_A = MyMetrics.InitFloat("xmi.e.trip.A", 10, 0, Kilometers);
    OvmsMetricFloat* ms_v_trip_B = MyMetrics.InitFloat("xmi.e.trip.B", 10, 0, Kilometers);

    OvmsMetricFloat* ms_v_bat_cac_rem = MyMetrics.InitFloat("xmi.v.bat.cac.rem", 10, 0, AmpHours);
    OvmsMetricFloat* ms_v_bat_max_input = MyMetrics.InitFloat("xmi.v.bat.max.input", 10, 0, kW);
    OvmsMetricFloat* ms_v_bat_max_output = MyMetrics.InitFloat("xmi.v.bat.max.output", 10, 0, kW);

    void vehicle_mitsubishi_car_on(bool isOn);

    int mi_start_time_utc;

    //config variables
    bool cfg_heater_old;
    unsigned char cfg_soh;
    unsigned char cfg_ideal;
    bool cfg_bms;
    bool cfg_newcell;

    //variables for QuickCharge
    bool mi_QC;

    //charge variables
    float mi_chargekwh;
    bool mi_SC;

    MI_Trip_Counter mi_park_trip_counter;
    MI_Trip_Counter mi_charge_trip_counter;

    float tripb = 0;

    bool has_odo;
    bool set_odo;

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
      // --------------------------------------------------------------------------
      // Webserver subsystem
      //  - implementation: mi_web.(h,cpp)
      //
    public:
      void WebInit();
      static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
      void GetDashboardConfig(DashboardConfig& cfg);
  #endif //CONFIG_OVMS_COMP_WEBSERVER

};

#define POS_ODO			StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers)
#endif //#ifndef __VEHICLE_MITSUBISHI_H__
