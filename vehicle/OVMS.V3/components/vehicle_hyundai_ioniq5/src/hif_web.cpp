/**
 * Project:      Open Vehicle Monitor System
 * Module:       Hyundai Ioniq Webserver
 *
 * (c) 2022 Michael Geddes <frog@bunyip.wheelycreek.net>
 *
 * ----- Kona/Kia Module -----
 * (c) 2019 Geir Øyvind Væidalo <geir@validalo.net>
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

//static const char *TAG = "v-kianiroev";
#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_hyundai_ioniq5.h"
#include "../../vehicle_kiasoulev/src/kia_common.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsHyundaiIoniqEv::WebInit()
{
  XARM("OvmsHyundaiIoniqEv::WebInit");
  // vehicle menu:
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/Auxbattery", "Aux battery monitor", WebAuxBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/battery", "Battery config", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  XDISARM;
}


/**
 * WebCfgFeatures: configure general parameters (URL /xkn/config)
 */
void OvmsHyundaiIoniqEv::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  XARM("OvmsHyundaiIoniqEv::WebCfgFeatures");
  std::string error;
#ifdef XIQ_CAN_WRITE
  bool canwrite;
#endif
  bool consoleKilometers;
  bool leftDrive;

  if (c.method == "POST") {
    // process form submission:
#ifdef XIQ_CAN_WRITE
    canwrite = (c.getvar("canwrite") == "yes");
#endif
    consoleKilometers = (c.getvar("consoleKilometers") == "yes");
    leftDrive = (c.getvar("leftDrive") == "yes");

    if (error == "") {
      // store:
#ifdef XIQ_CAN_WRITE
      MyConfig.SetParamValueBool("xiq", "canwrite", canwrite);
#endif
      MyConfig.SetParamValueBool("xiq", "consoleKilometers", consoleKilometers);
      MyConfig.SetParamValueBool("xiq", "leftDrive", leftDrive);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Hyundai Ioniq EV feature configuration saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      XDISARM;
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
#ifdef XIQ_CAN_WRITE
    canwrite = MyConfig.GetParamValueBool("xiq", "canwrite", false);
#endif
    consoleKilometers = MyConfig.GetParamValueBool("xiq", "consoleKilometers", true);
    leftDrive = MyConfig.GetParamValueBool("xiq", "leftDrive", true);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Hyundai Ioniq EV feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("General");
#ifdef XIQ_CAN_WRITE
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, all control functions depend on this.</p>");
#endif
  c.input_checkbox("Console odometer in Kilometers", "consoleKilometers", consoleKilometers,
    "<p>Enable for cars with console odometer in Kilometers, disable for Miles</p>");
  c.input_checkbox("Left hand drive", "leftDrive", leftDrive,
    "<p>Enable for left hand drive cars, disable for right hand drive.</p>");
  c.fieldset_end();

  //c.fieldset_start("Functionality");
  //c.input_checkbox("Enable open charge port with key fob", "remote_charge_port", remote_charge_port,
  //  "<p>Enable using the Hold-button on the remote key fob to open up the charge port.</p>");
  //c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  XDISARM;
}


/**
 * WebCfgBattery: configure battery parameters (URL /xkn/battery)
 */
void OvmsHyundaiIoniqEv::WebCfgBattery(PageEntry_t &p, PageContext_t &c)
{
  XARM("OvmsHyundaiIoniqEv::WebCfgBattery");
  std::string error;
  //    cap_act_kwh     Battery capacity in wH (Default: 640000)
  //  suffsoc           Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange         Sufficient range [km] (Default: 0=disabled)
  //  maxrange          Maximum ideal range at 20 °C [km] (Default: 160)
  std::string cap_act_kwh, maxrange, suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    cap_act_kwh = c.getvar("cap_act_kwh");
    maxrange = c.getvar("maxrange");
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");

    // check:
    if (!cap_act_kwh.empty()) {
      float n = atof(cap_act_kwh.c_str());
      if (n < 1) {
        error += "<li data-input=\"cap_act_kwh\">Battery Wh capacity must be &ge; 1</li>";
      }
    }
    if (!maxrange.empty()) {
      float n = atof(maxrange.c_str());
      if (n < 1) {
        error += "<li data-input=\"maxrange\">Maximum range invalid, must be &ge; 1</li>";
      }
    }
    if (!suffrange.empty()) {
      float n = atof(suffrange.c_str());
      if (n < 0) {
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
      }
    }
    if (!suffsoc.empty()) {
      float n = atof(suffsoc.c_str());
      if (n < 0) {
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
      }
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xiq", "cap_act_kwh", cap_act_kwh);
      MyConfig.SetParamValue("xiq", "maxrange", maxrange);
      MyConfig.SetParamValue("xiq", "suffrange", suffrange);
      MyConfig.SetParamValue("xiq", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Hyundai Ioniq battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      XDISARM;
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    cap_act_kwh = MyConfig.GetParamValue("xiq", "cap_act_kwh", STR(CGF_DEFAULT_BATTERY_CAPACITY));
    maxrange = MyConfig.GetParamValue("xiq", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
    suffrange = MyConfig.GetParamValue("xiq", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xiq", "suffsoc", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Hyundai Ioniq EV battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery properties");

  c.input("number", "Battery capacity", "cap_nom_ah", cap_act_kwh.c_str(), "Default: " STR(CGF_DEFAULT_BATTERY_CAPACITY),
    "<p>This is the usable battery capacity of your battery when new.</p>",
    "min=\"1\" step=\"0.1\"", "Wh");

  c.input("number", "Maximum drive range", "maxrange", maxrange.c_str(), "Default: " STR(CFG_DEFAULT_MAXRANGE),
    "<p>The range you normally get at 100% SOC and 20 °C.</p>",
    "min=\"1\" step=\"1\"", "km");

  c.fieldset_end();

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");
  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  XDISARM;
}

/**
 * GetDashboardConfig: Hyundai Ioniq specific dashboard setup
 */
void OvmsHyundaiIoniqEv::GetDashboardConfig(DashboardConfig &cfg)
{
  XARM("OvmsHyundaiIoniqEv::GetDashboardConfig");
  cfg.gaugeset1 =
    "yAxis: [{"
    // Speed:
    "min: 0, max: 170,"
    "plotBands: ["
    "{ from: 0, to: 70, className: 'green-band' },"
    "{ from: 70, to: 120, className: 'yellow-band' },"
    "{ from: 120, to: 170, className: 'red-band' }]"
    "},{"
    // Voltage:
    "min: 300, max: 405,"
    "plotBands: ["
    "{ from: 330, to: 360, className: 'red-band' },"
    "{ from: 360, to: 380, className: 'yellow-band' },"
    "{ from: 380, to: 405, className: 'green-band' }]"
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
    "{ from: -10, to: 0, className: 'violet-band' },"
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
  XDISARM;
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
