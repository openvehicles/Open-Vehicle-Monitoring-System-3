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
;    (C) 2019       Marko Juhanne
;    (C) 2021       Alexander Kiiashko
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

#include "ovms_log.h"
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include "ovms.h"
#include "ovms_config.h"
#include "ovms_notify.h"
#include "ovms_location.h"
#include "vehicle_voltampera.h"
#include "canutils.h"

#define VA_CANDATA_TIMEOUT 10

// Window position is a 3 bit field per window. 0 is closed; 6 is the largest value observed
// on a fully open window, so it is treated as 100%.
#define VA_WINDOW_OPEN_MAX     6
#define VA_WINDOW_UNKNOWN      5   // filler the BCM sends for a window it is not reporting
#define VA_WINDOW_SETTLE_SECS  4   // no position update for this long means travel has ended
// A full window travel takes a few seconds; keep nudging the bus awake across it so the final
// resting position is actually broadcast.
#define VA_WINDOW_WAKE_SECS    15
#define VA_WINDOW_CMD_REPEATS  4        // extra sends after the first, 1 s apart
#define VA_CMD_PENDING_SECS   10        // give up showing 'busy' after this long

#define VA_CHARGING_12V_THRESHOLD (float)12.7

// Pack model, used to turn the BECM's measured amp-hour capacity (PID 0x41a3 / 0x45ff) into
// the health and kWh figures owners actually look at. Both tables below, and the model-year
// split, are taken from the Voltage app (io.tripovan.voltage), which selects them by VIN.
//
// Amp-hours of a healthy pack: the denominator for state of health. GM changed cell capacity
// across the Gen1 run, so a single Gen1 figure would misreport an older car by up to 10
// points. Checked on a 2017 reporting 41.8Ah: 41.8/51.8 = 81%, against 82% in that app.
static float va_cac_fresh(int modelyear)
  {
  if (modelyear >= 16) return 51.8;       // Gen2 Volt, 2016 onwards
  if (modelyear >= 15) return 50.2;       // final Gen1 year
  if (modelyear >= 13) return 48.0;       // 2013-2014
  return 45.0;                            // 2011-2012
  }

// Nominal pack voltage, purely to express amp-hours as energy. These are NOT the pack's
// electrical nominal (96 groups x 3.7V would be 355V); they are scaling constants chosen so
// the product lands on the energy available for driving. The Gen1 value is exact against GM's
// published usable figure (45.0Ah x 230V = 10.35kWh vs 10.3), and the Gen2 value sits a little
// above GM's 14.0kWh but matches what owners measure on aged packs. No separate buffer
// fraction is applied on top: the constant already accounts for it.
//
// The Gen2 branch keys on model year alone, as the app does. There is no Gen2 Ampera, so an
// Ampera can only reach it via a VIN that decodes to 2016 or later, which should not happen.
static float va_pack_voltage(int modelyear)
  {
  return (modelyear >= 16) ? 297.0 : 230.0;
  }


#define VA_BCM 0x241
#define VA_TESTER_PRESENT_TIMEOUT 30*60 //  seconds

#define VA_POLLING_START_DELAY 2 // seconds
#define VA_POLLING_NORMAL_THROTTLING 1
#define VA_POLLING_HIGH_THROTTLING 20

// While AC charging, the Volt keeps both CAN buses silent (no broadcasts at all), but the
// BECM (0x7E4) and OBC (0x7E7) answer diagnostic requests. The APM also holds the 12V rail
// well above resting voltage (measured 13.38V charging vs 12.19V resting), which is the only
// bus-free signal that a charge has started while OVMS believes the car is asleep (deferred
// or scheduled charging never wakes the buses).
#define VA_CHARGING_12V_POLL_THRESHOLD (float)13.1
#define VA_SILENT_CHARGE_DELAY 15      // seconds 12V must hold above threshold before probing
#define VA_CHARGE_POLL_TIMEOUT 60      // seconds without poll replies in state 3 before sleeping
#define VA_SILENT_CHARGE_BACKOFF 300   // seconds to wait after an unanswered probe attempt


// Use states:
// 0 = bus is idle, car sleeping
// 1 = car is on and ready to Drive
// 2 = car wakeup or powertrain off, one request with high polling speed. After swith to state 1.
// 3 = car sleeping but AC charging: buses silent, poll BECM/OBC only (see ChargePollTicker)

static const OvmsPoller::poll_pid_t va_polls[]
  =
  {
    // Poll intervals per state { 0: sleeping, 1: driving, 2: wake sweep, 3: silent charging }.
    // State 3 only lists modules awake during an AC charge (BECM 0x7e4, OBC 0x7e7); the ECM,
    // TCM and drive units are powered down and would just time out.
    { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x002F, {  0, 600,  0,   0 },  0, ISOTP_STD }, // Fuel Level
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDII_1A,      0x0076, {  0, 30,   30, 30 },  0, ISOTP_STD }, // Charge mode: 1 immediate, 2 departure
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4531, {  0, 30,   30, 30 },  0, ISOTP_STD }, // Charger type / supply level
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369, {  0, 10,   10, 10 },  0, ISOTP_STD }, // On-board charger current
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4368, {  0, 10,   10, 10 },  0, ISOTP_STD }, // On-board charger voltage
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x434f, {  0, 10,   10, 60 },  0, ISOTP_STD }, // High-voltage Battery temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1c43, {  0, 10,   10, 60 },  0, ISOTP_STD }, // PEM temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8334, {  0, 10,   10, 10 },  0, ISOTP_STD }, // SOC displayed (8 bit)
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43af, {  0, 10,   10, 10 },  0, ISOTP_STD }, // SOC raw HD (16 bit)
    // Charge-side energy. The drive-cycle broadcasts are all discharge/usage; this is the only
    // charge figure, and at 10Wh steps it is 10x finer than DrvCycElEnrgUsd. Polled fastest in
    // state 3 because "last charge" is the RUNNING session while charging (unconfirmed on-car).
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x437d, {  0, 60,   60, 30 },  0, ISOTP_STD }, // AC input, last/current charge
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4389, {  0, 0,   300, 300 }, 0, ISOTP_STD }, // lifetime charge energy
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x432d, {  0, 1,    5,  10 },  0, ISOTP_STD }, // High-voltage Battery pack voltage
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40d4, {  0, 1,    5,  10 },  0, ISOTP_STD }, // High-voltage Battery pack current
    { 0x257, 0x657, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x28cb, {  0, 10,   0,   0 },  0, ISOTP_STD }, // Motor-generator A temperature
    { 0x258, 0x658, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x368f, {  0, 10,   0,   0 },  0, ISOTP_STD }, // Motor-generator B temperature
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1940, {  0, 60,   60,  0 },  0, ISOTP_STD }, // Transmission temperature (drive unit fallback)
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x41a6, {  0, 30,   30, 60 },  0, ISOTP_STD }, // EV range remaining
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x439e, {  0, 30,   30, 30 },  0, ISOTP_STD }, // Battery heater percent
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x41b6, {  0, 30,   30, 30 },  0, ISOTP_STD }, // Battery heater power
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x41a3, {  0, 600,  0,   0 },  0, ISOTP_STD }, // High-voltage Battery capacity
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x45ff, {  0, 600,  0,   0 },  0, ISOTP_STD }, // High-voltage Battery capacity (2019+)
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40d7, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 1 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40d9, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 2 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40db, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 3 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40dd, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 4 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40df, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 5 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40e1, {  0, 0,    60, 120 }, 0, ISOTP_STD }, // High-voltage Battery Section 6 temperature
    POLL_LIST_END
  };
// These are not polled anymore but instead received passively
// { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x000d, {  0, 10,  0 } }, // Vehicle speed
// { 0x7e1, 0x7e9, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2487, {  0, 100,  100 }, 0, ISOTP_STD }, // Distance Traveled on Battery Energy This Drive Cycle
// { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801f, {  0, 10,  0 } }, // Outside temperature (filtered)
// { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801e, {  0, 10,  0 } }, // Outside temperature (raw)


OvmsVehicleVoltAmpera::OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Volt/Ampera vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  m_type[0] = 'V';
  m_type[1] = 'A';
  m_type[2] = 0;
  m_type[3] = 0;
  m_type[4] = 0;
  m_type[5] = 0;
  m_modelyear = 0;
  m_charge_timer = 0;
  m_charge_wm = 0;
  m_candata_timer = VA_CANDATA_TIMEOUT;
  m_silent_charge_timer = 0;
  m_silent_charge_backoff = 0;
  m_last_poll_reply = 0;
  m_tx_retry_counter = 0;
  m_tester_present_timer = 0;
  m_controlled_lights = 0;
  m_startPolling_timer = 0;
  m_pPollingList = NULL;
  m_use_swcan_adapter = false;
  m_state2_swept = false;
  m_cac_from_41a3 = false;
  m_chargeseq = VA_CHGSEQ_IDLE;
  m_chargeseq_timer = 0;
  m_chargeseq_spoof = false;
  m_defer_count = 0;
  m_defer_active = false;
  m_defer_manual = false;
  m_limit_override = false;
  m_override_odo = 0;
  m_was_in_location = false;
  m_engine_mode = VA_ENG_AUTO;
  m_engine_ka_timer = 0;

  BmsSetCellArrangementVoltage(96, 16);
  BmsSetCellArrangementTemperature(6, 1);
  BmsSetCellLimitsVoltage(2,5);
  BmsSetCellLimitsTemperature(-35,90);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(4.0, 8.0);

  // VA metrics
  mt_charging_limits = MyMetrics.InitVector<int>("xva.v.b.charging_limits", SM_STALE_HIGH, "0");
  mt_coolant_temp = new OvmsMetricInt("xva.v.e.coolant_temp", SM_STALE_MIN, Celcius);
  mt_coolant_heater_pwr = new OvmsMetricFloat("xva.v.e.coolant_heater_pwr", SM_STALE_MIN, kW);
  mt_v_12v_voltage     = new OvmsMetricFloat("xva.v.b.12v.voltage", SM_STALE_MID, Volts);
  mt_v_12v_soc         = new OvmsMetricFloat("xva.v.b.12v.soc", SM_STALE_MID, Percentage);
  mt_hv_power_disp     = new OvmsMetricFloat("xva.v.b.power.disp", SM_STALE_MID, kW);
  mt_chargecycle_econ  = new OvmsMetricFloat("xva.v.b.chargecycle_econ", SM_STALE_HIGH, kWhP100K);
  mt_ac_evap_temp      = new OvmsMetricFloat("xva.v.ac.evap_temp", SM_STALE_MIN, Celcius);
  mt_ac_compressor_rpm = new OvmsMetricInt("xva.v.ac.compressor_rpm", SM_STALE_MIN);
  mt_heatercore_temp   = new OvmsMetricFloat("xva.v.e.heatercore_temp", SM_STALE_MID, Celcius);
  mt_fuel_used         = new OvmsMetricFloat("xva.v.e.fuel.used", SM_STALE_HIGH);
  mt_bat_heater_pct    = new OvmsMetricFloat("xva.v.b.heater.pct", SM_STALE_MID, Percentage);
  mt_bat_heater_pwr    = new OvmsMetricFloat("xva.v.b.heater.pwr", SM_STALE_MID);
  mt_mot_temp_mga      = new OvmsMetricFloat("xva.v.m.temp.mga", SM_STALE_MID, Celcius);
  mt_mot_temp_mgb      = new OvmsMetricFloat("xva.v.m.temp.mgb", SM_STALE_MID, Celcius);
  mt_fuel_level = new OvmsMetricInt("xva.v.e.fuel", SM_STALE_HIGH, Percentage, true);
  mt_v_trip_ev = new OvmsMetricFloat("xva.v.p.trip.ev", SM_STALE_HIGH, Kilometers);
  mt_charge_level      = new OvmsMetricString("xva.v.c.level", SM_STALE_HIGH);
  mt_charge_deferred   = new OvmsMetricBool("xva.v.c.deferred", SM_STALE_HIGH);
  mt_soc_displayed     = new OvmsMetricFloat("xva.v.b.soc.displayed", SM_STALE_MID, Percentage);
  mt_soc_raw           = new OvmsMetricFloat("xva.v.b.soc.raw", SM_STALE_MID, Percentage);
  mt_window_drv        = new OvmsMetricFloat("xva.v.e.window.driver", SM_STALE_HIGH, Percentage);
  mt_window_pass       = new OvmsMetricFloat("xva.v.e.window.passenger", SM_STALE_HIGH, Percentage);
  mt_window_lr         = new OvmsMetricFloat("xva.v.e.window.rearleft", SM_STALE_HIGH, Percentage);
  mt_window_rr         = new OvmsMetricFloat("xva.v.e.window.rearright", SM_STALE_HIGH, Percentage);
  mt_windows           = new OvmsMetricString("xva.v.e.windows", SM_STALE_NONE);
  // Startup value only. Position is knowable ONLY while the glass moves, so at boot there is
  // nothing to report until something does, and an empty metric makes the dashboard control
  // disappear. Seed closed as the opening assumption; the first real movement replaces it.
  //
  // This is deliberately NOT applied to the idle pattern on 0x325. That pattern carries no
  // position either, but writing "closed" every time it arrives destroys genuine state: a
  // window left open would be reported shut seconds later, once the bus went quiet, and a
  // toggle acting on that reading then sends the wrong direction. Ignore idle, seed once here.
  mt_windows->SetValue("closed");
  mt_cmd_pending       = new OvmsMetricString("xva.v.e.cmd.pending", SM_STALE_NONE);
  mt_range_fuel        = new OvmsMetricFloat("xva.v.e.range.fuel", SM_STALE_HIGH, Kilometers);
  mt_range_total       = new OvmsMetricFloat("xva.v.range.total", SM_STALE_HIGH, Kilometers);
  mt_limit_override    = new OvmsMetricBool("xva.v.c.limit.override", SM_STALE_NONE);
  mt_limit_override->SetValue(false);
  // Drive-cycle ("since last charge") set, from the GMLAN low-speed broadcasts. SM_STALE_HIGH
  // throughout: these are slow counters, not live telemetry, and the last value stays true
  // while the car sleeps.
  mt_dc_energy_used    = new OvmsMetricFloat("xva.v.dc.energy.used", SM_STALE_HIGH, kWh);
  mt_charge_inhibit    = new OvmsMetricInt("xva.v.c.inhibit", SM_STALE_HIGH);
  mt_dc_pct1           = new OvmsMetricFloat("xva.v.dc.energy.pct1", SM_STALE_HIGH, Percentage);
  mt_dc_pct2           = new OvmsMetricFloat("xva.v.dc.energy.pct2", SM_STALE_HIGH, Percentage);
  mt_dc_pct3           = new OvmsMetricFloat("xva.v.dc.energy.pct3", SM_STALE_HIGH, Percentage);
  mt_dc_pct4           = new OvmsMetricFloat("xva.v.dc.energy.pct4", SM_STALE_HIGH, Percentage);
  mt_dc_dist_batt      = new OvmsMetricFloat("xva.v.dc.dist.batt", SM_STALE_HIGH, Kilometers);
  mt_dc_dist_fuel      = new OvmsMetricFloat("xva.v.dc.dist.fuel", SM_STALE_HIGH, Kilometers);
  mt_dc_dist_total     = new OvmsMetricFloat("xva.v.dc.dist.total", SM_STALE_HIGH, Kilometers);
  mt_dc_batt_ratio     = new OvmsMetricFloat("xva.v.dc.batt.ratio", SM_STALE_HIGH, Percentage);
  mt_dc_eff_batt       = new OvmsMetricFloat("xva.v.dc.eff.batt", SM_STALE_HIGH, Percentage);
  mt_dc_eff_cabin      = new OvmsMetricFloat("xva.v.dc.eff.cabin", SM_STALE_HIGH, Percentage);
  mt_dc_eff_drive      = new OvmsMetricFloat("xva.v.dc.eff.drive", SM_STALE_HIGH, Percentage);
  mt_dc_eff_total      = new OvmsMetricFloat("xva.v.dc.eff.total", SM_STALE_HIGH, Percentage);
  mt_dc_fuel_used      = new OvmsMetricFloat("xva.v.dc.fuel.used", SM_STALE_HIGH);   // liters
  mt_dc_fuel_econ      = new OvmsMetricFloat("xva.v.dc.fuel.econ", SM_STALE_HIGH);   // L/100km
  // Service life, read only. See IncomingDriveCycleSWCAN for why there is no reset here.
  mt_oil_life          = new OvmsMetricFloat("xva.v.e.oil.life", SM_STALE_NONE, Percentage, true);
  // OVMS's own tally, kept whether or not the car's reset can be reproduced. Persistent so a
  // module reboot does not silently restart the count mid-cycle.
  mt_dc_energy_own     = new OvmsMetricFloat("xva.v.dc.energy.own", SM_STALE_NONE, kWh, true);
  mt_dc_distance_own   = new OvmsMetricFloat("xva.v.dc.distance.own", SM_STALE_NONE, Kilometers, true);
  mt_charge_input      = new OvmsMetricFloat("xva.v.c.energy.input", SM_STALE_HIGH, kWh);
  mt_charge_lifetime   = new OvmsMetricFloat("xva.v.c.energy.lifetime", SM_STALE_NONE, kWh, true);

  mt_engine_mode       = new OvmsMetricString("xva.v.e.mode", SM_STALE_NONE);
  mt_engine_mode->SetValue("auto");

  // Config parameters
  MyConfig.RegisterParam("xva", "Volt/Ampera", true, true);
  ConfigChanged(NULL);

  ClimateControlInit();

  // Vehicle-specific controls. No standard OVMS command covers engine/trunk/windows/horn,
  // so they live under "xva". All are gated on config xva/control.enabled.
  cmd_xva = MyCommandApp.RegisterCommand("xva", "Volt/Ampera controls");
  OvmsCommand* cmd_engine = cmd_xva->RegisterCommand("engine", "Internal combustion engine control");
  cmd_engine->RegisterCommand("on", "Force the engine on (held by keep-alive)", shell_engine, "<pin>", 1, 1);
  cmd_engine->RegisterCommand("off", "Force the engine off (refused at or below 16% SOC)", shell_engine, "<pin>", 1, 1);
  cmd_engine->RegisterCommand("auto", "Release the override, HPCM back in control", shell_engine);
  cmd_engine->RegisterCommand("release", "Alias for 'auto'", shell_engine);
  cmd_xva->RegisterCommand("trunk", "Release the trunk/hatch", shell_trunk, "<pin>", 1, 1);
  cmd_xva->RegisterCommand("horn", "Sound the horn", shell_alert);
  cmd_xva->RegisterCommand("flash", "Flash the exterior lights", shell_alert);
  cmd_xva->RegisterCommand("locate", "Horn and lights together (OnStar vehicle locate)", shell_alert);
  OvmsCommand* cmd_cl = cmd_xva->RegisterCommand("chargelimit", "SOC charge limit", shell_chargelimit);
  cmd_cl->RegisterCommand("status", "Show charge limit state", shell_chargelimit);
  cmd_cl->RegisterCommand("override", "Charge to full this time (resets on leaving)", shell_chargelimit);
  cmd_cl->RegisterCommand("resume", "Cancel the override, re-arm the limit", shell_chargelimit);
  OvmsCommand* cmd_win = cmd_xva->RegisterCommand("windows", "Window control");
  cmd_win->RegisterCommand("up", "Close all windows", shell_windows, "<pin>", 1, 1);
  cmd_win->RegisterCommand("down", "Open all windows", shell_windows, "<pin>", 1, 1);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // powertrain bus
  //RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // chassis bus 
  
  const int nCellListSize = 96; 
  const int va_pollsSize = sizeof(va_polls)/sizeof(va_polls[0]);
  m_pPollingList = new OvmsPoller::poll_pid_t[va_pollsSize + nCellListSize];
  uint16_t pid = 0x4181;
  
  int iPoll=0;
  for(iPoll=0; iPoll<nCellListSize; iPoll++)
    {
    if(iPoll==31)
      pid+=96;

    //Updating of the cells takes significant time. To prevent fake deviation, start updates in state 2 only (without HV battery load)
    m_pPollingList[iPoll] = { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pid, {0, 0, 10}, 0, ISOTP_STD };
    pid++;
    }

  for(int i=0; i<va_pollsSize; i++)
    {
      m_pPollingList[iPoll] = va_polls[i];
      iPoll++;
    }

  PollSetPidList(m_can1, m_pPollingList);
  PollSetState(0);

  if(!m_use_swcan_adapter)
  {
    ESP_LOGI(TAG, "Register 2nd MCP2515 as SWCAN can3");
    RegisterCanBus(3, CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);  // single wire can
    p_swcan = m_can3;
  }
  else
  {
    // External SWCAN module with MCP2515
    #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    ESP_LOGI(TAG, "Register external SWCAN on can4");
    swcan::Init();
    RegisterCanBus(4, CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);  // single wire can
    p_swcan = m_can4;
    p_swcan_if = (swcan*)MyPcpApp.FindDeviceByName("can4");
    #endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleVoltAmpera::TxCallback, this, _1, _2), true);

  wakeup_frame_sent = std::bind(&OvmsVehicleVoltAmpera::CommandWakeupComplete, this, _1, _2);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif  
  }

OvmsVehicleVoltAmpera::~OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Shutdown Volt/Ampera vehicle module");
  MyCan.DeregisterCallback(TAG);
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebCleanup();
#endif
  PollSetPidList(m_can1, NULL); 
  delete m_pPollingList; 

  MyMetrics.DeregisterMetric(mt_coolant_temp);
  if (cmd_xva != NULL)
    {
    MyCommandApp.UnregisterCommand(cmd_xva->GetName());
    cmd_xva = NULL;
    }

  MyMetrics.DeregisterMetric(mt_coolant_heater_pwr);
  MyMetrics.DeregisterMetric(mt_v_12v_voltage);
  MyMetrics.DeregisterMetric(mt_v_12v_soc);
  MyMetrics.DeregisterMetric(mt_hv_power_disp);
  MyMetrics.DeregisterMetric(mt_chargecycle_econ);
  MyMetrics.DeregisterMetric(mt_ac_evap_temp);
  MyMetrics.DeregisterMetric(mt_ac_compressor_rpm);
  MyMetrics.DeregisterMetric(mt_heatercore_temp);
  MyMetrics.DeregisterMetric(mt_fuel_used);
  MyMetrics.DeregisterMetric(mt_bat_heater_pct);
  MyMetrics.DeregisterMetric(mt_bat_heater_pwr);
  MyMetrics.DeregisterMetric(mt_mot_temp_mga);
  MyMetrics.DeregisterMetric(mt_mot_temp_mgb);
  MyMetrics.DeregisterMetric(mt_fuel_level);
  MyMetrics.DeregisterMetric(mt_v_trip_ev);
  MyMetrics.DeregisterMetric(mt_charge_level);
  MyMetrics.DeregisterMetric(mt_charge_deferred);
  MyMetrics.DeregisterMetric(mt_soc_displayed);
  MyMetrics.DeregisterMetric(mt_soc_raw);
  MyMetrics.DeregisterMetric(mt_limit_override);
  MyMetrics.DeregisterMetric(mt_range_fuel);
  MyMetrics.DeregisterMetric(mt_range_total);
  MyMetrics.DeregisterMetric(mt_window_drv);
  MyMetrics.DeregisterMetric(mt_window_pass);
  MyMetrics.DeregisterMetric(mt_window_lr);
  MyMetrics.DeregisterMetric(mt_window_rr);
  MyMetrics.DeregisterMetric(mt_windows);
  MyMetrics.DeregisterMetric(mt_cmd_pending);
  MyMetrics.DeregisterMetric(mt_dc_energy_used);
  MyMetrics.DeregisterMetric(mt_charge_inhibit);
  MyMetrics.DeregisterMetric(mt_dc_pct1);
  MyMetrics.DeregisterMetric(mt_dc_pct2);
  MyMetrics.DeregisterMetric(mt_dc_pct3);
  MyMetrics.DeregisterMetric(mt_dc_pct4);
  MyMetrics.DeregisterMetric(mt_dc_dist_batt);
  MyMetrics.DeregisterMetric(mt_dc_dist_fuel);
  MyMetrics.DeregisterMetric(mt_dc_dist_total);
  MyMetrics.DeregisterMetric(mt_dc_batt_ratio);
  MyMetrics.DeregisterMetric(mt_dc_eff_batt);
  MyMetrics.DeregisterMetric(mt_dc_eff_cabin);
  MyMetrics.DeregisterMetric(mt_dc_eff_drive);
  MyMetrics.DeregisterMetric(mt_dc_eff_total);
  MyMetrics.DeregisterMetric(mt_dc_fuel_used);
  MyMetrics.DeregisterMetric(mt_dc_fuel_econ);
  MyMetrics.DeregisterMetric(mt_oil_life);
  MyMetrics.DeregisterMetric(mt_dc_energy_own);
  MyMetrics.DeregisterMetric(mt_dc_distance_own);
  MyMetrics.DeregisterMetric(mt_charge_input);
  MyMetrics.DeregisterMetric(mt_charge_lifetime);
  MyMetrics.DeregisterMetric(mt_engine_mode);
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleVoltAmpera::ConfigChanged(OvmsConfigParam* param)
  {
  if (param && param->GetName() != "xva")
    return;

  ESP_LOGD(TAG, "Volt/Ampera reload configuration");

  // The VIN only arrives once the car is awake and broadcasting, but it is what selects the
  // pack constants. Remember the decoded year so battery health and capacity are available
  // straight after a reboot instead of staying blank until the car is next used.
  if (m_modelyear == 0)
    m_modelyear = MyConfig.GetParamValueInt("xva", "modelyear", 0);
    m_defer_unclear = MyConfig.GetParamValueBool("xva", "chargelimit.unclear", false);

  m_range_rated_km = MyConfig.GetParamValueInt("xva", "range.km", 0);
  m_extended_wakeup = MyConfig.GetParamValueBool("xva", "extended_wakeup", false);
  m_notify_metrics = MyConfig.GetParamValueBool("xva", "notify_va_metrics", false);
  m_use_swcan_adapter = MyConfig.GetParamValueBool("xva", "use_swcan_adapter", false);

  m_chargelimit_enabled  = MyConfig.GetParamValueBool("xva", "chargelimit.enabled", false);
  m_chargelimit_soc      = MyConfig.GetParamValueInt("xva", "chargelimit.soc", 80);
  m_chargelimit_maxdefer = MyConfig.GetParamValueInt("xva", "chargelimit.maxdefer", 20);
  m_chargelimit_notify   = MyConfig.GetParamValueBool("xva", "chargelimit.notify", true);
  m_chargelimit_debug    = MyConfig.GetParamValueBool("xva", "chargelimit.debug", false);
  m_chargelimit_location = MyConfig.GetParamValue("xva", "chargelimit.location");
  // The limit is a decision about the pack, not about the dashboard, so it defaults to raw
  // like the Voltage app does. Independent of soc.source: the owner can watch the dashboard
  // number and still cap the real charge level.
  m_chargelimit_use_raw  = (MyConfig.GetParamValue("xva", "chargelimit.source", "raw") == "raw");
  m_chargelimit_scale_warned = false;
  m_control_enabled      = MyConfig.GetParamValueBool("xva", "control.enabled", false);
  // Which scale feeds the standard v.b.soc. Defaults to the dashboard number, the figure most
  // consumers expect; the raw value is always published alongside either way.
  m_soc_use_raw          = (MyConfig.GetParamValue("xva", "soc.source", "displayed") == "raw");
  m_soc_scale_warned     = false;
  UpdateSoc();

  // Publish the target so the app/UI can show it. 0 when the feature is off.
  if (m_chargelimit_enabled && m_chargelimit_soc > 0 && m_chargelimit_soc < 100)
    StandardMetrics.ms_v_charge_limit_soc->SetValue((float)m_chargelimit_soc, Percentage);
  else
    StandardMetrics.ms_v_charge_limit_soc->SetValue(0, Percentage);

  UpdateBatteryCapacity();
  }

// Publish battery health and usable energy, both derived from the capacity the BECM measures,
// so they follow the pack as it ages instead of being fixed per model.
//
// Worked example, a 2017 reporting 41.8Ah: health 41.8/51.8 = 81%, usable 41.8 x 297V = 12.4kWh.
// v.b.soc follows whichever scale the owner picked. Both are always published under xva.*,
// so switching the setting never loses data and consumers can pin to one explicitly.
void OvmsVehicleVoltAmpera::UpdateSoc()
  {
  OvmsMetricFloat* src = m_soc_use_raw ? mt_soc_raw : mt_soc_displayed;

  // Fall back rather than publish nothing. 0x43af is confirmed on Gen 2 only, and v.b.soc is
  // persistent, so an early return here would not blank it: it would freeze it at a stale
  // value forever, with the app, the server and the framework's MINSOC alert all reading it
  // and nothing anywhere saying why.
  if (!src->IsDefined())
    {
    OvmsMetricFloat* other = m_soc_use_raw ? mt_soc_displayed : mt_soc_raw;
    if (other->IsDefined() && !m_soc_scale_warned)
      {
      m_soc_scale_warned = true;
      ESP_LOGW(TAG, "soc.source is %s but that PID has not answered; publishing v.b.soc from "
                    "the %s scale instead",
               m_soc_use_raw ? "raw" : "displayed", m_soc_use_raw ? "displayed" : "raw");
      }
    src = other;
    }
  if (src->IsDefined())
    StandardMetrics.ms_v_bat_soc->SetValue(src->AsFloat(), Percentage);

  // Ideal range stays on the displayed scale whichever source is selected: rated range is
  // the distance to a flat dashboard, and raw never reaches either end of its own scale.
  // Deliberately outside the block above: its only input is the displayed metric, so a
  // silent raw PID must not stop it updating.
  if (m_range_rated_km != 0 && mt_soc_displayed->IsDefined())
    StandardMetrics.ms_v_bat_range_ideal->SetValue(
      mt_soc_displayed->AsFloat() * m_range_rated_km / 100, Kilometers);
  }

// The physical state of the battery, for decisions that must not depend on a display setting.
// Falls back to v.b.soc only until the raw PID has answered once.
float OvmsVehicleVoltAmpera::SocRaw()
  {
  if (mt_soc_raw->IsDefined())
    return mt_soc_raw->AsFloat();
  return StandardMetrics.ms_v_bat_soc->AsFloat();
  }

// SOC on whichever scale the charge limit is set against. Every comparison in the limiter
// path has to go through here, including the hysteresis and the "was this pause ours" test:
// mixing scales would put the resume threshold and the target on different rulers.
float OvmsVehicleVoltAmpera::ChargeLimitSoc()
  {
  OvmsMetricFloat* want = m_chargelimit_use_raw ? mt_soc_raw : mt_soc_displayed;
  if (want->IsDefined())
    return want->AsFloat();

  // The selected scale has not answered. Falling back beats silently never enforcing (0x43af
  // is confirmed on Gen 2 only), but it does move the effective cut-off by an amount that
  // varies with charge level (the two scales track each other closely near 80 and diverge by
  // over 12% down low), so say so rather than capping wrong quietly. Re-armed on every
  // config change.
  OvmsMetricFloat* other = m_chargelimit_use_raw ? mt_soc_displayed : mt_soc_raw;
  if (!other->IsDefined())
    return StandardMetrics.ms_v_bat_soc->AsFloat();   // nothing polled yet, not a substitution

  if (!m_chargelimit_scale_warned)
    {
    m_chargelimit_scale_warned = true;
    ESP_LOGW(TAG, "Charge limit is set against %s SOC but that PID has not answered; "
                  "falling back to the other scale, so the cut-off will be off target "
                  "unless the limit is 80%%",
             m_chargelimit_use_raw ? "raw" : "displayed");
    }
  return other->AsFloat();
  }

// The one-off "charge to full this time" flag, mirrored to a metric so Home Assistant can show
// it as a switch rather than a pair of stateless buttons. Always set it through here.
void OvmsVehicleVoltAmpera::SetLimitOverride(bool on)
  {
  m_limit_override = on;
  mt_limit_override->SetValue(on);
  }

// Total range is what the dash shows as the combined figure: whatever is left in the pack plus
// whatever is left in the tank. Published only once both halves have been seen, so it never
// briefly reads as just one of them.
void OvmsVehicleVoltAmpera::UpdateRangeTotal()
  {
  if (!StandardMetrics.ms_v_bat_range_est->IsDefined() || !mt_range_fuel->IsDefined())
    return;
  mt_range_total->SetValue(roundf(StandardMetrics.ms_v_bat_range_est->AsFloat()
                                  + mt_range_fuel->AsFloat()), Kilometers);
  }

void OvmsVehicleVoltAmpera::UpdateBatteryCapacity()
  {
  if (m_modelyear == 0)
    return;                     // VIN not decoded yet; called again from IncomingFrameCan1

  float cac = StandardMetrics.ms_v_bat_cac->AsFloat();
  if (cac > 0)
    StandardMetrics.ms_v_bat_soh->SetValue(cac / va_cac_fresh(m_modelyear) * 100, Percentage);

  // The owner's figure if they set one, else derived. Falling back to a healthy pack of this
  // model year keeps the metric right when an override is cleared before the car has ever
  // reported a capacity, which would otherwise leave the old value published.
  float kwh = MyConfig.GetParamValueFloat("xva", "battery.capacity", 0);
  if (kwh <= 0)
    kwh = ((cac > 0) ? cac : va_cac_fresh(m_modelyear)) * va_pack_voltage(m_modelyear) / 1000;
  if (kwh > 0)
    StandardMetrics.ms_v_bat_capacity->SetValue(kwh, kWh);
  }

void OvmsVehicleVoltAmpera::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle:  Volt Ampera (%s %d)\n", m_type, m_modelyear+2000);
  writer->printf("VIN:      %s\n",m_vin);
  writer->puts("");
  writer->printf("Ranges:   %dkm (rated) %dkm (estimated)\n",
    m_range_rated_km, StandardMetrics.ms_v_bat_range_est->AsInt());
  writer->printf("SOC:      %.1f%% displayed, %.1f%% raw (v.b.soc follows %s)\n",
    mt_soc_displayed->AsFloat(), mt_soc_raw->AsFloat(),
    m_soc_use_raw ? "raw" : "displayed");
  writer->printf("Charge:   Timer %d (%d wm)\n", m_charge_timer, m_charge_wm);
  writer->printf("Can Data: Timer %d (poll state %d)\n",m_candata_timer,m_poll_state);
  ClimateControlPrintStatus(verbosity,writer);
  }


void OvmsVehicleVoltAmpera::TxCallback(const CAN_frame_t* p_frame, bool success)
  {
  const uint8_t *d = p_frame->data.u8;
  if (p_frame->MsgID == 0x7e4)
    {
    if (!success)
      ESP_LOGE(TAG, "TxCallback. Error sending poll request. MsgId: 0x%" PRIx32, p_frame->MsgID);
    return;
    }

  if (success) 
    {
    ESP_LOGD(TAG,"TxCallback. Success. Frame %08" PRIx32 ": [%02x %02x %02x %02x %02x %02x %02x %02x]", 
      p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
    } 
  else
    {
    ESP_LOGE(TAG,"TxCallback. Failed! Frame %08" PRIx32 ": [%02x %02x %02x %02x %02x %02x %02x %02x]", 
      p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
    }
  }


void OvmsVehicleVoltAmpera::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  int k;

  ESP_LOGV(TAG,"CAN1 message received: %08" PRIx32 ": [%02x %02x %02x %02x %02x %02x %02x %02x]",
    p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  if ((p_frame->MsgID & 0xff8) == 0x7e8)
    return; // Ignore poll responses

  // Only BROADCAST traffic counts as evidence the car is awake, which is why this sits below
  // the poll-response return rather than above it.
  //
  // Refreshing on every frame creates a loop the car cannot escape: after a drive the module
  // enters the state 2 cell sweep, the BECM answers, those answers reset the timer, so the
  // sleep timeout never fires, so polling never stops, and the polling itself is what keeps
  // the BECM awake. Observed on a MY2017: four hours parked with v.e.awake stuck on, poll
  // state 2 still running, and the 12V battery down to 11.88V and falling.
  m_candata_timer = VA_CANDATA_TIMEOUT;


  // Activity on the bus means the car is up. Flagging awake is unconditional and idempotent;
  // arming the poll-start timer is the part that matters, and only applies in state 0 (in
  // state 3 the charge polls are already running, and NotifiedVehicleOn handles a real
  // power-on).
  //
  // Deliberately NOT gated on ms_v_env_awake being false.
  // That metric is PERSISTENT (metrics_standard.cpp: persist=true), so after a reboot it comes
  // back true and the false->true edge never happens again, so polling never starts for as
  // long as the car keeps chattering. No SOC, no charge state, and the charge limit silently
  // dead, with no error anywhere, and the only recovery a manual "metrics set v.e.awake no".
  StandardMetrics.ms_v_env_awake->SetValue(true);
  if (m_poll_state == 0 && m_startPolling_timer == 0)
    {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    m_startPolling_timer = VA_POLLING_START_DELAY;
    }

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    case 0x0c9: 
      {
      StandardMetrics.ms_v_env_on->SetValue((d[0] & 0xC0) != 0);  // true for Ign ON, remote preheating
      StandardMetrics.ms_v_mot_rpm->SetValue( GET16(p_frame, 1) >> 2 );
      break;
      }

    /*
    case 0x1ef: {
      // Which is better source of engine RPM?
      StandardMetrics.ms_v_mot_rpm->SetValue( GET16(p_frame, 2) );
      break;
      }
    */

    /*
    // Which is more reliable indicator of Engine On?
    case 0x0bc: {
      bool powertrain_enabled = (d[0] & 0x80) != 0);
      if (StandardMetrics.ms_v_env_on->GetValueAsBool() != powertrain_enabled)
        {
        ESP_LOGI(TAG,"Powertrain %s",powertrain_enabled ? "enabled", "disabled");
        StandardMetrics.ms_v_env_on->SetValue(powertrain_enabled);
        }
      break;
      }
    */

    case 0x120: 
      {
      StandardMetrics.ms_v_pos_odometer->SetValue(GET32(p_frame, 0) / 64);
      break;
      } 

    case 0x3e9:
      {
      StandardMetrics.ms_v_pos_speed->SetValue(GET16(p_frame, 0)/100, Mph);
      break;
      } 

    case 0x1a1:
      {
      StandardMetrics.ms_v_env_throttle->SetValue( d[7]*100/256 );
      break;
      } 

    case 0xd1:
      {
      StandardMetrics.ms_v_env_footbrake->SetValue( d[4] );
      break;
      } 

    case 0x4c1: 
      {
      // Outside temperature (filtered) (aka ambient temperature)
      StandardMetrics.ms_v_env_temp->SetValue((int)d[4]/2 - 0x28);
      // Coolant temp
      mt_coolant_temp->SetValue((int)d[2] - 0x28);
      break;
      }

    // VIN digits 10-17
    case 0x4e1:
      {
      for (k=0;k<8;k++)
        m_vin[k+9] = d[k];
      break;
      }

    // VIN digits 2-9
    case 0x514:
      {
      for (k=0;k<8;k++)
        m_vin[k+1] = d[k];
      if (m_vin[9] != 0)
        {
        m_vin[0] = '1';
        m_vin[17] = 0;
        if (m_vin[2] == '1')
          m_type[2] = 'V';
        else if (m_vin[2] == '0')
          m_type[2] = 'A';
        else
          m_type[2] = m_vin[2];
        m_modelyear = (m_vin[9]-'A')+10;
        m_type[3] = (m_modelyear / 10) + '0';
        m_type[4] = (m_modelyear % 10) + '0';
        StandardMetrics.ms_v_vin->SetValue(m_vin);
        StandardMetrics.ms_v_type->SetValue(m_type);
        if (m_range_rated_km == 0)
          {
          switch (m_modelyear)
            {
            case 11:
            case 12:
              m_range_rated_km = 56;
              break;
            case 13:
            case 14:
            case 15:
              m_range_rated_km = 61;
              break;
            default:
              m_range_rated_km = 85;
              break;
            }
          }
        }
      if (mt_charging_limits->AsString()=="0")
        {
        if (m_type[2]=='V')
          {
          mt_charging_limits->SetValue("12,8");
          }
        else if (m_type[2]=='A')
          {
          mt_charging_limits->SetValue("10,6");
          }
        }
      // Persist it so the pack constants survive a reboot while the car sleeps (ConfigChanged)
      if (MyConfig.GetParamValueInt("xva", "modelyear", 0) != m_modelyear)
        MyConfig.SetParamValueInt("xva", "modelyear", m_modelyear);
      UpdateBatteryCapacity();  // model year is known now, so the constants can be picked
      break;
      }

    case 0x1f5: // Byte 4: Shift Position PRNDL 1=Park, 2=Reverse, 3=Neutral, 4=Drive, 5=Low
      {
      if (d[3]==1) 
        StandardMetrics.ms_v_env_gear->SetValue(-2); // Park
      else if (d[3]==2)
        StandardMetrics.ms_v_env_gear->SetValue(-1); // Reverse
      else if (d[3]==3)
        StandardMetrics.ms_v_env_gear->SetValue(0); // Neutral
      else if (d[3]==4)
        StandardMetrics.ms_v_env_gear->SetValue(1); // Drive
      else if (d[3]==5)
        StandardMetrics.ms_v_env_gear->SetValue(2); // L
      break;
      }

    default:
      break;
    }
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  //uint8_t *d = p_frame->data.u8;
  //ESP_LOGI(TAG,"CAN2 message received: %08x: %02x %02x %02x %02x %02x %02x %02x %02x", 
  //  p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  IncomingFrameCan4(p_frame);  // assume third can bus messages coming from SWCAN bus
  }

//=====================================================================================
// Drive-cycle ("since last charge") energy data, and service life counters.
//
// All GMLAN low-speed BROADCASTS: nothing is requested, nothing is left hanging. Matched on
// the 13-bit PID rather than the full arbitration ID, because priority and source bits differ
// between senders and between cars (PID 0x391 arrives as 0x107220A9 on this one), so the
// exact-ID switch below would silently miss them elsewhere.
//
// Why these matter here: the car clears its drive-cycle counters when it charges to 100%.
// With the SOC limit in place it never does, so they run on forever. There is no way to
// reproduce the reset from the bus, so they are read only and mirrored when the car clears them.
//=====================================================================================

// DBC Motorola (@0+) "sawtooth" bit numbering: bit n lives in byte n/8 at bit n%8 counted from
// the LSB. A big-endian signal starts at its MSB and walks DOWN within the byte, jumping to
// bit 7 of the next byte at each boundary, which is why start=39 len=8 means "all of byte 4"
// and not "bits spanning bytes 4 and 5". EngOilRmnLf (39|8 -> byte 4) and FuelFltRmnLf
// (63|8 -> byte 7) are the two signals whose byte position is independently known, and they
// are what pins this implementation.
static uint32_t va_dbc_be(const uint8_t* d, uint8_t dlc, int start, int len)
  {
  uint32_t v = 0;
  int bit = start;
  for (int i = 0; i < len; i++)
    {
    int byte = bit >> 3;
    if (byte < 0 || byte >= dlc)
      return 0;                     // backstop; every caller already checks the DLC
    v = (v << 1) | ((d[byte] >> (bit & 7)) & 1);
    bit = ((bit & 7) == 0) ? bit + 15 : bit - 1;
    }
  return v;
  }

void OvmsVehicleVoltAmpera::IncomingDriveCycleSWCAN(CAN_frame_t* p_frame)
  {
  const uint8_t* d = p_frame->data.u8;
  uint8_t dlc = p_frame->FIR.B.DLC;
  uint16_t pid = (p_frame->MsgID >> 13) & 0x1FFF;

  switch (pid)
    {
    case 0x0141:  // Energy_Storage_System_LS
      {
      if (dlc < 8)
        break;
      // 0.36 MJ per LSB is exactly 0.1 kWh. Published in kWh because that is the unit the
      // car's own "since last charge" screen uses, which is what this decode is checked
      // against; MJ would make the comparison needlessly indirect.
      float used = va_dbc_be(d, dlc, 21, 14) * 0.1f;

      // A drop means the car cleared its counters, which it does ONLY on reaching 100%.
      // There is no manual reset anywhere in the DIC, and no bus message triggers one: a
      // full-bus ring-buffer capture across a reset shows 652s of
      // complete SWCAN silence spanning the moment it happened, so it is done internally while
      // the bus sleeps. Mirroring the drop is therefore the only way to keep the OVMS-side
      // tally aligned with the car's; there is nothing to replay.
      if (mt_dc_energy_used->IsDefined() && used < mt_dc_energy_used->AsFloat() - 0.5f)
        {
        ESP_LOGI(TAG, "Drive-cycle counters reset by the car (%.1f -> %.1f kWh)",
                 mt_dc_energy_used->AsFloat(), used);
        mt_dc_energy_own->SetValue(0, kWh);
        mt_dc_distance_own->SetValue(0, Kilometers);
        }

      mt_dc_energy_used->SetValue(used, kWh);
      // DrvCyclElecEngyEcon (32|9) and DrvCyclTrpDstTrvld (54|15) are NOT decoded: this car
      // transmits bytes 4..7 of this message as literal zeros, confirmed on the wire and
      // still zero after a drive with the engine running. The signals exist in the DBC but
      // are unused here.
      mt_charge_inhibit->SetValue((int)va_dbc_be(d, dlc, 36, 4));
      break;
      }

    case 0x0210:  // Drv_Cycl_Elec_Enrgy_Consumd_LS
      {
      if (dlc < 8)
        break;
      // The four-way energy split behind the dash's usage bar graph. Which slice is which is
      // not documented anywhere available, so they keep their DBC ordinals.
      mt_dc_pct1->SetValue(va_dbc_be(d, dlc, 23, 8) * 0.392157f, Percentage);
      mt_dc_pct2->SetValue(va_dbc_be(d, dlc, 31, 8) * 0.392157f, Percentage);
      mt_dc_pct3->SetValue(va_dbc_be(d, dlc, 39, 8) * 0.392157f, Percentage);
      mt_dc_pct4->SetValue(va_dbc_be(d, dlc, 47, 8) * 0.392157f, Percentage);
      break;
      }

    case 0x0225:  // Drive_Cycle_Efficiency_LS
      {
      if (dlc < 8)
        break;
      mt_dc_dist_batt->SetValue(va_dbc_be(d, dlc, 7, 17) * 0.015625f, Kilometers);
      mt_dc_dist_fuel->SetValue(va_dbc_be(d, dlc, 22, 17) * 0.015625f, Kilometers);
      mt_dc_dist_total->SetValue(va_dbc_be(d, dlc, 37, 17) * 0.015625f, Kilometers);
      mt_dc_batt_ratio->SetValue(va_dbc_be(d, dlc, 50, 11) * 0.048852f, Percentage);
      break;
      }

    case 0x0223:  // Drive_Cycle_Energy_Efficiency_LS
      {
      if (dlc < 8)
        break;
      mt_dc_eff_batt->SetValue(va_dbc_be(d, dlc, 7, 8) * 0.392157f, Percentage);
      mt_dc_eff_cabin->SetValue(va_dbc_be(d, dlc, 23, 8) * 0.392157f, Percentage);
      mt_dc_eff_drive->SetValue(va_dbc_be(d, dlc, 31, 8) * 0.392157f, Percentage);
      mt_dc_eff_total->SetValue(va_dbc_be(d, dlc, 39, 8) * 0.392157f, Percentage);
      {
      // DrvCyclFuelUsd 51|12 at 0.125 L. 0xFFF is the "not available" code, not 511.9 liters.
      uint32_t fuelused = va_dbc_be(d, dlc, 51, 12);
      if (fuelused != 0xFFF)
        mt_dc_fuel_used->SetValue(fuelused * 0.125f);   // liters, no unit enum
      }
      {
      // DrvCyclFuelEnmy 47|12 at 0.1 km/L, the car's own drive-cycle gasoline economy.
      // Saturates at 0xFFF (409.5 km/L) while no fuel has been burned this cycle, and 0 is
      // equally meaningless, so both are skipped. Published inverted as L/100km to sit
      // alongside the electric consumption figure.
      uint32_t kmpl = va_dbc_be(d, dlc, 47, 12);
      if (kmpl > 0 && kmpl < 0xFFF)
        mt_dc_fuel_econ->SetValue(100.0f / (kmpl * 0.1f));
      }
      break;
      }

    case 0x0325:  // Window_Position_Status_LS
      {
      if (dlc < 2)
        break;
      // Two bytes, 3 bits per window: drv byte0 bits 0-2, LR byte0 bits 3-5, Ps byte1 bits
      // 0-2, RR byte1 bits 3-5. Values run 0 (shut) to 6 (fully down).
      //
      // The message only carries real positions WHILE THE GLASS IS MOVING. At rest the BCM
      // emits a fixed idle pattern, 28 2d, meaning drv 0 and the other three at 5, and it
      // sends that same pattern regardless of where the windows actually are. Proven on-car
      // with all four windows physically DOWN and at rest, four separate bus wakes all return
      // 28 2d, byte-identical to the pattern captured with every window shut. The only other
      // values seen, such as 06 00, arrive while the glass is traveling.
      //
      // So there is no way to read position at rest, and no way to force a report: only
      // moving the glass produces real data. What does work is remembering the last value
      // seen during a move, because that IS where the window came to rest. Ignore the idle
      // pattern completely rather than decoding it, or a resting-open window reads as shut.
      //
      // 5 is that idle filler, and it is per window rather than only whole-frame. Measured
      // over 476 logged readings: the driver window reports 0, 1, 2, 3 and 6, travelling
      // through the intermediate positions and never once landing on 5. The other three
      // report only 0, 5 and 6, never an intermediate value at all. So 5 is not a position on
      // this car, it is "no reading", and a window can carry it while its neighbours carry
      // real values. Starting or stopping remote climate makes exactly that happen: the rear
      // fields flip to 5 while the front stay put, which decodes as both rear windows sliding
      // 83% open and back with nothing having moved.
      static const uint8_t shift[4] = { 0, 3, 0, 3 };
      static const uint8_t byteix[4] = { 0, 0, 1, 1 };
      OvmsMetricFloat* mw_all[4] = { mt_window_drv, mt_window_lr, mt_window_pass, mt_window_rr };
      int raw[4];
      for (int i = 0; i < 4; i++)
        raw[i] = (d[byteix[i]] >> shift[i]) & 0x07;
      if (raw[1] == 5 && raw[2] == 5 && raw[3] == 5)
        break;    // full idle pattern: even the driver 0 is filler, so take nothing from it

      OvmsMetricFloat** mw = mw_all;
      int maxpos = 0;
      int moved = 0;                          // +1 something opened, -1 something closed
      for (int i = 0; i < 4; i++)
        {
        int pos = raw[i];
        if (pos == VA_WINDOW_UNKNOWN)         // this window has no reading in this frame
          {
          if (mw[i]->IsDefined())             // keep what it was, and never call it movement
            {
            int known = (int)roundf(mw[i]->AsFloat() * VA_WINDOW_OPEN_MAX / 100.0f);
            if (known > maxpos) maxpos = known;
            }
          continue;
          }
        if (pos > VA_WINDOW_OPEN_MAX) pos = VA_WINDOW_OPEN_MAX;
        if (pos != m_window_raw[i])           // changed since the last frame: a real position
          {
          if (m_window_raw[i] >= 0)           // not the first frame, so it is real travel
            moved = (pos > m_window_raw[i]) ? 1 : -1;
          mw[i]->SetValue(pos * 100.0f / VA_WINDOW_OPEN_MAX, Percentage);
          m_window_raw[i] = pos;
          }
        // Aggregate from what each window is believed to be, not from this frame alone.
        if (mw[i]->IsDefined())
          {
          int known = (int)roundf(mw[i]->AsFloat() * VA_WINDOW_OPEN_MAX / 100.0f);
          if (known > maxpos) maxpos = known;
          }
        }

      // Direction comes from a window actually traveling, not from the aggregate changing.
      // Keying it off the aggregate breaks the moment one window sits somewhere different:
      // its position pins the maximum, the max stops changing while the others move, and no
      // transition is ever reported, which is what a driver window stuck down while the other
      // three close would produce.
      const char* state = (maxpos == 0) ? "closed" : "open";
      if (moved > 0)
        state = "opening";
      else if (moved < 0)
        state = "closing";
      m_window_last = maxpos;
      m_window_settle = VA_WINDOW_SETTLE_SECS;
      mt_windows->SetValue(state);
      break;
      }

    case 0x0176:  // HMI_Hybrid_Vehicle_Status_LS: the EV range the cluster shows.
      {
      if (dlc < 2)
        break;
      // HVDpltnMdRng 0|16 at 0.015625 km. Decoded off the 13-bit PID rather than the full
      // arbitration ID 0x102EC0CB, which carries the same bits but only matches this car's
      // source byte.
      // Whole km: the car computes ranges in integer km, the 0.015625 scale is only the
      // wire encoding (every observed raw step is a multiple of 64).
      StandardMetrics.ms_v_bat_range_est->SetValue(roundf(va_dbc_be(d, dlc, 0, 16) * 0.015625f),
                                                   Kilometers);
      UpdateRangeTotal();
      break;
      }

    case 0x0224:  // Fuel_Level_Status_LS: how far the gasoline will take it.
      {
      if (dlc < 4)
        break;
      // VehFuelRngCalc 8|17 at 0.015625 km. The DBC gives it a validity bit, VehFuelRngCalcV
      // at 9|1, and gating on that is wrong on this car: it never asserts, not through a bus
      // wake, a remote climate run, or a drive. Measured on-car, the field itself is the
      // honest signal. It sits at 0 until the car is switched on, then carries a real figure
      // about 5 s later. The engine does not have to run, the car only has to be on, because that
      // is what brings the ECM up to compute it. A bus wake and a remote climate run both leave
      // it at 0 for the same reason, neither powers the ECM. Measured against the cluster
      // over one drive, 515 km at a full tank, drifting to 519 mid-drive as the estimate
      // adapted, and 471 km at the end with 4.25 L burned. The bus read 471.0 at that point,
      // matching the dash. So treat 0 as "car not on yet, no figure" and publish anything
      // above it.
      uint32_t fuelrng = va_dbc_be(d, dlc, 8, 17);
      if (fuelrng == 0)
        break;
      mt_range_fuel->SetValue(roundf(fuelrng * 0.015625f), Kilometers);
      UpdateRangeTotal();
      break;
      }

    case 0x0168:  // Engine_Information_4_LS
      {
      if (dlc < 8)
        break;
      // READ ONLY, deliberately. A reset request exists (PID 0x01EC) and is not implemented
      // anywhere in this component, deliberately. Do not add it.
      // 39|8 is byte 4. FuelFltRmnLf (63|8) is not decoded: it reads a constant 100% on this
      // car, which has no serviceable fuel filter to report on.
      mt_oil_life->SetValue(va_dbc_be(d, dlc, 39, 8) * 0.392157f, Percentage);
      break;
      }

    default:
      break;
    }
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan4(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  m_candata_timer = VA_CANDATA_TIMEOUT;
  // Frames are arriving right now. Unlike ms_v_env_awake, which lingers true for
  // VA_CANDATA_TIMEOUT seconds after the bus goes quiet, this is only ever set by a real
  // received frame and expires in 3 s, so it can be trusted to gate a transmit.
  m_swcan_live = 3;

  ESP_LOGV(TAG,"SW CAN message received: %08" PRIx32 ": [%02x %02x %02x %02x %02x %02x %02x %02x]",
    p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  // Same as IncomingFrameCan1: awake unconditionally, poll-start timer only in state 0, and
  // never gated on the persistent awake metric. See the comment there for why.
  StandardMetrics.ms_v_env_awake->SetValue(true);
  if (m_poll_state == 0 && m_startPolling_timer == 0)
    {
    ESP_LOGI(TAG,"Car has woken (SWCAN bus activity)");
    m_startPolling_timer = VA_POLLING_START_DELAY;
    }

  IncomingDriveCycleSWCAN(p_frame);

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    // Door lock command
    case 0x0C414040: 
      {
        const char *lock_src;
        switch(d[3]){
          case 0x01:
            lock_src = "panel"; //with console panel button
            break;
          case 0x05:
            lock_src = "fob";
            break;
          case 0x06:
            lock_src = "keyless"; 
            break;
          case 0x07:
            lock_src = "OnStar"; 
            break;
          case 0x0A:
            lock_src = "auto"; //door is open when lock command activated, etc
            break;
          default:
            lock_src = "unknown";
            break;
        }

        // d[1]=0x05/0x04; for Volt MY14 d[1]=0x01/0x00
        if(d[1] & 1) {
            ESP_LOGI(TAG,"Car locked via %s(%u)", lock_src, d[3]);
            StandardMetrics.ms_v_env_locked->SetValue(1); 
            break;
        }
        else{
          ESP_LOGI(TAG,"Car unlocked via %s(%u)", lock_src, d[3]);
          StandardMetrics.ms_v_env_locked->SetValue(0); 
          break;
        }
      break;
      }

    /*
    // Read gear data from high speed CAN bus      
    case 0x102CA040: 
      {
      int ngear = d[6] & 7;
      if (ngear > 0 && ngear < 6) {
        StandardMetrics.ms_v_env_gear->SetValue(ngear - 1);
        }
      break;
      }
    */

    // Battery_Voltage (arb 0x124): 12V bus from the intelligent battery sensor.
    // BatVlt 23|8 (0.1,+3), BatSOC 31|8 (0.392157), BattCrntFltrd 47|8 signed (0.5).
    case 0x10248040:
      {
      if (p_frame->FIR.B.DLC < 6)
        break;
      // Kept off v.b.12v.voltage: that is written by housekeeping from the module's own ADC,
      // and a resting 12.8V here would falsely trip VA_CHARGING_12V_THRESHOLD (12.7).
      mt_v_12v_voltage->SetValue((float)GET8(p_frame, 2) * 0.1f + 3.0f);
      mt_v_12v_soc->SetValue((float)GET8(p_frame, 3) * 0.392157f);
      // Charge-positive, the usual convention for v.b.12v.current; note this is the
      // opposite sign handling to ms_v_bat_current, which is discharge-positive.
      StandardMetrics.ms_v_bat_12v_current->SetValue((float)(int8_t)GET8(p_frame, 5) * 0.5f, Amps);
      break;
      }

    // Thrml_Ref_Compressor_Status_LS (arb 0x138): A/C compressor speed + evaporator outlet air.
    // EvpCorOtltAirTmpCalcd 15|8 (0.5,-40), ThrmlRefCompSpd 21|14 (1 rpm).
    case 0x102700CB:
      {
      if (p_frame->FIR.B.DLC < 4)
        break;
      mt_ac_evap_temp->SetValue((float)GET8(p_frame, 1) * 0.5f - 40.0f);
      mt_ac_compressor_rpm->SetValue(((GET8(p_frame, 2) & 0x3F) << 8) | GET8(p_frame, 3));
      break;
      }

    // Climate_Control_Status_LS (arb 0x13A): HV power the BECM grants climate.
    // ClmtCtrlUpprPwrLmt 7|8 (0.1 kW). Event driven, goes stale between negotiations.
    case 0x102740CB:
      {
      if (p_frame->FIR.B.DLC < 1)
        break;
      // (ClmtCtrlUpprPwrLmt is not decoded: it reads 0 through a drive with the compressor at
      //  4500rpm, so the frame or offset is wrong.)
      break;
      }

    // Aux_Coolant_Heater_Status_LS (arb 0x13D): electric economy since last charge.
    // ChrgCyclElecEngyEcnEq 35|12 (0.1 km/l gasoline-equivalent).
    case 0x1027A0CB:
      {
      if (p_frame->FIR.B.DLC < 6)
        break;
      {
      // The car reports this as km per liter of gasoline-equivalent, which is the MPGe idea in
      // metric clothing and not what anyone reads an EV in. Convert to kWh/100km using the
      // EPA's equivalence of 33.7kWh per US gallon (8.9kWh/l), the same constant that defines
      // MPGe. Sanity-checked against a measured trip: 26.7km/l-e converts to 33kWh/100km,
      // against 39kWh/100km derived from the SOC drop over the same 6km (1% of SOC is worth
      // ~2kWh/100km on a trip that short, so the two agree inside the error).
      float kmpl = (float)(((GET8(p_frame, 4) & 0x0F) << 8) | GET8(p_frame, 5)) * 0.1f;
      if (kmpl > 0)
        mt_chargecycle_econ->SetValue(100.0f / kmpl * 8.9f);
      }
      break;
      }

    // Engine_Maintenance_Mode_Req_LS (arb 0x162): pack power as shown on the driver display.
    // PrpDspTtlPwr 45|13 (0.5 kW, offset -326.6); bit 0 of byte 6 belongs to the next signal,
    // hence the >>1. Negative = regen. Not on v.b.power, which UpdateBatteryPower() owns.
    case 0x102C40CB:
      {
      if (p_frame->FIR.B.DLC < 7)
        break;
      mt_hv_power_disp->SetValue((float)(((GET8(p_frame, 5) & 0x3F) << 7) | (GET8(p_frame, 6) >> 1)) * 0.5f - 326.6f);
      break;
      }

    // Drive_Cycle_Energy_Efficiency_LS (arb 0x223): gasoline burned this drive cycle, i.e.
    // whether the engine ran. DrvCyclFuelUsd 51|12 (0.125 l); 0xFFF = not available.
    case 0x104460CB:
      {
      if (p_frame->FIR.B.DLC < 8)
        break;
      uint16_t fuelused = ((GET8(p_frame, 6) & 0x0F) << 8) | GET8(p_frame, 7);
      if (fuelused != 0x0FFF)
        mt_fuel_used->SetValue((float)fuelused * 0.125f);
      break;
      }

    // Climate_Control_Voltage_LS (arb 0x312): cabin coolant heater power delivered + requested.
    // ClntHtrElecPwrRat 15|8 (0.04 kW), ClmCtrHiVltPwrRqtd 23|8 (0.1 kW).
    case 0x10624099:
      {
      if (p_frame->FIR.B.DLC < 3)
        break;
      mt_coolant_heater_pwr->SetValue((float)GET8(p_frame, 1) * 0.04f);
      // (ClmCtrHiVltPwrRqtd is not decoded: same, always 0 while climate runs.)
      break;
      }

    // Power_Elec_Info_LS (arb 0x31A): inverter/charger coolant loop temperature.
    // PwrElecCoolLpTemp 15|8 (1 degC, offset -40).
    //
    // Same physical sensor as poll PID 0x1C43, which feeds ms_v_inv_temp, so write
    // the standard metric rather than keeping a parallel xva copy. This path is passive and
    // costs no bus traffic, so it will usually be the fresher of the two.
    case 0x106340CB:
      {
      if (p_frame->FIR.B.DLC < 2)
        break;
      float t = (float)GET8(p_frame, 1) - 40.0f;
      StandardMetrics.ms_v_inv_temp->SetValue(t, Celcius);
      // The charger, inverter and DC-DC share one power-electronics coolant loop on this
      // car, so there is no separate charger sensor: the loop temperature is the best
      // available charger temperature.
      StandardMetrics.ms_v_charge_temp->SetValue(t, Celcius);
      break;
      }

    // Auxiliary_Heater_Status_LS (arb 0x36A): heater core inlet coolant temp, i.e. the cabin
    // heat loop, the clearest signal that preheat actually reached the cabin.
    // HtrCoreInltClntTmpCalc 23|8 (1 degC, offset -40); raw 0 = not available.
    case 0x106D4099:
      {
      if (p_frame->FIR.B.DLC < 3)
        break;
      if (GET8(p_frame, 2) != 0)
        mt_heatercore_temp->SetValue((float)GET8(p_frame, 2) - 40.0f);
      break;
      }

    // Hyb_Redundant_Batt_Data2_LS (GM doc 1818125, arb 0x2C7): HV pack voltage + current,
    // broadcast passively so no polling is needed. Scaling checked against a known
    // cabin-heater load: 390.6V / -11.55A = -4.51kW during preheat.
    case 0x1058E0CB:
      {
      if (p_frame->FIR.B.DLC < 4)
        break;
      // RdHVltBatPckVlt: start bit 19, 12 bits big-endian, 0.125 V/LSB
      uint16_t uvolt = ((GET8(p_frame, 2) & 0x0F) << 8) | GET8(p_frame, 3);
      // RdHVltBatPckCrnt: start bit 4, 13 bits big-endian signed, 0.15 A/LSB
      int16_t icurr = ((GET8(p_frame, 0) & 0x1F) << 8) | GET8(p_frame, 1);
      if (icurr > 4095)
        icurr -= 8192;
      StandardMetrics.ms_v_bat_voltage->SetValue((float)uvolt * 0.125f, Volts);
      // GM reports charge-positive; OVMS standard metric is output(discharge)-positive.
      StandardMetrics.ms_v_bat_current->SetValue(-(float)icurr * 0.15f, Amps);
      UpdateBatteryPower();
      break;
      }

    // High Volt Time Based Chrg (Current and available charging levels)
    case 0x1086C0CB:
      {
      int set_level = (GET8(p_frame, 3) >> 4) & 0x07; 

      int levels[4];
      levels[0] = (GET16(p_frame, 3) >> 7) & 0x1f; // normal (maximum) charging level
      levels[1] = (GET8(p_frame, 4) >> 2) & 0x1f;
      levels[2] = (GET16(p_frame, 4) >> 5) & 0x1f; 
      levels[3] = (GET8(p_frame, 5) >> 0) & 0x1f;
      int i=1;
      while ((i<4) && (levels[i]>0) )
        i++;

      mt_charging_limits->SetElemValues(0,i,levels);

      if (set_level>=i)
        {
        ESP_LOGE(TAG,"Invalid charging level index %d. Available levels: %s",set_level, mt_charging_limits->AsString().c_str());
        }
      else
        {
        ESP_LOGI(TAG,"Current charging limit: %d amps. Available levels: %s", levels[set_level], mt_charging_limits->AsString().c_str());
        StandardMetrics.ms_v_charge_climit->SetValue( (float)levels[set_level]);
        }
      break;
      }

    // Tire pressure
    case 0x103D4040: 
      {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL,  d[2] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL,  d[3] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR,  d[4] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR,  d[5] << 2 );

      // Tire temperature is not sent via CAN. For now set bogus tire temperature, 
      // because iOS app does not show tire pressure unless tempereature is set and >0 ..
      int temp;
      if (StandardMetrics.ms_v_env_temp->IsDefined())
        temp = StandardMetrics.ms_v_env_temp->AsInt();
      else
        temp = 255;
      if (temp<1)
        temp=1;
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, temp);
      break;
      } 

    // Content theft sensor
    case 0x10260040:
      {
      switch (d[2] & 0x7) // alarm status
        {
        case 0x2:
          {
          ESP_LOGI(TAG,"Alarm unarmed");
          MyEvents.SignalEvent("vehicle.alarm.unarmed",NULL);
          // If previously alarm sounded, notify it as off
          if (StandardMetrics.ms_v_env_alarm->AsBool())
            StandardMetrics.ms_v_env_alarm->SetValue(false);
          break;
          }
        case 0x1:
          {
          ESP_LOGI(TAG,"Alarm armed");
          MyEvents.SignalEvent("vehicle.alarm.armed",NULL);
          break;
          }
        case 0x4:
          {
          // This is the first status after doors are locked. After about 30 secs it is transitioned to 0x1 (armed)
          ESP_LOGI(TAG,"Alarm standby(?)");
          break;
          }
        case 0x3:
          {
          ESP_LOGW(TAG,"Alarm sounded!");
          StandardMetrics.ms_v_env_alarm->SetValue(true);  // Event is signaled here
          // TODO: Parse alarm trigger source (door, window break, charger removed, tilt sensor etc..)
          break;
          }
        default:
          {
          ESP_LOGW(TAG,"Alarm: unhandled status %d", d[2] & 0x7);
          break;
          }
        }
      break;
      } 
    
    // Door status FR
    case 0x0C2F6040: 
      {
      StdMetrics.ms_v_door_fr->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status RL
    case 0x0C2F8040: 
      {
      StdMetrics.ms_v_door_rl->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status RR
    case 0x0C2FA040: 
      {
      StdMetrics.ms_v_door_rr->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status FL
    case 0x0C630040: 
      {
      StdMetrics.ms_v_door_fl->SetValue(d[0] & 1<<0);
      break;
      } 

    // Hood
    case 0x10728040: 
      {
      StdMetrics.ms_v_door_hood->SetValue(d[0] & 1<<1);
      break;
      } 

    // Trunk
    case 0x0C6AA040: 
      {
      StdMetrics.ms_v_door_trunk->SetValue(d[0] & 1<<0);
      break;
      } 

    // External Lighting
    case 0x1020C040: 
      {
      StdMetrics.ms_v_env_headlights->SetValue(d[2] & 1<<2); // Parking Light
      break;
      } 

    // This Charge miles statistic
    case 0x1044A0CB: 
      {
      StdMetrics.ms_v_pos_trip->SetValue((float)((d[4]<<8 | d[5]) & 0x3fff)/8, Kilometers); // total km this charge
      mt_v_trip_ev->SetValue((float)((d[0]<<6 | d[1]>>2))/8, Kilometers); // ev km this charge
      break;
      } 

    // This Charge kWh statistic
    case 0x102820CB: 
      {
      StdMetrics.ms_v_bat_energy_used->SetValue((float)((d[2]<<8 | d[3]) & 0x3fff)/10, kWh);
      break;
      } 

 
    default:
      break;
    }

    // Handle the rest (AC / Preheating related CAN frames) here
    ClimateControlIncomingSWCAN(p_frame);
  }

// OVMS's own "since last charge" tally. Independent of the car's counters on purpose: it
// works whether or not the car's reset turns out to be reproducible, and OVMS can always
// clear its own. Integrated at 1 Hz from the same pack power the rest of the component uses.
void OvmsVehicleVoltAmpera::DriveCycleAccumulate()
  {
  float odo = StandardMetrics.ms_v_pos_odometer->AsFloat();

  // Discharge only. v.b.power is discharge-positive, so regen and charging simply do not
  // count against the tally, the same convention as the car's "energy used" figure.
  float kw = StandardMetrics.ms_v_bat_power->AsFloat();
  if (StandardMetrics.ms_v_env_on->AsBool() && kw > 0)
    mt_dc_energy_own->SetValue(mt_dc_energy_own->AsFloat() + kw / 3600.0f, kWh);

  // Odometer deltas rather than integrating speed: it is already accumulated by the car, so
  // this cannot drift. A negative or implausible jump means the reading was reset or garbage.
  if (odo > 0)
    {
    if (m_dc_last_odo > 0 && odo > m_dc_last_odo && (odo - m_dc_last_odo) < 10.0f)
      mt_dc_distance_own->SetValue(mt_dc_distance_own->AsFloat() + (odo - m_dc_last_odo),
                                   Kilometers);
    m_dc_last_odo = odo;
    }
  }

// Derive pack power from the most recent voltage/current readings. Both are polled at the
// same cadence and arrive separately, so this recomputes on whichever of the pair lands
// second, which is why it is a helper rather than an inline multiply at one arrival site.
void OvmsVehicleVoltAmpera::UpdateBatteryPower()
  {
  if (!StandardMetrics.ms_v_bat_voltage->IsDefined() ||
      !StandardMetrics.ms_v_bat_current->IsDefined())
    return;
  StandardMetrics.ms_v_bat_power->SetValue(
    StandardMetrics.ms_v_bat_voltage->AsFloat() *
    StandardMetrics.ms_v_bat_current->AsFloat() / 1000.0f, kW);
  }

// The drive unit has two motor-generators, each with its own sensor, but OVMS has a single
// v.m.temp. Publish the hotter of the pair, the one that matters for thermal headroom, and
// keep both individually on xva.v.m.temp.mga/.mgb.
void OvmsVehicleVoltAmpera::UpdateMotorTemp()
  {
  bool a = mt_mot_temp_mga->IsDefined(), b = mt_mot_temp_mgb->IsDefined();
  if (!a && !b)
    return;
  float t = !b ? mt_mot_temp_mga->AsFloat()
          : !a ? mt_mot_temp_mgb->AsFloat()
               : std::max(mt_mot_temp_mga->AsFloat(), mt_mot_temp_mgb->AsFloat());
  StandardMetrics.ms_v_mot_temp->SetValue(t, Celcius);
  }

void OvmsVehicleVoltAmpera::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  uint8_t value = *data;

  m_last_poll_reply = monotonictime;  // state-3 liveness (see ChargePollTicker)

  //Cell voltage
  const uint16_t pid_cellv1 = 0x4181;
  if((job.pid>=pid_cellv1 && job.pid<=pid_cellv1+30) || (job.pid>=pid_cellv1+127 && job.pid<=pid_cellv1+191)){
    if(length <2)
      return;

    int nCellNum = 0;
    if(job.pid <= pid_cellv1+30){
      nCellNum = job.pid - pid_cellv1;
    }
    else{
      nCellNum = job.pid - pid_cellv1 - 96;
    }

    if(nCellNum == 0)
      BmsResetCellVoltages();

    uint32_t uCellv =  (data[0]<<8 | data[1]);
    BmsSetCellVoltage(nCellNum, (float)5 * uCellv / 65535);
    if (nCellNum == 95)
      m_state2_swept = true;  // full sweep done; PollRunFinished may now end state 2
    return;
  }

  switch (job.pid)
    {
    case 0x002f:  // Fuel level
      if(mt_fuel_level->SetValue((int)value * 100 / 255))
        NotifyFuel();
      break;
    case 0x0076:  // Charge mode, read back with GMLAN 1A (the read side of the 3B 76 write
      // used to defer). data[0]: 1 = immediate, 2 = departure/rate-based i.e. deferred.
      // Reading this is what makes recovery possible: m_defer_active lives in RAM
      // and does not survive a reboot, but the setting lives in the HPCM and does.
      if (job.type != VEHICLE_POLL_TYPE_OBDII_1A || length < 1)
        break;
      mt_charge_deferred->SetValue(value == 2);
      break;
    case 0x4531:  // Charger type / supply level, same PID the Bolt EV module reads.
      // The Volt is AC only (J1772, no CCS inlet), so 1 and 2 differ by supply rather than
      // connector: L1 is a 120V household outlet, L2 a 240V wallbox. Deliberately does NOT
      // touch v.c.climit: the real selected limit comes from SWCAN 0x1086C0CB, and this
      // car stores it per location (8A away, 12A at home).
      switch (value)
        {
        case 1:
          StandardMetrics.ms_v_charge_type->SetValue("type1");
          mt_charge_level->SetValue("L1 (120V)");
          break;
        case 2:
          StandardMetrics.ms_v_charge_type->SetValue("type1");
          mt_charge_level->SetValue("L2 (240V)");
          break;
        case 3:
          StandardMetrics.ms_v_charge_type->SetValue("ccs");   // not fitted to a Volt
          mt_charge_level->SetValue("DC fast");
          break;
        case 0:
          StandardMetrics.ms_v_charge_type->SetValue("undefined");
          mt_charge_level->SetValue("unplugged");
          break;
        default:
          // Not a level this PID is known to report. Say nothing rather than guess, and leave
          // the plug state below untouched so an unexpected reply cannot unplug the car.
          ESP_LOGW(TAG, "Unknown charger supply level %d from PID 0x4531", value);
          return;
        }
      // This is also the only honest plug sensor available. Inferring "plugged in" from current
      // flow, as the charge-start block does, cannot see a car that is connected but deferred:
      // it draws nothing, so v.c.pilot would stay undefined and the limiter would bail out at
      // its not-plugged-in check, leaving a stranded defer unrecoverable.
      StandardMetrics.ms_v_charge_pilot->SetValue(value != 0);
      StandardMetrics.ms_v_door_chargeport->SetValue(value != 0);
      break;
    case 0x4369:  // On-board charger current
      StandardMetrics.ms_v_charge_current->SetValue((unsigned int)value / 5);
      break;
    case 0x4368:  // On-board charger voltage
      StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)value <<1);
      break;
    case 0x432d:  // High-voltage Battery pack voltage
      if (length < 2)
        break;
      StandardMetrics.ms_v_bat_voltage->SetValue((float)(data[0]<<8 | data[1]) * 0.52f, Volts);
      UpdateBatteryPower();
      break;
    case 0x40d4:  // High-voltage Battery pack current
      // The BECM reports charge-positive; OVMS standard metric is output(discharge)-positive.
      if (length < 2)
        break;
      StandardMetrics.ms_v_bat_current->SetValue(
        -(float)(int16_t)(data[0]<<8 | data[1]) / 20.0f, Amps);
      UpdateBatteryPower();
      break;
    case 0x1940:  // Transmission temperature (node 0x7e2, answers 0x7ea)
      // The drive unit carries a sensor per motor-generator and UpdateMotorTemp publishes the
      // hotter of the pair. This node is the only source on cars whose drive units do not
      // answer 0x28cb/0x368f, so it fills in behind them rather than competing.
      if (!mt_mot_temp_mga->IsDefined() && !mt_mot_temp_mgb->IsDefined())
        StandardMetrics.ms_v_mot_temp->SetValue((int)value - 0x28);
      break;

    case 0x28cb:  // Motor-generator A temperature (node 0x257, answers 0x657)
      mt_mot_temp_mga->SetValue((float)value - 40.0f, Celcius);
      UpdateMotorTemp();
      break;
    case 0x368f:  // Motor-generator B temperature (node 0x258, answers 0x658)
      mt_mot_temp_mgb->SetValue((float)value - 40.0f, Celcius);
      UpdateMotorTemp();
      break;
    case 0x41a6:  // EV range remaining. The car's own figure, not derived from SOC.
      if (length < 2)
        break;
      StandardMetrics.ms_v_bat_range_est->SetValue(roundf((float)(data[0]<<8 | data[1]) / 64.0f), Kilometers);
      // Same quantity and same scale as 0x0176 on the single wire bus, but this one arrives
      // whenever polling is running rather than only when the cluster broadcasts, so the total
      // has to be recomputed here too or it goes stale against a fresh EV figure.
      UpdateRangeTotal();
      break;
    case 0x439e:  // Battery heater duty
      mt_bat_heater_pct->SetValue((float)value / 2.55f, Percentage);
      break;
    case 0x41b6:  // Battery heater power
      if (length < 2)
        break;
      mt_bat_heater_pwr->SetValue((float)(data[0]<<8 | data[1]));
      break;
    case 0x801f:  // Outside temperature (filtered) (aka ambient temperature)
      StandardMetrics.ms_v_env_temp->SetValue((int)value/2 - 0x28);
      break;
    case 0x801e:  // Outside temperature (raw)
      break;
    case 0x434f:  // High-voltage Battery temperature
      StandardMetrics.ms_v_bat_temp->SetValue((int)value - 0x28);
      break;
    case 0x1c43:  // PEM / power-electronics coolant loop temperature
      StandardMetrics.ms_v_inv_temp->SetValue((int)value - 0x28);
      // Same loop feeds the charger; see IncomingFrameCan4 case 0x106340CB.
      StandardMetrics.ms_v_charge_temp->SetValue((float)value - 40.0f, Celcius);
      break;
    case 0x8334:  // State of charge, DISPLAYED: the dashboard number, 8 bit, ~0.4% steps.
      {
      // One decimal: the raw step is 100/255 = 0.39%, so anything finer is invented digits.
      mt_soc_displayed->SetValue(roundf((float)value * 1000 / 255) / 10, Percentage);
      UpdateSoc();
      break;
      }
    case 0x43af:  // State of charge, RAW high-definition: 16 bit, ~0.0015% steps.
      {
      // Displayed and raw are different scales, not different precisions of the same number,
      // and the mapping between them is NOT linear. Measured on a MY2017 over one charge,
      // (displayed, raw): (20.0, 32.4) (38.0, 46.9) (50.2, 56.8) (78.8, 79.4) (83.5, 83.0)
      // (95.3, 92.7) (99.6, 99.9). Raw runs well above the dash when low, converges around
      // 79, runs below it through the 80s and 90s, then crosses back at the very top.
      //
      // Nothing here converts one into the other; both are read from their own PID. The
      // numbers above are recorded so nobody is tempted to "simplify" this into a formula.
      //
      // Anything reasoning about the physical state of the battery therefore has to use raw:
      // a threshold expressed against the displayed value silently means a different amount
      // of charge at each end of the range. The Voltage app makes the same choice.
      if (length < 2)
        break;
      mt_soc_raw->SetValue(roundf((float)((data[0] << 8) | data[1]) * 10000 / 65535) / 100,
                           Percentage);
      UpdateSoc();
      break;
      }
    case 0x437d:  // Charge energy: AC input from the wall for the last/current session.
      {
      if (length < 2)
        break;
      uint16_t raw16 = (data[0] << 8) | data[1];
      if (raw16 == 0xFFFF)                 // the module's explicit "not available"
        break;
      // Wall-side, so it includes charger and cooling losses and reads HIGHER than the pack's
      // SOC delta. That is what makes it the right number for cost and efficiency, and the
      // wrong one for estimating what actually went into the battery.
      mt_charge_input->SetValue(raw16 * 10 / 1000.0f, kWh);
      break;
      }

    case 0x4389:  // Lifetime charge energy, 32 bit.
      {
      if (length < 4)
        break;
      // MY2019 reports kWh here and Voltage divides by 1000; earlier years report Wh. 2019 is
      // the last model year, the Volt was discontinued after it, so this is an equality test
      // rather than a range. Gated on model year rather than guessed from magnitude, which
      // would misread a car that genuinely has little lifetime charge.
      uint32_t raw32 = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
                     | ((uint32_t)data[2] << 8) | data[3];
      bool reports_kwh = (m_modelyear == 19);
      mt_charge_lifetime->SetValue(reports_kwh ? (float)raw32 : raw32 / 1000.0f, kWh);
      break;
      }

    case 0x000d:  // Vehicle speed
      StandardMetrics.ms_v_pos_speed->SetValue(value,Kilometers);
      break;
    /* 
    //ms_v_bat_range_est set from swcan message
    case 0x2487:  //Distance Traveled on Battery Energy This Drive Cycle
      {
      unsigned int edriven = (((int)data[4])<<8) + data[5];
      if ((edriven > m_range_estimated_km)&&
          (StandardMetrics.ms_v_charge_state->AsString().compare("done")))
        m_range_estimated_km = edriven;
      break;
      }
    */
    // The BECM's learned pack capacity, and the basis for state of health. Two PIDs carry it
    // with different scaling, and which one answers is what distinguishes the cars rather than
    // any model check: 0x41a3 answers on 2011-2018, 0x45ff on 2019+. Where both answer, 0x41a3
    // wins. The Voltage app resolves them the same way, by simply letting 0x41a3 overwrite.
    case 0x45ff:  // High-voltage Battery Capacity (2019+)
      if (length < 2 || m_cac_from_41a3)
        break;
      StandardMetrics.ms_v_bat_cac->SetValue((float)(data[0]<<8 | data[1]) / 100, AmpHours);
      UpdateBatteryCapacity();
      break;
    case 0x41a3:  // High-voltage Battery Capacity
      if (length < 2)
        break;
      m_cac_from_41a3 = true;
      StandardMetrics.ms_v_bat_cac->SetValue((float)(data[0]<<8 | data[1]) / 10, AmpHours);
      UpdateBatteryCapacity();  // derives health and usable energy from it
      break;
    case 0x40d7:  // High-voltage Battery Section 1 temperature  
      BmsResetCellTemperatures();
      BmsSetCellTemperature(0, data[0] - 40);
      break;
    case 0x40d9:  // High-voltage Battery Section 2 temperature  
      BmsSetCellTemperature(1, data[0] - 40);
      break;    
    case 0x40db:  // High-voltage Battery Section 3 temperature  
      BmsSetCellTemperature(2, data[0] - 40);
      break;
    case 0x40dd:  // High-voltage Battery Section 4 temperature  
      BmsSetCellTemperature(3, data[0] - 40);
      break;
    case 0x40df:  // High-voltage Battery Section 5 temperature  
      BmsSetCellTemperature(4, data[0] - 40);
      break;
    case 0x40e1:  // High-voltage Battery Section 6 temperature  
      BmsSetCellTemperature(5, data[0] - 40);
      break;   
    default:
      break;
    }
  }

  void OvmsVehicleVoltAmpera::Ticker300(uint32_t ticker)
    {
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
        NotifyMetrics();
      }
    }

void OvmsVehicleVoltAmpera::Ticker10(uint32_t ticker)
  {
  if (m_tx_retry_counter>0)
    {
    ESP_LOGI(TAG, "Resetting tx_retry_counter");
    m_tx_retry_counter=0;
    }

  ChargeLimitTicker();
  }

//=====================================================================================
// SOC charge limit
//
// The Volt exposes no single-shot "stop charging" command. The documented approach, used in
// the field by the Voltage Android app, is to flip the HPCM (node 0x7E4) into
// departure-based charging with a target far enough ahead that it defers starting, then
// re-apply that whenever the car resumes on its own. It is a continuous loop, not a
// one-shot: a single defer does not hold.
//
// Addressing verified on a MY2017 Gen2 Volt: 0x7E4 answers on SWCAN (can4, 11-bit) and
// holds a diagnostic session (01 3E -> 01 7E). It rejects UDS service 0x22 there with
// NRC 0x11 (serviceNotSupported), which is expected: the low-speed bus uses the legacy
// GMLAN service set (3B WriteDataByIdentifier, AE DeviceControl).
//
// !! These 3B write frames are not from a live capture. Everything here is gated behind
// !! xva/chargelimit.enabled, which defaults to false.
//
// Departure slot: the app spoofs the vehicle RTC to noon, writes a 23:45 departure, then
// restores the real clock. The spoof only runs when a real clock is available to restore
// afterwards (see ChargeDeferBegin). Without it the same fixed 23:45 slot still goes out,
// against whatever time the car already holds, so the deferral gap is whatever that leaves.
//=====================================================================================

#define VA_OVERRIDE_RESET_KM 0.2f  // drive this far and the one-off override expires
#define VA_CHGSEQ_WAKE_SECS 2   // let CommandWakeup settle before transmitting
#define VA_CHGSEQ_GAP_1   1   // Ticker1 counts between the two frames of a sequence
#define VA_CHGSEQ_GAP_2   1

// Send a single-frame ISO-TP GMLAN request: [PCI][payload...] zero-padded to 8 bytes.
bool OvmsVehicleVoltAmpera::GmlanWrite(canbus* bus, uint32_t txid, const uint8_t* payload, uint8_t len)
  {
  if (bus == NULL || len == 0 || len > 7)
    return false;
  CAN_frame_t txframe;
  memset(&txframe, 0, sizeof(txframe));
  txframe.origin = bus;
  txframe.FIR.U = 0;
  txframe.FIR.B.DLC = 8;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.MsgID = txid;
  txframe.data.u8[0] = len;
  memcpy(&txframe.data.u8[1], payload, len);
  if (m_chargelimit_debug)
    {
    ESP_LOGI(TAG, "GmlanWrite %03x: %02x %02x %02x %02x %02x %02x %02x %02x", (unsigned int)txid,
      txframe.data.u8[0], txframe.data.u8[1], txframe.data.u8[2], txframe.data.u8[3],
      txframe.data.u8[4], txframe.data.u8[5], txframe.data.u8[6], txframe.data.u8[7]);
    }
  return (bus->Write(&txframe) != ESP_FAIL);
  }

// Raw "is the car inside the configured location". No config = treated as inside, since an
// unset geofence means "limit everywhere".
bool OvmsVehicleVoltAmpera::ChargeLimitInLocation()
  {
  if (m_chargelimit_location.empty())
    return true;
  if (!MyLocations.m_gpsgood)
    return false;
  auto it = MyLocations.m_locations.find(m_chargelimit_location);
  if (it == MyLocations.m_locations.end())
    return false;
  return it->second->m_inlocation;
  }

// Geofence gate: limit to 80% at the configured location, charge to full everywhere else.
// Without a trustworthy GPS fix the limit is NOT enforced: home is indistinguishable from a
// public charger, and wrongly capping somewhere the owner needs range is the worse failure.
bool OvmsVehicleVoltAmpera::ChargeLimitLocationOk()
  {
  if (m_chargelimit_location.empty())
    return true;
  if (!MyLocations.m_gpsgood)
    {
    ESP_LOGD(TAG, "Charge limit: no reliable GPS, not enforcing location '%s'",
      m_chargelimit_location.c_str());
    return false;
    }
  auto it = MyLocations.m_locations.find(m_chargelimit_location);
  if (it == MyLocations.m_locations.end())
    {
    ESP_LOGW(TAG, "Charge limit: location '%s' is not defined, not enforcing",
      m_chargelimit_location.c_str());
    return false;
    }
  return it->second->m_inlocation;
  }

bool OvmsVehicleVoltAmpera::ChargeDeferBegin()
  {
  // Gate here rather than only in the callers: this and ChargeStartBegin are the only two
  // functions that put charge-control frames on the bus, so checking at the choke point means
  // no future caller can bypass the master switch.
  if (!m_chargelimit_enabled)
    {
    ESP_LOGE(TAG, "Charge control is disabled (set xva/chargelimit.enabled)");
    return false;
    }
  if (m_can1 == NULL)
    return false;
  if (m_chargeseq != VA_CHGSEQ_IDLE)
    return false;                       // a sequence is already running
  // Wake the car so the HPCM is listening. Note the mode/time writes go out on can1;
  // CommandWakeup() also flips the SWCAN transceiver into high-voltage mode and only
  // restores it asynchronously, so nothing may transmit until that has settled: frames sent
  // before it does are queued and fail (txfail/TX_Queue).
  CommandWakeup();
  // Spoof the car clock to noon for the macro (see ChargeSeqTicker): tested live, a bare
  // mode+time write is ACKed (7B) but the session keeps charging and the dash stays on
  // Immediate. Only spoof when a real clock is available to restore afterwards.
  m_chargeseq_spoof = (p_swcan != NULL) && (time(NULL) > 1000000000);
  ESP_LOGI(TAG, "Charge defer: starting %s macro",
    m_chargeseq_spoof ? "clock-spoof" : "bare (no SWCAN/clock)");
  m_chargeseq = VA_CHGSEQ_WAKE_DEFER;
  m_chargeseq_timer = VA_CHGSEQ_WAKE_SECS;
  return true;
  }

bool OvmsVehicleVoltAmpera::ChargeStartBegin()
  {
  if (!m_chargelimit_enabled)           // see ChargeDeferBegin
    {
    ESP_LOGE(TAG, "Charge control is disabled (set xva/chargelimit.enabled)");
    return false;
    }
  if (m_can1 == NULL)
    return false;
  if (m_chargeseq != VA_CHGSEQ_IDLE)
    return false;
  CommandWakeup();
  m_chargeseq = VA_CHGSEQ_WAKE_START;
  m_chargeseq_timer = VA_CHGSEQ_WAKE_SECS;
  return true;
  }

// Steps the pending sequence. Called once per second from Ticker1 so the inter-frame
// delays cost nothing: no vTaskDelay on a ticker task.
void OvmsVehicleVoltAmpera::ChargeSeqTicker()
  {
  if (m_chargeseq == VA_CHGSEQ_IDLE)
    return;
  if (m_chargeseq_timer > 0)
    {
    m_chargeseq_timer--;
    return;
    }

  switch (m_chargeseq)
    {
    case VA_CHGSEQ_WAKE_DEFER:
      {
      // Wakeup has settled; walk the defer macro, one frame per tick from the next tick on
      m_chargeseq = m_chargeseq_spoof ? VA_CHGSEQ_DEFER_CLKH : VA_CHGSEQ_DEFER_F1;
      break;
      }

    case VA_CHGSEQ_DEFER_CLKH:
      {
      const uint8_t f[] = { 0x3b, 0x30, 12 };     // car clock hour := 12 (fake noon)
      GmlanWrite(p_swcan, 0x244, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_CLKM;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_CLKM:
      {
      const uint8_t f[] = { 0x3b, 0x31, 0 };      // car clock minute := 00
      GmlanWrite(p_swcan, 0x244, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_F1;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_F1:
      {
      const uint8_t f[] = { 0x3b, 0x77, 0x02, 0x00, 0x00 };  // clear departure time
      GmlanWrite(m_can1, 0x7e4, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_F2;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_F2:
      {
      const uint8_t f[] = { 0x3b, 0x76, 0x01, 0x01 };  // mode -> immediate (forces replan)
      GmlanWrite(m_can1, 0x7e4, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_H1;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_H1:
      {
      const uint8_t f[] = { 0x3b, 0x76, 0x02, 0x01 };  // mode -> departure/rate-based
      GmlanWrite(m_can1, 0x7e4, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_H2;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_H2:
      {
      const uint8_t f[] = { 0x3b, 0x77, 0x02, 0x5f, 0x00 };  // departure 23:45 (slot 95 of the 15-min 0..95 index)
      GmlanWrite(m_can1, 0x7e4, f, sizeof(f));
      m_chargeseq = VA_CHGSEQ_DEFER_RESTH;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_DEFER_RESTH:
      {
      if (m_chargeseq_spoof)
        {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        const uint8_t f[] = { 0x3b, 0x30, (uint8_t)t.tm_hour };  // restore real hour
        GmlanWrite(p_swcan, 0x244, f, sizeof(f));
        m_chargeseq_timer = VA_CHGSEQ_GAP_1;
        }
      m_chargeseq = VA_CHGSEQ_DEFER_RESTM;
      break;
      }

    case VA_CHGSEQ_DEFER_RESTM:
      {
      if (m_chargeseq_spoof)
        {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        const uint8_t f[] = { 0x3b, 0x31, (uint8_t)t.tm_min };   // restore real minute
        GmlanWrite(p_swcan, 0x244, f, sizeof(f));
        }
      m_chargeseq = VA_CHGSEQ_IDLE;
      m_defer_active = true;
      m_defer_count++;
      // The car keeps departure mode until something writes immediate back. Remember that
      // we owe it that write, in config rather than RAM: losing this to a reboot is exactly
      // what strands a car in departure mode with nothing left that knows why.
      if (!m_defer_unclear)
        {
        m_defer_unclear = true;
        MyConfig.SetParamValueBool("xva", "chargelimit.unclear", true);
        }
      ESP_LOGI(TAG, "Charge defer applied (attempt %d of %d)", m_defer_count, m_chargelimit_maxdefer);
      // Notify only on the FIRST defer of a plug-in session. The car resumes charging on its
      // own and the defer is re-applied each time, so notifying per attempt spams the owner;
      // OVMS framework already sends its own charge.stopped for each of those transitions.
      // One message explaining why it stopped is useful; twenty are not.
      if (m_chargelimit_notify && m_defer_count == 1)
        {
        MyNotify.NotifyStringf("info", "charge.limit",
          "Charging paused at %d%% (limit %d%%)",
          (int)ChargeLimitSoc(), m_chargelimit_soc);
        }
      break;
      }

    case VA_CHGSEQ_WAKE_START:
      {
      // clear the departure time
      const uint8_t f1[] = { 0x3b, 0x77, 0x02, 0x00, 0x00 };
      GmlanWrite(m_can1, 0x7e4, f1, sizeof(f1));
      m_chargeseq = VA_CHGSEQ_START_1;
      m_chargeseq_timer = VA_CHGSEQ_GAP_1;
      break;
      }

    case VA_CHGSEQ_START_1:
      {
      // back to immediate charging
      const uint8_t f2[] = { 0x3b, 0x76, 0x01, 0x01 };
      GmlanWrite(m_can1, 0x7e4, f2, sizeof(f2));
      m_chargeseq = VA_CHGSEQ_IDLE;
      m_defer_active = false;
  m_defer_manual = false;
      ESP_LOGI(TAG, "Charge resume applied");
      break;
      }

    default:
      m_chargeseq = VA_CHGSEQ_IDLE;
      break;
    }
  }

// Enforcement loop. Runs every 10s; re-applies the defer whenever the HPCM resumes
// charging above the set point (the "whack-a-mole" the app relies on).
void OvmsVehicleVoltAmpera::ChargeLimitTicker()
  {
  if (!m_chargelimit_enabled || m_chargelimit_soc <= 0 || m_chargelimit_soc >= 100)
    return;
  if (m_chargeseq != VA_CHGSEQ_IDLE)
    return;

  bool pluggedin = StandardMetrics.ms_v_charge_pilot->AsBool();
  bool charging  = StandardMetrics.ms_v_charge_inprogress->AsBool();

  // Unplugged: forget the session so the next plug-in gets a fresh attempt budget.
  if (!pluggedin)
    {
    if (m_defer_count != 0 || m_defer_active)
      {
      ESP_LOGD(TAG, "Charge limit: unplugged, resetting session state");
      m_defer_count = 0;
      m_defer_active = false;
      m_defer_manual = false;
      }

    // Put the car back to immediate now the session is over, rather than trying to spot a
    // stale defer at the next plug-in. Departure mode is persistent in the HPCM, so a defer
    // applied at the limit outlives the unplug and the car refuses to charge next time.
    //
    // Doing it on the way out is what makes it reliable, and the reason is when people plug
    // in. At a public charger the cable goes in seconds after the driver gets out, so the car
    // is still awake, the polls are running and a stale defer would be noticed. At home the
    // car gets unloaded first and is asleep before the cable goes in, which is exactly when
    // nothing is polling, v.c.pilot never turns true and nothing notices. So the case that
    // needs rescuing is the one where rescue cannot see anything.
    //
    // Only a defer of ours is cleared: m_defer_unclear is raised where the defer is issued
    // and nowhere else, so departure charging the owner set up themselves is never touched.
    //
    // The flag lives in config rather than RAM, because losing it to a reboot is precisely
    // what strands a car in departure mode with nothing left that knows why. Retried on every
    // tick until a write actually goes out, so an unplug while the car is asleep is fine.
    if (m_defer_unclear && StandardMetrics.ms_v_env_awake->AsBool())
      {
      if (ChargeStartBegin())
        {
        ESP_LOGI(TAG, "Charge limit: unplugged after a defer, restoring immediate charging");
        m_defer_unclear = false;
        MyConfig.SetParamValueBool("xva", "chargelimit.unclear", false);
        }
      }
    return;
    }

  // Expire a one-off override once the car has actually been driven away. Distance is used
  // rather than the geofence edge because a 100m radius plus GPS jitter can flip in/out
  // while parked, which would silently re-arm the limit mid-charge. Falls back to the
  // geofence edge only if the odometer is unavailable.
  bool inloc = ChargeLimitInLocation();
  if (m_limit_override)
    {
    float odo = StandardMetrics.ms_v_pos_odometer->AsFloat();
    bool driven_away = (m_override_odo > 0 && odo > 0 &&
                        (odo - m_override_odo) >= VA_OVERRIDE_RESET_KM);
    bool left_geofence = (m_override_odo <= 0 && m_was_in_location && !inloc);
    if (driven_away || left_geofence)
      {
      SetLimitOverride(false);
      ESP_LOGI(TAG, "Charge limit: %s, one-off full-charge override cleared",
        driven_away ? "driven away" : "left the location");
      if (m_chargelimit_notify)
        MyNotify.NotifyString("info", "charge.limit",
          "Charge limit re-armed for next time");
      }
    }
  m_was_in_location = inloc;

  if (m_limit_override)
    return;                             // charging to full this once

  // Recover a defer OVMS has lost track of. Deliberately ABOVE the location gate: that gate
  // exists so the charge is never capped at a public charger, which is the right caution for
  // DEFERRING, but a defer left behind is stale wherever the car happens to be, and away
  // from home is precisely when the owner most needs it to charge. It also has to run before
  // the gate because an unreliable GPS fix (few satellites, low quality) fails it outright.
  //
  // The defer lives in the HPCM and is persistent; the OVMS record of it (m_defer_active) is in
  // RAM and is not, so a reboot in between strands the car in departure mode with nothing
  // left that knows to undo it. The Voltage app can leave the same state behind.
  //
  // Only act when the defer is provably stale: the car reports departure mode, OVMS holds no
  // defer of its own, it is not charging, and SOC is below the target so no defer would be
  // issued anyway. An unconditional resume would fight a departure time the
  // owner set themselves.
  // Same 2% margin as the resume below. Without it a defer sitting exactly on target looks
  // stale the moment a reading lands a hair under it, including the brief window after a
  // reboot where ChargeLimitSoc() is still falling back to the other scale.
  if (mt_charge_deferred->AsBool() && !m_defer_active && !charging
      && (int)ChargeLimitSoc() < (m_chargelimit_soc - 2))
    {
    int soc_now = (int)ChargeLimitSoc();
    ESP_LOGW(TAG, "Car is in departure mode but we hold no defer, and SOC %d%% is below "
                  "%d%%: resuming (defer was probably left behind by a reboot)",
                  soc_now, m_chargelimit_soc - 2);
    if (m_chargelimit_notify)
      MyNotify.NotifyStringf("info", "charge.limit",
        "Charging was left deferred at %d%%, resuming", soc_now);
    ChargeStartBegin();
    return;
    }

  if (!ChargeLimitLocationOk())
    return;

  int soc = (int)ChargeLimitSoc();

  // While the car is powered up, "charging" does not mean what it usually means, so the flags
  // cannot drive the loop.
  //
  // Plugged in, switched on and deferred, the car draws from the EVSE to run its own loads and
  // keep the pack topped up WITHOUT raising SOC (measured flat at 80.2% throughout; the draw
  // itself varies with what the climate system is doing). The defer is honored, but
  // v.c.charging and v.c.power read as an active charge regardless. Treating the flags as
  // evidence of a resume would re-defer every 10s against a defer that is already working,
  // burning 12 of 20 attempts in about two minutes and ending in a false
  // "charge limit failed" alert.
  //
  // So: still apply a cap here, or plugging in with the car running would charge past the
  // target unopposed. Just apply at most ONE and never retry on the flags. Resuming waits
  // until the car is off, where the flags mean what they say.
  //
  // ms_v_env_on covers remote preheat as well as ignition.
  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    if (charging && soc >= m_chargelimit_soc && !m_defer_active)
      {
      ESP_LOGI(TAG, "Charge limit: SOC %d%% >= %d%% with the car on, deferring once "
                    "(no retries while powered, the charge flags are unreliable here)",
               soc, m_chargelimit_soc);
      ChargeDeferBegin();
      }
    return;
    }

  if (charging && soc >= m_chargelimit_soc)
    {
    if (m_defer_count >= m_chargelimit_maxdefer)
      {
      // The car keeps overriding the defer. Stop fighting the bus and say so once.
      if (m_defer_count == m_chargelimit_maxdefer)
        {
        ESP_LOGE(TAG, "Charge limit: %d defer attempts exhausted, giving up this session",
          m_chargelimit_maxdefer);
        if (m_chargelimit_notify)
          {
          MyNotify.NotifyStringf("alert", "charge.limit",
            "Charge limit failed: car resumed charging %d times, giving up",
            m_chargelimit_maxdefer);
          }
        m_defer_count++;      // only notify once
        // The pause is no longer the limiter's: the car charges to full unopposed, and the
        // eventual completion must take the normal done path (notification, charge.finish
        // event, pilot clear), not the defer-pause path in Ticker1.
        m_defer_active = false;
        m_defer_manual = false;
        }
      return;
      }
    ESP_LOGI(TAG, "Charge limit: SOC %d%% >= %d%%, deferring", soc, m_chargelimit_soc);
    ChargeDeferBegin();
    }
  // Resume with 2% hysteresis to avoid oscillating at the boundary. Two guards against
  // acting on a stale pilot latch (the cable may have been pulled without a drive): never
  // resume while the car is on (an unplugged preheat drains SOC below the threshold), and
  // only when the charge system answered a poll recently, i.e. an EVSE is really present.
  else if (!charging && m_defer_active && !m_defer_manual && soc < (m_chargelimit_soc - 2)
    && !StandardMetrics.ms_v_env_on->AsBool()
    && (monotonictime - m_last_poll_reply) <= 60)
    {
    ESP_LOGI(TAG, "Charge limit: SOC %d%% fell below %d%%, resuming", soc, m_chargelimit_soc);
    ChargeStartBegin();
    }
  }

//=====================================================================================
// Physical actuator controls (engine / trunk / windows / horn / lights)
//
// All frames below were recovered by decompiling the Voltage Android app
// (io.tripovan.voltage v2.2.3) rather than captured from one. Locate has only ever been
// exercised as its two halves, never as the combined command.
//
// Everything is gated behind config xva/control.enabled (default false).
//=====================================================================================

// $AE DeviceControl, preceded by 3E TesterPresent to the same node, on the given bus.
//
// Node/bus map, established by TesterPresent survey on a MY2017 Gen2 Volt (car awake,
// powertrain off). "answered" = the node returned a positive 01 7E response:
//
//   0x7E4  HPCM/BECM     SWCAN (can4)   answered   (charge control)
//   0x244  clock         SWCAN (can4)   answered   (RTC, for the clock spoof)
//   0x241  BCM/windows   can1           answered   (silent on SWCAN, alive on HS-GMLAN)
//   0x7E1  engine        neither        SILENT     (see CommandEngine())
bool OvmsVehicleVoltAmpera::GmlanDeviceControl(canbus* bus, uint32_t txid, const uint8_t* payload, uint8_t len)
  {
  if (bus == NULL || len == 0 || len > 7)
    return false;
  CAN_frame_t txframe;
  SEND_STD_FRAME(bus, txframe, txid, 8, 0x01, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  vTaskDelay(50 / portTICK_PERIOD_MS);
  return GmlanWrite(bus, txid, payload, len);
  }

#define VA_ENGINE_KEEPALIVE_SECS  2     // must beat GM's ~5s diagnostic session timeout

// Internal combustion engine override, three states: AUTO / FORCE_ON / FORCE_OFF.
//
// GMLAN $AE DeviceControl is a diagnostic OVERRIDE, not a latched command. It holds only
// while the diagnostic session is alive, and GM's session timeout is ~5s, which is why a
// single 3E + AE 31 06 starts the engine and then loses it a few seconds later. Both forced
// states therefore run a keep-alive (see EngineTicker), and AUTO explicitly releases with
// AE 00 00 so the HPCM regains control immediately instead of waiting out the timeout.
//
// Bus/precondition, established by TesterPresent survey: 0x7E1 lives on can1 and answers
// only once the car is POWERED (switched on, or preheat active). It is NOT a SWCAN node, so the
// SWCAN high-voltage wakeup does nothing for it; the gate is ms_v_env_on instead.
//
// SAFEGUARD, ported from the app: FORCE_OFF is refused at or below 16% SOC. FORCE_ON and
// AUTO are ungated.
//
// Caveat: near 100% SOC the HPCM may accept FORCE_ON and still decline to keep the
// generator running. The keep-alive cannot override that; validate at a lower SOC.
//
// A forced state is held for as long as the owner leaves it set. There is no time cap: the
// only thing that ends an override by itself is the car powering down, which takes 0x7E1
// with it (see EngineTicker).

// (Re)assert the AE frame for whichever forced mode is active. Idempotent: re-sending also
// covers modules that need re-commanding rather than just session-keeping.
void OvmsVehicleVoltAmpera::EngineAssert()
  {
  if (m_engine_mode == VA_ENG_FORCE_ON)
    {
    const uint8_t p[] = { 0xae, 0x31, 0x06, 0x00, 0x00, 0x00, 0x00 };
    GmlanDeviceControl(m_can1, 0x7e1, p, sizeof(p));
    }
  else if (m_engine_mode == VA_ENG_FORCE_OFF)
    {
    const uint8_t p[] = { 0xae, 0x31, 0x05, 0x00, 0x00, 0x00, 0x00 };
    GmlanDeviceControl(m_can1, 0x7e1, p, sizeof(p));
    }
  }

// Keep-alive. Called once per second from Ticker1, inert in AUTO.
void OvmsVehicleVoltAmpera::EngineTicker()
  {
  if (m_engine_mode == VA_ENG_AUTO)
    return;

  // Losing vehicle power means 0x7E1 is gone; drop the override rather than shout at a
  // module that is not listening. This is the only thing that releases an override on its
  // own: a forced state otherwise holds until it is released explicitly.
  if (!StandardMetrics.ms_v_env_on->AsBool())
    {
    ESP_LOGI(TAG, "Engine override: car no longer powered, reverting to AUTO");
    CommandEngine(VA_ENG_AUTO);
    return;
    }

  if (--m_engine_ka_timer <= 0)
    {
    m_engine_ka_timer = VA_ENGINE_KEEPALIVE_SECS;
    EngineAssert();
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandEngine(va_engine_mode_t mode, const char* pin)
  {
  // Forcing the engine either way changes how the car behaves under someone who may be
  // driving it, so both need the PIN. Releasing does not: a forced state that cannot be
  // released is worse than one that could be set, and EngineTicker reverts through here
  // when the car powers down. The control.enabled gate below is skipped for AUTO for the
  // same reason.
  if (mode != VA_ENG_AUTO && !PinCheck(pin))
    {
    ESP_LOGE(TAG, "Engine control: PIN check failed");
    return Fail;
    }

  // Note the gate deliberately does NOT cover the AUTO/release path below. Releasing an
  // override must always be possible: EngineTicker's power-loss check reverts through here,
  // and if the owner disables controls while an override is held, refusing the release would
  // strand the car in the forced state with the keep-alive still asserting every 2s.
  if (!m_control_enabled && mode != VA_ENG_AUTO)
    {
    ESP_LOGE(TAG, "Vehicle controls are disabled (set xva/control.enabled)");
    return Fail;
    }

  if (mode == VA_ENG_AUTO)
    {
    bool was_forced = (m_engine_mode != VA_ENG_AUTO);
    m_engine_mode = VA_ENG_AUTO;
    mt_engine_mode->SetValue("auto");
    m_engine_ka_timer = 0;
    // Explicit release so the HPCM takes back control now, not in ~5s.
    const uint8_t p[] = { 0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    GmlanDeviceControl(m_can1, 0x7e1, p, sizeof(p));
    if (was_forced)
      ESP_LOGI(TAG, "Engine override released, HPCM back in control");
    return Success;
    }

  // 0x7E1 is only powered when the car is. Preheat counts, which is the app's flow.
  if (!StandardMetrics.ms_v_env_on->AsBool())
    {
    ESP_LOGE(TAG, "Engine control needs the car switched on, or climate started first");
    return Fail;
    }

  if (mode == VA_ENG_FORCE_OFF)
    {
    // Raw, never v.b.soc: this is a physical limit on the pack and the app tests it against
    // the raw scale (>16.0). Read off the dashboard it would fire at a different charge
    // level entirely: at the low end raw runs more than 12% above the dash, so a
    // dashboard-based test would refuse the stop with a large reserve still in the pack.
    float soc = SocRaw();
    if (soc <= 16.0f)
      {
      ESP_LOGE(TAG, "Refusing engine OFF at %.1f%% raw SOC (needs >16%%)", soc);
      MyNotify.NotifyStringf("alert", "engine.control",
        "Engine stop refused: raw SOC %.1f%% is at or below the 16%% safety limit", soc);
      return Fail;
      }
    }

  m_engine_mode = mode;
  mt_engine_mode->SetValue((mode == VA_ENG_FORCE_ON) ? "forced-on" : "forced-off");
  m_engine_ka_timer = VA_ENGINE_KEEPALIVE_SECS;
  ESP_LOGI(TAG, "Engine override: %s (keep-alive every %ds)",
    (mode == VA_ENG_FORCE_ON) ? "FORCE ON" : "FORCE OFF", VA_ENGINE_KEEPALIVE_SECS);
  EngineAssert();
  return Success;
  }

// Mark a command as in flight. Watch the metric it is supposed to change so the indicator
// clears on real feedback rather than on a fixed delay, and fall back to the timeout when the
// car does not answer at all.
void OvmsVehicleVoltAmpera::SetCmdPending(const char* what, OvmsMetric* watch)
  {
  mt_cmd_pending->SetValue(what);
  m_pending_watch = watch;
  m_pending_seen = (watch != NULL) ? watch->AsString() : std::string();
  m_pending_secs = VA_CMD_PENDING_SECS;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandTrunk(const char* pin)
  {
  // Opens the car.
  if (!PinCheck(pin))
    {
    ESP_LOGE(TAG, "Trunk release: PIN check failed");
    return Fail;
    }

  if (!m_control_enabled)
    {
    ESP_LOGE(TAG, "Vehicle controls are disabled (set xva/control.enabled)");
    return Fail;
    }
  if (!m_use_swcan_adapter || p_swcan == NULL)
    return NotImplemented;

  CommandWakeup();
  CAN_frame_t txframe;
  vTaskDelay(720 / portTICK_PERIOD_MS);
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x02, 0x00, 0xff)
  vTaskDelay(180 / portTICK_PERIOD_MS);
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x0c, 0x00, 0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00, 0x00, 0xff)
  ESP_LOGI(TAG, "Trunk release sent");
  return Success;
  }

// Windows. CPID 0x3B, byte 2 = 0xFF, then four bytes: 0x02 = up/close, 0x01 = down/open.
//
// Validated on-car (MY2017 Gen2): node 0x241 on can1, car merely AWAKE (no climate/remote
// start needed, unlike the engine). "07 AE 3B FF 01 01 01 01" returns 02 EE 3B and all four
// windows travel FULLY open; the close frame shuts them FULLY. One frame is enough, with no
// keep-alive, unlike the engine override.
OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandWindows(bool up, const char* pin)
  {
  // Opens the car, and closing glass on someone is its own hazard.
  if (!PinCheck(pin))
    {
    ESP_LOGE(TAG, "Window control: PIN check failed");
    return Fail;
    }

  if (!m_control_enabled)
    {
    ESP_LOGE(TAG, "Vehicle controls are disabled (set xva/control.enabled)");
    return Fail;
    }
  if (m_can1 == NULL)
    return NotImplemented;

  // Node 0x241 only answers on a live bus, so a sleeping car has to be woken first. Refusing
  // instead just moves that job to the owner, who then has to press Wake, wait, and press
  // again, and from a dashboard button the refusal is invisible. Wake here and carry on.
  // CommandWakeup() also flips the SWCAN transceiver into high-voltage mode and restores it
  // asynchronously, so allow that to settle before transmitting or the frame is queued and
  // dropped (the same txfail the charge sequence is exposed to).
  const uint8_t dir = up ? 0x02 : 0x01;
  m_window_cmd_dir = dir;
  // Set before the wakeup, not after: the wakeup is about four seconds of nothing visible
  // happening, which is exactly the gap the busy indicator exists to cover.
  SetCmdPending(up ? "windows up" : "windows down", mt_windows);

  // If SWCAN frames are not arriving, the bus is asleep and the BCM will not report the move
  // even though the windows obey, because the command goes out on the HS bus while position
  // comes back on SW-CAN. Wake first, then command, so the whole travel is observed and the
  // opening/closing transition is actually seen rather than missed entirely.
  //
  // Always run the FULL wakeup, even when the bus is already carrying traffic. A bare 0x100
  // high voltage frame wakes the bus electrically but does not bring the BCM to a state where
  // it acts on window commands: five correctly formed command frames produce no movement and
  // no status at all. Commands only land when preceded by CommandWakeup, whose tail
  // (FlashLights(Interior_lamp), the ae 08 device control) appears on the bus right before
  // every successful command. That sequence wakes each module individually and a single frame
  // does not substitute for it.
  //
  // Gating the wakeup on recent SW-CAN traffic looks like an obvious saving, since a bus with
  // frames on it is plainly awake, but it makes the second command of a pair unreliable: an
  // open wakes the car and the keep-alive below holds the bus up, so the close that follows
  // would skip the wakeup and go to a BCM that is not ready to act. The tell is the interior
  // lamp, the last step of CommandWakeup: it flashes on the command that ran a wakeup and not
  // on the one that skipped it. An electrically live bus is not a BCM ready to accept device
  // control.
  //
  // The ~4 s of vTaskDelay inside CommandWakeup is survivable on this path. It is NOT
  // survivable on a ticker, which is why the follow-up re-sends below use bare writes.
  ESP_LOGI(TAG, "Windows %s: waking before commanding", up ? "UP" : "DOWN");
  CommandWakeup();
  vTaskDelay(VA_CHGSEQ_WAKE_SECS * 1000 / portTICK_PERIOD_MS);

  const uint8_t p[] = { 0xae, 0x3b, 0xff, dir, dir, dir, dir };
  ESP_LOGI(TAG, "Windows %s", up ? "UP" : "DOWN");
  if (!GmlanDeviceControl(m_can1, 0x241, p, sizeof(p)))
    return Fail;

  // Hold the bus awake while the glass travels. The BCM stops broadcasting
  // Window_Position_Status_LS (0x325) within a second or two of the bus going quiet, so
  // otherwise only a partial mid-travel snapshot arrives and the position metrics stay
  // frozen part open forever, which is worse than reporting nothing because it looks live.
  // Observed on-car: a commanded close leaves three windows reading 83% while they are
  // physically shut.
  //
  // Handed to the ticker rather than looped here. Doing it inline blocks the calling task for
  // ~15s, and when the command arrives over MQTT that task is "OVMS Events", which the task
  // watchdog then kills: abort() and a reboot, on every single press.
  // Commands must return promptly; anything that needs to span seconds belongs in Ticker1.
  m_window_wake = VA_WINDOW_WAKE_SECS;

  // Re-send the same request once a second for a few seconds. A single frame moves the three
  // passenger windows but frequently leaves the DRIVER window untouched, reproduced on-car
  // repeatedly, each time needing a second send. That window is the
  // express one and behaves as though the request has to be sustained, the way holding the
  // switch would, rather than pulsed once. Re-sending is harmless for the other three, which
  // simply stay at the limit they already reached.
  //
  // From the ticker, not a loop here: this runs on the events task when the command arrives
  // over MQTT, and blocking it trips the task watchdog. GmlanWrite is a bare bus write with no
  // delay, so it is safe to call from Ticker1, unlike GmlanDeviceControl with its 50 ms wait.
  m_window_cmd_dir = dir;
  m_window_cmd_left = VA_WINDOW_CMD_REPEATS;
  return Success;
  }

//----- console shims ------------------------------------------------------------------

OvmsVehicleVoltAmpera* OvmsVehicleVoltAmpera::GetActiveVehicle(OvmsWriter* writer)
  {
  OvmsVehicle* v = MyVehicleFactory.ActiveVehicle();
  if (v == NULL || strcmp(MyVehicleFactory.ActiveVehicleType(), "VA") != 0)
    {
    if (writer) writer->puts("Error: Volt/Ampera vehicle module is not selected");
    return NULL;
    }
  return (OvmsVehicleVoltAmpera*)v;
  }

static const char* va_cmdresult(OvmsVehicle::vehicle_command_t r)
  {
  switch (r)
    {
    case OvmsVehicle::Success:        return "Sent";
    case OvmsVehicle::NotImplemented: return "Error: needs the SWCAN adapter (xva/use_swcan_adapter)";
    default:                          return "Error: refused - see log";
    }
  }

void OvmsVehicleVoltAmpera::shell_engine(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicleVoltAmpera* v = GetActiveVehicle(writer);
  if (v == NULL) return;
  const char* what = cmd->GetName();
  va_engine_mode_t mode = (strcmp(what, "on") == 0)  ? VA_ENG_FORCE_ON
                        : (strcmp(what, "off") == 0) ? VA_ENG_FORCE_OFF
                                                     : VA_ENG_AUTO;
  vehicle_command_t r = v->CommandEngine(mode, (argc > 0) ? argv[0] : NULL);
  if (r != Success)
    {
    writer->puts(va_cmdresult(r));
    return;
    }
  // The 02 EE 31 ack means "accepted", not "running", so report only that.
  if (mode == VA_ENG_AUTO)
    writer->puts("Engine override released (HPCM back in control)");
  else
    writer->printf("Engine override %s asserted, holding with keep-alive.\n"
                   "This is the override state, not proof the engine is running, watch "
                   "v.b.current going negative (charging) for the generator.\n",
                   (mode == VA_ENG_FORCE_ON) ? "ON" : "OFF");
  }

void OvmsVehicleVoltAmpera::shell_trunk(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicleVoltAmpera* v = GetActiveVehicle(writer);
  if (v == NULL) return;
  writer->puts(va_cmdresult(v->CommandTrunk(argv[0])));
  }

void OvmsVehicleVoltAmpera::shell_chargelimit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicleVoltAmpera* v = GetActiveVehicle(writer);
  if (v == NULL) return;
  const char* what = cmd->GetName();

  if (strcmp(what, "override") == 0)
    {
    v->SetLimitOverride(true);
    v->m_override_odo = StandardMetrics.ms_v_pos_odometer->AsFloat();
    writer->printf("Charge limit overridden: charging to full this time.\n");

    // Setting the flag only stops FUTURE defers. A defer already programmed into the HPCM
    // leaves the car in departure-based charging, where it will sit indefinitely, so send
    // the resume too, or this promises a full charge and silently does nothing.
    //
    // Unconditional rather than gated on m_defer_active: that flag does not survive a
    // reboot, so OVMS can easily be unaware of a defer the car is still honoring. The
    // resume is idempotent (it just selects immediate charging), so sending it when no
    // defer is active costs nothing.
    if (v->ChargeStartBegin())
      writer->puts("Resuming charge now (clearing any departure schedule).");
    else
      writer->puts("Could not start the resume sequence; try \"charge start\".");

    if (v->m_override_odo > 0)
      writer->printf("Resets automatically after driving %.1f km.\n", VA_OVERRIDE_RESET_KM);
    else
      writer->puts("Resets automatically when the car leaves the location.");
    return;
    }
  if (strcmp(what, "resume") == 0)
    {
    v->SetLimitOverride(false);
    writer->puts("Charge limit re-armed.");
    return;
    }

  // status
  writer->printf("Charge limit:   %s\n", v->m_chargelimit_enabled ? "enabled" : "disabled");
  writer->printf("Target SOC:     %d%% (%s scale, now %.1f%%)\n", v->m_chargelimit_soc,
    v->m_chargelimit_use_raw ? "raw" : "displayed", v->ChargeLimitSoc());
  writer->printf("Location:       %s\n",
    v->m_chargelimit_location.empty() ? "(anywhere)" : v->m_chargelimit_location.c_str());
  writer->printf("Currently in:   %s\n", v->ChargeLimitInLocation() ? "yes" : "no");
  writer->printf("Override:       %s\n", v->m_limit_override ? "ACTIVE (charging to full)" : "no");
  if (v->m_limit_override && v->m_override_odo > 0)
    writer->printf("  clears in:    %.2f km\n",
      VA_OVERRIDE_RESET_KM - (StandardMetrics.ms_v_pos_odometer->AsFloat() - v->m_override_odo));
  writer->printf("Defers used:    %d of %d\n", v->m_defer_count, v->m_chargelimit_maxdefer);
  writer->printf("Defer applied:  %s\n", v->m_defer_active ? "yes" : "no");
  // Two separate facts, and they disagreeing is exactly the bug worth surfacing: the first is
  // what OVMS believes, the second what the car is actually doing.
  writer->printf("Car reports:    %s\n",
    v->mt_charge_deferred->IsDefined()
      ? (v->mt_charge_deferred->AsBool() ? "DEPARTURE MODE (deferred)" : "immediate charging")
      : "(not read yet - needs the car awake)");
  }


//=====================================================================================
// OnStar telematics alerts: horn, lights, and the two together ("vehicle locate")
//
// The frame is 0x1024E097, which opendbc names Telematics_Contol_LS (PID 0x0127, 3 bytes).
// The whole frame is field-decoded, and the layout is pinned by the frames this component
// sends for lock and trunk, whose byte values are known good:
//
//   byte0  0x02  EnhSrvRClsRlsRq   trunk            (CommandTrunk sends 02 00 FF)
//          0x0C  EnhSrvVisAlRq     flash lights     (CommandLock sends 0C 00 FF)
//          0x30  EnhSrvAudAlRq     HORN
//          0xC0  EnhSrvRmStrtRq    remote start     (2 = on, 1 = off; preheat uses 80/40)
//   byte1  0x07  EnhSrvLckRq       lock 1 / unlock 3
//   byte2        EnhSvVehTopSpdLim top speed limiter, 0xFF = none. NOT exposed: it caps the
//                                  car's speed, which is not something a button should do.
//
// The 0x0C that CommandLock/CommandUnlock send after the lock byte is OnStar's visual
// confirmation flash, not a "commit/apply" step. It is optional, not required for the lock
// to take.
//
// One precondition: the SWCAN bus must be awake, or the
// controller goes transmit error-passive with no node to ACK and every frame silently fails.
// CommandWakeup() below covers that.
//
// Unlike the heated seats, this does NOT need the body powered. The seat module only answers
// once the car itself is up, but the BCM acts on these alerts from a plain bus wake.
OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandTelematicsAlert(bool horn,
                                                                            bool lights)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  if (!m_use_swcan_adapter)
    return NotImplemented;
  if (!m_control_enabled)
    {
    ESP_LOGE(TAG, "Horn and lights need config xva/control.enabled");
    return Fail;
    }

  uint8_t b0 = (horn ? 0x30 : 0x00) | (lights ? 0x0C : 0x00);
  ESP_LOGI(TAG, "Telematics alert: %s%s%s (byte0 0x%02x)",
           horn ? "horn" : "", (horn && lights) ? " + " : "", lights ? "lights" : "", b0);

  CommandWakeup();
  CAN_frame_t txframe;
  vTaskDelay(720 / portTICK_PERIOD_MS);
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, b0, 0x00, 0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00, 0x00, 0xff)   // release
  return Success;
#else
  return NotImplemented;
#endif
  }

void OvmsVehicleVoltAmpera::shell_alert(int verbosity, OvmsWriter* writer, OvmsCommand* cmd,
                                        int argc, const char* const* argv)
  {
  OvmsVehicleVoltAmpera* v = GetActiveVehicle(writer);
  if (v == NULL) return;
  const char* what = cmd->GetName();
  bool horn   = (strcmp(what, "horn") == 0 || strcmp(what, "locate") == 0);
  bool lights = (strcmp(what, "flash") == 0 || strcmp(what, "locate") == 0);
  writer->puts(va_cmdresult(v->CommandTelematicsAlert(horn, lights)));
  }

void OvmsVehicleVoltAmpera::shell_windows(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicleVoltAmpera* v = GetActiveVehicle(writer);
  if (v == NULL) return;
  writer->puts(va_cmdresult(v->CommandWindows(strcmp(cmd->GetName(), "up") == 0, argv[0])));
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandStopCharge()
  {
  SetCmdPending("charge stopping", StandardMetrics.ms_v_charge_inprogress);
  if (!m_chargelimit_enabled)
    {
    ESP_LOGE(TAG, "Charge control is disabled (set xva/chargelimit.enabled)");
    return Fail;
    }
  // Mark this defer as the owner's, not the limiter's. Both end up in the same HPCM state and
  // set m_defer_active, but the limiter auto-resumes below its target, which would undo a
  // manual stop within ten seconds, every time, at any SOC under the threshold.
  if (!ChargeDeferBegin())
    return Fail;
  m_defer_manual = true;
  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandStartCharge()
  {
  SetCmdPending("charge starting", StandardMetrics.ms_v_charge_inprogress);
  if (!m_chargelimit_enabled)
    {
    ESP_LOGE(TAG, "Charge control is disabled (set xva/chargelimit.enabled)");
    return Fail;
    }
  m_defer_manual = false;
  return ChargeStartBegin() ? Success : Fail;
  }

void OvmsVehicleVoltAmpera::PollRunFinished(canbus* bus)
  {
  if(m_poll_state == 2)
    {
    // OvmsPoller::Do_PollSetState() pre-sets m_poll_run_finished, and PollerSend() evaluates
    // that latch before its init_ticker early-return, so this callback fires once immediately
    // on entering a state, before a single request has gone out. Taking it at face value
    // would tear state 2 down before any cell is polled, leaving v.b.c.* permanently empty.
    // Only accept the callback once the sweep has actually reached the last cell.
    if (!m_state2_swept)
      return;
    PollSetState(1);
    PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
    }
  }

// Silent-charge polling supervision (poll state 3). A deferred or scheduled AC charge
// starts without waking either CAN bus, so the only entry signal is the 12V rail held
// high by the APM. Exit is by poll replies drying up: the BECM/OBC answer polls for as
// long as the charger is active. Called once per second from Ticker1, before the charge
// detection block so cleared metrics take effect the same tick.
void OvmsVehicleVoltAmpera::ChargePollTicker()
  {
  if (m_silent_charge_backoff > 0)
    m_silent_charge_backoff--;

  if (m_poll_state == 0 && m_startPolling_timer == 0 && m_silent_charge_backoff == 0
    && StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= VA_CHARGING_12V_POLL_THRESHOLD)
    {
    if (++m_silent_charge_timer >= VA_SILENT_CHARGE_DELAY)
      {
      ESP_LOGI(TAG,"12V at %.2fV while asleep, probing for a silent charge (state 3)",
        StandardMetrics.ms_v_bat_12v_voltage->AsFloat());
      m_silent_charge_timer = 0;
      m_last_poll_reply = monotonictime;
      PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
      PollSetState(3);
      }
    }
  else
    m_silent_charge_timer = 0;

  if (m_poll_state == 3 && (monotonictime - m_last_poll_reply) > VA_CHARGE_POLL_TIMEOUT)
    {
    // The BECM/OBC stopped answering: the charge has ended, or the 12V trigger was a
    // false positive (surface charge). Clear the charger metrics so the charge-stop
    // logic below runs on real data instead of the last polled values.
    ESP_LOGI(TAG,"No poll replies for %ds in state 3, going to sleep", VA_CHARGE_POLL_TIMEOUT);
    StandardMetrics.ms_v_charge_current->SetValue(0);
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    StandardMetrics.ms_v_env_awake->SetValue(false);
    m_silent_charge_backoff = VA_SILENT_CHARGE_BACKOFF;
    PollSetState(0);
    }
  }

void OvmsVehicleVoltAmpera::Ticker1(uint32_t ticker)
  {
  // Check if the car has gone to sleep
  if (m_candata_timer > 0)
    {
    if (--m_candata_timer == 0)
      {
      // Car body has gone to sleep
      StandardMetrics.ms_v_env_gear->SetValue(-2);
      StandardMetrics.ms_v_env_on->SetValue(false);
      if (StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
        // The body sleeps during an AC charge but the BECM/OBC still answer polls. Keep
        // v.e.awake true (the charge systems are up); ChargePollTicker clears it when
        // they stop answering. When already in state 3 this timeout just means replies
        // stopped: re-seeding m_last_poll_reply here would delay the state-3 exit.
        if (m_poll_state != 3)
          {
          ESP_LOGI(TAG,"Car body asleep, charge in progress: keep polling (state 3)");
          m_last_poll_reply = monotonictime;
          PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
          PollSetState(3);
          }
        }
      else
        {
        ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
        StandardMetrics.ms_v_env_awake->SetValue(false);
        // Instantaneous readings only broadcast while the car is awake, so they would sit
        // frozen at their last value: a parked car showing several amps of 12V draw, and
        // a throttle position, indefinitely. The server re-sends every metric every 20
        // minutes regardless of change, so this cannot be left to staleness at the far end.
        StandardMetrics.ms_v_bat_12v_current->SetValue(0, Amps);
        StandardMetrics.ms_v_env_throttle->SetValue(0);
        StandardMetrics.ms_v_mot_rpm->SetValue(0);
        // Leaving state 3 through this path must arm the probe backoff too, or the 12V
        // maintenance cycle (DC/DC holds ~13.4V after a charge) re-probes every 15s in a
        // 0<->3 flap: stray chatter arms the CAN timer, it expires here, probe re-fires.
        if (m_poll_state == 3)
          m_silent_charge_backoff = VA_SILENT_CHARGE_BACKOFF;
        PollSetState(0);
        }
      }
    }

  if (m_startPolling_timer > 0)
    {
    if (--m_startPolling_timer == 0)
      {
      if(m_poll_state == 0)
        {
        // Start polling with delay (battery module need time to wake up)
        m_state2_swept = false;
        PollSetThrottling(VA_POLLING_HIGH_THROTTLING);  // get all cells info before sleep
        PollSetState(2);
        }
      }
    }
  
  // Keep the car reporting window position across a movement by putting a little traffic on
  // the bus each second, so its modules do not drop straight back to sleep.
  //
  // Deliberately NOT CommandWakeup() here: that blocks for about four seconds (sixteen
  // vTaskDelay calls), and Ticker1 runs on the vehicle task, where stalling that long trips
  // the task watchdog. One frame, no delays, no transceiver mode change: the car is already
  // awake at this point, it just needs to stay that way.
  if (m_swcan_live > 0)
    m_swcan_live--;

  // Busy indicator. Lock, climate and charging are plain booleans that snap from one state to
  // the other with nothing in between, so there is no way to show that a command is in flight
  // the way the windows' own opening/closing does, so it is published here: set on accepting a
  // command, cleared as soon as the metric it should move actually moves, or after
  // VA_CMD_PENDING_SECS if the car never answers. The timeout matters as much as the signal,
  // since a command that silently failed must not leave the dashboard spinning forever.
  if (m_pending_secs > 0)
    {
    bool landed = (m_pending_watch != NULL)
                  && (m_pending_watch->AsString() != m_pending_seen);
    if (landed || --m_pending_secs == 0)
      {
      mt_cmd_pending->SetValue("");
      m_pending_secs = 0;
      m_pending_watch = NULL;
      }
    }

  // Deferred window command: send as soon as the bus is genuinely live, or when the wait runs
  // out (better to command a quiet bus than to silently drop what the owner asked for).
  if (m_window_pending > 0 && m_can1 != NULL)
    {
    m_window_pending--;
    if (m_swcan_live > 0 || m_window_pending == 0)
      {
      const uint8_t p[] = { 0xae, 0x3b, 0xff, m_window_cmd_dir, m_window_cmd_dir,
                            m_window_cmd_dir, m_window_cmd_dir };
      ESP_LOGI(TAG, "Windows: bus %s, sending command now",
               m_swcan_live > 0 ? "live" : "still quiet, sending anyway");
      GmlanDeviceControl(m_can1, 0x241, p, sizeof(p));
      m_window_pending = 0;
      m_window_cmd_left = VA_WINDOW_CMD_REPEATS;
      }
    }

  if (m_window_cmd_left > 0 && m_can1 != NULL)
    {
    m_window_cmd_left--;
    const uint8_t p[] = { 0xae, 0x3b, 0xff, m_window_cmd_dir, m_window_cmd_dir,
                          m_window_cmd_dir, m_window_cmd_dir };
    GmlanWrite(m_can1, 0x241, p, sizeof(p));
    }

  if (m_window_wake > 0)
    {
    m_window_wake--;
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    // A 0x100 frame in NORMAL transceiver mode does not wake a sleeping single wire bus. It
    // is transmitted happily (tx counters climb, no failures) and nothing on the bus hears
    // it, which makes the failure invisible: the keepalive looks healthy while the BCM stays
    // asleep and never broadcasts 0x325, so a commanded move produces no status at all and
    // the metrics keep whatever they held. Only the high voltage wakeup mode wakes the bus.
    if (p_swcan != NULL && p_swcan_if != NULL && m_use_swcan_adapter
        && !StandardMetrics.ms_v_env_awake->AsBool())
      {
      CAN_frame_t txframe;
      p_swcan_if->SetTransceiverMode(tmode_highvoltagewakeup);
      FRAME_FILL(0, p_swcan, txframe, 0x100, 0, 0,0,0,0,0,0,0,0)
      txframe.callback = &wakeup_frame_sent;      // restores normal mode when sent
      p_swcan->Write(&txframe);
      }
    else if (p_swcan != NULL)
      {
      // Already awake: a plain frame is enough to keep it that way.
      CAN_frame_t txframe;
      SEND_STD_FRAME(p_swcan, txframe, 0x100, 8, 0,0,0,0,0,0,0,0)
      }
#endif
    }

  // 0x325 only broadcasts while the bus is awake, so a movement state would otherwise stick
  // forever once the car sleeps mid-travel. Settle it back to open/closed.
  if (m_window_settle > 0 && --m_window_settle == 0 && m_window_last >= 0)
    mt_windows->SetValue(m_window_last == 0 ? "closed" : "open");

  DriveCycleAccumulate();
  ChargePollTicker();
  ChargeSeqTicker();
  EngineTicker();

  PreheatWatchdog();

  if (m_controlled_lights)
    {
    m_tester_present_timer++;
    if ( (m_tester_present_timer > VA_TESTER_PRESENT_TIMEOUT) || (StandardMetrics.ms_v_env_gear->AsInt(-2) > -2) )
      {
      // Lights have been on too long or the car isn't parked anymore -> prevent Tester Present message (lights will go off after few secs)
      ESP_LOGI(TAG,"Lights on timeout OR car not parked -> turning off");
      m_tester_present_timer = 0;
      m_controlled_lights = (va_light_t)0;
      }
    else
      SendTesterPresentMessage(VA_BCM);
    }

  // During a deferred-charge pause v.c.pilot stays latched (see the charge-stop branch
  // below), because this car has no direct plug-state signal to poll. The one trustworthy
  // unplug signal is being driven: no AC charge can be active at speed. Preheat sets
  // env_on with speed 0, so the speed gate keeps it from counting as a drive.
  if (StandardMetrics.ms_v_charge_pilot->AsBool()
    && !StandardMetrics.ms_v_charge_inprogress->AsBool()
    && StandardMetrics.ms_v_env_on->AsBool()
    && StandardMetrics.ms_v_pos_speed->AsFloat() > 1)
    {
    ESP_LOGI(TAG,"Driving with no charge active: charge cable is out");
    StandardMetrics.ms_v_charge_pilot->SetValue(false);
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    }

  int cc = StandardMetrics.ms_v_charge_current->AsInt();
  int cv = StandardMetrics.ms_v_charge_voltage->AsInt();

  if ((cc != 0)&&(cv != 0))
    {
    // The car is charging
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    StandardMetrics.ms_v_env_awake->SetValue(true);  // charge systems are up and answering
    // Nothing else publishes v.c.power for this vehicle, and consumers read it as 0 rather
    // than "unknown", so derive it from the charger readings that are available.
    StandardMetrics.ms_v_charge_power->SetValue((float)cc * cv / 1000, kW);
    if (StandardMetrics.ms_v_charge_inprogress->AsBool() == false)
      {
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      // The real selected level arrives on SWCAN 0x1086C0CB and is set from the charging-limits
      // decode. The car keeps a per-location setting (8A away, 12A at home on this vehicle), so
      // writing a fixed 16 unconditionally would clobber the true value on every charge start
      // until the next broadcast corrected it. Fall back to it only while nothing better is
      // known, which is the case on cars that never broadcast that frame.
      if (!StandardMetrics.ms_v_charge_climit->IsDefined())
        StandardMetrics.ms_v_charge_climit->SetValue(16);
      if (m_defer_active)
        {
        // The HPCM resumed a deferred charge (the whack-a-mole): same plug-in session,
        // ChargeLimitTicker will re-defer. Notifying every cycle would spam the owner.
        ESP_LOGI(TAG,"Car resumed a deferred charge");
        }
      else
        {
        // Do NOT call NotifyChargeState() here. Writing v.c.state already arms the framework's
        // own notification (MetricModified -> m_chargestate_ticker -> NotifyChargeState), and
        // m_autonotifications defaults to true, so a manual call on top sends the message
        // twice: measured on-car, "Standard - Charging" arrives once, then again three
        // seconds later, the gap being the ticker delay.
        ESP_LOGI(TAG,"Car has started a charge");
        StandardMetrics.ms_v_charge_kwh->SetValue(0);   // energy for THIS session
        m_charge_timer = 0;
        m_charge_wm = 0;
        }
      }
    else
      {
      // A charge is ongoing
      m_charge_timer++;
      if (m_charge_timer >= 60)
        {
        m_charge_timer -= 60;
        m_charge_wm += (StandardMetrics.ms_v_charge_voltage->AsInt()
                      * StandardMetrics.ms_v_charge_current->AsInt());
        if (m_charge_wm > 60000)
          {
          // 60000 watt-minutes is 1 kWh, so add 1. Adding 10 would match the v2 server wire
          // format, which scales the metric by 10 itself (ovms_server_v2 charge record), but
          // would leave v.c.kwh reading ten times the real energy everywhere else.
          StandardMetrics.ms_v_charge_kwh->SetValue(
            StandardMetrics.ms_v_charge_kwh->AsFloat() + 1);
          m_charge_wm -= 60000;
          }
        }
      }
    }
  else if ((cc == 0) && (cv == 0 || m_defer_active))
    {
    // The car is not charging. During a limiter defer pause, current alone decides: the OBC may
    // still report the AC line voltage while delivering 0 A, which would otherwise leave a
    // phantom "charging" latched and burn the whole defer budget in minutes. Outside a
    // defer, keep requiring both zero: GM's factory scheduled charging produces the same
    // 0 A + line-voltage signature between windows, and flagging each window boundary as
    // interrupted/started would spam owners who use it.
    //
    // Publish zero rather than leaving the metric undefined: consumers that have never seen
    // a charge would otherwise show "unknown" indefinitely instead of "not charging".
    StandardMetrics.ms_v_charge_power->SetValue(0, kW);
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      // A stop close to the set point is the limiter's own defer pause; one well past it means
      // limiter lost control mid-session (gave up, or the location gate flipped) and the
      // charge ended for real. Relative to the limit, so limits of 95-99% work too.
      if (m_defer_active && (int)ChargeLimitSoc() <= m_chargelimit_soc + 2)
        {
        // The limiter paused this charge (SOC limit defer): the cable is still in. Keep v.c.pilot
        // latched so ChargeLimitTicker sees the same plug-in session (defer budget and
        // once-per-session notification survive), and skip the framework notification, since
        // the limiter already sent its own "Charging paused" message.
        ESP_LOGI(TAG,"Charge paused by SOC limit defer");
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        StandardMetrics.ms_v_charge_substate->SetValue("scheduledstop");
        }
      else
        {
        // The charge has completed/stopped. A charge that ran to 95%+ is over regardless
        // of any defer bookkeeping (e.g. the location gate flipped mid-session): drop the
        // flag so the NotifyChargeState override does not swallow the done notification.
        m_defer_active = false;
  m_defer_manual = false;
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        // Dashboard scale on purpose: "nearly full" is a display-side judgement, and a full
        // pack only reads ~96% raw, so this test against raw would call every charge
        // interrupted.
        float soc_done = mt_soc_displayed->IsDefined()
                       ? mt_soc_displayed->AsFloat()
                       : StandardMetrics.ms_v_bat_soc->AsFloat();
        if (soc_done < 95)
          {
          ESP_LOGI(TAG,"Car charge session was interrupted");
          StandardMetrics.ms_v_charge_state->SetValue("stopped");
          StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
          }
        else
          {
          ESP_LOGI(TAG,"Car charge session completed");
          StandardMetrics.ms_v_charge_state->SetValue("done");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          MyEvents.SignalEvent("vehicle.charge.finish",NULL);
          }
        // Same as above: the v.c.state writes in both branches arm the framework's own
        // notification, so notifying here as well duplicates every stop and completion.
        }

      m_charge_timer = 0;
      m_charge_wm = 0;
      }

    // 12V battery may be charging via High Voltage battery when car is on, or via external battery charger
    if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat() > VA_CHARGING_12V_THRESHOLD)
      StandardMetrics.ms_v_env_charging12v->SetValue(true);
    else
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
    }

  }

void OvmsVehicleVoltAmpera::NotifiedVehicleOn()
  {
  ESP_LOGI(TAG,"Powertrain enabled");
  PollSetState(1); // abort state 2 if not complete yet
  PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
  }

void OvmsVehicleVoltAmpera::NotifiedVehicleOff()
  {
  // The framework delays this callback ~3s after env_on flips false
  // (GetNotifyVehicleStateDelay), so it can land AFTER a transition into state 3: both the
  // CAN-timeout handler and the broadcast-silence downgrade clear env_on and then enter
  // state 3. Starting a state-2 sweep here would stomp the charge polling, and if the bus
  // has gone dead, state 2 has no exit path, wedging the module with a phantom charge.
  // A sweep during a charge is unwanted anyway: state 2 exists to read cells WITHOUT HV load.
  if (m_poll_state == 3)
    return;
  ESP_LOGI(TAG,"Powertrain disabled");

  // update all cells info without HV load
  m_state2_swept = false;
  PollSetThrottling(VA_POLLING_HIGH_THROTTLING);
  PollSetState(2);
  }

void OvmsVehicleVoltAmpera::NotifyChargeState()
  {
  // The SOC-limit whack-a-mole flips v.c.state stopped<->charging repeatedly within one
  // plug-in session, and the framework auto-notifies every flip (MetricModified arms a
  // delayed NotifyChargeState no matter who wrote the metric). One "Charging paused"
  // message from the limiter covers the session; suppress the per-cycle spam.
  if (m_defer_active)
    {
    ESP_LOGD(TAG,"Charge state notification suppressed (defer active)");
    return;
    }
  OvmsVehicle::NotifyChargeState();
  }

void OvmsVehicleVoltAmpera::NotifiedVehicleAwake()
  {
  NotifyMetrics();
  }

void OvmsVehicleVoltAmpera::CommandWakeupComplete( const CAN_frame_t* p_frame, bool success )
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  if(!m_use_swcan_adapter)
    return;
  ESP_LOGI(TAG,"CommandWakeupComplete. Success: %d", success);

  // Switch to normal mode no matter if the wakeup msg was sent successfully or not
  vTaskDelay(20 / portTICK_PERIOD_MS);  
  p_swcan_if->SetTransceiverMode(tmode_normal);
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }


#define WAKEUP_DELAY_1 220
#define WAKEUP_DELAY_2 360
#define WAKEUP_DELAY_3 180

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandWakeup()
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  if(!m_use_swcan_adapter)
    return NotImplemented;
  CAN_frame_t txframe;

  p_swcan_if->SetTransceiverMode(tmode_highvoltagewakeup);

  // Send the 0x100 message with callback so that High Voltage Wakeup mode can be exited
  FRAME_FILL(0, p_swcan, txframe,   0x100, 0, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)
  txframe.callback = &wakeup_frame_sent;
  p_swcan->Write(&txframe);

  vTaskDelay(WAKEUP_DELAY_1 / portTICK_PERIOD_MS);

  // Body Control Module (BCM)
  SEND_STD_FRAME(p_swcan, txframe,  0x621, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

  if (m_extended_wakeup)
    {
    ESP_LOGI(TAG,"Sending extended wake up messages...");
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x621, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    SEND_STD_FRAME(p_swcan, txframe,  0x100, 8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)

    // ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x620, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x620, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);

    // Theft Deterrent Module (TDM) ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x622, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x622, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x100, 8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)

    // Sensing and Diagmostic module (SDM) ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x627, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x627, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // Instrument Panel Cluster (IPC)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62c, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62c, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62d, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62d, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // Heating Ventilation Air Conditioning (HVAC)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x631, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x631, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
  }

  FlashLights(Interior_lamp);

  ESP_LOGI(TAG,"CommandWakeup End");
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }



OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandLock(const char* pin)
  {
  if (!PinCheck(pin))
    {
    ESP_LOGE(TAG, "Lock: PIN check failed");
    return Fail;
    }
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  if(!m_use_swcan_adapter)
    return NotImplemented;
  SetCmdPending("locking", StandardMetrics.ms_v_env_locked);
  CommandWakeup();

  CAN_frame_t txframe;

  // Onstar lock command emulation
  // Does not seem to work on MY2014 Ampera

  // Telematics control
  vTaskDelay(720 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x01,0xff)
  vTaskDelay(180 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x0c,0x00,0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)
  /*
  Alternative: Diagnostic lock command. But will not arm alarm!
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8,  0x01,0x3e,0x00,0x00,0x00,0x00,0x00,0x00)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8,  0x07,0xae,0x01,0x01,0x01,0x00,0x00,0x00)
  */

  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandUnlock(const char* pin)
  {
  if (!PinCheck(pin))
    {
    ESP_LOGE(TAG, "Unlock: PIN check failed");
    return Fail;
    }
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  if(!m_use_swcan_adapter)
    return NotImplemented;
  SetCmdPending("unlocking", StandardMetrics.ms_v_env_locked);
  CommandWakeup();

  CAN_frame_t txframe;
  // Onstar unlock command emulation
  // Does not seem to work on MY2014 Ampera

  // Telematics control
  vTaskDelay(720 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x03,0xff)
  vTaskDelay(180 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x0c,0x00,0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)

  /*
  Alternative: Diagnostic unlock command. WILL NOT DISARM ALARM! 
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x01,0x3e,0x00,0x00,0x00,0x00,0x00,0x00)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  // Driver door unlock
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x07,0xae,0x01,0x04,0x04,0x00,0x00,0x00)
  // Passengers door unlock
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x07,0xae,0x01,0x02,0x02,0x00,0x00,0x00)
  */

  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandLights(va_light_t lights, bool turn_on)
  {
  ESP_LOGI(TAG,"CommandLights: lights 0x%" PRIx32 ":%d",(uint32_t)lights,turn_on);
  SendTesterPresentMessage(VA_BCM);
  vTaskDelay(200 / portTICK_PERIOD_MS);  

  CAN_frame_t txframe;
  uint8_t *d = txframe.data.u8;
  if (lights & Park)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x07, 0xff, 0x2f, 0xff, 0x2f, 0xff)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x07, 0xff, 0x00, 0x00, 0x00, 0x00)
      }

  if (lights & Charging_indicator)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x14, 0x00, 0x00, 0x02, 0x00, 0x02)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x14, 0x00, 0x00, 0x02, 0x00, 0x00)
      }

  if (lights & Interior_lamp)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x08, 0x01, 0x7f, 0xff, 0x00, 0x00)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00)
      }

  if ( (lights & Left_rear_signal) || (lights & Right_rear_signal) || (lights & Driver_front_signal) || (lights & Passenger_front_signal) 
      || (lights & Center_stop_lamp) )
    {
    // Light group 0x02
    FRAME_FILL(0, m_can1, txframe, VA_BCM, 8, 0x07,0xae,0x02,0x00,0x00,0x00,0x00,0x00)

    if (lights & Left_rear_signal)
        {
        d[3] |= 0x20;
        if (turn_on)
          d[4] |= 0x20;
        }

    if (lights & Right_rear_signal)
        {
        d[3] |= 0x80;
        if (turn_on)
          d[4] |= 0x80;
        }

    if (lights & Driver_front_signal)
        {
        d[3] |= 0x10;
        if (turn_on)
          d[4] |= 0x10;
        }

    if (lights & Passenger_front_signal)
        {
        d[3] |= 0x40;
        if (turn_on)
          d[4] |= 0x40;
        }

    if (lights & Center_stop_lamp)
        {
        d[5] |= 0x20;
        if (turn_on)
          d[6] |= 0x20;
        }

    m_can1->Write(&txframe);
    return Success;
    }

  // Set the bitwise status of the lights that we control and are now ON. If any of these are set, we send periodic Tester Present messages
  m_controlled_lights = (m_controlled_lights & ~lights) | (lights*turn_on);
  ESP_LOGI(TAG,"CommandLights: controlled_lights 0x%" PRIx32,m_controlled_lights);
  return Success;
  }

void OvmsVehicleVoltAmpera::FlashLights(va_light_t light, int interval, int count)
  {
  for (int i=0;i<count;i++)
    {
    CommandLights(light, true);
    vTaskDelay(interval / portTICK_PERIOD_MS);
    CommandLights(light, false);      
    if (i+1 < count)
      vTaskDelay(interval / portTICK_PERIOD_MS);
    }
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandHomelink(int button, int durationms)
  {
  switch (button)
    {
    case 0:
      return CommandClimateControl(true);
      break;
    case 1:
      return CommandClimateControl(false);
      break;
    case 2:
      return CommandWakeup();
      break;
    default:
      break;
    }
  return NotImplemented;
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandSetChargeCurrent(uint16_t limit)
  {
  ESP_LOGI(TAG,"CommandSetChargeCurrent: %d amps",limit);
  CommandWakeup();

  int highest=0;
  int highest_index=-1;
  // Find the highest possible available charging limit
  for (int i=0;i<mt_charging_limits->GetSize();i++)
    {
    int lim = mt_charging_limits->GetElemValue(i);
    ESP_LOGI(TAG,"CommandSetChargeCurrent: %d:%d",i,lim);
    if ( (lim<=limit) && (lim > highest) )
      {
      highest = mt_charging_limits->GetElemValue(i);
      highest_index = i;
      }
    }
  if (highest_index == -1)
    {
    ESP_LOGE(TAG,"CommandSetChargeCurrent: No valid current limit found!");
    return Fail;      
    }

  ESP_LOGI(TAG,"CommandSetChargeCurrent: Selected charging limit %d:%d amps",highest_index,highest);
  CAN_frame_t txframe;
  SEND_EXT_FRAME(p_swcan, txframe, 0x10864080, 8, (highest_index+1) << 4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  return Success;
  }

void OvmsVehicleVoltAmpera::SendTesterPresentMessage( uint32_t id )
  {
  CAN_frame_t txframe;
  SEND_STD_FRAME(m_can1, txframe, id, 8, 0x01, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  }


class OvmsVehicleVoltAmperaInit
  {
  public: OvmsVehicleVoltAmperaInit();
} MyOvmsVehicleVoltAmperaInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVoltAmperaInit::OvmsVehicleVoltAmperaInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Chevrolet Volt/Ampera (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVoltAmpera>("VA","Chevrolet Volt/Ampera");
  }
