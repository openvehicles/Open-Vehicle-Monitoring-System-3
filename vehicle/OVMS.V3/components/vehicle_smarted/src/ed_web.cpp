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

#include <string.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <dirent.h>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_smarted.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleSmartED::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xse/features",   "Features",          WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/battery",    "Battery config",    WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  //MyWebServer.RegisterPage("/xse/brakelight", "Brake Light",      OvmsWebServer::HandleCfgBrakelight,  PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/cellmon",    "BMS Cell Monitor",  WebCfgBmsCellMonitor,                PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/eco",        "Eco Scores",        WebCfgEco,                           PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/cellcapa",   "BMS Cell Capacity", WebCfgBmsCellCapacity,               PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/commands",   "Commands",          WebCfgCommands,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/notify",     "Notifys",           WebCfgNotify,                        PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleSmartED::WebDeInit()
{
  MyWebServer.DeregisterPage("/xse/features");
  MyWebServer.DeregisterPage("/xse/battery");
  MyWebServer.DeregisterPage("/xse/eco");
  //MyWebServer.DeregisterPage("/xse/brakelight");
  MyWebServer.DeregisterPage("/xse/cellmon");
  MyWebServer.DeregisterPage("/xse/cellcapa");
  MyWebServer.DeregisterPage("/xse/commands");
  MyWebServer.DeregisterPage("/xse/notify");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xse/config)
 */
void OvmsVehicleSmartED::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  bool canwrite;
  bool soc_rsoc;
  bool lockstate;
  bool gpio_highlow;
  std::string doorlock, doorunlock, ignition, doorstatus, rangeideal, egpio_timout, reboot;

  if (c.method == "POST") {
    // process form submission:
    doorlock = c.getvar("doorlock");
    doorunlock = c.getvar("doorunlock");
    ignition = c.getvar("ignition");
    doorstatus = c.getvar("doorstatus");
    rangeideal = c.getvar("rangeideal");
    egpio_timout = c.getvar("egpio_timout");
    soc_rsoc = (c.getvar("soc_rsoc") == "yes");
    canwrite  = (c.getvar("canwrite") == "yes");
    lockstate  = (c.getvar("lockstate") == "yes");
    reboot = c.getvar("reboot");
    gpio_highlow = (c.getvar("gpio_highlow") == "yes");

    // validate:
    if (doorlock != "") {
      int v = atoi(doorlock.c_str());
      if (v < 3 || v > 9) {
        error += "<li data-input=\"doorlock\">Port must be one of 3…9</li>";
      }
    }
    if (doorunlock != "") {
      int v = atoi(doorunlock.c_str());
      if (v < 3 || v > 9) {
        error += "<li data-input=\"doorunlock\">Port must be one of 3…9</li>";
      }
    }
    if (ignition != "") {
      int v = atoi(ignition.c_str());
      if (v == 2 || v < 1 || v > 9) {
        error += "<li data-input=\"ignition\">Port must be one of 1, 3…9</li>";
      }
    }
    if (doorstatus != "") {
      int v = atoi(doorstatus.c_str());
      if (v == 2 || v < 1 || v > 9) {
        error += "<li data-input=\"doorstatus\">Port must be one of 1, 3…9</li>";
      }
    }
    if (rangeideal != "") {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 200) {
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 90…200 km</li>";
      }
    }
    if (egpio_timout != "") {
      int v = atoi(egpio_timout.c_str());
      if (v < 1) {
        error += "<li data-input=\"egpio_timout\">Ignition Timeout must be greater or equal 1.</li>";
      }
    }
    if (!reboot.empty()) {
      float n = atof(reboot.c_str());
      if (n < 0 || n > 60)
        error += "<li data-input=\"reboot\">Network restart time invalid, must be 0…60</li>";
    }
    
    if (error == "") {
      // success:
      MyConfig.SetParamValue("xse", "doorlock.port", doorlock);
      MyConfig.SetParamValue("xse", "doorunlock.port", doorunlock);
      MyConfig.SetParamValue("xse", "ignition.port", ignition);
      MyConfig.SetParamValue("xse", "doorstatus.port", doorstatus);
      MyConfig.SetParamValue("xse", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xse", "egpio_timout", egpio_timout);
      MyConfig.SetParamValue("xse", "reboot", reboot);
      MyConfig.SetParamValueBool("xse", "soc_rsoc", soc_rsoc);
      MyConfig.SetParamValueBool("xse", "canwrite",   canwrite);
      MyConfig.SetParamValueBool("xse", "lockstate",   lockstate);
      MyConfig.SetParamValueBool("xse", "gpio_highlow",   gpio_highlow);

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
  }
  else {
    // read configuration:
    doorlock     = MyConfig.GetParamValue("xse", "doorlock.port", "9");
    doorunlock   = MyConfig.GetParamValue("xse", "doorunlock.port", "8");
    ignition     = MyConfig.GetParamValue("xse", "ignition.port", "7");
    doorstatus   = MyConfig.GetParamValue("xse", "doorstatus.port", "6");
    rangeideal   = MyConfig.GetParamValue("xse", "rangeideal", "135");
    egpio_timout = MyConfig.GetParamValue("xse", "egpio_timout", "5");
    reboot       = MyConfig.GetParamValue("xse", "reboot", "0");
    soc_rsoc     = MyConfig.GetParamValueBool("xse", "soc_rsoc", false);
    canwrite     = MyConfig.GetParamValueBool("xse", "canwrite", false);
    lockstate    = MyConfig.GetParamValueBool("xse", "lockstate", false);
    gpio_highlow = MyConfig.GetParamValueBool("xse", "gpio_highlow", false);
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Smart ED feature configuration");
  c.form_start(p.uri);
  
  c.input("number", "Range Ideal", "rangeideal", rangeideal.c_str(), "Default: 135",
    "<p>This determines the Ideal Range.</p>",
    "min=\"90\" max=\"200\" step=\"1\"");
  
  c.input_checkbox("change Display SOC = realSOC", "soc_rsoc", soc_rsoc,
    "<p>WARNING: change Displayed SOC to realSOC.</p>");
  
  c.input_checkbox("Enable CAN write(Poll)", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  
  c.input_checkbox("Enable Door lock/unlock Status", "lockstate", lockstate,
    "<p>Only needed when External door status wired to expansion.</p>");
  
  c.input_checkbox("Change EGPIO direction", "gpio_highlow", gpio_highlow,
    "<p>Change EGPIO direction. (HIGH -> LOW | LOW -> HIGH)</br>Only for Door lock/unlock</p>");

  c.input_select_start("… Vehicle lock port", "doorlock");
  c.input_select_option("EGPIO_2", "3", doorlock == "3");
  c.input_select_option("EGPIO_3", "4", doorlock == "4");
  c.input_select_option("EGPIO_4", "5", doorlock == "5");
  c.input_select_option("EGPIO_5", "6", doorlock == "6");
  c.input_select_option("EGPIO_6", "7", doorlock == "7");
  c.input_select_option("EGPIO_7", "8", doorlock == "8");
  c.input_select_option("EGPIO_8", "9", doorlock == "9");
  c.input_select_end();

  c.input_select_start("… Vehicle unlock port", "doorunlock");
  c.input_select_option("EGPIO_2", "3", doorunlock == "3");
  c.input_select_option("EGPIO_3", "4", doorunlock == "4");
  c.input_select_option("EGPIO_4", "5", doorunlock == "5");
  c.input_select_option("EGPIO_5", "6", doorunlock == "6");
  c.input_select_option("EGPIO_6", "7", doorunlock == "7");
  c.input_select_option("EGPIO_7", "8", doorunlock == "8");
  c.input_select_option("EGPIO_8", "9", doorunlock == "9");
  c.input_select_end();

  c.input_select_start("… Ignition port", "ignition");
  c.input_select_option("SW_12V (DA26 pin 18)", "1", ignition == "1");
  c.input_select_option("EGPIO_2", "3", ignition == "3");
  c.input_select_option("EGPIO_3", "4", ignition == "4");
  c.input_select_option("EGPIO_4", "5", ignition == "5");
  c.input_select_option("EGPIO_5", "6", ignition == "6");
  c.input_select_option("EGPIO_6", "7", ignition == "7");
  c.input_select_option("EGPIO_7", "8", ignition == "8");
  c.input_select_option("EGPIO_8", "9", ignition == "9");
  c.input_select_end();
  
  c.input_select_start("… Vehicle lock/unlock Status", "doorstatus");
  c.input_select_option("SW_12V (DA26 pin 18)", "1", doorstatus == "1");
  c.input_select_option("EGPIO_2", "3", doorstatus == "3");
  c.input_select_option("EGPIO_3", "4", doorstatus == "4");
  c.input_select_option("EGPIO_4", "5", doorstatus == "5");
  c.input_select_option("EGPIO_5", "6", doorstatus == "6");
  c.input_select_option("EGPIO_6", "7", doorstatus == "7");
  c.input_select_option("EGPIO_7", "8", doorstatus == "8");
  c.input_select_option("EGPIO_8", "9", doorstatus == "9");
  c.input_select_end();

  c.input_slider("… Ignition Timeout", "egpio_timout", 5, "min",
    -1, egpio_timout.empty() ? 5 : atof(egpio_timout.c_str()), 5, 1, 30, 1,
    "<p>How long the Ignition should stay on in minutes.</p>");

  c.input_slider("Restart Network Time", "reboot", 3, "min",
    atof(reboot.c_str()) > 0, atof(reboot.c_str()), 15, 0, 60, 1,
    "<p>Default 0=off. Restart Network automatic when no v2Server connection.</p>");

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}

/**
 * WebCfgBattery: configure battery parameters (URL /xse/battery)
 */
void OvmsVehicleSmartED::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  std::string suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    suffrange = c.getvar("suffrange");
    suffsoc   = c.getvar("suffsoc");

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
      MyConfig.SetParamValue("xse", "suffrange", suffrange);
      MyConfig.SetParamValue("xse", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">SmartED3 battery setup saved.</p>");
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
    suffrange = MyConfig.GetParamValue("xse", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xse", "suffsoc", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Smart ED Battery Setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 75, 0, 150, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 80, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebCfgCommands: Vehicle Commands (URL /xse/commands)
 */
void OvmsVehicleSmartED::WebCfgCommands(PageEntry_t& p, PageContext_t& c)
{
  // generate form:
  c.head(200);
  PAGE_HOOK("body.pre");
  c.panel_start("primary", "Smart ED Vehicle Commands");
  c.print(
    "<div class=\"panel-body\">\n"
      "<div class=\"row text-center\">\n"
        "<p><button class=\"btn btn-default btn-lg\" style=\"font-size:50px;border-radius:18px;\" data-cmd=\"xse recu up\" data-target=\"#output\" data-watchcnt=\"0\">Recu Up</button></p>\n"
        "<br>\n"
        "<p><button class=\"btn btn-default btn-lg\" style=\"font-size:50px;border-radius:18px;\" data-cmd=\"xse recu down\" data-target=\"#output\" data-watchcnt=\"0\">Recu Down</button></p>\n"
        "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
      "</div>\n"
    "</div>\n"
  );
  c.panel_end();

  c.done();
}

/**
 * WebCfgNotify: Vehicle Notify (URL /xse/notify)
 */
void OvmsVehicleSmartED::WebCfgNotify(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;;
  bool trip;
  
  if (c.method == "POST") {
    // process form submission:
    trip = (c.getvar("trip") == "yes");

    MyConfig.SetParamValueBool("xse", "notify.trip", trip);
    
    info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
    c.head(200);
    c.alert("success", info.c_str());
  }
  else {
    // read configuration:
    trip = MyConfig.GetParamValueBool("xse", "notify.trip", true);
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Smart ED Vehicle Notifys");
  c.form_start(p.uri);
  
  c.input_checkbox("Notify trip", "trip", trip,
    "<p>Notify Trip values after driving end</p>");

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}

/**
 * WebCfgBmsCellMonitor: display cell voltages & temperatures
 */
void OvmsVehicleSmartED::WebCfgBmsCellMonitor(PageEntry_t& p, PageContext_t& c)
{
  float stemwidth_v = 0.5, stemwidth_t = 0.5;
  float
    volt_warn_def=BMS_DEFTHR_VWARN, volt_alert_def=BMS_DEFTHR_VALERT,
    temp_warn_def=BMS_DEFTHR_TWARN, temp_alert_def=BMS_DEFTHR_TALERT;
  float volt_warn=0, volt_alert=0, temp_warn=0, temp_alert=0;
  bool alerts_enabled=true;

  // get vehicle BMS configuration:
  OvmsVehicle* vehicle = MyVehicleFactory.ActiveVehicle();
  if (vehicle) {
    int readings_v = vehicle->BmsGetCellArangementVoltage();
    if (readings_v) {
      stemwidth_v   = 0.1 + 20.0 / readings_v;  // 14 → 1.5 … 96 → 0.3
    }
    int readings_t = vehicle->BmsGetCellArangementTemperature();
    if (readings_t) {
      stemwidth_t   = 0.1 + 10.0 / readings_t;  //  7 → 1.5 … 96 → 0.4
    }
    vehicle->BmsGetCellDefaultThresholdsVoltage(&volt_warn_def, &volt_alert_def);
    vehicle->BmsGetCellDefaultThresholdsTemperature(&temp_warn_def, &temp_alert_def);
  }

  if (c.method == "POST") {
    // process form submission:
    
    volt_warn = atof(c.getvar("volt_warn").c_str()) / 1000;
    if (volt_warn > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.warn", volt_warn);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.warn", "");

    volt_alert = atof(c.getvar("volt_alert").c_str()) / 1000;
    if (volt_alert > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.alert", volt_alert);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.alert", "");

    temp_warn = atof(c.getvar("temp_warn").c_str());
    if (temp_warn > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.temp.warn", temp_warn);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.temp.warn", "");

    temp_alert = atof(c.getvar("temp_alert").c_str());
    if (temp_alert > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.temp.alert", temp_alert);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.temp.alert", "");

    MyConfig.SetParamValueBool("vehicle", "bms.alerts.enabled", c.getvar("alerts_enabled") == "yes");
    
    if (c.getvar("action") == "save-reset" && vehicle)
      vehicle->BmsResetCellStats();
    
    c.head(200);
    c.alert("success", "<p>Saved.</p>");
    c.print("<script>after(2, function(){ $('.modal').modal('hide'); });</script>");
    c.done();
    return;
  }

  // read config:
  volt_warn = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.warn");
  volt_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.alert");
  temp_warn = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.warn");
  temp_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.alert");
  alerts_enabled = MyConfig.GetParamValueBool("vehicle", "bms.alerts.enabled", true);
  
  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<div class=\"panel panel-primary panel-single\">\n"
      "<div class=\"panel-heading\">BMS Cell Monitor</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"row\">\n"
          "<div class=\"receiver\" id=\"livestatus\">\n"
            "<div id=\"voltchart\" style=\"width: 100%; max-width: 100%; height: 45vh; min-height: 280px; margin: 0 auto\"></div>\n"
            "<div id=\"tempchart\" style=\"width: 100%; max-width: 100%; height: 25vh; min-height: 160px; margin: 0 auto\"></div>\n"
          "</div>\n"
        "</div>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<button class=\"btn btn-default\" data-toggle=\"modal\" data-target=\"#cfg-dialog\">Alert config</button>\n"
        "<button class=\"btn btn-default\" data-cmd=\"bms reset\" data-target=\"#output\" data-watchcnt=\"0\">Reset min/max</button>\n"
        "<button class=\"btn btn-default\" data-cmd=\"xse obd request getvolts\" data-target=\"#output\" data-watchcnt=\"0\">Get Voltages</button>\n"
        "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<div class=\"modal fade\" id=\"cfg-dialog\" role=\"dialog\" data-backdrop=\"true\" data-keyboard=\"true\">\n"
      "<div class=\"modal-dialog modal-lg\">\n"
        "<div class=\"modal-content\">\n"
          "<div class=\"modal-header\">\n"
            "<button type=\"button\" class=\"close\" data-dismiss=\"modal\">&times;</button>\n"
            "<h4 class=\"modal-title\">Alert configuration</h4>\n"
          "</div>\n"
          "<div class=\"modal-body\">\n");

  // Alert configuration form:

  std::ostringstream sval, sdef;
  sval << std::fixed;
  sdef << std::fixed;
  
  c.form_start(p.uri, "#cfg-result");

  c.input_info("Thresholds",
    "<p>Deviation warnings and alerts are triggered by cell deviation from the current average.</p>"
    "<p>You may need to change the thresholds to adapt to the quality and age of your battery.</p>");

  sval << std::setprecision(0);
  sdef << std::setprecision(0);
  sval.str(""); if (volt_warn > 0) sval << volt_warn * 1000;
  sdef.str(""); sdef << "Default: " << volt_warn_def * 1000;
  c.input("number", "Voltage warning", "volt_warn", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"1\" step=\"1\"", "mV");
  sval.str(""); if (volt_alert > 0) sval << volt_alert * 1000;
  sdef.str(""); sdef << "Default: " << volt_alert_def * 1000;
  c.input("number", "Voltage alert", "volt_alert", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"1\" step=\"1\"", "mV");

  sval << std::setprecision(1);
  sdef << std::setprecision(1);
  sval.str(""); if (temp_warn > 0) sval << temp_warn;
  sdef.str(""); sdef << "Default: " << temp_warn_def;
  c.input("number", "Temperature warning", "temp_warn", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0.1\" step=\"0.1\"", "°C");
  sval.str(""); if (temp_alert > 0) sval << temp_alert;
  sdef.str(""); sdef << "Default: " << temp_alert_def;
  c.input("number", "Temperature alert", "temp_alert", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0.1\" step=\"0.1\"", "°C");

  c.input_checkbox("Enable alert notifications", "alerts_enabled", alerts_enabled);

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save\">Save</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save-reset\">Save &amp; reset alerts</button> "
      "</div>"
    "</div>"
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<div id=\"cfg-result\" class=\"modal-autoclear\"></div>"
      "</div>"
    "</div>");
  c.form_end();

  c.print(
          "</div>\n"
          "<div class=\"modal-footer\">\n"
            "<button type=\"button\" class=\"btn btn-default\" data-dismiss=\"modal\">Close</button>\n"
          "</div>\n"
        "</div>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "\n"
    "<style>\n"
    "\n"
    ".highcharts-boxplot-stem {\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".highcharts-point {\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".night .highcharts-columnrange-series .highcharts-point {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    ".night .highcharts-legend-item text {\n"
      "fill: #dddddd;\n"
    "}\n"
    "\n"
    ".highcharts-plot-line {\n"
      "fill: none;\n"
      "stroke: #3625c3;\n"
      "stroke-width: 2px;\n"
    "}\n"
    ".night .highcharts-plot-line {\n"
      "stroke: #728def;\n"
    "}\n"
    ".highcharts-plot-band {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
    "}\n"
    ".night .highcharts-plot-band {\n"
      "fill: #728def;\n"
      "fill-opacity: 0.2;\n"
    "}\n"
    "\n"
    ".plot-line-sdmaxlo {\n"
      "stroke: #3625c3;\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".plot-line-sdmaxhi {\n"
      "stroke: #3625c3;\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".night .plot-line-sdmaxlo {\n"
      "stroke: #728def;\n"
    "}\n"
    ".night .plot-line-sdmaxhi {\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    ".highcharts-plot-line-label {\n"
      "fill: #888;\n"
      "font-size: 10px;\n"
    "}\n"
    "\n"
    ".highcharts-color-1 {\n"
      "fill: #ffb500;\n"
      "stroke: #cc950e;\n"
    "}\n"
    ".highcharts-color-2 {\n"
      "fill: #ff3400;\n"
      "stroke: #ff3400;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
      "stroke: #3625c3;\n"
    "}\n"
    ".night #voltchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #728def;\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-boxplot-stem {\n"
      "fill: #63cc7a;\n"
      "fill-opacity: 1;\n"
      "stroke: #63cc7a;\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".night #voltchart .highcharts-boxplot-stem {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-boxplot-median {\n"
      "stroke: #193e07;\n"
      "stroke-width: 5px;\n"
      "stroke-linecap: round;\n"
    "}\n"
    ".night #voltchart .highcharts-boxplot-median {\n"
      "stroke: #afff45;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
      "stroke: #3625c3;\n"
    "}\n"
    ".night #tempchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #728def;\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-boxplot-stem {\n"
      "fill: #da9073;\n"
      "fill-opacity: 1;\n"
      "stroke: #da9073;\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".night #tempchart .highcharts-boxplot-stem {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-boxplot-median {\n"
      "stroke: #7d1313;\n"
      "stroke-width: 5px;\n"
      "stroke-linecap: round;\n"
    "}\n"
    ".night #tempchart .highcharts-boxplot-median {\n"
      "stroke: #fdd02e;\n"
    "}\n");
  
  c.printf(
    "#voltchart .highcharts-boxplot-stem {\n"
      "stroke-width: %.1f%%;\n"
    "}\n"
    "#tempchart .highcharts-boxplot-stem {\n"
      "stroke-width: %.1f%%;\n"
    "}\n"
    "</style>\n"
    , stemwidth_v, stemwidth_t);

  c.print(
    "<script>\n"
    "\n"
    "/**\n"
     "* Cell voltage chart\n"
     "*/\n"
    "\n"
    "var voltchart;\n"
    "\n"
    "// get_volt_data: build boxplot dataset from metrics\n"
    "function get_volt_data() {\n"
      "var data = { cells: [], volts: [], devmax: [], voltmean: 0, sdlo: 0, sdhi: 0, sdmaxlo: 0, sdmaxhi: 0 };\n"
      "var cnt = metrics[\"v.b.c.voltage\"] ? metrics[\"v.b.c.voltage\"].length : 0;\n"
      "if (cnt == 0)\n"
        "return data;\n"
      "var i, act, min, max, devmax, dalert, dlow, dhigh, voffset;\n"
      "voffset = metrics[\"xse.mybms.adc.volts.offset\"]/1000;\n"
      "data.voltmean = metrics[\"v.b.p.voltage.avg\"]-voffset || 0;\n"
      "data.sdlo = data.voltmean - (metrics[\"v.b.p.voltage.stddev\"] || 0);\n"
      "data.sdhi = data.voltmean + (metrics[\"v.b.p.voltage.stddev\"] || 0);\n"
      "data.sdmaxlo = data.voltmean - (metrics[\"v.b.p.voltage.stddev.max\"] || 0);\n"
      "data.sdmaxhi = data.voltmean + (metrics[\"v.b.p.voltage.stddev.max\"] || 0);\n"
      "for (i=0; i<cnt; i++) {\n"
        "act = metrics[\"v.b.c.voltage\"][i]-voffset;\n"
        "min = metrics[\"v.b.c.voltage.min\"][i]-voffset || act;\n"
        "max = metrics[\"v.b.c.voltage.max\"][i]-voffset || act;\n"
        "devmax = metrics[\"v.b.c.voltage.dev.max\"][i] || 0;\n"
        "dalert = metrics[\"v.b.c.voltage.alert\"][i] || 0;\n"
        "if (devmax > 0) {\n"
          "dlow = data.voltmean;\n"
          "dhigh = data.voltmean + devmax;\n"
        "} else {\n"
          "dlow = data.voltmean + devmax;\n"
          "dhigh = data.voltmean;\n"
        "}\n"
        "data.cells.push(i+1);\n"
        "data.volts.push([min,act,act,act,max]);\n"
        "data.devmax.push({ x:i, low:dlow, high:dhigh, colorIndex:dalert,\n"
            "id: ((devmax < 0) ? '▼ ' : '▲ ') + Math.abs(devmax * 1000).toFixed(0) + ' mV' });\n"
      "}\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_volt_chart() {\n"
      "var data = get_volt_data();\n"
      "voltchart.xAxis[0].setCategories(data.cells);\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-mean');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-mean', className: 'plot-line-mean', value: data.voltmean, zIndex: 3 });\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-sdmaxlo');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 });\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-sdmaxhi');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } });\n"
      "voltchart.yAxis[0].removePlotBand('plot-band-sd');\n"
      "voltchart.yAxis[0].addPlotBand({ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi });\n"
      "voltchart.series[0].setData(data.volts);\n"
      "voltchart.series[1].setData(data.devmax);\n"
    "}\n"
    "\n"
    "function init_volt_chart() {\n"
      "var data = get_volt_data();\n"
      "voltchart = Highcharts.chart('voltchart', {\n"
        "chart: {\n"
          "type: 'boxplot',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"v.b.c.voltage\"] != null\n"
                 "|| update[\"v.b.c.voltage.min\"] != null\n"
                 "|| update[\"v.b.c.voltage.max\"] != null\n"
                 "|| update[\"v.b.c.voltage.dev.max\"] != null\n"
                 "|| update[\"v.b.c.voltage.alert\"] != null)\n"
                  "update_volt_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'y',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "enabled: true,\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
          "margin: 2,\n"
          "padding: 2,\n"
        "},\n"
        "xAxis: { categories: data.cells },\n"
        "yAxis: {\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.2f}V\" },\n"
          "minTickInterval: 0.01,\n"
          "minorTickInterval: 'auto',\n"
          "plotLines: [\n"
            "{ id: 'plot-line-mean', className: 'plot-line-mean', value: data.voltmean, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } },\n"
          "],\n"
          "plotBands: [{ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi }],\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "padding: 4,\n"
          "positioner: function (labelWidth, labelHeight, point) {\n"
            "return {\n"
              "x: voltchart.plotLeft + Math.min(voltchart.plotWidth - labelWidth, Math.max(0, point.plotX - labelWidth/2)),\n"
              "y: 0 };\n"
          "},\n"
          "headerFormat: 'Voltage #{point.key}: ',\n"
          "pointFormatter: function () {\n"
            "if (this.series.index == 0) {\n"
              "return '<b>' + this.median.toFixed(3) + ' V</b>'\n"
                "+ '  [' + this.low.toFixed(3) + ' – ' + this.high.toFixed(3) + ']<br/>';\n"
            "} else {\n"
              "return 'Max deviation: <b>' + this.id + '</b>';\n"
            "}\n"
          "},\n"
        "},\n"
        "series: [{\n"
          "name: 'Voltage',\n"
          "zIndex: 1,\n"
          "data: data.volts,\n"
          "whiskerLength: 0,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max deviation',\n"
          "zIndex: 0,\n"
          "type: 'columnrange',\n"
          "data: data.devmax,\n"
          "maxPointWidth: 35,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#voltchart').data('chart', voltchart).addClass('has-chart');"
    "}\n"
    "\n"
    "\n"
    "/**\n"
     "* Cell temperature chart\n"
     "*/\n"
    "\n"
    "var tempchart;\n"
    "\n"
    "// get_temp_data: build boxplot dataset from metrics\n"
    "function get_temp_data() {\n"
      "var data = { cells: [], temps: [], devmax: [], tempmean: 0, sdlo: 0, sdhi: 0, sdmaxlo: 0, sdmaxhi: 0 };\n"
      "var cnt = metrics[\"v.b.c.temp\"] ? metrics[\"v.b.c.temp\"].length : 0;\n"
      "if (cnt == 0)\n"
        "return data;\n"
      "var i, act, min, max, devmax, dalert, dlow, dhigh;\n"
      "data.tempmean = metrics[\"v.b.p.temp.avg\"] || 0;\n"
      "data.sdlo = data.tempmean - (metrics[\"v.b.p.temp.stddev\"] || 0);\n"
      "data.sdhi = data.tempmean + (metrics[\"v.b.p.temp.stddev\"] || 0);\n"
      "data.sdmaxlo = data.tempmean - (metrics[\"v.b.p.temp.stddev.max\"] || 0);\n"
      "data.sdmaxhi = data.tempmean + (metrics[\"v.b.p.temp.stddev.max\"] || 0);\n"
      "for (i=0; i<cnt; i++) {\n"
        "act = metrics[\"v.b.c.temp\"][i];\n"
        "min = metrics[\"v.b.c.temp.min\"][i] || act;\n"
        "max = metrics[\"v.b.c.temp.max\"][i] || act;\n"
        "devmax = metrics[\"v.b.c.temp.dev.max\"][i] || 0;\n"
        "dalert = metrics[\"v.b.c.temp.alert\"][i] || 0;\n"
        "if (devmax > 0) {\n"
          "dlow = data.tempmean;\n"
          "dhigh = data.tempmean + devmax;\n"
        "} else {\n"
          "dlow = data.tempmean + devmax;\n"
          "dhigh = data.tempmean;\n"
        "}\n"
        "data.cells.push(i+1);\n"
        "data.temps.push([min,act,act,act,max]);\n"
        "data.devmax.push({ x:i, low:dlow, high:dhigh, colorIndex:dalert,\n"
            "id: ((devmax < 0) ? '▼ ' : '▲ ') + Math.abs(devmax).toFixed(1) + ' °C' });\n"
      "}\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_temp_chart() {\n"
      "var data = get_temp_data();\n"
      "tempchart.xAxis[0].setCategories(data.cells);\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-mean');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-mean', className: 'plot-line-mean', value: data.tempmean, zIndex: 3 });\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-sdmaxlo');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 });\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-sdmaxhi');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } });\n"
      "tempchart.yAxis[0].removePlotBand('plot-band-sd');\n"
      "tempchart.yAxis[0].addPlotBand({ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi });\n"
      "tempchart.series[0].setData(data.temps);\n"
      "tempchart.series[1].setData(data.devmax);\n"
    "}\n"
    "\n"
    "function init_temp_chart() {\n"
      "var data = get_temp_data();\n"
      "tempchart = Highcharts.chart('tempchart', {\n"
        "chart: {\n"
          "type: 'boxplot',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"v.b.c.temp\"] != null\n"
                 "|| update[\"v.b.c.temp.min\"] != null\n"
                 "|| update[\"v.b.c.temp.max\"] != null\n"
                 "|| update[\"v.b.c.temp.dev.max\"] != null\n"
                 "|| update[\"v.b.c.temp.alert\"] != null)\n"
                  "update_temp_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'y',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "enabled: true,\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
          "margin: 2,\n"
          "padding: 2,\n"
        "},\n"
        "xAxis: { categories: data.cells },\n"
        "yAxis: {\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.0f} °C\" },\n"
          "minTickInterval: 1,\n"
          "minorTickInterval: 'auto',\n"
          "plotLines: [\n"
            "{ id: 'plot-line-mean', className: 'plot-line-mean', value: data.tempmean, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } },\n"
          "],\n"
          "plotBands: [{ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi }],\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "padding: 4,\n"
          "positioner: function (labelWidth, labelHeight, point) {\n"
            "return {\n"
              "x: tempchart.plotLeft + Math.min(tempchart.plotWidth - labelWidth, Math.max(0, point.plotX - labelWidth/2)),\n"
              "y: 0 };\n"
          "},\n"
          "headerFormat: 'Temperature #{point.key}: ',\n"
          "pointFormatter: function () {\n"
            "if (this.series.index == 0) {\n"
              "return '<b>' + this.median.toFixed(1) + ' °C</b>'\n"
                "+ '  [' + this.low.toFixed(1) + ' – ' + this.high.toFixed(1) + ']<br/>';\n"
            "} else {\n"
              "return 'Max deviation: <b>' + this.id + '</b>';\n"
            "}\n"
          "},\n"
        "},\n"
        "series: [{\n"
          "name: 'Temperature',\n"
          "zIndex: 1,\n"
          "data: data.temps,\n"
          "whiskerLength: 0,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max deviation',\n"
          "zIndex: 0,\n"
          "type: 'columnrange',\n"
          "data: data.devmax,\n"
          "maxPointWidth: 35,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#tempchart').data('chart', tempchart).addClass('has-chart');"
    "}\n"
    "\n"
    "\n"
    "/**\n"
     "* Chart initialization\n"
     "*/\n"
    "\n"
    "function init_charts() {\n"
      "init_volt_chart();\n"
      "init_temp_chart();\n"
    "}\n"
    "\n"
    "if (window.Highcharts) {\n"
      "init_charts();\n"
    "} else {\n"
      "$.ajax({\n"
        "url: \"" URL_ASSETS_CHARTS_JS "\",\n"
        "dataType: \"script\",\n"
        "cache: true,\n"
        "success: function(){ init_charts(); }\n"
      "});\n"
    "}\n"
    "\n"
    "</script>\n");
  
  PAGE_HOOK("body.post");
  c.done();
}

/**
 * WebCfgBmsCellCapacity: display cell capacitys
 */
void OvmsVehicleSmartED::WebCfgBmsCellCapacity(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  c.printf(
    "<div class=\"panel panel-primary panel-single\">"
    "<div class=\"panel-heading\">Smart ED BMS Cell Capacity</div>"
    "<div class=\"panel-body\">"
      "<div class=\"receiver\" id=\"livestatus\">"
        "<div id=\"container\" class=\"container\"></div>"
      "</div>"
    "</div>"
    "<div class=\"panel-footer\">"
    "<p></p>\n"
      "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
    "</div>\n"
    "</div>\n"
    "\n"
    "<style>\n"
    ".outer {\n"
      "width: 600px;\n"
      "height: 200px;\n"
      "margin: 1em auto;\n"
    "}\n"
    ".container {\n"
      "min-width: 300px;\n"
      "width: 100%;\n"
      "max-width: 100%;\n"
      "height: 45vh;\n"
      "min-height: 280px;\n"
      "margin: 1em auto;\n"
    "}\n"
    "@media (max-width: 600px) {\n"
      ".outer {\n"
        "width: 100%%;\n"
        "height: 400px;\n"
      "}\n"
      ".container {\n"
        "min-width: 300px;\n"
        "width: 100%;\n"
        "height: 45vh;\n"
        "min-height: 280px;\n"
        "margin: 1em auto;\n"
      "}\n"
    "}\n"
    "</style>\n"
    "\n"
    "<script>\n"
    "(function(){\n"
    "var cells = [];\n"
    "var cnt = metrics[\"xse.v.b.c.capacity\"] ? metrics[\"xse.v.b.c.capacity\"].length : 0;\n"
    "for (i=0; i<cnt; i++) {\n"
      "cells.push(i+1);\n"
    "}\n"
    "$(\"#container\").chart({\n"
        "chart: { type: 'column', zoomType: 'xy' },\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "xAxis: {\n"
          "categories: cells,\n"
          "crosshair: true\n"
        "},\n"
        "yAxis: {\n"
          "min: 10000,\n"
          "title: {\n"
            "text: 'Capacity As/10'\n"
          "}\n"
        "},\n"
        "tooltip: {\n"
          "headerFormat: 'Cell #{point.key} <table>',\n"
          "pointFormatter: function () {\n"
          "var amps = this.y/360.0;\n"
          "return '<tr><td>Capacity: </td>' +\n"
            "'<td style=\"padding:0\"><b>' + this.y + ' As/10</b></td></tr>' +\n"
            "'<tr><td>Amps: </td>' +\n"
            "'<td style=\"padding:0\"><b>' + amps.toFixed(1) + ' Ah</b></td></tr>';\n"
          "},\n"
          "footerFormat: '</table>',\n"
          "shared: true,\n"
          "useHTML: true\n"
        "},\n"
        "plotOptions: {\n"
          "column: {\n"
            "pointPadding: 0.2,\n"
            "borderWidth: 0\n"
          "}\n"
        "},\n"
        "series: [{\n"
          "name: 'Capacity',\n"
          "data: metrics[\"xse.v.b.c.capacity\"]\n"
        "}]\n"
      "});\n"
    "})();\n"
    "</script>\n");
  c.done();
}

/**
 * WebCfgEco: display eco scores (like old dead smart home page)
 */
void OvmsVehicleSmartED::WebCfgEco(PageEntry_t& p, PageContext_t& c)
{
   c.head(200);
   c.printf(
    "<div class=\"panel panel-primary panel-single\">"
    "<div class=\"panel-heading\">smart ED ECO Scores</div>"
    "<div class=\"panel-body\">"
      "<div class=\"receiver\" id=\"livestatus\">"
        "<div id=\"container-ECO\" class=\"container\"></div>"
        "<div id=\"container-acceleration\" class=\"container\"></div>"
        "<div id=\"container-constant\" class=\"container\"></div>"
        "<div id=\"container-coasting\" class=\"container\"></div>"
      "</div>"
    "</div>"
    "<div class=\"panel-footer\">"
    "<p>The Eco display is intended to help you optimize your driving style.\n"
    "It is used to reduce your vehicle's energy consumption and increase its range.\n"
    "The calculated Eco value indicates, in percentage terms, if and to what extent your driving style differs from the ideal driving style (100%%).</p>\n"
    "<p>Note: After a prolonged period of disuse, the Eco display starts with a value of 50%%.</p>\n"
      "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
    "</div>\n"
    "</div>\n"
    "\n"
    "<style>\n"
    "\n"
    ".outer {\n"
        "width: 600px;\n"
        "height: 200px;\n"
        "margin: 0 auto;\n"
      "}\n"
      ".container {\n"
        "width: 300px;\n"
        "float: left;\n"
        "height: 200px;\n"
      "}\n"
      ".highcharts-yaxis-grid .highcharts-grid-line {\n"
        "display: none;\n"
      "}\n"
      "@media (max-width: 600px) {\n"
        ".outer {\n"
          "width: 100%%;\n"
          "height: 400px;\n"
        "}\n"
        ".container {\n"
          "width: 300px;\n"
          "float: none;\n"
          "margin: 0 auto;\n"
        "}\n"
        "</style>\n"
    "\n"
    "<script type=\"text/javascript\">\n"
    "var gaugeOptions;\n"
    "var chartSpeed;\n"
    "var chartAccel;\n"
    "var chartConst;\n"
    "var chartCoast;\n"
    "\n"
    "function init_Gauges() {\n"
    "gaugeOptions = {\n"
    "\n"
        "chart: {\n"
            "type: 'solidgauge'\n"
        "},\n"
        "\n"
        "title: null,\n"
        "\n"
        "pane: {\n"
            "center: ['50%%', '85%%'],\n"
            "size: '140%%',\n"
            "startAngle: -90,\n"
            "endAngle: 90,\n"
            "background: {\n"
                "backgroundColor: (Highcharts.theme && Highcharts.theme.background2) || '#EEE',\n"
                "innerRadius: '60%%',\n"
                "outerRadius: '100%%',\n"
                "shape: 'arc'\n"
            "}\n"
        "},\n"
        "\n"
        "tooltip: {\n"
            "enabled: false\n"
        "},\n"
        "\n"
        "yAxis: {\n"
            "stops: [\n"
                "[0.1, '#DF5353'],\n"
                "[0.5, '#DDDF0D'],\n"
                "[0.9, '#55BF3B'],\n"
            "],\n"
            "lineWidth: 0,\n"
            "minorTickInterval: null,\n"
            "tickAmount: 2,\n"
            "title: {\n"
                "y: -70\n"
            "},\n"
            "labels: {\n"
                "y: 16\n"
            "}\n"
        "},\n"
        "\n"
        "plotOptions: {\n"
            "solidgauge: {\n"
                "dataLabels: {\n"
                    "y: 5,\n"
                    "borderWidth: 0,\n"
                    "useHTML: true\n"
                "}\n"
            "}\n"
        "}\n"
    "}};\n"
    "\n"
    "function init_chartEcoScore() {\n"
    "chartSpeed = Highcharts.chart('container-ECO', Highcharts.merge(gaugeOptions, {\n"
    "yAxis: {\n"
        "min: 0,\n"
        "max: 100,\n"
        "title: {\n"
            "text: 'ECO display'\n"
        "},\n"
    "},\n"
         "subtitle: {\n"
            "text: 'mean value',\n"
            "verticalAlign: 'bottom',\n"
            "floating: true,\n"
        "},\n"
    "\n"
    "credits: {\n"
        "enabled: false\n"
    "},\n"
    "\n"
    "series: [{\n"
        "name: 'Eco',\n"
        "data: [metrics[\"xse.v.display.ecoscore\"]],\n"
        "dataLabels: {\n"
            "format: '<div style=\"text-align:center\"><span style=\"font-size:25px;color:' +"
                "((Highcharts.theme && Highcharts.theme.contrastTextColor) || 'black') + '\">{y}</span><br/>' +"
                   "'<span style=\"font-size:12px;color:silver\">%%</span></div>'\n"
        "},\n"
        "tooltip: {\n"
            "valueSuffix: ' %%'\n"
        "}\n"
    "}]\n"
"}))};\n"
  "\n"
    "function init_chartAccel() {\n"
    "\n"
    "chartAccel = Highcharts.chart('container-acceleration', Highcharts.merge(gaugeOptions, {\n"
    "yAxis: {\n"
        "min: 0,\n"
        "max: 100,\n"
        "title: {\n"
            "text: 'Acceleration'\n"
        "},\n"
    "},\n"
        "subtitle: {\n"
            "text: 'moderate',\n"
            "verticalAlign: 'bottom',\n"
            "floating: true,\n"
        "},\n"
    "\n"
    "credits: {\n"
        "enabled: false\n"
    "},\n"
    "\n"
    "series: [{\n"
        "name: 'Acceleration',\n"
        "data: [metrics[\"xse.v.display.accel\"]],\n"
        "dataLabels: {\n"
            "format: '<div style=\"text-align:center\"><span style=\"font-size:25px;color:' +"
                "((Highcharts.theme && Highcharts.theme.contrastTextColor) || 'black') + '\">{y}</span><br/>' +"
                   "'<span style=\"font-size:12px;color:silver\">%%</span></div>'\n"
        "},\n"
        "tooltip: {\n"
            "valueSuffix: ' %%'\n"
        "}\n"
    "}]\n"
"}))};\n"
  "\n"
    "function init_chartConst() {\n"
    "\n"
    "chartConst = Highcharts.chart('container-constant', Highcharts.merge(gaugeOptions, {\n"
    "yAxis: {\n"
        "min: 0,\n"
        "max: 100,\n"
        "title: {\n"
            "text: 'Steady Driving'\n"
        "},\n"
    "},\n"
        "subtitle: {\n"
            "text: 'uniform',\n"
            "verticalAlign: 'bottom',\n"
            "floating: true,\n"
        "},\n"
    "\n"
    "credits: {\n"
        "enabled: false\n"
    "},\n"
    "\n"
    "series: [{\n"
        "name: 'Steady Driving',\n"
        "data: [metrics[\"xse.v.display.const\"]],\n"
        "dataLabels: {\n"
            "format: '<div style=\"text-align:center\"><span style=\"font-size:25px;color:' +"
                "((Highcharts.theme && Highcharts.theme.contrastTextColor) || 'black') + '\">{y}</span><br/>' +"
                   "'<span style=\"font-size:12px;color:silver\">%%</span></div>'\n"
        "},\n"
        "tooltip: {\n"
            "valueSuffix: ' %%'\n"
        "}\n"
    "}]\n"
"}))};\n"
  "\n"
    "function init_chartCoast() {\n"
    "\n"
    "chartCoast = Highcharts.chart('container-coasting', Highcharts.merge(gaugeOptions, {\n"
    "yAxis: {\n"
        "min: 0,\n"
        "max: 100,\n"
        "title: {\n"
            "text: 'Coasting'\n"
        "},\n"
    "},\n"
        "subtitle: {\n"
            "text: 'anticipatory',\n"
            "verticalAlign: 'bottom',\n"
            "floating: true,\n"
        "},\n"
    "\n"
    "credits: {\n"
        "enabled: false\n"
    "},\n"
    "\n"
    "series: [{\n"
        "name: 'Coasting',\n"
        "data: [metrics[\"xse.v.display.coast\"]],\n"
        "dataLabels: {\n"
            "format: '<div style=\"text-align:center\"><span style=\"font-size:25px;color:' +"
                "((Highcharts.theme && Highcharts.theme.contrastTextColor) || 'black') + '\">{y}</span><br/>' +"
                   "'<span style=\"font-size:12px;color:silver\">%%</span></div>'\n"
        "},\n"
        "tooltip: {\n"
            "valueSuffix: ' %%'\n"
        "}\n"
    "}]\n"
"}))};\n"
    "\n"
    "\n"
    "function init_charts() {\n"
      "init_Gauges();\n"
      "init_chartEcoScore();\n"
      "init_chartAccel();\n"
      "init_chartConst();\n"
      "init_chartCoast();\n"
    "}\n"
    "\n"
    "if (window.Highcharts) {\n"
      "init_charts();\n"
    "} else {\n"
      "$.ajax({\n"
        "url: \"/assets/charts.js\",\n"
        "dataType: \"script\",\n"
        "cache: true,\n"
        "success: function(){ init_charts(); }\n"
      "});\n"
    "}\n"
    "\n"
        "</script>\n");
c.done();
}

/**
 * GetDashboardConfig: smart ED specific dashboard setup
 */
void OvmsVehicleSmartED::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 145,"
      "plotBands: ["
        "{ from: 0, to: 70, className: 'green-band' },"
        "{ from: 70, to: 100, className: 'yellow-band' },"
        "{ from: 100, to: 145, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 260, max: 400,"
      "plotBands: ["
        "{ from: 260, to: 305, className: 'red-band' },"
        "{ from: 305, to: 355, className: 'yellow-band' },"
        "{ from: 355, to: 400, className: 'green-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency:
      "min: 0, max: 300,"
      "plotBands: ["
        "{ from: 0, to: 150, className: 'green-band' },"
        "{ from: 150, to: 250, className: 'yellow-band' },"
        "{ from: 250, to: 300, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -30, max: 30,"
      "plotBands: ["
        "{ from: -30, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 15, className: 'green-band' },"
        "{ from: 15, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 30, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 80, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 25,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 50, className: 'normal-band border' },"
        "{ from: 50, to: 65, className: 'red-band border' }]"
    "},{"
      // Inverter temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 125, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 110, className: 'normal-band border' },"
        "{ from: 110, to: 125, className: 'red-band border' }]"
    "}]";
}

#endif //CONFIG_OVMS_COMP_WEBSERVER