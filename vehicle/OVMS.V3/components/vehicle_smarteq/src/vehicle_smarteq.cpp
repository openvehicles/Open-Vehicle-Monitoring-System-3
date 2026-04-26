/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; https://github.com/MyLab-odyssey/ED4scan
 */
 
static const char *TAG = "v-smarteq";

#include "vehicle_smarteq.h"

OvmsVehicleSmartEQ* OvmsVehicleSmartEQ::GetInstance(OvmsWriter* writer)
{
  OvmsVehicleSmartEQ* smarteq = (OvmsVehicleSmartEQ*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!smarteq || type != "SQ") 
    {
    if (writer)
      writer->puts("Error: smart EQ vehicle module not selected");
    return NULL;
    }
  return smarteq;
}

/**
 * Constructor & destructor
 */

OvmsVehicleSmartEQ::OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Start smart EQ vehicle module");

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 1);               // 96 cells, 1 series string
  BmsSetCellArrangementTemperature(27, 1);           // 27 temp sensors, 1 series string
  BmsSetCellLimitsVoltage(3.0, 4.5);                 // Min 3.0V, Max 4.5V
  BmsSetCellLimitsTemperature(-39, 60);              // Min -39°C, Max 60°C
  BmsSetCellDefaultThresholdsVoltage(0.040, 0.060);  // Warn: 40mV, Alert: 60mV
  BmsSetCellDefaultThresholdsTemperature(4.0, 5.5);  // Warn: 4,0°C, Alert: 5,5°C

  mt_bus_awake                  = MyMetrics.InitBool("xsq.v.bus.awake", SM_STALE_MIN, true);
  mt_canbyte                    = MyMetrics.InitString("xsq.ddt4all.canbyte", SM_STALE_MAX, "", Other);
  mt_adc_factor                 = MyMetrics.InitFloat("xsq.adc.factor", SM_STALE_MAX, 0, Other);
  mt_adc_factor_history         = MyMetrics.InitVector<float>("xsq.adc.factor.history", SM_STALE_MAX, nullptr, Other);
  mt_adc_factor_history->SetElemValue(m_adc_samples -1, 0.0f);  // Pre-allocate x samples to avoid reallocs
  mt_poll_state                 = MyMetrics.InitString("xsq.poll.state", SM_STALE_MAX, "UNKNOWN", Other);  
  mt_ed4_values                 = MyMetrics.InitInt("xsq.ed4.values", SM_STALE_MAX, 10);

  mt_start_time                 = MyMetrics.InitString("xsq.v.start.time", SM_STALE_MID, 0, Other);
  mt_start_distance             = MyMetrics.InitFloat("xsq.v.start.distance", SM_STALE_MID, 0, Kilometers);

  mt_reset_time                 = MyMetrics.InitString("xsq.v.reset.time", SM_STALE_MID, 0, Other);
  mt_reset_consumption          = MyMetrics.InitFloat("xsq.v.reset.consumption", SM_STALE_MID, 0, kWhP100K);
  mt_reset_distance             = MyMetrics.InitFloat("xsq.v.reset.distance", SM_STALE_MID, 0, Kilometers);
  mt_reset_energy               = MyMetrics.InitFloat("xsq.v.reset.energy", SM_STALE_MID, 0, kWh);
  mt_reset_speed                = MyMetrics.InitFloat("xsq.v.reset.speed", SM_STALE_MID, 0, Kph);
  // 0x658 metrics
  mt_bat_serial                 = MyMetrics.InitString("xsq.v.bat.serial", SM_STALE_MAX, "");
    // BMS production data (PID 0x9000)
  mt_bms_prod_data              = MyMetrics.InitString("xsq.bms.prod.data", SM_STALE_MAX, "");
  // 0x646 metrics
  mt_energy_used                = MyMetrics.InitFloat("xsq.v.energy.used", SM_STALE_MID, 0, kWh);
  mt_energy_recd                = MyMetrics.InitFloat("xsq.v.energy.recd", SM_STALE_MID, 0, kWh);
  mt_energy_aux                 = MyMetrics.InitFloat("xsq.v.energy.aux", SM_STALE_MID, 0, kWh);
  // 0x62d
  mt_worst_consumption          = MyMetrics.InitFloat("xsq.v.bat.consumption.worst", SM_STALE_MID, 0, kWhP100K);
  mt_best_consumption           = MyMetrics.InitFloat("xsq.v.bat.consumption.best", SM_STALE_MID, 0, kWhP100K);
  mt_bcb_power_mains            = MyMetrics.InitFloat("xsq.v.charge.bcb.power", SM_STALE_MID, 0, Watts);
  // 0x763 OBD metrics
  mt_obd_duration               = MyMetrics.InitInt("xsq.obd.charge.duration", SM_STALE_MID, 0, Minutes);
  mt_obd_mt_day_prewarn         = MyMetrics.InitInt("xsq.obd.mt.day.prewarn", SM_STALE_MID, 45, Other);
  mt_obd_mt_day_usual           = MyMetrics.InitInt("xsq.obd.mt.day.usual", SM_STALE_MID, 0, Other);
  mt_obd_mt_km_usual            = MyMetrics.InitInt("xsq.obd.mt.km.usual", SM_STALE_MID, 0, Kilometers);
  mt_obd_mt_level               = MyMetrics.InitString("xsq.obd.mt.level", SM_STALE_MID, "unknown", Other);

  mt_pos_odometer_start         = MyMetrics.InitFloat("xsq.odometer.start", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_start_total   = MyMetrics.InitFloat("xsq.odometer.start.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_trip_total    = MyMetrics.InitFloat("xsq.odometer.trip.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_trip          = MyMetrics.InitFloat("xsq.odometer.trip", SM_STALE_MID, 0, Kilometers);

  mt_tpms_low_batt               = MyMetrics.InitVector<short> ("xsq.tpms.lowbatt", SM_STALE_MID, nullptr, Other);
  mt_tpms_missing_tx             = MyMetrics.InitVector<short> ("xsq.tpms.missing", SM_STALE_MID, nullptr, Other);
    // Pre-allocate TPMS vectors for 4 wheels to avoid heap fragmentation
  StdMetrics.ms_v_tpms_temp->SetElemValue(3, 0.0f);
  StdMetrics.ms_v_tpms_pressure->SetElemValue(3, 0.0f);
  StdMetrics.ms_v_tpms_alert->SetElemValue(3, 0);
  mt_tpms_low_batt->SetElemValue(3, 0);
  mt_tpms_missing_tx->SetElemValue(3, 0);

  mt_dummy_pressure              = MyMetrics.InitFloat("xsq.tpms.dummy", SM_STALE_NONE, 210, kPa);  // Dummy pressure for TPMS alert testing
  // 0x765 BCM metrics
  mt_bcm_vehicle_state           = MyMetrics.InitString("xsq.bcm.state", SM_STALE_MIN, "UNKNOWN", Other);
  mt_driver_door_locked          = MyMetrics.InitBool("xsq.driver.door.locked", SM_STALE_MID, false);
  // 0x7EC EVC metrics
  // EVC 12V values: Index 0=dcdc_volt_req, 1=dcdc_volt, 2=dcdc_power, 3=usm_volt, 4=batt_volt_can, 5=batt_volt_req, 6=dcdc_amps, 7=dcdc_load
  mt_evc_dcdc                   = MyMetrics.InitVector<float>("xsq.evc.12v.dcdc", SM_STALE_MID, nullptr, Other);
  mt_evc_dcdc->SetElemValue(7, 0.0f);  // Pre-allocate 8 entries
  mt_evc_traceability           = MyMetrics.InitString("xsq.evc.traceability", SM_STALE_MAX, "");
  mt_evc_plug_detected          = MyMetrics.InitBool("xsq.evc.plug.detected", SM_STALE_MIN, false);
  // 0x793 OBL charger metrics
  mt_obl_fastchg                = MyMetrics.InitBool("xsq.obl.fastchg", SM_STALE_MIN, false);
  mt_obl_main_volts             = MyMetrics.InitVector<float>("xsq.obl.volts", SM_STALE_HIGH, nullptr, Volts);
  mt_obl_main_amps              = MyMetrics.InitVector<float>("xsq.obl.amps", SM_STALE_HIGH, nullptr, Amps);
  mt_obl_main_CHGpower          = MyMetrics.InitVector<float>("xsq.obl.power", SM_STALE_HIGH, nullptr, kW);
    // Pre-allocate OBL charger vectors for 3 phases to avoid reallocs
  mt_obl_main_volts->SetElemValue(2, 0.0f);
  mt_obl_main_amps->SetElemValue(2, 0.0f);
  mt_obl_main_CHGpower->SetElemValue(1, 0.0f);
    // OBL misc values: Index 0=freq, 1=ground_resistance, 2=max_current, 3=dc_current, 4=hf10kHz_current, 5=hf_current, 6=lf_current
  mt_obl_misc                   = MyMetrics.InitVector<float>("xsq.obl.misc", SM_STALE_MID, nullptr, Other);
  mt_obl_misc->SetElemValue(6, 0.0f);  // Pre-allocate 7 entries
  mt_obl_main_leakage_diag      = MyMetrics.InitString("xsq.obl.leakdiag", SM_STALE_MID, "", Other);
  // 0x7BB BMS metrics
  mt_bms_voltages               = MyMetrics.InitVector<float>("xsq.bms.voltages", SM_STALE_MID, nullptr, Volts);
  mt_bms_voltages->SetElemValue(6, 0.0f);  // Pre-allocate: [0]=cv_min, [1]=cv_max, [2]=cv_mean, [3]=link, [4]=contactor, [5]=cv_sum, [6]=12v_system
  mt_bms_contactor_cycles       = MyMetrics.InitVector<int>("xsq.bms.contactor.cycles", SM_STALE_HIGH, nullptr);
  mt_bms_contactor_cycles->SetElemValue(1, 0);  // Pre-allocate Max/Total entries
  mt_bms_soc_values             = MyMetrics.InitVector<float>("xsq.bms.soc.values", SM_STALE_MID, nullptr, Percentage);
  mt_bms_soc_values->SetElemValue(4, 0.0f);  // Pre-allocate: [0]=kernel, [1]=real, [2]=min, [3]=max, [4]=display
  mt_bms_soc_recal_state        = MyMetrics.InitString("xsq.bms.soc.recal.state", SM_STALE_MID, "");
  mt_bms_soh                    = MyMetrics.InitFloat("xsq.bms.soh", SM_STALE_MID, 0, Percentage);
  // BMS capacity values: Index 0=usable_max, 1=init, 2=estimate, 3=loss_pct
  mt_bms_cap                    = MyMetrics.InitVector<float>("xsq.bms.cap", SM_STALE_MID, nullptr, Other);
  mt_bms_cap->SetElemValue(4, 0.0f);  // Pre-allocate 5 entries
  mt_bms_mileage                = MyMetrics.InitInt("xsq.bms.mileage", SM_STALE_HIGH, 0, Kilometers);
  mt_bms_voltage_state          = MyMetrics.InitString("xsq.bms.voltage.state", SM_STALE_MID, "");
  mt_bms_cell_resistance        = MyMetrics.InitVector<float>("xsq.bms.cell.resistance", SM_STALE_HIGH, nullptr, Other);
  mt_bms_cell_resistance->SetElemValue(CELLCOUNT - 1, 0.0f);  // Pre-allocate for all 96 cells
  mt_bms_nominal_energy         = MyMetrics.InitFloat("xsq.bms.energy.nominal", SM_STALE_HIGH, 0, kWh);
  mt_bms_HVcontactStateTXT      = MyMetrics.InitString("xsq.bms.contact", SM_STALE_MID, "", Other);
  mt_bms_EVmode_txt             = MyMetrics.InitString("xsq.bms.ev.mode", SM_STALE_MID, "", Other);
  mt_bms_interlock_hvplug       = MyMetrics.InitBool("xsq.bms.interlock.hvplug",SM_STALE_MID, false);
  mt_bms_interlock_service      = MyMetrics.InitBool("xsq.bms.interlock.service", SM_STALE_MID, false);
  mt_bms_fusi_mode_txt          = MyMetrics.InitString("xsq.bms.fusi",SM_STALE_MID, "", Other);
  mt_bms_safety_mode_txt        = MyMetrics.InitString("xsq.bms.safety",SM_STALE_MID, "", Other);

  // Start CAN bus in Listen-only mode - will be set according to m_enable_write in ConfigChanged()
  RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);

  // register config container for smart EQ module, with callback to ConfigChanged() on changes
  MyConfig.RegisterParam("xsq", "smartEQ", true, true); 
  ConfigChanged(NULL);

  // init commands:
  cmd_xsq = MyCommandApp.RegisterCommand("xsq","smartEQ 453 Gen.4");
  cmd_xsq->RegisterCommand("mtdata", "Maintenance data", xsq_maintenance);
  cmd_xsq->RegisterCommand("ddt4all", "DDT4all Command", xsq_ddt4all,"<number>",1,1);
  cmd_xsq->RegisterCommand("ddt4list", "DDT4all Command List", xsq_ddt4list);
  cmd_xsq->RegisterCommand("canwrite", "Send CAN command", xsq_canwrite,"<txid,rxid,hexbytes[,reset,wakeup]>",1,1);
  cmd_xsq->RegisterCommand("calcadc", "Recalculate ADC factor (optional: 12V voltage override)", xsq_calc_adc, "[voltage]", 0, 1);
  cmd_xsq->RegisterCommand("wakeup", "Wake up the car", xsq_wakeup);
  cmd_xsq->RegisterCommand("ed4scan", "ED4scan-like BMS Data", xsq_ed4scan);
  cmd_xsq->RegisterCommand("preset", "smart EQ config preset", xsq_preset);
  cmd_xsq->RegisterCommand("default", "smart EQ config set default", xsq_setdefault);

  cmd_tpms = cmd_xsq->RegisterCommand("tpms", "TPMS status");
  cmd_tpms->RegisterCommand("setdummy", "set TPMS dummy value", xsq_tpms_set);
  cmd_tpms->RegisterCommand("status", "Show extended TPMS data", xsq_tpms_status);

  cmd_show = cmd_xsq->RegisterCommand("show", "Show smartEQ data");
  cmd_show->RegisterCommand("start", "Show OBD trip start", xsq_trip_start);
  cmd_show->RegisterCommand("reset", "Show OBD trip total", xsq_trip_reset);
  cmd_show->RegisterCommand("counter", "Show vehicle trip counter", xsq_trip_counters);
  cmd_show->RegisterCommand("total", "Show vehicle trip total", xsq_trip_total);

  StdMetrics.ms_v_gen_current->SetValue(2);                    // activate gen metrics to app transfer
  StdMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);      // set 12V alert to false
  StdMetrics.ms_v_env_charging12v->SetValue(false);            // set 12V charging state to false
  StdMetrics.ms_v_env_aux12v->SetValue(false);
  StdMetrics.ms_v_env_hvac->SetValue(false);  

  if (mt_pos_odometer_trip_total->AsFloat(0) < 1.0f)           // reset at boot
    {
    ResetTotalCounters();
    ResetTripCounters();
    }

  if (m_cfg_preset_version != PRESET_VERSION)                  // preset version changed
    CommandPreset(0, NULL);                                    // set smart EQ config preset  

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
  #endif
}

OvmsVehicleSmartEQ::~OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Stop smart EQ vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (m_enable_LED_state) 
    {
    MyPeripherals->m_max7317->Output(9, 0);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
    }
#endif
}

/**
 * ConfigChanged: reload single/all configuration variables (cfgupdate)
 */
void OvmsVehicleSmartEQ::ConfigChanged(OvmsConfigParam* param) {
  if (param && param->GetName() == "vehicle")
    setTPMSValue();   // update TPMS metrics
  if (param && param->GetName() != "xsq")
    return;

  ESP_LOGI(TAG, "smart EQ reload configuration");

  // Use OvmsConfigParam getters for efficient reading (OVMS standard API):
  // Note: GetValueBool/Int/Float treat empty string as "not set" and return the default.
  OvmsConfigParam* map = MyConfig.CachedParam("xsq");
  
  int cell_interval_drv   = 60;
  int cell_interval_chg   = 60;
  bool stateWrite         = m_enable_write;
  bool obdii_743          = true;
  bool obdii_745          = true;
  bool obdii_745_tpms     = true;
  bool obdii_7e4          = true;
  bool obdii_79b_cell     = true;
  bool obdii_79b          = true;
  bool obdii_7e4_dcdc     = true;
  
  if (map)
    {
    m_enable_write         = map->GetValueBool("canwrite", false);
    m_enable_write_caron   = map->GetValueBool("canwrite.caron", true);
    m_enable_LED_state     = map->GetValueBool("led", false);
    m_bcvalue              = map->GetValueBool("bcvalue", false);
    m_enable_lock_state    = map->GetValueBool("unlock.warning", true);
    m_enable_door_state    = map->GetValueBool("door.warning", true);
    m_resettrip            = map->GetValueBool("resettrip", true);
    m_resettotal           = map->GetValueBool("resettotal", false);
    m_tripnotify           = map->GetValueBool("reset.notify", false);
    m_tpms_alert_enable    = map->GetValueBool("tpms.alert.enable", true);
    m_tpms_temp_enable     = map->GetValueBool("tpms.temp", true);
    m_12v_charge           = map->GetValueBool("12v.charge", true);
    m_enable_calcADCfactor = map->GetValueBool("calc.adcfactor", false);
    m_indicator            = map->GetValueBool("indicator", false);
    m_extendedStats        = map->GetValueBool("extended.stats", false);
    obdii_79b              = map->GetValueBool("obdii.79b", true);
    obdii_79b_cell         = map->GetValueBool("obdii.79b.cell", true);
    obdii_743              = map->GetValueBool("obdii.743", true);
    obdii_745              = map->GetValueBool("obdii.745", true);
    obdii_745_tpms         = map->GetValueBool("obdii.745.tpms", true);
    obdii_7e4              = map->GetValueBool("obdii.7e4", true);
    obdii_7e4_dcdc         = map->GetValueBool("obdii.7e4.dcdc", true);

    m_reboot_time          = map->GetValueInt("rebootnw", 30);
    m_park_timeout_secs    = map->GetValueInt("park.timeout", 600);
    m_full_km              = map->GetValueInt("full.km", 126);
    m_cfg_preset_version   = map->GetValueInt("cfg.preset.ver", 0);
    m_suffsoc              = map->GetValueInt("suffsoc", 0);
    m_suffrange            = map->GetValueInt("suffrange", 0);
    cell_interval_drv      = map->GetValueInt("cell_interval_drv", 60);
    cell_interval_chg      = map->GetValueInt("cell_interval_chg", 60);
    
    m_front_pressure       = map->GetValueFloat("tpms.front.pressure", 225.0f);
    m_rear_pressure        = map->GetValueFloat("tpms.rear.pressure", 255.0f);
    m_pressure_warning     = map->GetValueFloat("tpms.value.warn", 40.0f);
    m_pressure_alert       = map->GetValueFloat("tpms.value.alert", 70.0f);
    }

#ifdef CONFIG_OVMS_COMP_MAX7317
  if (!m_enable_LED_state) 
    {
    MyPeripherals->m_max7317->Output(9, 1);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
    }
#endif

  // set CAN bus transceiver to active or listen-only depending on user selection
  if ( stateWrite != m_enable_write )
    {
    smartCANmode(m_enable_write);
    }
  // disable caron write mode if normal write mode is enabled to avoid conflicts
  if(m_enable_write_caron && m_enable_write) 
    {
    m_enable_write_caron = false;
    MyConfig.SetParamValueBool("xsq", "canwrite.caron", false);
    }

  bool do_modify_poll = (
    (cell_interval_drv != m_cfg_cell_interval_drv) ||
    (cell_interval_chg != m_cfg_cell_interval_chg) ||
    (obdii_79b != m_obdii_79b) ||
    (obdii_79b_cell != m_obdii_79b_cell) ||
    (obdii_743 != m_obdii_743) ||
    (obdii_745 != m_obdii_745) ||
    (obdii_745_tpms != m_obdii_745_tpms) ||
    (obdii_7e4 != m_obdii_7e4) ||
    (obdii_7e4_dcdc != m_obdii_7e4_dcdc)
  );
  
  m_cfg_cell_interval_drv = cell_interval_drv;
  m_cfg_cell_interval_chg = cell_interval_chg;
  m_obdii_79b = obdii_79b;
  m_obdii_79b_cell = obdii_79b_cell;
  m_obdii_743 = obdii_743;
  m_obdii_745 = obdii_745;
  m_obdii_745_tpms = obdii_745_tpms;
  m_obdii_7e4 = obdii_7e4;
  m_obdii_7e4_dcdc = obdii_7e4_dcdc;

  if (do_modify_poll) 
    {
    HandleOBDpolling();
    }
  StdMetrics.ms_v_charge_limit_soc->SetValue((float) m_suffsoc, Percentage );
  StdMetrics.ms_v_charge_limit_range->SetValue((float) m_suffrange, Kilometers );
}

uint64_t OvmsVehicleSmartEQ::swap_uint64(uint64_t val) {
  val = ((val << 8) & 0xFF00FF00FF00FF00ull) | ((val >> 8) & 0x00FF00FF00FF00FFull);
  val = ((val << 16) & 0xFFFF0000FFFF0000ull) | ((val >> 16) & 0x0000FFFF0000FFFFull);
  return (val << 32) | (val >> 32);
}

void OvmsVehicleSmartEQ::UpdateChargeMetrics() {
  // Phase detection and voltage calculation
  int phasecnt = 0;
  int active_phase = -1;
  float voltagesum = 0.0f;
  
  for (int i = 0; i < 3; i++) {
    float voltage = mt_obl_main_volts->GetElemValue(i);
    if (voltage > 120.0f) {
      phasecnt++;
      active_phase = i;
      voltagesum += voltage;
    }
  }
  
  // Calculate average voltage for multi-phase
  float avg_voltage = (phasecnt > 1) ? (voltagesum / phasecnt) : voltagesum;
  StdMetrics.ms_v_charge_voltage->SetValue(avg_voltage);
  
  // Current calculation based on charging mode
  float total_current = 0.0f;
  bool is_fast_charge = mt_obl_fastchg->AsBool();
  
  if (phasecnt == 1 && is_fast_charge) {
    // Single-phase DC fast charge: use only active phase
    total_current = mt_obl_main_amps->GetElemValue(active_phase);
  } else {
    // Multi-phase or AC charging: sum all positive currents
    for (int i = 0; i < 3; i++) {
      float current = mt_obl_main_amps->GetElemValue(i);
      if (current > 0.0f) {
        total_current += current;
      }
    }
    
    // For 3-phase DC fast charge, use average (as per original logic)
    if (phasecnt == 3 && is_fast_charge) {
      total_current /= 3.0f;
    }
  }
  
  StdMetrics.ms_v_charge_current->SetValue(total_current);
  
  // Safe power calculation with default values
  float power_grid = mt_obl_main_CHGpower->GetElemValue(0) + 
                     mt_obl_main_CHGpower->GetElemValue(1);
  StdMetrics.ms_v_charge_power->SetValue(power_grid);
  
  // Use absolute value of battery power for efficiency calculation
  float power_battery = fabs(StdMetrics.ms_v_bat_power->AsFloat(0.0f));  
  float efficiency = 0.0f;

  if (power_grid > 0.01f) {  // Avoid division by zero, require at least 10W
    efficiency = (power_battery / power_grid) * 100.0f;
    
    // Sanity check: efficiency should be between 0-100%
    if (efficiency > 100.0f) {
      efficiency = 100.0f;
    } else if (efficiency < 0.0f) {
      efficiency = 0.0f;
    }
  }
  
  StdMetrics.ms_v_charge_efficiency->SetValue(efficiency);
}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
int OvmsVehicleSmartEQ::calcMinutesRemaining(float target_soc, float charge_voltage, float charge_current) {
  float bat_soc = StdMetrics.ms_v_bat_soc->AsFloat(0.0f);
  if (bat_soc > target_soc) {
    return 0;   // Done!
  }
  float remaining_wh    = DEFAULT_BATTERY_CAPACITY * (target_soc - bat_soc) / 100.0f;
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0f;

  return MIN( 1440, (int)remaining_mins );
}

void OvmsVehicleSmartEQ::CalculateEfficiency() {
  if (StdMetrics.ms_v_pos_speed->AsFloat(0.0f) >= 5) {
    float _use_grid = StdMetrics.ms_v_bat_energy_used->AsFloat(0.0f) - StdMetrics.ms_v_bat_energy_recd->AsFloat(0.0f);
    float _use_grid_total = StdMetrics.ms_v_bat_energy_used_total->AsFloat(0.0f) - StdMetrics.ms_v_bat_energy_recd_total->AsFloat(0.0f);
    float _trip_km = (_use_grid / StdMetrics.ms_v_pos_trip->AsFloat(0.0f)) * 100.0f;
    float _trip_total_km = (_use_grid_total / mt_pos_odometer_trip_total->AsFloat(0.0f)) * 100.0f;
    
    if (_trip_km > 0.1f) 
      StdMetrics.ms_v_charge_kwh_grid->SetValue(_trip_km);
    if (_trip_total_km > 0.1f) 
      StdMetrics.ms_v_charge_kwh_grid_total->SetValue(_trip_total_km);
  }
}

/**
 * CalculateRangeSpeed: derive momentary range gain/loss speed (km/h equivalent)
 *
 * Uses power-based calculation against the full battery energy (cap_full × HV_link / 1000)
 * to avoid inflated values at low SOC (ms_v_bat_cac is the usable fraction, not full cap).
 * Sign: negative = losing range (driving), positive = gaining range (charging).
 */
void OvmsVehicleSmartEQ::CalculateRangeSpeed()
  {
  float bat_power  = StdMetrics.ms_v_bat_power->AsFloat();      // kW (pos=discharge, neg=charge)
  float cap_full   = mt_bms_cap->GetElemValue(0);               // Ah - full usable capacity
  float v_link     = mt_bms_voltages->GetElemValue(3);          // V  - HV link voltage
  float range_full = StdMetrics.ms_v_bat_range_full->AsFloat(); // km at 100% SOC

  if (cap_full <= 0 || v_link <= 0 || range_full <= 0)
    return;

  float energy_kWh = (cap_full * v_link) / 1000.0f;
  *StdMetrics.ms_v_bat_range_speed = TRUNCPREC(-bat_power / energy_kWh * range_full, 1);
  }

void OvmsVehicleSmartEQ::OnlineState() {
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (StdMetrics.ms_m_net_ip->AsBool()) {
    // connected:
    if (StdMetrics.ms_s_v2_connected->AsBool()) {
      if (m_led_state != 1) {
        MyPeripherals->m_max7317->Output(9, 1);
        MyPeripherals->m_max7317->Output(8, 0);
        MyPeripherals->m_max7317->Output(7, 1);
        m_led_state = 1;
        ESP_LOGI(TAG,"LED GREEN");
      }
    } else if (StdMetrics.ms_m_net_connected->AsBool()) {
      if (m_led_state != 2) {
        MyPeripherals->m_max7317->Output(9, 1);
        MyPeripherals->m_max7317->Output(8, 1);
        MyPeripherals->m_max7317->Output(7, 0);
        m_led_state = 2;
        ESP_LOGI(TAG,"LED BLUE");
      }
    } else {
      if (m_led_state != 3) {
        MyPeripherals->m_max7317->Output(9, 0);
        MyPeripherals->m_max7317->Output(8, 1);
        MyPeripherals->m_max7317->Output(7, 1);
        m_led_state = 3;
        ESP_LOGI(TAG,"LED RED");
      }
    }
  }
  else if (m_led_state != 0) {
    // not connected:
    MyPeripherals->m_max7317->Output(9, 1);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
    m_led_state = 0;
    ESP_LOGI(TAG,"LED Off");
  }
#endif
}

class OvmsVehicleSmartEQInit {
  public:
    OvmsVehicleSmartEQInit();
} MyOvmsVehicleSmartEQInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEQInit::OvmsVehicleSmartEQInit() {
  ESP_LOGI(TAG, "Registering Vehicle: smart EQ (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartEQ>("SQ", "smart 453 4.Gen");
}
