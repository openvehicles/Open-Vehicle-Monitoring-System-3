/**
 * Project:      Open Vehicle Monitor System
 * Module:       VW e-Up via OBD Port
 *
 * (c) 2020  Soko
 * (c) 2021  Michael Balzer <dexter@dexters-web.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#include <stdio.h>
#include <string>
#include <iomanip>

#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_vweup.h"
#include "vweup_obd.h"


//
// General PIDs for all model years
//
const OvmsVehicle::poll_pid_t vweup_polls[] = {
  // Note: poller ticker cycles at 3600 seconds = max period
  // { ecu, type, pid, {_OFF,_ON,_CHARGING}, bus, protocol }

  {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_POWER_MOT,        {  0,  1,  0}, 1, ISOTP_STD},

  {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_SOC_NORM,         {  0, 20,  0}, 1, ISOTP_STD},
  {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_SOC_ABS,          {  0, 20,  0}, 1, ISOTP_STD},
  {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_SOC_ABS,          {  0, 20, 20}, 1, ISOTP_STD},
  {VWUP_CHG_MGMT, UDS_READ, VWUP_CHG_MGMT_SOC_NORM,         {  0,  0, 20}, 1, ISOTP_STD},
  {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_ENERGY_COUNTERS,  {  0, 20, 20}, 1, ISOTP_STD},

  {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_CELL_MAX,         {  0, 20, 20}, 1, ISOTP_STD},
  {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_CELL_MIN,         {  0, 20, 20}, 1, ISOTP_STD},
  // Same tick & order important of above 2: VWUP_BAT_MGMT_CELL_MIN calculates the delta

  {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_TEMP,             {  0, 20, 20}, 1, ISOTP_STD},

  {VWUP_CHG,      UDS_READ, VWUP_CHG_POWER_EFF,             {  0,  5, 10}, 1, ISOTP_STD}, // 5 @ _ON to detect charging
  {VWUP_CHG,      UDS_READ, VWUP_CHG_POWER_LOSS,            {  0,  0, 10}, 1, ISOTP_STD},

  {VWUP_MFD,      UDS_READ, VWUP_MFD_ODOMETER,              {  0, 60,  0}, 1, ISOTP_STD},
  {VWUP_MFD,      UDS_READ, VWUP_MFD_RANGE_CAP,             {  0, 60,  0}, 1, ISOTP_STD},

//{VWUP_BRK,      UDS_READ, VWUP_BRK_TPMS,                  {  0,  5,  0}, 1, ISOTP_STD},
  {VWUP_MFD,      UDS_READ, VWUP_MFD_SERV_RANGE,            {  0, 60,  0}, 1, ISOTP_STD},
  {VWUP_MFD,      UDS_READ, VWUP_MFD_SERV_TIME,             {  0, 60,  0}, 1, ISOTP_STD},

  {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_TEMP_DCDC,        {  0, 20,  0}, 1, ISOTP_STD},
  {VWUP_ELD,      UDS_READ, VWUP_ELD_DCDC_U,                {  0,  5, 10}, 1, ISOTP_STD},
  {VWUP_ELD,      UDS_READ, VWUP_ELD_DCDC_I,                {  0,  5, 10}, 1, ISOTP_STD},
  {VWUP_ELD,      UDS_READ, VWUP_ELD_TEMP_MOT,              {  0, 20, 60}, 1, ISOTP_STD},
  {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_TEMP_PEM,         {  0, 20,  0}, 1, ISOTP_STD},
  {VWUP_CHG,      UDS_READ, VWUP_CHG_TEMP_COOLER,           {  0, 20, 20}, 1, ISOTP_STD},
//{VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_TEMP_MAX,         {  0, 20,  0}, 1, ISOTP_STD},
//{VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_TEMP_MIN,         {  0, 20,  0}, 1, ISOTP_STD},

  {VWUP_CHG_MGMT, UDS_READ, VWUP_CHG_MGMT_REM,              {  0,  0, 30}, 1, ISOTP_STD},
};

//
// Specific PIDs for gen1 model (before year 2020)
//
const OvmsVehicle::poll_pid_t vweup_gen1_polls[] = {
  {VWUP_CHG,      UDS_READ, VWUP1_CHG_AC_U,                 {  0,  0,  5}, 1, ISOTP_STD},
  {VWUP_CHG,      UDS_READ, VWUP1_CHG_AC_I,                 {  0,  0,  5}, 1, ISOTP_STD},
  // Same tick & order important of above 2: VWUP_CHG_AC_I calculates the AC power
  {VWUP_CHG,      UDS_READ, VWUP1_CHG_DC_U,                 {  0,  0,  5}, 1, ISOTP_STD},
  {VWUP_CHG,      UDS_READ, VWUP1_CHG_DC_I,                 {  0,  0,  5}, 1, ISOTP_STD},
  // Same tick & order important of above 2: VWUP_CHG_DC_I calculates the DC power
  // Same tick & order important of above 4: VWUP_CHG_DC_I calculates the power loss & efficiency
};

//
// Specific PIDs for gen2 model (from year 2020)
//
const OvmsVehicle::poll_pid_t vweup_gen2_polls[] = {
  {VWUP_CHG,      UDS_READ, VWUP2_CHG_AC_U,                 {  0,  0,  5}, 1, ISOTP_STD},
  {VWUP_CHG,      UDS_READ, VWUP2_CHG_AC_I,                 {  0,  0,  5}, 1, ISOTP_STD},
  // Same tick & order important of above 2: VWUP_CHG_AC_I calculates the AC power
  {VWUP_CHG,      UDS_READ, VWUP2_CHG_DC_U,                 {  0,  0,  5}, 1, ISOTP_STD},
  {VWUP_CHG,      UDS_READ, VWUP2_CHG_DC_I,                 {  0,  0,  5}, 1, ISOTP_STD},
  // Same tick & order important of above 2: VWUP_CHG_DC_I calculates the DC power
  // Same tick & order important of above 4: VWUP_CHG_DC_I calculates the power loss & efficiency
};


void OvmsVehicleVWeUp::OBDInit()
{
  ESP_LOGI(TAG, "Starting connection: OBDII");

  //
  // First time initialization
  //

  if (m_obd_state == OBDS_Init)
  {
    // Init metrics:
    BatMgmtSoCAbs = MyMetrics.InitFloat("xvu.b.soc.abs", 100, 0, Percentage);
    MotElecSoCAbs = MyMetrics.InitFloat("xvu.m.soc.abs", 100, 0, Percentage);
    MotElecSoCNorm = MyMetrics.InitFloat("xvu.m.soc.norm", 100, 0, Percentage);
    ChgMgmtSoCNorm = MyMetrics.InitFloat("xvu.c.soc.norm", 100, 0, Percentage);
    BatMgmtCellDelta = MyMetrics.InitFloat("xvu.b.cell.delta", SM_STALE_NONE, 0, Volts);

    ChargerPowerEffEcu = MyMetrics.InitFloat("xvu.c.eff.ecu", 100, 0, Percentage);
    ChargerPowerLossEcu = MyMetrics.InitFloat("xvu.c.loss.ecu", SM_STALE_NONE, 0, Watts);
    ChargerPowerEffCalc = MyMetrics.InitFloat("xvu.c.eff.calc", 100, 0, Percentage);
    ChargerPowerLossCalc = MyMetrics.InitFloat("xvu.c.loss.calc", SM_STALE_NONE, 0, Watts);
    ChargerACPower = MyMetrics.InitFloat("xvu.c.ac.p", SM_STALE_NONE, 0, Watts);
    ChargerAC1U = MyMetrics.InitFloat("xvu.c.ac.u1", SM_STALE_NONE, 0, Volts);
    ChargerAC2U = MyMetrics.InitFloat("xvu.c.ac.u2", SM_STALE_NONE, 0, Volts);
    ChargerAC1I = MyMetrics.InitFloat("xvu.c.ac.i1", SM_STALE_NONE, 0, Amps);
    ChargerAC2I = MyMetrics.InitFloat("xvu.c.ac.i2", SM_STALE_NONE, 0, Amps);
    ChargerDC1U = MyMetrics.InitFloat("xvu.c.dc.u1", SM_STALE_NONE, 0, Volts);
    ChargerDC2U = MyMetrics.InitFloat("xvu.c.dc.u2", SM_STALE_NONE, 0, Volts);
    ChargerDC1I = MyMetrics.InitFloat("xvu.c.dc.i1", SM_STALE_NONE, 0, Amps);
    ChargerDC2I = MyMetrics.InitFloat("xvu.c.dc.i2", SM_STALE_NONE, 0, Amps);
    ChargerDCPower = MyMetrics.InitFloat("xvu.c.dc.p", SM_STALE_NONE, 0, Watts);
    ServiceDays =  MyMetrics.InitInt("xvu.e.serv.days", SM_STALE_NONE, 0);

    // Start can1:
    RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  }

  //
  // Init/reconfigure poller
  //

  m_obd_state = OBDS_Config;

  PollSetPidList(m_can1, NULL);
  PollSetThrottling(0);
  PollSetResponseSeparationTime(1);

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    PollSetState(VWEUP_CHARGING);
  else if (StandardMetrics.ms_v_env_on->AsBool())
    PollSetState(VWEUP_ON);
  else
    PollSetState(VWEUP_OFF);
  TimeOffRequested = 0;

  m_poll_vector.clear();

  // Add vehicle state detection PIDs:
  for (auto p : poll_list_t {
      {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_U, {  0,  1,  5}, 1, ISOTP_STD},
      {VWUP_BAT_MGMT, UDS_READ, VWUP_BAT_MGMT_I, {  0,  1,  5}, 1, ISOTP_STD},
      // Same tick & order important of above 2: VWUP_BAT_MGMT_I calculates the power
    }) {
    if (HasNoT26()) {
      // only OBD connected -> get car state by polling OBD
      // (is this still necessary with state detection by 12V level?)
      p.polltime[VWEUP_OFF] = 30;
    }
    m_poll_vector.push_back(p);
  }

  // Add high priority PIDs only necessary without T26:
  if (HasNoT26()) {
    m_poll_vector.insert(m_poll_vector.end(), {
      {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_SPEED,    {  0,  1,  0}, 1, ISOTP_STD},
      // … speed interval = VWUP_BAT_MGMT_U & _I to get a consistent consumption calculation
    });
  }

  // Add general & model year specific PIDs:
  m_poll_vector.insert(m_poll_vector.end(), vweup_polls, endof_array(vweup_polls));
  if (vweup_modelyear < 2020) {
    m_poll_vector.insert(m_poll_vector.end(), vweup_gen1_polls, endof_array(vweup_gen1_polls));
  }
  else {
    m_poll_vector.insert(m_poll_vector.end(), vweup_gen2_polls, endof_array(vweup_gen2_polls));
  }

  // Add low priority PIDs only necessary without T26:
  if (HasNoT26()) {
    m_poll_vector.insert(m_poll_vector.end(), {
      {VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_TEMP_AMB, {  0, 30,  0}, 1, ISOTP_STD},
      {VWUP_MFD,      UDS_READ, VWUP_MFD_RANGE_DSP,     {  0, 30,  0}, 1, ISOTP_STD},
    });
  }

  // Add BMS cell PIDs if enabled:
  if (m_cfg_cell_interval_drv || m_cfg_cell_interval_chg)
  {
    // Battery pack layout:
    //  Gen2 (2020): 2P84S in 14 modules
    //  Gen1 (2013): 2P102S in 16+1 modules
    int volts = (vweup_modelyear > 2019) ? 84 : 102;
    int temps = (vweup_modelyear > 2019) ? 14 : 16;

    // Add PIDs to poll list:
    OvmsVehicle::poll_pid_t p = { VWUP_BAT_MGMT, UDS_READ, 0, {0,0,0}, 1, ISOTP_STD };
    p.polltime[VWEUP_ON]        = m_cfg_cell_interval_drv;
    p.polltime[VWEUP_CHARGING]  = m_cfg_cell_interval_chg;
    for (int i = 0; i < volts; i++) {
      p.pid = VWUP_BAT_MGMT_CELL_VBASE + i;
      m_poll_vector.push_back(p);
    }
    for (int i = 0; i < temps; i++) {
      p.pid = VWUP_BAT_MGMT_CELL_TBASE + i;
      m_poll_vector.push_back(p);
    }
    if (vweup_modelyear <= 2019) {
      p.pid = VWUP_BAT_MGMT_CELL_T17;
      m_poll_vector.push_back(p);
    }

    // Init processing:
    m_cell_last_vi = 0;
    m_cell_last_ti = 0;
    BmsRestartCellVoltages();
    BmsRestartCellTemperatures();
  }

  // Terminate poll list:
  m_poll_vector.push_back(POLL_LIST_END);
  ESP_LOGD(TAG, "Poll vector: size=%d cap=%d", m_poll_vector.size(), m_poll_vector.capacity());
  PollSetPidList(m_can1, m_poll_vector.data());

  m_obd_state = OBDS_Run;
}


void OvmsVehicleVWeUp::OBDDeInit()
{
  m_obd_state = OBDS_DeInit;
  ESP_LOGI(TAG, "Stopping connection: OBDII");
  PollSetPidList(m_can1, NULL);
}


void OvmsVehicleVWeUp::OBDCheckCarState()
{
  if (m_obd_state != OBDS_Run)
    return;

  ESP_LOGV(TAG, "CheckCarState(): 12V=%f ChargerEff=%f BatI=%f BatIModified=%u time=%u",
    StdMetrics.ms_v_bat_12v_voltage->AsFloat(),
    ChargerPowerEffEcu->AsFloat(),
    StdMetrics.ms_v_bat_current->AsFloat(),
    StdMetrics.ms_v_bat_current->LastModified(),
    monotonictime);

  // 12V Battery: if voltage >= 12.9 it is charging and the car must be on (or charging) for that
  bool voltageSaysOn = StdMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9f;
  StdMetrics.ms_v_env_charging12v->SetValue(voltageSaysOn);

  // HV-Batt current: If there is a current flowing and the value is not older than 2 minutes (120 secs) we are on
  bool currentSaysOn = StdMetrics.ms_v_bat_current->AsFloat() != 0.0f &&
    (monotonictime - StdMetrics.ms_v_bat_current->LastModified()) < 120;

  // Charger ECU: When it reports an efficiency > 0 the car is charging
  bool chargerSaysOn = ChargerPowerEffEcu->AsFloat() > 0.0f;

  if (chargerSaysOn) {
    if (!IsCharging()) {
      ESP_LOGI(TAG, "Setting car state to CHARGING");
      StdMetrics.ms_v_env_on->SetValue(false);
      StdMetrics.ms_v_env_awake->SetValue(false);

      ResetChargeCounters();

      // TODO: get real charge mode, port & pilot states, fake for now:
      StdMetrics.ms_v_charge_mode->SetValue("standard");
      StdMetrics.ms_v_door_chargeport->SetValue(true);
      StdMetrics.ms_v_charge_pilot->SetValue(true);
      StdMetrics.ms_v_charge_inprogress->SetValue(true);
      StdMetrics.ms_v_charge_state->SetValue("charging");

      PollSetState(VWEUP_CHARGING);
      TimeOffRequested = 0;
    }
    return;
  }

  if (IsCharging()) {
    // Charge stopped/done:
    // TODO: get real charge port & pilot states, fake for now:
    StdMetrics.ms_v_charge_inprogress->SetValue(false);
    StdMetrics.ms_v_charge_pilot->SetValue(false);
    StdMetrics.ms_v_door_chargeport->SetValue(false);
    // Determine type of charge end by the SOC reached;
    //  tolerate SOC not reaching 100%
    //  TODO: read user defined destination SOC, read actual charge stop reason
    if (StdMetrics.ms_v_bat_soc->AsFloat() > 99) {
      StdMetrics.ms_v_charge_state->SetValue("done");
    }
    else {
      StdMetrics.ms_v_charge_state->SetValue("stopped");
    }
  }

  if (voltageSaysOn || currentSaysOn) {
    if (!IsOn()) {
      ESP_LOGI(TAG, "Setting car state to ON");

      ResetTripCounters();

      // TODO: get real "ignition" state, assume on for now:
      StdMetrics.ms_v_env_awake->SetValue(true);
      StdMetrics.ms_v_env_on->SetValue(true);

      // Fetch VIN once:
      if (!StdMetrics.ms_v_vin->IsDefined()) {
        std::string vin;
        if (PollSingleRequest(m_can1, VWUP_MOT_ELEC, UDS_READ, VWUP_MOT_ELEC_VIN, vin) == 0) {
          StdMetrics.ms_v_vin->SetValue(vin.substr(1));
        }
      }

      // Start regular polling:
      PollSetState(VWEUP_ON);
      TimeOffRequested = 0;
    }
    return;
  }

  if (TimeOffRequested == 0) {
    TimeOffRequested = monotonictime;
    if (TimeOffRequested == 0) {
      // For the small chance we are requesting exactly at 0 time
      TimeOffRequested--;
    }
    ESP_LOGI(TAG, "Car state to OFF requested. Waiting for possible re-activation ...");
  }

  // When already OFF or I haven't waited for 60 seconds: return
  if (IsOff() || (monotonictime - TimeOffRequested) < 60) {
    return;
  }

  // Set car to OFF
  ESP_LOGI(TAG, "Wait is over: Setting car state to OFF");
  StdMetrics.ms_v_env_on->SetValue(false);
  StdMetrics.ms_v_env_awake->SetValue(false);
  PollSetState(VWEUP_OFF);
}


void OvmsVehicleVWeUp::IncomingPollReply(canbus *bus, uint16_t type, uint16_t pid, uint8_t *data, uint8_t length, uint16_t mlremain)
{
  if (m_obd_state != OBDS_Run)
    return;

  ESP_LOGV(TAG, "IncomingPollReply(type=%u, pid=%X, length=%u, mlremain=%u): called", type, pid, length, mlremain);

  // for (uint8_t i = 0; i < length; i++)
  // {
  //   ESP_LOGV(TAG, "data[%u]=%X", i, data[i]);
  // }

  // If not all data is here: wait for the next call
  if (!PollReply.AddNewData(pid, data, length, mlremain)) {
    return;
  }

  float value;

  //
  // Handle BMS cell voltage & temperatures
  //

  if (pid >= VWUP_BAT_MGMT_CELL_VBASE && pid <= VWUP_BAT_MGMT_CELL_VLAST)
  {
    uint16_t vi = pid - VWUP_BAT_MGMT_CELL_VBASE;
    if (vi < m_cell_last_vi) {
      BmsRestartCellVoltages();
    }
    if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_VOLT", value)) {
      BmsSetCellVoltage(vi, value / 4096);
    }
    m_cell_last_vi = vi;
  }

  if ((pid >= VWUP_BAT_MGMT_CELL_TBASE && pid <= VWUP_BAT_MGMT_CELL_TLAST) ||
      (pid == VWUP_BAT_MGMT_CELL_T17))
  {
    uint16_t ti = (pid == VWUP_BAT_MGMT_CELL_T17) ? 16 : pid - VWUP_BAT_MGMT_CELL_TBASE;
    if (ti < m_cell_last_ti) {
      BmsRestartCellTemperatures();
    }
    if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_TEMP", value)) {
      BmsSetCellTemperature(ti, value / 64);
    }
    m_cell_last_ti = ti;
  }

  //
  // Handle regular PIDs
  //

  switch (pid) {
    case VWUP_BAT_MGMT_U:
      if (PollReply.FromUint16("VWUP_BAT_MGMT_U", value)) {
        StdMetrics.ms_v_bat_voltage->SetValue(value / 4.0f);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_U=%f => %f", value, StdMetrics.ms_v_bat_voltage->AsFloat());
      }
      break;

    case VWUP_BAT_MGMT_I:
      if (PollReply.FromUint16("VWUP_BAT_MGMT_I", value)) {
        // ECU delivers negative current when it goes out of the battery. OVMS wants positive when the battery outputs current.
        StdMetrics.ms_v_bat_current->SetValue(((value - 2044.0f) / 4.0f) * -1.0f);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_I=%f => %f", value, StdMetrics.ms_v_bat_current->AsFloat());

        value = StdMetrics.ms_v_bat_voltage->AsFloat() * StdMetrics.ms_v_bat_current->AsFloat() / 1000.0f;
        StdMetrics.ms_v_bat_power->SetValue(value);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_POWER=%f => %f", value, StdMetrics.ms_v_bat_power->AsFloat());
      }
      break;

    case VWUP_MOT_ELEC_SOC_NORM:
      // Gets updates while driving
      if (PollReply.FromUint16("VWUP_MOT_ELEC_SOC_NORM", value)) {
        StdMetrics.ms_v_bat_soc->SetValue(value / 100.0f);
        MotElecSoCNorm->SetValue(value / 100.0f);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_SOC_NORM=%f => %f", value, StdMetrics.ms_v_bat_soc->AsFloat());
        // Update range:
        StandardMetrics.ms_v_bat_range_ideal->SetValue(
          StdMetrics.ms_v_bat_range_full->AsFloat() * (StdMetrics.ms_v_bat_soc->AsFloat() / 100));
      }
      break;

    case VWUP_CHG_MGMT_SOC_NORM:
      // Gets updates while charging
      if (PollReply.FromUint8("VWUP_CHG_MGMT_SOC_NORM", value)) {
        float soc = value / 2.0f;
        StdMetrics.ms_v_bat_soc->SetValue(soc);
        ChgMgmtSoCNorm->SetValue(soc);
        VALUE_LOG(TAG, "VWUP_CHG_MGMT_SOC_NORM=%f => %f", value, soc);
        // Update range:
        StdMetrics.ms_v_bat_range_ideal->SetValue(
          StdMetrics.ms_v_bat_range_full->AsFloat() * (soc / 100));
        if (HasNoT26()) {
          // Calculate estimated range from last known factor:
          StdMetrics.ms_v_bat_range_est->SetValue(soc * m_range_est_factor);
        }
      }
      break;

    case VWUP_MFD_RANGE_DSP:
      // Gets updates while driving
      if (PollReply.FromUint16("VWUP_MFD_RANGE_DSP", value)) {
        StdMetrics.ms_v_bat_range_est->SetValue(value);
        VALUE_LOG(TAG, "VWUP_MFD_RANGE_DSP=%f", value);
        // Update range factor for calculation during charge:
        float soc = StdMetrics.ms_v_bat_soc->AsFloat();
        if (value > 10 && soc > 10) {
          float range_factor = value / soc;
          if (range_factor > 0.1) {
            m_range_est_factor = range_factor;
          }
        }
      }
      break;

    case VWUP_MFD_RANGE_CAP:
      if (PollReply.FromUint16("VWUP_MFD_RANGE_CAP", value)) {
        // Usable battery energy [kWh] from range estimation:
        //  We assume this to be usable as an indicator for the overall CAC & SOH,
        //  as the range estimation needs to be based on the actual (aged) battery capacity.
        //  The value may include a battery temperature compensation, so may change
        //  from summer to winter, this isn't known yet. There also may be a separate
        //  actual SOH reading available (to be discovered).
        float energy_avail = value / 10;
        float soc_fct = StdMetrics.ms_v_bat_soc->AsFloat() / 100;
        float energy_full = energy_avail / soc_fct;
        
        // Gen2: 32.3 kWh net / 36.8 kWh gross, 2P84S = 120 Ah, 260 km WLTP
        // Gen1: 16.4 kWh net / 18.7 kWh gross, 2P102S = 50 Ah, 160 km WLTP
        float cap_fct    = energy_full / ((vweup_modelyear > 2019) ?  32.3f :  16.4f);
        float cap_ah     = cap_fct     * ((vweup_modelyear > 2019) ? 120.0f :  50.0f);
        float range_full = cap_fct     * ((vweup_modelyear > 2019) ? 260.0f : 160.0f);
        
        // Update metrics:
        StdMetrics.ms_v_bat_soh->SetValue(cap_fct * 100);
        StdMetrics.ms_v_bat_cac->SetValue(cap_ah);
        StdMetrics.ms_v_bat_range_full->SetValue(range_full);
        StdMetrics.ms_v_bat_range_ideal->SetValue(range_full * soc_fct);
        VALUE_LOG(TAG, "VWUP_MFD_RANGE_CAP=%f => %.1fkWh"
          " => CAC=%.1fAh, SOH=%.1f%%, RangeFull=%.0fkm, RangeIdeal=%.0fkm",
          value, energy_avail, cap_ah, cap_fct * 100, range_full, range_full * soc_fct);
      }
      break;

    case VWUP_MOT_ELEC_SOC_ABS:
      if (PollReply.FromUint8("VWUP_MOT_ELEC_SOC_ABS", value)) {
        MotElecSoCAbs->SetValue(value / 2.55f);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_SOC_ABS=%f => %f", value, MotElecSoCAbs->AsFloat());
      }
      break;

    case VWUP_BAT_MGMT_SOC_ABS:
      if (PollReply.FromUint8("VWUP_BAT_MGMT_SOC_ABS", value)) {
        BatMgmtSoCAbs->SetValue(value / 2.5f);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_SOC_ABS=%f => %f", value, BatMgmtSoCAbs->AsFloat());
      }
      break;

    case VWUP_BAT_MGMT_ENERGY_COUNTERS:
      if (PollReply.FromInt32("VWUP_BAT_MGMT_COULOMB_COUNTERS_RECD", value, 0)) {
        float coulomb_recd_total = value / 549.2949f;
        StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(coulomb_recd_total);
        // Get trip difference:
        if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
          if (m_coulomb_recd_start <= 0)
            m_coulomb_recd_start = coulomb_recd_total;
          StdMetrics.ms_v_bat_coulomb_recd->SetValue(coulomb_recd_total - m_coulomb_recd_start);
        }
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_COULOMB_COUNTERS_RECD=%f => %f", value, coulomb_recd_total);
      }
      if (PollReply.FromInt32("VWUP_BAT_MGMT_COULOMB_COUNTERS_USED", value, 4)) {
        // Used is negative here, standard metric is positive
        float coulomb_used_total = (value * -1.0f) / 549.2949f;
        StdMetrics.ms_v_bat_coulomb_used_total->SetValue(coulomb_used_total);
        // Get trip difference:
        if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
          if (m_coulomb_used_start <= 0)
            m_coulomb_used_start = coulomb_used_total;
          StdMetrics.ms_v_bat_coulomb_used->SetValue(coulomb_used_total - m_coulomb_used_start);
        }
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_COULOMB_COUNTERS_USED=%f => %f", value, coulomb_used_total);
      }
      if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD", value, 8)) {
        float energy_recd_total = value / ((0xFFFFFFFF / 2.0f) / 250200.0f);
        StdMetrics.ms_v_bat_energy_recd_total->SetValue(energy_recd_total);
        // Get charge/trip difference:
        if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
          if (m_energy_recd_start <= 0)
            m_energy_recd_start = energy_recd_total;
          StdMetrics.ms_v_bat_energy_recd->SetValue(energy_recd_total - m_energy_recd_start);
        }
        else {
          if (m_energy_charged_start <= 0)
            m_energy_charged_start = energy_recd_total;
          StdMetrics.ms_v_charge_kwh->SetValue(energy_recd_total - m_energy_charged_start);
        }
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_RECD=%f => %f", value, energy_recd_total);
      }
      if (PollReply.FromInt32("VWUP_BAT_MGMT_ENERGY_COUNTERS_USED", value, 12)) {
        // Used is negative here, standard metric is positive
        float energy_used_total = (value * -1.0f) / ((0xFFFFFFFF / 2.0f) / 250200.0f);
        StdMetrics.ms_v_bat_energy_used_total->SetValue(energy_used_total);
        // Get trip difference:
        if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
          if (m_energy_used_start <= 0)
            m_energy_used_start = energy_used_total;
          StdMetrics.ms_v_bat_energy_used->SetValue(energy_used_total - m_energy_used_start);
        }
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_ENERGY_COUNTERS_USED=%f => %f", value, energy_used_total);
      }
      break;

    case VWUP_BAT_MGMT_CELL_MAX:
      if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_MAX", value)) {
        BatMgmtCellMax = value / 4096.0f;
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_MAX=%f => %f", value, BatMgmtCellMax);
        StdMetrics.ms_v_bat_pack_vmax->SetValue(BatMgmtCellMax);
      }
      break;

    case VWUP_BAT_MGMT_CELL_MIN:
      if (PollReply.FromUint16("VWUP_BAT_MGMT_CELL_MIN", value)) {
        BatMgmtCellMin = value / 4096.0f;
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_MIN=%f => %f", value, BatMgmtCellMin);
        StdMetrics.ms_v_bat_pack_vmin->SetValue(BatMgmtCellMin);

        value = BatMgmtCellMax - BatMgmtCellMin;
        BatMgmtCellDelta->SetValue(value);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_CELL_DELTA=%f => %f", value, BatMgmtCellDelta->AsFloat());
      }
      break;

    case VWUP_BAT_MGMT_TEMP:
      if (PollReply.FromInt16("VWUP_BAT_MGMT_TEMP", value)) {
        StdMetrics.ms_v_bat_temp->SetValue(value / 64.0f);
        VALUE_LOG(TAG, "VWUP_BAT_MGMT_TEMP=%f => %f", value, StdMetrics.ms_v_bat_temp->AsFloat());
      }
      break;

    case VWUP1_CHG_AC_U:
      if (PollReply.FromUint16("VWUP_CHG_AC1_U", value)) {
        StdMetrics.ms_v_charge_voltage->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_AC1_U=%f => %f", value, StdMetrics.ms_v_charge_voltage->AsFloat());
      }
      break;

    case VWUP1_CHG_AC_I:
      if (PollReply.FromUint8("VWUP_CHG_AC1_I", value)) {
        StdMetrics.ms_v_charge_current->SetValue(value / 10.0f);
        VALUE_LOG(TAG, "VWUP_CHG_AC1_I=%f => %f", value, StdMetrics.ms_v_charge_current->AsFloat());

        value = (StdMetrics.ms_v_charge_voltage->AsFloat() * StdMetrics.ms_v_charge_current->AsFloat()) / 1000.0f;
        ChargerACPower->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_AC_P=%f => %f", value, ChargerACPower->AsFloat());

        // Standard Charge Power and Charge Efficiency calculation as requested by the standard
        StdMetrics.ms_v_charge_power->SetValue(value);
        value = value == 0.0f
                ? 0.0f
                : ((StdMetrics.ms_v_bat_power->AsFloat() * -1.0f) / value) * 100.0f;
        StdMetrics.ms_v_charge_efficiency->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_EFF_STD=%f => %f", value, StdMetrics.ms_v_charge_efficiency->AsFloat());
      }
      break;

    case VWUP2_CHG_AC_U: {
      int phasecnt = 0;
      float voltagesum = 0;

      if (PollReply.FromUint16("VWUP_CHG_AC1_U", value)) {
        ChargerAC1U->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_AC1_U=%f => %f", value, ChargerAC1U->AsFloat());
        if (value > 90) {
          phasecnt++;
          voltagesum += value;
        }
      }
      if (PollReply.FromUint16("VWUP_CHG_AC2_U", value, 2)) {
        ChargerAC2U->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_AC2_U=%f => %f", value, ChargerAC2U->AsFloat());
        if (value > 90) {
          phasecnt++;
          voltagesum += value;
        }
      }
      if (phasecnt > 1) {
        voltagesum /= phasecnt;
      }
      StdMetrics.ms_v_charge_voltage->SetValue(voltagesum);
      break;
    }

    case VWUP2_CHG_AC_I:
      if (PollReply.FromUint8("VWUP_CHG_AC1_I", value)) {
        ChargerAC1I->SetValue(value / 10.0f);
        VALUE_LOG(TAG, "VWUP_CHG_AC1_I=%f => %f", value, ChargerAC1I->AsFloat());
      }
      if (PollReply.FromUint8("VWUP_CHG_AC2_I", value, 1)) {
        ChargerAC2I->SetValue(value / 10.0f);
        VALUE_LOG(TAG, "VWUP_CHG_AC2_I=%f => %f", value, ChargerAC2I->AsFloat());
        StdMetrics.ms_v_charge_current->SetValue(ChargerAC1I->AsFloat() + ChargerAC2I->AsFloat());

        value = (ChargerAC1U->AsFloat() * ChargerAC1I->AsFloat() +
                 ChargerAC2U->AsFloat() * ChargerAC2I->AsFloat()) / 1000.0f;
        ChargerACPower->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_AC_P=%f => %f", value, ChargerACPower->AsFloat());

        // Standard Charge Power and Charge Efficiency calculation as requested by the standard
        StdMetrics.ms_v_charge_power->SetValue(value);
        value = value == 0.0f
                ? 0.0f
                : ((StdMetrics.ms_v_bat_power->AsFloat() * -1.0f) / value) * 100.0f;
        StdMetrics.ms_v_charge_efficiency->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_EFF_STD=%f => %f", value, StdMetrics.ms_v_charge_efficiency->AsFloat());
      }
      break;

    case VWUP1_CHG_DC_U:
      if (PollReply.FromUint16("VWUP_CHG_DC_U", value)) {
        ChargerDC1U->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_DC_U=%f => %f", value, ChargerDC1U->AsFloat());
      }
      break;

    case VWUP1_CHG_DC_I:
      if (PollReply.FromUint16("VWUP_CHG_DC_I", value)) {
        ChargerDC1I->SetValue((value - 510.0f) / 5.0f);
        VALUE_LOG(TAG, "VWUP_CHG_DC_I=%f => %f", value, ChargerDC1I->AsFloat());

        value = (ChargerDC1U->AsFloat() * ChargerDC1I->AsFloat()) / 1000.0f;
        ChargerDCPower->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_DC_P=%f => %f", value, ChargerDCPower->AsFloat());

        value = ChargerACPower->AsFloat() - ChargerDCPower->AsFloat();
        ChargerPowerLossCalc->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

        value = ChargerACPower->AsFloat() > 0
                ? ChargerDCPower->AsFloat() / ChargerACPower->AsFloat() * 100.0f
                : 0.0f;
        ChargerPowerEffCalc->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
      }
      break;

    case VWUP2_CHG_DC_U:
      if (PollReply.FromUint16("VWUP_CHG_DC1_U", value)) {
        ChargerDC1U->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_DC1_U=%f => %f", value, ChargerDC1U->AsFloat());
      }
      if (PollReply.FromUint16("VWUP_CHG_DC2_U", value, 2)) {
        ChargerDC2U->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_DC2_U=%f => %f", value, ChargerDC2U->AsFloat());
      }
      break;

    case VWUP2_CHG_DC_I:
      if (PollReply.FromUint16("VWUP_CHG_DC1_I", value)) {
        ChargerDC1I->SetValue((value - 510.0f) / 5.0f);
        VALUE_LOG(TAG, "VWUP_CHG_DC1_I=%f => %f", value, ChargerDC1I->AsFloat());
      }
      if (PollReply.FromUint16("VWUP_CHG_DC2_I", value, 2)) {
        ChargerDC2I->SetValue((value - 510.0f) / 5.0f);
        VALUE_LOG(TAG, "VWUP_CHG_DC2_I=%f => %f", value, ChargerDC2I->AsFloat());

        value = (ChargerDC1U->AsFloat() * ChargerDC1I->AsFloat() +
                 ChargerDC2U->AsFloat() * ChargerDC2I->AsFloat()) / 1000.0f;
        ChargerDCPower->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_DC_P=%f => %f", value, ChargerDCPower->AsFloat());

        value = ChargerACPower->AsFloat() - ChargerDCPower->AsFloat();
        ChargerPowerLossCalc->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_LOSS_CALC=%f => %f", value, ChargerPowerLossCalc->AsFloat());

        value = ChargerACPower->AsFloat() > 0
                ? ChargerDCPower->AsFloat() / ChargerACPower->AsFloat() * 100.0f
                : 0.0f;
        ChargerPowerEffCalc->SetValue(value);
        VALUE_LOG(TAG, "VWUP_CHG_EFF_CALC=%f => %f", value, ChargerPowerEffCalc->AsFloat());
      }
      break;

    case VWUP_CHG_POWER_EFF:
      // Value is offset from 750d%. So a value > 250 would be (more) than 100% efficiency!
      // This means no charging is happening at the moment (standardvalue replied for no charging is 0xFE)
      if (PollReply.FromUint8("VWUP_CHG_POWER_EFF", value)) {
        ChargerPowerEffEcu->SetValue(value <= 250.0f ? value / 10.0f + 75.0f : 0.0f);
        VALUE_LOG(TAG, "VWUP_CHG_POWER_EFF=%f => %f", value, ChargerPowerEffEcu->AsFloat());
      }
      break;

    case VWUP_CHG_POWER_LOSS:
      if (PollReply.FromUint8("VWUP_CHG_POWER_LOSS", value)) {
        ChargerPowerLossEcu->SetValue((value * 20.0f) / 1000.0f);
        VALUE_LOG(TAG, "VWUP_CHG_POWER_LOSS=%f => %f", value, ChargerPowerLossEcu->AsFloat());
      }
      break;

    case VWUP_MOT_ELEC_SPEED:
      if (PollReply.FromUint8("VWUP_MOT_ELEC_SPEED", value)) {
        StdMetrics.ms_v_pos_speed->SetValue(value);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_SPEED=%f", value);
      }
      break;
    case VWUP_MOT_ELEC_POWER_MOT:
      if (PollReply.FromInt16("VWUP_MOT_ELEC_POWER_MOT", value)) {
        StdMetrics.ms_v_inv_power->SetValue(value / 250);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_POWER_MOT=%f => %f", value, StdMetrics.ms_v_inv_power->AsFloat());
      }
      break;
    case VWUP_MFD_ODOMETER:
      if (PollReply.FromUint16("VWUP_MFD_ODOMETER", value)) {
        float odo = value * 10.0f;
        StdMetrics.ms_v_pos_odometer->SetValue(odo);
        // Set trip reference / difference:
        if (m_odo_start <= 0)
          m_odo_start = odo;
        StdMetrics.ms_v_pos_trip->SetValue(odo - m_odo_start);
        VALUE_LOG(TAG, "VWUP_MFD_ODOMETER=%f => %f", value, odo);
      }
      break;

    case VWUP_MFD_SERV_RANGE:
      if (PollReply.FromUint16("VWUP_MFD_SERV_RANGE", value)) {
        StdMetrics.ms_v_env_service_range->SetValue(value);
        VALUE_LOG(TAG, "VWUP_MFD_SERV_RANGE=%f => %f", value, StdMetrics.ms_v_env_service_range->AsFloat());
      }
      break;
    case VWUP_MFD_SERV_TIME:
      if (PollReply.FromUint16("VWUP_MFD_SERV_TIME", value)) {
        ServiceDays -> SetValue(value);
        StdMetrics.ms_v_env_service_time->SetValue(StdMetrics.ms_m_timeutc->AsInt() + value * 86400);
        VALUE_LOG(TAG, "VWUP_MFD_SERV_TIME=%f => %f", value, StdMetrics.ms_v_env_service_time->AsFloat());
      }
      break;

    case VWUP_MOT_ELEC_TEMP_DCDC:
      if (PollReply.FromUint16("VWUP_MOT_ELEC_TEMP_DCDC", value)) {
        StdMetrics.ms_v_charge_12v_temp->SetValue(value / 10.0f - 273.1f);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_TEMP_DCDC=%f => %f", value, StdMetrics.ms_v_charge_12v_temp->AsFloat());
      }
      break;
    case VWUP_ELD_DCDC_U:
      if (PollReply.FromUint16("VWUP_ELD_DCDC_U", value)) {
        StdMetrics.ms_v_charge_12v_voltage->SetValue(value / 512.0f);
        VALUE_LOG(TAG, "VWUP_ELD_DCDC_U=%f => %f", value, StdMetrics.ms_v_charge_12v_voltage->AsFloat());
      }
      break;
    case VWUP_ELD_DCDC_I:
      if (PollReply.FromUint16("VWUP_ELD_DCDC_I", value)) {
        StdMetrics.ms_v_charge_12v_current->SetValue(value / 16.0f);
        StdMetrics.ms_v_bat_12v_current->SetValue(value / 16.0f); // until we find a separate reading
        VALUE_LOG(TAG, "VWUP_ELD_DCDC_I=%f => %f", value, StdMetrics.ms_v_charge_12v_current->AsFloat());
        StdMetrics.ms_v_charge_12v_power->SetValue(
          StdMetrics.ms_v_charge_12v_voltage->AsFloat() * StdMetrics.ms_v_charge_12v_current->AsFloat());
        VALUE_LOG(TAG, "VWUP_ELD_DCDC_P=%f => %f",
          StdMetrics.ms_v_charge_12v_power->AsFloat(), StdMetrics.ms_v_charge_12v_power->AsFloat());
      }
      break;

    case VWUP_ELD_TEMP_MOT:
      if (PollReply.FromInt16("VWUP_ELD_TEMP_MOT", value)) {
        StdMetrics.ms_v_mot_temp->SetValue(value / 64.0f);
        VALUE_LOG(TAG, "VWUP_ELD_TEMP_MOT=%f => %f", value, StdMetrics.ms_v_mot_temp->AsFloat());
      }
      break;
    case VWUP_MOT_ELEC_TEMP_PEM:
      if (PollReply.FromUint16("VWUP_MOT_ELEC_TEMP_PEM", value)) {
        StdMetrics.ms_v_inv_temp->SetValue(value / 10.0f - 273.1);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_TEMP_PEM=%f => %f", value, StdMetrics.ms_v_inv_temp->AsFloat());
      }
      break;
    case VWUP_CHG_TEMP_COOLER:
      if (PollReply.FromUint8("VWUP_CHG_TEMP_COOLER", value)) {
        StdMetrics.ms_v_charge_temp->SetValue(value - 40.0f);
        VALUE_LOG(TAG, "VWUP_CHG_TEMP_COOLER=%f => %f", value, StdMetrics.ms_v_charge_temp->AsFloat());
      }
      break;
    case VWUP_MOT_ELEC_TEMP_AMB:
      if (PollReply.FromUint8("VWUP_MOT_ELEC_TEMP_AMB", value) && value > 0 && value < 255) {
        StdMetrics.ms_v_env_temp->SetValue(value - 40.0f);
        VALUE_LOG(TAG, "VWUP_MOT_ELEC_TEMP_AMB=%f => %f", value, StdMetrics.ms_v_env_temp->AsFloat());
      }
      break;

    case VWUP_CHG_MGMT_REM:
      // Ignore charge shutdown value of 127 to keep last estimation:
      if (PollReply.FromUint8("VWUP_CHG_MGMT_REM", value) && value != 127) {
        StdMetrics.ms_v_charge_duration_full->SetValue(value * 5.0f);
        VALUE_LOG(TAG, "VWUP_CHG_MGMT_REM=%f => %f", value, StdMetrics.ms_v_charge_duration_full->AsFloat());
      }
      break;

  }
}
