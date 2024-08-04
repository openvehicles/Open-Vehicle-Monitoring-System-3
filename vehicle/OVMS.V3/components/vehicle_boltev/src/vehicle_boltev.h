/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2022       Alexander von Gluck IV
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

#ifndef __VEHICLE_BOLTEV_H__
#define __VEHICLE_BOLTEV_H__

#include "vehicle.h"

using namespace std;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#include "swcan.h"
#endif

class OvmsVehicleBoltEV : public OvmsVehicle
{
public:
    OvmsVehicleBoltEV();
    ~OvmsVehicleBoltEV();

public:
    void Status(int verbosity, OvmsWriter* writer) override;
    void ConfigChanged(OvmsConfigParam* param) override;

    typedef enum {
        Park = 1,
        Left_rear_signal = 2,
        Right_rear_signal = 4,
        Driver_front_signal = 8,
        Passenger_front_signal = 16,
        Center_stop_lamp = 32,
        Charging_indicator = 64,
        Interior_lamp = 128
    } va_light_t;

    typedef enum {
        Disabled,   // preheat disabled
        Fob,        // preheat is activated via key fob and BCM controls it. OVMS just follows the procedure passively
        Onstar,     // Onstar emulation. OVMS sends the Start/Stop commands but BCM controls the actual preheating
        OVMS        // OVMS takes care of everything (starting, stopping, lights)
    } va_preheat_commander_t;

    vehicle_command_t CommandWakeup() override;
    vehicle_command_t CommandClimateControl(bool enable) override;
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandHomelink(int button, int durationms=1000) override;
    vehicle_command_t CommandLights(va_light_t lights, bool turn_on);
    vehicle_command_t CommandSetChargeCurrent(uint16_t limit) override;
    void FlashLights(va_light_t light, int interval=500, int count=1); // milliseconds

protected:
    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    void IncomingFrameCan2(CAN_frame_t* p_frame) override;
    void IncomingFrameCan3(CAN_frame_t* p_frame) override;
    void IncomingFrameCan4(CAN_frame_t* p_frame) override;
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
    void TxCallback(const CAN_frame_t* p_frame, bool success);
    void CommandWakeupComplete(const CAN_frame_t* p_frame, bool success);
    void SendTesterPresentMessage( uint32_t id );
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;
    void Ticker300(uint32_t ticker) override;
    void NotifiedVehicleOn() override;
    void NotifiedVehicleOff() override;
    void NotifiedVehicleAwake() override;

    // va_ac_preheat
    void ClimateControlInit();
    void ClimateControlPrintStatus(int verbosity, OvmsWriter* writer);
    void ClimateControlIncomingSWCAN(CAN_frame_t* p_frame);
    void AirConStatusUpdated( bool ac_enabled );
    const char * PreheatStatus();
    void PreheatModeChange( uint8_t preheat_status );
    void PreheatWatchdog();

    //va_notify
    void NotifyFuel();
    void NotifyMetrics();

    void PollRunFinished(canbus *bus) override;
protected:
    char m_vin[18];
    char m_type[6];
    int m_modelyear;
    unsigned int m_charge_timer;
    unsigned int m_charge_wm;
    unsigned int m_candata_timer;
    unsigned int m_range_rated_km;
    unsigned int m_startPolling_timer;
    OvmsPoller::poll_pid_t * m_pPollingList;

    canbus* p_swcan;    // Either "can4" or "can3" bus, depending on which is connected to slow speed GMLAN bus
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    swcan* p_swcan_if;  // Actual SWCAN interface with facilities to switch between normal and HVWUP modes
#endif

    CanFrameCallback wakeup_frame_sent;
    unsigned int m_tx_retry_counter;

    // Bitwise status of the lights that we control. Used for establishing if sending periodic Tester Present messages is required
    uint32_t m_controlled_lights;
    uint32_t m_tester_present_timer;
    bool m_extended_wakeup;
    bool m_notify_metrics;

    OvmsMetricInt * mt_preheat_status;
    OvmsMetricInt *  mt_preheat_timer;
    OvmsMetricBool * mt_ac_active;
    OvmsMetricInt *  mt_ac_front_blower_fan_speed;  // %
    OvmsMetricFloat *  mt_coolant_heater_pwr;       // kW
    OvmsMetricInt *  mt_coolant_temp;
    OvmsMetricVector<int> * mt_charging_limits;
    OvmsMetricInt *  mt_fuel_level;                 // %
    OvmsMetricFloat *  mt_v_trip_ev;                // km

    unsigned long m_preheat_modechange_timer;
    va_preheat_commander_t m_preheat_commander;
    unsigned int m_preheat_retry_counter;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: va_web.(h,cpp)
    //

public:
    void WebInit();
    void WebCleanup();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);

#endif //CONFIG_OVMS_COMP_WEBSERVER

};

#endif //#ifndef __VEHICLE_VOLTAMPERA_H__
