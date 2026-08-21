/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
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

#ifndef __VEHICLE_VOLTAMPERA_H__
#define __VEHICLE_VOLTAMPERA_H__

#include "vehicle.h"

using namespace std;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#include "swcan.h"
#endif

// Shared with the web config form, which stringizes it with STR() to fill in the field
// default. It has to be visible to va_web.cpp: with no definition in scope STR() yields the
// identifier itself and the form offers a non-numeric default that browsers render as an
// empty box.
#define VA_PREHEAT_MAX_TIME_DEFAULT 20 // minutes

class OvmsVehicleVoltAmpera : public OvmsVehicle
  {
  public:
    OvmsVehicleVoltAmpera();
    ~OvmsVehicleVoltAmpera();

  public:
    void Status(int verbosity, OvmsWriter* writer) override;
    void ConfigChanged(OvmsConfigParam* param) override;

    typedef enum
      {
      Park = 1,
      Left_rear_signal = 2,
      Right_rear_signal = 4,
      Driver_front_signal = 8,
      Passenger_front_signal = 16,
      Center_stop_lamp = 32,
      Charging_indicator = 64,
      Interior_lamp = 128
      } va_light_t;

    typedef enum
      {
        Disabled,           Fob,        // preheat is activated via key fob and BCM controls it. OVMS just follows the procedure passively
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
    void IncomingDriveCycleSWCAN(CAN_frame_t* p_frame);   // PID-matched drive-cycle broadcasts
    void DriveCycleAccumulate();    // OVMS's own since-last-charge tally, called from Ticker1

    float m_dc_last_odo = 0;        // odometer at the previous accumulate tick
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
    void UpdateBatteryPower();
    void UpdateMotorTemp();         // v.m.temp = hotter of the two motor-generators

  public:
    vehicle_command_t CommandStartCharge() override;
    vehicle_command_t CommandStopCharge() override;

  protected:
    // SOC charge limit. The Volt has no single-shot "stop charge", so the HPCM is flipped
    // into departure-based charging with a far target, re-applied whenever charging resumes.
    bool GmlanWrite(canbus* bus, uint32_t txid, const uint8_t* payload, uint8_t len);
    // GMLAN mode $AE DeviceControl, preceded by a 3E TesterPresent to the same node.
    // Bus differs per node, see the TesterPresent survey above GmlanDeviceControl().
    bool GmlanDeviceControl(canbus* bus, uint32_t txid, const uint8_t* payload, uint8_t len);

  public:
    // Physical actuator controls. All gated on config xva/control.enabled, which defaults
    // to false. GMLAN $AE DeviceControl only holds while the
    // diagnostic session is kept alive (~5s GM timeout), so the two forced states run a
    // keep-alive ticker; AUTO stops it and explicitly releases.
    typedef enum
      {
      VA_ENG_AUTO = 0,      // no override, HPCM in control
      VA_ENG_FORCE_ON,
      VA_ENG_FORCE_OFF,
      } va_engine_mode_t;

    vehicle_command_t CommandEngine(va_engine_mode_t mode, const char* pin = NULL);
    vehicle_command_t CommandTrunk(const char* pin);
    // OnStar telematics alerts on 0x1024E097. horn = EnhSrvAudAlRq, lights = EnhSrvVisAlRq.
    vehicle_command_t CommandTelematicsAlert(bool horn, bool lights);
    vehicle_command_t CommandWindows(bool up, const char* pin);

    static OvmsVehicleVoltAmpera* GetActiveVehicle(OvmsWriter* writer);
    static void shell_engine(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_trunk(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_alert(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    void SetCmdPending(const char* what, OvmsMetric* watch);
    static void shell_windows(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_chargelimit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  protected:
    bool m_control_enabled;         // config xva/control.enabled
    OvmsCommand* cmd_xva;

    va_engine_mode_t m_engine_mode;
    int m_engine_ka_timer;          // Ticker1 countdown to the next keep-alive burst
    void EngineAssert();            // (re)send the AE frame for the current forced mode
    void EngineTicker();            // keep-alive, called from Ticker1
    float ChargeLimitSoc();         // SOC on the scale the limit is set against
    bool m_chargelimit_use_raw;     // config xva/chargelimit.source, default raw
    bool m_chargelimit_scale_warned;// one-shot log when that scale is unavailable
    bool m_soc_scale_warned;        // ditto for soc.source
    bool m_defer_manual;            // the active defer is the owner's, do not auto-resume it
    bool ChargeLimitLocationOk();   // true if the geofence permits enforcing right now
    bool ChargeLimitInLocation();   // raw "is the car inside the configured location"
    bool ChargeDeferBegin();        // starts the defer sequence (returns false if refused)
    bool ChargeStartBegin();        // starts the resume sequence
    void ChargeSeqTicker();         // steps the sequence, called from Ticker1
    void ChargeLimitTicker();       // enforcement loop, called from Ticker10
    void ChargePollTicker();        // silent-charge poll state 3, called from Ticker1
    void SetLimitOverride(bool on); // sets m_limit_override and mirrors it to the metric
    void UpdateRangeTotal();        // xva.v.range.total = v.b.range.est + xva.v.e.range.fuel
    void UpdateBatteryCapacity();   // publishes v.b.capacity from config or model year
    void UpdateSoc();               // points v.b.soc at the configured scale
    float SocRaw();                 // true pack SOC; use for every safety decision
    bool m_soc_use_raw;             // config xva/soc.source
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
    void NotifyChargeState() override;

    void PollRunFinished(canbus* bus) override;
  protected:
    char m_vin[18];
    char m_type[6];
    int m_modelyear;
    unsigned int m_charge_timer;
    unsigned int m_charge_wm;
    unsigned int m_candata_timer;
    unsigned int m_range_rated_km;
    unsigned int m_startPolling_timer;
    // Silent-charge polling (poll state 3), see ChargePollTicker():
    unsigned int m_silent_charge_timer;    // seconds 12V has been above the probe threshold
    unsigned int m_silent_charge_backoff;  // seconds to hold off after an unanswered probe
    uint32_t m_last_poll_reply;            // monotonictime of the last IncomingPollReply
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
    bool m_use_swcan_adapter;
    // Set once the final cell of a state-2 sweep has been read. Guards against the poller
    // firing PollRunFinished() before the state just entered has sent anything (see
    // PollRunFinished).
    bool m_state2_swept;
    // 0x41a3 and 0x45ff both carry pack capacity with different scaling; once 0x41a3 has
    // answered it owns the value, so a later 0x45ff reply cannot overwrite it.
    bool m_cac_from_41a3;

    // --- SOC charge limit (see ChargeLimitTicker) -------------------------------------
    // Config, namespace "xva":
    bool m_chargelimit_enabled;     // chargelimit.enabled  master gate for ALL write frames
    int  m_chargelimit_soc;         // chargelimit.soc      target %, 0 or >=100 disables
    int  m_chargelimit_maxdefer;    // chargelimit.maxdefer attempts per plug-in session
    bool m_chargelimit_notify;      // chargelimit.notify
    bool m_chargelimit_debug;       // chargelimit.debug    log every frame
    // chargelimit.location: name of an OVMS location (Config > Locations). Empty = limit
    // everywhere. When set, the limit only applies inside that geofence, so charging away
    // from home is never interfered with. The master toggle above still wins either way.
    std::string m_chargelimit_location;
    // One-off "charge to full this time" override, e.g. before a long trip. Not persisted:
    // it clears itself when the car leaves the configured location, so the limit comes back
    // automatically on the next return home.
    bool m_limit_override;
    float m_override_odo;           // odometer (km) when the override was set
    bool m_was_in_location;         // geofence edge, fallback when no odometer

    // Deferred-charge sequence state machine. The GMLAN writes need 500-1000ms between
    // frames; rather than blocking a ticker task with vTaskDelay the sequence steps forward
    // from Ticker1(). See ChargeSeqTicker().
    typedef enum
      {
      VA_CHGSEQ_IDLE = 0,
      VA_CHGSEQ_WAKE_DEFER,     // waiting out the wakeup before the defer macro
      VA_CHGSEQ_WAKE_START,     // waiting out the wakeup before the resume pair
      // Defer macro, one frame per Ticker1 step, mirroring the Voltage app's g(23,45).
      // A bare mode+time write is ACKed but ignored (dash keeps showing Immediate); the
      // spoofed-noon clock plus the immediate->departure toggle is what makes it stick.
      VA_CHGSEQ_DEFER_CLKH,     // 3B 30 0C @0x244 SWCAN  spoof car clock hour to 12
      VA_CHGSEQ_DEFER_CLKM,     // 3B 31 00 @0x244 SWCAN  spoof car clock minute to 00
      VA_CHGSEQ_DEFER_F1,       // 3B 77 02 00 00 @0x7E4  clear departure time
      VA_CHGSEQ_DEFER_F2,       // 3B 76 01 01    @0x7E4  mode -> immediate (forces replan)
      VA_CHGSEQ_DEFER_H1,       // 3B 76 02 01    @0x7E4  mode -> departure/rate-based
      VA_CHGSEQ_DEFER_H2,       // 3B 77 02 5F 00 @0x7E4  departure 23:45, ~12h from fake noon
      VA_CHGSEQ_DEFER_RESTH,    // restore real clock hour (skipped when not spoofing)
      VA_CHGSEQ_DEFER_RESTM,    // restore real clock minute, then session bookkeeping
      VA_CHGSEQ_START_1,        // sent 3B 77 02 00 00, waiting to send 3B 76 01 01
      } va_chargeseq_t;
    va_chargeseq_t m_chargeseq;
    int  m_chargeseq_timer;         // Ticker1 counts remaining before next frame
    bool m_chargeseq_spoof;         // clock spoof in use for the pending defer (needs SWCAN + valid clock)
    int  m_defer_count;             // defers issued this plug-in session
    bool m_defer_active;            // a defer is believed to be currently applied
    bool m_defer_unclear;           // we deferred and have not put the car back to
                                    // immediate yet; persisted, see ChargeLimitTicker

    OvmsMetricInt * mt_preheat_status;
    OvmsMetricInt *  mt_preheat_timer;
    OvmsMetricBool * mt_ac_active;
    OvmsMetricInt *  mt_ac_front_blower_fan_speed;  // %
    OvmsMetricFloat *  mt_coolant_heater_pwr;       // kW
    OvmsMetricInt *  mt_coolant_temp;
    OvmsMetricVector<int> * mt_charging_limits;
    OvmsMetricInt *  mt_fuel_level;                 // %
    OvmsMetricFloat *  mt_v_trip_ev;                // km
    // Decoded from SWCAN using GM message database doc 1818125 (see IncomingFrameCan4)
    OvmsMetricFloat *  mt_v_12v_voltage;            // V, 12V battery at the IBS
    OvmsMetricFloat *  mt_v_12v_soc;                // %, 12V battery
    OvmsMetricFloat *  mt_hv_power_disp;            // kW, pack power as the cluster shows it
    OvmsMetricFloat *  mt_chargecycle_econ;         // km/l equivalent since last charge
    OvmsMetricFloat *  mt_ac_evap_temp;             // degC, evaporator outlet air
    OvmsMetricInt *    mt_ac_compressor_rpm;        // rpm
    OvmsMetricFloat *  mt_heatercore_temp;          // degC, cabin heat loop
    OvmsMetricFloat *  mt_fuel_used;                // liters, this drive cycle
    OvmsMetricFloat *  mt_bat_heater_pct;           // %, HV battery heater duty
    OvmsMetricFloat *  mt_bat_heater_pwr;           // HV battery heater power
    OvmsMetricFloat *  mt_mot_temp_mga;             // degC, motor-generator A
    OvmsMetricFloat *  mt_mot_temp_mgb;             // degC, motor-generator B
    OvmsMetricString*  mt_charge_level;             // L1 (120V) / L2 (240V) / unplugged
    OvmsMetricBool*    mt_charge_deferred;          // HPCM is in departure mode (read via 1A 76)
    OvmsMetricFloat*   mt_soc_displayed;            // dashboard SOC, 8 bit (PID 0x8334)
    OvmsMetricFloat*   mt_soc_raw;                  // true pack SOC, 16 bit (PID 0x43af)
    OvmsMetricBool*    mt_limit_override;           // charge to full this once
    OvmsMetricFloat*   mt_range_fuel;               // km left on gasoline (0x224)
    OvmsMetricFloat*   mt_range_total;              // EV + gasoline, what the dash totals
    OvmsMetricFloat*   mt_window_drv;               // window positions, 0 shut .. 100 open
    OvmsMetricFloat*   mt_window_pass;
    OvmsMetricFloat*   mt_window_lr;
    OvmsMetricFloat*   mt_window_rr;
    OvmsMetricString*  mt_windows;                  // open/closed/opening/closing
    OvmsMetricString*  mt_cmd_pending;              // command in flight, for a busy indicator
    OvmsMetric*        m_pending_watch = NULL;      // metric whose change means it landed
    std::string        m_pending_seen;              // that metric's value at command start
    int                m_pending_secs = 0;          // countdown before giving up on it
    int m_window_last = -1;                         // last aggregate position, for direction
    int m_window_settle = 0;
    int m_window_wake = 0;          // seconds left holding the bus awake after a window move
    int m_window_raw[4] = { -1, -1, -1, -1 };  // last raw 0x325 field per window
    int m_swcan_live = 0;           // seconds since a SWCAN frame actually arrived
    int m_window_pending = 0;       // ticks left waiting for the bus before commanding
    int m_window_cmd_left = 0;      // window command re-sends left, see CommandWindows
    uint8_t m_window_cmd_dir = 0;   // direction being re-sent (0x01 down, 0x02 up)

    // Drive-cycle ("since last charge") broadcasts, PIDs 0x0141 / 0x0210 / 0x0225 / 0x0223
    OvmsMetricFloat*   mt_dc_energy_used;           // kWh drawn from the pack this cycle
    OvmsMetricInt*     mt_charge_inhibit;           // HVChrgInhbRsn, why charging is blocked
    OvmsMetricFloat*   mt_dc_pct1;
    OvmsMetricFloat*   mt_dc_pct2;
    OvmsMetricFloat*   mt_dc_pct3;
    OvmsMetricFloat*   mt_dc_pct4;
    OvmsMetricFloat*   mt_dc_dist_batt;
    OvmsMetricFloat*   mt_dc_dist_fuel;
    OvmsMetricFloat*   mt_dc_dist_total;
    OvmsMetricFloat*   mt_dc_batt_ratio;
    OvmsMetricFloat*   mt_dc_eff_batt;
    OvmsMetricFloat*   mt_dc_eff_cabin;
    OvmsMetricFloat*   mt_dc_eff_drive;
    OvmsMetricFloat*   mt_dc_eff_total;
    OvmsMetricFloat*   mt_dc_fuel_used;
    OvmsMetricFloat*   mt_dc_fuel_econ;             // gas-only, L/100km
    OvmsMetricFloat*   mt_oil_life;                 // READ ONLY, no reset implemented
    OvmsMetricFloat*   mt_dc_energy_own;            // OVMS's own tally, independent of the car
    OvmsMetricFloat*   mt_dc_distance_own;
    OvmsMetricFloat*   mt_charge_input;             // AC input, last/current charge (0x437d)
    OvmsMetricFloat*   mt_charge_lifetime;          // lifetime charge energy (0x4389)
    OvmsMetricString*  mt_engine_mode;              // auto | forced-on | forced-off

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
    static void WebControls(PageEntry_t& p, PageContext_t& c);

#endif //CONFIG_OVMS_COMP_WEBSERVER

  };

#endif //#ifndef __VEHICLE_VOLTAMPERA_H__
