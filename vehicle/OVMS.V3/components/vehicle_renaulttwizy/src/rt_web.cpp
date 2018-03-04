/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy Webserver
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

// #include "ovms_log.h"
// static const char *TAG = "v-twizy";

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_renaulttwizy.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleRenaultTwizy::WebInit()
{
  MyWebServer.RegisterPage("/xrt/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/battery", "Battery", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/drivemode", "Drivemode", WebConsole, PageMenu_Main, PageAuth_Cookie);
}


/**
 * WebCfgFeatures: configure general parameters (URL /xrt/config)
 */
void OvmsVehicleRenaultTwizy::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  bool canwrite, autopower, autoreset, console, kickdown;
  std::string kd_compzero, kd_threshold, gpslogint;

  if (c.method == "POST") {
    // process form submission:
    canwrite = (c.getvar("canwrite") == "yes");
    autoreset = (c.getvar("autoreset") == "yes");
    kickdown = (c.getvar("kickdown") == "yes");
    autopower = (c.getvar("autopower") == "yes");
    console = (c.getvar("console") == "yes");
    kd_threshold = c.getvar("kd_threshold");
    kd_compzero = c.getvar("kd_compzero");
    gpslogint = c.getvar("gpslogint");
    
    // check:
    if (!kd_threshold.empty()) {
      int n = atoi(kd_threshold.c_str());
      if (n < 0 || n > 250)
        error += "<li data-input=\"kd_threshold\">Kickdown threshold out of range (0…250)</li>";
    }
    if (!kd_compzero.empty()) {
      int n = atoi(kd_compzero.c_str());
      if (n < 0 || n > 250)
        error += "<li data-input=\"kd_compzero\">Kickdown compensation out of range (0…250)</li>";
    }
    if (!gpslogint.empty()) {
      int n = atoi(gpslogint.c_str());
      if (n < 0)
        error += "<li data-input=\"gpslogint\">GPS log interval must be zero or positive</li>";
    }
    
    if (error == "") {
      // store:
      MyConfig.SetParamValueBool("xrt", "canwrite", canwrite);
      MyConfig.SetParamValueBool("xrt", "autoreset", autoreset);
      MyConfig.SetParamValueBool("xrt", "kickdown", kickdown);
      MyConfig.SetParamValueBool("xrt", "autopower", autopower);
      MyConfig.SetParamValueBool("xrt", "console", console);
      MyConfig.SetParamValue("xrt", "kd_threshold", kd_threshold);
      MyConfig.SetParamValue("xrt", "kd_compzero", kd_compzero);
      MyConfig.SetParamValue("xrt", "gpslogint", gpslogint);
      
      c.head(200);
      c.alert("success", "<p class=\"lead\">Twizy feature configuration saved.</p>");
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
    canwrite = MyConfig.GetParamValueBool("xrt", "canwrite", false);
    autoreset = MyConfig.GetParamValueBool("xrt", "autoreset", true);
    kickdown = MyConfig.GetParamValueBool("xrt", "kickdown", true);
    autopower = MyConfig.GetParamValueBool("xrt", "autopower", true);
    console = MyConfig.GetParamValueBool("xrt", "console", false);
    kd_threshold = MyConfig.GetParamValue("xrt", "kd_threshold", STR(CFG_DEFAULT_KD_THRESHOLD));
    kd_compzero = MyConfig.GetParamValue("xrt", "kd_compzero", STR(CFG_DEFAULT_KD_COMPZERO));
    gpslogint = MyConfig.GetParamValue("xrt", "gpslogint", "0");
    
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Twizy feature configuration");
  c.form_start(p.uri);
  
  c.fieldset_start("General");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, all control functions depend on this.</p>");
  c.input_checkbox("Enable emergency reset", "autoreset", autoreset,
    "<p>Enables SEVCON tuning reset by pushing 3x DNR in STOP.</p>");
  c.input_checkbox("Enable auto power adjustment", "autopower", autopower);
  c.input_checkbox("SimpleConsole connected", "console", console);
  c.fieldset_end();
  
  c.fieldset_start("Kickdown");
  c.input_checkbox("Enable kickdown", "kickdown", kickdown);
  c.input_slider("Sensitivity", "kd_threshold", 3, NULL,
    -1, atof(kd_threshold.c_str()), 35, 0, 250, 1,
    "<p>Default 35, lower value = higher overall sensitivity</p>");
  c.input_slider("Compensation", "kd_compzero", 3, NULL,
    -1, atof(kd_compzero.c_str()), 120, 0, 250, 1,
    "<p>Default 120, lower value = higher sensitivity at high speeds</p>");
  c.fieldset_end();
  
  c.fieldset_start("Telemetry");
  c.input("number", "GPS log interval", "gpslogint", gpslogint.c_str(), "Default: 0 = off",
    "<p>0 = no GPS log, else interval in seconds. Note: only applies while driving.</p>",
    "min=\"0\" step=\"1\"", "s");
  // todo: SDO log
  c.fieldset_end();
  
  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * WebCfgBattery: configure battery parameters (URL /xrt/battery)
 */
void OvmsVehicleRenaultTwizy::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string cap_nom_ah, cap_act_prc, maxrange, chargelevel, chargemode, suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    cap_nom_ah = c.getvar("cap_nom_ah");
    cap_act_prc = c.getvar("cap_act_prc");
    maxrange = c.getvar("maxrange");
    chargelevel = c.getvar("chargelevel");
    chargemode = c.getvar("chargemode");
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");
    
    // check:
    if (!cap_nom_ah.empty()) {
      float n = atof(cap_nom_ah.c_str());
      if (n < 1)
        error += "<li data-input=\"cap_nom_ah\">Nominal Ah capacity must be &ge; 1</li>";
    }
    if (!cap_act_prc.empty()) {
      float n = atof(cap_act_prc.c_str());
      if (n < 1 || n > 100)
        error += "<li data-input=\"cap_act_prc\">Actual capacity percentage invalid (range 1…100)</li>";
    }
    if (!maxrange.empty()) {
      float n = atof(maxrange.c_str());
      if (n < 1)
        error += "<li data-input=\"maxrange\">Maximum range invalid, must be &ge; 1</li>";
    }
    if (!suffrange.empty()) {
      float n = atof(suffrange.c_str());
      if (n < 0)
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
    }
    if (!suffsoc.empty()) {
      float n = atof(suffsoc.c_str());
      if (n < 0)
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
    }
    
    if (error == "") {
      // store:
      MyConfig.SetParamValue("xrt", "cap_nom_ah", cap_nom_ah);
      MyConfig.SetParamValue("xrt", "cap_act_prc", cap_act_prc);
      MyConfig.SetParamValue("xrt", "maxrange", maxrange);
      MyConfig.SetParamValue("xrt", "chargelevel", chargelevel);
      MyConfig.SetParamValue("xrt", "chargemode", chargemode);
      MyConfig.SetParamValue("xrt", "suffrange", suffrange);
      MyConfig.SetParamValue("xrt", "suffsoc", suffsoc);
      
      c.head(200);
      c.alert("success", "<p class=\"lead\">Twizy battery setup saved.</p>");
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
    cap_nom_ah = MyConfig.GetParamValue("xrt", "cap_nom_ah", STR(CFG_DEFAULT_CAPACITY));
    cap_act_prc = MyConfig.GetParamValue("xrt", "cap_act_prc", "100");
    maxrange = MyConfig.GetParamValue("xrt", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
    chargelevel = MyConfig.GetParamValue("xrt", "chargelevel", "0");
    chargemode = MyConfig.GetParamValue("xrt", "chargemode", "0");
    suffrange = MyConfig.GetParamValue("xrt", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xrt", "suffsoc", "0");
    
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Twizy battery setup");
  c.form_start(p.uri);
  
  c.fieldset_start("Battery properties");
  
  c.input("number", "Nominal capacity", "cap_nom_ah", cap_nom_ah.c_str(), "Default: " STR(CFG_DEFAULT_CAPACITY),
    "<p>This is the usable capacity of your battery when new.</p>",
    "min=\"1\" step=\"0.1\"", "Ah");
  c.input("number", "Actual capacity", "cap_act_prc", cap_act_prc.c_str(), "Default: 100",
    NULL, "min=\"1\" max=\"100\" step=\"0.01\"", "%");
  
  c.input("number", "Maximum drive range", "maxrange", maxrange.c_str(), "Default: " STR(CFG_DEFAULT_MAXRANGE),
    "<p>The range you normally get at 100% SOC and 20 °C.</p>",
    "min=\"1\" step=\"1\"", "km");
  
  c.fieldset_end();
  
  c.fieldset_start("Charge control");
  
  c.input_select_start("Charge power limit", "chargelevel");
  c.input_select_option("&mdash;", "0", chargelevel == "0");
  c.input_select_option( "400W", "1", chargelevel == "1");
  c.input_select_option( "700W", "2", chargelevel == "2");
  c.input_select_option("1050W", "3", chargelevel == "3");
  c.input_select_option("1400W", "4", chargelevel == "4");
  c.input_select_option("1750W", "5", chargelevel == "5");
  c.input_select_option("2100W", "6", chargelevel == "6");
  c.input_select_option("2300W", "7", chargelevel == "7");
  c.input_select_end();

  c.input_radio_start("Charge limit mode", "chargemode");
  c.input_radio_option("chargemode", "Notify", "0", chargemode == "0");
  c.input_radio_option("chargemode", "Stop", "1", chargemode == "1");
  c.input_radio_end();
  
  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, 200, 1,
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
}


/**
 * WebConsole: (/xrt/drivemode)
 */
void OvmsVehicleRenaultTwizy::WebConsole(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance();
  SevconClient* sc = twizy ? twizy->GetSevconClient() : NULL;
  if (!sc) {
    c.error(404, "Twizy/SEVCON module not loaded");
    return;
  }
  
  if (c.method == "POST") {
    std::string output, arg;
    
    if ((arg = c.getvar("load")) != "") {
      // execute profile switch:
      output = MyWebServer.ExecuteCommand("xrt cfg load " + arg);
      output += MyWebServer.ExecuteCommand("xrt cfg info");
      
      // output result:
      c.head(200);
      c.printf("%s", _html(output));
      c.printf(
        "<script>"
          "$('#loadmenu btn').removeClass('base unsaved btn-warning').addClass('btn-default');"
          "$('#prof-%d').removeClass('btn-default').addClass('btn-warning %s');"
          , sc->m_drivemode.profile_user
          , sc->m_drivemode.unsaved ? "unsaved" : "");
      if (sc->m_drivemode.profile_user != sc->m_drivemode.profile_cfgmode) {
        c.printf(
          "$('#prof-%d').addClass('base');", sc->m_drivemode.profile_cfgmode);
      }
      c.printf(
        "</script>");
      c.done();
      return;
    }
    else {
      c.error(400, "Bad request");
      return;
    }
  }
  
  // output status page:
  
  ostringstream buf;
  
  buf << "Active profile: ";
  if (sc->m_drivemode.profile_user == sc->m_drivemode.profile_cfgmode) {
    buf << "#" << sc->m_drivemode.profile_user;
  }
  else {
    buf << "base #" << sc->m_drivemode.profile_cfgmode;
    buf << ", live #" << sc->m_drivemode.profile_user;
  }
  
  buf << "\n\n" << MyWebServer.ExecuteCommand("xrt cfg info");
  
  // profile labels (todo: make configurable):
  const char* proflabel[4] = { "STD", "PWR", "ECO", "ICE" };
  
  c.head(200);
  c.printf(
    "<style>"
    ".btn-default.base { background-color: #fffca8; }"
    ".btn-default.base:hover, .btn-default.base:focus { background-color: #fffa62; }"
    ".unsaved > *:after { content: \"*\"; }"
    "</style>");
  c.panel_start("primary", "Drivemode");
  c.printf(
    "<samp id=\"loadres\">%s</samp>", _html(buf.str()));
  c.printf(
    "<div id=\"loadmenu\" class=\"center-block\"><ul class=\"list-inline\">");
  for (int prof=0; prof<=3; prof++) {
    c.printf(
      "<li><a id=\"prof-%d\" class=\"btn btn-lg btn-%s %s%s\" data-method=\"post\" target=\"#loadres\" href=\"?load=%d\"><strong>%s</strong></a></li>"
      , prof
      , (sc->m_drivemode.profile_user == prof) ? "warning" : "default"
      , (sc->m_drivemode.profile_cfgmode == prof) ? "base" : ""
      , (sc->m_drivemode.profile_user == prof && sc->m_drivemode.unsaved) ? "unsaved" : ""
      , prof
      , proflabel[prof]
      );
  }
  c.printf(
    "</ul></div>");
  c.panel_end();
  c.done();
}
