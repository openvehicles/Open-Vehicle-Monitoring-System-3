/*
; Project:        Open Vehicle Monitor System
; Subproject:     Integration of support for the VW e-UP
;
; (c) 2021 sharkcow <sharkcow@gmx.de>, Chris van der Meijden, SokoFromNZ, Michael Balzer <dexter@dexters-web.de>
;
; Biggest thanks to Dimitrie78, E-Imo and 'der kleine Nik'.
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
#include <string>
static const char *TAG = "v-vweup";

#define VERSION "0.15.1"

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

#include "vehicle_vweup.h"

using namespace std;

/**
 * Framework registration
 */

class OvmsVehicleVWeUpInit
{
public:
  OvmsVehicleVWeUpInit();
} OvmsVehicleVWeUpInit __attribute__((init_priority(9000)));

OvmsVehicleVWeUpInit::OvmsVehicleVWeUpInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: VW e-Up (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUp>("VWUP", "VW e-Up");
}


OvmsVehicleVWeUp *OvmsVehicleVWeUp::GetInstance(OvmsWriter *writer)
{
  OvmsVehicleVWeUp *eup = (OvmsVehicleVWeUp *) MyVehicleFactory.ActiveVehicle();

  string type = StdMetrics.ms_v_type->AsString();
  if (!eup || type != "VWUP") {
    if (writer) {
      writer->puts("Error: VW e-Up vehicle module not selected");
    }
    return NULL;
  }
  return eup;
}


/**
 * Constructor & destructor
 */

//size_t OvmsVehicleVWeUp::m_modifier = 0;

OvmsVehicleVWeUp::OvmsVehicleVWeUp()
{
  ESP_LOGI(TAG, "Start VW e-Up vehicle module");

  // Init general state:
  vweup_enable_write = false;
  vweup_enable_obd = false;
  vweup_enable_t26 = false;
  vweup_con = 0;
  vweup_modelyear = 0;

  m_use_phase = UP_None;
  m_obd_state = OBDS_Init;
  m_chargestop_ticker = 0;

  // Init metrics:
  m_version = MyMetrics.InitString("xvu.m.version", 0, VERSION " " __DATE__ " " __TIME__);

  // init configs:
  MyConfig.RegisterParam("xvu", "VW e-Up", true, true);

  ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
}

OvmsVehicleVWeUp::~OvmsVehicleVWeUp()
{
  ESP_LOGI(TAG, "Stop VW e-Up vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif

  // delete metrics:
  MyMetrics.DeregisterMetric(m_version);
  // TODO: delete all xvu metrics
}

/*
const char* OvmsVehicleVWeUp::VehicleShortName()
{
  return "e-Up";
}
*/

bool OvmsVehicleVWeUp::SetFeature(int key, const char *value)
{
  int i;
  int n;
  switch (key) {
    case 15: {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xvu", "canwrite", (bits & 1) != 0);
      return true;
    }
    case 20:
      // check:
      if (strlen(value) == 0) {
        value = "2020";
      }
      for (i = 0; i < strlen(value); i++) {
        if (isdigit(value[i]) == false) {
          value = "2020";
          break;
        }
      }
      n = atoi(value);
      if (n < 2013) {
        value = "2013";
      }
      MyConfig.SetParamValue("xvu", "modelyear", value);
      return true;
    case 21:
      // check:
      if (strlen(value) == 0) {
        value = "22";
      }
      for (i = 0; i < strlen(value); i++) {
        if (isdigit(value[i]) == false) {
          value = "22";
          break;
        }
      }
      n = atoi(value);
      if (n < 15) {
        value = "15";
      }
      if (n > 30) {
        value = "30";
      }
      MyConfig.SetParamValue("xvu", "cc_temp", value);
      return true;
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}

const std::string OvmsVehicleVWeUp::GetFeature(int key)
{
  switch (key) {
    case 15: {
      int bits = (MyConfig.GetParamValueBool("xvu", "canwrite", false) ? 1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 20:
      return MyConfig.GetParamValue("xvu", "modelyear", STR(DEFAULT_MODEL_YEAR));
    case 21:
      return MyConfig.GetParamValue("xvu", "cc_temp", STR(21));
    default:
      return OvmsVehicle::GetFeature(key);
  }
}

void OvmsVehicleVWeUp::ConfigChanged(OvmsConfigParam *param)
{
  if (param && param->GetName() != "xvu") {
    return;
  }

  ESP_LOGD(TAG, "VW e-Up reload configuration");
  int vweup_modelyear_new = MyConfig.GetParamValueInt("xvu", "modelyear", DEFAULT_MODEL_YEAR);
  bool vweup_enable_obd_new = MyConfig.GetParamValueBool("xvu", "con_obd", true);
  bool vweup_enable_t26_new = MyConfig.GetParamValueBool("xvu", "con_t26", true);
  vweup_enable_write = MyConfig.GetParamValueBool("xvu", "canwrite", false);
  vweup_cc_temp_int = MyConfig.GetParamValueInt("xvu", "cc_temp", 22);
  int dc_interval = MyConfig.GetParamValueInt("xvu", "dc_interval", 0);
  int cell_interval_drv = MyConfig.GetParamValueInt("xvu", "cell_interval_drv", 15);
  int cell_interval_chg = MyConfig.GetParamValueInt("xvu", "cell_interval_chg", 60);
  int cell_interval_awk = MyConfig.GetParamValueInt("xvu", "cell_interval_awk", 60);

  bool do_obd_init = (
    (!vweup_enable_obd && vweup_enable_obd_new) ||
    (vweup_enable_t26_new != vweup_enable_t26) ||
    (vweup_modelyear < 2020 && vweup_modelyear_new > 2019) ||
    (vweup_modelyear_new < 2020 && vweup_modelyear > 2019) ||
    (dc_interval != m_cfg_dc_interval) ||
    (cell_interval_drv != m_cfg_cell_interval_drv) ||
    (cell_interval_chg != m_cfg_cell_interval_chg) ||
    (cell_interval_awk != m_cfg_cell_interval_awk));

  vweup_modelyear = vweup_modelyear_new;
  vweup_enable_obd = vweup_enable_obd_new;
  vweup_enable_t26 = vweup_enable_t26_new;
  m_cfg_dc_interval = dc_interval;
  m_cfg_cell_interval_drv = cell_interval_drv;
  m_cfg_cell_interval_chg = cell_interval_chg;
  m_cfg_cell_interval_awk = cell_interval_awk;

  // Connectors:
  vweup_con = vweup_enable_obd * 2 + vweup_enable_t26;
  if (!vweup_con) {
    ESP_LOGW(TAG, "Module will not work without any connection!");
  }

  // Set model specific general vehicle properties:
  //  Note: currently using standard specs
  //  TODO: get actual capacity/SOH & max charge current
  float socfactor = StdMetrics.ms_v_bat_soc->AsFloat() / 100;
  float sohfactor = StdMetrics.ms_v_bat_soh->AsFloat() / 100;
  if (sohfactor == 0) sohfactor = 1;
  if (vweup_modelyear > 2019)
  {
    // 32.3 kWh net / 36.8 kWh gross, 2P84S = 120 Ah, 260 km WLTP
    if (StdMetrics.ms_v_bat_cac->AsFloat() == 0)
      StdMetrics.ms_v_bat_cac->SetValue(120 * sohfactor);
    StdMetrics.ms_v_bat_range_full->SetValue(260 * sohfactor);
    if (StdMetrics.ms_v_bat_range_ideal->AsFloat() == 0)
      StdMetrics.ms_v_bat_range_ideal->SetValue(260 * sohfactor * socfactor);
    if (StdMetrics.ms_v_bat_range_est->AsFloat() > 10 && StdMetrics.ms_v_bat_soc->AsFloat() > 10)
      m_range_est_factor = StdMetrics.ms_v_bat_range_est->AsFloat() / StdMetrics.ms_v_bat_soc->AsFloat();
    else
      m_range_est_factor = 2.6f;
    StdMetrics.ms_v_charge_climit->SetValue(32);

    // Battery pack layout: 2P84S in 14 modules
    BmsSetCellArrangementVoltage(84, 6);
    BmsSetCellArrangementTemperature(14, 1);
    BmsSetCellLimitsVoltage(2.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
  }
  else
  {
    // 16.4 kWh net / 18.7 kWh gross, 2P102S = 50 Ah, 160 km WLTP
    if (StdMetrics.ms_v_bat_cac->AsFloat() == 0)
      StdMetrics.ms_v_bat_cac->SetValue(50 * sohfactor);
    StdMetrics.ms_v_bat_range_full->SetValue(160 * sohfactor);
    if (StdMetrics.ms_v_bat_range_ideal->AsFloat() == 0)
      StdMetrics.ms_v_bat_range_ideal->SetValue(160 * sohfactor * socfactor);
    if (StdMetrics.ms_v_bat_range_est->AsFloat() > 10 && StdMetrics.ms_v_bat_soc->AsFloat() > 10)
      m_range_est_factor = StdMetrics.ms_v_bat_range_est->AsFloat() / StdMetrics.ms_v_bat_soc->AsFloat();
    else
      m_range_est_factor = 1.6f;
    StdMetrics.ms_v_charge_climit->SetValue(16);

    // Battery pack layout: 2P102S in 17 modules
    BmsSetCellArrangementVoltage(102, 6);
    BmsSetCellArrangementTemperature(17, 1);
    BmsSetCellLimitsVoltage(2.0, 5.0);
    BmsSetCellLimitsTemperature(-39, 200);
    BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
    BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
  }

  // Init OBD subsystem:
  // (needs to be initialized first as the T26 module depends on OBD being ready)
  if (vweup_enable_obd && do_obd_init) {
    OBDInit();
  }
  else if (!vweup_enable_obd) {
    OBDDeInit();
  }

  // Init T26 subsystem:
  if (vweup_enable_t26) {
    T26Init();
  }

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // Init Web subsystem:
  WebDeInit();    // this can probably be done more elegantly... :-/
  WebInit();
#endif

  // Set standard SOH from configured source:
  if (IsOBDReady())
  {
    std::string soh_source = MyConfig.GetParamValue("xvu", "bat.soh.source", "charge");
    if (soh_source == "range" && m_bat_soh_range->IsDefined())
      SetSOH(m_bat_soh_range->AsFloat());
    else if (soh_source == "charge" && m_bat_soh_charge->IsDefined())
      SetSOH(m_bat_soh_charge->AsFloat());
  }
}


/**
 * MetricModified: hook into general listener for metrics changes
 */
void OvmsVehicleVWeUp::MetricModified(OvmsMetric* metric)
{
  // If one of our SOH sources got updated, derive standard SOH, CAC and ranges from it
  // if it's the configured SOH source:
  if (metric == m_bat_soh_range || metric == m_bat_soh_charge)
  {
    // Check SOH source configuration:
    float soh_new = 0;
    std::string soh_source = MyConfig.GetParamValue("xvu", "bat.soh.source", "charge");
    if (metric == m_bat_soh_range && soh_source == "range")
      soh_new = metric->AsFloat();
    else if (metric == m_bat_soh_charge && soh_source == "charge")
      soh_new = metric->AsFloat();

    // Update metrics:
    if (soh_new)
      SetSOH(soh_new);
  }

  // Pass update on to standard handler:
  OvmsVehicle::MetricModified(metric);
}


/**
 * SetSOH: set SOH, derive standard SOH, CAC and ranges
 */
void OvmsVehicleVWeUp::SetSOH(float soh_new)
{
  float soh_fct    = soh_new / 100;
  float cap_ah     = soh_fct * ((vweup_modelyear > 2019) ? 120.0f :  50.0f);
  float range_full = soh_fct * ((vweup_modelyear > 2019) ? 260.0f : 160.0f);
  float soc_fct    = StdMetrics.ms_v_bat_soc->AsFloat() / 100;
  StdMetrics.ms_v_bat_soh->SetValue(soh_new);
  StdMetrics.ms_v_bat_cac->SetValue(cap_ah);
  StdMetrics.ms_v_bat_range_full->SetValue(range_full);
  StdMetrics.ms_v_bat_range_ideal->SetValue(range_full * soc_fct);
}


void OvmsVehicleVWeUp::Ticker1(uint32_t ticker)
{
  if (HasT26()) {
    T26Ticker1(ticker);
  }
}


/**
 * GetNotifyChargeStateDelay: framework hook
 */
int OvmsVehicleVWeUp::GetNotifyChargeStateDelay(const char *state)
{
  // With OBD data, wait for first voltage & current when starting the charge:
  if (HasOBD() && strcmp(state, "charging") == 0) {
    return 6;
  }
  else {
    return 3;
  }
}


/**
 * SetUsePhase: track phase transitions between charging & driving
 */
void OvmsVehicleVWeUp::SetUsePhase(use_phase_t use_phase)
{
  if (m_use_phase == use_phase)
    return;

  // Phase transition: reset BMS statistics?
  if (MyConfig.GetParamValueBool("xvu", "bms.autoreset")) {
    ESP_LOGD(TAG, "SetUsePhase %d: resetting BMS statistics", use_phase);
    BmsResetCellStats();
  }

  m_use_phase = use_phase;
}


/**
 * ResetTripCounters: called at trip start to set reference points
 *  Called by the connector subsystem detecting vehicle state changes,
 *  i.e. T26 has priority if available.
 */
void OvmsVehicleVWeUp::ResetTripCounters()
{
  // Clear per trip counters:
  StdMetrics.ms_v_pos_trip->SetValue(0);
  StdMetrics.ms_v_bat_energy_recd->SetValue(0);
  StdMetrics.ms_v_bat_energy_used->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_recd->SetValue(0);
  StdMetrics.ms_v_bat_coulomb_used->SetValue(0);

  // Get trip start references as far as available:
  //  (if we don't have them yet, IncomingPollReply() will set them ASAP)
  if (IsOBDReady()) {
    m_soc_abs_start       = BatMgmtSoCAbs->AsFloat();
  }
  m_odo_start           = StdMetrics.ms_v_pos_odometer->AsFloat();
  m_soc_norm_start      = StdMetrics.ms_v_bat_soc->AsFloat();
  m_energy_recd_start   = StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
  m_energy_used_start   = StdMetrics.ms_v_bat_energy_used_total->AsFloat();
  m_coulomb_recd_start  = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat();
  m_coulomb_used_start  = StdMetrics.ms_v_bat_coulomb_used_total->AsFloat();

  ESP_LOGD(TAG, "Trip start ref: socnrm=%f socabs=%f odo=%f, er=%f, eu=%f, cr=%f, cu=%f",
    m_soc_norm_start, m_soc_abs_start, m_odo_start,
    m_energy_recd_start, m_energy_used_start, m_coulomb_recd_start, m_coulomb_used_start);
}


/**
 * ResetChargeCounters: call at charge start to set reference points
 *  Called by the connector subsystem detecting vehicle state changes,
 *  i.e. T26 has priority if available.
 */
void OvmsVehicleVWeUp::ResetChargeCounters()
{
  // Clear per charge counter:
  StdMetrics.ms_v_charge_kwh->SetValue(0);
  StdMetrics.ms_v_charge_kwh_grid->SetValue(0);
  m_charge_kwh_grid = 0;

  // Get charge start reference as far as available:
  //  (if we don't have it yet, IncomingPollReply() will set it ASAP)
  if (IsOBDReady()) {
    m_soc_abs_start         = BatMgmtSoCAbs->AsFloat();
  }
  m_soc_norm_start        = StdMetrics.ms_v_bat_soc->AsFloat();
  m_energy_charged_start  = StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
  m_coulomb_charged_start = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat();
  m_charge_kwh_grid_start = StdMetrics.ms_v_charge_kwh_grid_total->AsFloat();

  ESP_LOGD(TAG, "Charge start ref: socnrm=%f socabs=%f cr=%f er=%f gr=%f",
    m_soc_norm_start, m_soc_abs_start, m_coulomb_charged_start,
    m_energy_charged_start, m_charge_kwh_grid_start);
}


/**
 * SetChargeType: set current internal & public charge type (AC / DC / None)
 *  Controlled by the OBD handler if enabled (derived from VWUP_CHG_MGMT_HV_CHGMODE).
 *  The charge type defines the source for the charge metrics, to query the type
 *  use IsChargeModeAC() and IsChargeModeDC().
 */
void OvmsVehicleVWeUp::SetChargeType(chg_type_t chgtype)
{
  if (m_chg_type == chgtype)
    return;

  m_chg_type = chgtype;

  if (m_chg_type == CHGTYPE_AC) {
    StdMetrics.ms_v_charge_type->SetValue("type2");
  }
  else if (m_chg_type == CHGTYPE_DC) {
    StdMetrics.ms_v_charge_type->SetValue("ccs");
  }
  else {
    StdMetrics.ms_v_charge_type->SetValue("");
    // …and clear/reset charge metrics:
    if (IsOBDReady()) {
      ChargerPowerEffEcu->SetValue(100);
      ChargerPowerLossEcu->SetValue(0);
      ChargerPowerEffCalc->SetValue(100);
      ChargerPowerLossCalc->SetValue(0);
      ChargerAC1U->SetValue(0);
      ChargerAC1I->SetValue(0);
      ChargerAC2U->SetValue(0);
      ChargerAC2I->SetValue(0);
      ChargerACPower->SetValue(0);
      ChargerDC1U->SetValue(0);
      ChargerDC1I->SetValue(0);
      ChargerDC2U->SetValue(0);
      ChargerDC2I->SetValue(0);
      ChargerDCPower->SetValue(0);
      m_chg_ccs_voltage->SetValue(0);
      m_chg_ccs_current->SetValue(0);
      m_chg_ccs_power->SetValue(0);
    }
    StdMetrics.ms_v_charge_voltage->SetValue(0);
    StdMetrics.ms_v_charge_current->SetValue(0);
    StdMetrics.ms_v_charge_power->SetValue(0);
    StdMetrics.ms_v_charge_efficiency->SetValue(0);
  }
}


/**
 * SetChargeState: set v.c.charging, v.c.state and v.c.substate according to current
 *  charge timer mode, limits and SOC
 *  Note: changing v.c.state triggers the notification, so this should be called last.
 */
void OvmsVehicleVWeUp::SetChargeState(bool charging)
{
  if (charging)
  {
    // Charge in progress:
    StdMetrics.ms_v_charge_inprogress->SetValue(true);

    if (StdMetrics.ms_v_charge_timermode->AsBool())
      StdMetrics.ms_v_charge_substate->SetValue("scheduledstart");
    else
      StdMetrics.ms_v_charge_substate->SetValue("onrequest");

    StdMetrics.ms_v_charge_state->SetValue("charging");
  }
  else
  {
    // Charge stopped:
    StdMetrics.ms_v_charge_inprogress->SetValue(false);

    int soc = StdMetrics.ms_v_bat_soc->AsInt();

    if (IsOBDReady() && StdMetrics.ms_v_charge_timermode->AsBool())
    {
      // Scheduled charge;
      int socmin = m_chg_timer_socmin->AsInt();
      int socmax = m_chg_timer_socmax->AsInt();
      // if stopped at maximum SOC, we've finished as scheduled:
      if (soc >= socmax-1 && soc <= socmax+1) {
        StdMetrics.ms_v_charge_substate->SetValue("scheduledstop");
        StdMetrics.ms_v_charge_state->SetValue("done");
      }
      // …if stopped at minimum SOC, we're waiting for the second phase:
      else if (soc >= socmin-1 && soc <= socmin+1) {
        StdMetrics.ms_v_charge_substate->SetValue("timerwait");
        StdMetrics.ms_v_charge_state->SetValue("stopped");
      }
      // …else the charge has been interrupted:
      else {
        StdMetrics.ms_v_charge_substate->SetValue("interrupted");
        StdMetrics.ms_v_charge_state->SetValue("stopped");
      }
    }
    else
    {
      // Unscheduled charge; done if fully charged:
      if (soc >= 99) {
        StdMetrics.ms_v_charge_substate->SetValue("stopped");
        StdMetrics.ms_v_charge_state->SetValue("done");
      }
      // …else the charge has been interrupted:
      else {
        StdMetrics.ms_v_charge_substate->SetValue("interrupted");
        StdMetrics.ms_v_charge_state->SetValue("stopped");
      }
    }
  }
}
