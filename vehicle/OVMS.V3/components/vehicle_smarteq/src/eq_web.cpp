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
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleSmartEQ::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xsq/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xsq/climate", "Climate/Heater", WebCfgClimate, PageMenu_Vehicle, PageAuth_Cookie);
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
  MyWebServer.DeregisterPage("/xsq/climate");
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
  std::string error, info, full_km, rebootnw;
  bool canwrite, led, resettrip, resettotal, bcvalue, climate_system;
  bool charge12v, extstats, unlocked, mdmcheck, wakeup, tripnotify, opendoors;

  if (c.method == "POST") {
    // process form submission:
    canwrite    = (c.getvar("canwrite") == "yes");
    led         = (c.getvar("led") == "yes");
    rebootnw    = (c.getvar("rebootnw"));
    resettrip   = (c.getvar("resettrip") == "yes");
    resettotal  = (c.getvar("resettotal") == "yes");
    bcvalue     = (c.getvar("bcvalue") == "yes");
    full_km  =  (c.getvar("full_km"));
    climate_system = (c.getvar("climate") == "yes");
    charge12v = (c.getvar("charge12v") == "yes");
    mdmcheck = (c.getvar("mdmcheck") == "yes");
    unlocked = (c.getvar("unlocked") == "yes");
    extstats = (c.getvar("extstats") == "yes");
    wakeup   = (c.getvar("wakeup") == "yes");
    tripnotify = (c.getvar("resetnotify") == "yes");
    opendoors = (c.getvar("opendoors") == "yes");

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
    if(!validFloat(full_km, 50, 300, "WLTP km")) full_km = "126";
    if(!validFloat(rebootnw, 0, 1440, "Restart Network Time")) rebootnw = "0";

    if (error.empty()) {
      // success:
      MyConfig.SetParamValueBool("xsq", "canwrite", canwrite);
      MyConfig.SetParamValueBool("xsq", "led", led);
      MyConfig.SetParamValue("xsq", "rebootnw", rebootnw);
      MyConfig.SetParamValueBool("xsq", "resettrip", resettrip);
      MyConfig.SetParamValueBool("xsq", "resettotal", resettotal);
      MyConfig.SetParamValueBool("xsq", "bcvalue", bcvalue);
      MyConfig.SetParamValue("xsq", "full.km", full_km);
      MyConfig.SetParamValueBool("xsq", "climate.system", climate_system);
      MyConfig.SetParamValueBool("xsq", "12v.charge", charge12v);
      MyConfig.SetParamValueBool("xsq", "unlock.warning", unlocked);
      MyConfig.SetParamValueBool("xsq", "modem.check", mdmcheck);
      MyConfig.SetParamValueBool("xsq", "extended.stats", extstats);
      MyConfig.SetParamValueBool("xsq", "restart.wakeup", wakeup);
      MyConfig.SetParamValueBool("xsq", "reset.notify", tripnotify);
      MyConfig.SetParamValueBool("xsq", "door.warning", opendoors);

      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      c.head(200);
      c.alert("success", info.c_str());
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    // read configuration:
    canwrite    = MyConfig.GetParamValueBool("xsq", "canwrite", false);
    led         = MyConfig.GetParamValueBool("xsq", "led", false);
    rebootnw    = MyConfig.GetParamValue("xsq", "rebootnw", "0");
    resettrip   = MyConfig.GetParamValueBool("xsq", "resettrip", false);
    resettotal  = MyConfig.GetParamValueBool("xsq", "resettotal", false);
    bcvalue     = MyConfig.GetParamValueBool("xsq", "bcvalue", false);
    full_km     = MyConfig.GetParamValue("xsq", "full.km", "126");
    climate_system     = MyConfig.GetParamValueBool("xsq", "climate.system", false);
    charge12v   = MyConfig.GetParamValueBool("xsq", "12v.charge", false);
    unlocked    = MyConfig.GetParamValueBool("xsq", "unlock.warning", false);
    mdmcheck    = MyConfig.GetParamValueBool("xsq", "modem.check", false);
    extstats    = MyConfig.GetParamValueBool("xsq", "extended.stats", false);
    wakeup      = MyConfig.GetParamValueBool("xsq", "restart.wakeup", false);
    tripnotify  = MyConfig.GetParamValueBool("xsq", "reset.notify", false);
    opendoors   = MyConfig.GetParamValueBool("xsq", "door.warning", false);

    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "smart EQ features configuration");
  c.form_start(p.uri);
  
  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN write(Poll)", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  c.fieldset_end();
  
  // trip reset or OBD activation
  c.fieldset_start("Trip calculated or OBD kWh/100km");
  c.input_checkbox("Enable Reset Trip when Charging", "resettrip", resettrip,
    "<p>Enable = Reset Trip Values when Charging, Disable = Reset Trip Values when Driving</p>");
  c.input_checkbox("Enable reset kWh/100km when Car switched on", "resettotal", resettotal,
    "<p>Enable = Reset calculated kWh/100km values on when Car switched on, auto disabled when resetted</p>");
  c.input_checkbox("Enable Trip Reset Notification", "resetnotify", tripnotify,
    "<p>Enable = send a notification with Trip values when Trip values are reseted</p>");
  c.input_checkbox("Enable OBD kWh/100km value", "bcvalue", bcvalue,
    "<p>Enable = show OBD kWh/100km value</p>");
  c.input_slider("WLTP km", "full_km", 3, "km",-1, atof(full_km.c_str()), 126, 100, 180, 1,
  "<p>set default max Range (126km WLTP, 155km NFEZ) at full charged HV for calculate ideal Range</p>");
  c.fieldset_end();

  c.fieldset_start("Diff Settings");
  c.input_checkbox("Enable/Disable Online state LED when installed", "led", led,
    "<p>RED=Internet no, BLUE=Internet yes, GREEN=Server v2 connected.<br>EGPIO Port 7,8,9 are used</p>");
  c.input_checkbox("Enable Climate System", "climate_system", climate_system,
    "<p>Enable = Climate/Heater system and data transfer to Android App activated</p>");
  c.input_checkbox("Enable 12V charging", "charge12v", charge12v,
    "<p>Enable = charge the 12V if low 12V alert is raised</p>");
  c.input_checkbox("Enable auto restart modem on Wifi disconnect", "mdmcheck", mdmcheck,
    "<p>Enable = The modem will restart as soon as the Wifi connection is no longer established.</p>");  
  c.input_checkbox("Enable Door unlocked warning", "unlocked", unlocked,
    "<p>Enable = send a warning when Car 10 minutes parked and unlocked</p>");
  c.input_checkbox("Enable Door open warning", "opendoors", opendoors,
    "<p>Enable = send a warning when Car 10 minutes parked and doors are open</p>");
  c.input_checkbox("Enable Wakeup on Restart", "wakeup", wakeup,
    "<p>Enable = Wakeup the Car on Restart of the OVMS</p>");
  c.input_checkbox("Enable extended statistics", "extstats", extstats,
      "<p>Enable = Show extended statistics incl. maintenance and trip data. Not recomment for iOS Open Vehicle App!</p>");  
  c.input_slider("Restart Network Time", "rebootnw", 3, "min",-1, atof(rebootnw.c_str()), 15, 0, 60, 1,
    "<p>Default 0 = off. Restart Network automatic when no v2Server connection.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}


/**
 * WebCfgClimate: Time based Climate/Heater start timer (URL /xsq/climate)
 */
void OvmsVehicleSmartEQ::WebCfgClimate(PageEntry_t& p, PageContext_t& c) {
  OvmsVehicleSmartEQ* sq = (OvmsVehicleSmartEQ*)MyVehicleFactory.ActiveVehicle();
  if (!sq) {
      c.head(400);
      c.alert("danger", "Error: smartEQ vehicle not available");
      c.done();
      return;
  }

  std::string error, info, climate_time, climate_ds, climate_de, climate_1to3;
  bool climate_on, climate_weekly, climate_notify, climate_system;
  int climate_on_int, climate_weekly_int, climate_time_int, climate_ds_int, climate_de_int, climate_1to3_int;
  
  if (c.method == "POST") {
    // Process form submission
    climate_on = (c.getvar("climate_on") == "yes");
    climate_weekly = (c.getvar("climate_weekly") == "yes");
    climate_time = c.getvar("climate_time");
    climate_ds = c.getvar("climate_ds");
    climate_de = c.getvar("climate_de");
    climate_1to3 = c.getvar("climate_1to3");
    climate_notify = (c.getvar("climate_notify") == "yes");
    climate_system = (c.getvar("climate_system") == "yes");

    // Input validation
    if (climate_time.empty()) {
      error += "<li>Time must be specified</li>";
    }
    if (climate_time.size()>0) {
      int t = atoi(climate_time.c_str());
      if (t<0 || t>2359 || (t%100)>59)
        error += "<li>Invalid time (HHMM)</li>";
    }

    // Convert values
    climate_on_int = climate_on ? 1 : 2;
    climate_weekly_int = climate_weekly ? 1 : 2;
    if(climate_on_int == 2){
      climate_weekly_int = 2;
    }
    climate_time_int = atoi(climate_time.c_str());
    climate_ds_int = atoi(climate_ds.c_str());
    climate_de_int = atoi(climate_de.c_str());
    climate_1to3_int = 0;

    // Convert climate_1to3
    if (atoi(climate_1to3.c_str()) == 5) climate_1to3_int = 0;
    else if (atoi(climate_1to3.c_str()) == 10) climate_1to3_int = 1;
    else if (atoi(climate_1to3.c_str()) == 15) climate_1to3_int = 2;
    else {
      error += "<li>Invalid climate duration</li>";
    }

    if (error.empty()) {
      // Format data string
      char buf[32];
      snprintf(buf, sizeof(buf), "1,%d,%d,%04d,%d,%d,%d",
        climate_on_int, climate_weekly_int, climate_time_int,
        climate_ds_int, climate_de_int, climate_1to3_int);

      // Save configuration
      sq->mt_climate_data->SetValue(std::string(buf));
      MyConfig.SetParamValueBool("xsq", "climate.notify", climate_notify);
      MyConfig.SetParamValueBool("xsq", "climate.system", climate_system);
      
      // Success response
      info = "<p>Climate control settings updated successfully</p>";
      c.head(200);
      c.alert("success", info.c_str());
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // Error response
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
  // read configuration using vehicle instance:
  climate_time = sq->mt_climate_time->AsString();
  climate_ds = sq->mt_climate_ds->AsString();
  climate_de = sq->mt_climate_de->AsString();
  climate_1to3 = sq->mt_climate_1to3->AsString();
  climate_on = sq->mt_climate_on->AsBool();
  climate_weekly = sq->mt_climate_weekly->AsBool();
  climate_notify = MyConfig.GetParamValueBool("xsq", "climate.notify", false);
  climate_system = MyConfig.GetParamValueBool("xsq", "climate.system", false);
      
  c.head(200);
  c.panel_start("primary", "Climate Control Settings");
  c.form_start(p.uri);

  c.fieldset_start("Climate/Heater start timer");
  c.input_checkbox("Enable Climate/Heater system", "climate_system", climate_system,
    "<p>Enable = Climate/Heater system and data transfer to Android App activated</p>");
  c.input_checkbox("Enable Climate/Heater at time", "climate_on", climate_on,
    "<p>Enable = start Climate/Heater at time</p>");
  c.input_checkbox("Enable Climate/Heater change notification", "climate_notify", climate_notify,
    "<p>Enable = sends a notification after a change in the set data</p>");  
  c.input_slider("Enable two time activation Climate/Heater", "climate_1to3", 3, "min",-1, atof(climate_1to3.c_str()), 5, 5, 15, 5,
    "<p>Enable = this option start Climate/Heater for 5-15 minutes</p>");
  c.input_text("time", "climate_time", climate_time.c_str(), "515","<p>Time: 5:15 = 515 or 15:30 = 1530</p>");

  c.input_checkbox("Enable auto timer activation Climate/Heater", "climate_weekly", climate_weekly,
    "<p>Enable = this option de/activate Climate/Heater at on/off day</p>");
  c.input_select_start("Select Auto on Day", "climate_ds");
  c.input_select_option("Sunday", "0", climate_ds == "0");
  c.input_select_option("Monday", "1", climate_ds == "1");
  c.input_select_option("Tuesday", "2", climate_ds == "2");
  c.input_select_option("Wednesday", "3", climate_ds == "3");
  c.input_select_option("Thursday", "4", climate_ds == "4");
  c.input_select_option("Friday", "5", climate_ds == "5");
  c.input_select_option("Saturday", "6", climate_ds == "6");
  c.input_select_end();

  c.input_select_start("Select Auto off Day", "climate_de");
  c.input_select_option("Sunday", "0", climate_de == "0");
  c.input_select_option("Monday", "1", climate_de == "1");
  c.input_select_option("Tuesday", "2", climate_de == "2");
  c.input_select_option("Wednesday", "3", climate_de == "3");
  c.input_select_option("Thursday", "4", climate_de == "4");
  c.input_select_option("Friday", "5", climate_de == "5");
  c.input_select_option("Saturday", "6", climate_de == "6");
  c.input_select_end();
  c.fieldset_end();

  c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
  }
    
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

  std::string error, info, TPMS_FL, TPMS_FR, TPMS_RL, TPMS_RR, front_pressure, rear_pressure, pressure_warning, pressure_alert;
  bool tpms_temp, enable;
  
  if (c.method == "POST") {
    // Process form submission
    tpms_temp      = (c.getvar("tpms.temp") == "yes");
    enable      = (c.getvar("enable") == "yes");
    TPMS_FL     = c.getvar("TPMS_FL");
    TPMS_FR     = c.getvar("TPMS_FR");
    TPMS_RL     = c.getvar("TPMS_RL");
    TPMS_RR     = c.getvar("TPMS_RR");
    front_pressure = c.getvar("front_pressure");
    rear_pressure  = c.getvar("rear_pressure");
    pressure_warning = c.getvar("pressure_warning");
    pressure_alert   = c.getvar("pressure_alert");

    if (error.empty()) {
      MyConfig.SetParamValueBool("xsq", "tpms.temp", tpms_temp);
      MyConfig.SetParamValueBool("xsq", "tpms.alert.enable", enable);
      MyConfig.SetParamValue("xsq", "TPMS_FL", TPMS_FL);
      MyConfig.SetParamValue("xsq", "TPMS_FR", TPMS_FR);
      MyConfig.SetParamValue("xsq", "TPMS_RL", TPMS_RL);
      MyConfig.SetParamValue("xsq", "TPMS_RR", TPMS_RR);
      MyConfig.SetParamValue("xsq", "tpms.front.pressure", front_pressure);
      MyConfig.SetParamValue("xsq", "tpms.rear.pressure", rear_pressure);
      MyConfig.SetParamValue("xsq", "tpms.value.warn", pressure_warning);
      MyConfig.SetParamValue("xsq", "tpms.value.alert", pressure_alert);
        
      // Success response
      info = "<p>TPMS settings updated successfully</p>";
      c.head(200);
      c.alert("success", info.c_str());
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // Error response
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    // read configuration:
    tpms_temp   = MyConfig.GetParamValueBool("xsq", "tpms.temp", false);
    enable      = MyConfig.GetParamValueBool("xsq", "tpms.alert.enable", true);
    TPMS_FL     = MyConfig.GetParamValue("xsq", "TPMS_FL", "0");
    TPMS_FR     = MyConfig.GetParamValue("xsq", "TPMS_FR", "1");
    TPMS_RL     = MyConfig.GetParamValue("xsq", "TPMS_RL", "2");
    TPMS_RR     = MyConfig.GetParamValue("xsq", "TPMS_RR", "3");
    front_pressure = MyConfig.GetParamValue("xsq", "tpms.front.pressure", "220"); // kPa
    rear_pressure  = MyConfig.GetParamValue("xsq", "tpms.rear.pressure", "250"); // kPa
    pressure_warning = MyConfig.GetParamValue("xsq", "tpms.value.warn", "25"); // kPa
    pressure_alert   = MyConfig.GetParamValue("xsq", "tpms.value.alert", "45"); // kPa
  }
      
  c.head(200);
  c.panel_start("primary", "TPMS Configuration");
  c.form_start(p.uri);

  // TPMS settings
  c.fieldset_start("TPMS Settings");
  c.input_checkbox("Enable TPMS Temperature", "tpms.temp", tpms_temp,
    "<p>enable TPMS Tire Temperatures display</p>");
  c.input_checkbox("Enable TPMS Alert", "enable", enable,
    "<p>enable TPMS Tire Pressures low/high alert</p>");
  c.input_slider("Front Tire Pressure", "front_pressure", 3, "kPa",-1, atof(front_pressure.c_str()), 220, 170, 350, 5,
    "<p>set Front Tire Pressure value</p>");
  c.input_slider("Rear Tire Pressure", "rear_pressure", 3, "kPa",-1, atof(rear_pressure.c_str()), 250, 170, 350, 5,
    "<p>set Rear Tire Pressure value</p>");
  c.input_slider("Pressure Warning", "pressure_warning", 3, "kPa",-1, atof(pressure_warning.c_str()), 25, 10, 60, 5,
    "<p>set under/over Pressure Warning value</p>");
  c.input_slider("Pressure Alert", "pressure_alert", 3, "kPa",-1, atof(pressure_alert.c_str()), 45, 30, 120, 5,
    "<p>set under/over Pressure Alert value</p>");
  c.input_select_start("Front Left Sensor", "TPMS_FL");
  c.input_select_option("Front_Left",  "0", TPMS_FL == "0");
  c.input_select_option("Front_Right", "1", TPMS_FL == "1");
  c.input_select_option("Rear_Left",   "2", TPMS_FL == "2");
  c.input_select_option("Rear_Right",  "3", TPMS_FL == "3");
  c.input_select_end();

  c.input_select_start("Front Right Sensor", "TPMS_FR");
  c.input_select_option("Front_Left",  "0", TPMS_FR == "0");
  c.input_select_option("Front_Right", "1", TPMS_FR == "1");
  c.input_select_option("Rear_Left",   "2", TPMS_FR == "2");
  c.input_select_option("Rear_Right",  "3", TPMS_FR == "3");
  c.input_select_end();

  c.input_select_start("Rear Left Sensor", "TPMS_RL");
  c.input_select_option("Front_Left",  "0", TPMS_RL == "0");
  c.input_select_option("Front_Right", "1", TPMS_RL == "1");
  c.input_select_option("Rear_Left",   "2", TPMS_RL == "2");
  c.input_select_option("Rear_Right",  "3", TPMS_RL == "3");
  c.input_select_end();

  c.input_select_start("Rear Right Sensor", "TPMS_RR");
  c.input_select_option("Front_Left",  "0", TPMS_RR == "0");
  c.input_select_option("Front_Right", "1", TPMS_RR == "1");
  c.input_select_option("Rear_Left",   "2", TPMS_RR == "2");
  c.input_select_option("Rear_Right",  "3", TPMS_RR == "3");
  c.input_select_end();
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
      calcADCfactor = MyConfig.GetParamValueBool("xsq", "calc.adcfactor", calcADCfactor);
      adc_factor  = MyConfig.GetParamValue("system.adc", "factor12v", adc_factor.empty() ? "195.7" : adc_factor);
      if (sq->mt_evc_LV_USM_volt) {
        std::string latest = sq->mt_evc_LV_USM_volt->AsString("0.000");
        if (!latest.empty())
          onboard_12v = latest;
      }
      if (onboard_12v.empty())
        onboard_12v  = sq->mt_evc_LV_USM_volt->AsString("0.000");
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
    calcADCfactor = MyConfig.GetParamValueBool("xsq", "calc.adcfactor", false);
    adc_factor  = MyConfig.GetParamValue("system.adc", "factor12v", "195.7");
    if (sq->mt_evc_LV_USM_volt)
      onboard_12v  = sq->mt_evc_LV_USM_volt->AsString("0.000");
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
  c.input_checkbox("Enable automatic recalculation of ADC factor", "calcADCfactor", calcADCfactor,
      "<p>Enable = Automatic recalculation of the ADC factor for the 12V battery voltage measurement.</p>"
      "<p>The ADC factor is recalculated one time each HV charging process by CAN 12V measure.</p>"
      "<p>This ensures that the 12V battery voltage is always measured correctly.</p>");
  if (!adc_history.empty()) {
    c.input_text("History of calculated ADC factors", "adc_history", adc_history.c_str(), "no historical ADC values",
      "<p>History of calculated ADC factors.</p><p>Most recent last, max 20 values.</p>");
  }
  c.input_slider("ADC factor", "adc_factor", 5, "", -1, atof(adc_factor.c_str()), 195.7, 160, 230, 0.01,
    "<p>Factor for the 12V battery voltage measurement by ADC.</p>"
    "<p>To calibrate the 12V ADC measurement.</p>"
    "<p>Default 195.7</p>");
  c.input_slider("12V measured", "onboard_12v", 3, "V",-1, atof(onboard_12v.c_str()), 12.60, 11.00, 15.00, 0.01,
    "<p>Set 12V battery voltage measured for ADC calibration.</p>"
    "<p>On page load = CAN voltage measured value. (xsq.evc.12V.usm.volt)</p>");
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
      MyConfig.SetParamValue("xsq", "suffrange", suffrange);
      MyConfig.SetParamValue("xsq", "suffsoc", suffsoc);
      MyConfig.SetParamValue("xsq", "cell_interval_drv", cell_interval_drv);
      MyConfig.SetParamValue("xsq", "cell_interval_chg", cell_interval_chg);

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
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",-1, atof(suffsoc.c_str()), 80, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();
  
  c.fieldset_start("BMS Cell Monitoring");
  c.input_slider("Update interval driving", "cell_interval_drv", 3, "s",-1, atof(cell_interval_drv.c_str()),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
  c.input_slider("Update interval charging", "cell_interval_chg", 3, "s",-1, atof(cell_interval_chg.c_str()),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
  
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
