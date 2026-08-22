/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform — charge session report
   Date:          5th June 2026

   (C) 2026       Jerry Kezar <solterra@kezarnet.com>

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

   Writes a self-contained HTML report to /store/charge-reports/ at the end of each
   charging session (plug-in to unplug). This is the first increment: a single-phase,
   live-telemetry summary. Multi-phase tracking, sleep-survival / summary-mode,
   limiting-side attribution, the per-sample timeline and the error DID-dump
   (see solterra-can docs/charging_state_machine_architecture.md §6) are follow-ups.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include <sdkconfig.h>
#include "ovms_log.h"
#include "metrics_standard.h"
#ifdef CONFIG_OVMS_COMP_LOCATION
#include "ovms_location.h"
#endif
#include "vehicle_toyota_etnga.h"

// Prefer the SD card (GBs, removable) for reports+CSV; fall back to internal flash.
std::string OvmsVehicleToyotaETNGA::ChargeReportDir()
{
    struct stat st;
    if (stat("/sd", &st) == 0 && S_ISDIR(st.st_mode))
        return "/sd/charge-reports";
    return "/store/charge-reports";
}

// Map a 0x1688 "Charging History Information" enum code to a human-readable label.
// All 26 states sourced from solterra-can/ecus/plug-in-charge-control.md (Known enum values).
// 6 of 26 are empirically confirmed; the rest are Techstream-inferred labels.
// Returns "" for unknown codes so the caller can fall back to showing raw hex.
const char* OvmsVehicleToyotaETNGA::ChargeOutcomeLabel(int code)
{
    switch (code & 0xFF) {
        case 0x00: return "Default";
        case 0x20: return "Default (AC Charging)";
        case 0x21: return "AC Charging Complete (Full Charge)";
        case 0x23: return "AC Charging Stop (Abnormal)";
        case 0x24: return "AC Charging Stop (Battery)";
        case 0x25: return "AC Charging Stop (High Power Consumption)";
        case 0x26: return "AC Charging Stop (Operation)";
        case 0x27: return "AC Charging Stop (Power Outage/Unplugged)";
        case 0x28: return "AC Charging Stop (Reduced Supply Power)";
        case 0x29: return "AC Charging Stop (System)";
        case 0x2A: return "AC Charging Complete (Freeze Prevention)";
        case 0x2C: return "AC Charging Stop (Connector Unlock)";
        case 0x30: return "Default (DC Charging)";
        case 0x31: return "DC Charging Complete";
        case 0x32: return "DC Charging Stop (Abnormal)";
        case 0x33: return "DC Charging Stop (Battery)";
        case 0x38: return "DC Charging Stop (Over 60 Minutes)";
        case 0x39: return "DC Charging Stop (System)";
        case 0x3A: return "DC Charging Stop (Vehicle/System)";
        case 0x40: return "Default (Power Feeding)";
        case 0x41: return "Battery Level Low";
        case 0x42: return "Fuel Level Low";
        case 0x43: return "VPC Unplugged";
        case 0x44: return "IG OFF Operation";
        case 0x45: return "Remote Stop";
        case 0x46: return "Vehicle Factor";
        default:   return "";   // unknown -> caller shows raw hex
    }
}

// Map a 0x1666 "DC HLC state" enum code (ISO 15118 high-level-communication state machine)
// to a human-readable label. Codes sourced from solterra-can (2026-05 DC-charge captures;
// 6 empirically confirmed, the rest inferred from the CCS state sequence).
// Returns "" for unknown codes so the caller skips logging them.
const char* OvmsVehicleToyotaETNGA::HlcStateLabel(int code)
{
    switch (code & 0xFF) {
        case 0x00: return "HLC: SLAC";
        case 0x01: return "HLC: SDP";
        case 0x02: return "HLC: App-protocol negotiation";
        case 0x03: return "HLC: Session setup";
        case 0x04: return "HLC: Service discovery";
        case 0x05: return "HLC: Service detail";
        case 0x06: return "HLC: Payment service selection";
        case 0x07: return "HLC: Certificate installation";
        case 0x08: return "HLC: Certificate update";
        case 0x09: return "HLC: Payment details";
        case 0x0A: return "HLC: Authorization";
        case 0x0B: return "HLC: Charge-parameter discovery";
        case 0x0C: return "HLC: Cable check";
        case 0x0D: return "HLC: Precharge";
        case 0x0E: return "HLC: Power delivery (start)";
        case 0x0F: return "HLC: Current demand";
        case 0x10: return "HLC: Power delivery (stop)";
        case 0x11: return "HLC: Welding detection";
        case 0x12: return "HLC: Session stop";
        case 0xFF: return "HLC: Unconnected";
        default:   return "";
    }
}

// Map a 0x1684 "AC Charging Operation Status" enum (confirmed 4-state: Stop/Startup/Running/
// Finishing, per solterra-can) to a human-readable label. Returns "" for Stop(0)/unknown so the
// caller skips it — that keeps DC sessions (where AC-Op stays Stop) free of spurious AC entries;
// the final stop is already covered by the outcome / "Charging paused" / "Unplugged" events.
const char* OvmsVehicleToyotaETNGA::AcOpStatusLabel(int code)
{
    switch (code & 0xFF) {
        case 0x01: return "AC: Startup";
        case 0x02: return "AC: Running";
        case 0x03: return "AC: Finishing";
        default:   return "";   // 0x00 Stop / unknown -> skipped
    }
}

// INC-2: LimSide enum -> short human label. Empty string for unknown (caller omits the row).
const char* OvmsVehicleToyotaETNGA::LimSideLabel(int side)
{
    switch (side) {
        case LIM_STATION: return "station";
        case LIM_CAR:     return "car";
        default:          return "";
    }
}

// Return the name of the first defined OVMS location (geofence) that contains (lat,lon),
// or "" if none match. Used to show a friendly place name in the report alongside coords.
std::string OvmsVehicleToyotaETNGA::LookupLocationName(float lat, float lon)
{
#ifdef CONFIG_OVMS_COMP_LOCATION
    for (LocationMap::iterator it = MyLocations.m_locations.begin();
         it != MyLocations.m_locations.end(); ++it) {
        if (it->second && it->second->IsInLocation(lat, lon))
            return it->first;
    }
#else
    (void)lat; (void)lon;
#endif
    return "";
}

// Append a monotonic-timestamped event to the session event log.
// No-op outside a session (guard matches the in_session flag set in TransitionToChargeHandshakeState).
void OvmsVehicleToyotaETNGA::LogChargeEvent(const char* label)
{
    if (!m_charge_session.in_session)
        return;
    m_charge_session.events.push_back(
        std::make_pair(StandardMetrics.ms_m_monotonic->AsInt(), label));
}

// INC-1: open a new charge phase on CHARGE_AC / CHARGE_DC entry. No-op if a phase is
// already open (guards a 1 s state flap from double-opening). Snapshots the session
// kWh meter so the phase energy is a delta at close.
void OvmsVehicleToyotaETNGA::OpenChargePhase(bool is_dc)
{
    if (!m_charge_session.in_session || m_charge_session.cur >= 0)
        return;
    ChargeSessionState::ChargePhase ph;
    ph.is_dc          = is_dc;
    ph.start_monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    ph.start_utc      = StandardMetrics.ms_m_timeutc->AsInt();
    ph.start_soc      = (int) StandardMetrics.ms_v_bat_soc->AsFloat();
    ph.kwh_at_open    = StandardMetrics.ms_v_charge_kwh->AsFloat();
    m_charge_session.phases.push_back(ph);
    m_charge_session.cur = (int) m_charge_session.phases.size() - 1;
    ESP_LOGD(TAG, "Charge phase %d open (%s)", m_charge_session.cur + 1, is_dc ? "DC" : "AC");
}

// INC-2: classify who capped the DC charge rate for the open phase. DC-only: compares the
// station's advertised max (0x166A) against the car's min permission (0x16A1). The smaller is
// the binding side. AC phases and phases with no caps seen stay LIM_UNKNOWN (report omits the
// row). Thresholds are provisional pending on-vehicle DC-charge tuning.
void OvmsVehicleToyotaETNGA::ClassifyLimitingSide()
{
    if (m_charge_session.cur < 0)
        return;
    ChargeSessionState::ChargePhase& ph = m_charge_session.phases[m_charge_session.cur];
    if (!ph.is_dc)
        return;   // AC attribution deferred (needs unconfirmed OBC-max / cable DID)

    const float LIM_MARGIN_KW = 2.0f;    // provisional: ignore near-equal caps as noise
    const float COLD_BATT_C   = 25.0f;   // provisional: car-limit below this reads as cold-battery derate

    if (ph.cap_car_seen && ph.cap_station_seen) {
        if (ph.cap_car_min < ph.cap_station_max - LIM_MARGIN_KW) {
            ph.limiting_side  = LIM_CAR;
            ph.limiting_value = ph.cap_car_min;
            ph.cold_battery   = (ph.temp_seen && ph.temp_min < COLD_BATT_C);
        } else {
            ph.limiting_side  = LIM_STATION;
            ph.limiting_value = ph.cap_station_max;
        }
    } else if (ph.cap_station_seen) {
        ph.limiting_side  = LIM_STATION;   // station cap seen, car never bound
        ph.limiting_value = ph.cap_station_max;
    } else if (ph.cap_car_seen) {
        ph.limiting_side  = LIM_CAR;       // car cap seen, station unknown
        ph.limiting_value = ph.cap_car_min;
        ph.cold_battery   = (ph.temp_seen && ph.temp_min < COLD_BATT_C);
    }
    // else: neither seen -> remains LIM_UNKNOWN

    if (ph.limiting_side != LIM_UNKNOWN)
        ESP_LOGI(TAG, "Phase %d limited by %s (%.1f kW)%s", m_charge_session.cur + 1,
                 LimSideLabel(ph.limiting_side), ph.limiting_value,
                 ph.cold_battery ? " [cold battery]" : "");
}

// INC-1: close the open phase on CHARGE_WAIT entry (or defensively at report time).
// No-op if no phase is open. Energy is the kWh-meter delta since open; outcome latches 0x1688.
void OvmsVehicleToyotaETNGA::CloseChargePhase()
{
    if (m_charge_session.cur < 0)
        return;
    ChargeSessionState::ChargePhase& ph = m_charge_session.phases[m_charge_session.cur];
    ph.end_monotonic = StandardMetrics.ms_m_monotonic->AsInt();
    ph.end_soc       = (int) StandardMetrics.ms_v_bat_soc->AsFloat();
    ph.energy_kwh    = StandardMetrics.ms_v_charge_kwh->AsFloat() - ph.kwh_at_open;
    if (ph.energy_kwh < 0.0f) ph.energy_kwh = 0.0f;
    ph.outcome       = m_v_charge_outcome->AsInt();
    ClassifyLimitingSide();   // INC-2: attribute the limiting side while cur still points at this phase
    ESP_LOGD(TAG, "Charge phase %d closed (%.2f kWh, %d%%->%d%%)",
             m_charge_session.cur + 1, ph.energy_kwh, ph.start_soc, ph.end_soc);
    m_charge_session.cur = -1;
}

// Live aggregation while charging: peak power, battery-temp range, ambient range,
// delivered-Ah, per-tick CSV row, and downsampled SVG buffer.
// Called every tick from HandleChargeAcState / HandleChargeDcState.
void OvmsVehicleToyotaETNGA::UpdateChargeSessionStats()
{
    if (!m_charge_session.in_session)
        return;

    int now = StandardMetrics.ms_m_monotonic->AsInt();

    // #138: if the CAN poll path has gone quiet (e.g. car locked → OBD gateway isolated from the
    // OBC), the charge metrics are frozen. Suspend per-sample logging + accumulation so we don't
    // record/integrate stale values; resume automatically when polls return.
    const int CHARGE_STALE_SECS = 3;   // charge poll path runs at 1s; 3 missed = clearly stale but quick
    if (m_last_poll_monotonic == 0 || now - m_last_poll_monotonic > CHARGE_STALE_SECS) {
        // Display honesty: zero the live power/current so the app/server/CSV don't show a frozen value.
        // (Does not touch the energy integrals; v.c.kwh recomputes from pack V×I on the next reply.)
        StandardMetrics.ms_v_charge_power->SetValue(0);
        StandardMetrics.ms_v_bat_power->SetValue(0);
        StandardMetrics.ms_v_bat_current->SetValue(0);
        // Keep the dt baselines current so resume doesn't bridge the gap into delivered_ah / station_kwh / SVG.
        m_charge_session.last_sample_monotonic = now;
        m_charge_session.last_svg_monotonic = now;
        return;   // skip peak/temp/ambient, station_kwh/delivered_ah, CSV row, SVG sample
    }

    float p = StandardMetrics.ms_v_charge_power->AsFloat();
    if (p > m_charge_session.peak_power)
        m_charge_session.peak_power = p;

    float t = StandardMetrics.ms_v_bat_temp->AsFloat();
    if (!m_charge_session.temp_seen) {
        m_charge_session.temp_min = m_charge_session.temp_max = t;
        m_charge_session.temp_seen = true;
    } else if (t < m_charge_session.temp_min) m_charge_session.temp_min = t;
    else if (t > m_charge_session.temp_max) m_charge_session.temp_max = t;

    // Only fold in ambient when a fresh reading exists (env temp is undefined when parked
    // until the in-charge 0x1F46 poll answers) — prevents a bogus 0 C dragging the range.
    if (StandardMetrics.ms_v_env_temp->IsDefined()) {
        float amb = StandardMetrics.ms_v_env_temp->AsFloat();
        if (!m_charge_session.amb_seen) {
            m_charge_session.amb_min = m_charge_session.amb_max = amb;
            m_charge_session.amb_seen = true;
        } else if (amb < m_charge_session.amb_min) m_charge_session.amb_min = amb;
        else if (amb > m_charge_session.amb_max) m_charge_session.amb_max = amb;
    }

    // INC-1: mirror peak power + battery-temp range into the open phase (additive to the
    // session-level aggregates above). cur < 0 between phases (WAIT) — skip then.
    if (m_charge_session.cur >= 0) {
        ChargeSessionState::ChargePhase& ph = m_charge_session.phases[m_charge_session.cur];
        if (p > ph.peak_power) ph.peak_power = p;
        if (!ph.temp_seen) { ph.temp_min = ph.temp_max = t; ph.temp_seen = true; }
        else if (t < ph.temp_min) ph.temp_min = t;
        else if (t > ph.temp_max) ph.temp_max = t;
        // INC-2: track the DC limiting caps. Station advertised max (0x166A) is DC-only and
        // reads ~0 on AC; car permission (0x16A1) is forward-filled (inactive sentinel skipped
        // upstream). Only meaningful while is_dc — AC classification is deferred.
        if (ph.is_dc) {
            float sta = m_v_charge_sta_max_p->AsFloat();        // 0x166A advertised station max kW
            if (sta > 0.1f) {
                if (!ph.cap_station_seen || sta > ph.cap_station_max) ph.cap_station_max = sta;
                ph.cap_station_seen = true;
            }
            float car = fabsf(m_v_charge_perm->AsFloat());      // 0x16A1 magnitude = car charge limit kW
            if (car > 0.1f) {
                if (!ph.cap_car_seen || car < ph.cap_car_min) ph.cap_car_min = car;
                ph.cap_car_seen = true;
            }
        }
    }

    m_charge_session.is_dc = (static_cast<PollState>(m_poll_state) == PollState::CHARGE_DC);

    // Delivered-Ah (charge-side coulomb): integrate pack current over dt.
    if (m_charge_session.last_sample_monotonic != 0) {
        int dt = now - m_charge_session.last_sample_monotonic;
        if (dt > 0 && dt <= 10) {   // ignore gaps (pause / lock-isolation / sleep) so delivered_ah stays accurate
            m_charge_session.delivered_ah += fabsf(StandardMetrics.ms_v_bat_current->AsFloat()) * (dt / 3600.0f);
            if (m_charge_session.cur >= 0)
                m_charge_session.phases[m_charge_session.cur].delivered_ah +=
                    fabsf(StandardMetrics.ms_v_bat_current->AsFloat()) * (dt / 3600.0f);
            float pv = StandardMetrics.ms_v_charge_voltage->AsFloat();
            float pa = StandardMetrics.ms_v_charge_current->AsFloat();
            float skw = m_charge_session.is_dc ? (pv * pa / 1000.0f) : m_v_charge_grid_power->AsFloat();
            m_charge_session.station_kwh += skw * (dt / 3600.0f);
        }
    }
    m_charge_session.last_sample_monotonic = now;

    // Stream a CSV row every tick (~1 s while charging).
    AppendChargeCsvRow();

    // Feed the downsampled SVG buffer at svg_interval_s.
    if (m_charge_session.last_svg_monotonic == 0 ||
        now - m_charge_session.last_svg_monotonic >= m_charge_session.svg_interval_s) {
        ChargeSessionState::Sample s;
        s.t_s = now - m_charge_session.start_monotonic;
        s.kw  = p;
        s.soc = (int) StandardMetrics.ms_v_bat_soc->AsFloat();
        s.sta_max  = m_v_charge_sta_max_p->AsFloat();        // station-offered max kW (0x166A, DC only; 0 on AC)
        s.car_perm = fabsf(m_v_charge_perm->AsFloat());      // car-permitted kW (0x16A1 is signed: |.| is the charge limit)
        s.station_kw = m_charge_session.is_dc
            ? (StandardMetrics.ms_v_charge_voltage->AsFloat() * StandardMetrics.ms_v_charge_current->AsFloat() / 1000.0f)
            : m_v_charge_grid_power->AsFloat();
        s.hvac_kw = m_v_env_hvac_power->AsFloat();
        m_charge_session.svg.push_back(s);
        m_charge_session.last_svg_monotonic = now;
        // Cap ~300 points: when exceeded, double the interval and decimate (keep every other).
        if (m_charge_session.svg.size() > 300) {
            std::vector<ChargeSessionState::Sample> dec;
            for (size_t i = 0; i < m_charge_session.svg.size(); i += 2)
                dec.push_back(m_charge_session.svg[i]);
            m_charge_session.svg.swap(dec);
            m_charge_session.svg_interval_s *= 2;
        }
    }
}

void OvmsVehicleToyotaETNGA::AppendChargeCsvRow()
{
    if (m_charge_session.base.empty())
        return;

    if (!m_charge_session.csv_started) {
        // Column semantics:
        //   car's asks      : car_perm_kw (0x16A1), car_target_a (0x166D) — both on the OBC / Plug-In Control ECU (0x745)
        //   station's caps  : station_max_kw/_a/_v
        //   actuals         : station_kw (from EVSE), battery_kw (into battery), hvac_kw (into cabin),
        //                     plus raw station_grid_kw / station_present_v / station_present_a
        //   obc_kw          : diagnostic — raw 0x10D4 (under-reads on DC, issue #109)
        //   pack temps      : batt_temp_c is the MEAN of all sensors; batt_tmin_c/_tmax_c are
        //                     the coolest/hottest. The BMS derates on the hottest sensor, so a
        //                     charging curve must be keyed to batt_tmax_c, not the mean (#155).
        // New columns are APPENDED — never inserted. Nothing in-tree parses these files, but
        // external decoders read them positionally, and `phase` was already prepended once.
        m_charge_session.csv_buf =
            "phase,elapsed_s,soc_pct,bms_soc_pct,station_kw,battery_kw,hvac_kw,pack_v,pack_a,"
            "batt_temp_c,ambient_c,state,station_max_kw,station_max_a,station_max_v,"
            "car_perm_kw,car_target_a,station_grid_kw,station_present_v,station_present_a,obc_kw,"
            "batt_tmin_c,batt_tmax_c,delivered_ah\n";
        m_charge_session.csv_started = true;
    }

    int now = StandardMetrics.ms_m_monotonic->AsInt();
    int elapsed = now - m_charge_session.start_monotonic;
    // Ambient is undefined until the first in-charge 0x1F46 reply (~30 s in): write an
    // empty field rather than 0.0, which is indistinguishable from a real 0 C reading.
    char amb[16] = "";
    if (StandardMetrics.ms_v_env_temp->IsDefined())
        snprintf(amb, sizeof(amb), "%.1f", StandardMetrics.ms_v_env_temp->AsFloat());
    char row[416];
    float present_v = StandardMetrics.ms_v_charge_voltage->AsFloat();
    float present_a = StandardMetrics.ms_v_charge_current->AsFloat();
    float grid_kw   = m_v_charge_grid_power->AsFloat();
    float station_kw = m_charge_session.is_dc ? (present_v * present_a / 1000.0f) : grid_kw;
    int phase_no = (m_charge_session.cur >= 0) ? (m_charge_session.cur + 1) : 0;
    snprintf(row, sizeof(row),
        "%d,%d,%.0f,%.1f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%s,%s,"
        "%.2f,%.0f,%.0f,%.2f,%.0f,%.3f,%.0f,%.0f,%.3f,%.1f,%.1f,%.4f\n",
        phase_no,
        elapsed,
        StandardMetrics.ms_v_bat_soc->AsFloat(),
        m_v_bat_soc_bms->AsFloat(),
        station_kw,
        StandardMetrics.ms_v_charge_power->AsFloat(),   // battery_kw (corrected, pack V×I)
        m_v_env_hvac_power->AsFloat(),                   // hvac_kw
        StandardMetrics.ms_v_bat_voltage->AsFloat(),
        StandardMetrics.ms_v_bat_current->AsFloat(),
        StandardMetrics.ms_v_bat_temp->AsFloat(),
        amb,
        (m_charge_session.is_dc ? "DC" : "AC"),
        m_v_charge_sta_max_p->AsFloat(), m_v_charge_sta_max_i->AsFloat(), m_v_charge_sta_max_v->AsFloat(),
        m_v_charge_perm->AsFloat(), m_v_charge_tgti->AsFloat(),
        grid_kw, present_v, present_a,
        m_charge_obc_kw,
        // Populated by BmsSetCellTemperature on every 0x1814 reply, so no new poll is
        // needed. Both read 0 until the first reply of the session arrives.
        StandardMetrics.ms_v_bat_pack_tmin->AsFloat(),
        StandardMetrics.ms_v_bat_pack_tmax->AsFloat(),
        // The module's own coulomb integral for the session; also what the report's
        // implied-capacity estimate is derived from.
        m_charge_session.delivered_ah);
    m_charge_session.csv_buf += row;

    // Flush at most every 30 s (or ~4 KB): a 1 Hz open/append/close per row would put
    // thousands of write cycles on the internal flash when /store is the report target.
    if (m_charge_session.last_csv_flush == 0)
        m_charge_session.last_csv_flush = now;
    if (now - m_charge_session.last_csv_flush >= 30 || m_charge_session.csv_buf.size() >= 4096)
        FlushChargeCsv();
}

// Write the buffered CSV rows to <base>.csv via the async I/O worker.
// FIFO order guarantees the first (truncate) write lands before later appends.
void OvmsVehicleToyotaETNGA::FlushChargeCsv()
{
    if (m_charge_session.csv_buf.empty() || m_charge_session.base.empty())
        return;
    // Hand the buffered rows to the async writer instead of writing on the Events task.
    // FIFO order guarantees the first (truncate) write lands before later appends.
    etnga_io_job* job = new etnga_io_job;
    job->op   = m_charge_session.csv_file_created ? etnga_io_job::WRITE_APPEND
                                                  : etnga_io_job::WRITE_TRUNCATE;
    job->path = m_charge_session.base + ".csv";
    job->data = m_charge_session.csv_buf;
    if (ChargeIoEnqueue(job)) {
        m_charge_session.csv_buf.clear();
        m_charge_session.csv_file_created = true;
        m_charge_session.last_csv_flush = StandardMetrics.ms_m_monotonic->AsInt();
    } else if (m_charge_session.csv_buf.size() > 16384) {
        // Worker persistently backlogged: bound the pending buffer (heap safety),
        // matching the prior synchronous behavior.
        ESP_LOGE(TAG, "Charge CSV buffer over limit — discarding buffered rows");
        m_charge_session.csv_buf.clear();
    }
}

// Render a self-contained inline SVG chart vs time, with auto-scaled axes:
//   - left axis: power (kW), auto-scaled to the max of battery/station/HVAC series
//   - right axis: SOC, fixed 0-100 %
//   - traces: battery power (blue), station power (orange), HVAC power (purple),
//     car-permitted limit (green, the 0x16A1 taper curve), and SOC overlay.
//     All powers plot as magnitudes (0x16A1 is signed: |.| is the charge limit).
// Streams into the caller's std::ostream incrementally rather than building its own buffer.
// The report is rendered into an in-RAM ostringstream (GenerateChargeReport) and handed to the
// async I/O worker to write off the Events task; the full ~25-30 KB report string therefore lives
// only for the brief enqueue-to-write window (once per charge session), not on the Events task.
void OvmsVehicleToyotaETNGA::RenderPowerSvg(std::ostream& out)
{
    const std::vector<ChargeSessionState::Sample>& s = m_charge_session.svg;
    if (s.size() < 2) {
        out << "<p>(not enough samples for a chart)</p>";
        return;
    }

    float pmax = 1.0f;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i].kw > pmax) pmax = s[i].kw;
        if (s[i].station_kw > pmax) pmax = s[i].station_kw;
        if (s[i].hvac_kw > pmax) pmax = s[i].hvac_kw;
    }
    const float pstep = (pmax > 60.0f) ? 25.0f : (pmax > 12.0f ? 10.0f : 2.0f);
    pmax = pstep * ceilf(pmax / pstep);

    const int W = 640, H = 260, PADL = 44, PADB = 28, PADT = 12, PADR = 44;
    const int PW = W - PADL - PADR, PH = H - PADT - PADB;
    int tmax = s.back().t_s > 0 ? s.back().t_s : 1;

    char b[256];
    snprintf(b, sizeof(b), "<svg viewBox=\"0 0 %d %d\" style=\"width:100%%;max-width:%dpx;height:auto\" xmlns=\"http://www.w3.org/2000/svg\">\n", W, H, W);
    out << b;
    out << "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>\n";

    // Horizontal gridlines + left power-axis (kW) labels.
    for (float kw = 0.0f; kw <= pmax + 0.01f; kw += pstep) {
        float y = PADT + (float)PH * (1.0f - kw / pmax);
        snprintf(b, sizeof(b), "<line x1=\"%d\" y1=\"%.1f\" x2=\"%d\" y2=\"%.1f\" stroke=\"#eee\"/>\n", PADL, y, W-PADR, y); out << b;
        snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%.1f\" font-size=\"10\" fill=\"#06c\" text-anchor=\"end\">%g</text>\n", PADL-3, y+3, kw); out << b;
    }
    // Right SOC-axis (%) labels, fixed 0-100.
    for (int soc = 0; soc <= 100; soc += 25) {
        float y = PADT + (float)PH * (1.0f - soc / 100.0f);
        snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%.1f\" font-size=\"10\" fill=\"#39c\">%d</text>\n", W-PADR+3, y+3, soc); out << b;
    }
    // Axis lines (left power, right SOC, bottom time).
    snprintf(b, sizeof(b), "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ccc\"/>\n", PADL, PADT, PADL, H-PADB); out << b;
    snprintf(b, sizeof(b), "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ccc\"/>\n", W-PADR, PADT, W-PADR, H-PADB); out << b;
    snprintf(b, sizeof(b), "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ccc\"/>\n", PADL, H-PADB, W-PADR, H-PADB); out << b;

    // INC-1: phase-boundary markers (vertical dashed lines at each phase start/end),
    // mapped from monotonic seconds onto the same time axis the traces use.
    for (size_t i = 0; i < m_charge_session.phases.size(); i++) {
        const ChargeSessionState::ChargePhase& ph = m_charge_session.phases[i];
        int bounds[2] = { ph.start_monotonic - m_charge_session.start_monotonic,
                          ph.end_monotonic   - m_charge_session.start_monotonic };
        for (int k = 0; k < 2; k++) {
            int ts = bounds[k];
            if (ts <= 0 || ts > tmax) continue;   // off-chart / unset end
            float x = PADL + (float)PW * ts / tmax;
            snprintf(b, sizeof(b),
                "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"#bbb\" "
                "stroke-width=\"0.7\" stroke-dasharray=\"2 2\"/>\n", x, PADT, x, H - PADB);
            out << b;
        }
    }

    // #138 follow-up: render each trace with breaks at time gaps. When the sample time jumps by
    // more than ~2 sample intervals — a CAN-stale window (logging suspended) or an inter-phase WAIT
    // pause — start a new polyline segment instead of drawing a straight line across the missing
    // data, so the chart shows an honest gap. Each trace clamps its value to [0, scale] and maps to y.
    const int svg_iv = m_charge_session.svg_interval_s > 0 ? m_charge_session.svg_interval_s : 20;
    const int gap_threshold = 2 * svg_iv;
    auto emitTrace = [&](const char* poly, float scale, float (*pick)(const ChargeSessionState::Sample&)) {
        out << poly << " points=\"";
        int prev = -1; char bb[32];
        for (size_t i = 0; i < s.size(); i++) {
            if (prev >= 0 && s[i].t_s - prev > gap_threshold)   // gap -> break the line
                out << "\"/>\n" << poly << " points=\"";
            prev = s[i].t_s;
            float x = PADL + (float)PW * s[i].t_s / tmax;
            float v = pick(s[i]); if (v < 0) v = 0; if (v > scale) v = scale;
            float y = PADT + (float)PH * (1.0f - v / scale);
            snprintf(bb, sizeof(bb), "%.1f,%.1f ", x, y); out << bb;
        }
        out << "\"/>\n";
    };
    // SOC overlay (right axis, 0-100), car-permitted limit, station power, HVAC power, delivered (primary).
    emitTrace("<polyline fill=\"none\" stroke=\"#39c\" stroke-width=\"1\" stroke-dasharray=\"1 3\"", 100.0f,
              [](const ChargeSessionState::Sample& smp)->float { return (float)smp.soc; });
    emitTrace("<polyline fill=\"none\" stroke=\"#0a0\" stroke-width=\"1\" stroke-dasharray=\"5 3\"", pmax,
              [](const ChargeSessionState::Sample& smp)->float { return smp.car_perm; });
    emitTrace("<polyline fill=\"none\" stroke=\"#e80\" stroke-width=\"1.5\"", pmax,
              [](const ChargeSessionState::Sample& smp)->float { return smp.station_kw; });
    emitTrace("<polyline fill=\"none\" stroke=\"#a0a\" stroke-width=\"1.5\"", pmax,
              [](const ChargeSessionState::Sample& smp)->float { return smp.hvac_kw; });
    emitTrace("<polyline fill=\"none\" stroke=\"#06c\" stroke-width=\"2\"", pmax,
              [](const ChargeSessionState::Sample& smp)->float { return smp.kw; });

    // Legend (bottom).
    int ly = H - 6;
    snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%d\" font-size=\"10\" fill=\"#06c\">battery kW</text>\n", PADL+6, ly); out << b;
    snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%d\" font-size=\"10\" fill=\"#e80\">station kW</text>\n", PADL+86, ly); out << b;
    snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%d\" font-size=\"10\" fill=\"#a0a\">HVAC kW</text>\n", PADL+166, ly); out << b;
    snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%d\" font-size=\"10\" fill=\"#0a0\">car permitted</text>\n", PADL+236, ly); out << b;
    snprintf(b, sizeof(b), "<text x=\"%d\" y=\"%d\" font-size=\"10\" fill=\"#39c\">SOC %%</text>\n", PADL+330, ly); out << b;
    out << "</svg>\n";
}

// Events-task: write the report and reset the session. Used both for the immediate close
// path and the deferred (wait-for-dump) path.
void OvmsVehicleToyotaETNGA::FinalizeChargeSession()
{
    GenerateChargeReport();
    m_charge_session = ChargeSessionState{};
}

void OvmsVehicleToyotaETNGA::GenerateChargeReport()
{
    if (m_charge_session.report_written)
        return;                 // defensive idempotency
    CloseChargePhase();         // close any still-open phase (direct AC/DC->AWAKE safety net)
    const float energy_kwh = StandardMetrics.ms_v_charge_kwh->AsFloat();
    bool have_dump = (m_dump_phase_idx >= 0) || (m_dump_remaining.load() > 0);
    if ((energy_kwh < 0.05f || m_charge_session.base.empty()) && !have_dump) {
        ESP_LOGD(TAG, "Charge report skipped (%.3f kWh)", energy_kwh);
        if (m_charge_session.csv_file_created) {  // remove the streamed stub CSV via the worker
            etnga_io_job* j = new etnga_io_job;
            j->op = etnga_io_job::UNLINK;
            j->path = m_charge_session.base + ".csv";
            ChargeIoEnqueue(j);
        }
        return;
    }
    FlushChargeCsv();   // complete the per-sample CSV before linking it from the report
    const float grid_kwh  = StandardMetrics.ms_v_charge_kwh_grid->AsFloat();
    const int   end_soc   = (int) StandardMetrics.ms_v_bat_soc->AsFloat();
    const int   start_soc = m_charge_session.start_soc;
    int dur = StandardMetrics.ms_m_monotonic->AsInt() - m_charge_session.start_monotonic;
    if (dur < 0) dur = 0;
    const float avg_kw = (dur > 0) ? energy_kwh / (dur / 3600.0f) : 0.0f;
    const int   outcome = m_v_charge_outcome->AsInt();
    const bool  time_ok = (m_charge_session.start_utc > 1000000000);

    char sbuf[40] = "(clock not synced)", ebuf[40] = "(clock not synced)";
    if (time_ok) {
        time_t st = (time_t) m_charge_session.start_utc, et = st + dur; struct tm tmv;
        gmtime_r(&st, &tmv); strftime(sbuf, sizeof(sbuf), "%Y-%m-%d %H:%M:%S UTC", &tmv);
        gmtime_r(&et, &tmv); strftime(ebuf, sizeof(ebuf), "%Y-%m-%d %H:%M:%S UTC", &tmv);
    }
    int dh = dur/3600, dm = (dur%3600)/60, ds = dur%60;

    std::ostringstream f;   // render into RAM; the async worker writes the file off the Events task

    const char* vn = MyVehicleFactory.ActiveVehicleName();
    std::string vname = (vn && *vn) ? vn : "Toyota e-TNGA";

    char b[96];
    f << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      << "<title>" << vname << " charge report " << sbuf << "</title>\n"
      << "<style>body{font:14px/1.4 system-ui,sans-serif;margin:1rem;max-width:46rem}"
      << "h1{font-size:1.3rem}h2{font-size:1.05rem;margin-top:1.3rem}"
      << "dl{display:grid;grid-template-columns:max-content 1fr;gap:.2rem .8rem}dt{font-weight:600}"
      << "table{border-collapse:collapse}td,th{border:1px solid #ddd;padding:.15rem .4rem;font-size:13px}"
      << ".est{color:#555}.note{color:#888;font-size:12px;margin-top:1.2rem}</style></head><body>\n"
      << "<h1>" << vname << " — charging session report</h1>\n";

    f << "<h2>Summary</h2>\n<dl>\n"
      << "<dt>Plug-in</dt><dd>" << sbuf << "</dd>\n<dt>Unplug</dt><dd>" << ebuf << "</dd>\n";
    snprintf(b, sizeof(b), "%dh %02dm %02ds", dh, dm, ds);
    f << "<dt>Duration</dt><dd>" << b << "</dd>\n";
    if (m_charge_session.has_loc) {
        std::string locname = LookupLocationName(m_charge_session.start_lat, m_charge_session.start_lon);
        snprintf(b, sizeof(b), "%.5f, %.5f", m_charge_session.start_lat, m_charge_session.start_lon);
        f << "<dt>Location</dt><dd>";
        if (!locname.empty()) f << locname << " &mdash; ";
        f << b
          << " (<a target=\"_blank\" href=\"https://www.openstreetmap.org/?mlat="
          << m_charge_session.start_lat << "&mlon=" << m_charge_session.start_lon
          << "#map=17/" << m_charge_session.start_lat << "/" << m_charge_session.start_lon << "\">map</a>)</dd>\n";
    }
    if (m_charge_session.amb_seen) {
        // Follow the user's configured temperature unit (°C or °F).
        metric_unit_t tu = OvmsMetricGetUserUnit(GrpTemp, Celcius);
        const char* tlabel = OvmsMetricUnitLabel(tu);
        float amb_lo = UnitConvert(Celcius, tu, m_charge_session.amb_min);
        float amb_hi = UnitConvert(Celcius, tu, m_charge_session.amb_max);
        snprintf(b, sizeof(b), "%.0f%s &rarr; %.0f%s", amb_lo, tlabel, amb_hi, tlabel);
        f << "<dt>Ambient</dt><dd>" << b << "</dd>\n";
    }
    f << "<dt>Type</dt><dd>" << (m_charge_session.is_dc ? "DC fast" : "AC") << "</dd>\n";
    {
        char pc[48];
        snprintf(pc, sizeof(pc), "%u", (unsigned) m_charge_session.phases.size());
        f << "<dt>Phases</dt><dd>" << pc << "</dd>\n";
    }
    snprintf(b, sizeof(b), "%d%% &rarr; %d%% (+%d%%)", start_soc, end_soc, start_soc>=0?end_soc-start_soc:0);
    f << "<dt>SOC</dt><dd>" << b << "</dd>\n";
    // "From grid" is an AC-charge concept (the 0x161D grid-input poll only answers on AC);
    // on DC the energy meter is the station's, so show delivered only.
    if (m_charge_session.is_dc)
        snprintf(b, sizeof(b), "%.2f kWh delivered", energy_kwh);
    else
        snprintf(b, sizeof(b), "%.2f kWh delivered, %.2f kWh from grid", energy_kwh, grid_kwh);
    f << "<dt>Energy</dt><dd>" << b << "</dd>\n";
    snprintf(b, sizeof(b), "%.1f kW peak / %.2f kW avg", m_charge_session.peak_power, avg_kw);
    f << "<dt>Power</dt><dd>" << b << "</dd>\n";
    {
        float e_batt = StandardMetrics.ms_v_charge_kwh->AsFloat();
        float e_hvac = m_v_env_hvac_kwh->AsFloat();
        float e_sta  = m_charge_session.station_kwh;
        float e_loss = e_sta - e_batt - e_hvac;
        int eff = (e_sta > 0.05f) ? (int)(100.0f * e_batt / e_sta + 0.5f) : 0;
        char acc[160];
        snprintf(acc, sizeof(acc),
            "<dt>Accounting</dt><dd>station %.2f kWh &rarr; battery %.2f kWh + HVAC %.2f kWh + losses %.2f kWh (%d%% to battery)</dd>\n",
            e_sta, e_batt, e_hvac, e_loss, eff);
        f << acc;
    }
    if (m_charge_session.temp_seen) {
        // Follow the user's configured temperature unit (°C or °F).
        metric_unit_t tu = OvmsMetricGetUserUnit(GrpTemp, Celcius);
        const char* tlabel = OvmsMetricUnitLabel(tu);
        float bat_lo = UnitConvert(Celcius, tu, m_charge_session.temp_min);
        float bat_hi = UnitConvert(Celcius, tu, m_charge_session.temp_max);
        snprintf(b, sizeof(b), "%.0f%s &rarr; %.0f%s", bat_lo, tlabel, bat_hi, tlabel);
        f << "<dt>Battery temp</dt><dd>" << b << "</dd>\n";
    }
    {
        const char* lbl = ChargeOutcomeLabel(outcome);
        if (lbl[0]) f << "<dt>Outcome</dt><dd>" << lbl << "</dd>\n";
        else { snprintf(b, sizeof(b), "0x%02X", outcome & 0xFF); f << "<dt>Outcome</dt><dd>" << b << " (raw)</dd>\n"; }
    }
    f << "</dl>\n";

    f << "<h2>Charging power</h2>\n";
    RenderPowerSvg(f);

    // INC-1: per-phase summary blocks (Option A — no per-phase sample tables; the full
    // timeline stays in the single CSV with its phase column).
    for (size_t i = 0; i < m_charge_session.phases.size(); i++) {
        const ChargeSessionState::ChargePhase& ph = m_charge_session.phases[i];
        int pdur = ph.end_monotonic - ph.start_monotonic; if (pdur < 0) pdur = 0;
        float pavg = (pdur > 0) ? ph.energy_kwh / (pdur / 3600.0f) : 0.0f;
        char pb[96];
        f << "<h2>Phase " << (i + 1) << " &mdash; " << (ph.is_dc ? "DC fast" : "AC") << "</h2>\n<dl>\n";
        if (ph.start_utc > 1000000000) {
            time_t st = (time_t) ph.start_utc, et = st + pdur; struct tm tmv;
            char ps[40], pe[40];
            gmtime_r(&st, &tmv); strftime(ps, sizeof(ps), "%H:%M:%S", &tmv);
            gmtime_r(&et, &tmv); strftime(pe, sizeof(pe), "%H:%M:%S", &tmv);
            snprintf(pb, sizeof(pb), "%s &rarr; %s UTC (%dh %02dm %02ds)", ps, pe,
                     pdur/3600, (pdur%3600)/60, pdur%60);
            f << "<dt>Active</dt><dd>" << pb << "</dd>\n";
        }
        snprintf(pb, sizeof(pb), "%d%% &rarr; %d%% (+%d%%)", ph.start_soc, ph.end_soc,
                 ph.start_soc >= 0 ? ph.end_soc - ph.start_soc : 0);
        f << "<dt>SOC</dt><dd>" << pb << "</dd>\n";
        snprintf(pb, sizeof(pb), "%.2f kWh; %.1f kW peak / %.2f kW avg",
                 ph.energy_kwh, ph.peak_power, pavg);
        f << "<dt>Energy</dt><dd>" << pb << "</dd>\n";
        if (ph.limiting_side != LIM_UNKNOWN) {
            snprintf(pb, sizeof(pb), "%s &mdash; %.1f kW%s", LimSideLabel(ph.limiting_side),
                     ph.limiting_value, ph.cold_battery ? " (cold battery)" : "");
            f << "<dt>Limiting</dt><dd>" << pb << "</dd>\n";
        }
        if (ph.temp_seen) {
            metric_unit_t tu = OvmsMetricGetUserUnit(GrpTemp, Celcius);
            const char* tlabel = OvmsMetricUnitLabel(tu);
            float lo = UnitConvert(Celcius, tu, ph.temp_min), hi = UnitConvert(Celcius, tu, ph.temp_max);
            snprintf(pb, sizeof(pb), "%.0f%s &rarr; %.0f%s", lo, tlabel, hi, tlabel);
            f << "<dt>Battery temp</dt><dd>" << pb << "</dd>\n";
        }
        {
            const char* lbl = ChargeOutcomeLabel(ph.outcome);
            if (lbl[0]) f << "<dt>Outcome</dt><dd>" << lbl << "</dd>\n";
            else if (ph.outcome >= 0) {
                snprintf(pb, sizeof(pb), "0x%02X (raw)", ph.outcome & 0xFF);
                f << "<dt>Outcome</dt><dd>" << pb << "</dd>\n";
            }
        }
        f << "</dl>\n";
    }

    // INC-3: diagnostic DID dump (fault-triggered; empty in normal sessions).
    {
        OvmsMutexLock lock(&m_dump_mutex);
        if (!m_dump_results.empty()) {
            f << "<h2>Diagnostic DID dump</h2>\n";
            char db[96];
            snprintf(db, sizeof(db), "Phase %d fault (outcome 0x%02X \"%s\") — OBC 0x745, raw UDS 0x22 responses.",
                     m_dump_phase_idx + 1, m_dump_outcome & 0xFF, ChargeOutcomeLabel(m_dump_outcome));
            f << "<p>" << db << "</p>\n";
            f << "<table><tr><th>DID</th><th>description</th><th>raw (hex)</th><th>decoded</th></tr>\n";
            for (std::map<uint16_t,std::string>::const_iterator it = m_dump_results.begin();
                 it != m_dump_results.end(); ++it) {
                char did[8];
                snprintf(did, sizeof(did), "0x%04X", it->first);
                f << "<tr><td>" << did << "</td><td>" << DumpDidName(it->first) << "</td><td>";
                if (it->second.empty()) {
                    f << "(no reply)";
                } else {
                    char hx[4];
                    for (size_t k = 0; k < it->second.size(); k++) {
                        snprintf(hx, sizeof(hx), "%02X ", (uint8_t) it->second[k]);
                        f << hx;
                    }
                }
                f << "</td><td>" << DumpDidDecode(it->first, it->second) << "</td></tr>\n";
            }
            f << "</table>\n";
        }
    }

    f << "<h2>Session events</h2>\n<table><tr><th>Time</th><th>Event</th></tr>\n";
    for (size_t i = 0; i < m_charge_session.events.size(); i++) {
        int rel = m_charge_session.events[i].first - m_charge_session.start_monotonic; if (rel < 0) rel = 0;
        snprintf(b, sizeof(b), "%d:%02d", rel/60, rel%60);
        f << "<tr><td>" << b << "</td><td>" << m_charge_session.events[i].second << "</td></tr>\n";
    }
    f << "</table>\n";

    f << "<h2 class=\"est\">Estimates</h2>\n<dl class=\"est\">\n";
    if (!m_charge_session.is_dc && grid_kwh > 0.01f) {   // efficiency needs grid energy (AC only)
        float loss = grid_kwh - energy_kwh;   // negative would imply delivered > grid (measurement skew)
        snprintf(b, sizeof(b), "%.0f%% (%.2f kWh %s)", energy_kwh/grid_kwh*100.0f, fabsf(loss),
            loss >= 0.0f ? "loss" : "gain?");
        f << "<dt>Charging efficiency</dt><dd>" << b << "</dd>\n";
    }
    // Implied full-charge capacity, computed against BMS SOC rather than display SOC.
    // Display SOC is the wrong denominator twice over: it is defined across the usable window
    // (display ~ 1.1145 x BMS - 4.673), so it understates capacity by ~10%; and it clamps at
    // exactly 100 once BMS >= 95.0%, so any session ending at a full charge has a truncated
    // span. BMS SOC is the true fraction of full charge, which makes this number directly
    // comparable to the 0x1D3E-derived v.b.cac shown alongside it.
    const float start_bms = m_charge_session.start_soc_bms;
    const float end_bms   = m_v_bat_soc_bms->AsFloat();
    if (start_bms >= 0.0f && end_bms > start_bms + 1.0f && m_charge_session.delivered_ah > 0.5f) {
        float cap = m_charge_session.delivered_ah / ((end_bms - start_bms) / 100.0f);
        float nominal = PackNominalAh();
        if (nominal > 0.0f)
            snprintf(b, sizeof(b), "%.0f Ah implied (~%.0f%% of %.0f Ah nominal), BMS SOC %.1f%% &rarr; %.1f%%",
                     cap, cap / nominal * 100.0f, nominal, start_bms, end_bms);
        else
            snprintf(b, sizeof(b), "%.0f Ah implied, BMS SOC %.1f%% &rarr; %.1f%% (no pack nominal configured)",
                     cap, start_bms, end_bms);
        f << "<dt>Implied capacity</dt><dd>" << b << "</dd>\n";

        float cac = StandardMetrics.ms_v_bat_cac->AsFloat();
        if (cac > 0.0f) {
            snprintf(b, sizeof(b), "%.2f Ah reported by the BMS (0x1D3E), %+.1f%% vs implied",
                     cac, (cac - cap) / cap * 100.0f);
            f << "<dt>Measured capacity</dt><dd>" << b << "</dd>\n";
        }
    }
    f << "</dl>\n";

    {
        std::string csv = m_charge_session.base + ".csv";
        std::string name = csv.substr(csv.find_last_of('/') + 1);
        // Include the storage location (the write side knows it) so WebChargeReport
        // serves the right file without a fallback scan over every location.
        const char* loc = (m_charge_session.base.compare(0, 4, "/sd/") == 0) ? "sd" : "store";
        f << "<p><a href=\"/xte/report?file=" << name << "&amp;loc=" << loc
          << "\">Download per-sample CSV</a></p>\n";
    }

    f << "<p class=\"note\">Generated on-module by OVMS (Toyota e-TNGA). Multi-phase; per-phase "
         "avg power is over each active interval. Estimates are provisional.</p>\n</body></html>\n";
    m_charge_session.report_written = true;
    // Hand the rendered HTML to the async writer; it writes the file and prunes off-thread.
    etnga_io_job* job = new etnga_io_job;
    job->op        = etnga_io_job::WRITE_TRUNCATE;
    job->path      = m_charge_session.base + ".html";
    job->data      = f.str();
    job->prune_dir = ChargeReportDir();
    ChargeIoEnqueue(job);
    ESP_LOGI(TAG, "Charge report queued: %s.html (%.2f kWh, %d%%->%d%%)",
        m_charge_session.base.c_str(), energy_kwh, start_soc, end_soc);
    // INC-3: dump state is session-scoped; clear after the report consumes it.
    {
        OvmsMutexLock lock(&m_dump_mutex);
        m_dump_results.clear();
    }
    m_dump_phase_idx = -1;
    m_dump_outcome = -1;
    m_dump_remaining = 0;
}
