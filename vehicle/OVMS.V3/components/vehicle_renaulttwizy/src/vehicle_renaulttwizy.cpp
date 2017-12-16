/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: Main class
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
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
static const char *TAG = "v-renaulttwizy";

#define VERSION "0.5.0"

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

#include "vehicle_renaulttwizy.h"

using namespace std;


/**
 * Constructor & destructor
 */

size_t OvmsVehicleRenaultTwizy::m_modifier = 0;

OvmsVehicleRenaultTwizy::OvmsVehicleRenaultTwizy()
{
  ESP_LOGI(TAG, "Renault Twizy vehicle module");
  
  memset(&twizy_flags, 0, sizeof twizy_flags);
  
  // init can bus:
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  
  // init configs:
  MyConfig.RegisterParam("x.rt", "Renault Twizy", true, true);
  ConfigChanged(NULL);
  
  // init metrics:
  if (m_modifier == 0) {
    m_modifier = MyMetrics.RegisterModifier();
    ESP_LOGD(TAG, "registered metric modifier is #%d", m_modifier);
  }
  m_version = MyMetrics.InitString("x.rt.m.version", 0, VERSION " " __DATE__ " " __TIME__);
  
  // init commands:
  cmd_xrt = MyCommandApp.RegisterCommand("xrt", "Renault Twizy", NULL, "", 0, 0, true);
  
  // init subsystems:
  BatteryInit();
  PowerInit();
  
  // init event listener:
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "gps.lock.acquired", std::bind(&OvmsVehicleRenaultTwizy::EventListener, this, _1, _2));
  
  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
}

OvmsVehicleRenaultTwizy::~OvmsVehicleRenaultTwizy()
{
  ESP_LOGI(TAG, "Shutdown Renault Twizy vehicle module");
  
  // release GPS:
  MyEvents.SignalEvent("vehicle.release.gps", NULL);
  MyEvents.SignalEvent("vehicle.release.gpstime", NULL);
  
  // unregister event listeners:
  MyEvents.DeregisterEvent(TAG);
}


/**
 * ConfigChanged: reload single/all configuration variables
 */

void OvmsVehicleRenaultTwizy::ConfigChanged(OvmsConfigParam* param)
{
  ESP_LOGD(TAG, "Renault Twizy reload configuration");
  
  // Instances:
  //  
  //  suffsoc           Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange         Sufficient range [km] (Default: 0=disabled)
  //  maxrange          Maximum ideal range at 20 °C [km] (Default: 80)
  //  
  //  cap_act_prc       Battery actual capacity level [%] (Default: 100.0)
  //  cap_nom_ah        Battery nominal capacity [Ah] (Default: 108.0)
  //  
  //  chargelevel       Charge power level [1-7] (Default: 0=unlimited)
  //  chargemode        Charge mode: 0=notify, 1=stop at sufficient SOC/range (Default: 0)
  //  
  //  canwrite          Bool: CAN write enabled (Default: no)
  //  autoreset         Bool: SEVCON reset on error (Default: yes)
  //  kickdown          Bool: SEVCON automatic kickdown (Default: yes)
  //  autopower         Bool: SEVCON automatic power level adjustment (Default: yes)
  //  console           Bool: SimpleConsole inputs enabled (Default: no)
  //  
  //  gpslogint         Seconds between RT-GPS-Log entries while driving (Default: 0 = disabled)
  //  
  //  kd_threshold      Kickdown threshold (Default: 35)
  //  kd_compzero       Kickdown pedal compensation (Default: 120)
  //  
  
  cfg_maxrange = MyConfig.GetParamValueInt("x.rt", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (cfg_maxrange <= 0)
    cfg_maxrange = CFG_DEFAULT_MAXRANGE;
  
  cfg_suffsoc = MyConfig.GetParamValueInt("x.rt", "suffsoc");
  *StdMetrics.ms_v_charge_limit_soc = (float) cfg_suffsoc;
  
  cfg_suffrange = MyConfig.GetParamValueInt("x.rt", "suffrange");
  *StdMetrics.ms_v_charge_limit_range = (float) cfg_suffrange;
  
  cfg_chargemode = MyConfig.GetParamValueInt("x.rt", "chargemode");
  cfg_chargelevel = MyConfig.GetParamValueInt("x.rt", "chargelevel");
  
  cfg_bat_cap_actual_prc = MyConfig.GetParamValueFloat("x.rt", "cap_act_prc", 100);
  cfg_bat_cap_nominal_ah = MyConfig.GetParamValueFloat("x.rt", "cap_nom_ah", CFG_DEFAULT_CAPACITY);
  
  twizy_flags.EnableWrite = MyConfig.GetParamValueBool("x.rt", "canwrite", false);
  twizy_flags.DisableReset = !MyConfig.GetParamValueBool("x.rt", "autoreset", true);
  twizy_flags.DisableKickdown = !MyConfig.GetParamValueBool("x.rt", "kickdown", true);
  twizy_flags.DisableAutoPower = !MyConfig.GetParamValueBool("x.rt", "autopower", true);
  twizy_flags.EnableInputs = MyConfig.GetParamValueBool("x.rt", "console", false);
  
  cfg_gpslog_interval = MyConfig.GetParamValueInt("x.rt", "gpslogint", 0);

  cfg_kd_threshold = MyConfig.GetParamValueInt("x.rt", "kd_threshold", CFG_DEFAULT_KD_THRESHOLD);
  cfg_kd_compzero = MyConfig.GetParamValueInt("x.rt", "kd_compzero", CFG_DEFAULT_KD_COMPZERO);
}


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleRenaultTwizy::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 0:
      MyConfig.SetParamValue("x.rt", "gpslogint", value);
      return true;
    case 1:
      MyConfig.SetParamValue("x.rt", "kd_threshold", value);
      return true;
    case 2:
      MyConfig.SetParamValue("x.rt", "kd_compzero", value);
      return true;
    case 6:
      MyConfig.SetParamValue("x.rt", "chargemode", value);
      return true;
    case 7:
      MyConfig.SetParamValue("x.rt", "chargelevel", value);
      return true;
    case 10:
      MyConfig.SetParamValue("x.rt", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("x.rt", "suffrange", value);
      return true;
    case 12:
      MyConfig.SetParamValue("x.rt", "maxrange", value);
      return true;
    case 13:
      MyConfig.SetParamValue("x.rt", "cap_act_prc", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("x.rt", "canwrite",  (bits& 1)!=0);
      MyConfig.SetParamValueBool("x.rt", "autoreset", (bits& 2)==0);
      MyConfig.SetParamValueBool("x.rt", "kickdown",  (bits& 4)==0);
      MyConfig.SetParamValueBool("x.rt", "autopower", (bits& 8)==0);
      MyConfig.SetParamValueBool("x.rt", "console",   (bits&16)!=0);
      return true;
    }
    case 16:
      MyConfig.SetParamValue("x.rt", "cap_nom_ah", value);
      return true;
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}

/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicleRenaultTwizy::GetFeature(int key)
{
  switch (key)
  {
    case 0:
      return MyConfig.GetParamValue("x.rt", "gpslogint", XSTR(0));
    case 1:
      return MyConfig.GetParamValue("x.rt", "kd_threshold", XSTR(CFG_DEFAULT_KD_THRESHOLD));
    case 2:
      return MyConfig.GetParamValue("x.rt", "kd_compzero", XSTR(CFG_DEFAULT_KD_COMPZERO));
    case 6:
      return MyConfig.GetParamValue("x.rt", "chargemode", XSTR(0));
    case 7:
      return MyConfig.GetParamValue("x.rt", "chargelevel", XSTR(0));
    case 10:
      return MyConfig.GetParamValue("x.rt", "suffsoc", XSTR(0));
    case 11:
      return MyConfig.GetParamValue("x.rt", "suffrange", XSTR(0));
    case 12:
      return MyConfig.GetParamValue("x.rt", "maxrange", XSTR(CFG_DEFAULT_MAXRANGE));
    case 13:
      return MyConfig.GetParamValue("x.rt", "cap_act_prc", XSTR(100));
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("x.rt", "canwrite",  false) ?  1 : 0) |
        (!MyConfig.GetParamValueBool("x.rt", "autoreset", true)  ?  2 : 0) |
        (!MyConfig.GetParamValueBool("x.rt", "kickdown",  true)  ?  4 : 0) |
        (!MyConfig.GetParamValueBool("x.rt", "autopower", true)  ?  8 : 0) |
        ( MyConfig.GetParamValueBool("x.rt", "console",   false) ? 16 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 16:
      return MyConfig.GetParamValue("x.rt", "cap_nom_ah", XSTR(CFG_DEFAULT_CAPACITY));
    default:
      return OvmsVehicle::GetFeature(key);
  }
}


/**
 * EventListener:
 *  - update GPS log ASAP
 */
void OvmsVehicleRenaultTwizy::EventListener(string event, void* data)
{
  if (event == "gps.lock.acquired")
  {
    // immediately update our track log:
    SendGPSLog();
  }
}


/**
 * Framework registration
 */

class OvmsVehicleRenaultTwizyInit
{
  public: OvmsVehicleRenaultTwizyInit();
} MyOvmsVehicleRenaultTwizyInit  __attribute__ ((init_priority (9000)));

OvmsVehicleRenaultTwizyInit::OvmsVehicleRenaultTwizyInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: Renault Twizy (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultTwizy>("RT","Renault Twizy");
}


/**
 * CommandStat: Twizy implementation of vehicle status output
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandStat(int verbosity, OvmsWriter* writer)
{
  metric_unit_t rangeUnit = Native; // TODO: use user config if set
  
  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
  {
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
    
    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";
    
    writer->puts(charge_state.c_str());
    
    // Power sums: battery input:
    float pwr_batt = StdMetrics.ms_v_bat_energy_recd->AsFloat() * 1000;
    // Grid drain estimation:
    //  charge efficiency is ~88.1% w/o 12V charge and ~86.4% with
    //  (depending on when to cut the grid connection)
    //  so we're using 87.2% as an average efficiency:
    float pwr_grid = pwr_batt / 0.872;
    writer->printf("CHG: %d (~%d) Wh\n", (int) pwr_batt, (int) pwr_grid);
  }
  else
  {
    // Charge port door is closed, not charging
    writer->puts("Not charging");
  }
  
  // Estimated charge time for 100%:
  if (twizy_soc < 10000)
  {
    int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
    if (duration_full)
      writer->printf("Full: %d min.\n", duration_full);
  }
  
  // Estimated + Ideal Range:
  const char* range_est = StdMetrics.ms_v_bat_range_est->AsString("?", rangeUnit, 0).c_str();
  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("?", rangeUnit, 0).c_str();
  writer->printf("Range: %s - %s\n", range_est, range_ideal);
  
  // SOC + min/max:
  writer->printf("SOC: %s (%s..%s)\n",
    (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str(),
    (char*) m_batt_soc_min->AsString("-", Native, 1).c_str(),
    (char*) m_batt_soc_max->AsUnitString("-", Native, 1).c_str());
  
  // ODOMETER:
  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);
  
  // BATTERY CAPACITY:
  if (cfg_bat_cap_actual_prc > 0)
  {
    writer->printf("CAP: %.1f%% %s\n",
      cfg_bat_cap_actual_prc,
      StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  }
  
  // BATTERY SOH:
  if (twizy_soh > 0)
  {
    writer->printf("SOH: %s\n",
      StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str());
  }
  
  return Success;
}


/**
 * CAN RX handler
 */

void OvmsVehicleRenaultTwizy::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  unsigned int u;
  
  uint8_t *can_databuffer = p_frame->data.u8;
  
  // CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
  #define CAN_BYTE(b)     can_databuffer[b]
  #define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
  #define CAN_UINT24(b)   (((UINT32)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) \
                            | CAN_BYTE(b+2))
  #define CAN_UINT32(b)   (((UINT32)CAN_BYTE(b) << 24) | ((UINT32)CAN_BYTE(b+1) << 16) \
                            | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
  #define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
  #define CAN_NIBH(b)     (can_databuffer[b] >> 4)
  #define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))
  
  
  switch (p_frame->MsgID)
  {
    case 0x081:
      // --------------------------------------------------------------------------
      // CAN ID 0x081: CANopen error message from SEVCON (Node #1)
      
      // count errors to detect manual CFG RESET request:
      if ((CAN_BYTE(1)==0x10) && (CAN_BYTE(2)==0x01))
        twizy_button_cnt++;
      
      break;
    
    
    case 0x155:
      // --------------------------------------------------------------------------
      // *** BMS: POWER STATUS ***
      // basic validation:
      // Byte 4:  0x94 = init/exit phase (CAN data invalid)
      //          0x54 = Twizy online (CAN data valid)
      if (can_databuffer[3] == 0x54)
      {
        unsigned int t;
        
        // BMS to CHG power level request:
        // (take only while charging to keep finishing level for "done" detection)
        if ((twizy_status & 0x60) == 0x20)
          twizy_chg_power_request = CAN_BYTE(0);
        
        // SOC:
        t = ((unsigned int) can_databuffer[4] << 8) + can_databuffer[5];
        if (t > 0 && t <= 40000)
        {
          twizy_soc = t >> 2;
          // car value derived in ticker1()
          
          // Remember maximum SOC for charging "done" distinction:
          if (twizy_soc > twizy_soc_max)
            twizy_soc_max = twizy_soc;
          
          // ...and minimum SOC for range calculation during charging:
          if (twizy_soc < twizy_soc_min)
          {
            twizy_soc_min = twizy_soc;
            twizy_soc_min_range = twizy_range_est;
          }
        }
        
        // CURRENT & POWER:
        t = ((unsigned int) (can_databuffer[1] & 0x0f) << 8) + can_databuffer[2];
        if (t > 0 && t < 0x0f00)
        {
          twizy_current = 2000 - (signed int) t;
          // ...in 1/4 A
          
          // set min/max:
          if (twizy_current < twizy_current_min)
            twizy_current_min = twizy_current;
          if (twizy_current > twizy_current_max)
            twizy_current_max = twizy_current;
          
          // calculate power:
          twizy_power = (twizy_current < 0)
            ? -((((long) -twizy_current) * twizy_batt[0].volt_act + 128) >> 8)
            : ((((long) twizy_current) * twizy_batt[0].volt_act + 128) >> 8);
          // ...in 256/40 W = 6.4 W
          
          // set min/max:
          if (twizy_power < twizy_power_min)
            twizy_power_min = twizy_power;
          if (twizy_power > twizy_power_max)
            twizy_power_max = twizy_power;
          
          // calculate distance from ref:
          if (twizy_dist >= twizy_speed_distref)
            t = twizy_dist - twizy_speed_distref;
          else
            t = twizy_dist + (0x10000L - twizy_speed_distref);
          twizy_speed_distref = twizy_dist;
          
          // add to speed state:
          twizy_speedpwr[twizy_speed_state].dist += t;
          if (twizy_current > 0)
          {
            twizy_speedpwr[twizy_speed_state].use += twizy_power;
            twizy_level_use += twizy_power;
            twizy_charge_use += twizy_current;
          }
          else
          {
            twizy_speedpwr[twizy_speed_state].rec += -twizy_power;
            twizy_level_rec += -twizy_power;
            twizy_charge_rec += -twizy_current;
          }
          
          // do we need to take base power consumption into account?
          // i.e. for lights etc. -- varies...
        }
      }
      break; // case 0x155
    
    
    case 0x196:
      // --------------------------------------------------------------------------
      // CAN ID 0x196: 10 ms period
      
      // MOTOR TEMPERATURE:
      if (CAN_BYTE(5) > 0 && CAN_BYTE(5) < 0xf0)
        twizy_tmotor = (signed int) CAN_BYTE(5) - 40;
      else
        twizy_tmotor = 0;
      
      break;
    
    
    case 0x424:
      // --------------------------------------------------------------------------
      // CAN ID 0x424: sent every 100 ms (10 per second)
      
      // max drive (discharge) + recup (charge) power:
      if (CAN_BYTE(2) != 0xff)
        twizy_batt[0].max_recup_pwr = CAN_BYTE(2);
      if (CAN_BYTE(3) != 0xff)
        twizy_batt[0].max_drive_pwr = CAN_BYTE(3);
      
      // BMS SOH:
      twizy_soh = CAN_BYTE(5);
      
      break;
    
    
    case 0x554:
      // --------------------------------------------------------------------------
      // CAN ID 0x554: Battery cell module temperatures
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        for (int i = 0; i < BATT_CMODS; i++)
          twizy_cmod[i].temp_act = CAN_BYTE(i);
      }
      break;
    
    case 0x556:
      // --------------------------------------------------------------------------
      // CAN ID 0x556: Battery cell voltages 1-5
      // 100 ms = 10 per second
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[0].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[1].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[2].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[3].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
        twizy_cell[4].volt_act = ((UINT) CAN_BYTE(6) << 4) | ((UINT) CAN_NIBH(7));
      }
      
      break;
    
    case 0x557:
      // --------------------------------------------------------------------------
      // CAN ID 0x557: Battery cell voltages 6-10
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[5].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[6].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[7].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[8].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
        twizy_cell[9].volt_act = ((UINT) CAN_BYTE(6) << 4) | ((UINT) CAN_NIBH(7));
      }
      break;
    
    case 0x55E:
      // --------------------------------------------------------------------------
      // CAN ID 0x55E: Battery cell voltages 11-14
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[10].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[11].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[12].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[13].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
      }
      break;
    
    case 0x55F:
      // --------------------------------------------------------------------------
      // CAN ID 0x55F: Battery pack voltages
      // (1000 ms = 1 per second)
      if (CAN_BYTE(5) != 0x0ff)
      {
        // we still don't know why there are two pack voltages
        // best guess: take avg
        UINT v1, v2;
        
        v1 = ((UINT) CAN_BYTE(5) << 4) | ((UINT) CAN_NIBH(6));
        v2 = ((UINT) CAN_NIBL(6) << 8) | ((UINT) CAN_BYTE(7));
        
        twizy_batt[0].volt_act = (v1 + v2 + 1) >> 1;
      }
      break;
    
    
#ifdef OVMS_TWIZY_CFG
    case 0x581:
      // --------------------------------------------------------------------------
      // CAN ID 0x581: CANopen SDO reply from SEVCON (Node #1)
      //
      
      // copy message into twizy_sdo object:
      for (u = 0; u < can_datalength; u++)
        twizy_sdo.byte[u] = CAN_BYTE(u);
      for (; u < 8; u++)
        twizy_sdo.byte[u] = 0;
      
      break;
#endif // OVMS_TWIZY_CFG
    
    
    case 0x597:
      // --------------------------------------------------------------------------
      // CAN ID 0x597: sent every 100 ms (10 per second)
      
      // VEHICLE state:
      //  [0]: 0x20 = power line connected
      if (CAN_BYTE(0) & 0x20)
        *StdMetrics.ms_v_charge_voltage = (float) 230; // fix 230 V
      else
        *StdMetrics.ms_v_charge_voltage = (float) 0;
      
      // twizy_status high nibble:
      //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
      //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
      //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase
      //
      // low nibble: taken from 59B, clear GO flag while OFFLINE
      // to prevent sticky GO after switch-off
      // (597 comes before/after 59B in init/exit phase)
      
      // init cyclic distance counter on switch-on:
      if ((CAN_BYTE(1) & CAN_STATUS_KEYON) && (!(twizy_status & CAN_STATUS_KEYON)))
        twizy_dist = twizy_speed_distref = 0;
      
      if (CAN_BYTE(1) & CAN_STATUS_OFFLINE)
        twizy_status = (twizy_status & 0x07) | (CAN_BYTE(1) & 0xF0);
      else
        twizy_status = (twizy_status & 0x0F) | (CAN_BYTE(1) & 0xF0);
      
      // Read 12V DC converter current level:
      *StdMetrics.ms_v_bat_12v_current = (float) CAN_BYTE(2) / 5;
      
      // Read 12V DC converter status:
      twizy_flags.Charging12V = ((CAN_BYTE(3) & 0xC0) != 0xC0);
      
      // CHARGER temperature:
      if (CAN_BYTE(7) > 0 && CAN_BYTE(7) < 0xf0)
        *StdMetrics.ms_v_charge_temp = (float) CAN_BYTE(7) - 40;
      else
        *StdMetrics.ms_v_charge_temp = (float) 0;
      
      break; // case 0x597
    
    
    case 0x599:
      // --------------------------------------------------------------------------
      // CAN ID 0x599: sent every 100 ms (10 per second)
      
      // RANGE:
      // we need to check for charging, as the Twizy
      // does not update range during charging
      if (((twizy_status & 0x60) == 0)
        && (can_databuffer[5] != 0xff) && (can_databuffer[5] > 0))
      {
        twizy_range_est = can_databuffer[5];
        // car values derived in ticker1()
      }
      
      // SPEED:
      u = ((unsigned int) can_databuffer[6] << 8) + can_databuffer[7];
      if (u != 0xffff)
      {
        int delta = (int) u - (int) twizy_speed;
        
        // set min/max:
        if (delta < twizy_accel_min)
          twizy_accel_min = delta;
        if (delta > twizy_accel_max)
          twizy_accel_max = delta;
        
        // running average over 4 samples:
        twizy_accel_avg = twizy_accel_avg * 3 + delta;
        // C18: no arithmetic >> sign propagation on negative ints
        twizy_accel_avg = (twizy_accel_avg < 0)
          ? -((-twizy_accel_avg + 2) >> 2)
          : ((twizy_accel_avg + 2) >> 2);
        
        // switch speed state:
        if (twizy_accel_avg >= CAN_ACCEL_THRESHOLD)
          twizy_speed_state = CAN_SPEED_ACCEL;
        else if (twizy_accel_avg <= -CAN_ACCEL_THRESHOLD)
          twizy_speed_state = CAN_SPEED_DECEL;
        else
          twizy_speed_state = CAN_SPEED_CONST;
        
        // speed/delta sum statistics while driving:
        if (u >= CAN_SPEED_THRESHOLD)
        {
          // overall speed avg:
          twizy_speedpwr[0].spdcnt++;
          twizy_speedpwr[0].spdsum += u;
          
          // accel/decel speed avg:
          if (twizy_speed_state != 0)
          {
            twizy_speedpwr[twizy_speed_state].spdcnt++;
            twizy_speedpwr[twizy_speed_state].spdsum += ABS(twizy_accel_avg);
          }
        }
        
        twizy_speed = u;
        // car value derived in ticker1()
      }
      
      break; // case 0x599
    
    
    case 0x59B:
      // --------------------------------------------------------------------------
      // CAN ID 0x59B: sent every 100 ms (10 per second)
      
      // twizy_status low nibble:
      twizy_status = (twizy_status & 0xF0) | (CAN_BYTE(1) & 0x09);
      if (CAN_BYTE(0) == 0x80)
        twizy_status |= CAN_STATUS_MODE_D;
      else if (CAN_BYTE(0) == 0x08)
        twizy_status |= CAN_STATUS_MODE_R;
      
      #ifdef OVMS_TWIZY_CFG
      
        // accelerator pedal:
        u = CAN_BYTE(3);
        
        // running average over 2 samples:
        u = (twizy_accel_pedal + u + 1) >> 1;
        
        // kickdown detection:
        s = KICKDOWN_THRESHOLD(twizy_accel_pedal);
        if ( ((s > 0) && (u > ((unsigned int)twizy_accel_pedal + s)))
          || ((twizy_kickdown_level > 0) && (u > twizy_kickdown_level)) )
        {
          twizy_kickdown_level = u;
        }
        
        twizy_accel_pedal = u;
      
      #endif // OVMS_TWIZY_CFG
      
      break;
      
      
    case 0x59E:
      // --------------------------------------------------------------------------
      // CAN ID 0x59E: sent every 100 ms (10 per second)
      
      // CYCLIC DISTANCE COUNTER:
      twizy_dist = ((UINT) CAN_BYTE(0) << 8) + CAN_BYTE(1);
      
      // SEVCON TEMPERATURE:
      if (CAN_BYTE(5) > 0 && CAN_BYTE(5) < 0xf0)
        *StdMetrics.ms_v_inv_temp = (float) CAN_BYTE(5) - 40;
      else
        *StdMetrics.ms_v_inv_temp = (float) 0;
      
      break;
    
    
    case 0x5D7:
      // --------------------------------------------------------------------------
      // *** ODOMETER ***
      twizy_odometer = ((unsigned long) CAN_BYTE(5) >> 4)
        | ((unsigned long) CAN_BYTE(4) << 4)
        | ((unsigned long) CAN_BYTE(3) << 12)
        | ((unsigned long) CAN_BYTE(2) << 20);
      break;
      
      
    case 0x69F:
      // --------------------------------------------------------------------------
      // *** VIN ***
      // last 7 digits of real VIN, in nibbles, reverse:
      // (assumption: no hex digits)
      if (!twizy_vin[0]) // we only need to process this once
      {
        twizy_vin[0] = '0' + CAN_NIB(7);
        twizy_vin[1] = '0' + CAN_NIB(6);
        twizy_vin[2] = '0' + CAN_NIB(5);
        twizy_vin[3] = '0' + CAN_NIB(4);
        twizy_vin[4] = '0' + CAN_NIB(3);
        twizy_vin[5] = '0' + CAN_NIB(2);
        twizy_vin[6] = '0' + CAN_NIB(1);
        twizy_vin[7] = 0;
        *StdMetrics.ms_v_vin = (string) twizy_vin;
      }
      break;
    
    
    case 0x700:
      // --------------------------------------------------------------------------
      // CAN ID 0x700: VirtualBMS extension:
      // see https://github.com/dexterbg/Twizy-Virtual-BMS/blob/master/API.md#extended-info-frame
      //   - Byte 0: BMS specific state #1 (main state, i.e. twizy.state())
      //   - Byte 1: highest 3 bits = BMS type ID (see below), remaining 5 bits = BMS specific error code (see below)
      //   - Bytes 2-4: cell voltages #15 & #16 (encoded in 12 bits like #1-#14)
      //   - Bytes 5-6: balancing status (bits 15…0 = cells 16…1, 1 = balancing active)
      //   - Byte 7: BMS specific state #2 (auxiliary state or data)
      if (CAN_BYTE(0) != 0x0ff)
      {
        // Battery cell voltages 15 + 16:
        twizy_cell[14].volt_act = ((UINT) CAN_BYTE(2) << 4) | ((UINT) CAN_NIBH(3));
        twizy_cell[15].volt_act = ((UINT) CAN_NIBL(3) << 8) | ((UINT) CAN_BYTE(4));
      }
      break;
      
  }
  
}


/**
 * Ticker1: per second ticker
 */

void OvmsVehicleRenaultTwizy::Ticker1(uint32_t ticker)
{
  // --------------------------------------------------------------------------
  // Update standard metrics:
  // 
  
  *StdMetrics.ms_v_pos_odometer = (float) twizy_odometer / 100;
  
  if (twizy_odometer >= twizy_odometer_tripstart)
    *StdMetrics.ms_v_pos_trip = (float) (twizy_odometer - twizy_odometer_tripstart) / 100;
  else
    *StdMetrics.ms_v_pos_trip = (float) 0;
  
  *StdMetrics.ms_v_pos_speed = (float) twizy_speed / 100;
  
  *StdMetrics.ms_v_mot_temp = (float) twizy_tmotor;
  
  
  // --------------------------------------------------------------------------
  // STATUS:
  //
  
  if (StdMetrics.ms_v_charge_voltage->AsFloat() > 0)
    twizy_flags.PilotSignal = 1;
  else
    twizy_flags.PilotSignal = 0;
  
  if ((twizy_status & 0x60) == 0x20)
  {
    twizy_flags.ChargePort = 1;
    twizy_flags.Charging = 1;
    twizy_flags.Charging12V = 1;
  }
  else
  {
    twizy_flags.Charging = 0;
    // Port will be closed on next use start
    // 12V charging will be stopped by DC converter status check
  }
  
  
  // Ignition status:
  twizy_flags.CarON = (twizy_status & CAN_STATUS_GO) ? true : false;
  
  
  // Power status change?
  if ((twizy_status & CAN_STATUS_KEYON)
    && !(twizy_status & CAN_STATUS_OFFLINE)
    && !twizy_flags.CarAwake)
  {
    // CAR has just been turned ON & CAN bus is online
    ESP_LOGI(TAG, "turned on");
    twizy_flags.CarAwake = 1;
    
    // set trip references:
    twizy_soc_tripstart = twizy_soc;
    twizy_odometer_tripstart = twizy_odometer;
    twizy_accel_avg = 0;
    twizy_accel_min = 0;
    twizy_accel_max = 0;
    
    // reset battery subsystem:
    BatteryReset();
    
    // reset power subsystem:
    PowerReset();
    
    // reset button cnt:
    twizy_button_cnt = 0;
  }
  else if (!(twizy_status & CAN_STATUS_KEYON) && twizy_flags.CarAwake)
  {
    // CAR has just been turned OFF
    ESP_LOGI(TAG, "turned off");
    twizy_flags.CarAwake = 0;
    twizy_flags.CtrlLoggedIn = 0;
    
    // set trip references:
    twizy_soc_tripend = twizy_soc;
    
    // send power statistics if 25+ Wh used:
    if ((twizy_speedpwr[CAN_SPEED_CONST].use
      +twizy_speedpwr[CAN_SPEED_ACCEL].use
      +twizy_speedpwr[CAN_SPEED_DECEL].use) > (WH_DIV * 25))
    {
      RequestNotify(SEND_PowerNotify | SEND_TripLog);
    }
    
    // reset button cnt:
    twizy_button_cnt = 0;
  }
  
  
  // --------------------------------------------------------------------------
  // Subsystem updates:
  
  // update battery subsystem:
  BatteryUpdate();
  
  // update power subsystem:
  PowerUpdate();
  
  
  // --------------------------------------------------------------------------
  // Charge notification + alerts:
  // 
  //  - twizy_chargestate: 1=charging, 2=top off, 4=done, 21=stopped charging
  //  - twizy_chg_stop_request: 1=stop
  // 

  if (twizy_flags.Charging)
  {
    // --------------------------------------------------------------------------
    // CHARGING
    // 
    
    twizy_chargeduration++;
    
    *StdMetrics.ms_v_charge_kwh = (float) twizy_speedpwr[CAN_SPEED_CONST].rec / WH_DIV / 1000;
    
    *StdMetrics.ms_v_charge_current = (float) -twizy_current / 4;

    // Calculate range during charging:
    // scale twizy_soc_min_range to twizy_soc
    if ((twizy_soc_min_range > 0) && (twizy_soc > 0) && (twizy_soc_min > 0))
    {
      // Update twizy_range_est:
      twizy_range_est = (((float) twizy_soc_min_range) / twizy_soc_min) * twizy_soc;

      if (twizy_maxrange > 0)
        twizy_range_ideal = (((float) twizy_maxrange) * twizy_soc) / 10000;
      else
        twizy_range_ideal = twizy_range_est;
    }


    // If charging has previously been interrupted...
    if (twizy_chargestate == 21)
    {
      // ...send charge alert:
      RequestNotify(SEND_ChargeState);
    }


    // If we've not been charging before...
    if (twizy_chargestate > 2)
    {
      ESP_LOGI(TAG, "charge start");
      
      // reset SOC max:
      twizy_soc_max = twizy_soc;

      // reset SOC min & power sums if not resuming from charge interruption:
      if (twizy_chargestate != 21)
      {
        twizy_soc_min = twizy_soc;
        twizy_soc_min_range = twizy_range_est;
        PowerReset();
      }

      // reset battery monitor:
      BatteryReset();

      // ...enter state 1=charging or 2=topping-off depending on the
      // charge power request level we're starting with (7=full power):
      twizy_chargestate = (twizy_chg_power_request == 7) ? 1 : 2;
        
      // Send charge start notification?
      // TODO if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SCHGPHASE)
        RequestNotify(SEND_ChargeState);
    }

    else
    {
      // We've already been charging:
      
      // check for crossing "sufficient SOC/Range" thresholds:
      if ((cfg_chargemode == TWIZY_CHARGEMODE_AUTOSTOP) &&
              (((cfg_suffsoc > 0) && (twizy_soc >= cfg_suffsoc*100)) ||
              ((cfg_suffrange > 0) && (twizy_range_ideal >= cfg_suffrange))))
      {
        // set charge stop request:
        twizy_chg_stop_request = 1;
      }

      else if (
              ((twizy_soc > 0) && (cfg_suffsoc > 0)
              && (twizy_soc >= cfg_suffsoc*100) && (twizy_last_soc < cfg_suffsoc*100))
              ||
              ((twizy_range_est > 0) && (cfg_suffrange > 0)
              && (twizy_range_ideal >= cfg_suffrange) && (twizy_last_idealrange < cfg_suffrange))
              )
      {
        // ...send sufficient charge alert:
        RequestNotify(SEND_ChargeState);
      }
      
      // Battery capacity estimation: detect end of CC phase
      // by monitoring the charge power level requested by the BMS;
      // if it drops, we're entering the CV phase
      // (Note: depending on battery temperature, this may happen at rather
      // low SOC and without having reached the nominal top voltage)
      if (twizy_chg_power_request >= twizy_cc_power_level)
      {
        // still in CC phase:
        twizy_cc_power_level = twizy_chg_power_request;
        twizy_cc_soc = twizy_soc;
        twizy_cc_charge = twizy_charge_rec;
      }
      else if (twizy_chargestate != 2)
      {
        // entering CV phase, set state 2=topping off:
        twizy_chargestate = 2;
        
        // Send charge phase notification?
        // TODO if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SCHGPHASE)
          RequestNotify(SEND_ChargeState);
      }

    }

    // update "sufficient" threshold helpers:
    twizy_last_soc = twizy_soc;
    twizy_last_idealrange = twizy_range_ideal;
    
    // Switch on additional charger if not in / short before CV phase
    // and charge power level has not been limited by the user:
#if 0 // TODO
    if ((twizy_chargestate == 1) && (twizy_soc < 9400) && (cfg_chargelevel == 0))
      PORTBbits.RB4 = 1;
    else
      PORTBbits.RB4 = 0;
#endif
    
    // END OF STATE: CHARGING
  }

  else
  {
    // --------------------------------------------------------------------------
    // NOT CHARGING
    // 

    // Switch off additional charger:
    // TODO PORTBbits.RB4 = 0;

    // Calculate range:
    if (twizy_range_est > 0)
    {
      if (twizy_maxrange > 0)
        twizy_range_ideal = (((float) twizy_maxrange) * twizy_soc) / 10000;
      else
        twizy_range_ideal = twizy_range_est;
    }


    // Check if we've been charging before:
    if (twizy_chargestate <= 2)
    {
      ESP_LOGI(TAG, "charge stop");
      
      // yes, check if charging has been finished by the BMS
      // by checking if we've reached charge power level 0
      // (this is more reliable than checking for SOC 100% as some Twizy will
      // not reach 100% while others will still top off in 100% for some time)
      if (twizy_chg_power_request == 0)
      {
        float charge;
        float new_cap_prc;
        
        // yes, means "done"
        twizy_chargestate = 4;
        RequestNotify(SEND_ChargeState);
        
        // calculate battery capacity if charge started below 40% SOC:
        if (twizy_soc_min < 4000)
        {
          // scale CC charge part by SOC range:
          charge = (twizy_cc_charge / (twizy_cc_soc - twizy_soc_min)) * twizy_cc_soc;
          // add CV charge part:
          charge += (twizy_charge_rec - twizy_cc_charge);
          
          // convert to Ah:
          charge = charge / 400 / 3600;
          
          // convert to percent:
          new_cap_prc = charge / cfg_bat_cap_nominal_ah * 100;
          
          // smooth over 10 samples:
          if (cfg_bat_cap_actual_prc > 0)
          {
            new_cap_prc = (cfg_bat_cap_actual_prc * 9 + new_cap_prc) / 10;
          }
          cfg_bat_cap_actual_prc = new_cap_prc;
          
          // store in config flash:
          MyConfig.SetParamValueFloat("x.rt", "cap_act_prc", cfg_bat_cap_actual_prc);
        }

      }
      else
      {
        // no, means "stopped"
        twizy_chargestate = 21;
        
        // send charge state or alert depending on stop by user request:
        RequestNotify(twizy_chg_stop_request ? SEND_ChargeState : SEND_ChargeAlert);
      }

    }

    else if (twizy_flags.CarAwake && twizy_flags.ChargePort)
    {
      // Car awake, not charging, charge port open:
      // beginning the next car usage cycle:

      // Close charging port:
      twizy_flags.ChargePort = 0;
      twizy_chargeduration = 0;

      // Set charge state to "done":
      twizy_chargestate = 4;

      // reset SOC minimum:
      twizy_soc_min = twizy_soc;
      twizy_soc_min_range = twizy_range_est;
    }
    
    // clear charge stop request:
    twizy_chg_stop_request = 0;

    // END OF STATE: NOT CHARGING
  }
  
  
  // convert battery capacity percent to framework Ah:
  *StdMetrics.ms_v_bat_cac = (float) cfg_bat_cap_actual_prc / 100 * cfg_bat_cap_nominal_ah;
  


  // --------------------------------------------------------------------------
  // Notifications:
  // 
  // Note: these are distributed within a full minute to minimize
  // current load on the modem and match GPS requests
  // 

  int i = ticker % 60;
  
  // Update charge ETR once per minute:
  if (i == 0) {
    UpdateChargeTimes();
  }

  // Send RT-PWR-Stats update once per minute:
  if (i == 21) {
    RequestNotify(SEND_PowerStats);
  }
  
  // Send RT-BAT-* update once per minute:
  if (i == 42) {
    RequestNotify(SEND_BatteryStats);
  }

  // Send GPS-Log updates:
  if (twizy_speed > 0 && cfg_gpslog_interval > 0) {
    // … streaming mode:
    if (ticker - twizy_last_gpslog >= cfg_gpslog_interval) {
      RequestNotify(SEND_GPSLog);
      twizy_last_gpslog = ticker;
    }
  }
  else if (i == 21) {
    // … parking mode:
    RequestNotify(SEND_GPSLog);
  }
  

#ifdef OVMS_TWIZY_CFG

  if ((twizy_flags.CarAwake) && (twizy_flags.EnableWrite))
  {
    // --------------------------------------------------------------------------
    // Login to SEVCON:
    // 
    
    if (!twizy_flags.CtrlLoggedIn)
    {
      if (login(1) == 0)
        twizy_flags.CtrlLoggedIn = 1;
      // else retry on next ticker1 call
    }
  
  
    if (twizy_flags.CtrlLoggedIn)
    {
      // --------------------------------------------------------------------------
      // Check for button presses in STOP mode => CFG RESET:
      // 

      if ((twizy_button_cnt >= 3) && (!twizy_flags.DisableReset))
      {
        // reset SEVCON profile:
        memset(&twizy_cfg_profile, 0, sizeof(twizy_cfg_profile));
        twizy_cfg.unsaved = (twizy_cfg.profile_user > 0);
        vehicle_twizy_cfg_applyprofile(twizy_cfg.profile_user);
        twizy_notify(SEND_ResetResult);

        // reset button cnt:
        twizy_button_cnt = 0;
      }

      else if (twizy_button_cnt >= 1)
      {
        // pre-op also sends a CAN error, so for button_cnt >= 1
        // check if we're stuck in pre-op state:
        if ((readsdo(0x5110,0x00) == 0) && (twizy_sdo.data == 0x7f)) {
          // we're in pre-op, try to solve:
          if (configmode(0) == 0)
            twizy_button_cnt = 0; // solved
        }
      }


      // --------------------------------------------------------------------------
      // Valet mode: lock speed if valet max odometer reached:
      // 

      if ((twizy_flags.ValetMode)
              && (!twizy_flags.CarLocked) && (twizy_odometer > twizy_valet_odo))
      {
        vehicle_twizy_cfg_restrict_cmd(FALSE, CMD_Lock, NULL);
      }

      // --------------------------------------------------------------------------
      // Auto drive & recuperation adjustment (if enabled):
      // 

      vehicle_twizy_cfg_autopower();

    } // if (twizy_flags.CtrlLoggedIn)

  }


#endif // OVMS_TWIZY_CFG
  
  
  
  // --------------------------------------------------------------------------
  // Publish metrics:
  
  *StdMetrics.ms_v_charge_mode = (string)
    ((cfg_chargemode == TWIZY_CHARGEMODE_AUTOSTOP) ? "storage" : "standard");
  
  *StdMetrics.ms_v_charge_state = (string) chargestate_code(twizy_chargestate);
  *StdMetrics.ms_v_charge_substate = (string) chargesubstate_code(twizy_chg_stop_request);
  
  *StdMetrics.ms_v_bat_range_ideal = (float) twizy_range_ideal;
  *StdMetrics.ms_v_bat_range_est = (float) twizy_range_est;
  
  //*StdMetrics.ms_v_env_drivemode = (int) twizy_cfg.drivemode;
  *StdMetrics.ms_v_env_awake = (bool) twizy_flags.CarAwake;
  *StdMetrics.ms_v_env_on = (bool) twizy_flags.CarON;
  *StdMetrics.ms_v_env_locked = (bool) twizy_flags.CarLocked;
  *StdMetrics.ms_v_env_valet = (bool) twizy_flags.ValetMode;
  
  *StdMetrics.ms_v_charge_pilot = (bool) twizy_flags.PilotSignal;
  *StdMetrics.ms_v_door_chargeport = (bool) twizy_flags.ChargePort;
  *StdMetrics.ms_v_charge_inprogress = (bool) twizy_flags.Charging;
  *StdMetrics.ms_v_env_charging12v = (bool) twizy_flags.Charging12V;
  
  *StdMetrics.ms_v_env_ctrl_login = (bool) twizy_flags.CtrlLoggedIn;
  *StdMetrics.ms_v_env_ctrl_config = (bool) twizy_flags.CtrlCfgMode;
  
  
  // --------------------------------------------------------------------------
  // Send notifications:
  
  DoNotify();
  
}


/**
 * Ticker10: per 10 second ticker
 */

void OvmsVehicleRenaultTwizy::Ticker10(uint32_t ticker)
{
  // Check if CAN-Bus has turned offline:
  if (twizy_status & CAN_STATUS_ONLINE)
  {
    // Clear online flag to test for CAN activity:
    twizy_status &= ~CAN_STATUS_ONLINE;

#ifdef OVMS_TWIZY_CFG
    // TODO
    // Check for new SEVCON fault:
    if (readsdo(0x5320,0x00) == 0) {
      if (twizy_sdo.data && (twizy_sdo.data != twizy_alert_code)) {
        twizy_alert_code = twizy_sdo.data;
        twizy_notify(SEND_CodeAlert);
      }
    }
#endif // #ifdef #ifdef OVMS_TWIZY_CFG
  }
  else
  {
    // CAN offline: implies...
    twizy_status = CAN_STATUS_OFFLINE;
    *StdMetrics.ms_v_charge_voltage = (float) 0;
    twizy_speed = 0;
    twizy_power = 0;
    twizy_power_min = twizy_current_min = 32767;
    twizy_power_max = twizy_current_max = -32768;
  }
  
  
  // Control additional charger fan by RB1:
  // Elips 2000W specified operation range is -20..+50 °C
  // => switch on additional fan above 45 °C, off below 45 °C
  // As we don't get temperature updates after switching the
  // Twizy off, we need the timer to keep the fan ON for a while.

  if (twizy_flags.CarAwake || twizy_flags.Charging) {
    if (StdMetrics.ms_v_charge_temp->AsFloat() > TWIZY_FAN_THRESHOLD) {
      // TODO PORTBbits.RB1 = 1;
      if (twizy_fan_timer == 0)
        ESP_LOGW(TAG, "charger temperature %.1f celcius; extra fan switched ON",
          StdMetrics.ms_v_charge_temp->AsFloat());
      twizy_fan_timer = TWIZY_FAN_OVERSHOOT * 6;
    }
    else if (StdMetrics.ms_v_charge_temp->AsFloat() < TWIZY_FAN_THRESHOLD) {
      // TODO PORTBbits.RB1 = 0;
      if (twizy_fan_timer != 0)
        ESP_LOGI(TAG, "charger temperature %.1f celcius; extra fan switched OFF",
          StdMetrics.ms_v_charge_temp->AsFloat());
      twizy_fan_timer = 0;
    }
  }
  else {
    if (twizy_fan_timer > 0 && --twizy_fan_timer == 0) {
      // TODO PORTBbits.RB1 = 0;
      if (twizy_fan_timer != 0)
        ESP_LOGI(TAG, "charger extra fan overshoot end; switched OFF");
    }
  }
  
}


/**
 * UpdateMaxRange: get MAXRANGE with temperature compensation
 *  (Reminder: this could be a listener on ms_v_bat_temp…)
 */

void OvmsVehicleRenaultTwizy::UpdateMaxRange()
{
  float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat();
  
  // Temperature compensation:
  //   - assumes standard cfg_maxrange specified at 20°C
  //   - temperature influence approximation: 0.6 km / °C
  twizy_maxrange = cfg_maxrange - ((20 - bat_temp) * 6) / 10;
  if (twizy_maxrange < 0)
    twizy_maxrange = 0;
  
  *StdMetrics.ms_v_bat_range_full = (float) twizy_maxrange;
}


/**
 * UpdateChargeTimes:
 */

void OvmsVehicleRenaultTwizy::UpdateChargeTimes()
{
  float maxrange;
  
  
  // get max ideal range:
  
  UpdateMaxRange();
  
  if (twizy_maxrange > 0)
  {
    maxrange = twizy_maxrange;
  }
  else
  {
    // estimate max range:
    if (twizy_soc_min && twizy_soc_min_range)
      maxrange = twizy_soc_min_range / twizy_soc_min * 10000;
    else
      maxrange = 0;
  }
  
  
  // calculate ETR for SUFFSOC:
  
  if (cfg_suffsoc == 0)
  {
    *StdMetrics.ms_v_charge_duration_soc = (int) -1;
  }
  else
  {
    *StdMetrics.ms_v_charge_duration_soc = (int) ChargeTime(cfg_suffsoc * 100);
  }
  
  
  // calculate ETR for SUFFRANGE:
  
  if (cfg_suffrange == 0)
  {
    *StdMetrics.ms_v_charge_duration_range = (int) -1;
  }
  else
  {
    *StdMetrics.ms_v_charge_duration_range = (int)
      (maxrange > 0) ? ChargeTime(cfg_suffrange * 10000 / maxrange) : 0;
  }
  
  
  // calculate ETR for full charge:
  
  *StdMetrics.ms_v_charge_duration_full = (int) ChargeTime(10000);
  
}


/**
 * ChargeTime:
 *  Utility: calculate estimated charge time in minutes
 *  to reach dstsoc from current SOC
 *  (dstsoc in 1/100 %)
 */

// Charge time approximation constants:
// @ ~13 °C  -- temperature compensation needed?
#define CHARGETIME_CVSOC    9400L    // CV phase normally begins at 93..95%
#define CHARGETIME_CC       180L     // CC phase time (160..180 min.)
#define CHARGETIME_CV       40L      // CV phase time (topoff) (20..40 min.)

int OvmsVehicleRenaultTwizy::ChargeTime(int dstsoc)
{
  int minutes, d, amps;
  
  if (dstsoc > 10000)
    dstsoc = 10000;
  
  if (twizy_soc >= dstsoc)
    return 0;
  
  minutes = 0;
  
  if (dstsoc > CHARGETIME_CVSOC)
  {
    // CV phase
    if (twizy_soc < CHARGETIME_CVSOC)
      minutes += (((long) dstsoc - CHARGETIME_CVSOC) * CHARGETIME_CV
        + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);
    else
      minutes += (((long) dstsoc - twizy_soc) * CHARGETIME_CV
        + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);
    
    dstsoc = CHARGETIME_CVSOC;
  }
  
  // CC phase
  if (twizy_soc < dstsoc)
  {
    // default time:
    d = (((long) dstsoc - twizy_soc) * CHARGETIME_CC
      + (CHARGETIME_CVSOC/2)) / CHARGETIME_CVSOC;
    // correction for reduced charge power:
    // (current is 32A at level 7, else level * 5A)
    amps = cfg_chargelevel * 5;
    if ((twizy_flags.EnableWrite) && (amps > 0) && (amps < 35))
      d = (d * 32) / amps;
    minutes += d;
  }
  
  return minutes;
}



/**
 * SendGPSLog: send "RT-GPS-Log" history record
 *  (GPS track log with extended power usage data)
 */
void OvmsVehicleRenaultTwizy::SendGPSLog()
{
  bool modified =
    StdMetrics.ms_v_pos_odometer->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_latitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_longitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_altitude->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_direction->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_gpsspeed->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_pos_speed->IsModifiedAndClear(m_modifier);
  
  if (!modified)
    return;
  
  unsigned long pwr_dist, pwr_use, pwr_rec;
  signed int pwr_min, pwr_max, curr_min, curr_max;

  // Read power stats:
  
  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;

  // read & reset GPS section power & current min/max:
  pwr_min = twizy_power_min;
  pwr_max = twizy_power_max;
  curr_min = twizy_current_min;
  curr_max = twizy_current_max;
  twizy_power_min = twizy_current_min = 32767;
  twizy_power_max = twizy_current_max = -32768;
  
  // H type "RT-GPS-Log", recno = odometer, keep for 1 day
  ostringstream buf;
  buf
    << "RT-GPS-Log,"
    << (long) (StdMetrics.ms_v_pos_odometer->AsFloat(0, Miles) * 10) // in 1/10 mi
    << ",86400"
    << setprecision(8)
    << fixed
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << setprecision(0)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_direction->AsFloat()
    << "," << StdMetrics.ms_v_pos_speed->AsFloat()
    << "," << (int) StdMetrics.ms_v_pos_gpslock->AsBool()
    << "," << 120 - StdMetrics.ms_v_pos_latitude->Age()
    << "," << StdMetrics.ms_m_net_sq->AsInt()
    << "," << twizy_power * 64 / 10
    << "," << pwr_use / WH_DIV
    << "," << pwr_rec / WH_DIV
    << "," << pwr_dist / 10;
  
  if (pwr_min == 32767) {
    buf
      << ",0,0";
  } else {
    buf
      << "," << (pwr_min * 64 + 5) / 10
      << "," << (pwr_max * 64 + 5) / 10;
  }
  
  buf
    << setbase(16)
    << "," << (unsigned int) twizy_status
    << setbase(10)
    << "," << twizy_batt[0].max_drive_pwr * 500
    << "," << twizy_batt[0].max_recup_pwr * -500
    << setprecision(3)
    << "," << (float) twizy_autodrive_level / 1000
    << "," << (float) twizy_autorecup_level / 1000;

  if (curr_min == 32767) {
    buf
      << ",0,0";
  } else {
    buf
      << setprecision(2)
      << "," << (float) curr_min / 4
      << "," << (float) curr_max / 4;
  }
  
  MyNotify.NotifyString("data", buf.str().c_str());
}


/**
 * SendTripLog: send "RT-PWR-Log" history record
 */
void OvmsVehicleRenaultTwizy::SendTripLog()
{
  unsigned long pwr_dist, pwr_use, pwr_rec;

  // Read power stats:
  
  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;

  // H type "RT-PWR-Log"
	//   ,odometer_km,latitude,longitude,altitude
	//   ,chargestate,parktime_min
	//   ,soc,soc_min,soc_max
	//   ,power_used_wh,power_recd_wh,power_distance
	//   ,range_estim_km,range_ideal_km
	//   ,batt_volt,batt_volt_min,batt_volt_max
	//   ,batt_temp,batt_temp_min,batt_temp_max
	//   ,motor_temp,pem_temp
	//   ,trip_length_km,trip_soc_usage
	//   ,trip_avg_speed_kph,trip_avg_accel_kps,trip_avg_decel_kps
	//   ,charge_used_ah,charge_recd_ah,batt_capacity_prc
	//   ,chg_temp
  
  ostringstream buf;
  buf
    << "RT-PWR-Log,0,31536000" // recno = 0, keep for 365 days
    << fixed
    << setprecision(2)
    << "," << StdMetrics.ms_v_pos_odometer->AsFloat()
    << setprecision(8)
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << setprecision(0)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << (twizy_flags.ChargePort ? twizy_chargestate : 0)
    << "," << StandardMetrics.ms_v_env_parktime->AsInt(0, Minutes)
    << setprecision(2)
    << "," << (float) twizy_soc / 100
    << "," << (float) twizy_soc_min / 100
    << "," << (float) twizy_soc_max / 100
    << setprecision(0)
    << "," << pwr_use / WH_DIV
    << "," << pwr_rec / WH_DIV
    << "," << pwr_dist / 10
    << "," << twizy_range_est
    << "," << twizy_range_ideal
    << setprecision(1)
    << "," << (float) twizy_batt[0].volt_act / 10
    << "," << (float) twizy_batt[0].volt_min / 10
    << "," << (float) twizy_batt[0].volt_max / 10
    << setprecision(0)
    << "," << CONV_Temp(twizy_batt[0].temp_act)
    << "," << CONV_Temp(twizy_batt[0].temp_min)
    << "," << CONV_Temp(twizy_batt[0].temp_max)
    << "," << StdMetrics.ms_v_mot_temp->AsInt()
    << "," << StdMetrics.ms_v_inv_temp->AsInt()
    << setprecision(2)
    << "," << (float) (twizy_odometer - twizy_odometer_tripstart) / 100
    << "," << (float) ABS((long)twizy_soc_tripstart - (long)twizy_soc_tripend) / 100
    
    << "," << (float) ((twizy_speedpwr[CAN_SPEED_CONST].spdcnt > 0)
          ? twizy_speedpwr[CAN_SPEED_CONST].spdsum / twizy_speedpwr[CAN_SPEED_CONST].spdcnt
          : 0) / 100 // avg speed kph
    << "," << (float) ((twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt > 0)
          ? (twizy_speedpwr[CAN_SPEED_ACCEL].spdsum * 10) / twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt
          : 0) / 100 // avg accel kph/s
    << "," << (float) ((twizy_speedpwr[CAN_SPEED_DECEL].spdcnt > 0)
          ? (twizy_speedpwr[CAN_SPEED_DECEL].spdsum * 10) / twizy_speedpwr[CAN_SPEED_DECEL].spdcnt
          : 0) / 100 // avg decel kph/s
    
    << "," << (float) twizy_charge_use / AH_DIV
    << "," << (float) twizy_charge_rec / AH_DIV
    << "," << (float) cfg_bat_cap_actual_prc
    << "," << StdMetrics.ms_v_charge_temp->AsInt()
    ;
  
  MyNotify.NotifyString("data", buf.str().c_str());
}


/**
 * RequestNotify: send notifications / alerts / data updates
 */

void OvmsVehicleRenaultTwizy::RequestNotify(unsigned int which)
{
  twizy_notifications |= which;
}

void OvmsVehicleRenaultTwizy::DoNotify()
{
  unsigned int which = twizy_notifications;
  
  // Send battery alert?
  if (which & SEND_BatteryAlert) {
    MyNotify.NotifyCommand("alert", "xrt batt status");
    twizy_notifications &= ~SEND_BatteryAlert;
  }
  
  // Send charge alert?
  if (which & SEND_ChargeAlert) {
    MyNotify.NotifyCommand("alert", "stat");
    twizy_notifications &= ~SEND_ChargeState;
  }
  
  // Send charge state?
  if (which & SEND_ChargeState) {
    MyNotify.NotifyCommand("info", "stat");
    twizy_notifications &= ~SEND_ChargeState;
  }
  
  // Send power usage statistics?
  if (which & SEND_PowerNotify) {
    MyNotify.NotifyCommand("info", "xrt power report");
    twizy_notifications &= ~SEND_PowerNotify;
  }
  
  // Send drive log?
  if (which & SEND_TripLog) {
    SendTripLog();
    twizy_notifications &= ~SEND_TripLog;
  }
  
  // Send GPS log update?
  if (which & SEND_GPSLog) {
    SendGPSLog();
    twizy_notifications &= ~SEND_GPSLog;
  }
  
  // Send battery status update?
  if (which & SEND_BatteryStats) {
    BatterySendDataUpdate();
    twizy_notifications &= ~SEND_BatteryStats;
  }
  
  // Send power usage update?
  if (which & SEND_PowerStats) {
    if (PowerIsModified())
      MyNotify.NotifyCommand("data", "xrt power stats");
    twizy_notifications &= ~SEND_PowerStats;
  }
  
  // Send SEVCON SDO log update?
  if (which & SEND_SDOLog) {
    // TODO
    // stat = vehicle_twizy_sdolog_msgp(stat, 0x4600);
    // stat = vehicle_twizy_sdolog_msgp(stat, 0x4602);
    twizy_notifications &= ~SEND_SDOLog;
  }
  
}


/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::ProcessMsgCommand(std::string &result, int command, const char* args)
{
  switch (command)
  {
    case CMD_BatteryAlert:
      MyNotify.NotifyCommand("alert", "xrt batt status");
      return Success;
    
    case CMD_BatteryStatus:
      // send complete set of battery status records:
      BatterySendDataUpdate(true);
      return Success;
    
    case CMD_PowerUsageNotify:
      // send power usage text report:
      // args: <mode>: 't' = totals, fallback 'e' = efficiency
      if (args && (args[0] | 0x20) == 't')
        MyNotify.NotifyCommand("info", "xrt power totals");
      else
        MyNotify.NotifyCommand("info", "xrt power report");
      return Success;
    
    case CMD_PowerUsageStats:
      // send power usage data record:
      MyNotify.NotifyCommand("data", "xrt power stats");
      return Success;
    
    default:
      return NotImplemented;
  }
}


