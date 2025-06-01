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
 */

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_smarteq.h"

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
  MyWebServer.DeregisterPage("/xsq/battery");
  MyWebServer.DeregisterPage("/xsq/cellmon");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xsq/config)
 */
void OvmsVehicleSmartEQ::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info, full_km, rebootnw, net_type;
  bool canwrite, led, ios, resettrip, resettotal, bcvalue, climate, gpsonoff, charge12v, v2server, extstats, unlocked, mdmcheck;

  if (c.method == "POST") {
    // process form submission:
    canwrite    = (c.getvar("canwrite") == "yes");
    led         = (c.getvar("led") == "yes");
    ios         = (c.getvar("ios") == "yes");
    rebootnw    = (c.getvar("rebootnw"));
    resettrip   = (c.getvar("resettrip") == "yes");
    resettotal  = (c.getvar("resettotal") == "yes");
    bcvalue     = (c.getvar("bcvalue") == "yes");
    full_km  =  (c.getvar("full_km"));
    climate = (c.getvar("climate") == "yes");
    gpsonoff = (c.getvar("gpsonoff") == "yes");
    charge12v = (c.getvar("charge12v") == "yes");
    v2server = (c.getvar("v2server") == "yes");
    net_type = c.getvar("net_type");
    mdmcheck = (c.getvar("mdmcheck") == "yes");
    unlocked = (c.getvar("unlock") == "yes");
    extstats = (c.getvar("extstats") == "yes");
    
    if (error.empty()) {
      // success:
      MyConfig.SetParamValueBool("xsq", "canwrite", canwrite);
      MyConfig.SetParamValueBool("xsq", "led", led);
      MyConfig.SetParamValueBool("xsq", "ios_tpms_fix", ios);
      MyConfig.SetParamValue("xsq", "rebootnw", rebootnw);
      MyConfig.SetParamValueBool("xsq", "resettrip", resettrip);
      MyConfig.SetParamValueBool("xsq", "resettotal", resettotal);
      MyConfig.SetParamValueBool("xsq", "bcvalue", bcvalue);
      MyConfig.SetParamValue("xsq", "full.km", full_km);
      MyConfig.SetParamValueBool("xsq", "climate.system", climate);
      MyConfig.SetParamValueBool("xsq", "gps.onoff", gpsonoff);
      MyConfig.SetParamValueBool("xsq", "12v.charge", charge12v);
      MyConfig.SetParamValueBool("xsq", "v2.check", v2server);
      MyConfig.SetParamValue("xsq", "modem.net.type", net_type);
      MyConfig.SetParamValueBool("xsq", "unlock.warning", unlocked);
      MyConfig.SetParamValueBool("xsq", "modem.check", mdmcheck);
      MyConfig.SetParamValueBool("xsq", "extended.stats", extstats);

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
    ios         = MyConfig.GetParamValueBool("xsq", "ios_tpms_fix", false);
    rebootnw    = MyConfig.GetParamValue("xsq", "rebootnw", "0");
    resettrip   = MyConfig.GetParamValueBool("xsq", "resettrip", false);
    resettotal  = MyConfig.GetParamValueBool("xsq", "resettotal", false);
    bcvalue     = MyConfig.GetParamValueBool("xsq", "bcvalue", false);
    full_km     = MyConfig.GetParamValue("xsq", "full.km", "135");
    climate     = MyConfig.GetParamValueBool("xsq", "climate.system", false);
    gpsonoff    = MyConfig.GetParamValueBool("xsq", "gps.onoff", false);
    charge12v   = MyConfig.GetParamValueBool("xsq", "12v.charge", false);
    v2server    = MyConfig.GetParamValueBool("xsq", "v2.check", false);
    net_type    = MyConfig.GetParamValue("xsq", "modem.net.type", "auto");
    unlocked    = MyConfig.GetParamValueBool("xsq", "unlock.warning", false);
    mdmcheck    = MyConfig.GetParamValueBool("xsq", "modem.check", false);
    extstats    = MyConfig.GetParamValueBool("xsq", "extended.stats", false);
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
    "<p>Enable = Reset Trip Values when Chaging, Disable = Reset Trip Values when Driving</p>");
  c.input_checkbox("Enable reset kWh/100km when Car switched on", "resettotal", resettotal,
    "<p>Enable = Reset calculated kWh/100km values on when Car switched on, auto disabled when resetted</p>");
  c.input_checkbox("Enable OBD kWh/100km value", "bcvalue", bcvalue,
    "<p>Enable = show OBD kWh/100km value</p>");
  c.input_slider("WLTP km", "full_km", 3, "km",
    atof(full_km.c_str()) > 0, atof(full_km.c_str()), 126, 100, 180, 1,
  "<p>set default max Range (126km WLTP, 155km NFEZ) at full charged HV for calculate ideal Range</p>");
  c.fieldset_end();

  c.fieldset_start("Diff Settings");
  c.input_checkbox("Enable/Disable Online state LED when installed", "led", led,
    "<p>RED=Internet no, BLUE=Internet yes, GREEN=Server v2 connected.<br>EGPIO Port 7,8,9 are used</p>");
  c.input_checkbox("Enable iOS TPMS fix", "ios", ios,
    "<p>Set External Temperatures to TPMS Temperatures to Display Tire Pressures in iOS App Open Vehicle</p>");
  c.input_checkbox("Enable Climate System", "climate", climate,
    "<p>Enable = time based Climate control</p>");
  c.input_checkbox("Enable GPS off at Parking", "gpsonoff", gpsonoff,
    "<p>Enable = switch GPS off at Parking for power saving. Every 50 minutes powered on the GPS for 10 minutes.</p>");
  c.input_checkbox("Enable 12V charging", "charge12v", charge12v,
    "<p>Enable = charge the 12V if low 12V alert is raised</p>");
  c.input_checkbox("Enable V2 Server", "v2server", v2server,
    "<p>Enable = keep v2 Server connected</p>");
  c.input_checkbox("Enable Modem check", "mdmcheck", mdmcheck,
    "<p>Enable = Restart Modem, when signal strength if less than 20 dBm</p>");  
  c.input_checkbox("Enable Door unlocked warning", "unlocked", unlocked,
    "<p>Enable = send a warning when Car 10 minutes parked and unlocked</p>");
  c.input_checkbox("Enable extended statistics", "extstats", extstats,
      "<p>Enable = Show extended statistics incl. maintenance and trip data. Not recomment for iOS Open Vehicle App!</p>");
  c.input_slider("Restart Network Time", "rebootnw", 3, "min",
    atof(rebootnw.c_str()) > 0, atof(rebootnw.c_str()), 15, 0, 60, 1,
    "<p>Default 0 = off. Restart Network automatic when no v2Server connection.</p>");
  c.input_select_start("Modem Network type", "net_type");
  c.input_select_option("Auto", "auto", net_type == "auto");
  c.input_select_option("GSM/LTE", "gsm", net_type == "gsm");
  c.input_select_option("LTE", "lte", net_type == "lte");
  c.input_select_end();
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

  std::string error, info;
  
  if (c.method == "POST") {
      // Process form submission
      bool climate_on = (c.getvar("climate_on") == "yes");
      bool climate_weekly = (c.getvar("climate_weekly") == "yes");
      std::string climate_time = c.getvar("climate_time");
      std::string climate_ds = c.getvar("climate_ds");
      std::string climate_de = c.getvar("climate_de");
      std::string climate_1to3 = c.getvar("climate_1to3");

      // Input validation
      if (climate_time.empty()) {
          error += "<li>Time must be specified</li>";
      }

      // Convert values
      int climate_on_int = climate_on ? 1 : 2;
      int climate_weekly_int = climate_weekly ? 1 : 2;
      if(climate_on_int == 2){
        climate_weekly_int = 2;
      }
      int climate_time_int = atoi(climate_time.c_str());
      int climate_ds_int = atoi(climate_ds.c_str());
      int climate_de_int = atoi(climate_de.c_str());
      int climate_1to3_int = 0;

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
  std::string climate_time = sq->mt_climate_time->AsString();
  std::string climate_ds = sq->mt_climate_ds->AsString();
  std::string climate_de = sq->mt_climate_de->AsString();
  std::string climate_1to3 = sq->mt_climate_1to3->AsString();
  bool climate_on = sq->mt_climate_on->AsBool();
  bool climate_weekly = sq->mt_climate_weekly->AsBool();
      
  c.head(200);
  c.panel_start("primary", "Climate Control Settings");
  c.form_start(p.uri);

  c.fieldset_start("Climate/Heater start timer");
  c.input_checkbox("Enable Climate/Heater at time", "climate_on", climate_on,
    "<p>Enable = start Climate/Heater at time</p>");
  c.input_slider("Enable two time activation Climate/Heater", "climate_1to3", 3, "min",
    atof(climate_1to3.c_str()) > -1, atof(climate_1to3.c_str()), 5, 5, 15, 5,
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
  bool iosfix, enable;
  
  if (c.method == "POST") {
    // Process form submission
    iosfix      = (c.getvar("iosfix") == "yes");
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
      MyConfig.SetParamValueBool("xsq", "ios_tpms_fix", iosfix);
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
    iosfix      = MyConfig.GetParamValueBool("xsq", "ios_tpms_fix", false);
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
  c.input_checkbox("Enable iOS TPMS fix", "iosfix", iosfix,
    "<p>Set External Temperatures to TPMS Temperatures to Display Tire Pressures in iOS App Open Vehicle</p>");
  c.input_checkbox("Enable TPMS Alert", "enable", enable,
    "<p>enable TPMS Tire Pressures low/high alert</p>");
  c.input_slider("Front Tire Pressure", "front_pressure", 3, "kPa",
    atof(front_pressure.c_str()) > 0, atof(front_pressure.c_str()), 220, 170, 350, 5,
    "<p>set Front Tire Pressure value</p>");
  c.input_slider("Rear Tire Pressure", "rear_pressure", 3, "kPa",
    atof(rear_pressure.c_str()) > 0, atof(rear_pressure.c_str()), 250, 170, 350, 5,
    "<p>set Rear Tire Pressure value</p>");
  c.input_slider("Pressure Warning", "pressure_warning", 3, "kPa",
    atof(pressure_warning.c_str()) > 0, atof(pressure_warning.c_str()), 25, 10, 60, 5,
    "<p>set under/over Pressure Warning value</p>");
  c.input_slider("Pressure Alert", "pressure_alert", 3, "kPa",
    atof(pressure_alert.c_str()) > 0, atof(pressure_alert.c_str()), 45, 30, 120, 5,
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

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 75, 0, 150, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 80, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();
  
  c.fieldset_start("BMS Cell Monitoring");
  c.input_slider("Update interval driving", "cell_interval_drv", 3, "s",
    atof(cell_interval_drv.c_str()) > 0, atof(cell_interval_drv.c_str()),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
  c.input_slider("Update interval charging", "cell_interval_chg", 3, "s",
    atof(cell_interval_chg.c_str()) > 0, atof(cell_interval_chg.c_str()),
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
