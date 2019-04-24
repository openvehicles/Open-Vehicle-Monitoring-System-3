/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 */

#ifndef __VEHICLE_SMARTED_H__
#define __VEHICLE_SMARTED_H__

#include "vehicle.h"
#include "freertos/timers.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

class OvmsVehicleSmartED : public OvmsVehicle
{
  public:
    OvmsVehicleSmartED();
    ~OvmsVehicleSmartED();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    char m_vin[17];

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);

  public:
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint32_t timerstart);
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    
  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    void GetDashboardConfig(DashboardConfig& cfg);
    TimerHandle_t m_locking_timer;
    
    OvmsMetricInt *mt_vehicle_time;         // vehicle time
    OvmsMetricInt *mt_trip_start;           // trip since start
    OvmsMetricInt *mt_trip_reset;           // trip since Reset
    OvmsMetricBool *mt_hv_active;           // HV active

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll = 0;
    unsigned int m_egpio_timer = 0;

  protected:
    int m_doorlock_port;                    // … MAX7317 output port number (2…9, default 2 = EGPIO_1)
    int m_doorunlock_port;                  // … MAX7317 output port number (2…9, default 3 = EGPIO_2)
    int m_ignition_port;                    // … MAX7317 output port number (2…9, default 4 = EGPIO_3)
    
};

#endif //#ifndef __VEHICLE_SMARTED_H__
