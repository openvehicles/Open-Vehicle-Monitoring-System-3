/*
;    Project:       Open Vehicle Monitor System
;    Date:          September 2022
;
;    (C) 2022 Michael Geddes <frog@bunyip.wheelycreek.net>
; ----- Kona/Kia Module -----
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_hyundai_ioniq5.h"
#include "ovms_log.h"
#include "ovms_utils.h"
#include <string.h>

/**
 * Incoming poll reply messages
 */
void OvmsHyundaiIoniqEv::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain)
{
  XARM("OvmsHyundaiIoniqEv::IncomingPollReply");
  /*
  ESP_LOGV(TAG, "IPR %03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", m_poll_moduleid_low, type, pid, length, data[0], data[1], data[2], data[3],
     data[4], data[5], data[6], data[7]);
  */
  bool process_all = false;
  switch (m_poll_moduleid_low) {
    // ****** IGMP *****
    case 0x778:
      process_all = true;
      break;

    // ****** OBC ******
    case 0x7ed:
      break;

    // ****** BCM ******
    case 0x7a8:
      process_all = true;
      break;

    // ****** (not) AirCon ******
    case 0x7bb:
      process_all = true;
      break;

    // ****** ABS ESP ******
    case 0x7d9:
      IncomingAbsEsp(bus, type, pid, data, length, mlremain);
      break;

    // ******* VMCU ******
    case 0x7ea:
      process_all = true;
      break;

    // ******* MCU ******
    case 0x7eb:
      break;

    // ***** BMC ****
    case 0x7ec:
      process_all = true;
      break;

    // ***** CM ****
    case 0x7ce:
      process_all = true;
      break;

    default:
      ESP_LOGD(TAG, "Unknown module: %03" PRIx32, m_poll_moduleid_low);
      XDISARM;
      return;
  }

  // Enabled above as they are converted.
  if (process_all) {
    std::string &rxbuf = obd_rxbuf;

    // Assemble first and following frames to get complete reply

    // init rx buffer on first (it tells us whole length)
    if (m_poll_ml_frame == 0) {
      obd_module = m_poll_moduleid_low;
      obd_rxtype = type;
      obd_rxpid = pid;
      obd_frame = 0;
      rxbuf.clear();
      ESP_LOGV(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Buffer: %d - Start",
        m_poll_moduleid_low, type, pid, length + mlremain);
      rxbuf.reserve(length + mlremain);
    }
    else {
      if (obd_frame == 0xffff) {
        XDISARM;
        return; // Aborted
      }
      if ((obd_rxtype != type) || (obd_rxpid != pid) || (obd_module != m_poll_moduleid_low)) {
        ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Dropped Frame",
          m_poll_moduleid_low, type, pid);
        XDISARM;
        return;
      }
      ++obd_frame;
      if (obd_frame != m_poll_ml_frame) {
        obd_frame = 0xffff;
        rxbuf.clear();
        ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Skipped Frame: %d",
          m_poll_moduleid_low, type, pid, obd_frame);
        XDISARM;
        return;
      }
    }
    // Append each piece..
    rxbuf.insert(rxbuf.end(), data, data + length);
    /*
    ESP_LOGV(TAG, "IoniqISOTP: IPR %03x TYPE:%x PID: %03x Frame: %d Append: %d Expected: %d - Append",
      m_poll_moduleid_low, type, pid, m_poll_ml_frame, length, mlremain);
    */
    if (mlremain > 0) {
      // we need more - return for now.
      XDISARM;
      return;
    }

    ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Frames: %d Message Size: %d",
      m_poll_moduleid_low, type, pid, obd_frame, rxbuf.size());
    ESP_BUFFER_LOGD(TAG, rxbuf.data(), rxbuf.size());
    switch (m_poll_moduleid_low) {
      case 0x778:
        IncomingIGMP_Full(bus, type, pid, rxbuf);
        break;
      case 0x7ce:
        IncomingCM_Full(bus, type, pid, rxbuf);
        break;
      case 0x7ec:
        IncomingBMC_Full(bus, type, pid, rxbuf);
        break;
      // ****** BCM ******
      case 0x7a8:
        IncomingBCM_Full(bus, type, pid, rxbuf);
        break;
      // ****** ?? Misc inc speed ******
      case 0x7bb:
        IncomingOther_Full(bus, type, pid, rxbuf);
        break;
      // ******* VMCU ******
      case 0x7ea:
        IncomingVMCU_Full(bus, type, pid, rxbuf);
        break;
    }
    obd_frame = 0xffff; // Received all - drop until we have a new frame 0
    rxbuf.clear();
  }
  XDISARM;
}

/**
 * Handle incoming messages from cluster.
 */
void OvmsHyundaiIoniqEv::IncomingCM_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  XARM("OvmsHyundaiIoniqEv::IncomingCM_Full");
  switch (pid) {
    case 0xb002: {
      uint32_t value;
      if (!get_uint_buff_be<3>(data, 6, value)) {
        ESP_LOGE(TAG, "IoniqISOTP.CM: ODO: Bad Buffer");
      }
      else {
        StdMetrics.ms_v_pos_odometer->SetValue(value, GetConsoleUnits());
        ESP_LOGD(TAG, "IoniqISOTP.CM: ODO : %" PRId32 " km", value);
      }
    }
    break;
  }
  XDISARM;
}
/**
 * Handle incoming messages from Aircon poll.
 */
void OvmsHyundaiIoniqEv::IncomingOther_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  // 0x7b3->0x7bb
  XARM("OvmsHyundaiIoniqEv::IncomingOther_Full");
  switch (pid) {
    case 0x0100:
      {
        uint8_t value;
        if (get_uint_buff_be<1>(data, 5, value)) {
          StdMetrics.ms_v_env_cabintemp->SetValue((value/2.0) - 40, Celcius);
        }
        if (get_uint_buff_be<1>(data, 6, value)) {
          StdMetrics.ms_v_env_temp->SetValue((value/2.0) - 40, Celcius);
        }
        if (get_uint_buff_be<1>(data, 29, value)) {
          StdMetrics.ms_v_pos_speed->SetValue(value);
          CalculateAcceleration();
        }
      } break;
  }
  XDISARM;
}

/**
 * Handle incoming messages from ABS ESP poll.
 */
void OvmsHyundaiIoniqEv::IncomingAbsEsp(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain)
{
  XARM("OvmsHyundaiIoniqEv::IncomingAbsEsp");
  //ESP_LOGD(TAG, "ABS/ESP PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
  //    data[4], data[5], data[6]);
  switch (pid) {
    case 0xC101:
      if ( m_poll_ml_frame == 3) {
        uint32_t value;
        if (get_uint_bytes_be<1>(data, 2, length, value)) {
          m_v_emergency_lights->SetValue(get_bit<6>(value));
        }
        if (get_uint_bytes_be<1>(data, 1, length, value)) {
          m_v_traction_control->SetValue(get_bit<6>(value));
        }
      }
      break;
  }
  XDISARM;
}

/**
 * Handle incoming messages from VMCU-poll
 *
 * - Aux battery SOC, Voltage and current
 */
void OvmsHyundaiIoniqEv::IncomingVMCU_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  // 0x7ea

  XARM("OvmsHyundaiIoniqEv::IncomingVMCU_Full");
  if (type == VEHICLE_POLL_TYPE_READDATA) {
    switch (pid) {
      case 0xe004: {
        uint32_t res;
        if (!get_uint_buff_be<1>(data, 14, res)) {
          ESP_LOGE(TAG, "IoniqISOTP.VMCU: Shift Mode : Bad Buffer");
        }
        else {
          switch (res & 0xf) {// need &0xf?? Possibly not??
            case 0:
              iq_shift_status = IqShiftStatus::Park;
              break;
            case 5:
              iq_shift_status = IqShiftStatus::Drive;
              break;
            case 6:
              iq_shift_status = IqShiftStatus::Neutral;
              break;
            case 7:
              iq_shift_status = IqShiftStatus::Reverse;
              break;
            default: // Other.
              ESP_LOGI(TAG, "Unknown Gear selection %" PRId32, res & 0xf);
          }
          switch (iq_shift_status) {
            case IqShiftStatus::Park:
            case IqShiftStatus::Neutral:
              StdMetrics.ms_v_env_gear->SetValue(0);
              break;
            case IqShiftStatus::Drive:
              StdMetrics.ms_v_env_gear->SetValue(1);
              break;
            case IqShiftStatus::Reverse:
              StdMetrics.ms_v_env_gear->SetValue(-1);
              break;
          }
        }
        if (!get_uint_buff_be<1>(data, 9, res)) {
          ESP_LOGE(TAG, "IoniqISOTP.VMCU: Accell: Bad Buffer");
        }
        else {
          // Seems likely as it has 0->200 value.
          StdMetrics.ms_v_env_throttle->SetValue( (float)res / 2.0, Percentage);
        }
      }
      break;
    }
  }
  XDISARM;

  /** NOT IMPLEMENTED
  switch (pid) {
    case 0x02:
      if (type == VEHICLE_POLL_TYPE_OBDIIGROUP) {
        uint32_t value;
        if (!get_buff_uint_le<2>(data, 18, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.VMCU: 12V Bat Voltage: Bad Buffer");
        }
        else {
          ESP_LOGD(TAG, "IoniqISOTP.VMCU: 12V Bat Voltage: %fV", value / 1000.0);
        }
        if (!get_buff_uint_le<2>(data, 20, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.VMCU: 12V Bat Current: Bad Buffer");
        }
        else {
          ESP_LOGD(TAG, "IoniqISOTP.VMCU: 12V Bat Current: %fA", value / 1000.0);
        }
      }
      break;
  }
  XDISARM;
  */
}

/**
 * Handle incoming messages from BMC-poll
 *
 * - Pilot signal available
 * - CCS / Type2
 * - Battery current
 * - Battery voltage
 * - Battery module temp 1-8
 * - Cell voltage max / min + cell #
 * + more
 */
void OvmsHyundaiIoniqEv::IncomingBMC_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  // 0x7e4->0x7ec
  XARM("OvmsHyundaiIoniqEv::IncomingBMC_Full");
  if (type == VEHICLE_POLL_TYPE_READDATA) {
    uint32_t value;
    switch (pid) {
      case 0x0101: {

        if (!get_uint_buff_be<1>(data, 9, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Relay: Bad Buffer");
        }
        else {
          m_b_bms_relay->SetValue(get_bit<0>(value));
        }

        if (!get_uint_buff_be<1>(data, 4, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS SOC: Bad Buffer");
        }
        else {
          ESP_LOGD(TAG, "IoniqISOTP.BMC: BMS SOC: %f", value / 2.0);
          m_b_bms_soc->SetValue(value / 2.0);
        }
        float current = 0;
        int32_t signedValue;
        if (!get_buff_int_be<2>(data, 10, signedValue )) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Current: Bad Buffer");
        }
        else {
          current = (float)signedValue / 10.0;
          StdMetrics.ms_v_bat_current->SetValue(current, Amps);
        }
        if (!get_uint_buff_be<2>(data, 12, value )) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Voltage: Bad Buffer");
        }
        else {
          float voltage = (float)value / 10.0;
          StdMetrics.ms_v_bat_voltage->SetValue(voltage, Volts);

          if (current != 0) {
            m_c_power->SetValue( current * voltage, Watts);
          }

          if (current < 0) {
            StdMetrics.ms_v_charge_power->SetValue(-current * voltage, Watts);
          } else {
            StdMetrics.ms_v_charge_power->Clear();
          }
        }

        if (!get_uint_buff_be<2>(data, 5, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Avail Power: Bad Buffer");
        }
        else {
          m_b_bms_availpower->SetValue(value, Watts);
        }
        if (!get_uint_buff_be<1>(data, 15, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Min Temp Bad Buffer");
        }
        else {
          m_b_min_temperature->SetValue( value );
        }
        if (!get_uint_buff_be<1>(data, 14, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Max Temp Bad Buffer");
        }
        else {
          m_b_max_temperature->SetValue( value );
        }

        BmsRestartCellTemperatures();

        int32_t temp;
        for (int i = 0; i < 5; ++i) {
          if (!get_buff_int_be<1>(data, 16 + i, temp)) {
            ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Temp %d Bad Buffer", i + 1);
            break;
          }
          BmsSetCellTemperature(i, temp);
          // ESP_LOGV(TAG, "[%03d] T =%.2dC", i, temp);
        }
        if (!get_buff_int_be<1>(data, 22, temp)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Inlet Temp Bad Buffer");
        }
        else {
          m_b_inlet_temperature->SetValue( value, Celcius );
        }
        if (!get_buff_int_be<1>(data, 23, temp)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Cell Volt Max Bad Buffer");
        }
        else {
          m_b_cell_volt_max->SetValue( (float)value / 50.0, Volts );
        }

        if (!get_uint_buff_be<1>(data, 24, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Cell Volt Max No Bad Buffer");
        }
        else {
          m_b_cell_volt_max_no->SetValue(value);
        }

        if (!get_buff_int_be<1>(data, 25, temp)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Cell Volt Min Bad Buffer");
        }
        else {
          m_b_cell_volt_min->SetValue( (float)value / 50.0, Volts );
        }

        if (!get_uint_buff_be<1>(data, 26, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Cell Volt Min No Bad Buffer");
        }
        else {
          m_b_cell_volt_min_no->SetValue(value);
        }

        // TODO Make these metrics!
        if (!get_uint_buff_be<1>(data, 28, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Battery Fan Feedback Bad Buffer");
        }
        else {
          kn_battery_fan_feedback = value;  // Hz
        }

        if (!get_uint_buff_be<1>(data, 28, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Battery Fan status Bad Buffer");
        }
        else {
          kn_charge_bits.FanStatus = value & 0xF;  //TODO Battery fan status
        }

        if (!get_uint_buff_be<4>(data, 30, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cumulative Charge Current Bad Buffer");
        }
        else {
          StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(value * 0.1f, AmpHours);
        }

        if (!get_uint_buff_be<4>(data, 34, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cumulative Discharge Current Bad Buffer");
        }
        else {
          StdMetrics.ms_v_bat_coulomb_used_total->SetValue(value/10, AmpHours);
        }
        //
        if (!get_uint_buff_be<4>(data, 38, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cumulative Charge Energy Bad Buffer");
        }
        else {
          StdMetrics.ms_v_bat_energy_recd_total->SetValue(value*100, WattHours);
        }

        if (!get_uint_buff_be<4>(data, 42, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cumulative Discharge Energy Bad Buffer");
        }
        else {
          StdMetrics.ms_v_bat_energy_used_total->SetValue(value*100, WattHours);
        }

        if (!get_uint_buff_be<4>(data, 46, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cumulative Operating time Bad Buffer");
        }
        else {
          m_v_accum_op_time->SetValue(value, Seconds);
        }

        if (!get_uint_buff_be<1>(data, 50, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Ignition Status Bad Buffer");
        }
        else {
          m_b_bms_ignition->SetValue(get_bit<2>(value));
        }
        int32_t drive1 = 0, drive2 = 0;

        if (get_buff_int_be<2>(data, 53, drive1)
          && get_buff_int_be<2>(data, 55, drive2)) {
          int32_t drive;
          if (drive2 == 0) {
            drive = drive1;
          } else if (drive1 == 0) {
            drive = drive2;
          } else {
            drive = ((drive1 + drive2) + 1) / 2;
          }
          StandardMetrics.ms_v_mot_rpm->SetValue(drive);
        }

      }
      break;
      case 0x0105: {
        if (!get_uint_buff_be<1>(data, 23, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS Temp: Bad Buffer");
        }
        else {
          m_b_heat_1_temperature->SetValue( value );
        }

        if (!get_uint_buff_be<2>(data, 25, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: SOH: Bad Buffer");
        }
        else {
          float sohval = (float) value / 10.0;
          ESP_LOGD(TAG, "IoniqISOTP.BMC: SOH: %f", sohval);
          StdMetrics.ms_v_bat_soh->SetValue( (float)value / 10.0 );
        }

        if (!get_uint_buff_be<1>(data, 27, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cell Det Max No: Bad Buffer");
        }
        else {
          m_b_cell_det_max_no->SetValue( value );
        }
        if (!get_uint_buff_be<2>(data, 28, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cell Det Min: Bad Buffer");
        }
        else {
          m_b_cell_det_min->SetValue( (float)value / 10.0 );
        }
        if (!get_uint_buff_be<1>(data, 30, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: Cell Det Min: Bad Buffer");
        }
        else {
          m_b_cell_det_min_no->SetValue( value, Percentage );
        }

        if (!get_uint_buff_be<1>(data, 31, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.BMC: SOC: Bad Buffer");
        }
        else {
          ESP_LOGD(TAG, "IoniqISOTP.BMC: SOC: %f", value / 2.0);
          StdMetrics.ms_v_bat_soc->SetValue(value / 2.0, Percentage);
        }

        int32_t temp;
        for (int i = 0; i < 11; ++i) {
          // Bytes 9..15 contain temps 5..11
          // Bytes 39..42 contain temps 12..15
          int byteIndex = i + ((i <= 6) ? 9 : (39 - 7));
          if (!get_buff_int_be<1>(data, byteIndex, temp)) {
            ESP_LOGE(TAG, "IoniqISOTP.BMC: BMS %d -> Temp %d Bad Buffer", byteIndex, i + 6);
            break;
          }
          BmsSetCellTemperature(5 + i, temp);
          ESP_LOGV(TAG, "[%03d] T =%.2" PRId32 "C", 5 + i, temp);
        }
      }
      break;
      case 0x0102: //Cell voltages
      case 0x0103:
      case 0x0104:
      case 0x010A:
      case 0x010B:
      case 0x010C: {
        int32_t base = 0;
        bool isfinal = false;
        switch (pid) {
          case 0x0102:
            ESP_LOGV(TAG, "Cell Voltage Reset");
            BmsRestartCellVoltages();
            iq_last_voltage_cell = -1;
            break;
          case 0x0103:
            base = 32;
            break;
          case 0x0104:
            base = 64;
            break;
          case 0x010A:
            base = 96;
            break;
          case 0x010B:
            base = 128;
            break;
          case 0x010C:
            base = 160;
            isfinal = true;
            break;
        }
        // It seems that the first 4 byte bitset indicate which cells are active.
        uint32_t present;
        if (! get_uint_buff_be<4>(data, 0, present)) {
          ESP_LOGE(TAG, "IoniqISOTP.Battery Cells from %" PRId32 " : Bad Buffer", base);

          break; // Can't get subsequent data if this fails!!
        }
        ESP_LOGV(TAG, "IoniqISOTP.Battery Cells Available from %" PRId32 " - %.8" PRIx32 " ", base, present);

        for (int32_t idx = 0; idx < 32; ++idx) {
          // Bitset is from high downto low, left to right.
          // Eg: last set of cells is byte: ( ff ff f0 00 )
          bool isPresent = (present & (0x80000000U >> idx)) != 0;
          if (isPresent) {
            int32_t cellNo = base + idx;
            if (cellNo > iq_last_voltage_cell) {
              iq_last_voltage_cell = cellNo;
            }

            if (! get_uint_buff_be<1>(data, 4 + idx, value)) {
              ESP_LOGE(TAG, "IoniqISOTP.Battery Cell %" PRId32 " : Bad Buffer", cellNo);
            }
            else {
              float voltage = (float)value * 0.02;
              BmsSetCellVoltage(cellNo, voltage);
            }
          }
        }
        if (isfinal && m_bms_bitset_cv > 0) {
          // Finalise with what we have!
          int cellcount = iq_last_voltage_cell + 1;
          if (BmsCheckChangeCellArrangementVoltage(cellcount) && !hif_override_capacity)
          {
            hif_battery_capacity = HIF_CELL_PAIR_CAPACITY * cellcount;
            m_v_bat_calc_cap->SetValue(hif_battery_capacity);
            UpdateMaxRangeAndSOH();
          }

        }
      }
      break;
    }
  }
  XDISARM;
}

/**
 * Handle incoming messages from BCM-poll
 *
 */
void OvmsHyundaiIoniqEv::IncomingBCM_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  XARM("OvmsHyundaiIoniqEv::IncomingBCM_Full");
  //0x7a8
  if (type == VEHICLE_POLL_TYPE_READDATA) {
    switch (pid) {
      case 0xC002: { // TPMS ID
        uint32_t idPart;
        for (int idx = 0; idx < 4; ++idx) {
          if (get_uint_buff_be<4>(data, 4 + (idx * 4), idPart)) {
            if (idPart != 0) {
              kia_tpms_id[idx] = idPart;
            }
          }
        }
      }
      break;
      case 0xC00B: { // Tyre
        uint32_t iPSI, iTemp;
        if (get_uint_buff_be<1>(data, 4, iPSI) && get_uint_buff_be<1>(data, 5, iTemp)) {
          if (iPSI > 0) {
            StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, iPSI / 5.0, PSI);
          }
          if (iTemp > 0) {
            StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, iTemp - 50.0, Celcius);
          }
        }
        if (get_uint_buff_be<1>(data, 9, iPSI) && get_uint_buff_be<1>(data, 10, iTemp)) {
          if (iPSI > 0) {
            StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, iPSI / 5.0, PSI);
          }
          if (iTemp > 0) {
            StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, iTemp - 50.0, Celcius);
          }
        }
        if (get_uint_buff_be<1>(data, 14, iPSI) && get_uint_buff_be<1>(data, 15, iTemp)) {
          if (iPSI > 0) {
            StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, iPSI / 5.0, PSI);
          }
          if (iTemp > 0) {
            StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, iTemp - 50.0, Celcius);
          }
        }
        if (get_uint_buff_be<1>(data, 19, iPSI) && get_uint_buff_be<1>(data, 20, iTemp)) {
          if (iPSI > 0) {
            StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, iPSI / 5.0, PSI);
          }
          if (iTemp > 0) {
            StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, iTemp - 50.0, Celcius);
          }
        }
      }
      break;
      case 0xb008: {
        uint32_t value;
        if (!get_uint_buff_be<1>(data, 6, value)) {
          ESP_LOGE(TAG, "IoniqISOTP.CM: Brake: Bad Buffer");
        }
        else {
          // TODO: Currently only 0 or 100. Possibly looking at brake light here?
          StdMetrics.ms_v_env_footbrake->SetValue( get_bit<4>(value) * 100);
        }
      }
      break;
      case 0xb00c: {
        // TODO: Check
        uint8_t value;
        if (get_uint_buff_be<1>(data, 5, value)) {
          m_v_heated_handle->SetValue(get_bit<5>(value));
        }
      }
      break;

    }
  }
  XDISARM;

}

/**
 * Handle incoming messages from IGMP-poll
 *
 */
void OvmsHyundaiIoniqEv::IncomingIGMP_Full(canbus *bus, uint16_t type, uint16_t pid, const std::string &data)
{
  XARM("OvmsHyundaiIoniqEv::IncomingIGMP_Full");
  // 0x778
  if (type != VEHICLE_POLL_TYPE_READDATA) {
    XDISARM;
    return;
  }
  switch (pid) {
    case 0xbc03: {
      XARMX(one, "IncomingIGMP_Full.0xbc03");
      uint32_t lVal;
      // Byte 5 Entries
      if (get_uint_buff_be<1>(data, 5, lVal)) {
        // Vehicle is in "ignition" state (drivable)
        StdMetrics.ms_v_env_on->SetValue((lVal & 0x60) > 0);
        // Vehicle is fully awake (switched on by the user)
        // ms_v_env_awake

        m_v_seat_belt_driver->SetValue(get_bit<1>(lVal));
        m_v_seat_belt_passenger->SetValue(get_bit<2>(lVal));

        StdMetrics.ms_v_door_hood->SetValue(get_bit<0>(lVal));
      }
      // Byte 4 entries
      if (!get_uint_buff_be<1>(data, 4, lVal)) {
        ESP_LOGE(TAG, "IoniqISOTP.IGMP: Door Locks/Open: Bad Buffer");
      }
      else {
        StdMetrics.ms_v_door_trunk->SetValue(get_bit<7>(lVal));
        /*
         * Car data is by driver, but metrics is left/right
         */
        bool passenger_rear_open = get_bit<0>(lVal);
        bool passenger_rear_lock = !get_bit<1>(lVal);
        bool driver_rear_open = get_bit<2>(lVal);
        bool driver_rear_lock = !get_bit<3>(lVal);
        bool passenger_front_open = get_bit<4>(lVal);
        bool driver_front_open = get_bit<5>(lVal);

        if (IsLHD()) {
          StdMetrics.ms_v_door_rl->SetValue(driver_rear_open);
          m_v_door_lock_rl->SetValue(driver_rear_lock);
          StdMetrics.ms_v_door_rr->SetValue(passenger_rear_open);
          m_v_door_lock_rr->SetValue(passenger_rear_lock);
          StdMetrics.ms_v_door_fl->SetValue(driver_front_open);
          StdMetrics.ms_v_door_fr->SetValue(passenger_front_open);
        }
        else {
          StdMetrics.ms_v_door_rr->SetValue(driver_rear_open);
          m_v_door_lock_rr->SetValue(driver_rear_lock);
          StdMetrics.ms_v_door_rl->SetValue(passenger_rear_open);
          m_v_door_lock_rl->SetValue(passenger_rear_lock);
          StdMetrics.ms_v_door_fr->SetValue(driver_front_open);
          StdMetrics.ms_v_door_fl->SetValue(passenger_front_open);
        }
      }
      XDISARMX(one);
    }
    break;
    case 0xbc04: {
      XARMX(two, "IncomingIGMP_Full.0xbc04");
      uint32_t lVal;
      /* Can't be this - only 8 bytes received
      if (get_uint_buff_be<1>(data, 8, lVal) ) {
        StdMetrics.ms_v_env_headlights->SetValue(get_bit<4>(lVal));
      } else {
          ESP_LOGE(TAG, "IoniqISOTP.IGMP: Headlights: Bad Buffer");
      }
      */
      bool is_lhd = IsLHD();
      if (!get_uint_buff_be<1>(data, 4, lVal)) {
        ESP_LOGE(TAG, "IoniqISOTP.IGMP: Door Locks: Bad Buffer");
      }
      else {
        /*
         * Car data is by driver, but metrics is left/right
         */
        bool driver_door = !get_bit<3>(lVal);
        bool passenger_door = !get_bit<2>(lVal);
        // ESP_LOGD(TAG, "IoniqISOTP.IGMP: Front Door Locks: Driver: %s Passenger: %s", (passenger_door?"Locked":"Unlocked"), (driver_door?"Locked":"Unlocked"));
        if (is_lhd) {
          m_v_door_lock_fl->SetValue(driver_door);
          m_v_door_lock_fr->SetValue(passenger_door);
        }
        else {
          m_v_door_lock_fr->SetValue(driver_door);
          m_v_door_lock_fl->SetValue(passenger_door);
        }
      }
      if (!get_uint_buff_be<1>(data, 7, lVal)) {
        ESP_LOGE(TAG, "IoniqISOTP.IGMP: Belts Bad Buffer");
      }
      else {
        m_v_seat_belt_back_middle->SetValue(get_bit<3>(lVal));
        bool driver_seat = get_bit<2>(lVal);
        bool passenger_seat = get_bit<4>(lVal);
        // SP_LOGD(TAG, "IoniqISOTP.IGMP: Seatbelts: Driver: %s Passenger: %s", (passenger_seat?"On":"Off"), (driver_seat?"On":"Off"));
        if (is_lhd) {
          m_v_seat_belt_back_left->SetValue(driver_seat);
          m_v_seat_belt_back_right->SetValue(passenger_seat);
        }
        else {
          m_v_seat_belt_back_right->SetValue(driver_seat);
          m_v_seat_belt_back_left->SetValue(passenger_seat);
        }
      }
      XDISARMX(two);
    }
    break;
    case 0xbc07: {
      XARMX(three, "IncomingIGMP_Full.0xbc04");
      uint32_t lVal;
      if (get_uint_buff_be<1>(data, 5, lVal)) {
        m_v_rear_defogger->SetValue(get_bit<1>(lVal));
      }
      XDISARMX(three);
    }
    break;
    case 0xbc09: {
      uint32_t lightVal;
      if (get_uint_buff_be<1>(data, 6, lightVal)) {
        bool llow = ((lightVal & 0xc0) == 0xc0);
        if (iq_lights_low != llow) {
          iq_lights_dirty = true;
          iq_lights_low = llow;
        }
      }
      if (get_uint_buff_be<1>(data, 7, lightVal)) {
        bool lhigh = ((lightVal & 0x03) == 0x03 );
        bool lpark = ((lightVal & 0x30) == 0x30 );
        if (iq_lights_high != lhigh || iq_lights_park != lpark) {
          iq_lights_dirty = true;
          iq_lights_high = lhigh;
          iq_lights_park = lpark;
        }
      }
    }
    break;
    case 0xbc10: {
      uint32_t lightVal;
      if (get_uint_buff_be<1>(data, 4, lightVal)) {
        bool lon = ((lightVal & 0xf0) == 0xf0);
        if (lon != iq_lights_on) {
          iq_lights_dirty = true;
          iq_lights_on = lon;
        }

      }
    }
    break;
  }
  if (iq_lights_dirty) {
    StandardMetrics.ms_v_env_headlights->SetValue(iq_lights_on & iq_lights_low);
    m_v_env_lowbeam->SetValue(iq_lights_on & iq_lights_low & !iq_lights_high);
    m_v_env_highbeam->SetValue(iq_lights_on & iq_lights_high);
    m_v_env_parklights->SetValue(iq_lights_on & iq_lights_park);
  }
  XDISARM;
}

int OvmsHyundaiIoniqEv::RequestVIN()
{
  //ESP_LOGD(TAG, "RequestVIN: Sending Request");
  if (!StdMetrics.ms_v_env_awake->AsBool()) {
    ESP_LOGD(TAG, "RequestVIN: Not Awake Request not sent");
    return -3;
  }
  std::string response;
  int res = PollSingleRequest( m_can1,
      VEHICLE_OBD_BROADCAST_MODULE_TX, VEHICLE_OBD_BROADCAST_MODULE_RX,
      VEHICLE_POLL_TYPE_OBDIIVEHICLE,  2, response, 1000);
  if (res != POLLSINGLE_OK) {
    switch (res) {
      case POLLSINGLE_TIMEOUT:
        ESP_LOGE(TAG, "RequestVIN: Request Timeout");
        break;
      case POLLSINGLE_TXFAILURE:
        ESP_LOGE(TAG, "RequestVIN: Request TX Failure");
        break;
      default:
        ESP_LOGE(TAG, "RequestVIN: UDC Error %d", res);
    }

    return res;
  }
  else {
    uint32_t byte;
    if (!get_uint_buff_be<1>(response, 4, byte)) {
      ESP_LOGE(TAG, "RequestVIN: Bad Buffer");
      return POLLSINGLE_TIMEOUT;
    }
    else if (byte != 1) {
      ESP_LOGI(TAG, "RequestVIN: Ignore Response");
      return POLLSINGLE_TIMEOUT;
    }
    else {
      std::string vin;
      if ( get_buff_string(response, 5, 17, vin)) {
        if (vin.length() > 5 && vin[4] == '-') {
          vin = vin.substr(0, 3) + vin.substr(10) + "-------";
        }
        StandardMetrics.ms_v_vin->SetValue(vin);
        ESP_BUFFER_LOGD(TAG, response.data(), response.size());

        vin.copy(m_vin, sizeof(m_vin) - 1);
        m_vin[sizeof(m_vin) - 1] = '\0';
        ESP_LOGD(TAG, "RequestVIN: Success: '%s'->'%s'", vin.c_str(), m_vin);
        return POLLSINGLE_OK;
      }
      else {
        ESP_LOGE(TAG, "RequestVIN.String: Bad Buffer");
        return POLLSINGLE_TIMEOUT;
      }
    }
  }
}

/**
 * Get console ODO units
 *
 * Currently from configuration
 */
metric_unit_t OvmsHyundaiIoniqEv::GetConsoleUnits()
{
  XARM("OvmsHyundaiIoniqEv::GetConsoleUnits");
  metric_unit_t res = MyConfig.GetParamValueBool("xkn", "consoleKilometers", true) ? Kilometers : Miles;
  XDISARM;
  return res;
}

/**
 * Determine if this car is left hand drive
 *
 * Currently from configuration
 */
bool OvmsHyundaiIoniqEv::IsLHD()
{
  XARM("OvmsHyundaiIoniqEv::IsLHD");
  bool res = MyConfig.GetParamValueBool("xkn", "leftDrive", true);
  XDISARM;
  return res;
}

