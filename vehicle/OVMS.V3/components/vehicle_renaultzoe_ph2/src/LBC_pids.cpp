/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2.h"

void OvmsVehicleRenaultZoePh2::IncomingLBC(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  switch (pid)
  {
  case 0x9005:
  { // Battery voltage
    StandardMetrics.ms_v_bat_voltage->SetValue((float)(CAN_UINT(0) * 0.1), Volts);
    // ESP_LOGD(TAG, "9005 LBC ms_v_bat_voltage: %f", CAN_UINT(0) * 0.1);
    StandardMetrics.ms_v_bat_power->SetValue(((CAN_UINT(0) * 0.1) * StandardMetrics.ms_v_bat_current->AsFloat()) * 0.001);
    break;
  }
  case 0x925D:
  { // Battery current
    StandardMetrics.ms_v_bat_current->SetValue((float)((CAN_UINT(0) * 0.03125 - 1020) * -1.0f), Amps);
    // ESP_LOGD(TAG, "925D LBC ms_v_bat_current: %f", (CAN_UINT(0) * 0.03125 - 1020));
    StandardMetrics.ms_v_bat_power->SetValue((((CAN_UINT(0) * 0.03125 - 1020) * -1.0f) * StandardMetrics.ms_v_bat_voltage->AsFloat()) * 0.001);
    break;
  }
  case 0x9012:
  { // Battery average temperature
    StandardMetrics.ms_v_bat_temp->SetValue((float)(CAN_UINT(0) * 0.0625 - 40), Celcius);
    // ESP_LOGD(TAG, "9012 LBC ms_v_bat_temp: %f", (CAN_UINT(0)  * 0.0625 - 40));
    break;
  }
  case 0x9002:
  { // Battery SOC
    // StandardMetrics.ms_v_bat_soc->SetValue((float) (CAN_UINT(0)) * 0.01, Percentage);
    float bat_soc = CAN_UINT(0) * 0.01;

    if (bat_soc < 100.0f)
    {
      StandardMetrics.ms_v_bat_soc->SetValue(bat_soc, Percentage);
    }
    else
    {
      StandardMetrics.ms_v_bat_soc->SetValue(100.0f, Percentage);
    }

    StandardMetrics.ms_v_bat_cac->SetValue(Bat_cell_capacity_Ah * CAN_UINT(0) * 0.0001);

    // ESP_LOGD(TAG, "9002 LBC mt_bat_lbc_soc: %f", bat_soc);
    // ESP_LOGD(TAG, "9002 LBC mt_bat_lbc_soc calculated: %f", bat_soc + (bat_soc * 0.03));
    break;
  }
  case 0x9003:
  { // Battery SOH
    StandardMetrics.ms_v_bat_soh->SetValue((float)(CAN_UINT(0) * 0.01), Percentage);
    StandardMetrics.ms_v_bat_health->SetValue(
      ((float)(CAN_UINT(0) * 0.01) >= 90.0f) ? "excellent"
    : ((float)(CAN_UINT(0) * 0.01) >= 80.0f) ? "good"
    : ((float)(CAN_UINT(0) * 0.01) >= 70.0f)   ? "average"
    : ((float)(CAN_UINT(0) * 0.01) >= 60.0f)   ? "poor"
    : "consider replacement");
    // ESP_LOGD(TAG, "9003 LBC ms_v_bat_soh: %f", CAN_UINT(0) * 0.01);
    break;
  }
  case 0x9243:
  { // Battery energy charged kWh
    StandardMetrics.ms_v_charge_kwh_grid_total->SetValue((float)(CAN_UINT32(0) * 0.001), kWh);
    // ESP_LOGD(TAG, "9243 LBC ms_v_charge_kwh_grid_total: %f", CAN_UINT32(0) * 0.001);
    break;
  }
  case 0x9245:
  { // Battery energy discharged kWh
    StandardMetrics.ms_v_bat_energy_used_total->SetValue((float)(CAN_UINT32(0) * 0.001), kWh);
    // ESP_LOGD(TAG, "9244 LBC ms_v_bat_energy_used_total: %f", CAN_UINT32(0) * 0.001);
    break;
  }
  case 0x9247:
  { // Battery energy regenerated kWh
    StandardMetrics.ms_v_bat_energy_recd_total->SetValue((float)(CAN_UINT32(0) * 0.001), kWh);
    // ESP_LOGD(TAG, "9246 LBC ms_v_bat_energy_recd_total: %f", CAN_UINT32(0) * 0.001);
    break;
  }
  case 0x9210:
  { // Number of charge cycles
    mt_bat_cycles->SetValue(CAN_UINT(0));
    // ESP_LOGD(TAG, "9210 LBC mt_bat_cycles: %d", CAN_UINT(0));
    break;
  }
  case 0x9018:
  { // Max charge power
    mt_bat_max_charge_power->SetValue((float)(CAN_UINT(0) * 0.01), kW);
    // ESP_LOGD(TAG, "9018 LBC mt_bat_max_charge_power: %f", CAN_UINT(0) * 0.01);
    break;
  }
  case 0x91C8:
  { // Available charge in kWh
    StandardMetrics.ms_v_bat_capacity->SetValue(float(CAN_UINT24(0) * 0.001), kWh);
    // ESP_LOGD(TAG, "91C8 LBC mt_bat_available_energy: %f", CAN_UINT24(0) * 0.001);
    break;
  }
  case 0x9131:
  {
    BmsSetCellTemperature(0, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9132:
  {
    BmsSetCellTemperature(1, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9133:
  {
    BmsSetCellTemperature(2, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9134:
  {
    BmsSetCellTemperature(3, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9135:
  {
    BmsSetCellTemperature(4, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9136:
  {
    BmsSetCellTemperature(5, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9137:
  {
    BmsSetCellTemperature(6, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9138:
  {
    BmsSetCellTemperature(7, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9139:
  {
    BmsSetCellTemperature(8, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x913A:
  {
    BmsSetCellTemperature(9, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x913B:
  {
    BmsSetCellTemperature(10, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x913C:
  {
    BmsSetCellTemperature(11, CAN_UINT(0) * 0.0625 - 40);
    // ESP_LOGD(TAG, "%x: %f C", pid, CAN_UINT(0) * 0.0625 - 40);
    break;
  }
  case 0x9021:
  {
    BmsSetCellVoltage(0, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9022:
  {
    BmsSetCellVoltage(1, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9023:
  {
    BmsSetCellVoltage(2, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9024:
  {
    BmsSetCellVoltage(3, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9025:
  {
    BmsSetCellVoltage(4, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9026:
  {
    BmsSetCellVoltage(5, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9027:
  {
    BmsSetCellVoltage(6, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9028:
  {
    BmsSetCellVoltage(7, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9029:
  {
    BmsSetCellVoltage(8, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902A:
  {
    BmsSetCellVoltage(9, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902B:
  {
    BmsSetCellVoltage(10, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902C:
  {
    BmsSetCellVoltage(11, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902D:
  {
    BmsSetCellVoltage(12, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902E:
  {
    BmsSetCellVoltage(13, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x902F:
  {
    BmsSetCellVoltage(14, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9030:
  {
    BmsSetCellVoltage(15, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9031:
  {
    BmsSetCellVoltage(16, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9032:
  {
    BmsSetCellVoltage(17, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9033:
  {
    BmsSetCellVoltage(18, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9034:
  {
    BmsSetCellVoltage(19, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9035:
  {
    BmsSetCellVoltage(20, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9036:
  {
    BmsSetCellVoltage(21, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9037:
  {
    BmsSetCellVoltage(22, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9038:
  {
    BmsSetCellVoltage(23, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9039:
  {
    BmsSetCellVoltage(24, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903A:
  {
    BmsSetCellVoltage(25, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903B:
  {
    BmsSetCellVoltage(26, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903C:
  {
    BmsSetCellVoltage(27, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903D:
  {
    BmsSetCellVoltage(28, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903E:
  {
    BmsSetCellVoltage(29, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x903F:
  {
    BmsSetCellVoltage(30, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9041:
  {
    BmsSetCellVoltage(31, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9042:
  {
    BmsSetCellVoltage(32, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9043:
  {
    BmsSetCellVoltage(33, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9044:
  {
    BmsSetCellVoltage(34, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9045:
  {
    BmsSetCellVoltage(35, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9046:
  {
    BmsSetCellVoltage(36, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9047:
  {
    BmsSetCellVoltage(37, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9048:
  {
    BmsSetCellVoltage(38, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9049:
  {
    BmsSetCellVoltage(39, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904A:
  {
    BmsSetCellVoltage(40, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904B:
  {
    BmsSetCellVoltage(41, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904C:
  {
    BmsSetCellVoltage(42, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904D:
  {
    BmsSetCellVoltage(43, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904E:
  {
    BmsSetCellVoltage(44, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x904F:
  {
    BmsSetCellVoltage(45, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9050:
  {
    BmsSetCellVoltage(46, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9051:
  {
    BmsSetCellVoltage(47, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9052:
  {
    BmsSetCellVoltage(48, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9053:
  {
    BmsSetCellVoltage(49, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9054:
  {
    BmsSetCellVoltage(50, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9055:
  {
    BmsSetCellVoltage(51, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9056:
  {
    BmsSetCellVoltage(52, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9057:
  {
    BmsSetCellVoltage(53, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9058:
  {
    BmsSetCellVoltage(54, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9059:
  {
    BmsSetCellVoltage(55, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905A:
  {
    BmsSetCellVoltage(56, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905B:
  {
    BmsSetCellVoltage(57, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905C:
  {
    BmsSetCellVoltage(58, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905D:
  {
    BmsSetCellVoltage(59, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905E:
  {
    BmsSetCellVoltage(60, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x905F:
  {
    BmsSetCellVoltage(61, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9061:
  {
    BmsSetCellVoltage(62, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9062:
  {
    BmsSetCellVoltage(63, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9063:
  {
    BmsSetCellVoltage(64, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9064:
  {
    BmsSetCellVoltage(65, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9065:
  {
    BmsSetCellVoltage(66, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9066:
  {
    BmsSetCellVoltage(67, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9067:
  {
    BmsSetCellVoltage(68, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9068:
  {
    BmsSetCellVoltage(69, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9069:
  {
    BmsSetCellVoltage(70, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906A:
  {
    BmsSetCellVoltage(71, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906B:
  {
    BmsSetCellVoltage(72, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906C:
  {
    BmsSetCellVoltage(73, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906D:
  {
    BmsSetCellVoltage(74, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906E:
  {
    BmsSetCellVoltage(75, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x906F:
  {
    BmsSetCellVoltage(76, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9070:
  {
    BmsSetCellVoltage(77, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9071:
  {
    BmsSetCellVoltage(78, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9072:
  {
    BmsSetCellVoltage(79, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9073:
  {
    BmsSetCellVoltage(80, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9074:
  {
    BmsSetCellVoltage(81, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9075:
  {
    BmsSetCellVoltage(82, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9076:
  {
    BmsSetCellVoltage(83, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9077:
  {
    BmsSetCellVoltage(84, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9078:
  {
    BmsSetCellVoltage(85, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9079:
  {
    BmsSetCellVoltage(86, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907A:
  {
    BmsSetCellVoltage(87, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907B:
  {
    BmsSetCellVoltage(88, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907C:
  {
    BmsSetCellVoltage(89, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907D:
  {
    BmsSetCellVoltage(90, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907E:
  {
    BmsSetCellVoltage(91, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x907F:
  {
    BmsSetCellVoltage(92, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9081:
  {
    BmsSetCellVoltage(93, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9082:
  {
    BmsSetCellVoltage(94, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }
  case 0x9083:
  {
    BmsSetCellVoltage(95, CAN_UINT(0) * 0.000976563);
    // ESP_LOGD(TAG, "%x: %f V", pid, CAN_UINT(0)  * 0.001);
    break;
  }

  default:
  {
    char *buf = NULL;
    size_t rlen = len, offset = 0;
    do
    {
      rlen = FormatHexDump(&buf, data + offset, rlen, 16);
      offset += 16;
      ESP_LOGW(TAG, "OBD2: unhandled reply from LBC [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}