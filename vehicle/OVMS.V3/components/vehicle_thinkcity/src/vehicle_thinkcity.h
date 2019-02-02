/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
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

#ifndef __VEHICLE_THINKCITY_H__
#define __VEHICLE_THINKCITY_H__

#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

class OvmsVehicleThinkCity : public OvmsVehicle
  {
  public:
    OvmsVehicleThinkCity();
    ~OvmsVehicleThinkCity();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
 
    vehicle_command_t CommandLock(const char* pin);
    vehicle_command_t CommandUnlock(const char* pin);
    vehicle_command_t CommandActivateValet(const char* pin);
    vehicle_command_t CommandDeactivateValet(const char* pin);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: ks_web.(h,cpp)
  //

  public:
    void WebInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    static void WebBattMon(PageEntry_t& p, PageContext_t& c);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);

#endif //CONFIG_OVMS_COMP_WEBSERVER

  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);

  private:
    
    void LogValues();

    float         tc_pack_voltage;
    float         tc_pack_current;
    float  tc_pack_maxchgcurr;
    float  tc_pack_maxchgvolt;
    unsigned int  tc_pack_failedcells;
    float    tc_pack_temp1;
    float    tc_pack_temp2;
    unsigned int  tc_pack_batteriesavail;
    unsigned int  tc_pack_rednumbatteries;
    float  tc_pack_mindchgvolt;
    float  tc_pack_maxdchgamps;
    signed int    tc_charger_temp = 0;
    signed int    tc_slibatt_temp = 0;
    float  tc_charger_pwm;
    float  tc_sys_voltmaxgen;
    unsigned char tc_srs_stat;
    unsigned int  tc_srs_nr_err;
    unsigned long tc_srs_tm;
    signed int    tc_heater_count = 0;
    float         tc_AC_volt; // ac line voltage
    float         tc_AC_current; // ac current
    float         tc_pack_soc;
    
    //Status flags:
    unsigned int tc_bit_eoc;
    unsigned int tc_bit_socgreater102;
    unsigned int tc_bit_chrgen;
    unsigned int tc_bit_ocvmeas;
    unsigned int tc_bit_mainsacdet;
    unsigned int tc_bit_syschgenbl;
    unsigned int tc_bit_fastchgenbl;
    unsigned int tc_bit_dischgenbl;
    unsigned int tc_bit_isotestinprog;
    unsigned int tc_bit_acheatrelaystat;
    unsigned int tc_bit_acheatswitchstat;
    unsigned int tc_bit_regenbrkenbl;
    unsigned int tc_bit_dcdcenbl;
    unsigned int tc_bit_fanactive;

    //Errors:
    unsigned int tc_bit_epoemerg;
    unsigned int tc_bit_crash;
    unsigned int tc_bit_generalerr;
    unsigned int tc_bit_intisoerr;
    unsigned int tc_bit_extisoerr;
    unsigned int tc_bit_thermalisoerr;
    unsigned int tc_bit_isoerr;
    unsigned int tc_bit_manyfailedcells;

    //Notifications:
    unsigned int tc_bit_chgwaittemp;
    unsigned int tc_bit_reacheocplease;
    unsigned int tc_bit_waitoktmpdisch;
    unsigned int tc_bit_chgwaitttemp2;

    //Warnings:
    unsigned int tc_bit_chgcurr;
    unsigned int tc_bit_chgovervolt;
    unsigned int tc_bit_chgovercurr;

    unsigned int m_candata_timer;

  };

#endif //#ifndef __VEHICLE_THINKCITY_H__
