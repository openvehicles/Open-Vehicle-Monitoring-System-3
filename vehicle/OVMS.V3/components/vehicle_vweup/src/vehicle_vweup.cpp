/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX

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

/*
;    Subproject:    Integration of support for the VW e-UP
;    Date:          30 December 2020
;
;    Changes:
;    0.1.0  Initial code
:           Crude merge of code from Chris van der Meijden (KCAN) and SokoFromNZ (OBD2)
;
;    0.1.1  make OBD code depend on car status from KCAN, no more OBD polling in off state
;
;    0.1.2  common namespace, metrics webinterface
;
;    0.1.3  bugfixes gen1/gen2, new metrics: temperatures & maintenance
;
;    0.1.4  bugfixes gen1/gen2, OBD refactoring
;
;    0.1.5  refactoring
;
;    0.1.6  Standard SoC now from OBD (more resolution), switch between car on and charging
;
;    0.2.0  refactoring: this is now plain VWEUP-module, KCAN-version moved to VWEUP_T26
;
;    0.2.1  SoC from OBD/KCAN depending on config & state
;
;    0.3.0  car state determined depending on connection
;
;    0.3.1  added charger standard metrics
;
;    0.3.2  refactoring; init CAN buses depending on connection; transmit charge current as float (server V2)
;
;    0.3.3  new standard metrics for maintenance range & days; added trip distance & energies as deltas
;
;    0.4.0  update changes in t26 code by devmarxx, update docs
;
;    (C) 2020 sharkcow <sharkcow@gmx.de> / Chris van der Meijden / SokoFromNZ
;
;    Biggest thanks to Dexter, Dimitrie78, E-Imo and 'der kleine Nik'.
*/

#include "ovms_log.h"
#include <string>
static const char *TAG = "v-vweup";

#define VERSION "0.10.2"

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

  m_obd_state = OBDS_Init;

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

  // Init T26 subsystem:
  if (vweup_enable_t26) {
    T26Init();
  }

  // Init OBD subsystem:
  if (vweup_enable_obd && do_obd_init) {
    OBDInit();
  }
  else if (!vweup_enable_obd) {
    OBDDeInit();
  }

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // Init Web subsystem:
  WebDeInit();    // this can probably be done more elegantly... :-/
  WebInit();
#endif
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
  m_odo_start           = StdMetrics.ms_v_pos_odometer->AsFloat();
  m_soc_norm_start      = StdMetrics.ms_v_bat_soc->AsFloat();
  m_soc_abs_start       = BatMgmtSoCAbs->AsFloat();
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
  m_soc_norm_start        = StdMetrics.ms_v_bat_soc->AsFloat();
  m_soc_abs_start         = BatMgmtSoCAbs->AsFloat();
  m_energy_charged_start  = StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
  m_coulomb_charged_start = StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat();
  m_charge_kwh_grid_start = StdMetrics.ms_v_charge_kwh_grid_total->AsFloat();

  ESP_LOGD(TAG, "Charge start ref: socnrm=%f socabs=%f cr=%f er=%f gr=%f",
    m_soc_norm_start, m_soc_abs_start, m_coulomb_charged_start,
    m_energy_charged_start, m_charge_kwh_grid_start);
}
