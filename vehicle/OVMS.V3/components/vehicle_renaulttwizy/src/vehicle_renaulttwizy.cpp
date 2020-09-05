/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: framework integration (config, metrics, commands, events)
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
static const char *TAG = "v-twizy";

#define VERSION "1.11.0"

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


OvmsVehicleRenaultTwizy* OvmsVehicleRenaultTwizy::GetInstance(OvmsWriter* writer /*=NULL*/)
{
  OvmsVehicleRenaultTwizy* twizy = (OvmsVehicleRenaultTwizy*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!twizy || type != "RT") {
    if (writer)
      writer->puts("Error: Twizy vehicle module not selected");
    return NULL;
  }
  return twizy;
}


/**
 * Constructor & destructor
 */

size_t OvmsVehicleRenaultTwizy::m_modifier = 0;

OvmsVehicleRenaultTwizy::OvmsVehicleRenaultTwizy()
  : twizy_obd_rxwait(1,1)
{
  ESP_LOGI(TAG, "Renault Twizy vehicle module");

  memset(&twizy_flags, 0, sizeof twizy_flags);

  // init configs:
  MyConfig.RegisterParam("xrt", "Renault Twizy", true, true);
  ConfigChanged(NULL);

  // init metrics:
  if (m_modifier == 0) {
    m_modifier = MyMetrics.RegisterModifier();
    ESP_LOGD(TAG, "registered metric modifier is #%d", m_modifier);
  }
  m_version = MyMetrics.InitString("xrt.m.version", 0, VERSION " " __DATE__ " " __TIME__);

  mt_charger_status = MyMetrics.InitInt("xrt.v.c.status", SM_STALE_MIN, 0);
  mt_bms_status     = MyMetrics.InitInt("xrt.v.b.status", SM_STALE_MIN, 0);
  mt_sevcon_status  = MyMetrics.InitInt("xrt.v.i.status", SM_STALE_MIN, 0);

  mt_bms_alert_12v  = MyMetrics.InitBool("xrt.v.b.alert.12v", SM_STALE_MIN, false);
  mt_bms_alert_batt = MyMetrics.InitBool("xrt.v.b.alert.batt", SM_STALE_MIN, false);
  mt_bms_alert_temp = MyMetrics.InitBool("xrt.v.b.alert.temp", SM_STALE_MIN, false);

  // init commands:
  cmd_xrt = MyCommandApp.RegisterCommand("xrt", "Renault Twizy");

  // init event listener:
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "gps.lock.acquired", std::bind(&OvmsVehicleRenaultTwizy::EventListener, this, _1, _2));

  // init subsystems:
  BatteryInit();
  PowerInit();
  ChargeInit();
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif

  // init internal state from persistent metrics:
  twizy_soc = StdMetrics.ms_v_bat_soc->AsFloat() * 100;
  twizy_odometer = StdMetrics.ms_v_pos_odometer->AsFloat() * 100;
  twizy_tmotor = StdMetrics.ms_v_mot_temp->AsFloat();
  if (StdMetrics.ms_v_bat_range_est->IsDefined())
    twizy_range_est = StdMetrics.ms_v_bat_range_est->AsFloat();
  twizy_speedpwr[CAN_SPEED_CONST].use = StdMetrics.ms_v_bat_energy_used->AsFloat() * 1000 * WH_DIV;
  twizy_speedpwr[CAN_SPEED_CONST].rec = StdMetrics.ms_v_bat_energy_recd->AsFloat() * 1000 * WH_DIV;

  // init can bus:
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleRenaultTwizy::CanResponder, this, _1));

  // init SEVCON connection:
  m_sevcon = new SevconClient(this);

  // init OBD2 poller:
  ObdInit();
}

OvmsVehicleRenaultTwizy::~OvmsVehicleRenaultTwizy()
{
  ESP_LOGI(TAG, "Shutdown Renault Twizy vehicle module");

  // unregister event listeners:
  MyEvents.DeregisterEvent(TAG);

  if (m_sevcon)
    delete m_sevcon;
}

const char* OvmsVehicleRenaultTwizy::VehicleShortName()
{
  return "Twizy";
}


/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleRenaultTwizy::ConfigChanged(OvmsConfigParam* param)
{
  if (param && param->GetName() != "xrt")
    return;

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
  //  lock_on           Engage lock mode (Default: no)
  //  valet_on          Engage valet mode (Default: no)
  //
  //  aux_fan_port      EGPIO port to control auxiliary charger fan (Default: 0 = disabled)
  //  aux_charger_port  EGPIO port to control auxiliary charger (Default: 0 = disabled)
  //
  //  dtc_autoreset     Reset DTC statistics on every drive/charge start (Default: no)
  //

  cfg_maxrange = MyConfig.GetParamValueInt("xrt", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (cfg_maxrange <= 0)
    cfg_maxrange = CFG_DEFAULT_MAXRANGE;

  cfg_suffsoc = MyConfig.GetParamValueInt("xrt", "suffsoc");
  *StdMetrics.ms_v_charge_limit_soc = (float) cfg_suffsoc;

  cfg_suffrange = MyConfig.GetParamValueInt("xrt", "suffrange");
  *StdMetrics.ms_v_charge_limit_range = (float) cfg_suffrange;

  cfg_chargemode = MyConfig.GetParamValueInt("xrt", "chargemode");
  cfg_chargelevel = MyConfig.GetParamValueInt("xrt", "chargelevel");

  cfg_bat_cap_actual_prc = MyConfig.GetParamValueFloat("xrt", "cap_act_prc", 100);
  cfg_bat_cap_nominal_ah = MyConfig.GetParamValueFloat("xrt", "cap_nom_ah", CFG_DEFAULT_CAPACITY);

  twizy_flags.EnableWrite = MyConfig.GetParamValueBool("xrt", "canwrite", false);
  twizy_flags.DisableReset = !MyConfig.GetParamValueBool("xrt", "autoreset", true);
  twizy_flags.DisableKickdown = !MyConfig.GetParamValueBool("xrt", "kickdown", true);
  twizy_flags.DisableAutoPower = !MyConfig.GetParamValueBool("xrt", "autopower", true);
  twizy_flags.EnableInputs = MyConfig.GetParamValueBool("xrt", "console", false);

  cfg_gpslog_interval = MyConfig.GetParamValueInt("xrt", "gpslogint", 0);

  cfg_kd_threshold = MyConfig.GetParamValueInt("xrt", "kd_threshold", CFG_DEFAULT_KD_THRESHOLD);
  cfg_kd_compzero = MyConfig.GetParamValueInt("xrt", "kd_compzero", CFG_DEFAULT_KD_COMPZERO);

  if (MyConfig.IsDefined("xrt", "lock_on") && !CarLocked()) {
    twizy_lock_speed = MyConfig.GetParamValueInt("xrt", "lock_on", 6);
    SetCarLocked(true);
  }
  if (MyConfig.IsDefined("xrt", "valet_on") && !ValetMode()) {
    twizy_valet_odo = MyConfig.GetParamValueInt("xrt", "valet_on");
    SetValetMode(true);
  }

  cfg_aux_fan_port = MyConfig.GetParamValueInt("xrt", "aux_fan_port");
  cfg_aux_charger_port = MyConfig.GetParamValueInt("xrt", "aux_charger_port");

  cfg_dtc_autoreset = MyConfig.GetParamValueBool("xrt", "dtc_autoreset", false);
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
      MyConfig.SetParamValue("xrt", "gpslogint", value);
      return true;
    case 1:
      MyConfig.SetParamValue("xrt", "kd_threshold", value);
      return true;
    case 2:
      MyConfig.SetParamValue("xrt", "kd_compzero", value);
      return true;
    case 6:
      MyConfig.SetParamValue("xrt", "chargemode", value);
      return true;
    case 7:
      MyConfig.SetParamValue("xrt", "chargelevel", value);
      return true;
    case 10:
      MyConfig.SetParamValue("xrt", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xrt", "suffrange", value);
      return true;
    case 12:
      MyConfig.SetParamValue("xrt", "maxrange", value);
      return true;
    case 13:
      MyConfig.SetParamValue("xrt", "cap_act_prc", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xrt", "canwrite",  (bits& 1)!=0);
      MyConfig.SetParamValueBool("xrt", "autoreset", (bits& 2)==0);
      MyConfig.SetParamValueBool("xrt", "kickdown",  (bits& 4)==0);
      MyConfig.SetParamValueBool("xrt", "autopower", (bits& 8)==0);
      MyConfig.SetParamValueBool("xrt", "console",   (bits&16)!=0);
      return true;
    }
    case 16:
      MyConfig.SetParamValue("xrt", "cap_nom_ah", value);
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
      return MyConfig.GetParamValue("xrt", "gpslogint", STR(0));
    case 1:
      return MyConfig.GetParamValue("xrt", "kd_threshold", STR(CFG_DEFAULT_KD_THRESHOLD));
    case 2:
      return MyConfig.GetParamValue("xrt", "kd_compzero", STR(CFG_DEFAULT_KD_COMPZERO));
    case 6:
      return MyConfig.GetParamValue("xrt", "chargemode", STR(0));
    case 7:
      return MyConfig.GetParamValue("xrt", "chargelevel", STR(0));
    case 10:
      return MyConfig.GetParamValue("xrt", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xrt", "suffrange", STR(0));
    case 12:
      return MyConfig.GetParamValue("xrt", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
    case 13:
      return MyConfig.GetParamValue("xrt", "cap_act_prc", STR(100));
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("xrt", "canwrite",  false) ?  1 : 0) |
        (!MyConfig.GetParamValueBool("xrt", "autoreset", true)  ?  2 : 0) |
        (!MyConfig.GetParamValueBool("xrt", "kickdown",  true)  ?  4 : 0) |
        (!MyConfig.GetParamValueBool("xrt", "autopower", true)  ?  8 : 0) |
        ( MyConfig.GetParamValueBool("xrt", "console",   false) ? 16 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    case 16:
      return MyConfig.GetParamValue("xrt", "cap_nom_ah", STR(CFG_DEFAULT_CAPACITY));
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
    RequestNotify(SEND_GPSLog);
  }
}


/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::ProcessMsgCommand(string& result, int command, const char* args)
{
  switch (command)
  {
    case CMD_BatteryAlert:
      MyNotify.NotifyCommand("alert", "battery.status", "xrt batt status");
      return Success;

    case CMD_BatteryStatus:
      // send complete set of battery status records:
      if (!BatteryLock(100))
        return Fail;
      BatterySendDataUpdate(true);
      BatteryUnlock();
      return Success;

    case CMD_PowerUsageNotify:
      // send power usage text report:
      // args: <mode>: 't' = totals, fallback 'e' = efficiency
      if (args && (args[0] | 0x20) == 't')
        MyNotify.NotifyCommand("info", "xrt.power.totals", "xrt power totals");
      else
        MyNotify.NotifyCommand("info", "xrt.trip.report", "xrt power report");
      return Success;

    case CMD_PowerUsageStats:
      // send power usage data record:
      MyNotify.NotifyCommand("data", "xrt.power.log", "xrt power stats");
      return Success;

    case CMD_QueryChargeAlerts:
    case CMD_SetChargeAlerts:
      return MsgCommandCA(result, command, args);

    case CMD_Homelink:
      return MsgCommandHomelink(result, command, args);

    case CMD_Lock:
    case CMD_UnLock:
    case CMD_ValetOn:
    case CMD_ValetOff:
      return MsgCommandRestrict(result, command, args);

    case CMD_QueryLogs:
      return MsgCommandQueryLogs(result, command, args);
    case CMD_ResetLogs:
      return MsgCommandResetLogs(result, command, args);

    default:
      return NotImplemented;
  }
}


/**
 * MsgCommandHomelink: switch tuning profile
 *      App labels "1"/"2"/"3", sent as 0/1/2 (standard homelink function)
 *      App label "Default" sent without key, mapped to -1
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandHomelink(string& result, int command, const char* args)
{
  // parse args:
  int key = -1;
  if (args && *args)
    key = atoi(args);

  // switch profile:
  CANopenResult_t res = m_sevcon->CfgSwitchProfile(key+1);
  m_sevcon->UpdateProfileMetrics();

  // send result:
  ostringstream buf;
  buf << "Profile #" << key+1 << ": " << m_sevcon->FmtSwitchProfileResult(res);
  result = buf.str();

  MyNotify.NotifyStringf("info", "xrt.sevcon.profile.switch", result.c_str());

  if (res == COR_OK || res == COR_ERR_StateChangeFailed)
    return Success;
  else
    return Fail;
}


/**
 * MsgCommandRestrict: lock/unlock, valet/unvalet
 *
 * case CMD_Lock:
 *    param = kph: restrict speed (default=6 kph)
 * case CMD_UnLock:
 *    set speed back to profile, if valet allow +1 km
 *
 * case CMD_ValetOn:
 *    param = km to drive before locking to 6 kph
 * case CMD_ValetOff:
 *    disable km limit, set speed back to profile
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandRestrict(string& result, int command, const char* args)
{
  int pval;
  CANopenResult_t res = COR_OK;

  switch (command)
  {
    case CMD_Lock:
      // set fwd/rev speed lock:
      pval = (args && *args) ? atoi(args) : 6;
      res = m_sevcon->CfgLock(pval);
      if (res == COR_OK) {
        twizy_lock_speed = pval;
        SetCarLocked(true);
        MyConfig.SetParamValueInt("xrt", "lock_on", pval);
      }
      break;

    case CMD_ValetOn:
      // switch on valet mode:
      pval = (args && *args) ? atoi(args) : 1;
      twizy_valet_odo = twizy_odometer + (pval * 100);
      SetValetMode(true);
      MyConfig.SetParamValueInt("xrt", "valet_on", pval);
      break;

    case CMD_ValetOff:
      // switch off valet mode:
      SetValetMode(false);
      MyConfig.DeleteInstance("xrt", "valet_on");
      if (!CarLocked())
        break;
      // car is locked, continue with UnLock:
#if __GNUC__ >= 7
      [[fallthrough]];
#endif
      
    case CMD_UnLock:
      res = m_sevcon->CfgUnlock();
      if (res == COR_OK) {
        SetCarLocked(false);
        MyConfig.DeleteInstance("xrt", "lock_on");
        // if in valet mode, allow 1 more km:
        if (ValetMode()) {
          twizy_valet_odo = twizy_odometer + 100;
          MyConfig.SetParamValueInt("xrt", "valet_on", twizy_valet_odo);
        }
      }
      break;
  }
  
  // format status result:

  ostringstream buf;

  if (res != COR_OK) {
    buf << m_sevcon->GetResultString(res);
    buf << " - Status: ";
  }

  if (CarLocked())
    buf << "LOCKED to max kph=" << twizy_lock_speed;
  else
    buf << "UNLOCKED";

  if (ValetMode())
    buf << ", VALET ON at odo=" << (float)twizy_valet_odo / 100;
  else
    buf << ", VALET OFF";
  
  result = buf.str();
  ESP_LOGD(TAG, "MsgCommandRestrict: cmd_%d(%s) => %s", command, args ? args : "", result.c_str());

  if (res == COR_OK)
    return Success;
  else
    return Fail;
}


OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandLock(const char* pin)
{
  string buf;
  return MsgCommandRestrict(buf, CMD_Lock, pin);
}

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandUnlock(const char* pin)
{
  string buf;
  return MsgCommandRestrict(buf, CMD_UnLock, pin);
}

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandActivateValet(const char* pin)
{
  string buf;
  return MsgCommandRestrict(buf, CMD_ValetOn, pin);
}

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandDeactivateValet(const char* pin)
{
  string buf;
  return MsgCommandRestrict(buf, CMD_ValetOff, pin);
}


/**
 * MsgCommandQueryLogs:
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandQueryLogs(string& result, int command, const char* args)
{
  int which = 1;
  int start = 0;

  // parse args:
  if (args && *args) {
    char *arg;
    if ((arg = strsep((char**) &args, ",")) != NULL)
      which = atoi(arg);
    if ((arg = strsep((char**) &args, ",")) != NULL)
      start = atoi(arg);
  }

  // execute:
  int totalcnt = 0, sendcnt = 0;
  CANopenResult_t res = m_sevcon->QueryLogs(0, NULL, which, start, &totalcnt, &sendcnt);
  if (res != COR_OK) {
    result = "Failed: ";
    result += mp_encode(m_sevcon->GetResultString(res));
    return Fail;
  }
  else {
    ostringstream buf;
    if (sendcnt == 0)
      buf << "No log entries retrieved";
    else
      buf << "Log entries #" << start << "-" << start+sendcnt-1 << " of " << totalcnt << " retrieved";
    result = buf.str();
    return Success;
  }
}


/**
 * MsgCommandResetLogs:
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandResetLogs(string& result, int command, const char* args)
{
  int which = 99;

  // parse args:
  if (args && *args) {
    char *arg;
    if ((arg = strsep((char**) &args, ",")) != NULL)
      which = atoi(arg);
  }

  // execute:
  int cnt = 0;
  CANopenResult_t res = m_sevcon->ResetLogs(which, &cnt);
  if (res != COR_OK) {
    result = "Failed: ";
    result += mp_encode(m_sevcon->GetResultString(res));
    return Fail;
  }
  else {
    ostringstream buf;
    buf << cnt << " log entries cleared";
    result = buf.str();
    return Success;
  }
}
