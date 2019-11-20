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
  MyWebServer.RegisterPage("/xse/features",   "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/battery",    "Battery config",   WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  //MyWebServer.RegisterPage("/xse/brakelight", "Brake Light",      OvmsWebServer::HandleCfgBrakelight,  PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/cellmon",    "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/commands",   "Commands",         WebCfgCommands,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xse/notify",     "Notifys",          WebCfgNotify,                        PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleSmartED::WebDeInit()
{
  MyWebServer.DeregisterPage("/xse/features");
  MyWebServer.DeregisterPage("/xse/battery");
  //MyWebServer.DeregisterPage("/xse/brakelight");
  MyWebServer.DeregisterPage("/xse/cellmon");
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
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Smart ED feature configuration");
  c.form_start(p.uri);
  
  c.input("number", "Range Ideal", "rangeideal", rangeideal.c_str(), "Default: 135",
    "<p>This determines the Ideal Range.</p>",
    "min=\"90\" max=\"200\" step=\"1\"");
  
  c.input_checkbox("change Display SOC = SOH", "soc_rsoc", soc_rsoc,
    "<p>WARNING: change Displayed SOC to SOH and SOH to SOC.</p>");
  
  c.input_checkbox("Enable CAN write(Poll)", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  
  c.input_checkbox("Enable Door lock/unlock Status)", "lockstate", lockstate,
    "<p>Only needed when External door status wired to expansion.</p>");

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
      "<button class=\"btn btn-default\" data-cmd=\"xse recu up\" data-target=\"#output\" data-watchcnt=\"0\">Recu Up</button>\n"
      "<button class=\"btn btn-default\" data-cmd=\"xse recu down\" data-target=\"#output\" data-watchcnt=\"0\">Recu Down</button>\n"
      "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
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