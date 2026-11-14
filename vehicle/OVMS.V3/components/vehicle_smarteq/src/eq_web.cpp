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

 
#include "vehicle_smarteq.h"
#include "ovms_utils.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleSmartEQ::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xsq/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/climateschedule", "Climate Schedule", OvmsWebServer::HandleCfgPreconditionSchedule, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/tpms", "TPMS Config", WebCfgTPMS, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/adc", "ADC Calc", WebCfgADC, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/trickle", "12V trickle charge", WebCfgTrickle, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/battery", "Battery config", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleSmartEQ::WebDeInit()
{
  MyWebServer.DeregisterPage("/xsq/features");
  MyWebServer.DeregisterPage("/xsq/climateschedule");
  MyWebServer.DeregisterPage("/xsq/tpms");
  MyWebServer.DeregisterPage("/xsq/adc");
  MyWebServer.DeregisterPage("/xsq/trickle");
  MyWebServer.DeregisterPage("/xsq/battery");
  MyWebServer.DeregisterPage("/xsq/cellmon");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xsq/config)
 */
void OvmsVehicleSmartEQ::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicleSmartEQ* sq = (OvmsVehicleSmartEQ*)MyVehicleFactory.ActiveVehicle();
  if (!sq) {
    c.head(400);
    c.alert("danger", "Error: smartEQ vehicle not available");
    c.done();
    return;
  }

  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("xsq");
  std::string error;

  if (c.method == "POST") {
    // process form submission:
    std::string canwrite_mode = c.getvar("canwrite_mode");
    map["canwrite"]               = (canwrite_mode == "ON") ? "yes" : "no";
    map["canwrite.caron"]         = (canwrite_mode == "CARON") ? "yes" : "no";
    map["canwrite.caroff"]        = (canwrite_mode == "CAROFF") ? "yes" : "no";
    map["canwrite.sleep"]         = (c.getvar("disable_canwrite") == "yes") ? "yes" : "no";
    map["led"]                    = (c.getvar("led") == "yes") ? "yes" : "no";
    map["rebootnw"]               = c.getvar("rebootnw");
    map["resettrip"]              = (c.getvar("resettrip") == "yes") ? "yes" : "no";
    map["resettotal"]             = (c.getvar("resettotal") == "yes") ? "yes" : "no";
    map["bcvalue"]                = (c.getvar("bcvalue") == "yes") ? "yes" : "no";
    map["full.km"]                = c.getvar("full_km");
    map["unlock.warning"]         = (c.getvar("unlocked") == "yes") ? "yes" : "no";
    map["extended.stats"]         = (c.getvar("extstats") == "yes") ? "yes" : "no";
    map["reset.notify"]           = (c.getvar("resetnotify") == "yes") ? "yes" : "no";
    map["door.warning"]           = (c.getvar("opendoors") == "yes") ? "yes" : "no";
    map["gps.log"]                = (c.getvar("gpslog") == "yes") ? "yes" : "no";
    map["obdii.79b"]              = (c.getvar("obdii79b") == "yes") ? "yes" : "no";
    map["obdii.79b.cell"]         = (c.getvar("obdii79b.cell") == "yes") ? "yes" : "no";
    map["obdii.743"]              = (c.getvar("obdii743") == "yes") ? "yes" : "no";
    map["obdii.745"]              = (c.getvar("obdii745") == "yes") ? "yes" : "no";
    map["obdii.745.tpms"]         = (c.getvar("obdii745_tpms") == "yes") ? "yes" : "no";
    map["obdii.7e4"]              = (c.getvar("obdii7e4") == "yes") ? "yes" : "no";
    map["obdii.7e4.dcdc"]         = (c.getvar("obdii7e4_dcdc") == "yes") ? "yes" : "no";
    map["bms.contactor.1h.limit"] = c.getvar("contactor_1h_limit");

    // Basic numeric validation:
    auto validFloat = [&](const std::string& s, double minv, double maxv, const char* fname)->bool {
      if (s.empty()) return false;
      char* end=nullptr;
      double v = strtod(s.c_str(), &end);
      if (end==s.c_str() || *end!='\0' || v<minv || v>maxv) {
        error += std::string("<li>Invalid ")+fname+"</li>";
        return false;
      }
      return true;
    };
    if (!validFloat(map["full.km"], 50, 300, "WLTP km")) map["full.km"] = "126.0";
    if (!validFloat(map["rebootnw"], 0, 1440, "Restart Network Time")) map["rebootnw"] = "0";
    if (!validFloat(map["bms.contactor.1h.limit"], 1, 100, "Contactor 1h limit")) map["bms.contactor.1h.limit"] = "8";

    if (error.empty()) {
      // Write all changes in one operation
      MyConfig.SetParamMap("xsq", map);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Success!</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    if (map["resettrip"].empty())              map["resettrip"] = "yes";
    if (map["unlock.warning"].empty())         map["unlock.warning"] = "yes";
    if (map["door.warning"].empty())           map["door.warning"] = "yes";
    if (map["obdii.79b"].empty())              map["obdii.79b"] = "yes";
    if (map["obdii.79b.cell"].empty())         map["obdii.79b.cell"] = "yes";
    if (map["obdii.743"].empty())              map["obdii.743"] = "yes";
    if (map["obdii.745"].empty())              map["obdii.745"] = "yes";
    if (map["obdii.745.tpms"].empty())         map["obdii.745.tpms"] = "yes";
    if (map["obdii.7e4"].empty())              map["obdii.7e4"] = "yes";
    if (map["obdii.7e4.dcdc"].empty())         map["obdii.7e4.dcdc"] = "yes";
    if (map["rebootnw"].empty())               map["rebootnw"] = "30";
    if (map["full.km"].empty())                map["full.km"] = "126";
    if (map["bms.contactor.1h.limit"].empty()) map["bms.contactor.1h.limit"] = "8";
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "smart EQ features configuration");
  c.form_start(p.uri);
  
  c.fieldset_start("Remote Control");

  std::string canwrite_mode = "OFF";
  if (strtobool(map["canwrite"])) canwrite_mode = "ON";
  else if (strtobool(map["canwrite.caron"])) canwrite_mode = "CARON";
  else if (strtobool(map["canwrite.caroff"])) canwrite_mode = "CAROFF";
  c.input_radiobtn_start("CAN Bus Write Access", "canwrite_mode");
    c.input_radiobtn_option("canwrite_mode", "Off", "OFF", canwrite_mode == "OFF");
    c.input_radiobtn_option("canwrite_mode", "On", "ON", canwrite_mode == "ON");
    c.input_radiobtn_option("canwrite_mode", "Car Off", "CAROFF", canwrite_mode == "CAROFF");
    c.input_radiobtn_option("canwrite_mode", "Car On", "CARON", canwrite_mode == "CARON");
  c.input_radiobtn_end(
    "<dl>"
    "<dt>Off</dt><dd><p>CAN write is never allowed. The module is only listening to the messages on the CAN bus</p></dd>"
    "<dt>On</dt><dd><p>CAN write is allowed at all times (OBDII polling, start preconditioning etc.)</p></dd>"
    "<dt>Car Off</dt><dd><p>CAN write access only when car is <strong>OFF</strong>. Clears polling list when vehicle is switched on."
    " This will stop CAN polling when the vehicle is on and moving</p></dd>"
    "<dt>Car On</dt><dd><p>CAN write access only, when the car is <strong>ON</strong>."
    " Polling is disabled, when the vehicle is in awake/sleep mode</p></dd>"
    "</dl>");

  c.input_checkbox("Disable CAN polling during sleep", "disable_canwrite", strtobool(map["canwrite.sleep"]),
    "<p>Clears polling list when vehicle is in sleep mode</p>");
  c.fieldset_end(); 

  // trip reset or OBD activation
  c.fieldset_start("Trip calculated or OBD kWh/100km");
  c.input_checkbox("Enable GPS history log", "gpslog", strtobool(map["gps.log"]),
    "<p>Opt in to send a GPS history record while driving. This contains position and vehicle data and is disabled by default.</p>"
    "<p>Note: For OVMS Connect App, this option improves tracking and fills in the waypoints if the connection to the PyOVMS V3 server has been interrupted.</p>");
  c.input_checkbox("Reset Trip when Charging", "resettrip", strtobool(map["resettrip"]),
    "<p>On=reset on charge, Off=reset on drive</p>");
  c.input_checkbox("Reset kWh/100km on car on", "resettotal", strtobool(map["resettotal"]),
    "<p>Auto-disabled after reset</p>");
  c.input_checkbox("Trip Reset Notification", "resetnotify", strtobool(map["reset.notify"]),
    "<p>Notify with trip values on reset</p>");
  c.input_checkbox("OBD kWh/100km value", "bcvalue", strtobool(map["bcvalue"]),
    "<p>Show OBD kWh/100km</p>");
  c.input_slider("WLTP km", "full_km", 3, "km",-1, atof(map["full.km"].c_str()), 126, 100, 180, 1,
    "<p>Max range at full HV (126 WLTP, 155 NFEZ) for ideal range calc</p>");
  c.fieldset_end();

  c.fieldset_start("Diff Settings");
  c.input_checkbox("Online state LED", "led", strtobool(map["led"]),
    "<p>RED=no inet, BLUE=inet, GREEN=v2 connected. EGPIO 7,8,9</p>");
  c.input_checkbox("Door unlocked warning", "unlocked", strtobool(map["unlock.warning"]),
    "<p>Warn if parked &gt;10min and unlocked</p>");
  c.input_checkbox("Door open warning", "opendoors", strtobool(map["door.warning"]),
    "<p>Warn if parked &gt;10min and doors open</p>");
  c.input_checkbox("Extended statistics", "extstats", strtobool(map["extended.stats"]),
      "<p>Maintenance + trip data. Not recommended for iOS app!</p>");
  c.input_slider("Restart Network Time", "rebootnw", 3, "min",-1, atof(map["rebootnw"].c_str()), 30, 0, 60, 1,
    "<p>0=off. Auto-restart network on v2 disconnect</p>");
  c.input_slider("Contactor 1h limit", "contactor_1h_limit", 3, "/h",-1, atof(map["bms.contactor.1h.limit"].c_str()), 8, 1, 100, 1,
    "<p>Max contactor cycles per hour before alert (default: 8)</p>");
  c.fieldset_end();

  c.fieldset_start("OBDII Polling (requires CAN write)");
  c.input_checkbox("Enable 743 polling", "obdii743", strtobool(map["obdii.743"]),
    "<p>e.g. maintenance data</p>");
  c.input_checkbox("Enable 745 polling", "obdii745", strtobool(map["obdii.745"]),
    "<p>e.g. Doorlock</p>");
  c.input_checkbox("Enable 745 TPMS polling", "obdii745_tpms", strtobool(map["obdii.745.tpms"]),
    "<p>Full TPMS (pressure/temp/alert). Off=basic (pressure only)</p>");
  c.input_checkbox("Enable 79b polling", "obdii79b", strtobool(map["obdii.79b"]),
    "<p>e.g. HV battery state</p>");
  c.input_checkbox("Enable 79b cell polling", "obdii79b.cell", strtobool(map["obdii.79b.cell"]),
    "<p>BMS cell monitor (voltages/temps/resistances)</p>");
  c.input_checkbox("Enable 7e4 polling", "obdii7e4", strtobool(map["obdii.7e4"]),
    "<p>e.g. charging plug plugged in</p>");
  c.input_checkbox("Enable 7e4 DCDC polling", "obdii7e4_dcdc", strtobool(map["obdii.7e4.dcdc"]),
    "<p>DC-DC converter values</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}

/**
 * WebCfgTPMS: TPMS Configuration (URL /xsq/tpms)
 */
void OvmsVehicleSmartEQ::WebCfgTPMS(PageEntry_t& p, PageContext_t& c) {
  OvmsVehicleSmartEQ* sq = (OvmsVehicleSmartEQ*)MyVehicleFactory.ActiveVehicle();
  if (!sq) {
      c.head(400);
      c.alert("danger", "Error: smartEQ vehicle not available");
      c.done();
      return;
  }

  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("xsq");
  ConfigParamMap map_veh = MyConfig.GetParamMap("vehicle");
  std::string error;
  
  if (c.method == "POST") {
    // Process form submission
    map["tpms.temp"]          = (c.getvar("tpms.temp") == "yes") ? "yes" : "no";
    map["tpms.alert.enable"]  = (c.getvar("enable") == "yes") ? "yes" : "no";
    map_veh["tpms.fl"]        = c.getvar("TPMS_FL");
    map_veh["tpms.fr"]        = c.getvar("TPMS_FR");
    map_veh["tpms.rl"]        = c.getvar("TPMS_RL");
    map_veh["tpms.rr"]        = c.getvar("TPMS_RR");
    map["tpms.front.pressure"] = c.getvar("front_pressure");
    map["tpms.rear.pressure"]  = c.getvar("rear_pressure");
    map["tpms.value.warn"]     = c.getvar("pressure_warning");
    map["tpms.value.alert"]    = c.getvar("pressure_alert");

    if (error.empty()) {
      // Write all changes in one operation
      MyConfig.SetParamMap("xsq", map);
      MyConfig.SetParamMap("vehicle", map_veh);

      c.head(200);
      c.alert("success", "<p>TPMS settings updated successfully</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // Error response
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    if (map["tpms.temp"].empty())          map["tpms.temp"] = "yes";
    if (map["tpms.alert.enable"].empty())  map["tpms.alert.enable"] = "yes";
    if (map_veh["tpms.fl"].empty())        map_veh["tpms.fl"] = "0";
    if (map_veh["tpms.fr"].empty())        map_veh["tpms.fr"] = "1";
    if (map_veh["tpms.rl"].empty())        map_veh["tpms.rl"] = "2";
    if (map_veh["tpms.rr"].empty())        map_veh["tpms.rr"] = "3";
    if (map["tpms.front.pressure"].empty()) map["tpms.front.pressure"] = "225";
    if (map["tpms.rear.pressure"].empty())  map["tpms.rear.pressure"] = "255";
    if (map["tpms.value.warn"].empty())     map["tpms.value.warn"] = "40";
    if (map["tpms.value.alert"].empty())    map["tpms.value.alert"] = "70";
    c.head(200);
  }
      
  c.panel_start("primary", "TPMS Configuration");
  c.form_start(p.uri);

  // TPMS settings
  c.fieldset_start("TPMS Settings");
  c.input_checkbox("TPMS Temperature", "tpms.temp", strtobool(map["tpms.temp"]),
    "<p>Show tire temperatures</p>");
  c.input_checkbox("TPMS Alert", "enable", strtobool(map["tpms.alert.enable"]),
    "<p>Low/high pressure alerts</p>");
  c.input_slider("Front Tire Pressure", "front_pressure", 3, "kPa",-1, atof(map["tpms.front.pressure"].c_str()), 225, 170, 350, 5,
    "<p>Target front pressure</p>");
  c.input_slider("Rear Tire Pressure", "rear_pressure", 3, "kPa",-1, atof(map["tpms.rear.pressure"].c_str()), 255, 170, 350, 5,
    "<p>Target rear pressure</p>");
  c.input_slider("Pressure Warning", "pressure_warning", 3, "kPa",-1, atof(map["tpms.value.warn"].c_str()), 40, 10, 60, 5,
    "<p>Over/under pressure warning threshold</p>");
  c.input_slider("Pressure Alert", "pressure_alert", 3, "kPa",-1, atof(map["tpms.value.alert"].c_str()), 70, 30, 120, 5,
    "<p>Over/under pressure alert threshold</p>");
  {
    static const char* const pos_labels[] = {"Front Left Sensor","Front Right Sensor","Rear Left Sensor","Rear Right Sensor"};
    static const char* const pos_names[]  = {"TPMS_FL","TPMS_FR","TPMS_RL","TPMS_RR"};
    static const char* const cfg_keys[]   = {"tpms.fl","tpms.fr","tpms.rl","tpms.rr"};
    static const char* const opt_labels[] = {"Front_Left","Front_Right","Rear_Left","Rear_Right"};
    static const char* const opt_vals[]   = {"0","1","2","3"};
    for (int s = 0; s < 4; s++) {
      c.input_select_start(pos_labels[s], pos_names[s]);
      for (int i = 0; i < 4; i++)
        c.input_select_option(opt_labels[i], opt_vals[i], map_veh[cfg_keys[s]] == opt_vals[i]);
      c.input_select_end();
    }
  c.print("<p><strong>Note:</strong></p>"
    "<p>Smart TPMS sensors do not always stay in one fixed wheel position.</p>"
    "<p>They may be front/rear, left/right, or diagonally swapped depending on the vehicle and sensor ID.</p>"
    "<p>Please check the assignment and correct it if needed.</p>"
    "<p>After changing the TPMS sensor positions, please drive the vehicle for a few minutes to allow the system to update and reflect the new configuration.</p>"
    "<p>Ensure that the sensors are correctly assigned to their respective wheel positions to avoid any confusion or incorrect readings.</p>"
    "<p>Remember to check the tire pressures regularly and maintain them within the recommended range for optimal performance and safety.</p>");
  }
  c.fieldset_end();

  c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
  c.done();
}

/**
 * WebCfgADC: ADC Configuration (URL /xsq/adc)
 */
void OvmsVehicleSmartEQ::WebCfgADC(PageEntry_t& p, PageContext_t& c) {
  OvmsVehicleSmartEQ* sq = (OvmsVehicleSmartEQ*)MyVehicleFactory.ActiveVehicle();
  if (!sq) {
      c.head(400);
      c.alert("danger", "Error: smartEQ vehicle not available");
      c.done();
      return;
  }
  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("xsq");
  ConfigParamMap map_adc = MyConfig.GetParamMap("system.adc");
  std::string error, info, onboard_12v, adc_history;
  bool header_sent = false;
  bool is_calc_request = (p.uri == "/xsq/adc" && c.getvar("action") == "calc");

  if (c.method == "POST") {
    // Process form submission
    map["calc.adcfactor"] = (c.getvar("calcADCfactor") == "yes") ? "yes" : "no";
    map_adc["factor12v"]  = c.getvar("adc_factor");
    onboard_12v           = c.getvar("onboard_12v");
    if (is_calc_request) {
      std::string cmd = "xsq calcadc";
      if (!onboard_12v.empty()) {
        cmd += " ";
        cmd += onboard_12v;
      }
      std::string cmd_output = MyWebServer.ExecuteCommand(cmd);
      if (cmd_output.empty())
        cmd_output = "Command executed.";
      std::string alert_type = (cmd_output.rfind("Error", 0) == 0) ? "danger" : "success";
      info = "<p class=\"lead\">ADC calculation command output</p><pre>" +
        c.encode_html(cmd_output) + "</pre>";
      c.head(200);
      header_sent = true;
      c.alert(alert_type.c_str(), info.c_str());

      // Refresh maps/metrics to show latest state after command execution
      map = MyConfig.GetParamMap("xsq");
      map_adc = MyConfig.GetParamMap("system.adc");
      if (map_adc["factor12v"].empty())
        map_adc["factor12v"] = "195.7";
      if (sq->mt_evc_dcdc && StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f) >= 13.1f) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f));  // DCDC voltage
        onboard_12v = buf;
      }
      if (onboard_12v.empty())
        onboard_12v = "12.500";
      if (sq->mt_adc_factor_history)
        adc_history = sq->mt_adc_factor_history->AsString();
    }
    else if (error.empty()) {
      if (map_adc["factor12v"].empty())
        map_adc.erase("factor12v");
      MyConfig.SetParamMap("system.adc", map_adc);
      MyConfig.SetParamMap("xsq", map);

      // Success response
      info = "<p>ADC settings updated successfully</p>";
      c.head(200);
      header_sent = true;
      c.alert("success", info.c_str());
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }
    else {
      // Error response
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.head(400);
      header_sent = true;
      c.alert("danger", error.c_str());
    }
  } else {
    // Fill in defaults for GET request if empty:
    if (map_adc["factor12v"].empty())
      map_adc["factor12v"] = "195.7";
    if (sq->mt_evc_dcdc && StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f) >= 13.1f) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.3f", StdMetrics.ms_v_charge_12v_voltage->AsFloat(0.0f));  // DCDC voltage
      onboard_12v = buf;
    }
    if (onboard_12v.empty())
      onboard_12v = "12.500";
    if (sq->mt_adc_factor_history)
      adc_history = sq->mt_adc_factor_history->AsString();
    c.head(200);
    header_sent = true;
  }

  // in case of validation error keep submitted values visible:
  if (c.method == "POST") {
    if (map_adc["factor12v"].empty())
      map_adc["factor12v"] = "195.7";
    if (onboard_12v.empty())
      onboard_12v = "12.500";
  }

  if (adc_history.empty() && sq->mt_adc_factor_history)
    adc_history = sq->mt_adc_factor_history->AsString();

  if (!header_sent) {
    c.head(200);
    header_sent = true;
  }
      
  c.panel_start("primary", "ADC Configuration");
  c.form_start(p.uri);

  // ADC settings
  c.fieldset_start("ADC Settings");
  c.input_checkbox("Auto-recalculate ADC factor", "calcADCfactor", strtobool(map["calc.adcfactor"]),
      "<p>Recalculate ADC factor once per 12V charge by DC/DC with CAN 12V measurement value</p>");
  if (!adc_history.empty()) {
    c.input_text("ADC factor history", "adc_history", adc_history.c_str(), "no history",
      "<p>Last 5 values, most recent last</p>");
  }
  c.input_slider("ADC factor", "adc_factor", 5, "", -1, atof(map_adc["factor12v"].c_str()), 195.7, 160, 230, 0.1,
    "<p>12V ADC calibration factor (default 195.7)</p>");
  c.input_slider("12V measured", "onboard_12v", 3, "V",-1, atof(onboard_12v.c_str()), 12.50, 11.00, 15.00, 0.01,
    "<p>Your measured 12V for calibration calculation</p>");
  c.printf("<input type=\"hidden\" name=\"onboard_12v_submit\" id=\"onboard_12v_submit\" value=\"%s\">\n",
           _attr(onboard_12v.c_str()));
  c.input_button("default", "ADC calculation", "action", "calc");
  c.fieldset_end();

  c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
  c.done();
}

/**
 * WebCfgTrickle: configure 12V trickle charging (URL /xsq/trickle)
 */
void OvmsVehicleSmartEQ::WebCfgTrickle(PageEntry_t& p, PageContext_t& c) {
  OvmsVehicleSmartEQ* sq = (OvmsVehicleSmartEQ*)MyVehicleFactory.ActiveVehicle();
  if (!sq) {
    c.head(400);
    c.alert("danger", "Error: smartEQ vehicle not available");
    c.done();
    return;
  }

  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("xsq");
  ConfigParamMap map_veh = MyConfig.GetParamMap("vehicle");
  std::string error;

  bool is_reset_counter = (c.method == "POST" && c.getvar("action") == "reset_counter");
  bool is_start_trickle = (c.method == "POST" && c.getvar("action") == "start_trickle");

  if (is_reset_counter) {
    sq->m_12v_trickle_charge_times.clear();
    sq->mt_12v_trickle_charge_count->SetValue(0);
    if (map["12v.charge"].empty())
      map["12v.charge"] = "yes";
    sq->m_12v_charge = strtobool(map["12v.charge"]);
    c.head(200);
    c.alert("success", "<p class=\"lead\">24h trickle charge counter reset to 0.</p>");
  }
  else if (is_start_trickle) {
    if (!sq->IsCANwrite()) {
      c.head(400);
      c.alert("danger", "<p class=\"lead\">Failed to start trickle charge!</p><p>CAN write access is disabled. Enable CAN write in <a href=\"/xsq/features\">Features</a>.</p>");
    }
    else if (StdMetrics.ms_v_bat_soc->AsInt(0) < 31) {
      char buf[128];
      snprintf(buf, sizeof(buf), "<p class=\"lead\">Failed to start trickle charge!</p><p>HV battery SOC is too low (%d%% &le; 31%%).</p>", StdMetrics.ms_v_bat_soc->AsInt(0));
      c.head(400);
      c.alert("danger", buf);
    }
    else if (sq->IsOnHVACEQ()) {
      c.head(200);
      c.alert("info", "<p class=\"lead\">Pre-conditioning / Trickle charge is already active.</p>");
    }
    else {
      vehicle_command_t res = sq->CommandClimateControlEQ(true, false, sq->m_12v_trickle_time, true);
      if (res == Success) {
        c.head(200);
        c.alert("success", "<p class=\"lead\">12V trickle charge started successfully via preconditioning!</p>");
      } else {
        c.head(400);
        c.alert("danger", "<p class=\"lead\">Failed to start 12V trickle charge.</p>");
      }
    }
  }
  else if (c.method == "POST") {
    // Process form submission
    map["12v.charge"]          = (c.getvar("charge12v") == "yes") ? "yes" : "no";
    map["dcdc.log"]            = (c.getvar("dcdclog") == "yes") ? "yes" : "no";
    map["charge12v.threshold"]        = c.getvar("charge12v_threshold");
    map_veh["12v.ref"]                = c.getvar("ref12V");
    map["12v.trickle.time"]           = c.getvar("trickle_time");
    map["12v.trickle.park.days"]      = c.getvar("trickle_park_days");
    map["12v.trickle.park.time"]      = c.getvar("trickle_park_time");
    map["12v.trickle.park.duration"]  = c.getvar("trickle_park_duration");

    auto validFloat = [&](const std::string& s, double minv, double maxv, const char* fname)->bool {
      if (s.empty()) return false;
      char* end = nullptr;
      double v = strtod(s.c_str(), &end);
      if (end == s.c_str() || *end != '\0' || v < minv || v > maxv) {
        error += std::string("<li>Invalid ") + fname + "</li>";
        return false;
      }
      return true;
    };

    auto validInt = [&](const std::string& s, int minv, int maxv, const char* fname)->bool {
      if (s.empty()) return false;
      char* end = nullptr;
      long v = strtol(s.c_str(), &end, 10);
      if (end == s.c_str() || *end != '\0' || v < minv || v > maxv) {
        error += std::string("<li>Invalid ") + fname + "</li>";
        return false;
      }
      return true;
    };

    if (!validFloat(map["charge12v.threshold"], 11.25, 12.25, "12V trickle charge threshold")) map["charge12v.threshold"] = "11.75";
    if (!validFloat(map_veh["12v.ref"], 12.00, 13.50, "12V reference voltage")) map_veh["12v.ref"] = "12.50";
    if (!validFloat(map["12v.trickle.time"], 5, 30, "Trickle charge duration")) map["12v.trickle.time"] = "10";
    if (!validInt(map["12v.trickle.park.days"], 0, 60, "Park time trickle days")) map["12v.trickle.park.days"] = "0";
    if (!validInt(map["12v.trickle.park.duration"], 5, 30, "Park time trickle duration")) map["12v.trickle.park.duration"] = "10";

    int park_hour = 12, park_min = 0;
    {
      const std::string& pt = map["12v.trickle.park.time"];
      size_t colon = pt.find(':');
      bool time_ok = false;
      if (colon != std::string::npos && colon > 0 && colon + 1 < pt.size()) {
        std::string sh = pt.substr(0, colon);
        std::string sm = pt.substr(colon + 1);
        char* eh = nullptr;
        char* em = nullptr;
        long h = strtol(sh.c_str(), &eh, 10);
        long m = strtol(sm.c_str(), &em, 10);
        if (*eh == '\0' && *em == '\0' && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
          park_hour = (int)h;
          park_min = (int)m;
          time_ok = true;
        }
      }
      if (!time_ok) {
        if (atoi(map["12v.trickle.park.days"].c_str()) == 0 && pt.empty()) {
          map["12v.trickle.park.time"] = "12:00";
        } else {
          error += "<li>Invalid park trickle scheduled time (use HH:MM)</li>";
          map["12v.trickle.park.time"] = "12:00";
        }
      }
    }

    int trickle_time = atoi(map["12v.trickle.time"].c_str());
    trickle_time = ((trickle_time + 2) / 5) * 5;  // round to nearest 5/10/.. minutes
    if (trickle_time < 5) trickle_time = 5;
    if (trickle_time > 30) trickle_time = 30;

    int park_days = atoi(map["12v.trickle.park.days"].c_str());
    int park_duration = atoi(map["12v.trickle.park.duration"].c_str());
    park_duration = ((park_duration + 2) / 5) * 5;
    if (park_duration < 5) park_duration = 5;
    if (park_duration > 30) park_duration = 30;

    if (error.empty()) {
      char tbuf[16];
      snprintf(tbuf, sizeof(tbuf), "%d", trickle_time);
      map["12v.trickle.time"] = tbuf;
      snprintf(tbuf, sizeof(tbuf), "%d", park_days);
      map["12v.trickle.park.days"] = tbuf;
      snprintf(tbuf, sizeof(tbuf), "%02d:%02d", park_hour, park_min);
      map["12v.trickle.park.time"] = tbuf;
      snprintf(tbuf, sizeof(tbuf), "%d", park_duration);
      map["12v.trickle.park.duration"] = tbuf;

      sq->m_12v_charge = strtobool(map["12v.charge"]);
      sq->m_dcdc_log_enable = strtobool(map["dcdc.log"]);
      sq->m_12v_trickle_time = trickle_time;
      sq->m_12v_trickle_park_days = park_days;
      sq->m_12v_trickle_park_hour = park_hour;
      sq->m_12v_trickle_park_min = park_min;
      sq->m_12v_trickle_park_time = park_duration;

      float new_ref = atof(map_veh["12v.ref"].c_str());
      float new_thresh = atof(map["charge12v.threshold"].c_str());
      float new_alert = new_ref - new_thresh;
      if (new_alert < 0.1f) new_alert = 0.1f;

      snprintf(tbuf, sizeof(tbuf), "%.2f", new_ref);
      map_veh["12v.ref"] = tbuf;
      snprintf(tbuf, sizeof(tbuf), "%.2f", new_alert);
      map_veh["12v.alert"] = tbuf;

      MyConfig.SetParamMap("xsq", map);
      MyConfig.SetParamMap("vehicle", map_veh);
      sq->m_ref12V = new_ref;
      sq->m_alert12V = new_alert;

      c.head(200);
      c.alert("success", "<p class=\"lead\">12V trickle charge settings saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    c.head(200);
  }

  // Fill in defaults if empty (for GET or action POSTs)
  if (map["12v.charge"].empty())
    map["12v.charge"] = "yes";
  if (map["12v.trickle.time"].empty())
    map["12v.trickle.time"] = "10";
  if (map["12v.trickle.park.days"].empty())
    map["12v.trickle.park.days"] = "0";
  if (map["12v.trickle.park.time"].empty())
    map["12v.trickle.park.time"] = "12:00";
  if (map["12v.trickle.park.duration"].empty())
    map["12v.trickle.park.duration"] = "10";
  if (map_veh["12v.ref"].empty())
    map_veh["12v.ref"] = "12.50";
  if (map_veh["12v.alert"].empty())
    map_veh["12v.alert"] = "0.75";
  if (map["charge12v.threshold"].empty()) {
    char def[16];
    snprintf(def, sizeof(def), "%.2f", atof(map_veh["12v.ref"].c_str()) - atof(map_veh["12v.alert"].c_str()));
    map["charge12v.threshold"] = def;
  }

  c.panel_start("primary", "smart EQ 12V Trickle Charge");
  c.form_start(p.uri);

  // Status & Monitoring
  c.fieldset_start("Status & Monitoring");

  float cur_12v = StdMetrics.ms_v_bat_12v_voltage->AsFloat(0.0f);
  bool alert_active = StdMetrics.ms_v_bat_12v_voltage_alert->AsBool(false);
  bool trickle_active = (sq->m_climate_trickle || sq->Is12VchargeEQ());
  int count_24h = sq->mt_12v_trickle_charge_count ? sq->mt_12v_trickle_charge_count->AsInt(0) : 0;
  int hv_soc = StdMetrics.ms_v_bat_soc->AsInt(0);
  int contactor_1h = sq->mt_bms_contactor_cycles ? sq->mt_bms_contactor_cycles->GetElemValue(4) : 0;

  char status_buf[256];
  snprintf(status_buf, sizeof(status_buf), "%.2f V", cur_12v);
  c.input_info("12V Battery Voltage", status_buf);

  snprintf(status_buf, sizeof(status_buf), "%s",
    alert_active ? "<span class=\"label label-danger\">Alert (Low Voltage)</span>" : "<span class=\"label label-success\">Normal</span>");
  c.input_info("12V Alert State", status_buf);

  snprintf(status_buf, sizeof(status_buf), "%s",
    trickle_active ? "<span class=\"label label-info\">Active</span>" : "<span class=\"label label-default\">Inactive</span>");
  c.input_info("Trickle Charge State", status_buf);

  if (count_24h >= 3) {
    snprintf(status_buf, sizeof(status_buf), "<span class=\"label label-danger\">%d / 3</span> &nbsp; <span class=\"text-danger\">Locked out (deactivated to protect 12V battery)</span>", count_24h);
  } else {
    snprintf(status_buf, sizeof(status_buf), "%d / 3 activations in last 24h", count_24h);
  }
  c.input_info("24h Activation Count", status_buf);

  if (!sq->m_12v_trickle_charge_times.empty()) {
    std::string times_str;
    for (size_t i = 0; i < sq->m_12v_trickle_charge_times.size(); i++) {
      time_t t = sq->m_12v_trickle_charge_times[i];
      struct tm tm_buf;
      localtime_r(&t, &tm_buf);
      char tbuf[32];
      strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_buf);
      if (i > 0) times_str += "<br>";
      times_str += tbuf;
    }
    c.input_info("Activation Timestamps", times_str.c_str());
  }

  if (sq->m_trickle_voltage > 0.0f) {
    snprintf(status_buf, sizeof(status_buf), "%.2f V", sq->m_trickle_voltage);
    c.input_info("Last Activation Voltage", status_buf);
  }

  if (sq->mt_12v_undervolt_history && sq->mt_12v_undervolt_history->GetSize() > 0) {
    std::string hist_str;
    for (int i = 0; i < sq->mt_12v_undervolt_history->GetSize(); i++) {
      float v = sq->mt_12v_undervolt_history->GetElemValue(i);
      if (v > 0.0f) {
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "%.2f V", v);
        if (!hist_str.empty()) hist_str += ", ";
        hist_str += vbuf;
      }
    }
    if (!hist_str.empty()) {
      c.input_info("Undervoltage History", hist_str.c_str());
    }
  }

  snprintf(status_buf, sizeof(status_buf), "%d%% %s",
    hv_soc, hv_soc > 30 ? "<span class=\"label label-success\">OK (&gt; 30%)</span>" : "<span class=\"label label-warning\">Too low (&le; 30%)</span>");
  c.input_info("HV Battery SOC", status_buf);

  snprintf(status_buf, sizeof(status_buf), "%d / h", contactor_1h);
  c.input_info("HV Contactor Cycles (1h)", status_buf);

  snprintf(status_buf, sizeof(status_buf), "%s",
    sq->IsCANwrite() ? "<span class=\"label label-success\">Enabled</span>" : "<span class=\"label label-warning\">Disabled (CAN write required)</span>");
  c.input_info("CAN Write Mode", status_buf);

  uint32_t park_secs = (uint32_t)StdMetrics.ms_v_env_parktime->AsInt(0);
  snprintf(status_buf, sizeof(status_buf), "%ud %02u:%02u:%02u",
    (unsigned)(park_secs / 86400u),
    (unsigned)((park_secs % 86400u) / 3600u),
    (unsigned)((park_secs % 3600u) / 60u),
    (unsigned)(park_secs % 60u));
  c.input_info("Current Park Time", status_buf);

  if (sq->m_12v_trickle_park_days > 0) {
    snprintf(status_buf, sizeof(status_buf), "Every %d day(s) at %02d:%02d for %d min",
      sq->m_12v_trickle_park_days, sq->m_12v_trickle_park_hour, sq->m_12v_trickle_park_min, sq->m_12v_trickle_park_time);
    c.input_info("Park Trickle Schedule", status_buf);

    if (sq->m_12v_trickle_park_marker > 0) {
      uint32_t m_secs = sq->m_12v_trickle_park_marker;
      snprintf(status_buf, sizeof(status_buf), "%ud %02u:%02u:%02u",
        (unsigned)(m_secs / 86400u),
        (unsigned)((m_secs % 86400u) / 3600u),
        (unsigned)((m_secs % 3600u) / 60u),
        (unsigned)(m_secs % 60u));
    } else {
      snprintf(status_buf, sizeof(status_buf), "Not triggered yet");
    }
    c.input_info("Park Trickle Marker", status_buf);
  }

  c.fieldset_end();

  // Configuration
  c.fieldset_start("Configuration");

  c.input_radiobtn_start("Enable trickle 12V charging", "charge12v");
    c.input_radiobtn_option("charge12v", "Off", "no", !strtobool(map["12v.charge"]));
    c.input_radiobtn_option("charge12v", "On", "yes", strtobool(map["12v.charge"]));
  c.input_radiobtn_end(
    "<p>When 12V undervoltage is detected for more than 10 minutes while parked, preconditioning is activated for the configured duration to charge the 12V battery from the HV battery.</p>"
    "<p>Safety limit: If triggered 3 times within 24 hours, trickle charging is paused to prevent deep discharge / battery damage.</p>");

  c.input_slider("Trickle charge duration", "trickle_time", 3, "min", -1, atof(map["12v.trickle.time"].c_str()), 10, 5, 30, 5,
    "<p>Duration of preconditioning during 12V trickle charging (default: 10 min, range: 5…30 min in 5 min steps).</p>"
    "<p>Note: Preconditioning has a fixed runtime of 5 minutes, and it restarts depending on the set time!</p>" 
    "<p>This means the power contactors are switched every time it restarts!</p>");

  c.input_slider("12V Reference Voltage", "ref12V", 3, "V", -1, atof(map_veh["12v.ref"].c_str()), 12.50, 12.20, 12.80, 0.05,
    "<p>Reference voltage of the 12V battery (default: 12.50V). Sets <code>[vehicle] 12v.ref</code>.</p>");

  c.input_slider("Start 12V trickle charge at", "charge12v_threshold", 3, "V", -1, atof(map["charge12v.threshold"].c_str()), 11.70, 11.25, 12.25, 0.05,
    "<p>Voltage threshold to initiate 12V trickle charging (default: 11.70V).</p>"
    "<p>Sets <code>[vehicle] 12v.alert</code> to <i>Reference Voltage &minus; Start Threshold</i>.</p>");

  c.input_checkbox("Enable DCDC charging log", "dcdclog", strtobool(map["dcdc.log"]),
    "<p>Opt in to send a DCDC history log record (<code>XSQ-DCDC-charging</code>) every minute while 12V charging is active.</p>");

  c.fieldset_end();

  // Park Time Trickle Charge
  c.fieldset_start("Park Time Trickle Charge");

  c.input_slider("Trickle after park days", "trickle_park_days", 3, "days", -1, atoi(map["12v.trickle.park.days"].c_str()), 0, 0, 60, 1,
    "<p>Automatically activate 12V trickle charging after the configured number of parked days (0 = disabled, 1…60 days).</p>"
    "<p>When triggered, the current park time is remembered so the next cycle activates after another configured number of days. Turning the vehicle on resets this marker.</p>");

  c.input("time", "Scheduled time", "trickle_park_time", map["12v.trickle.park.time"].c_str(), "HH:MM",
    "<p>Local time of day (HH:MM) at which the park-time 12V trickle charge is activated once the configured park days have elapsed.</p>");

  int park_dur = atoi(map["12v.trickle.park.duration"].c_str());
  park_dur = ((park_dur + 2) / 5) * 5;
  if (park_dur < 5) park_dur = 5;
  if (park_dur > 30) park_dur = 30;
  c.input_radiobtn_start("Park trickle duration", "trickle_park_duration");
    c.input_radiobtn_option("trickle_park_duration", "5 min", "5", park_dur == 5);
    c.input_radiobtn_option("trickle_park_duration", "10 min", "10", park_dur == 10);
    c.input_radiobtn_option("trickle_park_duration", "15 min", "15", park_dur == 15);
    c.input_radiobtn_option("trickle_park_duration", "20 min", "20", park_dur == 20);
    c.input_radiobtn_option("trickle_park_duration", "25 min", "25", park_dur == 25);
    c.input_radiobtn_option("trickle_park_duration", "30 min", "30", park_dur == 30);
  c.input_radiobtn_end(
    "<p>Duration of preconditioning for park-time 12V trickle charging (5…30 min).</p>");

  c.fieldset_end();

  // Manual Actions
  c.fieldset_start("Manual Actions");
  c.input_button("warning", "Reset 24h Counter", "action", "reset_counter");
  c.input_button("info", "Start 12V trickle charge Now", "action", "start_trickle");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}

/**
 * WebCfgBattery: configure battery parameters (URL /xnl/battery)
 */
void OvmsVehicleSmartEQ::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("xsq");
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)

  if (c.method == "POST") {
    // process form submission:
    map["suffrange"] = c.getvar("suffrange");
    map["suffsoc"]   = c.getvar("suffsoc");

    // check:
    if (!map["suffrange"].empty()) {
      float n = atof(map["suffrange"].c_str());
      if (n < 0)
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
    }
    if (!map["suffsoc"].empty()) {
      float n = atof(map["suffsoc"].c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
    }

    if (error.empty()) {
      if (map["suffrange"].empty())
        map.erase("suffrange");
      if (map["suffsoc"].empty())
        map.erase("suffsoc");

      MyConfig.SetParamMap("xsq", map);

      c.head(200);
      c.alert("success", "<p class=\"lead\">smart EQ battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // Fill in defaults for GET request if empty:
    if (map["suffrange"].empty())
      map["suffrange"] = "0";
    if (map["suffsoc"].empty())
      map["suffsoc"] = "0";

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "smart EQ Battery Setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",-1, atof(map["suffrange"].c_str()), 75, 0, 150, 1,
    "<p>0=off. Notify/stop at this range</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",-1, atof(map["suffsoc"].c_str()), 80, 0, 100, 1,
    "<p>0=off. Notify/stop at this SOC</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * GetDashboardConfig: smart EQ specific dashboard setup
 */
void OvmsVehicleSmartEQ::GetDashboardConfig(DashboardConfig& cfg)
{
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 145, 5);
  speed_dash.AddBand("green", 0, 70);
  speed_dash.AddBand("yellow", 70, 100);
  speed_dash.AddBand("red", 100, 145);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(260, 400);
  voltage_dash.AddBand("red", 260, 305);
  voltage_dash.AddBand("yellow", 305, 355);
  voltage_dash.AddBand("green", 355, 400);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 300);
  eff_dash.AddBand("green", 0, 150);
  eff_dash.AddBand("yellow", 150, 250);
  eff_dash.AddBand("red", 250, 300);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-30, 30);
  power_dash.AddBand("violet", -30, 0);
  power_dash.AddBand("green", 0, 15);
  power_dash.AddBand("yellow", 15, 25);
  power_dash.AddBand("red", 25, 30);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ",Celcius);
  charget_dash.SetMinMax(20, 80);
  charget_dash.SetTick(20);
  charget_dash.AddBand("normal", 20, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ",Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 50);
  batteryt_dash.AddBand("red", 50, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ",Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ",Celcius);
  motort_dash.SetMinMax(50, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 50, 110);
  motort_dash.AddBand("red", 110, 125);

  std::ostringstream str;
  str << "yAxis: ["
      << speed_dash << "," // Speed
      << voltage_dash << "," // Voltage
      << soc_dash << "," // SOC
      << eff_dash << "," // Efficiency
      << power_dash << "," // Power
      << charget_dash << "," // Charger temperature
      << batteryt_dash << "," // Battery temperature
      << invertert_dash << "," // Inverter temperature
      << motort_dash // Motor temperature
      << "]";
  cfg.gaugeset1 = str.str();
}
#endif //CONFIG_OVMS_COMP_WEBSERVER
