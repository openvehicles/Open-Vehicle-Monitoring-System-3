/**
 * Project:      Open Vehicle Monitor System
 * Module:       Nissan Leaf Webserver
 *
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
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


#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_nissanleaf.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleNissanLeaf::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xnl/features", "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xnl/battery",  "Battery config",   WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/bms/cellmon",  "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleNissanLeaf::WebDeInit()
{
  MyWebServer.DeregisterPage("/xnl/features");
  MyWebServer.DeregisterPage("/xnl/battery");
  MyWebServer.DeregisterPage("/bms/cellmon");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xnl/config)
 */
void OvmsVehicleNissanLeaf::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  bool canwrite;
  bool socnewcar;
  bool sohnewcar;
  std::string modelyear;
  std::string cabintempoffset;
  std::string maxgids;
  std::string newcarah;

  if (c.method == "POST") {
    // process form submission:
    modelyear       = c.getvar("modelyear");
    cabintempoffset = c.getvar("cabintempoffset");
    maxgids         = c.getvar("maxgids");
    newcarah        = c.getvar("newcarah");
    socnewcar       = (c.getvar("socnewcar") == "yes");
    sohnewcar       = (c.getvar("sohnewcar") == "yes");
    canwrite        = (c.getvar("canwrite") == "yes");

    // check:
    if (!modelyear.empty()) {
      int n = atoi(modelyear.c_str());
      if (n < 2011)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2011</li>";
    }
    
    if (cabintempoffset.empty()) {
      error += "<li data-input=\"cabintempoffset\">Cabin Temperature Offset can not be empty</li>";
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xnl", "modelyear", modelyear);
      MyConfig.SetParamValue("xnl", "cabintempoffset", cabintempoffset);
      MyConfig.SetParamValue("xnl", "maxGids",   maxgids);
      MyConfig.SetParamValue("xnl", "newCarAh",  newcarah);
      MyConfig.SetParamValueBool("xnl", "soc.newcar", socnewcar);
      MyConfig.SetParamValueBool("xnl", "soh.newcar", sohnewcar);
      MyConfig.SetParamValueBool("xnl", "canwrite",   canwrite);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf feature configuration saved.</p>");
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
    modelyear       = MyConfig.GetParamValue("xnl", "modelyear", STR(DEFAULT_MODEL_YEAR));
    cabintempoffset = MyConfig.GetParamValue("xnl", "cabintempoffset", STR(DEFAULT_CABINTEMP_OFFSET));
    maxgids         = MyConfig.GetParamValue("xnl", "maxGids", STR(GEN_1_NEW_CAR_GIDS));
    newcarah        = MyConfig.GetParamValue("xnl", "newCarAh", STR(GEN_1_NEW_CAR_AH));
    socnewcar       = MyConfig.GetParamValueBool("xnl", "soc.newcar", false);
    sohnewcar       = MyConfig.GetParamValueBool("xnl", "soh.newcar", false);
    canwrite        = MyConfig.GetParamValueBool("xnl", "canwrite", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("General");
  c.input_radio_start("SOC Display", "socnewcar");
  c.input_radio_option("socnewcar", "from dashboard display",   "no",  socnewcar == false);
  c.input_radio_option("socnewcar", "relative to fixed value:", "yes", socnewcar == true);
  c.input_radio_end("");
  c.input("number", NULL, "maxgids", maxgids.c_str(), "Default: " STR(GEN_1_NEW_CAR_GIDS),
      "<p>Enter the maximum GIDS value when fully charged. Default values are " STR(GEN_1_NEW_CAR_GIDS) " (24kWh) or " STR(GEN_1_30_NEW_CAR_GIDS) " (30kWh) or " STR(GEN_2_40_NEW_CAR_GIDS) " (40kWh)</p>",
      "min=\"1\" step=\"1\"", "GIDS");

  c.input_radio_start("SOH Display", "sohnewcar");
  c.input_radio_option("sohnewcar", "from dashboard display",   "no",  sohnewcar == false);
  c.input_radio_option("sohnewcar", "relative to fixed value:", "yes", sohnewcar == true);
  c.input_radio_end("");
  c.input("number", NULL, "newcarah", newcarah.c_str(), "Default: " STR(GEN_1_NEW_CAR_AH),
      "<p>This is the usable capacity of your battery when new. Default values are " STR(GEN_1_NEW_CAR_AH) " (24kWh) or " STR(GEN_1_30_NEW_CAR_AH) " (30kWh) or " STR(GEN_2_40_NEW_CAR_AH) " (40kWh)</p>",
      "min=\"1\" step=\"1\"", "Ah");
  c.input("number", "Cabin Temperature Offset", "cabintempoffset", cabintempoffset.c_str(), "Default: " STR(DEFAULT_CABINTEMP_OFFSET),
      "<p>This allows an offset adjustment to the cabin temperature sensor readings in Celcius.</p>",
      "step=\"0.1\"", "");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
    "<p>This determines the format of CAN write messages as it differs slightly between model years.</p>",
    "min=\"2011\" step=\"1\"", "");
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
void OvmsVehicleNissanLeaf::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  std::string suffrange, suffsoc, minrange, minsoc;

  if (c.method == "POST") {
    // process form submission:
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");
    minrange = c.getvar("minrange");
    minsoc = c.getvar("minsoc");

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
    if (!minrange.empty()) {
      float n = atof(minrange.c_str());
      if (n < 0)
        error += "<li data-input=\"minrange\">Minimum range invalid, must be &ge; 0</li>";
    }
    if (!minsoc.empty()) {
      float n = atof(minsoc.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"minsoc\">Minimum SOC invalid, must be 0…100</li>";
    }
    
    if (error == "") {
      // store:
      MyConfig.SetParamValue("xnl", "suffrange", suffrange);
      MyConfig.SetParamValue("xnl", "suffsoc", suffsoc);
      MyConfig.SetParamValue("xnl", "minrange", minrange);
      MyConfig.SetParamValue("xnl", "minsoc", minsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf battery setup saved.</p>");
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
    suffrange = MyConfig.GetParamValue("xnl", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xnl", "suffsoc", "0");
    minrange = MyConfig.GetParamValue("xnl", "minrange", "0");
    minsoc = MyConfig.GetParamValue("xnl", "minsoc", "0");
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();
  
  c.fieldset_start("V2X control");

  c.input_slider("Minimum range", "minrange", 3, "km",
    atof(minrange.c_str()) > 0, atof(minrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop discharge when reaching this level.</p>");

  c.input_slider("Minimum SOC", "minsoc", 3, "%",
    atof(minsoc.c_str()) > 0, atof(minsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop discharge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * GetDashboardConfig: Nissan Leaf specific dashboard setup
 */
void OvmsVehicleNissanLeaf::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 135,"
      "plotBands: ["
        "{ from: 0, to: 70, className: 'green-band' },"
        "{ from: 70, to: 100, className: 'yellow-band' },"
        "{ from: 100, to: 135, className: 'red-band' }]"
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
        "{ from: 0, to: 120, className: 'green-band' },"
        "{ from: 120, to: 250, className: 'yellow-band' },"
        "{ from: 250, to: 300, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -20, max: 50,"
      "plotBands: ["
        "{ from: -20, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 10, className: 'green-band' },"
        "{ from: 10, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 50, className: 'red-band' }]"
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
        "{ from: 0, to: 40, className: 'normal-band border' },"
        "{ from: 40, to: 65, className: 'red-band border' }]"
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
