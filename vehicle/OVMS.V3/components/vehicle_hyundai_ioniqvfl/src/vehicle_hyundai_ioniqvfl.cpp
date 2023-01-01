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

// Vehicle states:
#define STATE_OFF             0
#define STATE_AWAKE           1
#define STATE_DRIVING         2
#define STATE_CHARGING        3

static const OvmsVehicle::poll_pid_t standard_polls[] =
{
  //                                    OFF  AWK  DRV  CHG
  { 0x7e4, 0x7ec, UDS_READ8,    0x01, {   1,   1,   1,   1 }, 0, ISOTP_STD },  // Battery status, speed, module temperatures 1-5
  { 0x7e4, 0x7ec, UDS_READ8,    0x02, {   0,  15,  15,  15 }, 0, ISOTP_STD },  // Cell voltages 1-32
  { 0x7e4, 0x7ec, UDS_READ8,    0x03, {   0,  15,  15,  15 }, 0, ISOTP_STD },  // Cell voltages 33-64
  { 0x7e4, 0x7ec, UDS_READ8,    0x04, {   0,  15,  15,  15 }, 0, ISOTP_STD },  // Cell voltages 65-96
  { 0x7e4, 0x7ec, UDS_READ8,    0x05, {   0,  15,  15,  15 }, 0, ISOTP_STD },  // SOC, SOH, module temperatures 6-12
  { 0x7e6, 0x7ee, UDS_READ8,    0x80, {   0,  60,  60, 120 }, 0, ISOTP_STD },  // Ambient temperature
  { 0x7c6, 0x7ce, UDS_READ16, 0xb002, {   0, 120,  30,   0 }, 0, ISOTP_STD },  // Odometer
  { 0x7a0, 0x7a8, UDS_READ16, 0xc00b, {   0,  60,  60,   0 }, 0, ISOTP_STD },  // TPMS
  //                                    OFF  AWK  DRV  CHG
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

  // Init configs:
  MyConfig.RegisterParam("xhi", "Hyundai Ioniq vFL", true, true);

  // Init metrics:
  m_xhi_charge_state = MyMetrics.InitInt("xhi.c.state", 10, 0, Other, true);
  m_xhi_env_state = MyMetrics.InitInt("xhi.e.state", 10, 0, Other, true);
  m_xhi_bat_soc_bms = MyMetrics.InitFloat("xhi.b.soc.bms", 30, 0, Percentage, true);
  m_xhi_bat_range_user = MyMetrics.InitFloat("xhi.b.range.user", 120, 0, Kilometers, true);

  // Init BMS:
  BmsSetCellArrangementVoltage(96, 12);
  BmsSetCellArrangementTemperature(12, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  // Init web UI:
  MyWebServer.RegisterPage("/xhi/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xhi/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);

  // Init polling:
  PollSetThrottling(0);
  PollSetResponseSeparationTime(1);
  PollSetState(STATE_OFF);
  PollSetPidList(m_can1, standard_polls);

  // Read config:
  ConfigChanged(NULL);
}


/**
 * OvmsVehicleHyundaiVFL destructor
 */
OvmsVehicleHyundaiVFL::~OvmsVehicleHyundaiVFL()
{
  ESP_LOGI(TAG, "Shutdown Hyundai Ioniq vFL vehicle module");
  PollSetPidList(m_can1, NULL);
  MyWebServer.DeregisterPage("/xhi/battmon");
  MyWebServer.DeregisterPage("/xhi/features");
  MyMetrics.DeregisterMetric(m_xhi_charge_state);
  MyMetrics.DeregisterMetric(m_xhi_env_state);
  MyMetrics.DeregisterMetric(m_xhi_bat_soc_bms);
  MyMetrics.DeregisterMetric(m_xhi_bat_range_user);
}


/**
 * ConfigChanged: configuration init/update
 */
void OvmsVehicleHyundaiVFL::ConfigChanged(OvmsConfigParam *param)
{
  if (param && param->GetName() != "xhi") {
    return;
  }

  // Configure range estimation:

  bool recalc_ranges = false;
  int cfg_range_ideal = MyConfig.GetParamValueInt("xhi", "range.ideal", 200);
  int cfg_range_user = MyConfig.GetParamValueInt("xhi", "range.user", 200);

  m_cfg_range_smoothing = MyConfig.GetParamValueInt("xhi", "range.smoothing", 10);
  if (m_cfg_range_smoothing < 0) {
    m_cfg_range_smoothing = 0;
  }

  if (m_cfg_range_ideal != cfg_range_ideal) {
    recalc_ranges = true;
  }

  if (m_cfg_range_user != cfg_range_user) {
    // update metric on user config changes and if unset:
    if (m_cfg_range_user != 0 || m_xhi_bat_range_user->AsFloat() == 0) {
      m_xhi_bat_range_user->SetValue(cfg_range_user);
    }
    recalc_ranges = true;
  }

  m_cfg_range_ideal = cfg_range_ideal;
  m_cfg_range_user = cfg_range_user;

  if (recalc_ranges) {
    ESP_LOGD(TAG, "ConfigChanged: range ideal=%d km, user=%d km",
             m_cfg_range_ideal, m_cfg_range_user);
    CalculateRangeEstimations(true);
  }
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
      m_xhi_env_state->SetValue(RXB_BYTE(50));
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
      float speed = UnitConvert(Mph, Kph, RXB_INT16(53) / 100.0f);
      StdMetrics.ms_v_pos_speed->SetValue(fabs(speed));

      // Update trip length & energy usage:
      UpdateTripCounters();

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
      bool soc_changed = StdMetrics.ms_v_bat_soc->SetValue(bat_soc);

      // Update trip reference for SOC usage:
      if (m_drive_startsoc == 0) {
        m_drive_startsoc = bat_soc;
      }
      
      // Update range estimations only at the SOC steps (0.5%):
      if (soc_changed) {
        CalculateRangeEstimations();
      }
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

    case 0xc00b:
    {
      // Read TPMS:
      std::vector<float> tpms_pressure(4);  // kPa
      std::vector<float> tpms_temp(4);      // °C
      std::vector<short> tpms_alert(4);     // 0,1,2

      // Metric layout:   FL,FR,RL,RR
      // Car layout:      FL,FR,RR,RL

      const float kpa_factor = 100 * 0.2 / 14.504;
      tpms_pressure[0] = TRUNCPREC((float)RXB_BYTE( 4) * kpa_factor, 1);
      tpms_pressure[1] = TRUNCPREC((float)RXB_BYTE( 8) * kpa_factor, 1);
      tpms_pressure[2] = TRUNCPREC((float)RXB_BYTE(16) * kpa_factor, 1);
      tpms_pressure[3] = TRUNCPREC((float)RXB_BYTE(12) * kpa_factor, 1);

      tpms_temp[0] = (float)RXB_BYTE( 5) - 55;
      tpms_temp[1] = (float)RXB_BYTE( 9) - 55;
      tpms_temp[2] = (float)RXB_BYTE(17) - 55;
      tpms_temp[3] = (float)RXB_BYTE(13) - 55;

      // Check for warnings/alerts:
      float pressure_warn   = MyConfig.GetParamValueFloat("xhi", "tpms.pressure.warn",  230); // kPa
      float pressure_alert  = MyConfig.GetParamValueFloat("xhi", "tpms.pressure.alert", 220); // kPa
      float temp_warn       = MyConfig.GetParamValueFloat("xhi", "tpms.temp.warn",       90); // °C
      float temp_alert      = MyConfig.GetParamValueFloat("xhi", "tpms.temp.alert",     100); // °C
      for (int i = 0; i < 4; i++) {
        if (tpms_pressure[i] <= pressure_alert || tpms_temp[i] >= temp_alert)
          tpms_alert[i] = 2;
        else if (tpms_pressure[i] <= pressure_warn || tpms_temp[i] >= temp_warn)
          tpms_alert[i] = 1;
        else
          tpms_alert[i] = 0;
      }

      // Publish metrics:
      StdMetrics.ms_v_tpms_pressure->SetValue(tpms_pressure);
      StdMetrics.ms_v_tpms_temp->SetValue(tpms_temp);
      StdMetrics.ms_v_tpms_alert->SetValue(tpms_alert);
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
  int env_state = m_xhi_env_state->AsInt();

  //bool flag_mainrelay = (charge_state & 0x01);      // BMS main relay closed
  //bool flag_pluggedin = (charge_state & 0x20);      // Plugged in / charge port / pilot (?)
  bool flag_charging  = (charge_state & 0x80);      // Main battery is charging
  bool flag_ignition  = (env_state & 0x04);         // Ignition switched on

  // Determine new polling state:
  int poll_state;
  if (!car_online)
    poll_state = STATE_OFF;
  else if (flag_charging)
    poll_state = STATE_CHARGING;
  else if (flag_ignition)
    poll_state = STATE_DRIVING;
  else
    poll_state = STATE_AWAKE;

  // Set base state flags:
  // TODO: find real indicators for charging12v & awake/on
  StdMetrics.ms_v_env_aux12v->SetValue(car_online);                   // base system awake
  StdMetrics.ms_v_env_charging12v->SetValue(car_online);              // charging the 12V battery
  StdMetrics.ms_v_env_awake->SetValue(poll_state == STATE_DRIVING);   // fully awake (switched on)
  StdMetrics.ms_v_env_on->SetValue(poll_state == STATE_DRIVING);      // ignition on

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

  if (poll_state == STATE_DRIVING)
  {
    if (m_poll_state != STATE_DRIVING) {
      // Ignition switched on:
      StdMetrics.ms_v_door_chargeport->SetValue(false);
      StdMetrics.ms_v_charge_substate->SetValue("");
      StdMetrics.ms_v_charge_state->SetValue("");
      PollSetState(STATE_DRIVING);

      // Start new trip:
      ResetTripCounters();
    }
  }

  if (poll_state == STATE_AWAKE)
  {
    if (m_poll_state != STATE_AWAKE) {
      // Just entered awake state, nothing to do here for now
      PollSetState(STATE_AWAKE);
    }
  }

  if (poll_state == STATE_OFF)
  {
    if (m_poll_state != STATE_OFF) {
      // Switched off:
      StdMetrics.ms_v_bat_current->SetValue(0);
      StdMetrics.ms_v_bat_power->SetValue(0);
      PollSetState(STATE_OFF);

      // Remember new user range?
      int range_user = m_xhi_bat_range_user->AsInt();
      if (ABS(m_cfg_range_user - range_user) >= 5) {
        ESP_LOGD(TAG, "PollerStateTicker: save new user range=%d km", range_user);
        m_cfg_range_user = range_user;
        MyConfig.SetParamValueInt("xhi", "range.user", range_user);
      }
    }
  }
}


/**
 * ResetTripCounters: called at trip start to set reference points
 */
void OvmsVehicleHyundaiVFL::ResetTripCounters()
{
  StdMetrics.ms_v_pos_trip->SetValue(0);
  m_odo_trip            = 0;
  m_tripfrac_reftime    = esp_log_timestamp();
  m_tripfrac_refspeed   = StdMetrics.ms_v_pos_speed->AsFloat();

  StdMetrics.ms_v_bat_energy_recd->SetValue(0);
  StdMetrics.ms_v_bat_energy_used->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_recd->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_used->SetValue(0);

  m_energy_recd_start   = 0;
  m_energy_used_start   = 0;
  m_coulomb_recd_start  = 0;
  m_coulomb_used_start  = 0;
}


/**
 * UpdateTripCounters: odometer resolution is only 1 km, so trip distances lack
 *  precision. To compensate, this method derives trip distance from speed.
 */
void OvmsVehicleHyundaiVFL::UpdateTripCounters()
{
  // Process speed update:

  uint32_t now = esp_log_timestamp();
  float speed = StdMetrics.ms_v_pos_speed->AsFloat();

  if (m_tripfrac_reftime && now > m_tripfrac_reftime) {
    float speed_avg = ABS(speed + m_tripfrac_refspeed) / 2;
    uint32_t time_ms = now - m_tripfrac_reftime;
    double meters = speed_avg / 3.6 * time_ms / 1000;
    m_odo_trip += meters / 1000;
    StdMetrics.ms_v_pos_trip->SetValue(TRUNCPREC(m_odo_trip,3));
  }

  m_tripfrac_reftime = now;
  m_tripfrac_refspeed = speed;

  // Process energy update:

  float energy_recd  = StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
  float energy_used  = StdMetrics.ms_v_bat_energy_used_total->AsFloat();
  float coulomb_recd = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat();
  float coulomb_used = StdMetrics.ms_v_bat_coulomb_used_total->AsFloat();

  if (m_coulomb_used_start == 0) {
    m_energy_recd_start  = energy_recd;
    m_energy_used_start  = energy_used;
    m_coulomb_recd_start = coulomb_recd;
    m_coulomb_used_start = coulomb_used;
  } else {
    StdMetrics.ms_v_bat_energy_recd->SetValue(energy_recd - m_energy_recd_start);
    StdMetrics.ms_v_bat_energy_used->SetValue(energy_used - m_energy_used_start);
    StdMetrics.ms_v_bat_coulomb_recd->SetValue(coulomb_recd - m_coulomb_recd_start);
    StdMetrics.ms_v_bat_coulomb_used->SetValue(coulomb_used - m_coulomb_used_start);
  }
}


/**
 * CalculateRangeEstimations: calculate ranges from SOH, SOC & driving metrics
 *  -- called by IncomingPollReply() on PID 0x05 (SOC,SOH) = every 15 seconds
 */
void OvmsVehicleHyundaiVFL::CalculateRangeEstimations(bool init /*=false*/)
{
  // Get battery state:
  float soc = StdMetrics.ms_v_bat_soc->AsFloat(), socfactor = soc / 100;
  float soh = StdMetrics.ms_v_bat_soh->AsFloat(), sohfactor = soh / 100;
  if (sohfactor == 0) {
    sohfactor = 1;
    StdMetrics.ms_v_bat_soh->SetValue(100);
  }

  // Get user range:
  float range_user = m_xhi_bat_range_user->AsFloat();

  // While driving, update user range estimation from trip metrics:
  if (!init && m_poll_state == STATE_DRIVING) {
    float trip_soc_used = m_drive_startsoc - soc;
    if (trip_soc_used >= 1 && m_odo_trip >= 1) {
      float range_trip = m_odo_trip / trip_soc_used * 100;
      range_user = (range_user * m_cfg_range_smoothing + range_trip) / (m_cfg_range_smoothing + 1);
      ESP_LOGD(TAG, "CalculateRangeEstimations: trip %.2f km / %.1f %%SOC -> range=%.2f smoothing(%d) -> user=%.2f km",
               m_odo_trip, trip_soc_used, range_trip, m_cfg_range_smoothing, range_user);
      m_xhi_bat_range_user->SetValue(TRUNCPREC(range_user,3));
    }
  }

  // Calculate ranges:
  StdMetrics.ms_v_bat_range_full->SetValue(m_cfg_range_ideal * sohfactor);
  StdMetrics.ms_v_bat_range_ideal->SetValue(m_cfg_range_ideal * sohfactor * socfactor);
  StdMetrics.ms_v_bat_range_est->SetValue(range_user * socfactor);
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
