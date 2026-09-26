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
#include "vehicle_toyota_etnga.h"

void OvmsVehicleToyotaETNGA::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
{
    m_last_poll_monotonic = StandardMetrics.ms_m_monotonic->AsInt();   // #138: bus-alive timestamp for charge-stale detection

    // Check if this is the first frame of the multi-frame response
    if (job.mlframe == 0) {
        m_rxbuf.clear();
        m_rxbuf.reserve(length + job.mlremain);
    }

    // Append the data to the receive buffer
    m_rxbuf.append(reinterpret_cast<char*>(data), length);

    // Check if response is complete
    if (job.mlremain != 0)
        return;

    // Log the received response
    ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", job.pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());

    // Process based on m_poll_moduleid_low
    switch (job.moduleid_rec) {
        case AIR_CONDITIONER_RX:
            IncomingAirConditionerSystem(job.pid);
            break;

        case HYBRID_BATTERY_SYSTEM_RX:
            IncomingHybridBatterySystem(job.pid);
            break;

        case HYBRID_CONTROL_SYSTEM_RX:
            IncomingHybridControlSystem(job.pid);
            break;

        case PLUG_IN_CONTROL_SYSTEM_RX:
            IncomingPlugInControlSystem(job.pid);
            break;

        case HPCM_HYBRIDPTCTR_RX:
            IncomingHPCMHybridPtCtr(job.pid);
            break;

        case TPMS_GW_RX:
            IncomingTPMS(job.pid);
            break;

        case BRAKE_EPB_RX:
            IncomingBrakeEpb(job.pid);
            break;

        default:
            ESP_LOGW(TAG, "Unknown module: %03" PRIx32, job.moduleid_rec);
            return;
    }
}

void OvmsVehicleToyotaETNGA::IncomingPollError(const OvmsPoller::poll_job_t &job, int32_t code)
{
    // UDS negative responses and poll TX failures previously vanished silently.  Logged
    // for diagnosis only (OBC lock-isolation NRCs, gateway refusals, and TX failures
    // while the bus is wedged) — deliberately NO state/session action here; see the
    // lock-isolation notes in HandleChargeAcState/HandleChargeDcState.
    // A repeating identical error (e.g. one TX failure per poll on a wedged bus) logs
    // at WARN once, then at debug level until the error signature changes.
    // Non-overlapping fold: module low byte (bits 24-31), pid (8-23), code (0-7).
    uint32_t sig = ((job.moduleid_rec & 0xFF) << 24) ^ ((job.pid & 0xFFFF) << 8) ^ (code & 0xFF);
    if (sig != m_last_poll_error) {
        m_last_poll_error = sig;
        ESP_LOGW(TAG, "Poll error: module %03" PRIx32 " PID %04X code %d", job.moduleid_rec, job.pid, (int)code);
    } else {
        ESP_LOGD(TAG, "Poll error (repeat): module %03" PRIx32 " PID %04X code %d", job.moduleid_rec, job.pid, (int)code);
    }
}

void OvmsVehicleToyotaETNGA::IncomingAirConditionerSystem(uint16_t pid)
{
    switch (pid) {
        case PID_AMBIENT_TEMPERATURE: {
            float temperature = CalculateAmbientTemperature(m_rxbuf);
            SetAmbientTemperature(temperature);
            break;
        }

        case PID_CABIN_TEMPERATURE: {
            float temperature = CalculateCabinTemperature(m_rxbuf);
            SetCabinTemperature(temperature);
            break;
        }

        case PID_HVAC_SETPOINT: {
            float temperature = CalculateHVACSetpoint(m_rxbuf);
            SetHVACSetpoint(temperature);
            break;
        }

        case PID_HEATER_POWER: {
            // 0x1086 HV electric heater power (W, 2-byte cluster, split TBD): any draw => heating active.
            StandardMetrics.ms_v_env_heating->SetValue(GetRxBUint16(m_rxbuf, 0) > 0);
            break;
        }

        case PID_BLOWER_LEVEL: {
            // 0x2801 blower level (u8 1-7) -> cabin fan percentage (standard metric unit is %).
            int level = GetRxBByte(m_rxbuf, 0);
            StandardMetrics.ms_v_env_cabinfan->SetValue(level * 100 / 7);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGW(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHybridControlSystem(uint16_t pid)
{
    switch (pid) {
        case PID_ACTIVE_DIAGNOSTIC_SESSION: {
            break;
        }

        case PID_AMBIENT_TEMPERATURE_EV: {
            float temperature = CalculateAmbientTemperatureEV(m_rxbuf);
            SetAmbientTemperature(temperature);
            break;
        }

        case PID_BATTERY_VOLTAGE_AND_CURRENT: {
            if (m_rxbuf.size() < 6) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            float batVoltage = CalculateBatteryVoltage(m_rxbuf);
            float batCurrent = CalculateBatteryCurrent(m_rxbuf);
            float batPower = CalculateBatteryPower(batVoltage, batCurrent);

            SetBatteryVoltage(batVoltage);
            SetBatteryCurrent(batCurrent);
            SetBatteryPower(batPower);

            break;
        }

        case PID_READY_SIGNAL: {
            bool readyStatus = CalculateReadyStatus(m_rxbuf);
            SetReadyStatus(readyStatus);
            break;
        }

        case PID_SHIFT_POSITION: {
            int shiftPosition = CalculateShiftPosition(m_rxbuf);
            SetShiftPosition(shiftPosition);
            break;
        }

        case PID_ODOMETER: {
            if (m_rxbuf.size() < 4) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            float odometer = CalculateOdometer(m_rxbuf);
            SetOdometer(odometer);
            break;
        }

        case PID_VEHICLE_SPEED: {
            float speed = CalculateVehicleSpeed(m_rxbuf);
            SetVehicleSpeed(speed);
            break;
        }

        case PID_AC_CONSUMPTION: {
            // HVAC power while driving — 0x106E from the hybrid control ECU (0x7D2); the OBC
            // (0x745) only answers while charging. Same decode, routes to the unified metric.
            SetHvacPower(CalculateAcConsumption(m_rxbuf));
            break;
        }

        case PID_THROTTLE: {
            SetThrottle(CalculateThrottle(m_rxbuf));
            break;
        }

        case PID_DRIVE_MODE_SELECT: {
            SetDriveMode(CalculateDriveMode(m_rxbuf));
            break;
        }

        case PID_AWD_MODE: {
            SetAwdMode(CalculateAwdMode(m_rxbuf));
            break;
        }

        case PID_AUX_BATTERY_CURRENT: {
            if (m_rxbuf.size() < 2) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetAux12vCurrent(CalculateAux12vCurrent(m_rxbuf));
            break;
        }

        case PID_AUX_BATTERY_VOLTAGE: {
            if (m_rxbuf.size() < 2) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetAux12vVoltage(CalculateAux12vVoltage(m_rxbuf));
            break;
        }

        case PID_AUX_BATTERY_TEMP: {
            if (m_rxbuf.size() < 2) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetAux12vTemperature(CalculateAux12vTemperature(m_rxbuf));
            break;
        }

        case PID_AUX_BATTERY_FULL_CHARGE: {
            if (m_rxbuf.size() < 1) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetAux12vFullCharge(CalculateAux12vFullCharge(m_rxbuf));
            break;
        }

        case PID_AUX_BATTERY_INTEGRATORS: {
            DecodeAux12vIntegrators(m_rxbuf);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGW(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingBrakeEpb(uint16_t pid)
{
    switch (pid) {
        case PID_BRAKE_PEDAL_STROKE: {
            SetFootBrake(CalculateFootBrake(m_rxbuf));
            break;
        }

        case PID_EPB_STATUS: {
            SetParkBrake(CalculateParkBrake(m_rxbuf));
            break;
        }

        default:
            ESP_LOGW(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingPlugInControlSystem(uint16_t pid)
{
    switch (pid) {
        case PID_ACTIVE_DIAGNOSTIC_SESSION: {
            break;
        }

        case PID_BATTERY_SOC: {
            float SOC = CalculateBatterySOC(m_rxbuf);
            SetBatterySOC(SOC);
            break;
        }

        case PID_CHARGING_LID: {
            bool chargingDoorStatus = CalculateChargingDoorStatus(m_rxbuf);
            SetChargingDoorStatus(chargingDoorStatus);
            break;
        }

        case PID_CONTROL_SYSTEM_MODE: {
            int controlMode = CalculateControlMode(m_rxbuf);
            SetControlMode(controlMode);
            break;
        }

        case PID_PISW_STATUS: {
            bool PISWStatus = CalculatePISWStatus(m_rxbuf);
            SetPISWStatus(PISWStatus);
            SetPISWRaw(CalculatePISWRaw(m_rxbuf));
            break;
        }

        case PID_AC_CHARGING_OP_STATUS: {
            SetAcOpStatus(CalculateAcOpStatus(m_rxbuf));
            break;
        }

        case PID_HLC_STATE: {
            SetHlcState(CalculateHlcState(m_rxbuf));
            break;
        }

        case PID_BATTERY_CHARGING_POWER: {
            // Biased u16: a zero-filled short reply decodes as -327.68 kW and would be
            // integrated into ms_v_charge_kwh — guard before the gate.
            if (m_rxbuf.size() < 2) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            // Only valid during AC or DC charging
            if (StandardMetrics.ms_v_charge_inprogress->AsBool()) {
                float batteryChargingPower = CalculateBatteryChargingPower(m_rxbuf);
                SetBatteryChargingPower(batteryChargingPower);
            }
            break;
        }

        case PID_CHARGER_INPUT_POWER: {
            // Accumulates into the LIFETIME grid-energy total — guard against short replies.
            if (m_rxbuf.size() < 2) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            // Only valid during AC charging
            if (m_poll_state == PollState::CHARGE_AC) {
                float chargerInputPower = CalculateChargerInputPower(m_rxbuf);
                SetChargerInputPower(chargerInputPower);
            }
            break;
        }

        case PID_CHARGING_VOLTAGE_TYPE: {
            int chargeType = CalculateChargeType(m_rxbuf);
            SetChargeType(chargeType);
            break;
        }

        case PID_DC_CHARGER_PRESENT_CURRENT:
            SetStationCurrent(CalculateStationCurrent(m_rxbuf));
            break;

        case PID_DC_CHARGER_PRESENT_VOLTAGE:
            SetStationVoltage(CalculateStationVoltage(m_rxbuf));
            break;

        case PID_MIN_PERMISSION_POWER: {
            if (GetRxBUint16(m_rxbuf, 0) != 0x8000)  // 0x8000 = feature inactive; preserve forward-fill
                SetPermissionPower(CalculatePermissionPower(m_rxbuf));
            break;
        }

        case PID_TARGET_CHARGING_CURRENT: {
            SetTargetCurrent(CalculateTargetCurrent(m_rxbuf));
            break;
        }

        case PID_DC_CHARGER_MAX_POWER: {
            SetStationMaxPower(CalculateStationMaxPower(m_rxbuf));
            break;
        }
        case PID_DC_CHARGER_MAX_CURRENT: {
            SetStationMaxCurrent(CalculateStationMaxCurrent(m_rxbuf));
            break;
        }
        case PID_DC_CHARGER_MAX_VOLTAGE: {
            SetStationMaxVoltage(CalculateStationMaxVoltage(m_rxbuf));
            break;
        }

        case PID_CHARGER_STATE_CLUSTER: {
            if (m_rxbuf.size() < 5) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetAcTargetPower(CalculateAcTargetPower(m_rxbuf));
            SetChargerOpStatus(CalculateChargerOpStatus(m_rxbuf));
            SetAcCurrentLimit(CalculateAcCurrentLimit(m_rxbuf));
            break;
        }
        case PID_CHARGER_OUTPUT_POWER: {
            if (m_rxbuf.size() < 4) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            SetChargerOutput(CalculateChargerOutput(m_rxbuf));
            SetChargerOutputTarget(CalculateChargerOutputTarget(m_rxbuf));
            break;
        }
        case PID_AC_USABLE_POWER: {
            SetAcUsable(CalculateAcUsable(m_rxbuf));
            break;
        }

        case PID_MYROOM: {
            SetMyRoom(CalculateMyRoom(m_rxbuf));
            break;
        }
        case PID_AC_CONSUMPTION: {
            SetHvacPower(CalculateAcConsumption(m_rxbuf));
            break;
        }
        case PID_CHARGE_HISTORY: {
            int outcome_code = CalculateChargeOutcome(m_rxbuf);
            bool outcome_changed = SetChargeOutcome(outcome_code);
            // INC-3: flag a diagnostic dump on a genuine charge fault (consumed on CHARGE_WAIT entry).
            //
            // 0x1688 RETAINS the last stop reason across sessions and does not reset on plug-in, so
            // the raw code is NOT evidence of a live fault: on a scheduled (timer-delayed) charge the
            // module wakes, reads the retained code, and would fire a 30-DID burst at a perfectly
            // healthy car still waiting for its start time. Two gates are needed, and neither alone
            // is sufficient:
            //   - outcome_changed: only a transition INTO a fault code counts, not every re-read.
            //   - m_charge_engaged: the metric can start at 0 after a boot, which makes the first
            //     read of a retained fault code look like a genuine transition. Requiring that this
            //     session actually reached CHARGE_AC/CHARGE_DC rules that out. A real mid-charge
            //     abnormal stop always satisfies it, because charging was by definition running.
            if (outcome_changed && IsChargeFaultCode(outcome_code) && m_charge_engaged.load()) {
                m_charge_fault_pending = true;
                m_dump_trigger_outcome = outcome_code;                          // the code that IS the fault
                m_dump_trigger_phase   = (m_charge_session.cur >= 0)
                                         ? m_charge_session.cur
                                         : (int) m_charge_session.phases.size() - 1;  // phase whose fault flagged
            }
            break;
        }
        case PID_CHARGE_STOP_REQ: {
            SetChargeStopReq(CalculateChargeStopReq(m_rxbuf));
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGW(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHybridBatterySystem(uint16_t pid)
{
    switch (pid) {
        case PID_ACTIVE_DIAGNOSTIC_SESSION: {
            break;
        }

        case PID_BATTERY_SOC_BMS: {
            float SOC = CalculateBatterySOCBMS(m_rxbuf);
            SetBatterySOCBMS(SOC);
            break;
        }

        case PID_BATTERY_TEMPERATURES: {
            // Temperature sensor count has no whitelist (it varies with the pack and is
            // grouped via m_bms_modules); reject only an empty/odd reply.
            std::vector<float> temperatures = CalculateBatteryTemperatures(m_rxbuf);
            if (temperatures.empty() || (m_rxbuf.size() % 2) != 0) {
                ESP_LOGW(TAG, "0x1814: invalid reply (%d bytes), skipping BMS temperature update",
                         (int)m_rxbuf.size());
                break;
            }
            SetBatteryTemperatures(temperatures);
            SetBatteryTemperatureStatistics(temperatures);
            break;
        }

        case PID_BATTERY_CELL_VOLTAGES: {
            // Dispatch only happens on a COMPLETE ISOTP reply (mlremain==0), so the length
            // reflects the pack, not a truncation. Accept any recognised pack; reject the
            // rest (no fixed 192-byte floor -- that rejected the smaller 78-cell pack).
            std::vector<float> voltages = CalculateBatteryCellVoltages(m_rxbuf);
            if (voltages.empty() || PackModuleCount(static_cast<int>(voltages.size())) == 0) {
                ESP_LOGW(TAG, "0x182E: unrecognised reply (%d bytes), skipping BMS voltage update",
                         (int)m_rxbuf.size());
                break;
            }
            SetBatteryCellVoltages(voltages);
            SetBatteryCellVoltageStatistics(voltages);
            break;
        }

        case PID_BATTERY_CAPACITY: {
            if (m_rxbuf.size() < 16) { ESP_LOGW(TAG, "Short reply for PID %04X (%d bytes)", pid, (int)m_rxbuf.size()); break; }
            std::vector<float> caps = CalculateBatteryCapacityArray(m_rxbuf);
            UpdateBatteryHealth(caps);   // v.b.cac / v.b.soh / v.b.capacity derive from this DID only
            if (caps.size() >= 8)
                ESP_LOGD(TAG, "Battery capacity 0x1D3E (Ah): %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
                         caps[0], caps[1], caps[2], caps[3], caps[4], caps[5], caps[6], caps[7]);
            break;
        }

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGW(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::IncomingHPCMHybridPtCtr(uint16_t pid)
{
    switch (pid) {

        // Add more cases for other PIDs if needed

        default:
            // Handle unsupported PID
            ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
            break;
    }
}

void OvmsVehicleToyotaETNGA::RequestVIN()
{
    if (!StandardMetrics.ms_v_vin->AsString().empty())
        return;

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    auto entry = std::shared_ptr<OvmsPoller::OnceOffPoll>(
        new OvmsPoller::OnceOffPoll(
            std::bind(&OvmsVehicleToyotaETNGA::IncomingVINSuccess, this, _1, _2, _3, _4, _5, _6),
            std::bind(&OvmsVehicleToyotaETNGA::IncomingVINFail,    this, _1, _2, _3, _4, _5),
            HYBRID_CONTROL_SYSTEM_TX, HYBRID_CONTROL_SYSTEM_RX,
            VEHICLE_POLL_TYPE_READDATA, PID_VIN,
            ISOTP_STD, 0, /*retry_fail=*/3));
    // "!v." prefix => auto-removed on vehicle shutdown (poller/docs/API.rst). This OnceOffPoll
    // binds 'this', so leaving it registered past teardown is a use-after-free.
    PollRequest(m_can2, "!v.xte.vin", entry);
}

void OvmsVehicleToyotaETNGA::IncomingVINSuccess(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, CAN_frame_format_t format, const std::string &data)
{
    SetVehicleVIN(data);
}

void OvmsVehicleToyotaETNGA::IncomingVINFail(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, int errorcode)
{
    ESP_LOGW(TAG, "RequestVIN: Failed with error code %d", errorcode);
}
