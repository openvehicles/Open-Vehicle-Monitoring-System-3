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

// Abort frame processing early on short DLC.
#define REQ_DLC(minlen)                                                \
  if (p_frame->FIR.B.DLC < (minlen)) {                                 \
    ESP_LOGV(TAG,"CAN %03X: DLC %u < %u -> ignore frame",              \
      p_frame->MsgID, p_frame->FIR.B.DLC, (unsigned)(minlen));         \
    return;                                                            \
  }

OvmsVehicleSmartEQ* OvmsVehicleSmartEQ::GetInstance(OvmsWriter* writer)
{
  OvmsVehicleSmartEQ* smarteq = (OvmsVehicleSmartEQ*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!smarteq || type != "SQ") {
    if (writer)
      writer->puts("Error: Smart EQ vehicle module not selected");
    return NULL;
  }
  return smarteq;
}

static const OvmsPoller::poll_pid_t obdii_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x07, {  0,300,60,60 }, 0, ISOTP_STD }, // rqBattState
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0,300,300,300 }, 0, ISOTP_STD }, // rqBattTemperatures
//  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, {  0,300,300,60 }, 0, ISOTP_STD }, // rqBattVoltages_P1
//  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, {  0,300,300,60 }, 0, ISOTP_STD }, // rqBattVoltages_P2
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200c, {  0,300,300,300 }, 0, ISOTP_STD }, // extern temp byte 2+3
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2101, {  0,300,60,300 }, 0, ISOTP_STD }, // OBD Trip Distance km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A0, {  0,300,60,300 }, 0, ISOTP_STD }, // OBD start Trip Distance km 
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2104, {  0,300,60,300 }, 0, ISOTP_STD }, // OBD Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A2, {  0,300,60,300 }, 0, ISOTP_STD }, // OBD start Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0204, {  0,3600,300,0 }, 0, ISOTP_STD }, // maintenance data days
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0203, {  0,3600,300,0 }, 0, ISOTP_STD }, // maintenance data usual km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0188, {  0,3600,300,0 }, 0, ISOTP_STD }, // maintenance level
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x81, {  0,3600,0,0 }, 0, ISOTP_STD }, // req.VIN
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8003, {  0,300,10,10 }, 0, ISOTP_STD },   // rq VehicleState
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x605e, {  0,300,10,10 }, 0, ISOTP_STD },   // rq UNDERHOOD_OPENED
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x320c, {  0,300,60,30 }, 0, ISOTP_STD }, // rqHV_Energy
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x302A, {  0,300,60,30 }, 0, ISOTP_STD }, // rqDCDC_State
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3495, {  0,300,60,30 }, 0, ISOTP_STD }, // rqDCDC_Load
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3023, {  0,300,60,30 }, 0, ISOTP_STD }, // 14V DCDC voltage request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3024, {  0,300,60,30 }, 0, ISOTP_STD }, // 14V DCDC voltage measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3025, {  0,300,60,30 }, 0, ISOTP_STD }, // 14V DCDC current measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3494, {  0,300,60,30 }, 0, ISOTP_STD }, // rqDCDC_Power
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x33BA, {  0,300,60,30 }, 0, ISOTP_STD }, // indicates ext power supply
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x339D, {  0,300,10,10 }, 0, ISOTP_STD }, // charging plug present
};

static const OvmsPoller::poll_pid_t slow_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7303, {  0,0,0,10 }, 0, ISOTP_STD }, // rqChargerAC
};

static const OvmsPoller::poll_pid_t fast_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503F, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph12_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5041, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph23_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5042, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph31_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2001, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph1_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503A, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph2_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503B, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph3_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Power
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x500E, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Power
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5038, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Power
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5049, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Frequency
  /* The following PIDs are defined, but not used (yet):
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, {  0,0,0,10 }, 0, ISOTP_STD }, // Mains phase frequency (Hz)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504B, {  0,0,0,10 }, 0, ISOTP_STD }, // Mains current sum (A)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504C, {  0,0,0,10 }, 0, ISOTP_STD }, // Mains voltage sum (V)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504D, {  0,0,0,10 }, 0, ISOTP_STD }, // HV net current (A)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504E, {  0,0,0,10 }, 0, ISOTP_STD }, // HV net voltage (V)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5057, {  0,0,0,10 }, 0, ISOTP_STD }, // Raw leakage current - DC part measurement (mA)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5058, {  0,0,0,10 }, 0, ISOTP_STD }, // Raw leakage current - HF 10kHz part measurement (mA)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5059, {  0,0,0,10 }, 0, ISOTP_STD }, // Raw leakage current - HF part measurement (mA)
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x505A, {  0,0,0,10 }, 0, ISOTP_STD }, // Raw leakage current - LF part measurement (mA)
  */
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5070, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Max Current limitation
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5062, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ground Resistance
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5064, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Leakage Diag
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5065, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_DC Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5066, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_HF10kHz Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5067, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_HF Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5068, {  0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_LF Current
};

/**
 * Constructor & destructor
 */

OvmsVehicleSmartEQ::OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Start Smart EQ vehicle module");

  m_climate_init = true;
  m_climate_start = false;
  m_climate_start_day = false;
  m_climate_ticker = 0;
  m_12v_ticker = 0;
  m_12v_charge_state = false;
  m_v2_ticker = 0;
  m_v2_restart = false;
  m_ddt4all = false;
  m_ddt4all_ticker = 0;
  m_ddt4all_exec = 0;
  m_warning_unlocked = false;
  m_modem_restart = false;
  m_modem_ticker = 0;
  m_ADCfactor_recalc = false;
  m_ADCfactor_recalc_timer = 0;

  m_enable_write = false;
  m_candata_timer = 0;
  m_candata_poll  = 0;

  m_charge_start = false;
  m_charge_finished = true;
  m_notifySOClimit = false;
  m_led_state = 4;
  m_cfg_cell_interval_drv = 0;
  m_cfg_cell_interval_chg = 0;

  for (int i = 0; i < 4; i++) {
    m_tpms_pressure[i] = 0.0f;
    m_tpms_index[i] = i;
  }

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 3);
  BmsSetCellArrangementTemperature(27, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  mt_bus_awake                  = MyMetrics.InitBool("xsq.v.bus.awake", SM_STALE_MIN, false);
  mt_use_at_reset               = MyMetrics.InitFloat("xsq.use.at.reset", SM_STALE_MID, 0, kWh);
  mt_use_at_start               = MyMetrics.InitFloat("xsq.use.at.start", SM_STALE_MID, 0, kWh);
  mt_canbyte                    = MyMetrics.InitString("xsq.ddt4all.canbyte", SM_STALE_NONE, "", Other);
  mt_dummy_pressure             = MyMetrics.InitFloat("xsq.dummy.pressure", SM_STALE_NONE, 235, kPa);  // Dummy pressure for TPMS alert testing
  mt_vehicle_state              = MyMetrics.InitString("xsq.v.state", SM_STALE_MIN, "UNKNOWN", Other);
  mt_vehicle_state_code         = MyMetrics.InitInt("xsq.v.state.code", SM_STALE_MIN, 0, Other);
  mt_adc_factor                 = MyMetrics.InitFloat("xsq.adc.factor", SM_STALE_NONE, 0, Other);
  mt_adc_factor_history         = new OvmsMetricVector<float>("xsq.adc.factor.history", SM_STALE_NONE, Other);

  mt_obd_duration               = MyMetrics.InitInt("xsq.obd.duration", SM_STALE_MID, 0, Minutes);
  mt_obd_trip_km                = MyMetrics.InitFloat("xsq.obd.trip.km", SM_STALE_MID, 0, Kilometers);
  mt_obd_start_trip_km          = MyMetrics.InitFloat("xsq.obd.trip.km.start", SM_STALE_MID, 0, Kilometers);
  mt_obd_trip_time              = MyMetrics.InitString("xsq.obd.trip.time", SM_STALE_MID, 0, Other);
  mt_obd_start_trip_time        = MyMetrics.InitString("xsq.obd.trip.time.start", SM_STALE_MID, 0, Other);
  mt_obd_mt_day_prewarn         = MyMetrics.InitInt("xsq.obd.mt.day.prewarn", SM_STALE_MID, 45, Other);
  mt_obd_mt_day_usual           = MyMetrics.InitInt("xsq.obd.mt.day.usual", SM_STALE_MID, 0, Other);
  mt_obd_mt_km_usual            = MyMetrics.InitInt("xsq.obd.mt.km.usual", SM_STALE_MID, 0, Kilometers);
  mt_obd_mt_level               = MyMetrics.InitString("xsq.obd.mt.level", SM_STALE_MID, "unknown", Other);

  mt_climate_on                 = MyMetrics.InitBool("xsq.climate.on", SM_STALE_MID, false);
  mt_climate_weekly             = MyMetrics.InitBool("xsq.climate.weekly", SM_STALE_MID, false);
  mt_climate_time               = MyMetrics.InitString("xsq.climate.time", SM_STALE_MID, "0515", Other);
  mt_climate_h                  = MyMetrics.InitInt("xsq.climate.h", SM_STALE_MID, 5, Other);
  mt_climate_m                  = MyMetrics.InitInt("xsq.climate.m", SM_STALE_MID, 15, Other);
  mt_climate_ds                 = MyMetrics.InitInt("xsq.climate.ds", SM_STALE_MID, 1, Other);
  mt_climate_de                 = MyMetrics.InitInt("xsq.climate.de", SM_STALE_MID, 6, Other);
  mt_climate_1to3               = MyMetrics.InitInt("xsq.climate.1to3", SM_STALE_MID, 0, Other);
  mt_climate_data               = MyMetrics.InitString("xsq.climate.data", SM_STALE_MID,"0,0,0,0,-1,-1,-1", Other);

  mt_pos_odometer_start         = MyMetrics.InitFloat("xsq.odometer.start", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_start_total   = MyMetrics.InitFloat("xsq.odometer.start.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odometer_trip_total    = MyMetrics.InitFloat("xsq.odometer.trip.total", SM_STALE_MID, 0, Kilometers);
  mt_pos_odo_trip               = MyMetrics.InitFloat("xsq.odo.trip", SM_STALE_MID, 0, Kilometers);

  mt_evc_hv_energy              = MyMetrics.InitFloat("xsq.evc.hv.energy", SM_STALE_MID, 0, kWh);
  mt_evc_LV_DCDC_amps           = MyMetrics.InitFloat("xsq.evc.lv.dcdc.amps", SM_STALE_MID, 0, Amps);
  mt_evc_LV_DCDC_load           = MyMetrics.InitFloat("xsq.evc.lv.dcdc.load", SM_STALE_MID, 0, Percentage);
  mt_evc_LV_DCDC_volt_req       = MyMetrics.InitFloat("xsq.evc.lv.dcdc.volt.req", SM_STALE_MID, 0, Volts);
  mt_evc_LV_DCDC_volt           = MyMetrics.InitFloat("xsq.evc.lv.dcdc.volt", SM_STALE_MID, 0, Volts);
  mt_evc_LV_DCDC_power          = MyMetrics.InitFloat("xsq.evc.lv.dcdc.power", SM_STALE_MID, 0, Watts);
  mt_evc_LV_DCDC_state          = MyMetrics.InitInt("xsq.evc.lv.dcdc.state", SM_STALE_MID, 0, Other);
  mt_evc_ext_power              = MyMetrics.InitBool("xsq.evc.ext.power", SM_STALE_MIN, false);
  mt_evc_plug_present           = MyMetrics.InitBool("xsq.evc.plug.present", SM_STALE_MIN, false);

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
  mt_bms_BattPower_voltage      = MyMetrics.InitFloat("xsq.bms.batt.voltage", SM_STALE_MID, 0, Volts);
  mt_bms_BattPower_current      = MyMetrics.InitFloat("xsq.bms.batt.current", SM_STALE_MID, 0, Amps);
  mt_bms_BattPower_power        = MyMetrics.InitFloat("xsq.bms.batt.power", SM_STALE_MID, 0, kW);
  mt_bms_HVcontactStateCode     = MyMetrics.InitInt("xsq.bms.contact.code", SM_STALE_MID, 0, Other);
  mt_bms_HVcontactStateTXT      = MyMetrics.InitString("xsq.bms.contact", SM_STALE_MID, "", Other);
  mt_bms_HV                     = MyMetrics.InitFloat("xsq.bms.hv", SM_STALE_MID, 0, Volts);
  mt_bms_EVmode                 = MyMetrics.InitInt("xsq.bms.ev.mode.code", SM_STALE_MID, 0, Other);
  mt_bms_EVmode_txt             = MyMetrics.InitString("xsq.bms.ev.mode", SM_STALE_MID, "", Other);
  mt_bms_12v                    = MyMetrics.InitFloat("xsq.bms.12v", SM_STALE_MID, 0, Volts);
  mt_bms_Amps                   = MyMetrics.InitFloat("xsq.bms.amps", SM_STALE_MID, 0, Amps);
  mt_bms_Amps2                  = MyMetrics.InitFloat("xsq.bms.amp2", SM_STALE_MID, 0, Amps);
  mt_bms_Power                  = MyMetrics.InitFloat("xsq.bms.power", SM_STALE_MID, 0, kW);
  mt_bms_interlock_hvplug       = MyMetrics.InitBool("xsq.bms.interlock.hvplug",SM_STALE_MID, false);
  mt_bms_interlock_service      = MyMetrics.InitBool("xsq.bms.interlock.service", SM_STALE_MID, false);
  mt_bms_fusi_mode              = MyMetrics.InitInt("xsq.bms.fusi.code",SM_STALE_MID, 0, Other);
  mt_bms_fusi_mode_txt          = MyMetrics.InitString("xsq.bms.fusi",SM_STALE_MID, "", Other);
  mt_bms_mg_rpm                 = MyMetrics.InitFloat("xsq.bms.mg.rpm",SM_STALE_MID, 0, Other);
  mt_bms_safety_mode            = MyMetrics.InitInt("xsq.bms.safety.code",SM_STALE_MID, 0, Other);
  mt_bms_safety_mode_txt        = MyMetrics.InitString("xsq.bms.safety",SM_STALE_MID, "", Other);
  mt_bms_relay_hv_status        = MyMetrics.InitInt("xsq.bms.relay.hv.code",SM_STALE_MID, 0, Other);
  mt_bms_relay_hv_status_txt    = MyMetrics.InitString("xsq.bms.relay.hv",SM_STALE_MID, "", Other);
  mt_bms_relay_permit_flag      = MyMetrics.InitInt("xsq.bms.relay.permit.code",SM_STALE_MID, 0, Other);
  mt_bms_relay_permit_flag_txt  = MyMetrics.InitString("xsq.bms.relay.permit",SM_STALE_MID, "", Other);

  // Start CAN bus in Listen-only mode - will be set according to m_enable_write in ConfigChanged()
  RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);

  // init commands:
  cmd_xsq = MyCommandApp.RegisterCommand("xsq","SmartEQ 453 Gen.4");
  cmd_xsq->RegisterCommand("start", "Show OBD trip start", xsq_trip_start);
  cmd_xsq->RegisterCommand("reset", "Show OBD trip total", xsq_trip_reset);
  cmd_xsq->RegisterCommand("counter", "Show vehicle trip counter", xsq_trip_counters);
  cmd_xsq->RegisterCommand("total", "Show vehicle trip total", xsq_trip_total);
  cmd_xsq->RegisterCommand("mtdata", "Show Maintenance data", xsq_maintenance);
  cmd_xsq->RegisterCommand("climate", "Show Climate timer data", xsq_climate);
  cmd_xsq->RegisterCommand("tpmsset", "set TPMS dummy value", xsq_tpms_set);
  cmd_xsq->RegisterCommand("ddt4all", "DDT4all Command", xsq_ddt4all,"<number>",1,1);
  cmd_xsq->RegisterCommand("ddt4list", "DDT4all Command List", xsq_ddt4list);
  cmd_xsq->RegisterCommand("calcadc", "Recalculate ADC factor (optional: 12V voltage override)", xsq_calc_adc, "[voltage]", 0, 1);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"vehicle.charge.start", std::bind(&OvmsVehicleSmartEQ::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.charge.stop", std::bind(&OvmsVehicleSmartEQ::EventListener, this, _1, _2));
  MyConfig.RegisterParam("xsq", "smartEQ", true, true);

  ConfigChanged(NULL);
  
  StdMetrics.ms_v_gen_current->SetValue(2);                // activate gen metrics to app transfer
  StdMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);  // set 12V alert to false

  m_network_type_ls = MyConfig.GetParamValue("xsq", "modem.net.type", "auto");

  if (MyConfig.GetParamValue("xsq", "12v.charge","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "12v.charge", true);
  }

  if (MyConfig.GetParamValueFloat("vehicle", "12v.alert", 1.6) == 1.6) {
    MyConfig.SetParamValueFloat("vehicle", "12v.alert", 0.8); // set default 12V alert threshold to 0.8V for Check12V System
  }

  if (MyConfig.GetParamValue("xsq", "v2.check","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "v2.check", false);
  }

  if (MyConfig.GetParamValue("xsq", "tpms.front.pressure","0") == "0") {
    MyConfig.SetParamValueInt("xsq", "tpms.front.pressure",  225); // kPa
    MyConfig.SetParamValueInt("xsq", "tpms.rear.pressure",  255); // kPa
    MyConfig.SetParamValueInt("xsq", "tpms.value.warn",  50); // kPa
    MyConfig.SetParamValueInt("xsq", "tpms.value.alert", 75); // kPa
  }

  if (MyConfig.GetParamValue("xsq", "tpms.alert.enable","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "tpms.alert.enable", true);
  }

  if(MyConfig.GetParamValue("xsq", "unlock.warning","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "unlock.warning", true);                                 
  }

  if (MyConfig.GetParamValue("xsq", "extended.stats","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "extended.stats", false);
  }

  if (MyConfig.GetParamValue("xsq", "modem.check","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "modem.check", false);
  }

  if (MyConfig.GetParamValue("xsq", "restart.wakeup","0") == "0") {
    MyConfig.SetParamValueBool("xsq", "restart.wakeup", false);
  }

  if (MyConfig.GetParamValue("xsq", "reset.notify", "0") == "0") {
    MyConfig.SetParamValueBool("xsq", "reset.notify", true);
  }
  
  if (MyConfig.GetParamValue("server.v3", "updatetime.priority","0") == "0") {
    MyConfig.SetParamValueBool("server.v3", "updatetime.priority", true);
  }
 
  if (mt_pos_odometer_trip_total->AsFloat(0) < 1.0f) {         // reset at boot
    ResetTotalCounters();
    ResetTripCounters();
  }

  if (MyConfig.GetParamValueBool("xsq", "restart.wakeup",false)) {
    CommandWakeup();                                           // wake up the car to get the first data
  }
  
  setTPMSValueBoot();                                          // set TPMS dummy values to 0

  #ifdef CONFIG_OVMS_COMP_CELLULAR
    
    // Features option: auto disable GPS when parked -> moved to cellular module
    if(MyConfig.GetParamValueBool("xsq", "gps.onoff", true) && MyConfig.GetParamValueBool("xsq", "gps.deact", true)) {
      MyConfig.SetParamValueBool("xsq", "gps.deact", false); // delete old config entry and set new entry one time
      MyConfig.SetParamValue("modem", "gps.parkpause", "600"); // default 10 min.
      MyConfig.SetParamValue("modem", "gps.parkreactivate", "50"); // default 50 min.
      MyConfig.SetParamValue("modem", "gps.parkreactlock", "5"); // default 5 min.
      MyConfig.SetParamValue("vehicle", "stream", "10"); // set stream to 10 sec.
      if(MyConfig.GetParamValue("xsq", "gps.onoff","0") != "0") MyConfig.DeleteInstance("xsq", "gps.onoff");  // delete old config entry
      if(MyConfig.GetParamValue("xsq", "gps.off","0") != "0") MyConfig.DeleteInstance("xsq", "gps.off");  // delete old config entry
      if(MyConfig.GetParamValue("xsq", "gps.reactmin","0") != "0") MyConfig.DeleteInstance("xsq", "gps.reactmin");  // delete old config entry
    }
    
    #ifdef CONFIG_OVMS_COMP_WIFI
      // Features option: auto restart modem on wifi disconnect
      MyEvents.RegisterEvent(TAG,"system.wifi.sta.disconnected", std::bind(&OvmsVehicleSmartEQ::ModemEventRestart, this, _1, _2));
#endif

  #endif
  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
  #endif
}

OvmsVehicleSmartEQ::~OvmsVehicleSmartEQ() {
  ESP_LOGI(TAG, "Stop Smart EQ vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (m_enable_LED_state) {
    MyPeripherals->m_max7317->Output(9, 0);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
  }
#endif
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

/**
 * ConfigChanged: reload single/all configuration variables (cfgupdate)
 */
void OvmsVehicleSmartEQ::ConfigChanged(OvmsConfigParam* param) {
  if (param && param->GetName() != "xsq")
    return;

  ESP_LOGI(TAG, "Smart EQ reload configuration");

  bool stateWrite = m_enable_write;
  m_enable_write      = MyConfig.GetParamValueBool("xsq", "canwrite", false);
  // set CAN bus transceiver to active or listen-only depending on user selection
  if ( stateWrite != m_enable_write )
  {
      CAN_mode_t mode = m_enable_write ? CAN_MODE_ACTIVE : CAN_MODE_LISTEN;
      RegisterCanBus(1, mode, CAN_SPEED_500KBPS);
  }
  m_enable_LED_state  = MyConfig.GetParamValueBool("xsq", "led", false);
  m_enable_lock_state = MyConfig.GetParamValueBool("xsq", "unlock.warning", false);
  m_ios_tpms_fix      = MyConfig.GetParamValueBool("xsq", "ios_tpms_fix", false);
  m_reboot_time       = MyConfig.GetParamValueInt("xsq", "rebootnw", 30);
  m_resettrip         = MyConfig.GetParamValueBool("xsq", "resettrip", false);
  m_resettotal        = MyConfig.GetParamValueBool("xsq", "resettotal", false);
  m_tripnotify        = MyConfig.GetParamValueBool("xsq", "reset.notify", false);
  m_tpms_index[3]     = MyConfig.GetParamValueInt("xsq", "TPMS_FL", 0);
  m_tpms_index[2]     = MyConfig.GetParamValueInt("xsq", "TPMS_FR", 1);
  m_tpms_index[1]     = MyConfig.GetParamValueInt("xsq", "TPMS_RL", 2);
  m_tpms_index[0]     = MyConfig.GetParamValueInt("xsq", "TPMS_RR", 3);
  m_front_pressure    = MyConfig.GetParamValueFloat("xsq", "tpms.front.pressure",  220); // kPa
  m_rear_pressure     = MyConfig.GetParamValueFloat("xsq", "tpms.rear.pressure",  250); // kPa
  m_pressure_warning  = MyConfig.GetParamValueFloat("xsq", "tpms.value.warn",  25); // kPa
  m_pressure_alert    = MyConfig.GetParamValueFloat("xsq", "tpms.value.alert", 45); // kPa
  m_tpms_alert_enable = MyConfig.GetParamValueBool("xsq", "tpms.alert.enable", true);
  
  m_modem_check       = MyConfig.GetParamValueBool("xsq", "modem.check", false);
  m_12v_charge        = MyConfig.GetParamValueBool("xsq", "12v.charge", true);
  m_v2_check          = MyConfig.GetParamValueBool("xsq", "v2.check", false);
  m_12v_measured_BMS_offset = MyConfig.GetParamValueFloat("xsq", "12v.measured.BMS.offset", 0.25);
  m_enable_calcADCfactor = MyConfig.GetParamValueBool("xsq", "calc.adcfactor", false);
  m_climate_system    = MyConfig.GetParamValueBool("xsq", "climate.system", false);
  m_network_type      = MyConfig.GetParamValue("xsq", "modem.net.type", "auto");
  m_indicator         = MyConfig.GetParamValueBool("xsq", "indicator", false);              //!< activate indicator e.g. 7 times or whtever
  m_extendedStats     = MyConfig.GetParamValueBool("xsq", "extended.stats", false);         //!< activate extended stats e.g. trip and maintenance data
  m_park_timeout_secs = MyConfig.GetParamValueInt("xsq", "park.timeout", 600);              //!< timeout in seconds for parking mode

#ifdef CONFIG_OVMS_COMP_MAX7317
  if (!m_enable_LED_state) {
    MyPeripherals->m_max7317->Output(9, 1);
    MyPeripherals->m_max7317->Output(8, 1);
    MyPeripherals->m_max7317->Output(7, 1);
  }
#endif
  int cell_interval_drv = MyConfig.GetParamValueInt("xsq", "cell_interval_drv", 60);
  int cell_interval_chg = MyConfig.GetParamValueInt("xsq", "cell_interval_chg", 60);

  bool do_modify_poll = (
    (cell_interval_drv != m_cfg_cell_interval_drv) ||
    (cell_interval_chg != m_cfg_cell_interval_chg));

  m_cfg_cell_interval_drv = cell_interval_drv;
  m_cfg_cell_interval_chg = cell_interval_chg;

  if (do_modify_poll) {
    ObdModifyPoll();
  }
  StdMetrics.ms_v_charge_limit_soc->SetValue((float) MyConfig.GetParamValueInt("xsq", "suffsoc", 0), Percentage );
  StdMetrics.ms_v_charge_limit_range->SetValue((float) MyConfig.GetParamValueInt("xsq", "suffrange", 0), Kilometers );
}

void OvmsVehicleSmartEQ::EventListener(std::string event, void* data) {
  if (event == "vehicle.charge.start") {
    if(m_enable_calcADCfactor && !m_ADCfactor_recalc) {
      m_ADCfactor_recalc_timer = 4;   // wait at least 4 min. before recalculation
      m_ADCfactor_recalc = true;      // recalculate ADC factor when HV charging
    }
  }
  if (event == "vehicle.charge.stop") {
      m_ADCfactor_recalc_timer = 0;
      m_ADCfactor_recalc = false;     // stop recalculation when HV charging stopped
  }
}

uint64_t OvmsVehicleSmartEQ::swap_uint64(uint64_t val) {
  val = ((val << 8) & 0xFF00FF00FF00FF00ull) | ((val >> 8) & 0x00FF00FF00FF00FFull);
  val = ((val << 16) & 0xFFFF0000FFFF0000ull) | ((val >> 16) & 0x0000FFFF0000FFFFull);
  return (val << 32) | (val >> 32);
}

void OvmsVehicleSmartEQ::IncomingFrameCan1(CAN_frame_t* p_frame) {
  uint8_t *data = p_frame->data.u8;
  uint64_t c = swap_uint64(p_frame->data.u64);
  
  static bool isCharging = false;
  static bool lastCharging = false;
  float _range_est;
  float _bat_temp;
  float _full_km;
  float _range_cac;
  float _soc;
  float _soh;
  float _temp;
  int _duration_full;
  //char buf[10];

  if (m_candata_poll != 1 && StdMetrics.ms_v_bat_voltage->AsFloat(0, Volts) > 100) {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    mt_bus_awake->SetValue(true);
    m_candata_poll = 1;
  }
  m_candata_timer = SQ_CANDATA_TIMEOUT;
  
  switch (p_frame->MsgID) {
    case 0x17e: //gear shift
    {
      REQ_DLC(7);      // uses bytes up to at least index 6, so DLC must be 7 or more
      switch(CAN_BYTE(6)) {
        case 0x00: // Parking
          StdMetrics.ms_v_env_gear->SetValue(0);
          StdMetrics.ms_v_gen_limit_soc->SetValue(1);
          break;
        case 0x10: // Rear
          StdMetrics.ms_v_env_gear->SetValue(-1);
          StdMetrics.ms_v_gen_limit_soc->SetValue(2);
          break;
        case 0x20: // Neutral
          StdMetrics.ms_v_env_gear->SetValue(1);
          StdMetrics.ms_v_gen_limit_soc->SetValue(3);
          break;
        case 0x70: // Drive
          StdMetrics.ms_v_env_gear->SetValue(2);
          StdMetrics.ms_v_gen_limit_soc->SetValue(4);
          break;
      }
      break;
    }
    case 0x350:
      REQ_DLC(7);
      StdMetrics.ms_v_env_locked->SetValue((CAN_BYTE(6) == 0x96));
      break;
    case 0x392:
      REQ_DLC(6);
      StdMetrics.ms_v_env_hvac->SetValue((CAN_BYTE(1) & 0x40) > 0);
      StdMetrics.ms_v_env_cabintemp->SetValue(CAN_BYTE(5) - 40.0f);
      break;
    case 0x42E:        // HV voltage / temp frame
      REQ_DLC(6);
      _temp = ((c >> 13) & 0x7Fu) > 40.0f
              ? ((c >> 13) & 0x7Fu) - 40.0f
              : (40.0f - ((c >> 13) & 0x7Fu)) * -1.0f;
      if (_temp != 87.0f)
        StdMetrics.ms_v_bat_temp->SetValue(_temp);
      // needs bytes 3 & 4 (CAN_UINT(3)) ? DLC=5 already covered by 6 above
      StdMetrics.ms_v_bat_voltage->SetValue((float)((CAN_UINT(3) >> 5) & 0x3ff) / 2.0f);
      StdMetrics.ms_v_charge_climit->SetValue((c >> 20) & 0x3Fu);
      break;
    case 0x4F8:
      REQ_DLC(3);
      StdMetrics.ms_v_env_handbrake->SetValue((CAN_BYTE(0) & 0x08) > 0);
      StdMetrics.ms_v_env_awake->SetValue((CAN_BYTE(0) & 0x40) > 0); // Ignition on
      break;
    case 0x5D7: // Speed, ODO
      REQ_DLC(6);
      StdMetrics.ms_v_pos_speed->SetValue((float) CAN_UINT(0) / 100.0f);
      StdMetrics.ms_v_pos_odometer->SetValue((float) (CAN_UINT32(2)>>4) / 100.0f);
      mt_pos_odo_trip->SetValue((float) (CAN_UINT(4)>>4) / 100.0f); // ODO trip //TODO: check if this is correct
      break;
    case 0x5de:
      REQ_DLC(8);
      StdMetrics.ms_v_env_headlights->SetValue((CAN_BYTE(0) & 0x04) > 0);
      StdMetrics.ms_v_door_fl->SetValue((CAN_BYTE(1) & 0x08) > 0);
      StdMetrics.ms_v_door_fr->SetValue((CAN_BYTE(1) & 0x02) > 0);
      StdMetrics.ms_v_door_rl->SetValue((CAN_BYTE(2) & 0x40) > 0);
      StdMetrics.ms_v_door_rr->SetValue((CAN_BYTE(2) & 0x10) > 0);
      StdMetrics.ms_v_door_trunk->SetValue((CAN_BYTE(7) & 0x10) > 0);
      break;
    case 0x646:
      REQ_DLC(3);
      mt_use_at_reset->SetValue(CAN_BYTE(1) * 0.1);
      mt_use_at_start->SetValue(CAN_BYTE(2) * 0.1);
      if( MyConfig.GetParamValueBool("xsq", "bcvalue",  false)){
        StdMetrics.ms_v_gen_kwh_grid_total->SetValue(mt_use_at_reset->AsFloat()); // not the best idea at the moment
      } else {
        StdMetrics.ms_v_gen_kwh_grid_total->SetValue(0.0f);
      }
      break;
    case 0x654:        // SOC / charge port status
      REQ_DLC(4);
      _soc = (float) CAN_BYTE(3);
      if (_soc <= 100.0f) StdMetrics.ms_v_bat_soc->SetValue(_soc); // SOC
      StdMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(0) & 0x20) != 0); // ChargingPlugConnected
      _duration_full = (((c >> 22) & 0x3ffu) < 0x3ff) ? (c >> 22) & 0x3ffu : 0;
      mt_obd_duration->SetValue((int)(_duration_full), Minutes);
      _range_est = ((c >> 12) & 0x3FFu); // VehicleAutonomy
      _bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(0) - 20.0;
      _full_km = MyConfig.GetParamValueFloat("xsq", "full.km", 126.0);
      _range_cac = _full_km + (_bat_temp); // temperature compensation +/- range
      if (_range_est != 1023.0f) 
        {
        StdMetrics.ms_v_bat_range_est->SetValue(_range_est);

        if (_soc > 0.1f)  // prevent div/0
          {
          StdMetrics.ms_v_bat_range_full->SetValue((_range_est / _soc) * 100.0f);
          StdMetrics.ms_v_bat_range_ideal->SetValue((_range_cac * _soc) / 100.0f);
          }
        }
      break;
    case 0x658:
      REQ_DLC(6);
      _soh = (float)(CAN_BYTE(4) & 0x7Fu);
      if (_soh <= 100.0f) StdMetrics.ms_v_bat_soh->SetValue(_soh); // SOH
      StdMetrics.ms_v_bat_health->SetValue(
        (_soh >= 95.0f) ? "excellent"
      : (_soh >= 85.0f) ? "good"
      : (_soh >= 75.0f) ? "average"
      : (_soh >= 65.0f) ? "poor"
      : "consider replacement");

      isCharging = (CAN_BYTE(5) & 0x20); // ChargeInProgress
      if (isCharging) { // STATE charge in progress
        //StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
      }
      if (isCharging != lastCharging) { // EVENT charge state changed
        if (isCharging) { // EVENT started charging
          // Set charging metrics
          StdMetrics.ms_v_charge_pilot->SetValue(true);
          StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StdMetrics.ms_v_charge_mode->SetValue("standard");
          StdMetrics.ms_v_charge_type->SetValue("type2");
          StdMetrics.ms_v_charge_state->SetValue("charging");
          StdMetrics.ms_v_charge_substate->SetValue("onrequest");
          StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
        } else { // EVENT stopped charging
          StdMetrics.ms_v_charge_pilot->SetValue(false);
          StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StdMetrics.ms_v_charge_mode->SetValue("standard");
          StdMetrics.ms_v_charge_type->SetValue("type2");
          StdMetrics.ms_v_charge_duration_full->SetValue(0);
          StdMetrics.ms_v_charge_duration_soc->SetValue(0);
          StdMetrics.ms_v_charge_duration_range->SetValue(0);
          StdMetrics.ms_v_charge_power->SetValue(0);
          StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
          if (StdMetrics.ms_v_bat_soc->AsInt() < 95) {
            // Assume the charge was interrupted
            ESP_LOGI(TAG,"Car charge session was interrupted");
            StdMetrics.ms_v_charge_state->SetValue("stopped");
            StdMetrics.ms_v_charge_substate->SetValue("interrupted");
          } else {
            // Assume the charge completed normally
            ESP_LOGI(TAG,"Car charge session completed");
            StdMetrics.ms_v_charge_state->SetValue("done");
            StdMetrics.ms_v_charge_substate->SetValue("onrequest");
          }
        }
      }
      lastCharging = isCharging;
      break;
    case 0x668:
      REQ_DLC(1);
      vehicle_smart_car_on((CAN_BYTE(0) & 0x40) > 0); // Drive Ready
      break;
    case 0x673:
      REQ_DLC(2);
      // Read TPMS pressure values:
      for (int i = 0; i < 4; i++) 
        {
        if (CAN_BYTE(2 + i) != 0xff) 
          {
          m_tpms_pressure[i] = (float) CAN_BYTE(2 + i) * 3.1; // kPa
          setTPMSValue(i, m_tpms_index[i]);
        }
      }
      break;
    
    default:
      //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
  }
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
      (charge_current <= 0.0) ) {
    return;
  }

  // Check if we have what is needed to calculate energy and remaining minutes
  if (charge_voltage > 0 && charge_current > 0) {
    // Update energy taken
    // Value is reset to 0 when a new charging session starts...
    float power  = charge_voltage * charge_current / 1000.0f;     // power in kw
    float energy = power / 3600.0f * 1.0f;                         // 1 second worth of energy in kwh's
    StdMetrics.ms_v_charge_kwh->SetValue( StdMetrics.ms_v_charge_kwh->AsFloat() + energy);

    // If no limits are set, then calculate remaining time to full charge
    if (limit_soc <= 0 && limit_range <= 0) {
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
    if (limit_soc > 0) {
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
    if (limit_range > 0 && max_range > 0.0f) {
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
  if (!StdMetrics.ms_v_door_chargeport->AsBool() && !mt_evc_plug_present->AsBool() && m_charge_start) {
    m_charge_start = false;
    m_charge_finished = true;
    StdMetrics.ms_v_charge_power->SetValue(0);
    ESP_LOGD(TAG,"Charge End");
  }
}

void OvmsVehicleSmartEQ::UpdateChargeMetrics() {
  int phasecnt = 0, i = 0, n = 0;
  float voltagesum = 0, ampsum = 0;
  
  for(i = 0; i < 3; i++) {
    if (mt_obl_main_volts->GetElemValue(i) > 120) {
      phasecnt++;
      n = i;
      voltagesum += mt_obl_main_volts->GetElemValue(i);
    }
  }
  if (phasecnt > 1) {
    voltagesum /= phasecnt;
  }
  StdMetrics.ms_v_charge_voltage->SetValue(voltagesum);

  if( phasecnt == 1 && mt_obl_fastchg->AsBool() ) {
    StdMetrics.ms_v_charge_current->SetValue(mt_obl_main_amps->GetElemValue(n));
    } else if( phasecnt == 3 && mt_obl_fastchg->AsBool() ) {
    for(i = 0; i < 3; i++) {
      if (mt_obl_main_amps->GetElemValue(i) > 0) {
        ampsum += mt_obl_main_amps->GetElemValue(i);
      }
    }
    StdMetrics.ms_v_charge_current->SetValue(ampsum/3);
  } else {
    for(i = 0; i < 3; i++) {
      if (mt_obl_main_amps->GetElemValue(i) > 0) {
        ampsum += mt_obl_main_amps->GetElemValue(i);
      }
    }
    StdMetrics.ms_v_charge_current->SetValue(ampsum);
  }

  StdMetrics.ms_v_charge_power->SetValue( mt_obl_main_CHGpower->GetElemValue(0) + mt_obl_main_CHGpower->GetElemValue(1) );
  float power = StdMetrics.ms_v_charge_power->AsFloat();
  float efficiency = (power == 0)
                     ? 0
                     : (StdMetrics.ms_v_bat_power->AsFloat() / power) * 100;
  StdMetrics.ms_v_charge_efficiency->SetValue(efficiency);
  ESP_LOGD(TAG, "SmartEQ_CHG_EFF_STD=%f", efficiency);
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
  if ( StdMetrics.ms_v_charge_pilot->AsBool() && m_poll_state != 3 && m_enable_write ) {
    PollSetState(3);
    ESP_LOGI(TAG,"Pollstate Charging");
  }
  else if ( !StdMetrics.ms_v_charge_pilot->AsBool() && StdMetrics.ms_v_env_on->AsBool() && m_poll_state != 2 && m_enable_write ) {
    PollSetState(2);
    ESP_LOGI(TAG,"Pollstate Running");
  }
  else if ( !StdMetrics.ms_v_charge_pilot->AsBool() && !StdMetrics.ms_v_env_on->AsBool() && mt_bus_awake->AsBool() && m_poll_state != 1 && m_enable_write ) {
    PollSetState(1);
    ESP_LOGI(TAG,"Pollstate Awake");
  }
  else if ( !mt_bus_awake->AsBool() && m_poll_state != 0 ) {
    PollSetState(0);
    ESP_LOGI(TAG,"Pollstate Off");
  }
}

void OvmsVehicleSmartEQ::CalculateEfficiency() {
  // float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5) {
    float mt_use_at_reset = StdMetrics.ms_v_bat_energy_used->AsFloat() - StdMetrics.ms_v_bat_energy_recd->AsFloat();
    float mt_use_at_total = StdMetrics.ms_v_bat_energy_used_total->AsFloat() - StdMetrics.ms_v_bat_energy_recd_total->AsFloat();
    float trip_km = StdMetrics.ms_v_pos_trip->AsFloat();
    float trip_total_km = mt_pos_odometer_trip_total->AsFloat();
    if (mt_use_at_reset > 0.1f) 
      {
      StdMetrics.ms_v_bat_consumption->SetValue(mt_use_at_reset * 10.0f);
      if (trip_km > 0.1f) StdMetrics.ms_v_charge_kwh_grid->SetValue((mt_use_at_reset / trip_km) * 100.0f);
      }
    if (mt_use_at_total > 0.1f) 
      {
      if (trip_total_km > 0.1f) StdMetrics.ms_v_charge_kwh_grid_total->SetValue((mt_use_at_total / trip_total_km) * 100.0f);
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

    #ifdef CONFIG_OVMS_COMP_CELLULAR
      m_12v_ticker = 0;
      m_climate_ticker = 0;
    #endif
  }
  else if (!isOn && StdMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG,"CAR IS OFF");
    //StdMetrics.ms_v_env_awake->SetValue(isOn);
  }

  // Always set this value to prevent it from going stale
  StdMetrics.ms_v_env_on->SetValue(isOn);
}

class OvmsVehicleSmartEQInit {
  public:
    OvmsVehicleSmartEQInit();
} MyOvmsVehicleSmartEQInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEQInit::OvmsVehicleSmartEQInit() {
  ESP_LOGI(TAG, "Registering Vehicle: SMART EQ (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartEQ>("SQ", "smart 453 4.Gen");
}
