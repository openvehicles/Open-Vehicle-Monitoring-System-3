/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Hyundai Ioniq vFL
;    Date:          2021-03-13
;
;    Changes:
;    1.0  Initial release
;
;    (c) 2021       Michael Balzer <dexter@dexters-web.de>
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
static const char *TAG = "v-hyundaivfl";

#include <stdio.h>
#include "vehicle_hyundai_ioniqvfl.h"
#include "ovms_webserver.h"

// Vehicle states:
#define STATE_OFF             0
#define STATE_ON              1
#define STATE_CHARGING        2

// RX buffer access macros: b=byte#
#define RXB_BYTE(b)           m_rxbuf[b]
#define RXB_UINT16(b)         (((uint16_t)RXB_BYTE(b) << 8) | RXB_BYTE(b+1))
#define RXB_UINT24(b)         (((uint32_t)RXB_BYTE(b) << 16) | ((uint32_t)RXB_BYTE(b+1) << 8) \
                               | RXB_BYTE(b+2))
#define RXB_UINT32(b)         (((uint32_t)RXB_BYTE(b) << 24) | ((uint32_t)RXB_BYTE(b+1) << 16) \
                               | ((uint32_t)RXB_BYTE(b+2) << 8) | RXB_BYTE(b+3))
#define RXB_INT8(b)           ((int8_t)RXB_BYTE(b))
#define RXB_INT16(b)          ((int16_t)RXB_UINT16(b))
#define RXB_INT32(b)          ((int32_t)RXB_UINT32(b))


/**
 * OvmsVehicleHyundaiVFL PID list
 */

#define UDS_READ8             0x21
#define UDS_READ16            0x22

static const OvmsVehicle::poll_pid_t standard_polls[] =
{
  { 0x7e4, 0x7ec, UDS_READ8,    0x01, {   1,   1,   1 }, 0, ISOTP_STD },
  { 0x7e4, 0x7ec, UDS_READ8,    0x02, {   0,  15,  15 }, 0, ISOTP_STD },
  { 0x7e4, 0x7ec, UDS_READ8,    0x03, {   0,  15,  15 }, 0, ISOTP_STD },
  { 0x7e4, 0x7ec, UDS_READ8,    0x04, {   0,  15,  15 }, 0, ISOTP_STD },
  { 0x7e4, 0x7ec, UDS_READ8,    0x05, {   0,  15,  15 }, 0, ISOTP_STD },
  { 0x7e6, 0x7ee, UDS_READ8,    0x80, {   0,  60, 120 }, 0, ISOTP_STD },
  { 0x7c6, 0x7ce, UDS_READ16, 0xb002, {   0,  30,   0 }, 0, ISOTP_STD },
  POLL_LIST_END
};


/**
 * OvmsVehicleHyundaiVFL constructor
 */
OvmsVehicleHyundaiVFL::OvmsVehicleHyundaiVFL()
{
  ESP_LOGI(TAG, "Hyundai Ioniq vFL vehicle module");

  // Init CAN:
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  // Init metrics:
  m_xhi_charge_state = MyMetrics.InitInt("xhi.c.state", 10, 0, Other, true);
  m_xhi_bat_soc_bms = MyMetrics.InitFloat("xhi.b.soc.bms", 30, 0, Percentage, true);

  // Init BMS:
  BmsSetCellArrangementVoltage(96, 12);
  BmsSetCellArrangementTemperature(12, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  // Init web UI:
  MyWebServer.RegisterPage("/xhi/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);

  // Init polling:
  PollSetThrottling(0);
  PollSetResponseSeparationTime(1);
  PollSetState(STATE_OFF);
  PollSetPidList(m_can1, standard_polls);
}


/**
 * OvmsVehicleHyundaiVFL destructor
 */
OvmsVehicleHyundaiVFL::~OvmsVehicleHyundaiVFL()
{
  ESP_LOGI(TAG, "Shutdown Hyundai Ioniq vFL vehicle module");
  PollSetPidList(m_can1, NULL);
  MyWebServer.DeregisterPage("/xhi/battmon");
  MyMetrics.DeregisterMetric(m_xhi_charge_state);
  MyMetrics.DeregisterMetric(m_xhi_bat_soc_bms);
}


/**
 * IncomingPollReply: framework callback: process response
 */
void OvmsVehicleHyundaiVFL::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    m_rxbuf.clear();
    m_rxbuf.reserve(length + mlremain);
  }
  m_rxbuf.append((char*)data, length);
  if (mlremain)
    return;

  // response complete:
  ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
  switch (pid)
  {
    case 0x01:
    {
      // Read status & battery metrics:
      m_xhi_bat_soc_bms->SetValue(RXB_BYTE(4) * 0.5f);
      m_xhi_charge_state->SetValue(RXB_BYTE(9));
      float bat_current = RXB_INT16(10) * 0.1f;
      float bat_voltage = RXB_UINT16(12) * 0.1f;
      StdMetrics.ms_v_bat_current->SetValue(bat_current);
      StdMetrics.ms_v_bat_voltage->SetValue(bat_voltage);
      StdMetrics.ms_v_bat_power->SetValue(bat_voltage * bat_current / 1000);
      StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(RXB_UINT32(30) * 0.1f);
      StdMetrics.ms_v_bat_coulomb_used_total->SetValue(RXB_UINT32(34) * 0.1f);
      StdMetrics.ms_v_bat_energy_recd_total->SetValue(RXB_UINT32(38) * 0.1f);
      StdMetrics.ms_v_bat_energy_used_total->SetValue(RXB_UINT32(42) * 0.1f);

      // Read vehicle speed:
      // …this seems to be wrong or needs another scaling/interpretation (rpm?)
      //float speed = RXB_INT16(53);
      //StdMetrics.ms_v_pos_speed->SetValue(fabs(speed));

      // Read battery module temperatures 1-5 (only when also polling PIDs 02…05):
      if (m_poll_state != STATE_OFF && m_poll_ticker % 15 == 0)
      {
        BmsRestartCellTemperatures();
        for (int i = 0; i < 5; i++) {
          BmsSetCellTemperature(i, RXB_INT8(16+i));
        }
      }
      break;
    }

    case 0x02:
    {
      // Read battery cell voltages 1-32:
      BmsRestartCellVoltages();
      for (int i = 0; i < 32; i++) {
        BmsSetCellVoltage(i, RXB_BYTE(4+i) * 0.02f);
      }
      break;
    }
    case 0x03:
    {
      // Read battery cell voltages 33-64:
      for (int i = 0; i < 32; i++) {
        BmsSetCellVoltage(32+i, RXB_BYTE(4+i) * 0.02f);
      }
      break;
    }
    case 0x04:
    {
      // Read battery cell voltages 65-96:
      for (int i = 0; i < 32; i++) {
        BmsSetCellVoltage(64+i, RXB_BYTE(4+i) * 0.02f);
      }
      break;
    }

    case 0x05:
    {
      // Read battery module temperatures 6-12:
      for (int i = 0; i < 7; i++) {
        BmsSetCellTemperature(5+i, RXB_INT8(9+i));
      }
      StdMetrics.ms_v_bat_temp->SetValue(StdMetrics.ms_v_bat_pack_tavg->AsFloat());

      // Read SOH & user SOC:
      float bat_soh = RXB_UINT16(25) * 0.1f;
      StdMetrics.ms_v_bat_soh->SetValue(bat_soh);
      float bat_soc = RXB_BYTE(31) * 0.5f;
      StdMetrics.ms_v_bat_soc->SetValue(bat_soc);
      break;
    }

    case 0x80:
    {
      // Read ambient temperature:
      float temp = RXB_BYTE(12) * 0.5f - 40.0f;
      StdMetrics.ms_v_env_temp->SetValue(temp);
      break;
    }

    case 0xb002:
    {
      // Read odometer:
      float odo = RXB_UINT24(6);
      StdMetrics.ms_v_pos_odometer->SetValue(odo);
      break;
    }

    default:
    {
      ESP_LOGW(TAG, "IncomingPollReply: unhandled PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
    }
  }
}


/**
 * PollerStateTicker: framework callback: check for state changes
 *  This is called by VehicleTicker1() just before the next PollerSend().
 */
void OvmsVehicleHyundaiVFL::PollerStateTicker()
{
  bool car_online = (m_can1->GetErrorState() < CAN_errorstate_passive && !m_xhi_charge_state->IsStale());
  int charge_state = m_xhi_charge_state->AsInt();

  // Determine new polling state:
  int poll_state;
  if (!car_online)
    poll_state = STATE_OFF;
  else if (charge_state & 0x80)
    poll_state = STATE_CHARGING;
  else
    poll_state = STATE_ON;

  // Set base state flags:
  StdMetrics.ms_v_env_aux12v->SetValue(car_online);
  StdMetrics.ms_v_env_charging12v->SetValue(car_online);
  StdMetrics.ms_v_env_awake->SetValue(poll_state == STATE_ON);
  StdMetrics.ms_v_env_on->SetValue(poll_state == STATE_ON);

  //
  // Handle polling state change
  //

  if (poll_state != m_poll_state) {
    ESP_LOGI(TAG,
      "PollerStateTicker: [%s] charge_state=%02x => PollState %d->%d",
      car_online ? "online" : "offline", charge_state, m_poll_state, poll_state);
  }

  if (poll_state == STATE_CHARGING)
  {
    if (m_poll_state != STATE_CHARGING) {
      // Charge started:
      StdMetrics.ms_v_door_chargeport->SetValue(true);
      StdMetrics.ms_v_charge_pilot->SetValue(true);
      StdMetrics.ms_v_charge_inprogress->SetValue(true);
      StdMetrics.ms_v_charge_substate->SetValue("onrequest");
      StdMetrics.ms_v_charge_state->SetValue("charging");
      PollSetState(STATE_CHARGING);
    }
  }
  else
  {
    if (m_poll_state == STATE_CHARGING) {
      // Charge stopped:
      StdMetrics.ms_v_charge_pilot->SetValue(false);
      StdMetrics.ms_v_charge_inprogress->SetValue(false);
      int soc = StdMetrics.ms_v_bat_soc->AsInt();
      if (soc >= 99) {
        StdMetrics.ms_v_charge_substate->SetValue("stopped");
        StdMetrics.ms_v_charge_state->SetValue("done");
      } else {
        StdMetrics.ms_v_charge_substate->SetValue("interrupted");
        StdMetrics.ms_v_charge_state->SetValue("stopped");
      }
    }
  }

  if (poll_state == STATE_ON)
  {
    if (m_poll_state != STATE_ON) {
      // Switched on:
      StdMetrics.ms_v_door_chargeport->SetValue(false);
      StdMetrics.ms_v_charge_substate->SetValue("");
      StdMetrics.ms_v_charge_state->SetValue("");
      PollSetState(STATE_ON);
    }
  }

  if (poll_state == STATE_OFF)
  {
    if (m_poll_state != STATE_OFF) {
      // Switched off:
      StdMetrics.ms_v_bat_current->SetValue(0);
      StdMetrics.ms_v_bat_power->SetValue(0);
      PollSetState(STATE_OFF);
    }
  }
}


/**
 * Vehicle framework registration
 */

class OvmsVehicleHyundaiVFLInit
{
  public: OvmsVehicleHyundaiVFLInit();
} MyOvmsVehicleHyundaiVFLInit  __attribute__ ((init_priority (9000)));

OvmsVehicleHyundaiVFLInit::OvmsVehicleHyundaiVFLInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: Hyundai Ioniq vFL (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleHyundaiVFL>("HIONVFL","Hyundai Ioniq vFL");
}
