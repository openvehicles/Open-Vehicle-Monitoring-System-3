/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th Sep 2019
;
;    Changes:
;    0.1.0  Initial release
;           - fork of vehicle_demo
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo 
;    (C) 2019       Thomas Heuer @Dimitrie78
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

#ifndef __VEHICLE_RENAULTZOE_H__
#define __VEHICLE_RENAULTZOE_H__

#include <atomic>

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "freertos/timers.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (data[b] & 0x0f)
#define CAN_NIBH(b)     (data[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

#define POLLSTATE_OFF					PollSetState(0);
#define POLLSTATE_ON					PollSetState(1);
#define POLLSTATE_RUNNING			PollSetState(2);
#define POLLSTATE_CHARGING		PollSetState(3);

#define RZ_CANDATA_TIMEOUT 10

using namespace std;

class OvmsVehicleRenaultZoe : public OvmsVehicle {

  public:
    OvmsVehicleRenaultZoe();
    ~OvmsVehicleRenaultZoe();
		static OvmsVehicleRenaultZoe* GetInstance(OvmsWriter* writer=NULL);
		void IncomingFrameCan1(CAN_frame_t* p_frame);
		void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);

	protected:
		void IncomingEPS(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingEVC(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingBCB(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingLBC(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingUBP(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void IncomingPEB(uint16_t type, uint16_t pid, const char* data, uint16_t len);
		void car_on(bool isOn);
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    void HandleCharging();
    void HandleEnergy();
    int calcMinutesRemaining(float target, float charge_voltage, float charge_current);
    
		OvmsCommand *cmd_zoe;
		
    // Renault ZOE specific metrics
    OvmsMetricFloat  *mt_pos_odometer_start;  // ODOmeter at Start
    OvmsMetricBool   *mt_bus_awake;           // can-bus awake status
    OvmsMetricFloat  *mt_available_energy;    // Available Energy

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);
  
  public:
    //virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    //virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandTrip(int verbosity, OvmsWriter* writer);
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
		void NotifyTrip();

  protected:
    bool m_enable_write;                    // canwrite
    char zoe_vin[8] = "";                   // last 7 digits of full VIN
    int m_range_ideal;                      // … Range Ideal (default 160 km)
    int m_battery_capacity;                 // Battery Capacity (default 27000)
    bool m_enable_egpio;                    // enable EGPIO for Homelink commands
    bool m_reset_trip;                      // Reset trip when charging else when env on
    int m_reboot_ticker;

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll;
  
  public:
		static void zoe_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
  
  protected:
    string zoe_obd_rxbuf;
};

#endif //#ifndef __VEHICLE_RENAUTZOE_H__
