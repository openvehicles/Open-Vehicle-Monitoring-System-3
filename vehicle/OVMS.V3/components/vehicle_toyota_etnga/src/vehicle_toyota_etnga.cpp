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

#include "ovms_log.h"
#include "ovms_config.h"
#include "vehicle_toyota_etnga.h"

// Poll state descriptions:
//    SLEEP (0)             : Vehicle is sleeping; no activity on the CAN bus. We are listening only.
//    AWAKE (1)             : Vehicle is alive; vehicle has been switched on by driver
//    DRIVING (2)           : Vehicle is "Ready" to drive or being driven
//    CHARGE_HANDSHAKE (3)  : Cable-negotiation fast-poll window (was CHARGING)
//    CHARGE_WAIT (4)       : Plugged in, not (yet/any longer) charging — sparse poll
//    CHARGE_AC (5)         : AC charging in progress
//    CHARGE_DC (6)         : DC fast charging in progress
//
// A poll list supports only VEHICLE_POLL_NSTATES (4) states, so the 7 states are
// split across two poll series ("blocks"), each polled by the framework according
// to its state offset (see OvmsPoller::StandardPollSeries::NextPollEntry):
//
//    obdii_polls_base   : offset 0  -> columns map to states 0..3 (SLEEP/AWAKE/DRIVING/-)
//    obdii_polls_charge : offset 3  -> columns map to states 3..6 (HANDSHAKE/WAIT/AC/DC)
//
// This mirrors the Hyundai Ioniq5 secondary-series pattern. PIDs that poll in both a
// non-charge state and a charge state (battery SOC/V-I/temp/voltage, capacity arrays,
// ctrl-mode, PISW) appear in BOTH tables, each carrying only its side's cadences.

// --- Block A: non-charge states. Offset 0; columns { SLEEP, AWAKE, DRIVING, (unused) }.
// Column 3 maps to CHARGE_HANDSHAKE and MUST stay 0 here — block B owns the charge
// states, and a nonzero here would double-poll during handshake.
static const OvmsPoller::poll_pid_t obdii_polls_base[] = {
    // Column layout: { SLEEP, AWAKE, DRIVING, - }

    // State variables polls
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CONTROL_SYSTEM_MODE,        { 0,  1,  1, 0}, 0, ISOTP_STD }, // 0x10D1 ctrl-mode: AWAKE+DRIVING @1s (also block B)
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_LID,               { 0, 10, 10, 0}, 0, ISOTP_STD }, // 0x1625 charge lid: AWAKE+DRIVING only

    // Combined (battery) polls — also polled in charge (block B)
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC,                { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1738 SOC: DRIVING @1s
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC_BMS,            { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1F5B BMS SOC: DRIVING @1s
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_READY_SIGNAL,               { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1076 ready signal: DRIVING only
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT,{ 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1F9A pack I: DRIVING @1s
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_TEMPERATURES,       { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x1814 cell-temp array: DRIVING @10s
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_CELL_VOLTAGES,      { 0,  0,  5, 0}, 0, ISOTP_STD }, // 0x182E cell voltages: DRIVING @5s

    // Capacity arrays — also polled in charge (block B)
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_CAPACITY,           { 0, 60,120, 0}, 0, ISOTP_STD }, // 0x1D3E 8x full-charge capacity (Ah) -> v.b.cac/soh/capacity

    // Driving polls
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_VEHICLE_SPEED,              { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1F0D speed: DRIVING only
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_SHIFT_POSITION,             { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1061 gear: DRIVING only
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_ODOMETER,                   { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1FA6 odometer: DRIVING only
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_AC_CONSUMPTION,             { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x106E HVAC power (hybrid control): DRIVING @1s (cabin-energy integrator)
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_THROTTLE,                   { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x1060 b1 accelerator position: DRIVING @1s (v.e.throttle)
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_DRIVE_MODE_SELECT,          { 0,  0,  5, 0}, 0, ISOTP_STD }, // 0x1004 b1 drive mode (Eco/Normal/Power): DRIVING @5s (v.e.drivemode)
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_AWD_MODE,                   { 0,  0,  5, 0}, 0, ISOTP_STD }, // 0x1087 b2 AWD/X-MODE status: DRIVING @5s (xte.v.e.awd)
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_CURRENT, { 0, 0,  10, 0}, 0, ISOTP_STD }, // 0x15F7 12V aux current (EV ECU): DRIVING @10s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_VOLTAGE, { 0, 0,  30, 0}, 0, ISOTP_STD }, // 0x15EE 12V aux voltage (EV ECU, hi-res): DRIVING @30s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_TEMP,    { 0, 0, 120, 0}, 0, ISOTP_STD }, // 0x15F8 12V aux temp (EV ECU): DRIVING @120s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_FULL_CHARGE, { 0, 0, 120, 0}, 0, ISOTP_STD }, // 0x15E5 12V aux CAC (EV ECU): DRIVING @120s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_INTEGRATORS, { 0, 0, 120, 0}, 0, ISOTP_STD }, // 0x15E8 12V lifetime integrators: DRIVING @120s
  { AIR_CONDITIONER_TX,        AIR_CONDITIONER_RX,        VEHICLE_POLL_TYPE_READDATA, PID_AMBIENT_TEMPERATURE,        { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x1002 ambient temp: DRIVING only
  { AIR_CONDITIONER_TX,        AIR_CONDITIONER_RX,        VEHICLE_POLL_TYPE_READDATA, PID_CABIN_TEMPERATURE,          { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x1001 cabin temp: DRIVING only
  { AIR_CONDITIONER_TX,        AIR_CONDITIONER_RX,        VEHICLE_POLL_TYPE_READDATA, PID_HVAC_SETPOINT,              { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x1036 HVAC setpoint: DRIVING only
  { AIR_CONDITIONER_TX,        AIR_CONDITIONER_RX,        VEHICLE_POLL_TYPE_READDATA, PID_HEATER_POWER,               { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x1086 HV heater power: DRIVING only (>0 => v.e.heating)
  { AIR_CONDITIONER_TX,        AIR_CONDITIONER_RX,        VEHICLE_POLL_TYPE_READDATA, PID_BLOWER_LEVEL,               { 0,  0, 10, 0}, 0, ISOTP_STD }, // 0x2801 blower level 1-7: DRIVING only (=> v.e.cabinfan %)

    // Chassis / driver inputs — Brake/EPB ECU (0x7B0), direct-poll standard ISO-TP.
    // EPB stays alive in the parked body tail, so park-brake is polled in AWAKE too; foot-brake is DRIVING-only.
  { BRAKE_EPB_TX,              BRAKE_EPB_RX,              VEHICLE_POLL_TYPE_READDATA, PID_BRAKE_PEDAL_STROKE,         { 0,  0,  1, 0}, 0, ISOTP_STD }, // 0x104C b1 brake pedal stroke: DRIVING @1s (v.e.footbrake)
  { BRAKE_EPB_TX,              BRAKE_EPB_RX,              VEHICLE_POLL_TYPE_READDATA, PID_EPB_STATUS,                 { 0,  5,  5, 0}, 0, ISOTP_STD }, // 0x1045 b1 EPB actuator status: AWAKE+DRIVING @5s (v.e.handbrake)

    // PISW — AWAKE keep-alive side only (charge cadences live in block B).
    // AWAKE @5s is required by the wake-reconcile (see etnga_poll_states.cpp).
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_PISW_STATUS,                { 0,  5,  0, 0}, 0, ISOTP_STD }, // 0x1669 PISW: AWAKE @5s

    // TPMS — gateway-relayed (0x750 sub-target 0x2A) via ISOTP_EXTADR; DRIVING @60s
    // (0x2A only answers while driving/My-Room — asleep parked & during charging it just times out)
  { TPMS_GW_TX, TPMS_GW_RX, VEHICLE_POLL_TYPE_READDATA, PID_TPMS_CORNERS,   { 0,  0, 60, 0}, 0, ISOTP_EXTADR }, // 0x2021 slot->corner map
  { TPMS_GW_TX, TPMS_GW_RX, VEHICLE_POLL_TYPE_READDATA, PID_TPMS_PRESSURES, { 0,  0, 60, 0}, 0, ISOTP_EXTADR }, // 0x1005 pressures (kPa)
  { TPMS_GW_TX, TPMS_GW_RX, VEHICLE_POLL_TYPE_READDATA, PID_TPMS_TEMPS,     { 0,  0, 60, 0}, 0, ISOTP_EXTADR }, // 0x1004 temperatures (C)

    POLL_LIST_END
};

// --- Block B: charge states. Registered with state offset CHARGE_HANDSHAKE (3), so its
// four columns map to states 3..6. Cadences tuned from sim POLLS dict in bin/ovms-sim.py.
static const OvmsPoller::poll_pid_t obdii_polls_charge[] = {
    // Column layout: { CHARGE_HANDSHAKE, CHARGE_WAIT, CHARGE_AC, CHARGE_DC }
    //
    // Order matters under bus saturation: the poller services this list top-down each
    // cycle and may not finish before the next 1s tick, so the per-second power/SOC
    // channels (the CSV's live columns) are listed FIRST and the heavy 96-cell multiframe
    // arrays (0x1814/0x182E/0x1D3E) LAST — a cut-short cycle defers the slow arrays,
    // not the live readings. Cadence values are unchanged; only order.

    // --- Per-second core: charge state + live power/SOC telemetry (CSV fast columns) ---
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CONTROL_SYSTEM_MODE,        { 1, 10,  1,  1}, 0, ISOTP_STD }, // 0x10D1 ctrl-mode: WAIT slowed to 10s (12V-drain fix)
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_VOLTAGE_AND_CURRENT,{ 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x1F9A pack I: AC+DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC,                { 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x1738 SOC: AC+DC @1s (absent in HS/WAIT)
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_SOC_BMS,            { 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x1F5B BMS SOC: AC+DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_CHARGING_POWER,     { 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x10D4 batt power: AC+DC @1s (handler gated on charge-in-progress)
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_MIN_PERMISSION_POWER,       { 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x16A1 perm power (curve): AC+DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_TARGET_CHARGING_CURRENT,    { 0,  0,  1,  1}, 0, ISOTP_STD }, // 0x166D target current: AC+DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_DC_CHARGER_PRESENT_CURRENT, { 0,  0,  0,  1}, 0, ISOTP_STD }, // 0x166C DC station A: DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_DC_CHARGER_PRESENT_VOLTAGE, { 0,  0,  0,  1}, 0, ISOTP_STD }, // 0x166B DC station V: DC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AC_CHARGING_OP_STATUS,      { 1, 10,  1,  1}, 0, ISOTP_STD }, // 0x1684 AC op: WAIT slowed to 10s (was 1s); AC engage caught within 10s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_HLC_STATE,                  { 1, 10,  0,  1}, 0, ISOTP_STD }, // 0x1666 HLC: WAIT slowed to 10s (was 1s); DC re-engage caught within 10s

    // --- Slower telemetry / charge-report channels (2-30s) ---
  { HYBRID_CONTROL_SYSTEM_TX,  HYBRID_CONTROL_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_AMBIENT_TEMPERATURE_EV,     {30, 30, 30, 30}, 0, ISOTP_STD }, // 0x1F46 ambient via HCS: all charge @30s (A/C ambient is DRIVING-only)
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_CURRENT, { 0, 0,  10,  10}, 0, ISOTP_STD }, // 0x15F7 12V aux current (EV ECU): AC+DC @10s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_VOLTAGE, { 0, 0,  30,  30}, 0, ISOTP_STD }, // 0x15EE 12V aux voltage (EV ECU): AC+DC @30s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_TEMP,    { 0, 0, 120, 120}, 0, ISOTP_STD }, // 0x15F8 12V aux temp (EV ECU): AC+DC @120s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_FULL_CHARGE, { 0, 0, 120, 120}, 0, ISOTP_STD }, // 0x15E5 12V aux CAC (EV ECU): AC+DC @120s
  { HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AUX_BATTERY_INTEGRATORS, { 0, 0, 120, 120}, 0, ISOTP_STD }, // 0x15E8 12V lifetime integrators: AC+DC @120s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_PISW_STATUS,                { 1, 30, 10, 10}, 0, ISOTP_STD }, // 0x1669 PISW: HS=1,WAIT=30,AC=10,DC=10
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_VOLTAGE_TYPE,      { 5,  0,  0,  0}, 0, ISOTP_STD }, // 0x161C VTYPE: HS=5s only
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGER_INPUT_POWER,        { 0,  0,  5,  5}, 0, ISOTP_STD }, // 0x161D grid power: AC+DC @5s (handler gated on AC charge)
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_DC_CHARGER_MAX_POWER,       { 0,  0,  0,  5}, 0, ISOTP_STD }, // 0x166A station max power: DC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_DC_CHARGER_MAX_CURRENT,     { 0,  0,  0,  5}, 0, ISOTP_STD }, // 0x1679 station max current: DC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_DC_CHARGER_MAX_VOLTAGE,     { 0,  0,  0,  5}, 0, ISOTP_STD }, // 0x1681 station max voltage: DC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGER_STATE_CLUSTER,      { 0,  0,  1,  0}, 0, ISOTP_STD }, // 0x1619 charger state cluster: AC @1s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGER_OUTPUT_POWER,       { 0,  0,  5,  0}, 0, ISOTP_STD }, // 0x161E charger output cluster: AC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AC_USABLE_POWER,            { 0,  0,  5,  0}, 0, ISOTP_STD }, // 0x1665 A/C useable power: AC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_MYROOM,                     { 0,  0,  5,  5}, 0, ISOTP_STD }, // 0x1692 My Room flag: AC+DC @5s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_AC_CONSUMPTION,             { 0,  0,  2,  2}, 0, ISOTP_STD }, // 0x106E A/C consumption power (PICS): AC+DC @2s
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGE_HISTORY,             { 5, 10,  5,  5}, 0, ISOTP_STD }, // 0x1688 outcome/history: HS=5,WAIT=10,AC=5,DC=5
  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGE_STOP_REQ,            { 1, 30,  5,  5}, 0, ISOTP_STD }, // 0x1667 stop-request: HS=1,WAIT=30,AC=5,DC=5

//  { PLUG_IN_CONTROL_SYSTEM_TX, PLUG_IN_CONTROL_SYSTEM_RX, VEHICLE_POLL_TYPE_READDATA, PID_CHARGING_CONTROL_INFORMATION, { 1, 1, 1, 1}, 0, ISOTP_STD }, // 0x1689 deferred

    // --- Heavy 96-cell multiframe arrays LAST (deferred first when a cycle is cut short) ---
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_TEMPERATURES,       { 0,  0, 30, 20}, 0, ISOTP_STD }, // 0x1814 cell-temp array (Hybrid Battery ECU): AC @30s, DC @20s (AC added so batt_temp_c logs during AC charge)
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_CELL_VOLTAGES,      { 0,  0, 60, 30}, 0, ISOTP_STD }, // 0x182E cell voltages: AC @60s, DC @30s
  { HYBRID_BATTERY_SYSTEM_TX,  HYBRID_BATTERY_SYSTEM_RX,  VEHICLE_POLL_TYPE_READDATA, PID_BATTERY_CAPACITY,           { 0,  0, 60, 60}, 0, ISOTP_STD }, // 0x1D3E per-module capacity: AC+DC @60s

    POLL_LIST_END
};

OvmsVehicleToyotaETNGA::OvmsVehicleToyotaETNGA()
{
    ESP_LOGI(TAG, "Toyota eTNGA platform module");

    // Init metrics
    InitializeMetrics();
    MyConfig.RegisterParam("xte", "Toyota eTNGA", true, true);

    // Init CAN
    RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

    // Set polling state directly (not via TransitionToSleepState, which would arm a
    // sleep cooldown and advance the backoff index — neither is wanted at boot).
    m_armed_for_charge = false;
    SetPollState(PollState::SLEEP);
    SetAwake(false);

    // Set polling PID lists. Block A (offset 0) is the default series covering the
    // non-charge states; block B is registered as a second series offset to the first
    // charge state so its 4 columns map to states CHARGE_HANDSHAKE..CHARGE_DC.
    PollSetPidList(m_can2, obdii_polls_base);

    auto charge_series = std::shared_ptr<OvmsPoller::StandardVehiclePollSeries>(
        new OvmsPoller::StandardVehiclePollSeries(
            nullptr, GetPollerSignal(), static_cast<uint16_t>(PollState::CHARGE_HANDSHAKE)));
    charge_series->PollSetPidList(2 /* CAN2 bus index */, obdii_polls_charge);
    // The "!v." name prefix is the poller's documented contract (poller/docs/API.rst):
    // series so named are auto-removed on vehicle shutdown. Without it this series held a
    // raw signal aliasing the vehicle's m_pollsignal and was dereferenced after teardown
    // freed it (use-after-free crash on a no-reboot vehicle switch).
    PollRequest(m_can2, "!v.xte.charge", charge_series);

    // BMS pack: owned by the e-TNGA platform (not per-badge). All e-TNGA EVs share the
    // Toyota EM "Type B" cell chemistry; only the cell/sensor counts and module grouping
    // vary by model year, and those are resolved per-reply from the bus (see PackModuleCount
    // + SetBatteryCellVoltages/SetBatteryTemperatures). Declare a bootstrap 96-cell
    // arrangement here so per-cell BMS routing is active from boot, before the first reply.
    BmsSetCellArrangementVoltage(96, 24);
    BmsSetCellArrangementTemperature(24, 6);

    BmsSetCellLimitsVoltage(2.5f, 4.3f);
    BmsSetCellLimitsTemperature(-30.0f, 60.0f);

    BmsSetCellDefaultThresholdsVoltage(0.020f, 0.030f);     // 20 mV warn / 30 mV alert
    BmsSetCellDefaultThresholdsTemperature(4.0f, 8.0f);     // 4 °C warn / 8 °C alert

    PollSetThrottling(0);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
#endif
}

OvmsVehicleToyotaETNGA::~OvmsVehicleToyotaETNGA()
{
    StopChargeIoTask();   // drain/stop the async charge-I/O worker before teardown
    ESP_LOGI(TAG, "Shutdown Toyota eTNGA platform module");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
#endif
}

void OvmsVehicleToyotaETNGA::NotifyVehicleOn()
{
    // Standard framework behavior first: the base sends the trip-log notification, which
    // must read the previous trip's metrics before they are reset below.
    OvmsVehicle::NotifyVehicleOn();

    ESP_LOGV(TAG, "Notification of vehicle on - Reset energy metrics for trip reporting");
    // Vehicle started. Reset the trip statistics (per-trip metrics only; *_total are lifetime)
    StandardMetrics.ms_v_bat_energy_used->SetValue(0);
    StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    StandardMetrics.ms_v_bat_coulomb_used->SetValue(0);
    StandardMetrics.ms_v_bat_coulomb_recd->SetValue(0);
    lastBatteryEnergyLogTime = 0;

    m_v_env_hvac_kwh_drive->SetValue(0);
    lastHvacDriveEnergyLogTime = 0;
}

// NotifyChargeStart is deliberately NOT overridden: the framework fires it on every
// ms_v_charge_state change to "charging" — including a CHARGE_WAIT pause/resume mid-
// session — so resetting the session energy counters there wiped earlier phases.
// The counters reset at session open in TransitionToChargeHandshakeState instead.

void OvmsVehicleToyotaETNGA::Ticker1(uint32_t ticker)
{
    // Aux-12V charge state is derived from the rail voltage every tick, independent of CAN and poll
    // state. The base 12V monitor in VehicleTicker1() samples v.b.12v.voltage.ref from this flag
    // later in the same tick, so it must be fresh before that runs.
    UpdateCharging12v();

    //ESP_LOGI(TAG, "Entering Ticker1: %d", ticker);
    ResetStaleMetrics();

    // Trip-average consumption (Wh/km) while driving — derived from net trip energy / distance.
    if (static_cast<PollState>(m_poll_state) == PollState::DRIVING) {
        float trip = StandardMetrics.ms_v_pos_trip->AsFloat();
        if (trip > 0.1f) {
            float net_wh = (StandardMetrics.ms_v_bat_energy_used->AsFloat()
                          - StandardMetrics.ms_v_bat_energy_recd->AsFloat()) * 1000.0f;
            StandardMetrics.ms_v_bat_consumption->SetValue(net_wh / trip);
        }
    }

    switch (static_cast<PollState>(m_poll_state)) {
        case PollState::SLEEP:
            HandleSleepState();
            break;

        case PollState::AWAKE:
            HandleAwakeState();
            break;

        case PollState::DRIVING:
            HandleDrivingState();
            break;

        case PollState::CHARGE_HANDSHAKE:
            HandleChargeHandshakeState();
            break;

        case PollState::CHARGE_WAIT:
            HandleChargeWaitState();
            break;

        case PollState::CHARGE_AC:
            HandleChargeAcState();
            break;

        case PollState::CHARGE_DC:
            HandleChargeDcState();
            break;

        default:
            ESP_LOGE(TAG, "Invalid poll state: %d", m_poll_state);
            break;
    }
}

