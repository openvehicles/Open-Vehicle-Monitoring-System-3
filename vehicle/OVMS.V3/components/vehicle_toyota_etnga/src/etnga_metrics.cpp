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

#include "ovms_config.h"
#include "vehicle_toyota_etnga.h"
#include <algorithm>
#include <cmath>

void OvmsVehicleToyotaETNGA::InitializeMetrics()
{
    m_s_controlstate = MyMetrics.InitInt("xte.s.controlstate", SM_STALE_MIN);  // This variable stores the control state variable
    m_v_charge_pisw_raw = MyMetrics.InitInt("xte.v.c.piswraw", SM_STALE_MID);
    m_v_charge_ac_op    = MyMetrics.InitInt("xte.v.c.ac.opstatus", SM_STALE_MID);
    m_v_charge_hlc      = MyMetrics.InitInt("xte.v.c.hlcstate",  SM_STALE_MID);
    m_v_charge_perm = MyMetrics.InitFloat("xte.v.c.permpower", SM_STALE_MID, 0.0f, kW);
    m_v_charge_tgti = MyMetrics.InitFloat("xte.v.c.tgtcurrent", SM_STALE_MID, 0.0f, Amps);
    m_v_charge_sta_max_p = MyMetrics.InitFloat("xte.v.c.dc.maxpower", SM_STALE_MID, 0.0f, kW);
    m_v_charge_sta_max_i = MyMetrics.InitFloat("xte.v.c.dc.maxcurrent", SM_STALE_MID, 0.0f, Amps);
    m_v_charge_sta_max_v = MyMetrics.InitFloat("xte.v.c.dc.maxvoltage", SM_STALE_MID, 0.0f, Volts);
    m_v_charge_ac_tgt_p  = MyMetrics.InitFloat("xte.v.c.ac.tgtpower", SM_STALE_MID, 0.0f, kW);
    m_v_charge_chgr_op   = MyMetrics.InitInt("xte.v.c.chargerstate", SM_STALE_MID);                     // enum: 0=Stand by,1=Ready,2=Stop,3=RoB Stop
    m_v_charge_ac_ilim   = MyMetrics.InitFloat("xte.v.c.ac.ilimit", SM_STALE_MID, 0.0f, Amps);
    m_v_charge_out       = MyMetrics.InitFloat("xte.v.c.output", SM_STALE_MID, 0.0f, kW);
    m_v_charge_out_tgt   = MyMetrics.InitFloat("xte.v.c.outputtarget", SM_STALE_MID, 0.0f, kW);
    m_v_charge_ac_usable = MyMetrics.InitFloat("xte.v.c.ac.usable", SM_STALE_MID, 0.0f, kW);
    m_v_charge_myroom  = MyMetrics.InitBool("xte.v.c.myroom", SM_STALE_MID);
    m_v_charge_grid_power = MyMetrics.InitFloat("xte.v.c.gridpower", SM_STALE_MID, 0.0f, kW);
    m_v_env_hvac_power = MyMetrics.InitFloat("xte.v.e.hvac.power", SM_STALE_MID, 0.0f, kW);
    m_v_env_hvac_kwh   = MyMetrics.InitFloat("xte.v.e.hvac.kwh",   SM_STALE_MID, 0.0f, kWh);
    m_v_env_hvac_kwh_drive = MyMetrics.InitFloat("xte.v.e.hvac.kwh.drive", SM_STALE_MID, 0.0f, kWh);  // Per-trip driving cabin/HVAC energy
    m_v_charge_outcome = MyMetrics.InitInt("xte.v.c.outcome", SM_STALE_MID);
    m_v_charge_stopreq = MyMetrics.InitInt("xte.v.c.stoprequest", SM_STALE_MID);
    m_v_bat_soc_bms = MyMetrics.InitFloat("xte.v.b.soc.bms", SM_STALE_MID, 0.0f, Percentage, true);  // This variable stores the SOC as reported by the BMS
    m_v_bat_12v_voltage_pid = MyMetrics.InitFloat("xte.v.b.12v.voltage", SM_STALE_MID, 0.0f, Volts);  // 0x15EE EV-ECU PID 12V voltage (compare vs hardware v.b.12v.voltage)
    m_v_bat_12v_temp = MyMetrics.InitFloat("xte.v.b.12v.temp", SM_STALE_MID, 0.0f, Celcius);   // 0x15F8 12V aux battery temperature
    m_v_bat_12v_cac  = MyMetrics.InitFloat("xte.v.b.12v.cac",  SM_STALE_MID, 0.0f, AmpHours);  // 0x15E5 12V aux full-charge capacity
    m_v_bat_12v_charge_ah    = MyMetrics.InitFloat("xte.v.b.12v.charge.ah",    SM_STALE_MAX, 0.0f, AmpHours); // 0x15E8 lifetime charge integral
    m_v_bat_12v_discharge_ah = MyMetrics.InitFloat("xte.v.b.12v.discharge.ah", SM_STALE_MAX, 0.0f, AmpHours); // 0x15E8 lifetime discharge integral
    m_v_bat_12v_readyon_h    = MyMetrics.InitFloat("xte.v.b.12v.readyon.h",    SM_STALE_MAX, 0.0f, Hours);    // 0x15E8 integrated Ready-ON time
    m_v_e_awd = MyMetrics.InitInt("xte.v.e.awd", SM_STALE_MID);  // 0x1087 b2 AWD / X-MODE status (custom)

    // Set initial values for metrics
    SetAwake(false);
    SetReadyStatus(false);
    SetChargingDoorStatus(false);
    // Default gear to Park (0) at boot. v.e.gear is otherwise only set by the 0x1061 poll
    // (DRIVING-only) or on the DRIVING->AWAKE transition, so a module that boots straight into
    // charging (e.g. after a crash) would never set it — leaving the is_parked override
    // (gear === 0) with no value, so is_parked goes absent during charging. Park is the safe
    // boot default; if we actually boot mid-drive, the DRIVING-state 0x1061 poll corrects it
    // within ~1s.
    SetShiftPosition(0);
    // v.c.charging is a persistent metric (restored from RTC across soft reboots) and is
    // read by ABRP as the charging indicator. Boot is forced into the SLEEP poll state, so
    // initialize it false to match — otherwise a reboot mid-charge leaves it stale-true (and
    // a fresh power-up leaves it undefined) until the poller re-derives the real state.
    SetChargingStatus(false);
    // v.c.mode pairs with the above: the poller only sets it ("standard" AC / "performance"
    // DC, the ABRP is_dcfc indicator) while charging and clears it to "" on session end, so
    // the not-charging boot value is empty.
    StandardMetrics.ms_v_charge_mode->SetValue("");

    // Set poll state transition variables to shorter autostale than default
    // in case their ECUs go to sleep before the 'false' poll
    StandardMetrics.ms_v_door_chargeport->SetAutoStale(10);
}

void OvmsVehicleToyotaETNGA::ResetStaleMetrics() // Reset stale variables
{
    // Check to make sure the 'Control Mode' signal has been updated recently
    if (m_s_controlstate->IsStale() && m_s_controlstate->AsInt() != 0) {
        ESP_LOGD(TAG, "Control Mode is stale. Manually setting to off");
        SetControlMode(0);
    }

    // Check to make sure the 'charging door' signal has been updated recently.
    // The lid (0x1625) is only polled in AWAKE+DRIVING, so in the charge states it
    // reads stale even though the cable is physically holding the door open — don't
    // force it off there, or the UI/server shows the port closed mid-charge. The arm
    // logic in HandleAwakeState has already latched by then, so holding "open" is safe.
    // in_session extends that to the AWAKE pass-through of a CHARGE_WAIT sleep/resume:
    // the cable is still seated, but we're briefly below CHARGE_HANDSHAKE and resume goes
    // to CHARGE_WAIT (never back through handshake), so a clear here would stick for the
    // rest of the session. On unplug the session closes and the AWAKE lid poll re-reads
    // the real value within 10s, so this cannot strand the port "open".
    if (StandardMetrics.ms_v_door_chargeport->IsStale() && StandardMetrics.ms_v_door_chargeport->AsBool() &&
            !m_charge_session.in_session &&
            static_cast<PollState>(m_poll_state) < PollState::CHARGE_HANDSHAKE) {
        ESP_LOGD(TAG, "Charging Door is stale. Manually setting to off");
        SetChargingDoorStatus(false);
    }

    // Check to make sure the 'pilot' signal has been updated recently
    if (StandardMetrics.ms_v_charge_pilot->IsStale() && StandardMetrics.ms_v_charge_pilot->AsBool()) {
        ESP_LOGD(TAG, "Pilot Status is stale. Manually setting to off");
        SetPISWStatus(false);
    }

    // Check to make sure the 'power' has been updated recently
    if (StandardMetrics.ms_v_bat_power->IsStale() && StandardMetrics.ms_v_bat_power->AsFloat() != 0) {
        ESP_LOGD(TAG, "Power is stale. Manually setting to zero");
        SetBatteryCurrent(0);
        SetBatteryPower(0);
    }
}

// Data calculation functions
float OvmsVehicleToyotaETNGA::CalculateAmbientTemperature(const std::string& data)
{
    return static_cast<float>(GetRxBInt16(data, 0)) / 100.0f;
}

float OvmsVehicleToyotaETNGA::CalculateAmbientTemperatureEV(const std::string& data)
{
    return static_cast<float>(GetRxBInt8(data, 0)) - 40.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryChargingPower(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0)- 32768) / 100.0f ;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryCurrent(const std::string& data)
{
    return static_cast<float>(GetRxBInt16(data, 4)) / 10.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryPower(float voltage, float current)
{
    return voltage * current / 1000.0f;
}

float OvmsVehicleToyotaETNGA::CalculateBatterySOC(const std::string& data)
{
    return static_cast<float>(GetRxBByte(data, 0));
}

float OvmsVehicleToyotaETNGA::CalculateBatterySOCBMS(const std::string& data)
{
    return GetRxBByte(data, 0) * 100.0f / 255.0f;
}

int OvmsVehicleToyotaETNGA::PackModuleCount(int cellCount)
{
    // e-TNGA HV pack variants (documented 2026-06-23). 96-cell is on-vehicle validated;
    // 78/104 are reasoned from spec and UNVALIDATED (no such hardware available).
    switch (cellCount) {
        case 96:  return 4;   // 2022-24            : 4 modules x 24 cells
        case 78:  return 3;   // 2025/26 FWD        : 3 modules x 26 cells (UNVALIDATED)
        case 104: return 4;   // 2025/26 AWD/high   : 4 modules x 26 cells (UNVALIDATED)
        default:  return 0;   // unrecognised -> caller keeps last good arrangement
    }
}

float OvmsVehicleToyotaETNGA::PackNominalAh()
{
    // Explicit override wins: the only way a non-96-cell car gets v.b.soh at all.
    float cfg = MyConfig.GetParamValueFloat("xte", "bat.nominal.ah", 0.0f);
    if (cfg > 0.0f)
        return cfg;

    // Auto only for packs whose nominal is actually established. A wrong nominal is silent --
    // it yields a plausible percentage -- so an unknown variant gets nothing, not a guess.
    switch (m_pack_cells) {
        case 96:  return 201.1f;   // 2022-24: 71.4 kWh / 355 V
        default:  return 0.0f;     // 78 / 104 / unread: nominal not established
    }
}

float OvmsVehicleToyotaETNGA::PackNominalVolt()
{
    float cfg = MyConfig.GetParamValueFloat("xte", "bat.nominal.volt", 0.0f);
    if (cfg > 0.0f)
        return cfg;

    switch (m_pack_cells) {
        case 96:  return 355.0f;   // 2022-24 nominal pack voltage
        default:  return 0.0f;
    }
}

void OvmsVehicleToyotaETNGA::UpdateBatteryHealth(const std::vector<float>& caps)
{
    if (caps.size() < 8)
        return;

    // Mean of the 8 slots. The slots are ONE measurement plus fixed per-slot offsets, not 8
    // independent sensor chains: across a 7-week series the per-slot offsets held to
    // 0.003-0.034 Ah while the mean moved 0.644 Ah. So the mean is the low-noise estimator.
    float sum = 0.0f;
    for (size_t i = 0; i < 8; i++)
        sum += caps[i];
    float cac = sum / 8.0f;

    StandardMetrics.ms_v_bat_cac->SetValue(cac);

    float nominal = PackNominalAh();
    if (nominal <= 0.0f) {
        // No trustworthy denominator -> publish nothing rather than a plausible wrong number.
        // Only worth telling the user about once we know the pack is one we have no nominal
        // for; m_pack_cells == 0 just means 0x182E has not replied yet, which resolves itself.
        if (m_pack_cells != 0 && !m_nominal_warned) {
            m_nominal_warned = true;
            ESP_LOGI(TAG, "v.b.cac = %.2f Ah, but no pack nominal for a %d-cell pack: "
                          "v.b.soh and v.b.capacity suppressed. Set [xte] bat.nominal.ah "
                          "(and bat.nominal.volt) to enable them.", cac, m_pack_cells);
        }
        return;
    }

    float soh = cac / nominal * 100.0f;
    StandardMetrics.ms_v_bat_soh->SetValue(soh);

    // Published unclamped on purpose: an SOH over 100% is a configuration bug (a nominal that
    // does not match the pack), and clamping would hide exactly the misconfiguration worth
    // seeing. Warn once per nominal -- keying on the value rather than a bool means correcting
    // the config re-arms the warning without needing a reboot.
    if (soh > 102.0f && m_soh_warned_nominal != nominal) {
        m_soh_warned_nominal = nominal;
        ESP_LOGW(TAG, "v.b.soh = %.1f%% (%.2f Ah / %.1f Ah nominal). Over 100%% means the "
                      "nominal does not match this pack, not a healthy battery -- check "
                      "[xte] bat.nominal.ah against the actual variant.", soh, cac, nominal);
    }

    // v.b.capacity is defined as USABLE kWh, so scale the full-charge Ah by the usable window
    // before converting. Display SOC spans BMS [4.19, 93.92] (fitted over 8,901 unclamped
    // sample pairs), i.e. 89.73% of full charge is reachable by the driver.
    float volt = PackNominalVolt();
    if (volt > 0.0f)
        StandardMetrics.ms_v_bat_capacity->SetValue(cac * PACK_USABLE_FRACTION * volt / 1000.0f);
}

std::vector<float> OvmsVehicleToyotaETNGA::CalculateBatteryCellVoltages(const std::string& data)
{
    // 0x182E payload (Hybrid Battery ECU): N cells x uint16 BE; each LSB = 5/65535 V (~76 uV).
    // Cell count is taken from the reply length so differently-sized e-TNGA packs are
    // index-safe -- there is no cell-count PID.
    std::vector<float> voltages;
    voltages.reserve(data.size() / 2);

    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint16_t raw = GetRxBUint16(data, i);
        voltages.push_back(static_cast<float>(raw) * 5.0f / 65535.0f);
    }

    return voltages;
}

std::vector<float> OvmsVehicleToyotaETNGA::CalculateBatteryCapacityArray(const std::string& data)
{
    // 0x1D3E payload: 8 x uint16 BE, each LSB = 0.01 Ah.
    // The 8 elements are NOT 8 independent measurements: over a 7-week series the per-slot
    // offsets from each reading's own mean held to 0.003-0.034 Ah while the mean itself moved
    // 0.644 Ah, so this is one measurement plus fixed per-slot offsets. Kept as a vector for
    // the raw series (two slots do drift relative to the rest, the only per-stack aging signal
    // we have); UpdateBatteryHealth takes the mean for v.b.cac.
    std::vector<float> caps;
    caps.reserve(8);

    for (size_t i = 0; i < 16; i += 2) {
        uint16_t raw = GetRxBUint16(data, i);
        caps.push_back(static_cast<float>(raw) * 0.01f);
    }

    return caps;
}

std::vector<float> OvmsVehicleToyotaETNGA::CalculateBatteryTemperatures(const std::string& data)
{
    // 0x1814 payload (Hybrid Battery ECU): N sensors x int16 BE Q8.8, -50 C. Sensor count
    // from reply length (no sensor-count PID) so all e-TNGA pack variants are index-safe.
    std::vector<float> temperatures;
    temperatures.reserve(data.size() / 2);

    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        int16_t temperatureRaw = GetRxBInt16(data, i);
        float temperature = static_cast<float>(temperatureRaw) / 256.0f - 50.0f;
        temperatures.push_back(temperature);
    }

    return temperatures;
}

float OvmsVehicleToyotaETNGA::CalculateBatteryVoltage(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 2)) / 64.0f;
}

float OvmsVehicleToyotaETNGA::CalculateCabinTemperature(const std::string& data)
{
    return static_cast<float>(GetRxBInt16(data, 0)) / 100.0f;
}

float OvmsVehicleToyotaETNGA::CalculateChargerInputPower(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0)) * 5.0f / 1000.0f ;
}

bool OvmsVehicleToyotaETNGA::CalculateChargingDoorStatus(const std::string& data)
{
    return GetRxBBit(data, 1, 1);
}

int OvmsVehicleToyotaETNGA::CalculateAcOpStatus(const std::string& data) { return GetRxBByte(data, 0); }
void OvmsVehicleToyotaETNGA::SetAcOpStatus(int v)
{
    if (m_v_charge_ac_op->SetValue(v))
        ESP_LOGD(TAG, "AC Op Status changed: 0x%02X (%d)", v, v);
    // Log each new 0x1684 AC-Op state as a session event (mirrors SetHlcState for DC).
    // Stop/unknown return "" from AcOpStatusLabel and are skipped.
    if (m_charge_session.in_session && v != m_charge_session.last_acop) {
        m_charge_session.last_acop = v;
        const char* lbl = AcOpStatusLabel(v);
        if (lbl && lbl[0])
            LogChargeEvent(lbl);
    }
}
int OvmsVehicleToyotaETNGA::CalculateHlcState(const std::string& data) { return GetRxBByte(data, 0); }
void OvmsVehicleToyotaETNGA::SetHlcState(int v)
{
    if (m_v_charge_hlc->SetValue(v))
        ESP_LOGD(TAG, "HLC State changed: 0x%02X (%d)", v, v);
    // Log each new 0x1666 HLC (DC handshake) state as a session event. Unknown codes
    // return "" and are skipped (avoids logging a non-static formatted string).
    if (m_charge_session.in_session && v != m_charge_session.last_hlc) {
        m_charge_session.last_hlc = v;
        const char* lbl = HlcStateLabel(v);
        if (lbl && lbl[0])
            LogChargeEvent(lbl);
    }
}
// Unsigned byte: a signed read made codes >= 0x80 negative, silently failing the
// state machine's `pisw >= 0x02` cable-present checks on any extended/error code.
int OvmsVehicleToyotaETNGA::CalculatePISWRaw(const std::string& data) { return GetRxBByte(data, 0); }
void OvmsVehicleToyotaETNGA::SetPISWRaw(int v)
{
    if (m_v_charge_pisw_raw->SetValue(v))
        ESP_LOGD(TAG, "PISW Raw changed: 0x%02X (%d)", v, v);
}

float OvmsVehicleToyotaETNGA::CalculatePermissionPower(const std::string& data)
{
    // 0x16A1: two's-complement s16, x0.01 kW. Sentinel 0x8000 handled by caller (skip).
    return static_cast<float>(GetRxBInt16(data, 0)) / 100.0f;
}
void OvmsVehicleToyotaETNGA::SetPermissionPower(float kw)
{
    LogMetricChange(m_v_charge_perm, kw, "Min Permission Power", "kW");
    m_v_charge_perm->SetValue(kw);
}

float OvmsVehicleToyotaETNGA::CalculateTargetCurrent(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0));  // x1 A
}
void OvmsVehicleToyotaETNGA::SetTargetCurrent(float amps)
{
    LogMetricChange(m_v_charge_tgti, amps, "Target Charging Current", "A");
    m_v_charge_tgti->SetValue(amps);
}

float OvmsVehicleToyotaETNGA::CalculateStationVoltage(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0));  // 0x166B x1 V/LSB; idle=0, no sentinel
}
void OvmsVehicleToyotaETNGA::SetStationVoltage(float volts)
{
    LogMetricChange(StandardMetrics.ms_v_charge_voltage, volts, "Station Voltage", "V");
    StandardMetrics.ms_v_charge_voltage->SetValue(volts);
}

float OvmsVehicleToyotaETNGA::CalculateStationCurrent(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0));  // 0x166C x1 A/LSB; idle=0, no sentinel
}
void OvmsVehicleToyotaETNGA::SetStationCurrent(float amps)
{
    LogMetricChange(StandardMetrics.ms_v_charge_current, amps, "Station Current", "A");
    StandardMetrics.ms_v_charge_current->SetValue(amps);
}

float OvmsVehicleToyotaETNGA::CalculateStationMaxPower(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0)) / 100.0f;  // 0x166A x0.01 kW/LSB; idle=0 when no station
}
void OvmsVehicleToyotaETNGA::SetStationMaxPower(float kw)
{
    LogMetricChange(m_v_charge_sta_max_p, kw, "Station Max Power", "kW");
    m_v_charge_sta_max_p->SetValue(kw);
}

float OvmsVehicleToyotaETNGA::CalculateStationMaxCurrent(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0));  // 0x1679 x1 A/LSB; idle=0 when no station
}
void OvmsVehicleToyotaETNGA::SetStationMaxCurrent(float amps)
{
    LogMetricChange(m_v_charge_sta_max_i, amps, "Station Max Current", "A");
    m_v_charge_sta_max_i->SetValue(amps);
}

float OvmsVehicleToyotaETNGA::CalculateStationMaxVoltage(const std::string& data)
{
    return static_cast<float>(GetRxBUint16(data, 0));  // 0x1681 x1 V/LSB; idle=0 when no station
}
void OvmsVehicleToyotaETNGA::SetStationMaxVoltage(float volts)
{
    LogMetricChange(m_v_charge_sta_max_v, volts, "Station Max Voltage", "V");
    m_v_charge_sta_max_v->SetValue(volts);
}

float OvmsVehicleToyotaETNGA::CalculateAcTargetPower(const std::string& data)
{
    // 0x1619 b1-2: biased-32768 x0.01 kW (idle 0x8000 -> 0.00 kW)
    return static_cast<float>(GetRxBUint16(data, 0) - 32768) / 100.0f;
}
int   OvmsVehicleToyotaETNGA::CalculateChargerOpStatus(const std::string& data)      { return GetRxBByte(data, 2); }                    // 0x1619 b3 enum: 0=Stand by,1=Ready,2=Stop,3=RoB Stop
float OvmsVehicleToyotaETNGA::CalculateAcCurrentLimit(const std::string& data)       { return static_cast<float>(GetRxBUint16(data, 3) - 32768) / 100.0f; }  // 0x1619 b4-5: biased-32768 x0.01 A (mirrors AC target power)
float OvmsVehicleToyotaETNGA::CalculateChargerOutput(const std::string& data)        { return GetRxBUint16(data, 0) * 5.0f / 1000.0f; }   // 0x161E b1-2: x5/1000 kW (unit inferred by analogy to 0x161D grid power; confirm on sustained AC)
float OvmsVehicleToyotaETNGA::CalculateChargerOutputTarget(const std::string& data)  { return GetRxBUint16(data, 2) * 5.0f / 1000.0f; }   // 0x161E b3-4: x5/1000 kW (unit inferred; confirm on sustained AC)
float OvmsVehicleToyotaETNGA::CalculateAcUsable(const std::string& data)             { return GetRxBByte(data, 0) * 0.01f; }              // 0x1665: x0.01 kW (PluginCtrlAC dict factor; unit inferred)

void OvmsVehicleToyotaETNGA::SetAcTargetPower(float kw)
{
    LogMetricChange(m_v_charge_ac_tgt_p, kw, "AC Target Power", "kW");
    m_v_charge_ac_tgt_p->SetValue(kw);
}
void OvmsVehicleToyotaETNGA::SetChargerOpStatus(int v)
{
    if (m_v_charge_chgr_op->SetValue(v))
        ESP_LOGD(TAG, "Charger Op Status changed: 0x%02X (%d)", v, v);
}
void OvmsVehicleToyotaETNGA::SetAcCurrentLimit(float v)
{
    if (m_v_charge_ac_ilim->SetValue(v))
        ESP_LOGD(TAG, "AC Current Limit changed: %.2f A", v);
}
void OvmsVehicleToyotaETNGA::SetChargerOutput(float v)
{
    if (m_v_charge_out->SetValue(v))
        ESP_LOGD(TAG, "Charger Output changed: %.2f kW", v);
}
void OvmsVehicleToyotaETNGA::SetChargerOutputTarget(float v)
{
    if (m_v_charge_out_tgt->SetValue(v))
        ESP_LOGD(TAG, "Charger Output Target changed: %.2f kW", v);
}
void OvmsVehicleToyotaETNGA::SetAcUsable(float v)
{
    if (m_v_charge_ac_usable->SetValue(v))
        ESP_LOGD(TAG, "AC Usable Power changed: %.2f kW", v);
}

bool OvmsVehicleToyotaETNGA::CalculateMyRoom(const std::string& data)
{
    return GetRxBBit(data, 1, 0);   // 0x1692 byte 2 (idx 1), bit 0 = My Room active
}
void OvmsVehicleToyotaETNGA::SetMyRoom(bool active)
{
    LogMetricChange(m_v_charge_myroom, active, "My Room", active ? "Active" : "Inactive");
    m_v_charge_myroom->SetValue(active);
}

float OvmsVehicleToyotaETNGA::CalculateAcConsumption(const std::string& data)
{
    // 0x106E A/C consumption: u8 x0.05 kW/LSB at offset 0. Confirmed empirically — candump
    // 62106E0B => raw 0x0B at b[0]; pin #35 raw 0x10 = 0.80 kW. ("byte 1" in the Toyota doc is
    // 1-indexed = offset 0.) Host charge_csv_writer.py / analyze-myroom decoders agree (b[0]).
    return static_cast<float>(GetRxBByte(data, 0)) * 0.05f;
}
void OvmsVehicleToyotaETNGA::SetHvacPower(float kw)
{
    m_v_env_hvac_power->SetValue(kw);

    // My-Room cabin energy: 0x106E is a dedicated cabin-power channel, so cabin energy is its
    // direct time-integral over the My-Room-active interval — valid for both AC and DC (the old
    // 0x161D charger-input delta read 0 during DC). Integrate ONLY while My Room is active; this
    // same power metric is also polled while DRIVING (from 0x7D2), which must not feed cabin energy.
    if (m_v_charge_myroom->AsBool()) {
        m_v_env_hvac_kwh->SetValue(m_v_env_hvac_kwh->AsFloat() + kw * EnergyIntervalHours(lastHvacEnergyLogTime));
    } else {
        lastHvacEnergyLogTime = 0;   // reset so the next My-Room interval starts fresh
    }

    // A/C cooling active: 0x106E is the dedicated A/C consumption channel, so any draw => cooling on.
    StandardMetrics.ms_v_env_cooling->SetValue(kw > 0.0f);

    // Driving cabin/HVAC energy: per-trip time-integral of this power while DRIVING (reset in
    // NotifyVehicleOn). My-Room (charging) energy above and this are exclusive — different poll states.
    if (static_cast<PollState>(m_poll_state) == PollState::DRIVING) {
        m_v_env_hvac_kwh_drive->SetValue(m_v_env_hvac_kwh_drive->AsFloat() + kw * EnergyIntervalHours(lastHvacDriveEnergyLogTime));
    } else {
        lastHvacDriveEnergyLogTime = 0;   // reset so the next driving interval starts fresh
    }
}

int OvmsVehicleToyotaETNGA::CalculateChargeOutcome(const std::string& data)  { return GetRxBByte(data, 0); }  // 0x1688 26-state enum; RETAINED between sessions (does not reset on plug-in) — report must scope it per-session
bool OvmsVehicleToyotaETNGA::SetChargeOutcome(int v)
{
    bool changed = m_v_charge_outcome->SetValue(v);
    if (changed)
        ESP_LOGD(TAG, "Charge Outcome changed: 0x%02X (%d)", v, v);
    return changed;
}

int OvmsVehicleToyotaETNGA::CalculateChargeStopReq(const std::string& data)  { return GetRxBByte(data, 0); }  // 0x1667 enum (partial)
void OvmsVehicleToyotaETNGA::SetChargeStopReq(int v)
{
    if (m_v_charge_stopreq->SetValue(v))
        ESP_LOGD(TAG, "Charge Stop Request changed: 0x%02X (%d)", v, v);
}

// --- 2026-06-06 pins: EV ECU driver inputs + Brake/EPB ---

float OvmsVehicleToyotaETNGA::CalculateThrottle(const std::string& data)
{
    // 0x1060 b1: accelerator position, u8 x0.5 %/LSB (0x00-0xC8 -> 0-100%)
    return static_cast<float>(GetRxBByte(data, 0)) * 0.5f;
}
void OvmsVehicleToyotaETNGA::SetThrottle(float pct)
{
    LogMetricChange(StandardMetrics.ms_v_env_throttle, pct, "Throttle", "%");
    StandardMetrics.ms_v_env_throttle->SetValue(pct);
}

int OvmsVehicleToyotaETNGA::CalculateDriveMode(const std::string& data)
{
    return GetRxBByte(data, 0);   // 0x1004 b1: Drive Mode Select Status enum
}
void OvmsVehicleToyotaETNGA::SetDriveMode(int mode)
{
    // Platform enum (this BEV trim emits only 0/1/6 — Normal/Power/Eco):
    const char* label;
    switch (mode)
    {
        case 0:  label = "Normal";    break;
        case 1:  label = "Power";     break;
        case 2:  label = "Sport";     break;
        case 3:  label = "Sport S";   break;
        case 4:  label = "Sport S+";  break;
        case 5:  label = "Comfort";   break;
        case 6:  label = "Eco";       break;
        case 9:  label = "Range";     break;
        case 11: label = "Chauffeur"; break;
        default: label = "unknown";   break;
    }
    LogMetricChange(StandardMetrics.ms_v_env_drivemode, mode, "Drive Mode", label);
    StandardMetrics.ms_v_env_drivemode->SetValue(mode);
}

int OvmsVehicleToyotaETNGA::CalculateAwdMode(const std::string& data)
{
    return GetRxBByte(data, 1);   // 0x1087 b2: AWD / X-MODE status enum
}
void OvmsVehicleToyotaETNGA::SetAwdMode(int mode)
{
    const char* label;
    switch (mode)
    {
        case 0:  label = "Normal";                      break;
        case 2:  label = "Snow/Dirt";                   break;
        case 3:  label = "Deep Snow/Mud";               break;
        case 4:  label = "Snow/Dirt (Temp Cancel)";     break;
        case 5:  label = "Deep Snow/Mud (Temp Cancel)"; break;
        default: label = "unknown";                     break;
    }
    LogMetricChange(m_v_e_awd, mode, "AWD Mode", label);
    m_v_e_awd->SetValue(mode);
}

float OvmsVehicleToyotaETNGA::CalculateFootBrake(const std::string& data)
{
    // 0x104C b1: brake pedal stroke, u8 ~1 mm/LSB. Captured range 0 (rest) .. 67 (full press).
    // Map to 0-100% of observed full-press stroke for ms_v_env_footbrake (% semantics).
    const float kFootBrakeFullStrokeMm = 67.0f;
    float pct = static_cast<float>(GetRxBByte(data, 0)) * 100.0f / kFootBrakeFullStrokeMm;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}
void OvmsVehicleToyotaETNGA::SetFootBrake(float pct)
{
    LogMetricChange(StandardMetrics.ms_v_env_footbrake, pct, "Foot Brake", "%");
    StandardMetrics.ms_v_env_footbrake->SetValue(pct);
}

bool OvmsVehicleToyotaETNGA::CalculateParkBrake(const std::string& data)
{
    // 0x1045 b1 (RH actuator): 0x00=Park Applied, 0x02=Released, 0x03=Applying, 0x04=Releasing, 0x05=Completely Released.
    // OVMS handbrake bool: applied only when Park Applied (0x00). (0x01 Hold Applied is the separate brake-hold feature.)
    return GetRxBByte(data, 0) == 0x00;
}
void OvmsVehicleToyotaETNGA::SetParkBrake(bool applied)
{
    LogMetricChange(StandardMetrics.ms_v_env_handbrake, applied, "Park Brake", applied ? "Applied" : "Released");
    StandardMetrics.ms_v_env_handbrake->SetValue(applied);
}

// --- 12V auxiliary battery (EV ECU 0x7D2) ---

float OvmsVehicleToyotaETNGA::CalculateAux12vCurrent(const std::string& data)     { return (static_cast<int>(GetRxBUint16(data, 0)) - 0x8000) * 0.0038147f; } // 0x15F7 biased-32768 (raw-0x8000) x0.0038147 A, bidirectional
float OvmsVehicleToyotaETNGA::CalculateAux12vVoltage(const std::string& data)     { return GetRxBUint16(data, 0) * 5.0f / 4096.0f; }                        // 0x15EE u16 x5/4096 V
float OvmsVehicleToyotaETNGA::CalculateAux12vTemperature(const std::string& data) { return static_cast<float>(GetRxBUint16(data, 0) - 400) * 0.1f; }          // 0x15F8 (raw-400) x0.1 C
float OvmsVehicleToyotaETNGA::CalculateAux12vFullCharge(const std::string& data)  { return GetRxBByte(data, 0) * 0.5f; }                                     // 0x15E5 u8 x0.5 Ah

void OvmsVehicleToyotaETNGA::DecodeAux12vIntegrators(const std::string& data)
{
    // 0x15E8 (EV ECU 0x7D2), 17-byte aux-battery cluster (1-indexed per solterra-can doc):
    //   bytes 1-4   charging integrated current    u32 BE x0.1 Ah
    //   bytes 5-8   discharging integrated current  u32 BE x0.1 Ah
    //   bytes 11-12 Integrated Ready ON Time        u16 BE hours
    if (data.size() < 12)
        return;
    m_v_bat_12v_charge_ah->SetValue(static_cast<float>(GetRxBUint32(data, 0)) / 10.0f);
    m_v_bat_12v_discharge_ah->SetValue(static_cast<float>(GetRxBUint32(data, 4)) / 10.0f);
    m_v_bat_12v_readyon_h->SetValue(static_cast<float>(GetRxBUint16(data, 10)));
}

// Aux-12V charge detection comes from the rail voltage (v.b.12v.voltage — the module's own ADC),
// NOT from the CAN-sourced DC-DC current: the current tapers below any sane threshold while the car
// is still running and the rail is still held at float, and it is unavailable entirely if the bus
// drops. A resting lead-acid aux never exceeds ~13V, so a rail above that means the DC-DC is holding
// it up. Hysteresis stops the flag chattering as the rail decays after shutdown.
// The rail voltage alone cannot tell "charging" from "resting" on this vehicle, so it is only one
// term of a union (see issue #153). Measured on the car: the DC-DC floats the aux battery at just
// ~12.85V during an HV charge (below OFF), it dips to ~12.86V mid-drive, and a healthy resting
// battery sits at 12.6-12.8V -- the bands overlap, so no pair of thresholds separates them.
// The rail term is kept because it is the only CAN-independent one: it alone must carry a CAN
// outage, which is exactly what the old CAN-derived rule failed to do (#147).
static const float AUX_12V_CHARGING_ON_V  = 13.2f;  // rail above resting -> DC-DC is charging
static const float AUX_12V_CHARGING_OFF_V = 12.9f;  // rail back at rest  -> not charging

void OvmsVehicleToyotaETNGA::UpdateCharging12v()
{
    float v = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    if (v > 0.0f) {  // 0 = ADC not ready yet: hold the latch rather than clearing it
        if (v > AUX_12V_CHARGING_ON_V)
            m_rail_charging12v = true;
        else if (v < AUX_12V_CHARGING_OFF_V)
            m_rail_charging12v = false;
    }

    // CHARGE_WAIT and CHARGE_HANDSHAKE are deliberately excluded: the DC-DC is off in those states
    // and the module drains the aux battery (the rail measurably declines from plug-in until the
    // charge actually starts), so calling them "charging" would mask that.
    bool hv_charging = (static_cast<PollState>(m_poll_state) == PollState::CHARGE_AC ||
                        static_cast<PollState>(m_poll_state) == PollState::CHARGE_DC);

    StandardMetrics.ms_v_env_charging12v->SetValue(
        m_rail_charging12v || StandardMetrics.ms_v_env_on->AsBool() || hv_charging);
}

void OvmsVehicleToyotaETNGA::SetAux12vCurrent(float v)
{
    StandardMetrics.ms_v_bat_12v_current->SetValue(v);
    // Any valid EV-ECU 12V reply means the aux system is energized.
    StandardMetrics.ms_v_env_aux12v->SetValue(true);
}
void OvmsVehicleToyotaETNGA::SetAux12vVoltage(float v)     { m_v_bat_12v_voltage_pid->SetValue(v); }
void OvmsVehicleToyotaETNGA::SetAux12vTemperature(float v) { m_v_bat_12v_temp->SetValue(v); }
void OvmsVehicleToyotaETNGA::SetAux12vFullCharge(float v)  { m_v_bat_12v_cac->SetValue(v); }

int OvmsVehicleToyotaETNGA::CalculateControlMode(const std::string& data)
{
    // 0x00 = None
    // 0x01 = Driving
    // 0x03 = Charging / Discharging Mode
    return GetRxBInt8(data, 0);
}

int OvmsVehicleToyotaETNGA::CalculateChargeType(const std::string& data)
{
    return GetRxBInt8(data, 0);
}

float OvmsVehicleToyotaETNGA::CalculateHVACSetpoint(const std::string& data)
{
    // Unsigned byte: the raw value is an enumerated code (0..255); a signed read sent
    // codes >= 0x80 into the wrong piecewise branch as negatives.
    int rawValue = GetRxBByte(data, 0);
    
    // Calculate the HVAC setpoint based on the piecewise function
    if (rawValue == 0) {
        return 0.0f;
    } else if (rawValue < 28) {
        return (rawValue / 2.0f) + 15.5f;
    } else if (rawValue < 55) {
        return (rawValue - 1) * 5.0f / 9.0f;
    } else if (rawValue == 55) {
        return 100.0f;
    } else if (rawValue < 100) {
        return (rawValue / 2.0f) - 34.0f;
    } else {
        return (rawValue - 74) * 5.0f / 9.0f;
    }
}

float OvmsVehicleToyotaETNGA::CalculateOdometer(const std::string& data)
{
    return static_cast<float>(GetRxBUint32(data, 0)) / 10.0f;
}

bool OvmsVehicleToyotaETNGA::CalculatePISWStatus(const std::string& data)
{
    // 0x00 = None
    // 0x03 = Charging connector connected
    return GetRxBByte(data, 0) != 0;
}

bool OvmsVehicleToyotaETNGA::CalculateReadyStatus(const std::string& data)
{
    return GetRxBBit(data, 1, 0);
}

int OvmsVehicleToyotaETNGA::CalculateShiftPosition(const std::string& data)
{
    return static_cast<int>(GetRxBInt8(data, 0));
}

float OvmsVehicleToyotaETNGA::CalculateVehicleSpeed(const std::string& data)
{
    // 0x1F0D: u8, 1 km/h/LSB (max 210) — must decode unsigned or speeds above
    // 127 km/h wrap negative.
    return static_cast<float>(GetRxBByte(data, 0));
}

// Metric setter functions
void OvmsVehicleToyotaETNGA::SetAmbientTemperature(float temperature)
{
    if (temperature == -40.0f)
    {
        // Ignore -40 temperature
        ESP_LOGI(TAG, "Ignoring invalid temperature value: %f", temperature);
    }
    else
    {
        LogMetricChange(StandardMetrics.ms_v_env_temp, temperature, "Ambient Temperature", "°C");
        StandardMetrics.ms_v_env_temp->SetValue(temperature);
    }
}

void OvmsVehicleToyotaETNGA::SetAwake(bool awake)
{
    StandardMetrics.ms_v_env_awake->SetValue(awake);
}

// CAN2 bus-liveness. Mirrors the old v.e.awake ~120s auto-stale window (SM_STALE_MID)
// but is kept internal, so v.e.awake can carry the standard "switched on" meaning.
// Uses the ms_m_monotonic seconds clock (same clock as the CHARGE_WAIT drain timer).
static const uint32_t BUS_STALE_SECS = 120;

bool OvmsVehicleToyotaETNGA::IsBusAlive() const
{
    if (m_last_can2_time == 0)
        return false;
    uint32_t now = (uint32_t) StandardMetrics.ms_m_monotonic->AsInt();
    return (now - m_last_can2_time) <= BUS_STALE_SECS;
}

// Hours elapsed since the last sample on an energy-integrator channel; updates the
// channel timestamp. The first sample of an interval (lastSampleTime == 0) counts as
// 1 s. Gaps over 60 s (charge pause, OBC lock-isolation, sleep) also count as 1 s —
// integrating the current power across a long gap would massively over-count energy
// (same rationale as the delivered_ah dt guard in UpdateChargeSessionStats).
float OvmsVehicleToyotaETNGA::EnergyIntervalHours(uint32_t& lastSampleTime)
{
    uint32_t now = esp_log_timestamp();
    float hours = 1.0f / 3600.0f;
    if (lastSampleTime != 0)
    {
        float h = static_cast<float>(now - lastSampleTime) / (1000.0f * 3600.0f);
        if (h <= 60.0f / 3600.0f)
            hours = h;
    }
    lastSampleTime = now;
    return hours;
}

void OvmsVehicleToyotaETNGA::SetBatteryChargingPower(float obc_power)
{
    // obc_power is the OBC 0x10D4 decode. It under-reports on DC (issue #109: ~0.73x of
    // actual; correct on AC), so keep it only as a diagnostic and derive the authoritative
    // charge power from pack V×I, which is accurate on both AC and DC.
    m_charge_obc_kw = obc_power;

    float v = StandardMetrics.ms_v_bat_voltage->AsFloat();
    float i = StandardMetrics.ms_v_bat_current->AsFloat();
    float power = -(v * i) / 1000.0f;   // pack current is negative while charging -> positive charge power
    if (power < 0.0f) power = 0.0f;

    ESP_LOGD(TAG, "Charge power pack V×I=%.2f kW (obc 0x10D4=%.2f kW)", power, obc_power);
    StandardMetrics.ms_v_charge_power->SetValue(power);

    const float energy = power * EnergyIntervalHours(lastChargerEnergyLogTime);
    StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);
}

void OvmsVehicleToyotaETNGA::SetBatteryCurrent(float current)
{
    LogMetricChange(StandardMetrics.ms_v_bat_current, current, "Battery Current", "A");
    StandardMetrics.ms_v_bat_current->SetValue(current);
}

void OvmsVehicleToyotaETNGA::SetBatteryPower(float power)
{
    LogMetricChange(StandardMetrics.ms_v_bat_power, power, "Battery Power", "kW");
    StandardMetrics.ms_v_bat_power->SetValue(power);

    const float hoursSinceLastUpdate = EnergyIntervalHours(lastBatteryEnergyLogTime);
    const float energy = power * hoursSinceLastUpdate;

    ESP_LOGD(TAG, "Battery Energy: %f kWh", energy);

    // Coulomb count (Ah) over the same interval. Current shares the power sign (+ = discharge);
    // ms_v_bat_current was just set from the same 0x1F9A reply (see IncomingHybridControlSystem).
    const float charge = StandardMetrics.ms_v_bat_current->AsFloat() * hoursSinceLastUpdate;

    // Only log energy/coulomb use/recovery while driving. The per-trip metrics reset in
    // NotifyVehicleOn; the *_total accumulators are persistent (lifetime) and never reset here.
    if (static_cast<PollState>(m_poll_state) == PollState::DRIVING)
    {
        if (power > 0)
        {
            StandardMetrics.ms_v_bat_energy_used->SetValue(
                StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);
            StandardMetrics.ms_v_bat_energy_used_total->SetValue(
                StandardMetrics.ms_v_bat_energy_used_total->AsFloat() + energy);
            StandardMetrics.ms_v_bat_coulomb_used->SetValue(
                StandardMetrics.ms_v_bat_coulomb_used->AsFloat() + charge);
            StandardMetrics.ms_v_bat_coulomb_used_total->SetValue(
                StandardMetrics.ms_v_bat_coulomb_used_total->AsFloat() + charge);
        }
        else if (power < 0)
        {
            StandardMetrics.ms_v_bat_energy_recd->SetValue(
                StandardMetrics.ms_v_bat_energy_recd->AsFloat() - energy);
            StandardMetrics.ms_v_bat_energy_recd_total->SetValue(
                StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() - energy);
            StandardMetrics.ms_v_bat_coulomb_recd->SetValue(
                StandardMetrics.ms_v_bat_coulomb_recd->AsFloat() - charge);
            StandardMetrics.ms_v_bat_coulomb_recd_total->SetValue(
                StandardMetrics.ms_v_bat_coulomb_recd_total->AsFloat() - charge);
        }
    }
}

void OvmsVehicleToyotaETNGA::SetBatterySOC(float soc)
{
    if (soc == 0.0 && StandardMetrics.ms_v_bat_soc->AsFloat() > 1.0)
    {
        // Ignore zero SOC if previousSOC was greater than 1
        ESP_LOGI(TAG, "Ignoring invalid SOC value: %f", soc);
    }
    else
    {
        LogMetricChange(StandardMetrics.ms_v_bat_soc, soc, "SOC", "%");
        StandardMetrics.ms_v_bat_soc->SetValue(soc);
    }
}

void OvmsVehicleToyotaETNGA::SetBatterySOCBMS(float soc)
{
    if (soc == 0.0 && m_v_bat_soc_bms->AsFloat() > 1.0)
    {
        // Ignore zero SOC if previousSOC was greater than 1
        ESP_LOGI(TAG, "Ignoring invalid SOC value: %f", soc);
    }
    else
    {
        LogMetricChange(m_v_bat_soc_bms, soc, "BMS SOC", "%");
        m_v_bat_soc_bms->SetValue(soc);
    }
}

void OvmsVehicleToyotaETNGA::SetBatteryCellVoltages(const std::vector<float>& voltages)
{
    // The e-TNGA base always declares an arrangement (bootstrap in the ctor), so per-cell
    // data routes through the BMS API. Re-arrange to the actual pack on each reply.
    if (BmsGetCellArangementVoltage() > 0) {
        int cells = static_cast<int>(voltages.size());
        int modules = PackModuleCount(cells);
        if (modules == 0) {
            ESP_LOGW(TAG, "0x182E: unrecognised cell count %d, keeping current BMS arrangement", cells);
            return;
        }
        m_bms_modules = modules;
        m_pack_cells = cells;   // observed count: the pack-nominal resolver keys off this
        // Align the arrangement total + per-module grouping with the detected pack.
        // No-op (returns false) when both already match, e.g. the 96-cell pack (4x24).
        BmsCheckChangeCellArrangementVoltage(cells, cells / modules);
        BmsRestartCellVoltages();
        for (size_t i = 0; i < voltages.size(); ++i) {
            BmsSetCellVoltage(static_cast<int>(i), voltages[i]);
        }
    } else {
        StandardMetrics.ms_v_bat_cell_voltage->SetValue(voltages);
    }
}

void OvmsVehicleToyotaETNGA::SetBatteryCellVoltageStatistics(const std::vector<float>& voltages)
{
    if (voltages.empty())
        return;

    if (BmsGetCellArangementVoltage() > 0) {
        // BmsSetCellVoltage already populated pack vmin/vmax/vavg/vstddev.
        return;
    }

    // No BMS arrangement: compute and publish pack stats manually.
    float minVoltage = *std::min_element(voltages.begin(), voltages.end());
    float maxVoltage = *std::max_element(voltages.begin(), voltages.end());
    float sum = std::accumulate(voltages.begin(), voltages.end(), 0.0f);
    float averageVoltage = sum / voltages.size();
    float variance = 0.0f;
    for (float v : voltages) {
        variance += (v - averageVoltage) * (v - averageVoltage);
    }
    float standardDeviation = sqrt(variance / voltages.size());

    StandardMetrics.ms_v_bat_pack_vmin->SetValue(minVoltage);
    StandardMetrics.ms_v_bat_pack_vmax->SetValue(maxVoltage);
    StandardMetrics.ms_v_bat_pack_vavg->SetValue(averageVoltage);
    StandardMetrics.ms_v_bat_pack_vstddev->SetValue(standardDeviation);
}

void OvmsVehicleToyotaETNGA::SetBatteryTemperatures(const std::vector<float>& temperatures)
{
    // Group temperature sensors using the module count resolved from the cell-voltage
    // reply (m_bms_modules). If the sensor count does not divide evenly, fall back to a
    // single group so the cell data stays correct and only the display grouping degrades.
    if (BmsGetCellArangementTemperature() > 0) {
        int sensors = static_cast<int>(temperatures.size());
        if (sensors == 0) {
            return;
        }
        int perModule = (m_bms_modules > 0 && (sensors % m_bms_modules) == 0)
                        ? sensors / m_bms_modules
                        : sensors;
        if (perModule == sensors && m_bms_modules > 1) {
            ESP_LOGW(TAG, "0x1814: %d sensors not divisible by %d modules; using one group",
                     sensors, m_bms_modules);
        }
        BmsCheckChangeCellArrangementTemperature(sensors, perModule);
        BmsRestartCellTemperatures();
        for (size_t i = 0; i < temperatures.size(); ++i) {
            BmsSetCellTemperature(static_cast<int>(i), temperatures[i]);
        }
    } else {
        StandardMetrics.ms_v_bat_cell_temp->SetValue(temperatures);
    }
}

void OvmsVehicleToyotaETNGA::SetBatteryTemperatureStatistics(const std::vector<float>& temperatures)
{
    if (temperatures.empty())
        return;

    // Average is needed for ms_v_bat_temp regardless of code path.
    float sum = std::accumulate(temperatures.begin(), temperatures.end(), 0.0f);
    float averageTemperature = sum / temperatures.size();

    if (BmsGetCellArangementTemperature() == 0) {
        // No BMS arrangement: compute pack stats manually and publish them.
        float minTemperature = *std::min_element(temperatures.begin(), temperatures.end());
        float maxTemperature = *std::max_element(temperatures.begin(), temperatures.end());
        float variance = 0.0f;
        for (float temperature : temperatures) {
            variance += (temperature - averageTemperature) * (temperature - averageTemperature);
        }
        float standardDeviation = sqrt(variance / temperatures.size());

        StandardMetrics.ms_v_bat_pack_tmin->SetValue(minTemperature);
        StandardMetrics.ms_v_bat_pack_tmax->SetValue(maxTemperature);
        StandardMetrics.ms_v_bat_pack_tavg->SetValue(averageTemperature);
        StandardMetrics.ms_v_bat_pack_tstddev->SetValue(standardDeviation);
    }
    // When arrangement IS set, BmsSetCellTemperature already populated tmin/tmax/tavg/tstddev.

    // ms_v_bat_temp (the single representative pack temperature) isn't auto-set by either
    // path, so publish the average here.
    StandardMetrics.ms_v_bat_temp->SetValue(averageTemperature);
}

void OvmsVehicleToyotaETNGA::SetBatteryVoltage(float voltage)
{
    LogMetricChange(StandardMetrics.ms_v_bat_voltage, voltage, "Voltage", "V");
    StandardMetrics.ms_v_bat_voltage->SetValue(voltage);
}

void OvmsVehicleToyotaETNGA::SetChargingStatus(bool status)
{
    LogMetricChange(StandardMetrics.ms_v_charge_inprogress, status, "Charging Status",
        status ? "Charging" : "Not Charging");
    StandardMetrics.ms_v_charge_inprogress->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetChargeState(PollState state)
{
    std::string s;
    switch (state) {
        case PollState::CHARGE_HANDSHAKE: s = "prepared"; break;
        case PollState::CHARGE_WAIT:      s = "stopped";  break;
        case PollState::CHARGE_AC:
        case PollState::CHARGE_DC:        s = "charging"; break;
        default:                          s = ""; break;
    }
    LogMetricChange(StandardMetrics.ms_v_charge_state, s, "Charge State");
    StandardMetrics.ms_v_charge_state->SetValue(s);
}

void OvmsVehicleToyotaETNGA::SetCabinTemperature(float temperature)
{
    if (temperature == -40.0f)
    {
        // Ignore -40 temperature
        ESP_LOGI(TAG, "Ignoring invalid temperature value: %f", temperature);
    }
    else
    {
        LogMetricChange(StandardMetrics.ms_v_env_cabintemp, temperature, "Cabin Temperature", "°C");
        StandardMetrics.ms_v_env_cabintemp->SetValue(temperature);
    }
}

void OvmsVehicleToyotaETNGA::SetChargeType(int chargeType)
{
    std::string chargeTypeValue;

    // 0x161C "Charger Power Supply Voltage Type" enum (confirmed 2026-05-10):
    // 0 = None/unconnected, 1 = 100V Series, 2 = 200V Series. raw 0 must clear the
    // metric (no charge connected), NOT report "ccs" — that was the prior bug.
    // This DID encodes only the AC voltage type: a real DC fast-charge (2026-06-13) read 00
    // here right up to engage, and 0x161C isn't polled in CHARGE_DC. The DC connector ("ccs")
    // is therefore set from the charge state in TransitionToChargeDcState, not from this poll.
    switch (chargeType)
    {
        case 0x00:
            chargeTypeValue = "";  // None / unconnected
            break;
        case 0x01:
            chargeTypeValue = "type1";  // 100V Series
            break;
        case 0x02:
            chargeTypeValue = "type2";  // 200V Series
            break;
        default:
            chargeTypeValue = "unknown";
            break;
    }
    
    LogMetricChange(StandardMetrics.ms_v_charge_type, chargeTypeValue, "Charge Type");
    StandardMetrics.ms_v_charge_type->SetValue(chargeTypeValue);
}

void OvmsVehicleToyotaETNGA::SetChargerInputPower(float power)
{
    ESP_LOGD(TAG, "Charger Input Power: %f", power);
    m_v_charge_grid_power->SetValue(power);

    const float energy = power * EnergyIntervalHours(lastGridEnergyLogTime);

    ESP_LOGD(TAG, "Grid Energy: %f kWh", energy);

    StandardMetrics.ms_v_charge_kwh_grid->SetValue(StandardMetrics.ms_v_charge_kwh_grid->AsFloat() + energy);
    StandardMetrics.ms_v_charge_kwh_grid_total->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat() + energy);  // persistent lifetime grid energy
}

void OvmsVehicleToyotaETNGA::SetChargingDoorStatus(bool status)
{
    int newValue = status ? 1 : 0;

    LogMetricChange(StandardMetrics.ms_v_door_chargeport, newValue, "Charging Door Status",
        status ? "Open" : "Closed");

    StandardMetrics.ms_v_door_chargeport->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetControlMode(int controlMode)
{
    const char* controlModeLabel;

    switch (controlMode)
    {
        case 0x00:
            controlModeLabel = "None";
            break;
        case 0x01:
            controlModeLabel = "Driving";
            break;
        case 0x03:
            controlModeLabel = "Charging";
            break;
        default:
            controlModeLabel = "unknown";
            break;
    }

    LogMetricChange(m_s_controlstate, controlMode, "Control Mode", controlModeLabel);
    m_s_controlstate->SetValue(controlMode);
}

void OvmsVehicleToyotaETNGA::SetHVACSetpoint(float temperature)
{
    LogMetricChange(StandardMetrics.ms_v_env_cabinsetpoint, temperature, "HVAC Setpoint", "°C");
    StandardMetrics.ms_v_env_cabinsetpoint->SetValue(temperature);
}

void OvmsVehicleToyotaETNGA::SetOdometer(float odometer)
{
    LogMetricChange(StandardMetrics.ms_v_pos_odometer, odometer, "Odometer", "km");
    StandardMetrics.ms_v_pos_odometer->SetValue(odometer);  // Set the odometer metric

    if (!m_trip_start_valid)
    {
        // Seed the trip-start baseline on the first reading of a trip.
        // It is invalidated on transition to DRIVING (see TransitionToDrivingState).
        m_trip_start_odo = odometer;
        m_trip_start_valid = true;
    }

    // Update the trip odometer
    float tripOdometer = odometer - m_trip_start_odo;
    StandardMetrics.ms_v_pos_trip->SetValue(tripOdometer);
}

void OvmsVehicleToyotaETNGA::SetPISWStatus(bool status)
{
    LogMetricChange(StandardMetrics.ms_v_charge_pilot, status, "Pilot Status",
        status ? "Connected" : "Not Connected");
    StandardMetrics.ms_v_charge_pilot->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetPollState(int state)
{
    const char* CurrentPollStateText = ConvertPollStateToString(m_poll_state);
    const char* NextPollStateText = ConvertPollStateToString(state);

    ESP_LOGI(TAG, "Transitioning from the %s to the %s state", CurrentPollStateText, NextPollStateText);

    // v.e.awake reflects the standard "switched on" meaning: true only while actively
    // awake for a non-charge reason. Charge states poll with the car off -> awake stays false.
    SetAwake(state == PollState::AWAKE || state == PollState::DRIVING);

    PollSetState(state);
}

void OvmsVehicleToyotaETNGA::SetReadyStatus(bool status)
{
    LogMetricChange(StandardMetrics.ms_v_env_on, status, "Ready Status",
        status ? "Ready" : "Not Ready");
    StandardMetrics.ms_v_env_on->SetValue(status);
}

void OvmsVehicleToyotaETNGA::SetShiftPosition(int position)
{
    const char* shiftPositionText;
    int gear;

    switch (position) {
        case 2:
            shiftPositionText = "Reverse";
            gear = -1;
            break;
        case 4:
            shiftPositionText = "Neutral";
            gear = 0;
            break;
        case 0:
            shiftPositionText = "Park";
            gear = 0;
            break;
        case 6:
            shiftPositionText = "Drive";
            gear = 1;
            break;
        // No case 8 ("B"): the Solterra/bZ4X has no B range — Toyota's RM Electronic
        // Shift Lever data list states "This vehicle does not have the B (S) range",
        // and gear-byte0 has only ever been 0/2/4/6 across all captures. Regen is via
        // paddles (Solterra) / boost button (bZ4X), not a selector position. Any
        // unexpected value correctly falls through to Unknown below.
        default:
            shiftPositionText = "Unknown";
            gear = -999;
            break;
    }

    LogMetricChange(StandardMetrics.ms_v_env_gear, gear, "Gear", shiftPositionText);
    StandardMetrics.ms_v_env_gear->SetValue(gear);
}

void OvmsVehicleToyotaETNGA::SetVehicleSpeed(float speed)
{
    LogMetricChange(StandardMetrics.ms_v_pos_speed, speed, "Speed", "kph");
    StandardMetrics.ms_v_pos_speed->SetValue(speed);
}

void OvmsVehicleToyotaETNGA::SetVehicleVIN(std::string vin)
{
    ESP_LOGD(TAG, "VIN: %s", vin.c_str());
    StandardMetrics.ms_v_vin->SetValue(std::move(vin));
}

void OvmsVehicleToyotaETNGA::LogMetricChange(OvmsMetricBool* metric, bool newValue, const char* label, const char* valueLabel)
{
    if (metric->AsBool() != newValue)
    {
        ESP_LOGD(TAG, "%s changed: %s (%d)", label, valueLabel, newValue ? 1 : 0);
    }
    else
    {
        ESP_LOGV(TAG, "%s unchanged: %s (%d)", label, valueLabel, newValue ? 1 : 0);
    }
}

void OvmsVehicleToyotaETNGA::LogMetricChange(OvmsMetricFloat* metric, float newValue, const char* label, const char* units)
{
    if (metric->AsFloat() != newValue)
    {
        ESP_LOGD(TAG, "%s changed: %.2f%s%s", label, newValue, units[0] ? " " : "", units);
    }
    else
    {
        ESP_LOGV(TAG, "%s unchanged: %.2f%s%s", label, newValue, units[0] ? " " : "", units);
    }
}

void OvmsVehicleToyotaETNGA::LogMetricChange(OvmsMetricInt* metric, int newValue, const char* label, const char* valueLabel)
{
    if (metric->AsInt() != newValue)
    {
        ESP_LOGD(TAG, "%s changed: %s (%d)", label, valueLabel, newValue);
    }
    else
    {
        ESP_LOGV(TAG, "%s unchanged: %s (%d)", label, valueLabel, newValue);
    }
}

void OvmsVehicleToyotaETNGA::LogMetricChange(OvmsMetricString* metric, const std::string& newValue, const char* label)
{
    if (metric->AsString() != newValue)
    {
        ESP_LOGD(TAG, "%s changed: %s", label, newValue.c_str());
    }
    else
    {
        ESP_LOGV(TAG, "%s unchanged: %s", label, newValue.c_str());
    }
}