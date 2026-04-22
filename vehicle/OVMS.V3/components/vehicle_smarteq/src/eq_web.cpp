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

  std::string error, full_km, rebootnw;
  bool canwrite, canwrite_caron, led, resettrip, resettotal, bcvalue;
  bool charge12v, extstats, unlocked, tripnotify, opendoors;
  bool obdii79b, obdii79b_cell, obdii743, obdii745, obdii745_tpms, obdii7e4, obdii7e4_dcdc;

  if (c.method == "POST") {
    // process form submission:
    canwrite    = (c.getvar("canwrite") == "yes");
    canwrite_caron = (c.getvar("canwrite_caron") == "yes");
    led         = (c.getvar("led") == "yes");
    rebootnw    = (c.getvar("rebootnw"));
    resettrip   = (c.getvar("resettrip") == "yes");
    resettotal  = (c.getvar("resettotal") == "yes");
    bcvalue     = (c.getvar("bcvalue") == "yes");
    full_km  =  (c.getvar("full_km"));
    charge12v = (c.getvar("charge12v") == "yes");
    unlocked = (c.getvar("unlocked") == "yes");
    extstats = (c.getvar("extstats") == "yes");
    tripnotify = (c.getvar("resetnotify") == "yes");
    opendoors = (c.getvar("opendoors") == "yes");
    obdii79b = (c.getvar("obdii79b") == "yes");
    obdii79b_cell = (c.getvar("obdii79b.cell") == "yes");
    obdii743 = (c.getvar("obdii743") == "yes");
    obdii745 = (c.getvar("obdii745") == "yes");
    obdii745_tpms = (c.getvar("obdii745_tpms") == "yes");
    obdii7e4 = (c.getvar("obdii7e4") == "yes");
    obdii7e4_dcdc = (c.getvar("obdii7e4_dcdc") == "yes");

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
    if(!validFloat(full_km, 50, 300, "WLTP km")) full_km = "126.0";
    if(!validFloat(rebootnw, 0, 1440, "Restart Network Time")) rebootnw = "0";

    if (error.empty()) {
      // Use GetParamMap() to get a COPY of the map
      auto map = MyConfig.GetParamMap("xsq");
      
      // Helper lambda to set bool values
      // Update all values in local map
      if(canwrite && canwrite_caron) 
         {
         // If canwrite/canwrite_caron changed, reset canwrite_caron to avoid inconsistent state
         canwrite_caron = false;
         }
      map.SetValueBool("canwrite", canwrite);
      map.SetValueBool("canwrite.caron", canwrite_caron);
      map.SetValueBool("led", led);
      map["rebootnw"] = rebootnw;
      map.SetValueBool("resettrip", resettrip);
      map.SetValueBool("resettotal", resettotal);
      map.SetValueBool("bcvalue", bcvalue);
      map["full.km"] = full_km;
      map.SetValueBool("12v.charge", charge12v);
      map.SetValueBool("unlock.warning", unlocked);
      map.SetValueBool("extended.stats", extstats);
      map.SetValueBool("reset.notify", tripnotify);
      map.SetValueBool("door.warning", opendoors);
      map.SetValueBool("obdii.79b", obdii79b);
      map.SetValueBool("obdii.79b.cell", obdii79b_cell);
      map.SetValueBool("obdii.743", obdii743);
      map.SetValueBool("obdii.745", obdii745);
      map.SetValueBool("obdii.7e4", obdii7e4);
      map.SetValueBool("obdii.7e4.dcdc", obdii7e4_dcdc);
      map.SetValueBool("obdii.745.tpms", obdii745_tpms);

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
    canwrite       = sq->m_enable_write;
    canwrite_caron = sq->m_enable_write_caron;
    led            = sq->m_enable_LED_state;
    resettrip      = sq->m_resettrip;
    resettotal     = sq->m_resettotal;
    bcvalue        = sq->m_bcvalue;
    charge12v      = sq->m_12v_charge;
    unlocked       = sq->m_enable_lock_state;
    extstats       = sq->m_extendedStats;
    tripnotify     = sq->m_tripnotify;
    opendoors      = sq->m_enable_door_state;
    obdii79b       = sq->m_obdii_79b;
    obdii79b_cell  = sq->m_obdii_79b_cell;
    obdii743       = sq->m_obdii_743;
    obdii745       = sq->m_obdii_745;
    obdii745_tpms  = sq->m_obdii_745_tpms;
    obdii7e4       = sq->m_obdii_7e4;
    obdii7e4_dcdc  = sq->m_obdii_7e4_dcdc;
    char def[16];
    snprintf(def, sizeof(def), "%d", sq->m_reboot_time);
    rebootnw       = def;
    snprintf(def, sizeof(def), "%d", sq->m_full_km);
    full_km        = def;
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "smart EQ features configuration");
  c.form_start(p.uri);
  
  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN write access", "canwrite", canwrite,
    "<p>Controls CAN write access (OBDII polling etc.)</p>");
  c.input_checkbox("Enable CAN write only when car on/charging", "canwrite_caron", canwrite_caron,
    "<p>Alternative to Canwrite; select one or neither!</p>");
  c.fieldset_end();
  
  // trip reset or OBD activation
  c.fieldset_start("Trip calculated or OBD kWh/100km");
  c.input_checkbox("Reset Trip when Charging", "resettrip", resettrip,
    "<p>On=reset on charge, Off=reset on drive</p>");
  c.input_checkbox("Reset kWh/100km on car on", "resettotal", resettotal,
    "<p>Auto-disabled after reset</p>");
  c.input_checkbox("Trip Reset Notification", "resetnotify", tripnotify,
    "<p>Notify with trip values on reset</p>");
  c.input_checkbox("OBD kWh/100km value", "bcvalue", bcvalue,
    "<p>Show OBD kWh/100km</p>");
  c.input_slider("WLTP km", "full_km", 3, "km",-1, atof(full_km.c_str()), 126, 100, 180, 1,
  "<p>Max range at full HV (126 WLTP, 155 NFEZ) for ideal range calc</p>");
  c.fieldset_end();

  c.fieldset_start("Diff Settings");
  c.input_checkbox("Online state LED", "led", led,
    "<p>RED=no inet, BLUE=inet, GREEN=v2 connected. EGPIO 7,8,9</p>");
  c.input_checkbox("Enable 12V charging", "charge12v", charge12v,
    "<p>Charge 12V on low-voltage alert</p>"); 
  c.input_checkbox("Door unlocked warning", "unlocked", unlocked,
    "<p>Warn if parked &gt;10min and unlocked</p>");
  c.input_checkbox("Door open warning", "opendoors", opendoors,
    "<p>Warn if parked &gt;10min and doors open</p>");
  c.input_checkbox("Extended statistics", "extstats", extstats,
      "<p>Maintenance + trip data. Not recommended for iOS app!</p>");  
  c.input_slider("Restart Network Time", "rebootnw", 3, "min",-1, atof(rebootnw.c_str()), 15, 0, 60, 1,
    "<p>0=off. Auto-restart network on v2 disconnect</p>");
  c.fieldset_end();

  c.fieldset_start("OBDII Polling (requires CAN write)");
  c.input_checkbox("Enable 743 polling", "obdii743", obdii743,
    "<p>e.g. maintenance data</p>");
  c.input_checkbox("Enable 745 polling", "obdii745", obdii745,
    "<p>e.g. VIN/Doorlock</p>");
  c.input_checkbox("Enable 745 TPMS polling", "obdii745_tpms", obdii745_tpms,
    "<p>Full TPMS (pressure/temp/alert). Off=basic (pressure only)</p>");
  c.input_checkbox("Enable 79b polling", "obdii79b", obdii79b,
    "<p>e.g. HV battery state</p>");
  c.input_checkbox("Enable 79b cell polling", "obdii79b.cell", obdii79b_cell,
    "<p>BMS cell monitor (voltages/temps/resistances)</p>");
  c.input_checkbox("Enable 7e4 polling", "obdii7e4", obdii7e4,
    "<p>e.g. charging plug plugged in</p>");
  c.input_checkbox("Enable 7e4 DCDC polling", "obdii7e4_dcdc", obdii7e4_dcdc,
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

  std::string error, TPMS_FL, TPMS_FR, TPMS_RL, TPMS_RR, front_pressure, rear_pressure, pressure_warning, pressure_alert;
  bool tpms_temp, enable;
  
  if (c.method == "POST") {
    // Process form submission
    tpms_temp      = (c.getvar("tpms.temp") == "yes");
    enable         = (c.getvar("enable") == "yes");
    TPMS_FL        = c.getvar("TPMS_FL");
    TPMS_FR        = c.getvar("TPMS_FR");
    TPMS_RL        = c.getvar("TPMS_RL");
    TPMS_RR        = c.getvar("TPMS_RR");
    front_pressure = c.getvar("front_pressure");
    rear_pressure  = c.getvar("rear_pressure");
    pressure_warning = c.getvar("pressure_warning");
    pressure_alert   = c.getvar("pressure_alert");

    if (error.empty()) {
      // Use GetParamMap() to get a COPY of the map
      auto map = MyConfig.GetParamMap("xsq");
      auto map_veh = MyConfig.GetParamMap("vehicle");
      
      // Update all values in local map
      map["tpms.temp"] = tpms_temp ? "yes" : "no";
      map["tpms.alert.enable"] = enable ? "yes" : "no";
      map_veh["tpms.fl"] = TPMS_FL;
      map_veh["tpms.fr"] = TPMS_FR;
      map_veh["tpms.rl"] = TPMS_RL;
      map_veh["tpms.rr"] = TPMS_RR;
      map["tpms.front.pressure"] = front_pressure;
      map["tpms.rear.pressure"] = rear_pressure;
      map["tpms.value.warn"] = pressure_warning;
      map["tpms.value.alert"] = pressure_alert;
      
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
    tpms_temp      = sq->m_tpms_temp_enable;
    enable         = sq->m_tpms_alert_enable;
    char def[16];
    snprintf(def, sizeof(def), "%d", sq->m_tpms_index[0]);  TPMS_FL = def;
    snprintf(def, sizeof(def), "%d", sq->m_tpms_index[1]);  TPMS_FR = def;
    snprintf(def, sizeof(def), "%d", sq->m_tpms_index[2]);  TPMS_RL = def;
    snprintf(def, sizeof(def), "%d", sq->m_tpms_index[3]);  TPMS_RR = def;
    snprintf(def, sizeof(def), "%.0f", sq->m_front_pressure);   front_pressure = def;
    snprintf(def, sizeof(def), "%.0f", sq->m_rear_pressure);    rear_pressure = def;
    snprintf(def, sizeof(def), "%.0f", sq->m_pressure_warning); pressure_warning = def;
    snprintf(def, sizeof(def), "%.0f", sq->m_pressure_alert);   pressure_alert = def;
  }
      
  c.head(200);
  c.panel_start("primary", "TPMS Configuration");
  c.form_start(p.uri);

  // TPMS settings
  c.fieldset_start("TPMS Settings");
  c.input_checkbox("TPMS Temperature", "tpms.temp", tpms_temp,
    "<p>Show tire temperatures</p>");
  c.input_checkbox("TPMS Alert", "enable", enable,
    "<p>Low/high pressure alerts</p>");
  c.input_slider("Front Tire Pressure", "front_pressure", 3, "kPa",-1, atof(front_pressure.c_str()), 225, 170, 350, 5,
    "<p>Target front pressure</p>");
  c.input_slider("Rear Tire Pressure", "rear_pressure", 3, "kPa",-1, atof(rear_pressure.c_str()), 255, 170, 350, 5,
    "<p>Target rear pressure</p>");
  c.input_slider("Pressure Warning", "pressure_warning", 3, "kPa",-1, atof(pressure_warning.c_str()), 40, 10, 60, 5,
    "<p>Over/under pressure warning threshold</p>");
  c.input_slider("Pressure Alert", "pressure_alert", 3, "kPa",-1, atof(pressure_alert.c_str()), 70, 30, 120, 5,
    "<p>Over/under pressure alert threshold</p>");
  {
    static const char* const pos_labels[] = {"Front Left Sensor","Front Right Sensor","Rear Left Sensor","Rear Right Sensor"};
    static const char* const pos_names[]  = {"TPMS_FL","TPMS_FR","TPMS_RL","TPMS_RR"};
    static const char* const opt_labels[] = {"Front_Left","Front_Right","Rear_Left","Rear_Right"};
    static const char* const opt_vals[]   = {"0","1","2","3"};
    const std::string* tpms_vals[] = {&TPMS_FL, &TPMS_FR, &TPMS_RL, &TPMS_RR};
    for (int s = 0; s < 4; s++) {
      c.input_select_start(pos_labels[s], pos_names[s]);
      for (int i = 0; i < 4; i++)
        c.input_select_option(opt_labels[i], opt_vals[i], *tpms_vals[s] == opt_vals[i]);
      c.input_select_end();
    }
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
  std::string error, info, adc_factor, onboard_12v, adc_history;
  bool calcADCfactor;
  bool header_sent = false;
  bool is_calc_request = (p.uri == "/xsq/adc" && c.getvar("action") == "calc");

  if (c.method == "POST") {
    // Process form submission
    calcADCfactor = (c.getvar("calcADCfactor") == "yes");
    adc_factor  = c.getvar("adc_factor");
    onboard_12v  = c.getvar("onboard_12v");
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

      // Refresh values from config/metrics to show latest state after command execution
    calcADCfactor = sq->m_enable_calcADCfactor;
      adc_factor  = MyConfig.GetParamValue("system.adc", "factor12v", adc_factor.empty() ? "195.7" : adc_factor);
      if (sq->mt_evc_dcdc && sq->mt_evc_dcdc->GetElemValue(1) > 10.0f) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", sq->mt_evc_dcdc->GetElemValue(1));  // DCDC voltage
        onboard_12v = buf;
      }
      if (onboard_12v.empty())
        onboard_12v = "12.600";
      if (sq->mt_adc_factor_history)
        adc_history = sq->mt_adc_factor_history->AsString();
    }
    else if (error.empty()) {
      MyConfig.SetParamValue("system.adc", "factor12v", adc_factor);
      MyConfig.SetParamValueBool("xsq", "calc.adcfactor", calcADCfactor);

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
    // read configuration:
    calcADCfactor = sq->m_enable_calcADCfactor;
    adc_factor  = MyConfig.GetParamValue("system.adc", "factor12v", "195.7");
    if (sq->mt_evc_dcdc && sq->mt_evc_dcdc->GetElemValue(1) > 10.0f) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", sq->mt_evc_dcdc->GetElemValue(1));  // DCDC voltage
        onboard_12v = buf;
      }
    if (onboard_12v.empty())
      onboard_12v = "12.600";
    if (sq->mt_adc_factor_history)
      adc_history = sq->mt_adc_factor_history->AsString();
    c.head(200);
    header_sent = true;
  }

  // in case of validation error keep submitted values visible:
  if (c.method == "POST") {
    if (adc_factor.empty())
      adc_factor = "195.7";
    if (onboard_12v.empty())
      onboard_12v = "12.600";
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
  c.input_checkbox("Auto-recalculate ADC factor", "calcADCfactor", calcADCfactor,
      "<p>Recalculate ADC factor once per 12V charge by DC/DC with CAN 12V measurement value</p>");
  if (!adc_history.empty()) {
    c.input_text("ADC factor history", "adc_history", adc_history.c_str(), "no history",
      "<p>Last 5 values, most recent last</p>");
  }
  c.input_slider("ADC factor", "adc_factor", 5, "", -1, atof(adc_factor.c_str()), 195.7, 160, 230, 0.1,
    "<p>12V ADC calibration factor (default 195.7)</p>");
  c.input_slider("12V measured", "onboard_12v", 3, "V",-1, atof(onboard_12v.c_str()), 12.60, 11.00, 15.00, 0.01,
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
 * WebCfgBattery: configure battery parameters (URL /xnl/battery)
 */
void OvmsVehicleSmartEQ::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  std::string suffrange, suffsoc, cell_interval_drv, cell_interval_chg;

  if (c.method == "POST") {
    // process form submission:
    suffrange = c.getvar("suffrange");
    suffsoc   = c.getvar("suffsoc");
    cell_interval_drv = c.getvar("cell_interval_drv");
    cell_interval_chg = c.getvar("cell_interval_chg");

    // check:
    if (!suffrange.empty()) {
      float n = atof(suffrange.c_str());
      if (n < 0)
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
    }
    if (!suffsoc.empty()) {
      float n = atof(suffsoc.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
    }

    if (error == "") {
      // store:
      auto map = MyConfig.GetParamMap("xsq");
      map["suffrange"] = suffrange;
      map["suffsoc"] = suffsoc;
      map["cell_interval_drv"] = cell_interval_drv;
      map["cell_interval_chg"] = cell_interval_chg;
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
    // read configuration:
    suffrange = MyConfig.GetParamValue("xsq", "suffrange", "0");
    suffsoc   = MyConfig.GetParamValue("xsq", "suffsoc", "0");
    cell_interval_drv = MyConfig.GetParamValue("xsq", "cell_interval_drv", "60");
    cell_interval_chg = MyConfig.GetParamValue("xsq", "cell_interval_chg", "60");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "smart EQ Battery Setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",-1, atof(suffrange.c_str()), 75, 0, 150, 1,
    "<p>0=off. Notify/stop at this range</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",-1, atof(suffsoc.c_str()), 80, 0, 100, 1,
    "<p>0=off. Notify/stop at this SOC</p>");

  c.fieldset_end();
  
  c.fieldset_start("BMS Cell Monitoring");
  c.input_slider("Update interval driving", "cell_interval_drv", 3, "s",-1, atof(cell_interval_drv.c_str()),
    60, 0, 300, 1,
    "<p>Default 60s, 0=off</p>");
  c.input_slider("Update interval charging", "cell_interval_chg", 3, "s",-1, atof(cell_interval_chg.c_str()),
    60, 0, 300, 1,
    "<p>Default 60s, 0=off</p>");
  
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
