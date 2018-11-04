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
#include "ovms_webserver.h"

using namespace std;

void CommandBatteryReset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

class OvmsVehicleMitsubishi : public OvmsVehicle
  {
  public:
    OvmsVehicleMitsubishi();
    ~OvmsVehicleMitsubishi();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);

  protected:
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();

  protected:
    char m_vin[18];
    unsigned int m_charge_watt;
    OvmsCommand *cmd_xmi;

  public:
    void BatteryReset();

    vehicle_command_t CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

    virtual vehicle_command_t CommandSetChargeMode(vehicle_mode_t mode);
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStartCharge();
    virtual vehicle_command_t CommandStopCharge();
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart);
    virtual vehicle_command_t CommandCooldown(bool cooldownon);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    OvmsMetricVector<float>* cell_volts_act = new OvmsMetricVector<float>("xmi.b.cell.volts.act", SM_STALE_MIN, Volts);
    OvmsMetricVector<float>* cell_volts_min = new OvmsMetricVector<float>("xmi.b.cell.volts.min", SM_STALE_MIN, Volts);
    OvmsMetricVector<float>* cell_volts_max = new OvmsMetricVector<float>("xmi.b.cell.volts.max", SM_STALE_MIN, Volts);
    OvmsMetricVector<float>* cell_volts_maxdev = new OvmsMetricVector<float>("xmi.b.cell.volts.maxdev", SM_STALE_MIN, Volts);

    OvmsMetricVector<float>* cell_temps_act = new OvmsMetricVector<float>("xmi.b.cell.temps.act", SM_STALE_MIN, Degrees);
    OvmsMetricVector<float>* cell_temps_min = new OvmsMetricVector<float>("xmi.b.cell.temps.min", SM_STALE_MIN, Degrees);
    OvmsMetricVector<float>* cell_temps_max = new OvmsMetricVector<float>("xmi.b.cell.temps.max", SM_STALE_MIN, Degrees);
    OvmsMetricVector<float>* cell_temps_maxdev = new OvmsMetricVector<float>("xmi.b.cell.temps.maxdev", SM_STALE_MIN, Degrees);

    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: mi_web.(h,cpp)
    //
  public:
    void WebInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    static void WebBattMon(PageEntry_t& p, PageContext_t& c);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);


  };

#endif //#ifndef __VEHICLE_MITSUBISHI_H__
