/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: charge control
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
 * vehicle_twizy_ca: command wrapper for CommandCA (charge attributes)
 */
void vehicle_twizy_ca(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = (OvmsVehicleRenaultTwizy*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  
  if (!twizy || type != "RT")
  {
    writer->puts("Error: Twizy vehicle module not selected");
    return;
  }

  twizy->CommandCA(verbosity, writer, cmd, argc, argv);
}


/**
 * ChargeInit:
 */
void OvmsVehicleRenaultTwizy::ChargeInit()
{
  ESP_LOGI(TAG, "charge subsystem init");
  
  twizy_chg_stop_request = false;
  
  // init command
  cmd_ca = cmd_xrt->RegisterCommand("ca", "Charge attributes", vehicle_twizy_ca, "[R] | [<range>] [<soc>%] [L<0-7>] [S|N|H]", 0, 4);
}


/**
 * ChargeShutdown:
 */
void OvmsVehicleRenaultTwizy::ChargeShutdown()
{
  ESP_LOGI(TAG, "charge subsystem shutdown");
  
  cmd_xrt->UnregisterCommand("ca");
}


OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandCA(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;
  int capacity = verbosity;
  
  
  // Process arguments:
  
  for (int i=0; i < argc; i++)
  {
    unsigned char f = tolower(argv[i][0]); // lower case of first letter
    char unit = (strlen(argv[i]) > 0) ? argv[i][strlen(argv[i]) - 1] : 0;

    if (f == 'r') {
      // _R_eset:
      MyConfig.SetParamValueInt("xrt", "suffrange", 0);
      MyConfig.SetParamValueInt("xrt", "suffsoc", 0);
      MyConfig.SetParamValueInt("xrt", "chargelevel", 0);
      MyConfig.SetParamValueInt("xrt", "chargemode", 0);
      *StdMetrics.ms_v_charge_substate = (string) "";
      twizy_chg_stop_request = false; // this should not be necessary, but just in case...
      if (capacity > 0)
        capacity -= writer->printf("CA reset OK.\n");
    }
    else if (f == 's')
    {
      // _S_top mode:
      MyConfig.SetParamValueInt("xrt", "chargemode", 1);
    }
    else if (f == 'n')
    {
      // _N_otify mode:
      MyConfig.SetParamValueInt("xrt", "chargemode", 0);
    }
    else if (f == 'l')
    {
      // _L_evel:
      MyConfig.SetParamValueInt("xrt", "chargelevel", atoi(argv[i]+1));
    }
    else if (f == 'h')
    {
      // _H_alt: stop a running charge immediately
      if (twizy_flags.Charging && !twizy_chg_stop_request)
      {
        ESP_LOGI(TAG, "requesting charge stop (user command)");
        MyEvents.SignalEvent("vehicle.charge.substate.scheduledstop", NULL);
        *StdMetrics.ms_v_charge_substate = (string) "scheduledstop";
        twizy_chg_stop_request = true;
        if (capacity > 0)
          capacity -= writer->printf("Stopping charge.\n");
      }
    }
    else if (unit == '%')
    {
      // set sufficient soc:
      MyConfig.SetParamValueInt("xrt", "suffsoc", atoi(argv[i]));
    }
    else
    {
      // set sufficient range:
      MyConfig.SetParamValueInt("xrt", "suffrange", atoi(argv[i]));
    }
  }
  
  // Synchronize with config changes:
  if (argc > 0)
    ConfigChanged(NULL);
  
  // Output current charge alerts and estimated charge time remaining:
  
  UpdateChargeTimes();
  
  // Limits:
  if (capacity > 0)
  {
    if (cfg_suffsoc == 0 && cfg_suffrange == 0)
    {
      capacity -= writer->printf("Limits: OFF");
    }
    else
    {
      capacity -= writer->printf("Limits: %s", (cfg_chargemode == 1) ? "STOP" : "Notify");
      
      if (cfg_suffsoc > 0)
        capacity -= writer->printf(" @ %d%%", cfg_suffsoc);
      if (cfg_suffrange > 0)
        capacity -= writer->printf(" %s %d km", (cfg_suffsoc > 0) ? "or" : "@", cfg_suffrange);
      
      // …smallest ETR:
      capacity -= writer->printf("\nTime: %d min.",
        (StdMetrics.ms_v_charge_duration_range->AsInt() > 0 &&
         StdMetrics.ms_v_charge_duration_range->AsInt() < StdMetrics.ms_v_charge_duration_soc->AsInt())
        ? StdMetrics.ms_v_charge_duration_range->AsInt()
        : StdMetrics.ms_v_charge_duration_soc->AsInt());
    }
  }
  
  // Power level:
  if (capacity > 0)
    capacity -= writer->printf("\nLevel: %d", (int) cfg_chargelevel);
  
  // Estimated charge time for 100%:
  if (capacity > 0)
    capacity -= writer->printf("\nFull: %d min.\n", StdMetrics.ms_v_charge_duration_full->AsInt());
  
  // Charge status:
  if (capacity > 0)
  {
    std::string charge_state;
    bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
    if (chargeport_open)
    {
      charge_state = StdMetrics.ms_v_charge_state->AsString();
      
      // Translate state codes:
      if (charge_state == "charging")
        charge_state = "Charging";
      else if (charge_state == "topoff")
        charge_state = "Topping off";
      else if (charge_state == "done")
        charge_state = "Charge Done";
      else if (charge_state == "stopped")
        charge_state = "Charge Stopped";
    }
    else
    {
      // Charge port door is closed, not charging
      charge_state = "Not charging";
    }
    capacity -= writer->printf("%s\n", charge_state.c_str());
  }
  
  // Estimated + Ideal Range:
  if (capacity > 0)
  {
    const char* range_est = StdMetrics.ms_v_bat_range_est->AsString("?", rangeUnit, 0).c_str();
    const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("?", rangeUnit, 0).c_str();
    writer->printf("Range: %s - %s\n", range_est, range_ideal);
  }
  
  // SOC:
  if (capacity > 0)
  {
    capacity -= writer->printf("SOC: %s\n",
      (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  }

  return Success;
}


/**
 * MsgCommandCA:
 *  - CMD_QueryChargeAlerts()
 *  - CMD_SetChargeAlerts(<range>,<soc>,[<power>],[<stopmode>])
 *  - Result: <range>,<soc>,<time_range>,<time_soc>,<time_full>,<power>,<stopmode>
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::MsgCommandCA(std::string &result, int command, const char* args)
{
  if (command == CMD_SetChargeAlerts)
  {
    std::istringstream sentence(args);
    std::string token;
    
    // CMD_SetChargeAlerts(<range>,<soc>,[<power>],[<stopmode>])
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xrt", "suffrange", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xrt", "suffsoc", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xrt", "chargelevel", atoi(token.c_str()));
    if (std::getline(sentence, token, ','))
      MyConfig.SetParamValueInt("xrt", "chargemode", atoi(token.c_str()));
    
    // Synchronize with config changes:
    ConfigChanged(NULL);
  }
  
  UpdateChargeTimes();
  
  // Result: <range>,<soc>,<time_range>,<time_soc>,<time_full>,<power>,<stopmode>
  std::ostringstream buf;
  buf
    << std::setprecision(0)
    << cfg_suffrange << ","
    << cfg_suffsoc << ","
    << StdMetrics.ms_v_charge_duration_range->AsInt() << ","
    << StdMetrics.ms_v_charge_duration_soc->AsInt() << ","
    << StdMetrics.ms_v_charge_duration_full->AsInt() << ","
    << cfg_chargelevel << ","
    << cfg_chargemode;
  
  result = buf.str();
  
  return Success;
}


OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandSetChargeMode(vehicle_mode_t mode)
{
  if (mode > 1)
    ESP_LOGW(TAG, "CommandSetChargeMode: unsupported mode %d, falling back to 1 (storage)", mode);
  MyConfig.SetParamValueInt("xrt", "chargemode", (mode == 0) ? 0 : 1);
  return Success;
}

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandSetChargeCurrent(uint16_t limit)
{
  MyConfig.SetParamValueInt("xrt", "chargelevel", limit / 5);
  return Success;
}

OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandStopCharge()
{
  if (twizy_flags.Charging && !twizy_chg_stop_request)
  {
    ESP_LOGI(TAG, "requesting charge stop (user command)");
    MyEvents.SignalEvent("vehicle.charge.substate.scheduledstop", NULL);
    *StdMetrics.ms_v_charge_substate = (string) "scheduledstop";
    twizy_chg_stop_request = true;
  }
  return Success;
}


