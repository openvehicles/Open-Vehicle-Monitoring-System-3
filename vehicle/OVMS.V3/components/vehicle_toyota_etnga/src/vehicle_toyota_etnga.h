/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef __VEHICLE_TOYOTA_ETNGA_H__
#define __VEHICLE_TOYOTA_ETNGA_H__

#include <string>
#include <vector>
#include <utility>
#include <atomic>
#include <map>
#include <iosfwd>
#include <time.h>
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// Fraction of full-charge capacity the driver can actually reach, used to convert the
// full-charge Ah of 0x1D3E into the USABLE kWh that v.b.capacity is defined as.
// Display SOC spans BMS SOC [4.19, 93.92] -> [0, 100] (least-squares fit over 8,901
// unclamped display/BMS sample pairs), so 89.73% of full charge is reachable. Sanity check:
// 197.9 Ah x 0.8973 x 355 V = 63.0 kWh against ~64 kWh advertised usable / 71.4 kWh gross.
#define PACK_USABLE_FRACTION 0.8973f

// Control states
enum ControlState {
    CS_NONE = 0,
    CS_DRIVING = 1,
    CS_CHARGING = 3
};

// Poll states
enum PollState : int
{
    SLEEP            = 0,
    AWAKE            = 1,
    DRIVING          = 2,
    CHARGE_HANDSHAKE = 3,  // was CHARGING; cable-negotiation fast-poll window
    CHARGE_WAIT      = 4,  // plugged in, not (yet/any longer) charging — sparse
    CHARGE_AC        = 5,
    CHARGE_DC        = 6,
};

class OvmsVehicleToyotaETNGA : public OvmsVehicle
{
public:
    OvmsVehicleToyotaETNGA();
    ~OvmsVehicleToyotaETNGA();

    void Ticker1(uint32_t ticker) override;

    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
    void IncomingPollError(const OvmsPoller::poll_job_t &job, int32_t code) override;

    void IncomingFrameCan2(CAN_frame_t* p_frame) override;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // Webserver subsystem (implementation: etnga_web.cpp)
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebDispChgMetrics(PageEntry_t& p, PageContext_t& c);
    static void WebChgRenderAc(PageContext_t& c, OvmsVehicleToyotaETNGA* v);   // AC charging panels
    static void WebChgRenderDc(PageContext_t& c, OvmsVehicleToyotaETNGA* v);   // DC charging panels
    static void WebChgChartJs(PageContext_t& c, OvmsVehicleToyotaETNGA* v, bool dc);          // live chart
    static void WebChgStateHistoryJs(PageContext_t& c, OvmsVehicleToyotaETNGA* v, bool dc);   // live state history
    static void WebChargeReports(PageEntry_t& p, PageContext_t& c);   // index of saved charge reports
    static void WebChargeReport(PageEntry_t& p, PageContext_t& c);    // stream one report (raw HTML)
#endif // CONFIG_OVMS_COMP_WEBSERVER

protected:
    std::string m_rxbuf;

    bool m_allow_wake = true;  // Used to implement a cooldown timer if the vehicle is put into sleep
    int m_sleep_entry_time = 0;  // Used to track the time that cooldown timer started
    int m_sleep_cooldown_secs = 10;  // Cooldown window (s) for the current sleep; default must equal SLEEP_COOLDOWN_SECS[0]
    int m_sleep_backoff_idx = 0;     // Index into SLEEP_COOLDOWN_SECS; escalates on consecutive no-activity sleeps
    uint32_t m_last_can2_time = 0;   // monotonic secs of last accepted CAN2 frame; drives SLEEP<->AWAKE bus-liveness
    bool m_12v_was_high = false;     // 12V-above-threshold latch: the SLEEP 12V wake fires on the rising edge only
                                     // (level-triggering oscillated; seeded on each sleep entry in TransitionToSleepState)
    bool m_rail_charging12v = false;    // rail-voltage term of v.e.charging12v; own (non-persistent) latch, so the union's other terms cannot pollute the hysteresis

    bool m_armed_for_charge = false;   // charge lid seen open since entering AWAKE
    int  m_cable_watch_start = 0;      // monotonic s when armed (15-min cable watch)
    int  m_charge_state_entry = 0;     // monotonic s of last charge-state entry (handshake 60s timer)
    std::atomic<int> m_last_poll_monotonic{0};   // #138: monotonic s of the last poll reply (any ECU); charge-stale detection (cross-task: poller writes, ticker reads — atomic like m_charge_fault_pending)
    bool m_charge_wait_slept = false;  // true after Tier-2 CHARGE_WAIT sleep; selects the short re-sleep threshold
    int  m_pisw_zero_count = 0;        // consecutive fresh AWAKE PISW==0x00 reads; debounces the OBC post-wake transient
    uint32_t m_pisw_last_modified = 0;  // LastModified() of the last PISW reading counted, so the debounce counts distinct polls not 1s ticks

    // INC-2: charge-rate limiting side (who capped the rate). DC-only in this increment;
    // AC/cable/obc/grid/thermal are deferred (unconfirmed DIDs/constants).
    enum LimSide { LIM_UNKNOWN = 0, LIM_STATION, LIM_CAR };

    struct ChargeSessionState {
        bool  in_session = false;
        int   start_monotonic = 0;
        int   start_soc = -1;
        float start_soc_bms = -1.0f;   // BMS SOC at session open. The implied-capacity estimate
                                       // must use this, not display SOC: display SOC is defined
                                       // over the USABLE window and clamps at 100 once BMS >= 95%.
        time_t start_utc = 0;
        bool  is_dc = false;
        float peak_power = 0.0f;
        bool  temp_seen = false;
        float temp_min = 0.0f;
        float temp_max = 0.0f;
        // v2: location + ambient captured at open
        bool  has_loc = false;
        float start_lat = 0.0f;
        float start_lon = 0.0f;
        bool  amb_seen = false;
        float amb_min = 0.0f;
        float amb_max = 0.0f;
        // v2: charge-side coulomb counter (Ah) for the implied-capacity estimate
        float delivered_ah = 0.0f;
        float station_kwh = 0.0f;   // ∫ station_kw dt — energy drawn from the EVSE this session
        int   last_sample_monotonic = 0;   // dt for delivered_ah + CSV row cadence
        // v2: event log (monotonic seconds, static label string)
        std::vector<std::pair<int,const char*>> events;
        int   last_hlc = -1;               // last 0x1666 HLC state logged as an event (change detection)
        int   last_acop = -1;              // last 0x1684 AC-Op state logged as an event (change detection)
        // v2: downsampled chart buffer (per sample: delivered kW, SOC, station-offered + car-permitted kW)
        struct Sample { int t_s; float kw; int soc; float sta_max; float car_perm; float station_kw; float hvac_kw; };
        std::vector<Sample> svg;
        int   svg_interval_s = 20;
        int   last_svg_monotonic = 0;
        // v2: file basename (resolved "<dir>/<timestamp>", no extension) + CSV state
        std::string base;
        bool  csv_started = false;        // header emitted (into csv_buf)
        bool  csv_file_created = false;   // <base>.csv exists on disk (first flush truncates)
        std::string csv_buf;              // rows pending flush (batched to limit flash write cycles)
        int   last_csv_flush = 0;         // monotonic s of last flush
        // INC-1: per-phase tracking. phases[] is additive — the session-level
        // aggregates above stay authoritative for the Summary block.
        struct ChargePhase {
            bool   is_dc = false;
            int    start_monotonic = 0;
            time_t start_utc = 0;
            int    start_soc = -1;
            int    end_monotonic = 0;
            int    end_soc = -1;
            float  kwh_at_open = 0.0f;   // ms_v_charge_kwh snapshot at phase open
            float  energy_kwh = 0.0f;    // computed at close (delta)
            float  peak_power = 0.0f;    // kW into battery
            float  delivered_ah = 0.0f;  // phase-local coulomb count
            bool   temp_seen = false;
            float  temp_min = 0.0f;
            float  temp_max = 0.0f;
            int    outcome = -1;         // 0x1688 latched at close
            // INC-2: limiting-side attribution (DC car-vs-station). Defaults = unknown/inert.
            int    limiting_side = 0;       // LimSide; 0 = unknown / not classified
            float  limiting_value = 0.0f;   // the binding cap (kW)
            bool   cold_battery = false;    // sub-attribution: car-limited while battery cold
            // per-phase cap trackers (DC), filled live in UpdateChargeSessionStats:
            bool   cap_car_seen = false;
            float  cap_car_min = 0.0f;      // min active fabsf(0x16A1) over the phase
            bool   cap_station_seen = false;
            float  cap_station_max = 0.0f;  // max 0x166A advertised station power over the phase
        };
        std::vector<ChargePhase> phases;
        int   cur = -1;                  // index of the open phase in phases, -1 = none
        bool  report_written = false;    // idempotency guard for GenerateChargeReport
    };
    ChargeSessionState m_charge_session;

    // INC-3: charge-fault diagnostic DID dump (raw one-shot snapshot of OBC DIDs on a fault).
    std::atomic<bool> m_charge_fault_pending{false};   // set on poller task (0x1688 fault), consumed on Events task
    std::atomic<bool> m_charge_engaged{false};         // true once this session actually reached CHARGE_AC/CHARGE_DC;
                                                       // gates the fault flag so a RETAINED 0x1688 code read while
                                                       // waiting for a scheduled charge cannot fake a fault
    std::atomic<int>  m_dump_remaining{0};             // OnceOffPolls still outstanding
    std::map<uint16_t,std::string> m_dump_results;     // pid -> raw response bytes ("" = no reply)
    OvmsMutex         m_dump_mutex;                     // guards m_dump_results (poller task writes, Events task reads)
    int               m_dump_phase_idx = -1;           // which phase faulted (index into m_charge_session.phases)
    int               m_dump_outcome = -1;             // the 0x1688 code that triggered the dump
    int               m_dump_trigger_outcome = -1;     // latched at fault-flag time (poller task); avoids stale code at dump-kickoff
    int               m_dump_trigger_phase   = -1;     // latched phase index at fault-flag time

    static constexpr int TPMS_SLOT_COUNT = 5;
    int8_t m_tpms_corner[TPMS_SLOT_COUNT] = {0};   // slot->corner cache: 0=unread/none, 1=FL,2=FR,3=RL,4=RR

//    ControlState m_s_controlstate;
    OvmsMetricInt* m_s_controlstate;
    OvmsMetricInt* m_v_charge_pisw_raw;   // 0x1669 raw u8 connector state
    OvmsMetricInt* m_v_charge_ac_op;      // 0x1684 AC op status
    OvmsMetricInt* m_v_charge_hlc;        // 0x1666 DC HLC state
    OvmsMetricFloat* m_v_charge_perm;     // 0x16A1 min permission power (kW, s16 two's-comp, NOT biased-32768) — DC curve
    OvmsMetricFloat* m_v_charge_tgti;     // 0x166D target charging current (A)
    OvmsMetricFloat* m_v_charge_sta_max_p;  // 0x166A station max power (kW)
    OvmsMetricFloat* m_v_charge_sta_max_i;  // 0x1679 station max current (A)
    OvmsMetricFloat* m_v_charge_sta_max_v;  // 0x1681 station max voltage (V)
    OvmsMetricFloat* m_v_charge_ac_tgt_p;  // 0x1619 b1-2 AC target power (kW)
    OvmsMetricInt*   m_v_charge_chgr_op;   // 0x1619 b3 charger op status (enum) — distinct from m_v_charge_ac_op (0x1684)
    OvmsMetricFloat* m_v_charge_ac_ilim;   // 0x1619 b4-5 AC current limit (A)
    OvmsMetricFloat* m_v_charge_out;       // 0x161E b1-2 charger output (kW, unit inferred)
    OvmsMetricFloat* m_v_charge_out_tgt;   // 0x161E b3-4 target-from-charger (kW, unit inferred)
    OvmsMetricFloat* m_v_charge_ac_usable; // 0x1665 useable power (kW, unit inferred)
    OvmsMetricBool*  m_v_charge_myroom;   // 0x1692 byte 2 (idx 1) bit 0 = My Room active
    OvmsMetricFloat* m_v_charge_grid_power;  // 0x161D AC charger/grid input power (kW) — live, for CSV/efficiency
    OvmsMetricFloat* m_v_env_hvac_power;  // 0x106E HVAC/cabin power draw (kW): OBC view (0x745) while charging, hybrid-control view (0x7D2) while driving
    OvmsMetricFloat* m_v_env_hvac_kwh;    // My-Room cabin energy (kWh): time-integral of m_v_env_hvac_power over the My-Room-active interval
    OvmsMetricFloat* m_v_env_hvac_kwh_drive;  // Driving cabin/HVAC energy (kWh): per-trip time-integral of m_v_env_hvac_power while DRIVING (reset in NotifyVehicleOn)
    OvmsMetricInt*   m_v_charge_outcome;  // 0x1688 charging history / outcome enum
    OvmsMetricInt*   m_v_charge_stopreq;  // 0x1667 charge seq stop request (enum, partial)
    OvmsMetricFloat* m_v_bat_soc_bms;
    OvmsMetricFloat* m_v_bat_12v_voltage_pid;  // xte.v.b.12v.voltage 0x15EE EV-ECU PID read (compare vs hardware v.b.12v.voltage)
    OvmsMetricFloat* m_v_bat_12v_temp;     // xte.v.b.12v.temp 0x15F8 12V aux temperature (C)
    OvmsMetricFloat* m_v_bat_12v_cac;      // xte.v.b.12v.cac 0x15E5 12V aux full-charge capacity (Ah)
    OvmsMetricFloat* m_v_bat_12v_charge_ah;    // xte.v.b.12v.charge.ah    0x15E8 bytes 1-4  lifetime charge integral (Ah)
    OvmsMetricFloat* m_v_bat_12v_discharge_ah; // xte.v.b.12v.discharge.ah 0x15E8 bytes 5-8  lifetime discharge integral (Ah)
    OvmsMetricFloat* m_v_bat_12v_readyon_h;    // xte.v.b.12v.readyon.h    0x15E8 bytes 11-12 integrated Ready-ON time (h)
    // Internal bookkeeping (not exposed as metrics):
    int   m_awake_entered = 0;        // ms_m_monotonic seconds when AWAKE was entered (awake-timeout watchdog)
    float m_trip_start_odo = 0.0f;    // odometer baseline at trip start
    bool  m_trip_start_valid = false; // false until the baseline is seeded; reset on transition to DRIVING
    OvmsMetricInt* m_v_e_awd;   // 0x1087 b2 AWD / X-MODE status (custom; no standard OVMS metric)
    int m_bms_modules = 4;   // resolved HV pack module count (bootstrap = 96-cell / 4 modules)
    int m_pack_cells = 0;    // cell count as actually OBSERVED on 0x182E; 0 until the first reply.
                             // Deliberately not read back from the BMS API: the ctor bootstraps the
                             // arrangement to 96 cells, so that would report a 96-cell pack on every
                             // car until 0x182E lands — silently picking the wrong pack nominal.
    bool  m_nominal_warned = false;      // one-shot: "no pack nominal, v.b.soh suppressed"
    float m_soh_warned_nominal = 0.0f;   // nominal the "SOH > 100%" warning last fired for, so
                                         // correcting the config re-arms it without a reboot
    
    void NotifyVehicleOn() override;
    // NotifyChargeStart deliberately not overridden — see vehicle_toyota_etnga.cpp;
    // session counters reset at session open in TransitionToChargeHandshakeState.

private:
    static constexpr const char* TAG = "v-etnga";
    // Energy-integrator timestamps (esp_log_timestamp ms; 0 = interval not started).
    // Must be zero-initialized: the first poll reply can arrive before NotifyVehicleOn /
    // NotifyChargeStart reset them, and a garbage dt would corrupt the persistent *_total metrics.
    uint32_t lastBatteryEnergyLogTime = 0;
    uint32_t lastChargerEnergyLogTime = 0;
    float m_charge_obc_kw = 0.0f;   // diagnostic: raw 0x10D4 OBC "battery charging power" (under-reads on DC, issue #109)
    uint32_t lastGridEnergyLogTime = 0;
    uint32_t lastHvacEnergyLogTime = 0;
    uint32_t lastHvacDriveEnergyLogTime = 0;

    // XOR-folded signature of the last poll error logged at WARN (module/pid/code — see
    // IncomingPollError): repeats of the same error (e.g. one TX failure per poll while
    // the bus is wedged) drop to debug level until the signature changes.
    uint32_t m_last_poll_error = 0;

    void InitializeMetrics();  // Initializes the metrics specific to this vehicle module
    void ResetStaleMetrics();  // Checks if state transition metrics are stale (and resets them)

    // Charge session report (etnga_charge_report.cpp)
    void UpdateChargeSessionStats();   // live aggregation while charging (peak power, temp range, type)
    void RenderPowerSvg(std::ostream& out);  // stream the inline SVG power(+SOC)-vs-time chart from m_charge_session.svg
    void GenerateChargeReport();       // write the session-end HTML report to /store/charge-reports/
    void OpenChargePhase(bool is_dc);   // INC-1: start a new charge phase on AC/DC entry
    void CloseChargePhase();            // INC-1: close the open phase on WAIT / report time
    void ClassifyLimitingSide();                        // INC-2: set the open phase's limiting side at close
    static const char* LimSideLabel(int side);          // INC-2: LimSide enum -> human text
    void LogChargeEvent(const char* label);            // append a timestamped event
    void AppendChargeCsvRow();                          // buffer one CSV row (header on first call)
    void FlushChargeCsv();                              // write buffered CSV rows to <base>.csv
    std::string ChargeReportDir();                      // "/sd/charge-reports" if SD mounted else "/store/..."

    // --- Async charge file-I/O worker (decouples SD writes from the Events task) ---
    // See docs/superpowers/specs/2026-06-20-etnga-charge-async-io-design.md
    struct etnga_io_job {
        enum Op { WRITE_APPEND, WRITE_TRUNCATE, UNLINK, STOP } op;
        std::string path;        // destination (producer resolves SD vs /store)
        std::string data;        // bytes to write (empty for UNLINK/STOP)
        std::string prune_dir;   // non-empty on the report write → prune that dir afterward
    };
    QueueHandle_t     m_io_queue   = NULL;
    TaskHandle_t      m_io_task     = NULL;
    SemaphoreHandle_t m_io_stopped  = NULL;
    uint32_t          m_io_dropcnt  = 0;
    static void ChargeIoTaskEntry(void* arg);
    void ChargeIoTask();
    void StartChargeIoTask();              // lazy, idempotent
    bool ChargeIoEnqueue(etnga_io_job* job);
    void StopChargeIoTask();               // sentinel + timed join (never blocks shutdown)
    static const char* ChargeOutcomeLabel(int code);    // 0x1688 enum -> human text
    static const char* HlcStateLabel(int code);         // 0x1666 DC HLC state enum -> human text ("" if unknown)
    static const char* AcOpStatusLabel(int code);       // 0x1684 AC-Op state enum -> human text ("" for Stop/unknown)
    std::string DumpDidDecode(uint16_t did, const std::string& raw);  // INC-3: human-readable decode for the confident DID subset; "" = raw only
    static const char* DumpDidName(uint16_t did);   // solterra-can RE label for a dump DID; "" if unknown
    std::string LookupLocationName(float lat, float lon); // matching OVMS named-location (geofence), or ""

    // Incoming message handling functions
    void IncomingAirConditionerSystem(uint16_t pid);
    void IncomingHPCMHybridPtCtr(uint16_t pid);
    void IncomingHybridBatterySystem(uint16_t pid);
    void IncomingHybridControlSystem(uint16_t pid);
    void IncomingPlugInControlSystem(uint16_t pid);
    void IncomingBrakeEpb(uint16_t pid);
    void IncomingTPMS(uint16_t pid);
    void UpdateTPMSAlert();
    bool TPMSCornerMapValid();

    // Data calculation functions
    float CalculateAmbientTemperature(const std::string& data);
    float CalculateAmbientTemperatureEV(const std::string& data);
    // Resolve the e-TNGA HV pack module count from the per-reply cell count.
    // Known packs: 96 (2022-24, 4x24), 78 (2025/26 FWD, 3x26), 104 (2025/26 AWD, 4x26).
    // Returns 0 for an unrecognised count so callers keep the last good arrangement.
    int PackModuleCount(int cellCount);
    // Pack nominal full-charge capacity (Ah) and nominal pack voltage (V) for the detected
    // variant, or 0.0f when unknown. Config [xte] bat.nominal.ah / bat.nominal.volt override.
    float PackNominalAh();
    float PackNominalVolt();
    std::vector<float> CalculateBatteryCellVoltages(const std::string& data);
    std::vector<float> CalculateBatteryCapacityArray(const std::string& data);
    float CalculateBatteryChargingPower(const std::string& data);
    float CalculateBatteryCurrent(const std::string& data);
    float CalculateBatteryPower(float voltage, float current);
    float CalculateBatterySOC(const std::string& data);
    float CalculateBatterySOCBMS(const std::string& data);
    std::vector<float> CalculateBatteryTemperatures(const std::string& data);
    float CalculateBatteryVoltage(const std::string& data);
    float CalculateCabinTemperature(const std::string& data);
    int CalculateAcOpStatus(const std::string& data);
    int CalculateChargeType(const std::string& data);
    int CalculateHlcState(const std::string& data);
    int CalculatePISWRaw(const std::string& data);
    float CalculatePermissionPower(const std::string& data);
    float CalculateTargetCurrent(const std::string& data);
    float CalculateStationVoltage(const std::string& data);
    float CalculateStationCurrent(const std::string& data);
    float CalculateStationMaxPower(const std::string& data);
    float CalculateStationMaxCurrent(const std::string& data);
    float CalculateStationMaxVoltage(const std::string& data);
    float CalculateChargerInputPower(const std::string& data);
    float CalculateAcTargetPower(const std::string& data);
    int   CalculateChargerOpStatus(const std::string& data);
    float CalculateAcCurrentLimit(const std::string& data);
    float CalculateChargerOutput(const std::string& data);
    float CalculateChargerOutputTarget(const std::string& data);
    float CalculateAcUsable(const std::string& data);
    bool  CalculateMyRoom(const std::string& data);
    float CalculateAcConsumption(const std::string& data);
    int   CalculateChargeOutcome(const std::string& data);
    int   CalculateChargeStopReq(const std::string& data);
    bool CalculateChargingDoorStatus(const std::string& data);
    int CalculateControlMode(const std::string& data);
    float CalculateHVACSetpoint(const std::string& data);
    float CalculateOdometer(const std::string& data);
    bool CalculatePISWStatus(const std::string& data);
    bool CalculateReadyStatus(const std::string& data);
    int CalculateShiftPosition(const std::string& data);
    float CalculateVehicleSpeed(const std::string& data);
    float CalculateThrottle(const std::string& data);
    int   CalculateDriveMode(const std::string& data);
    int   CalculateAwdMode(const std::string& data);
    float CalculateFootBrake(const std::string& data);
    bool  CalculateParkBrake(const std::string& data);
    float CalculateAux12vCurrent(const std::string& data);
    float CalculateAux12vVoltage(const std::string& data);
    float CalculateAux12vTemperature(const std::string& data);
    float CalculateAux12vFullCharge(const std::string& data);

    // Metric setter functions
    void SetAcOpStatus(int v);
    void SetAmbientTemperature(float temperature);
    void SetAwake(bool awake);
    bool IsBusAlive() const;   // true if a CAN2 frame arrived within BUS_STALE_SECS
    void SetBatteryChargingPower(float power);
    void SetBatteryCurrent(float current);
    void SetBatteryPower(float power);
    void SetBatterySOC(float soc);
    void SetBatterySOCBMS(float soc);
    void SetBatteryCellVoltages(const std::vector<float>& voltages);
    // Derive v.b.cac / v.b.soh / v.b.capacity from a fresh 0x1D3E array. Called from the
    // 0x1D3E handler only, so the standard metrics never outrun their source.
    void UpdateBatteryHealth(const std::vector<float>& caps);
    void SetBatteryCellVoltageStatistics(const std::vector<float>& voltages);
    void SetBatteryTemperatures(const std::vector<float>& temperatures);
    void SetBatteryTemperatureStatistics(const std::vector<float>& temperatures);
    void SetBatteryVoltage(float voltage);
    void SetCabinTemperature(float temperature);
    void SetChargeType(int chargeType);
    void SetChargeState(PollState state);
    void SetChargerInputPower(float power);
    void SetChargingStatus(bool status);
    void SetChargingDoorStatus(bool status);
    void SetControlMode(int controlMode);
    void SetHlcState(int v);
    void SetHVACSetpoint(float temperature);
    void SetOdometer(float odometer);
    void SetPISWRaw(int v);
    void SetPISWStatus(bool status);
    void SetPollState(int state);
    void SetReadyStatus(bool status);
    void SetShiftPosition(int position);
    void SetVehicleSpeed(float speed);
    void SetVehicleVIN(std::string vin);
    void SetStationVoltage(float volts);
    void SetStationCurrent(float amps);
    void SetPermissionPower(float kw);
    void SetTargetCurrent(float amps);
    void SetStationMaxPower(float kw);
    void SetStationMaxCurrent(float amps);
    void SetStationMaxVoltage(float volts);
    void SetAcTargetPower(float kw);
    void SetChargerOpStatus(int v);
    void SetAcCurrentLimit(float v);
    void SetChargerOutput(float v);
    void SetChargerOutputTarget(float v);
    void SetAcUsable(float v);
    void SetMyRoom(bool active);
    void SetHvacPower(float kw);
    bool SetChargeOutcome(int v);   // returns true if the 0x1688 code actually changed
    void SetChargeStopReq(int v);
    void SetThrottle(float pct);
    void SetDriveMode(int mode);
    void SetAwdMode(int mode);
    void SetFootBrake(float pct);
    void SetParkBrake(bool applied);
    void SetAux12vCurrent(float v);
    void SetAux12vVoltage(float v);
    void SetAux12vTemperature(float v);
    void SetAux12vFullCharge(float v);
    void UpdateCharging12v();
    void DecodeAux12vIntegrators(const std::string& data);

    // const char* throughout: these run on every poll reply, and std::string
    // parameters meant heap allocations per call even with logging filtered out.
    void LogMetricChange(OvmsMetricBool* metric, bool newValue, const char* label, const char* valueLabel);
    void LogMetricChange(OvmsMetricFloat* metric, float newValue, const char* label, const char* units);
    void LogMetricChange(OvmsMetricInt* metric, int newValue, const char* label, const char* valueLabel);
    void LogMetricChange(OvmsMetricString* metric, const std::string& newValue, const char* label);

    // Hours since the last sample on an energy-integrator channel; updates the channel
    // timestamp. Implementation: etnga_metrics.cpp.
    float EnergyIntervalHours(uint32_t& lastSampleTime);

    // State transition functions
    void HandleSleepState();
    void HandleAwakeState();
    void HandleDrivingState();
    void HandleChargeHandshakeState();
    void HandleChargeWaitState();
    void HandleChargeAcState();
    void HandleChargeDcState();
    void ResetSleepBackoff();   // reset cooldown escalation to the base step (real activity seen)
    void TransitionToSleepState();
    void TransitionToAwakeState();
    void TransitionToDrivingState();
    void TransitionToChargeHandshakeState();
    void TransitionToChargeWaitState();
    void TransitionToChargeAcState();
    void TransitionToChargeDcState();

    void RequestVIN();
    void IncomingVINSuccess(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, CAN_frame_format_t format, const std::string &data);
    void IncomingVINFail(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, int errorcode);

    static bool IsChargeFaultCode(int code);   // INC-3: true for abnormal 0x1688 stop codes
    void MaybeStartChargeFaultDump();          // INC-3: kick off the OnceOffPoll burst (Events task)
    void IncomingDumpSuccess(uint16_t type, uint32_t module_sent, uint32_t module_rec,
                             uint16_t pid, CAN_frame_format_t format, const std::string& data);
    void IncomingDumpFail(uint16_t type, uint32_t module_sent, uint32_t module_rec,
                          uint16_t pid, int errorcode);
    // INC-3: deferred report finalization (Events task only)
    void FinalizeChargeSession();              // write the report + reset m_charge_session
    bool m_report_pending = false;             // true while waiting for a fault dump to complete
    int  m_report_pending_deadline = 0;        // monotonic s: give up and finalize after this
    static const int DUMP_WAIT_SECS = 10;      // max seconds to wait for the dump before finalizing anyway

};

// CAN bus addresses
enum CANAddress
{
    AIR_CONDITIONER_TX = 0x7C4,
    AIR_CONDITIONER_RX = 0x7CC,
    HYBRID_BATTERY_SYSTEM_TX = 0x747,
    HYBRID_BATTERY_SYSTEM_RX = 0x74F,
    HYBRID_CONTROL_SYSTEM_TX = 0x7D2,
    HYBRID_CONTROL_SYSTEM_RX = 0x7DA,
    PLUG_IN_CONTROL_SYSTEM_TX = 0x745,
    PLUG_IN_CONTROL_SYSTEM_RX = 0x74D,
    HPCM_HYBRIDPTCTR_RX = 0x7EA,
    BRAKE_EPB_TX = 0x7B0,    // Brake/EPB ECU (ABS/VSC/TRC + Electric Parking Brake) — direct-poll, standard ISO-TP
    BRAKE_EPB_RX = 0x7B8,
    TPMS_GW_TX = 0x7502A,    // (0x750 << 8) | 0x2A  -> MsgID 0x750, sub 0x2A (ISOTP_EXTADR mixed addressing)
    TPMS_GW_RX = 0x7582A,    // (0x758 << 8) | 0x2A  -> MsgID 0x758, sub 0x2A
};

// CAN PIDs
enum CANPID
{
    PID_ACTIVE_DIAGNOSTIC_SESSION = 0xF186,
    PID_AMBIENT_TEMPERATURE = 0x1002,
    PID_AMBIENT_TEMPERATURE_EV = 0x1F46,
    PID_BATTERY_CAPACITY = 0x1D3E,        // 8x u16 BE x0.01 Ah — full-charge capacity; drives v.b.cac (see CalculateBatteryCapacityArray)
    PID_BATTERY_CELL_VOLTAGES = 0x182E,
    PID_BATTERY_CHARGING_POWER = 0x10D4,
    PID_BATTERY_TEMPERATURES = 0x1814,
    PID_BATTERY_SOC = 0x1738,
    PID_BATTERY_SOC_BMS = 0x1F5B,
    PID_BATTERY_VOLTAGE_AND_CURRENT = 0x1F9A,
    PID_CABIN_TEMPERATURE = 0x1001,
    PID_CHARGER_INPUT_POWER = 0x161D,
    PID_CONTROL_SYSTEM_MODE = 0x10D1,
    PID_CHARGING_CONTROL_INFORMATION = 0x1689,
    PID_CHARGING_LID = 0x1625,
    PID_CHARGING_VOLTAGE_TYPE = 0x161C,
    PID_HVAC_SETPOINT = 0x1036,
    PID_HEATER_POWER = 0x1086,            // HV electric heater power (W, 2-byte cluster, split TBD); >0 => v.e.heating
    PID_BLOWER_LEVEL = 0x2801,            // Blower level (u8 1-7) => v.e.cabinfan %
    PID_ODOMETER = 0x1FA6,
    PID_PISW_STATUS = 0x1669,
    PID_READY_SIGNAL = 0x1076,
    PID_SHIFT_POSITION = 0x1061,
    PID_VEHICLE_SPEED = 0x1F0D,
    PID_VIN = 0xF190,
    
    PID_DC_CHARGER_PRESENT_CURRENT = 0x166C,  // u16 BE x1 A/LSB; DC station present current (idle 0 when no station)

    PID_DC_CHARGER_PRESENT_VOLTAGE = 0x166B,  // u16 BE x1 V/LSB; DC station present voltage (idle 0 when no station)

    PID_AC_CHARGING_OP_STATUS = 0x1684,  // 0=Stop,1=Startup,2=Running,3=Finishing
    PID_HLC_STATE = 0x1666,              // DC HLC: 0xFF=Unconnected, 0x0A-0x12 active

    PID_MIN_PERMISSION_POWER = 0x16A1,   // s16 BE x0.01 kW; 0x8000 sentinel = inactive — THE DC taper curve
    PID_TARGET_CHARGING_CURRENT = 0x166D, // u16 BE x1 A; live current request

    PID_DC_CHARGER_MAX_POWER = 0x166A,    // u16 BE x0.01 kW; station advertised max power
    PID_DC_CHARGER_MAX_CURRENT = 0x1679,  // u16 BE x1 A; station advertised max current (CCS)
    PID_DC_CHARGER_MAX_VOLTAGE = 0x1681,  // u16 BE x1 V; station advertised max voltage (CCS)

    PID_CHARGER_STATE_CLUSTER = 0x1619,   // AC-only: b1-2 target power (biased-32768 x0.01 kW), b3 op status enum, b4-5 current limit (biased-32768 x0.01 A)
    PID_CHARGER_OUTPUT_POWER = 0x161E,    // AC-only: b1-2 output (x5/1000 kW, unit inferred), b3-4 target-from-charger (x5/1000 kW, unit inferred)
    PID_AC_USABLE_POWER = 0x1665,         // AC-only: u8 x0.01 kW (unit inferred)

    PID_AC_CONSUMPTION = 0x106E,    // A/C consumption power: u8 x0.05 kW/LSB (50 W/LSB) — cabin draw, OBC view
    PID_CHARGE_STOP_REQ = 0x1667,   // Charge Seq Stop Request from CCM: u8 enum (0x00 Normal / 0x06 HLC-error; partial)
    PID_CHARGE_HISTORY = 0x1688,    // Charging History (outcome/stop-reason): u8 26-state enum
    PID_MYROOM = 0x1692,            // byte 2 bit 0 = My Room active flag (live)
    PID_TPMS_PRESSURES = 0x1005,  // gateway 0x2A: 5x u16 [status][raw]; psi = raw*0.25 - 7.35
    PID_TPMS_TEMPS = 0x1004,      // gateway 0x2A: 5x u8;  C = raw - 40
    PID_TPMS_CORNERS = 0x2021,    // gateway 0x2A: 5x u8 corner enum (0 none/1 FL/2 FR/3 RL/4 RR)

    // 2026-06-06 pins — EV ECU (0x7D2) driver-input signals
    PID_THROTTLE = 0x1060,          // b1: accelerator position, u8 x0.5 %/LSB (0x00-0xC8 -> 0-100%)
    PID_DRIVE_MODE_SELECT = 0x1004, // b1: Eco/Normal/Power enum. NOTE: same numeric value as PID_TPMS_TEMPS,
                                    //     but dispatched on a different ECU (0x7DA vs TPMS gateway) so no conflict.
    PID_AWD_MODE = 0x1087,          // b2: X-MODE/AWD status enum

    // 2026-06-06 pins — Brake/EPB ECU (0x7B0) direct-poll
    PID_BRAKE_PEDAL_STROKE = 0x104C, // b1: brake pedal stroke, u8 ~1 mm/LSB (0 rest .. ~67 full)
    PID_EPB_STATUS = 0x1045,         // b1 = RH actuator status enum; handbrake applied = 0x00 (Park Applied)

    // 12V auxiliary battery — EV ECU (0x7D2 req / 0x7DA resp), READDATA 0x22
    PID_AUX_BATTERY_CURRENT = 0x15F7,      // 12V aux: u16 BE biased-32768 (raw-0x8000) x0.0038147 A, bidirectional
    PID_AUX_BATTERY_VOLTAGE = 0x15EE,      // 12V aux: u16 BE x5/4096 V
    PID_AUX_BATTERY_TEMP = 0x15F8,         // 12V aux: u16 BE (raw-400) x0.1 C
    PID_AUX_BATTERY_FULL_CHARGE = 0x15E5,  // 12V aux: u8 x0.5 Ah
    PID_AUX_BATTERY_INTEGRATORS = 0x15E8,  // 12V aux cluster (EV ECU 0x7D2): lifetime charge/discharge Ah + ready-on hours

};

// RX buffer access functions

inline uint8_t GetRxBByte(const std::string& rxbuf, size_t index)
{
    // Bounds-checked: a short/garbled UDS reply must not read past the buffer.
    // Handlers should still length-check multi-byte payloads before decoding
    // (a zero-fill is memory-safe but not valid data).
    return (index < rxbuf.size()) ? static_cast<uint8_t>(rxbuf[index]) : 0;
}

inline uint16_t GetRxBUint16(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint16_t>(GetRxBByte(rxbuf, index)) << 8) | GetRxBByte(rxbuf, index + 1);
}

inline uint32_t GetRxBUint24(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint32_t>(GetRxBByte(rxbuf, index)) << 16) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 1)) << 8) |
        GetRxBByte(rxbuf, index + 2);
}

inline uint32_t GetRxBUint32(const std::string& rxbuf, size_t index)
{
    return (static_cast<uint32_t>(GetRxBByte(rxbuf, index)) << 24) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 1)) << 16) |
        (static_cast<uint32_t>(GetRxBByte(rxbuf, index + 2)) << 8) |
        GetRxBByte(rxbuf, index + 3);
}

inline int8_t GetRxBInt8(const std::string& rxbuf, size_t index)
{
    return static_cast<int8_t>(GetRxBByte(rxbuf, index));
}

inline int16_t GetRxBInt16(const std::string& rxbuf, size_t index)
{
    return static_cast<int16_t>(GetRxBUint16(rxbuf, index));
}

inline int32_t GetRxBInt32(const std::string& rxbuf, size_t index)
{
    return static_cast<int32_t>(GetRxBUint32(rxbuf, index));
}

inline bool GetRxBBit(const std::string& rxbuf, size_t byteIndex, size_t bitIndex)
{
    uint8_t byte = GetRxBByte(rxbuf, byteIndex);
    return (byte & (1 << bitIndex)) != 0;
}

inline const char* ConvertPollStateToString(int state) {
    // Indexed by PollState (SLEEP..CHARGE_DC).
    static const char* const names[] = {
        "SLEEP", "AWAKE", "DRIVING", "CHARGE_HANDSHAKE", "CHARGE_WAIT", "CHARGE_AC", "CHARGE_DC"
    };
    if (state < 0 || state >= (int)(sizeof(names) / sizeof(names[0])))
        return "UNKNOWN";
    return names[state];
}

#endif // __VEHICLE_TOYOTA_ETNGA_H__
