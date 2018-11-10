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

#define VERSION "0.18.1"

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

  // init commands:
  cmd_xrt = MyCommandApp.RegisterCommand("xrt", "Renault Twizy", NULL, "", 0, 0, true);

  // init event listener:
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "gps.lock.acquired", std::bind(&OvmsVehicleRenaultTwizy::EventListener, this, _1, _2));

  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

  // init subsystems:
  BatteryInit();
  PowerInit();
  ChargeInit();
  WebInit();

  // init can bus:
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleRenaultTwizy::CanResponder, this, _1));

  // init SEVCON connection:
  m_sevcon = new SevconClient(this);

  m_ready = true;
}

OvmsVehicleRenaultTwizy::~OvmsVehicleRenaultTwizy()
{
  ESP_LOGI(TAG, "Shutdown Renault Twizy vehicle module");

  // release GPS:
  MyEvents.SignalEvent("vehicle.release.gps", NULL);
  MyEvents.SignalEvent("vehicle.release.gpstime", NULL);

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
      return MyConfig.GetParamValue("xrt", "gpslogint", XSTR(0));
    case 1:
      return MyConfig.GetParamValue("xrt", "kd_threshold", XSTR(CFG_DEFAULT_KD_THRESHOLD));
    case 2:
      return MyConfig.GetParamValue("xrt", "kd_compzero", XSTR(CFG_DEFAULT_KD_COMPZERO));
    case 6:
      return MyConfig.GetParamValue("xrt", "chargemode", XSTR(0));
    case 7:
      return MyConfig.GetParamValue("xrt", "chargelevel", XSTR(0));
    case 10:
      return MyConfig.GetParamValue("xrt", "suffsoc", XSTR(0));
    case 11:
      return MyConfig.GetParamValue("xrt", "suffrange", XSTR(0));
    case 12:
      return MyConfig.GetParamValue("xrt", "maxrange", XSTR(CFG_DEFAULT_MAXRANGE));
    case 13:
      return MyConfig.GetParamValue("xrt", "cap_act_prc", XSTR(100));
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
      return MyConfig.GetParamValue("xrt", "cap_nom_ah", XSTR(CFG_DEFAULT_CAPACITY));
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
      MyNotify.NotifyCommand("alert", "batt.alert", "xrt batt status");
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
        MyNotify.NotifyCommand("info", "xrt.pwr.totals", "xrt power totals");
      else
        MyNotify.NotifyCommand("info", "xrt.trip.report", "xrt power report");
      return Success;

    case CMD_PowerUsageStats:
      // send power usage data record:
      MyNotify.NotifyCommand("data", "xrt.pwr.log", "xrt power stats");
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

  // send result:
  ostringstream buf;
  buf << "Profile #" << key+1 << ": " << m_sevcon->FmtSwitchProfileResult(res);
  result = buf.str();

  MyNotify.NotifyStringf("info", "xrt.sevcon.profile", result.c_str());

  if (res == COR_OK || res == COR_ERR_StateChangeFailed)
    return Success;
  else
    return Fail;
}


/**
 * MsgCommandRestrict: lock/unlock, valet/unvalet
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandRestrict(string& result, int command, const char* args)
{
  // TODO
  return NotImplemented;
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
