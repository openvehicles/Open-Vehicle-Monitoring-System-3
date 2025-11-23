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
#include "eq_poller.h"

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

  m_12v_ticker = 0;
  m_ddt4all = false;
  m_ddt4all_ticker = 0;
  m_ddt4all_exec = 0;
  m_warning_unlocked = false;
  m_warning_dooropen = false;
  m_modem_restart = false;
  m_modem_ticker = 0;
  m_ADCfactor_recalc = false;
  m_ADCfactor_recalc_timer = 0;

  m_enable_write = false;
  m_candata_poll = false;
  m_candata_timer = -1;

  m_charge_start = false;
  m_charge_finished = true;
  m_notifySOClimit = false;
  m_led_state = 4;
  m_cfg_cell_interval_drv = 0;
  m_cfg_cell_interval_chg = 0;

  for (int i = 0; i < 4; i++) 
    {
    m_tpms_pressure[i] = 0.0f;
    m_tpms_temperature[i] = 0.0f;
    m_tpms_lowbatt[i] = false;
    m_tpms_missing_tx[i] = false;
    }

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 3);
  BmsSetCellArrangementTemperature(27, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  mt_bus_awake                  = MyMetrics.InitBool("xsq.v.bus.awake", SM_STALE_MIN, true);
  mt_canbyte                    = MyMetrics.InitString("xsq.ddt4all.canbyte", SM_STALE_NONE, "", Other);
  mt_adc_factor                 = MyMetrics.InitFloat("xsq.adc.factor", SM_STALE_NONE, 0, Other);
  mt_adc_factor_history         = new OvmsMetricVector<float>("xsq.adc.factor.history", SM_STALE_NONE, Other);
  mt_poll_state                 = MyMetrics.InitString("xsq.poll.state", SM_STALE_NONE, "UNKNOWN", Other);

  mt_start_time                 = MyMetrics.InitString("xsq.v.start.time", SM_STALE_MID, 0, Other);
  mt_start_distance             = MyMetrics.InitFloat("xsq.v.start.distance", SM_STALE_MID, 0, Kilometers);

  mt_reset_time                 = MyMetrics.InitString("xsq.v.reset.time", SM_STALE_MID, 0, Other);
  mt_reset_consumption          = MyMetrics.InitFloat("xsq.v.reset.consumption", SM_STALE_MID, 0, kWhP100K);
  mt_reset_distance             = MyMetrics.InitFloat("xsq.v.reset.distance", SM_STALE_MID, 0, Kilometers);
  mt_reset_energy               = MyMetrics.InitFloat("xsq.v.reset.energy", SM_STALE_MID, 0, kWh);
  mt_reset_speed                = MyMetrics.InitFloat("xsq.v.reset.speed", SM_STALE_MID, 0, Kph);
  // 0x658 metrics
  mt_bat_serial                 = MyMetrics.InitString("xsq.v.bat.serial", SM_STALE_MAX, "");
  // 0x646 metrics
  mt_energy_used                = MyMetrics.InitFloat("xsq.v.energy.used", SM_STALE_MID, 0, kWh);
  mt_energy_recd                = MyMetrics.InitFloat("xsq.v.energy.recd", SM_STALE_MID, 0, kWh);
  mt_aux_consumption            = MyMetrics.InitFloat("xsq.v.aux.consumption", SM_STALE_MID, 0, kWh);
  mt_total_recovery             = MyMetrics.InitFloat("xsq.v.total.recovery", SM_STALE_MID, 0, kWh);
  // 0x62d
  mt_worst_consumption          = MyMetrics.InitFloat("xsq.v.bat.consumption.worst", SM_STALE_MID, 0, kWhP100K);
  mt_best_consumption           = MyMetrics.InitFloat("xsq.v.bat.consumption.best", SM_STALE_MID, 0, kWhP100K);
  mt_bcb_power_mains            = MyMetrics.InitFloat("xsq.v.charge.bcb.power", SM_STALE_MID, 0, Watts);
  // 0x634
  mt_tcu_refuse_sleep           = MyMetrics.InitBool("xsq.v.tcu.refuse.sleep", SM_STALE_MID, 0);
  mt_tcu_refuse_timestamp       = MyMetrics.InitString("xsq.v.tcu.refuse.timestamp", SM_STALE_MID, "unknown", Other);
  mt_charging_timer_value       = MyMetrics.InitInt("xsq.v.charge.timer.value", SM_STALE_MID, 0, Minutes);
  mt_charging_timer_status      = MyMetrics.InitInt("xsq.v.charge.timer.status", SM_STALE_MID, 0);
  mt_charge_prohibited          = MyMetrics.InitInt("xsq.v.charge.prohibited", SM_STALE_MID, 0);
  mt_charge_authorization       = MyMetrics.InitInt("xsq.v.charge.authorization", SM_STALE_MID, 0);
  mt_ext_charge_manager         = MyMetrics.InitInt("xsq.v.charge.ext.manager", SM_STALE_MID, 0);

  mt_obd_duration               = MyMetrics.InitInt("xsq.obd.charge.duration", SM_STALE_MID, 0, Minutes);
  mt_obd_mt_day_prewarn         = MyMetrics.InitInt("xsq.obd.mt.day.prewarn", SM_STALE_MID, 45, Other);
  mt_obd_mt_day_usual           = MyMetrics.InitInt("xsq.obd.mt.day.usual", SM_STALE_MID, 0, Other);
  mt_obd_mt_km_usual            = MyMetrics.InitInt("xsq.obd.mt.km.usual", SM_STALE_MID, 0, Kilometers);
  mt_obd_mt_level               = MyMetrics.InitString("xsq.obd.mt.level", SM_STALE_MID, "unknown", Other);

  mt_pos_odometer_start         = MyMetrics.InitFloat("xsq.odometer.start", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_start_total   = MyMetrics.InitFloat("xsq.odometer.start.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_trip_total    = MyMetrics.InitFloat("xsq.odometer.trip.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_trip          = MyMetrics.InitFloat("xsq.odometer.trip", SM_STALE_MID, 0, Kilometers);

  mt_tpms_temp                   = MyMetrics.InitVector<float>("xsq.tpms.temp", SM_STALE_MID, nullptr, Celcius);
  mt_tpms_pressure               = MyMetrics.InitVector<float>("xsq.tpms.pressure", SM_STALE_MID, nullptr, kPa);
  mt_tpms_alert                  = MyMetrics.InitVector<short> ("xsq.tpms.alert", SM_STALE_MID, nullptr, Other);
  mt_tpms_low_batt               = MyMetrics.InitVector<short> ("xsq.tpms.lowbatt", SM_STALE_MID, nullptr, Other);
  mt_tpms_missing_tx             = MyMetrics.InitVector<short> ("xsq.tpms.missing", SM_STALE_MID, nullptr, Other);  
  mt_dummy_pressure              = MyMetrics.InitFloat("xsq.tpms.dummy", SM_STALE_NONE, 235, kPa);  // Dummy pressure for TPMS alert testing
  // 0x765 BCM metrics
  mt_bcm_vehicle_state           = MyMetrics.InitString("xsq.bcm.state", SM_STALE_MIN, "UNKNOWN", Other);
  mt_bcm_gen_mode                = MyMetrics.InitString("xsq.bcm.gen.mode", SM_STALE_MID, "UNKNOWN", Other);

  mt_evc_hv_energy              = MyMetrics.InitFloat("xsq.evc.hv.energy", SM_STALE_MID, 0, kWh);
  mt_evc_LV_DCDC_act_req        = MyMetrics.InitBool("xsq.evc.12V.dcdc.act.req", SM_STALE_MID, false);
  mt_evc_LV_DCDC_amps           = MyMetrics.InitFloat("xsq.evc.12V.dcdc.amps", SM_STALE_MID, 0, Amps);
  mt_evc_LV_DCDC_load           = MyMetrics.InitFloat("xsq.evc.12V.dcdc.load", SM_STALE_MID, 0, Percentage);
  mt_evc_LV_DCDC_volt_req       = MyMetrics.InitFloat("xsq.evc.12V.dcdc.volt.req", SM_STALE_MID, 0, Volts);
  mt_evc_LV_DCDC_volt           = MyMetrics.InitFloat("xsq.evc.12V.dcdc.volt", SM_STALE_MID, 0, Volts);
  mt_evc_LV_DCDC_power          = MyMetrics.InitFloat("xsq.evc.12V.dcdc.power", SM_STALE_MID, 0, Watts);
  mt_evc_LV_USM_volt            = MyMetrics.InitFloat("xsq.evc.12V.volt.usm", SM_STALE_MIN, 0, Volts);
  mt_evc_LV_batt_voltage_can    = MyMetrics.InitFloat("xsq.evc.12V.volt.can", SM_STALE_MIN, 0, Volts);
  mt_evc_LV_batt_voltage_req    = MyMetrics.InitFloat("xsq.evc.12V.batt.volt.req.int", SM_STALE_MIN, 0, Volts);

  mt_obl_fastchg                = MyMetrics.InitBool("xsq.obl.fastchg", SM_STALE_MIN, false);
  mt_obl_main_volts             = new OvmsMetricVector<float>("xsq.obl.volts", SM_STALE_HIGH, Volts);
  mt_obl_main_amps              = new OvmsMetricVector<float>("xsq.obl.amps", SM_STALE_HIGH, Amps);
  mt_obl_main_CHGpower          = new OvmsMetricVector<float>("xsq.obl.power", SM_STALE_HIGH, kW);
  mt_obl_main_ground_resistance = MyMetrics.InitFloat("xsq.obl.ground.resistance", SM_STALE_MID, 0, Other);
  mt_obl_main_freq              = MyMetrics.InitFloat("xsq.obl.freq", SM_STALE_MID, 0, Other);
  mt_obl_main_max_current       = MyMetrics.InitInt("xsq.obl.max.current", SM_STALE_MID, 0, Amps);
  mt_obl_main_leakage_diag      = MyMetrics.InitString("xsq.obl.leakdiag", SM_STALE_MID, "", Other);
  mt_obl_main_current_leakage_dc        = MyMetrics.InitFloat("xsq.obl.current.dc", SM_STALE_MID, 0, Amps);
  mt_obl_main_current_leakage_hf_10khz  = MyMetrics.InitFloat("xsq.obl.current.hf10kHz", SM_STALE_MID, 0, Amps);
  mt_obl_main_current_leakage_hf        = MyMetrics.InitFloat("xsq.obl.current.hf", SM_STALE_MID, 0, Amps);
  mt_obl_main_current_leakage_lf        = MyMetrics.InitFloat("xsq.obl.current.lf", SM_STALE_MID, 0, Amps);

  mt_bms_temps                  = new OvmsMetricVector<float>("xsq.bms.temps", SM_STALE_HIGH, Celcius);
  mt_bms_CV_Range_min           = MyMetrics.InitFloat("xsq.bms.cv.range.min", SM_STALE_MID, 0, Volts);
  mt_bms_CV_Range_max           = MyMetrics.InitFloat("xsq.bms.cv.range.max", SM_STALE_MID, 0, Volts);
  mt_bms_CV_Range_mean          = MyMetrics.InitFloat("xsq.bms.cv.range.mean", SM_STALE_MID, 0, Volts);
  mt_bms_BattLinkVoltage        = MyMetrics.InitFloat("xsq.bms.batt.link.voltage", SM_STALE_MID, 0, Volts);
  mt_bms_BattContactorVoltage   = MyMetrics.InitFloat("xsq.bms.batt.contactor.voltage", SM_STALE_MID, 0, Volts);
  mt_bms_BattCV_Sum             = MyMetrics.InitFloat("xsq.bms.batt.cv.sum", SM_STALE_MID, 0, Volts);
  mt_bms_BattPower_power        = MyMetrics.InitFloat("xsq.bms.batt.power", SM_STALE_MID, 0, kW);
  mt_bms_HVcontactStateTXT      = MyMetrics.InitString("xsq.bms.contact", SM_STALE_MID, "", Other);
  mt_bms_HV                     = MyMetrics.InitFloat("xsq.bms.hv", SM_STALE_MID, 0, Volts);
  mt_bms_EVmode_txt             = MyMetrics.InitString("xsq.bms.ev.mode", SM_STALE_MID, "", Other);
  mt_bms_12v                    = MyMetrics.InitFloat("xsq.bms.12v", SM_STALE_MID, 0, Volts);
  mt_bms_interlock_hvplug       = MyMetrics.InitBool("xsq.bms.interlock.hvplug",SM_STALE_MID, false);
  mt_bms_interlock_service      = MyMetrics.InitBool("xsq.bms.interlock.service", SM_STALE_MID, false);
  mt_bms_fusi_mode_txt          = MyMetrics.InitString("xsq.bms.fusi",SM_STALE_MID, "", Other);
  mt_bms_mg_rpm                 = MyMetrics.InitFloat("xsq.bms.mg.rpm",SM_STALE_MID, 0, Other);
  mt_bms_safety_mode_txt        = MyMetrics.InitString("xsq.bms.safety",SM_STALE_MID, "", Other);

  // Start CAN bus in Listen-only mode - will be set according to m_enable_write in ConfigChanged()
  RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);

  // init commands:
  cmd_xsq = MyCommandApp.RegisterCommand("xsq","smartEQ 453 Gen.4");
  cmd_xsq->RegisterCommand("start", "Show OBD trip start", xsq_trip_start);
  cmd_xsq->RegisterCommand("reset", "Show OBD trip total", xsq_trip_reset);
  cmd_xsq->RegisterCommand("counter", "Show vehicle trip counter", xsq_trip_counters);
  cmd_xsq->RegisterCommand("total", "Show vehicle trip total", xsq_trip_total);
  cmd_xsq->RegisterCommand("mtdata", "Show Maintenance data", xsq_maintenance);
  cmd_xsq->RegisterCommand("tpmsset", "set TPMS dummy value", xsq_tpms_set);
  cmd_xsq->RegisterCommand("ddt4all", "DDT4all Command", xsq_ddt4all,"<number>",1,1);
  cmd_xsq->RegisterCommand("ddt4list", "DDT4all Command List", xsq_ddt4list);
  cmd_xsq->RegisterCommand("calcadc", "Recalculate ADC factor (optional: 12V voltage override)", xsq_calc_adc, "[voltage]", 0, 1);
  cmd_xsq->RegisterCommand("wakeup", "Wake up the car", xsq_wakeup);
  cmd_xsq->RegisterCommand("preset", "smart EQ config preset", xsq_preset);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"vehicle.charge.start", std::bind(&OvmsVehicleSmartEQ::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.charge.stop", std::bind(&OvmsVehicleSmartEQ::EventListener, this, _1, _2));
  MyConfig.RegisterParam("xsq", "smartEQ", true, true);

  ConfigChanged(NULL);
  StdMetrics.ms_v_gen_current->SetValue(2);                    // activate gen metrics to app transfer
  StdMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);      // set 12V alert to false

  if (mt_pos_odometer_trip_total->AsFloat(0) < 1.0f)           // reset at boot
    {
    ResetTotalCounters();
    ResetTripCounters();
    }

  setTPMSValueBoot();                                          // set TPMS dummy values to 0

    if (m_enable_write)
      PollSetState(1);                                           // start polling to get the first data

  if (m_enable_write && m_cfg_preset_version != PRESET_VERSION)  // preset version changed
    CommandPreset(0, NULL);                                      // set smart EQ config preset

  #ifdef CONFIG_OVMS_COMP_CELLULAR
    #ifdef CONFIG_OVMS_COMP_WIFI
      // Features option: auto restart modem on wifi disconnect
      if (m_modem_check) 
      {
      MyEvents.RegisterEvent(TAG,"cellular.modem.disconnected", std::bind(&OvmsVehicleSmartEQ::ModemEventRestart, this, _1, _2));
      }
    #endif
  #endif

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
  if (param && param->GetName() != "xsq")
    return;

  ESP_LOGI(TAG, "smart EQ reload configuration");

  bool stateWrite = m_enable_write;
  m_enable_write  = MyConfig.GetParamValueBool("xsq", "canwrite", false);

  // set CAN bus transceiver to active or listen-only depending on user selection
  if ( stateWrite != m_enable_write )
    {
    CAN_mode_t mode = m_enable_write ? CAN_MODE_ACTIVE : CAN_MODE_LISTEN;
    RegisterCanBus(1, mode, CAN_SPEED_500KBPS);
    }

  // Use GetParamMap for efficient bulk reading
  OvmsConfigParam* xsq_param = MyConfig.CachedParam("xsq");
  
  int cell_interval_drv   = 60;
  int cell_interval_chg   = 60;
  
  if (xsq_param) 
    {
    const auto& xsqcfg = xsq_param->GetMap();
    
    // Helper lambdas
    auto getBool = [&xsqcfg](const char* key, bool def) -> bool {
      auto it = xsqcfg.find(key);
      if (it == xsqcfg.end()) return def;
      const std::string& val = it->second;
      return (val == "yes" || val == "true" || val == "1");
    };
    
    auto getInt = [&xsqcfg](const char* key, int def) -> int {
      auto it = xsqcfg.find(key);
      return (it != xsqcfg.end()) ? atoi(it->second.c_str()) : def;
    };
    
    auto getFloat = [&xsqcfg](const char* key, float def) -> float {
      auto it = xsqcfg.find(key);
      return (it != xsqcfg.end()) ? atof(it->second.c_str()) : def;
    };

    /*  // not used currently
    auto getString = [&xsqcfg](const char* key, const char* def) -> std::string {
      auto it = xsqcfg.find(key);
      return (it != xsqcfg.end()) ? it->second : def;
    };
    */
    
    // Read all config values from map
    m_enable_LED_state     = getBool("led", false);
    m_bcvalue              = getBool("bcvalue", false);
    m_enable_lock_state    = getBool("unlock.warning", true);
    m_enable_door_state    = getBool("door.warning", true);
    m_reboot_time          = getInt("rebootnw", 30);
    m_resettrip            = getBool("resettrip", true);
    m_resettotal           = getBool("resettotal", false);
    m_tripnotify           = getBool("reset.notify", false);
    m_front_pressure       = getFloat("tpms.front.pressure", 225.0f);
    m_rear_pressure        = getFloat("tpms.rear.pressure", 255.0f);
    m_pressure_warning     = getFloat("tpms.value.warn", 40.0f);
    m_pressure_alert       = getFloat("tpms.value.alert", 70.0f);
    m_tpms_alert_enable    = getBool("tpms.alert.enable", true);
    m_tpms_temp_enable     = getBool("tpms.temp", true);
    m_modem_check          = getBool("modem.check", false);
    m_12v_charge           = getBool("12v.charge", true);
    m_enable_calcADCfactor = getBool("calc.adcfactor", false);
    m_adc_samples          = getInt("adc.samples", 4);
    m_indicator            = getBool("indicator", false);
    m_extendedStats        = getBool("extended.stats", false);
    m_park_timeout_secs    = getInt("park.timeout", 600);
    m_full_km              = getInt("full.km", 126);
    m_cfg_preset_version   = getInt("cfg.preset.ver", 0);
    m_suffsoc              = getInt("suffsoc", 0);
    m_suffrange            = getInt("suffrange", 0);
    cell_interval_drv      = getInt("cell_interval_drv", 60);
    cell_interval_chg      = getInt("cell_interval_chg", 60);
    }

#ifdef CONFIG_OVMS_COMP_MAX7317
  if (!m_enable_LED_state) 
    {
    MyPeripherals->m_max7317->Output(9, 1);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
    }
#endif
  
  bool do_modify_poll = (
    (cell_interval_drv != m_cfg_cell_interval_drv) ||
    (cell_interval_chg != m_cfg_cell_interval_chg));

  m_cfg_cell_interval_drv = cell_interval_drv;
  m_cfg_cell_interval_chg = cell_interval_chg;

  if (do_modify_poll) 
    {
    ObdModifyPoll();
    }
  StdMetrics.ms_v_charge_limit_soc->SetValue((float) m_suffsoc, Percentage );
  StdMetrics.ms_v_charge_limit_range->SetValue((float) m_suffrange, Kilometers );
}

void OvmsVehicleSmartEQ::ObdModifyPoll() {
  PollSetPidList(m_can1, NULL);
  PollSetState(0);
  PollSetThrottling(0);
  PollSetResponseSeparationTime(20);

  // modify Poller..
  m_poll_vector.clear();
  // Add PIDs to poll list:
  m_poll_vector.insert(m_poll_vector.end(), obdii_polls, endof_array(obdii_polls));
  if (mt_obl_fastchg->AsBool()) {
    m_poll_vector.insert(m_poll_vector.end(), fast_charger_polls, endof_array(fast_charger_polls));
  } else {
    m_poll_vector.insert(m_poll_vector.end(), slow_charger_polls, endof_array(slow_charger_polls));
  }
  OvmsPoller::poll_pid_t p1 = { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, {  0,300,0,0 }, 0, ISOTP_STD };
  p1.polltime[2] = m_cfg_cell_interval_drv;
  p1.polltime[3] = m_cfg_cell_interval_chg;
  m_poll_vector.push_back(p1);
  
  OvmsPoller::poll_pid_t p2 = { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, {  0,300,0,0 }, 0, ISOTP_STD };
  p2.polltime[2] = m_cfg_cell_interval_drv;
  p2.polltime[3] = m_cfg_cell_interval_chg;
  m_poll_vector.push_back(p2);

  // Terminate poll list:
  m_poll_vector.push_back(POLL_LIST_END);
  ESP_LOGI(TAG, "Poll vector: size=%d cap=%d", m_poll_vector.size(), m_poll_vector.capacity());

  PollSetPidList(m_can1, m_poll_vector.data());
}

void OvmsVehicleSmartEQ::EventListener(std::string event, void* data) {

  if (event == "vehicle.charge.start") 
    {
    if(m_enable_calcADCfactor && !m_ADCfactor_recalc) 
      {
      m_ADCfactor_recalc_timer = 4;   // wait at least 4 min. before recalculation
      m_ADCfactor_recalc = true;      // recalculate ADC factor when HV charging
      }
    }
  if (event == "vehicle.charge.stop") 
    {
    m_ADCfactor_recalc_timer = 0;
    m_ADCfactor_recalc = false;     // stop recalculation when HV charging stopped
    }
}

uint64_t OvmsVehicleSmartEQ::swap_uint64(uint64_t val) {
  val = ((val << 8) & 0xFF00FF00FF00FF00ull) | ((val >> 8) & 0x00FF00FF00FF00FFull);
  val = ((val << 16) & 0xFFFF0000FFFF0000ull) | ((val >> 16) & 0x0000FFFF0000FFFFull);
  return (val << 32) | (val >> 32);
}

/**
 * Update derived energy metrics while driving
 * Called once per second from Ticker1
 */
void OvmsVehicleSmartEQ::HandleEnergy() {
  float voltage  = StdMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current  = -StdMetrics.ms_v_bat_current->AsFloat(0, Amps);

  // Power (in kw) resulting from voltage and current
  float power = voltage * current / 1000.0;

  // Are we driving?
  if (power != 0.0 && StdMetrics.ms_v_env_on->AsBool()) {
    // Update energy used and recovered   
    float energy = power / 3600.0;       // 1 second worth of energy in kwh's
    if (energy < 0.0f){
      StdMetrics.ms_v_bat_energy_used->SetValue( StdMetrics.ms_v_bat_energy_used->AsFloat() - energy);
      StdMetrics.ms_v_bat_energy_used_total->SetValue( StdMetrics.ms_v_bat_energy_used_total->AsFloat() - energy);
    }else{ // (energy > 0.0f)
      StdMetrics.ms_v_bat_energy_recd->SetValue( StdMetrics.ms_v_bat_energy_recd->AsFloat() + energy);
      StdMetrics.ms_v_bat_energy_recd_total->SetValue( StdMetrics.ms_v_bat_energy_recd_total->AsFloat() + energy);
    }
  }
}

void OvmsVehicleSmartEQ::HandleTripcounter(){
  if (mt_pos_odometer_start->AsFloat(0) == 0 && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) {
    mt_pos_odometer_start->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
  }
  if (StdMetrics.ms_v_env_on->AsBool() && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start->AsFloat(0) > 0.0) {
    StdMetrics.ms_v_pos_trip->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }

  if (mt_pos_odometer_start_total->AsFloat(0) == 0 && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) {
    mt_pos_odometer_start_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat());
  }
  if (StdMetrics.ms_v_env_on->AsBool() && StdMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start_total->AsFloat(0) > 0.0) {
    mt_pos_odometer_trip_total->SetValue(StdMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start_total->AsFloat(0));
  }
}

void OvmsVehicleSmartEQ::Handlev2Server(){
  // Handle v2Server connection
  if (StdMetrics.ms_s_v2_connected->AsBool()) {
    m_reboot_ticker = m_reboot_time; // set reboot ticker
  }
  else if (m_reboot_ticker > 0 && --m_reboot_ticker == 0) {
    MyNetManager.RestartNetwork();
    m_reboot_ticker = m_reboot_time;
  }
}

/**
 * Update derived metrics when charging
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleSmartEQ::HandleCharging() {
  float act_soc        = StdMetrics.ms_v_bat_soc->AsFloat(0, Percentage);
  float limit_soc       = StdMetrics.ms_v_charge_limit_soc->AsFloat(0, Percentage);
  float limit_range     = StdMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
  float max_range       = StdMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers);
  float charge_current  = -StdMetrics.ms_v_bat_current->AsFloat(0, Amps);
  float charge_voltage  = StdMetrics.ms_v_bat_voltage->AsFloat(0, Volts);

  // Are we charging?
  if (!StdMetrics.ms_v_charge_pilot->AsBool()      ||
      !StdMetrics.ms_v_charge_inprogress->AsBool() ||
      (charge_current <= 0.0f) ) {
    return;
  }

  // Check if we have what is needed to calculate energy and remaining minutes
  if (charge_voltage > 0.0f && charge_current > 0.0f) {
    // Update energy taken
    // Value is reset to 0 when a new charging session starts...
    float power  = charge_voltage * charge_current / 1000.0f;     // power in kw
    float energy = power / 3600.0f * 1.0f;                         // 1 second worth of energy in kwh's
    StdMetrics.ms_v_charge_kwh->SetValue( StdMetrics.ms_v_charge_kwh->AsFloat() + energy);

    // If no limits are set, then calculate remaining time to full charge
    if (limit_soc <= 0.0f && limit_range <= 0.0f) {
      // If the charge power is above 12kW, then use the OBD duration value
      if(StdMetrics.ms_v_charge_power->AsFloat() > 12.0f){
        StdMetrics.ms_v_charge_duration_full->SetValue(mt_obd_duration->AsInt(), Minutes);
        ESP_LOGV(TAG, "Time remaining: %d mins to 100%% soc", mt_obd_duration->AsInt());
      } else {
        float soc100 = 100.0f;
        int remaining_soc = calcMinutesRemaining(soc100, charge_voltage, charge_current);
        StdMetrics.ms_v_charge_duration_full->SetValue(remaining_soc, Minutes);
        ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", remaining_soc, soc100);
      }
    }    
    // if limit_soc is set, then calculate remaining time to limit_soc
    if (limit_soc > 0.0f) {
      int minsremaining_soc = calcMinutesRemaining(limit_soc, charge_voltage, charge_current);

      StdMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
      if (act_soc >= limit_soc && !m_notifySOClimit) {
        m_notifySOClimit = true;
        StdMetrics.ms_v_charge_duration_soc->SetValue(0, Minutes);
        ESP_LOGV(TAG, "Time remaining: 0 mins to %0.0f%% soc (already above limit)", limit_soc);
        NotifySOClimit();
      }
    }
    // If limit_range is set, then calculate remaining time to that range
    if (limit_range > 0.0f && max_range > 0.0f) {
      float range_soc           = limit_range / max_range * 100.0f;
      int   minsremaining_range = calcMinutesRemaining(range_soc, charge_voltage, charge_current);

      StdMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
    }
  }
}

void OvmsVehicleSmartEQ::HandleChargeport(){
  if (StdMetrics.ms_v_door_chargeport->AsBool() && !m_charge_start) {
    m_charge_start = true;
    if (m_charge_finished) ResetChargingValues();
    if (m_resettrip) ResetTripCounters();
    ESP_LOGD(TAG,"Charge Start");
  } 
  if (!StdMetrics.ms_v_door_chargeport->AsBool() && m_charge_start) {
    m_charge_start = false;
    m_charge_finished = true;
    StdMetrics.ms_v_charge_power->SetValue(0);
    ESP_LOGD(TAG,"Charge End");
  }
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
  
  float power_grid = mt_obl_main_CHGpower->GetElemValue(0) + mt_obl_main_CHGpower->GetElemValue(1);
  StdMetrics.ms_v_charge_power->SetValue(power_grid);
  
  // Use absolute value of battery power for efficiency calculation
  float power_battery = fabs(StdMetrics.ms_v_bat_power->AsFloat());
  
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
  float bat_soc = StdMetrics.ms_v_bat_soc->AsFloat(0, Percentage);
  if (bat_soc > target_soc) {
    return 0;   // Done!
  }
  float remaining_wh    = DEFAULT_BATTERY_CAPACITY * (target_soc - bat_soc) / 100.0;
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0;

  return MIN( 1440, (int)remaining_mins );
}

void OvmsVehicleSmartEQ::HandlePollState() {
  // Determine poll state:
  // 0 = Off
  // 1 = Awake (accessories on)
  // 2 = Running (ignition on)
  // 3 = Charging

  if (!m_enable_write) {
    if (m_poll_state != 0) {
      PollSetState(0);
      ESP_LOGI(TAG, "Pollstate Off (write disabled)");
    }
    mt_poll_state->SetValue("Pollstate Off (write disabled)");
    return;
  }

  static const char* state_names[] = {"Off", "Awake", "Running", "Charging"};
  
  int desired_state = m_poll_state;

  if (StdMetrics.ms_v_charge_pilot->AsBool(false) || StdMetrics.ms_v_charge_inprogress->AsBool(false))
    desired_state = 3;
  else if (StdMetrics.ms_v_env_on->AsBool(false))
    desired_state = 2;
  else if (mt_bus_awake->AsBool(false) || StdMetrics.ms_v_env_awake->AsBool(false))
    desired_state = 1;
  else if (!StdMetrics.ms_v_env_awake->AsBool(false) && !mt_bus_awake->AsBool(false) && 
          (!StdMetrics.ms_v_charge_pilot->AsBool(false) || !StdMetrics.ms_v_charge_inprogress->AsBool(false)))
    desired_state = 0;

  if (desired_state != m_poll_state) {
    PollSetState(desired_state);
    ESP_LOGI(TAG, "Pollstate %s", state_names[desired_state]);
    mt_poll_state->SetValue(state_names[desired_state]);
  }
}

void OvmsVehicleSmartEQ::CalculateEfficiency() {
  // float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5) {
    float _use_grid = StdMetrics.ms_v_bat_energy_used->AsFloat() - StdMetrics.ms_v_bat_energy_recd->AsFloat();
    float _use_grid_total = StdMetrics.ms_v_bat_energy_used_total->AsFloat() - StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
    float _trip_km = StdMetrics.ms_v_pos_trip->AsFloat();
    float _trip_total_km = mt_pos_odometer_trip_total->AsFloat();
    if (_use_grid > 0.1f) 
      {
      if (_trip_km > 0.1f) StdMetrics.ms_v_charge_kwh_grid->SetValue((_use_grid / _trip_km) * 100.0f);
      }
    if (_use_grid_total > 0.1f) 
      {
      if (_trip_total_km > 0.1f) StdMetrics.ms_v_charge_kwh_grid_total->SetValue((_use_grid_total / _trip_total_km) * 100.0f);
      }
  }
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

void OvmsVehicleSmartEQ::vehicle_smart_car_on(bool isOn) {
  if (isOn && !StdMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG,"CAR IS ON");
    //StdMetrics.ms_v_env_awake->SetValue(isOn);

    // Reset trip values
    if (!m_resettrip) {
      ResetTripCounters();
    }
    // Reset kWh/100km values
    if (m_resettotal) {
      ResetTotalCounters();
    }

    m_warning_unlocked = false;
    m_warning_dooropen = false;
    m_12v_ticker = 0;
    m_climate_restart = false;
    m_climate_restart_ticker = 0;
  }
  else if (!isOn && StdMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
  }

  // Always set this value to prevent it from going stale
  StdMetrics.ms_v_env_on->SetValue(isOn);
}

class OvmsVehicleSmartEQInit {
  public:
    OvmsVehicleSmartEQInit();
} MyOvmsVehicleSmartEQInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEQInit::OvmsVehicleSmartEQInit() {
  ESP_LOGI(TAG, "Registering Vehicle: smart EQ (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartEQ>("SQ", "smart 453 4.Gen");
}
