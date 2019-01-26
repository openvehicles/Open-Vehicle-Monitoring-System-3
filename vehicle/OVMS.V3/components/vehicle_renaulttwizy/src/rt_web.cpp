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

#ifdef CONFIG_OVMS_COMP_WEBSERVER

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
  // vehicle menu:
  MyWebServer.RegisterPage("/xrt/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/profed", "Profile Editor", WebProfileEditor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/dmconfig", "Drivemode Config", WebDrivemodeConfig, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/battery", "Battery Config", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/scmon", "Sevcon Monitor", WebSevconMon, PageMenu_Vehicle, PageAuth_Cookie);

  // main menu:
  MyWebServer.RegisterPage("/xrt/drivemode", "Drivemode", WebConsole, PageMenu_Main, PageAuth_Cookie);

  // page callbacks:
  MyWebServer.RegisterCallback("xrt", "/dashboard", WebExtDashboard);
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
      if (n < 1 || n > 120)
        error += "<li data-input=\"cap_act_prc\">Actual capacity percentage invalid (range 1…120)</li>";
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
    NULL, "min=\"1\" max=\"120\" step=\"0.01\"", "%");
  
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

  c.input_radiobtn_start("Charge limit mode", "chargemode");
  c.input_radiobtn_option("chargemode", "Notify", "0", chargemode == "0");
  c.input_radiobtn_option("chargemode", "Stop", "1", chargemode == "1");
  c.input_radiobtn_end();
  
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

static void print_loadmenu_buttons(PageContext_t& c, cfg_drivemode drivemode)
{
  int prof;
  StringWriter pfx;
  std::string key, label, title;

  const char* default_label[4] = { "STD", "PWR", "ECO", "ICE" };
  const char* default_title[4] = { "Standard", "Power", "Economy", "Winter" };
  std::istringstream buttons(MyConfig.GetParamValue("xrt", "profile_buttons", "0,1,2,3"));

  while (std::getline(buttons, key, ','))
  {
    prof = atoi(key.c_str());
    if (prof < 0 || prof > 99) continue;
    pfx.clear();
    pfx.printf("profile%02d", prof);

    label = MyConfig.GetParamValue("xrt", pfx + ".label", (prof < 4) ? default_label[prof] : "");
    title = MyConfig.GetParamValue("xrt", pfx + ".title", (prof < 4) ? default_title[prof] : "");
    if (label.empty())
      label = "#" + pfx.substr(7);

    c.printf(
        "<div class=\"btn-group btn-group-lg\">\n"
          "<button type=\"submit\" name=\"load\" value=\"%d\" id=\"prof-%d\" class=\"btn btn-%s %s%s\" title=\"%s\">%s</button>\n"
        "</div>\n"
      , prof, prof
      , (drivemode.profile_user == prof) ? "warning" : "default"
      , (drivemode.profile_cfgmode == prof) ? "base" : ""
      , (drivemode.profile_user == prof && drivemode.unsaved) ? "unsaved" : ""
      , _attr(title)
      , _html(label)
      );
  }
}

void OvmsVehicleRenaultTwizy::WebConsole(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance();
  SevconClient* sc = twizy ? twizy->GetSevconClient() : NULL;
  if (!sc) {
    c.error(404, "Twizy/SEVCON module not loaded");
    return;
  }
  
  std::string arg;
  if ((arg = c.getvar("load")) != "") {
    // execute profile switch:
    std::string output;
    output = MyWebServer.ExecuteCommand("xrt cfg load " + arg);
    if (c.getvar("info") != "off")
      output += MyWebServer.ExecuteCommand("xrt cfg info");
    
    // output result:
    c.head(200);
    c.print(c.encode_html(output));
    c.printf(
      "<script>"
        "$('#loadmenu .btn').removeClass('base unsaved btn-warning').addClass('btn-default');"
        "$('#prof-%d').removeClass('btn-default').addClass('btn-warning %s');"
        , sc->m_drivemode.profile_user
        , sc->m_drivemode.unsaved ? "unsaved" : "");
    if (sc->m_drivemode.profile_user != sc->m_drivemode.profile_cfgmode) {
      c.printf(
        "$('#prof-%d').addClass('base');", sc->m_drivemode.profile_cfgmode);
    }
    c.print(
      "</script>");
    c.done();
    return;
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
  
  c.head(200);
  c.print(
    "<style>\n"
    "#loadmenu { margin: 20px 0; }\n"
    "#loadmenu .btn-group-lg>.btn, #loadmenu .btn-lg { padding: 10px 3px; overflow: hidden; font-weight: bold; }\n"
    "#loadmenu .btn-default.base { background-color: #fffca8; }\n"
    "#loadmenu .btn-default.base:hover,\n"
    "#loadmenu .btn-default.base:focus { background-color: #fffa62; }\n"
    "#loadmenu .unsaved > *:after { content: \"*\"; }\n"
    ".fullscreened .panel-single .panel-body { padding: 10px; }\n"
    "</style>\n");
  PAGE_HOOK("body.pre");
  
  c.panel_start("primary", "Drivemode");
  
  c.printf(
    "<samp id=\"loadres\">%s</samp>", _html(buf.str()));
  
  c.print(
    "<div id=\"livestatus\" class=\"receiver\">"
      "<div class=\"row\">"
        "<div class=\"col-sm-2\"><label>Throttle:</label></div>"
        "<div class=\"col-sm-10\">"
          "<div class=\"metric progress\" data-metric=\"v.e.throttle\" data-prec=\"0\">"
            "<div id=\"pb-throttle\" class=\"progress-bar progress-bar-success no-transition text-left value-low\" role=\"progressbar\""
              " aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
              "<div><span class=\"value\">0</span><span class=\"unit\">%</span></div>"
            "</div>"
          "</div>"
        "</div>"
      "</div>"
    "</div>");
  
  c.print(
    "<form id=\"loadmenu\" action=\"/xrt/drivemode\" target=\"#loadres\" method=\"post\">\n"
      "<div class=\"btn-group btn-group-justified btn-longtouch\">\n");
  print_loadmenu_buttons(c, sc->m_drivemode);
  c.print(
      "</div>\n"
    "</form>\n");

  c.panel_end(
    "<button type=\"button\" class=\"btn btn-sm btn-default action-profed\">Edit current Profile</button>\n"
    "<button type=\"button\" class=\"btn btn-sm btn-default action-dmconfig\">Configure Buttons</button>\n"
  );
  
  c.print(
    "<script>"
    "$(\"#livestatus\").on(\"msg:event\", function(e, event){"
      "if (event == \"vehicle.kickdown.engaged\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-success progress-bar-warning\").addClass(\"progress-bar-danger\");"
      "else if (event == \"vehicle.kickdown.releasing\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-success progress-bar-danger\").addClass(\"progress-bar-warning\");"
      "else if (event == \"vehicle.kickdown.released\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-warning progress-bar-danger\").addClass(\"progress-bar-success\");"
    "});"
    "$('.action-profed').on('click', function(ev) {\n"
      "loadPage(\"/xrt/profed?key=\" + (metrics[\"xrt.cfg.user\"]||0));\n"
    "});\n"
    "$('.action-dmconfig').on('click', function(ev) {\n"
      "loadPage(\"/xrt/dmconfig\");\n"
    "});\n"
    "</script>");
  
  PAGE_HOOK("body.post");
  c.done();
}


/**
 * WebSevconMon: (/xrt/scmon)
 */
void OvmsVehicleRenaultTwizy::WebSevconMon(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<style type=\"text/css\">\n"
    ".metric.number .value { min-width: 5em; }\n"
    ".metric.number .unit { min-width: 3em; }\n"
    ".night .highcharts-color-1 {\n"
      "fill: #c3c3c8;\n"
      "stroke: #c3c3c8;\n"
    "}\n"
    ".night .highcharts-legend-item text {\n"
      "fill: #dddddd;\n"
    "}\n"
    ".highcharts-graph {\n"
      "stroke-width: 4px;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "fill: #000000;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "color: #bbb;\n"
    "}\n"
    "#dynochart {\n"
      "width: 100%;\n"
      "max-width: 100%;\n"
      "height: 50vh;\n"
      "min-height: 265px;\n"
    "}\n"
    ".fullscreened #dynochart {\n"
      "height: 80vh;\n"
    "}\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary panel-single\">\n"
      "<div class=\"panel-heading\">Sevcon Monitor</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"row receiver\" id=\"livestatus\">\n"
          "<div class=\"table-responsive\">\n"
            "<table class=\"table table-bordered table-condensed\">\n"
              "<tbody>\n"
                "<tr>\n"
                  "<th>Speed</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.p.speed\"><span class=\"value\">?</span><span class=\"unit\">kph</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.m.rpm\"><span class=\"value\">?</span><span class=\"unit\">rpm</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.throttle\"><span class=\"value\">?</span><span class=\"unit\">%thr</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.footbrake\"><span class=\"value\">?</span><span class=\"unit\">%brk</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Torque</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.trq.limit\"><span class=\"value\">?</span><span class=\"unit\">Nm<sub>lim</sub></span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.trq.demand\"><span class=\"value\">?</span><span class=\"unit\">Nm<sub>dem</sub></span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.trq.act\"><span class=\"value\">?</span><span class=\"unit\">Nm</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Battery</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.vlt.bat\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.current\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.power\"><span class=\"value\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.vlt.cap\"><span class=\"value\">?</span><span class=\"unit\">V<sub>cap</sub></span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Motor</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.vlt.act\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.cur.act\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.pwr.act\"><span class=\"value\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.vlt.mod\"><span class=\"value\">?</span><span class=\"unit\">%mod</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Slip</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.frq.slip\"><span class=\"value\">?</span><span class=\"unit\">rad/s</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xrt.i.frq.output\"><span class=\"value\">?</span><span class=\"unit\">rad/s</span></div>\n"
                  "</td>\n"
                "</tr>\n"
              "</tbody>\n"
            "</table>\n"
          "</div>\n"
          "<div id=\"dynochart\"></div>\n"
        "</div>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<form id=\"form-scmon\" class=\"form-horizontal\" method=\"post\" action=\"#\">\n"
          "<div class=\"form-group\">\n"
            "<label class=\"control-label col-sm-3\" for=\"input-filename\">Monitoring control:</label>\n"
            "<div class=\"col-sm-9\">\n"
              "<div class=\"input-group\">\n"
                "<input type=\"text\" class=\"form-control font-monospace\"\n"
                  "placeholder=\"optional recording file path, blank = monitoring only\"\n"
                  "name=\"filename\" id=\"input-filename\" value=\"\" autocapitalize=\"none\" autocorrect=\"off\"\n"
                  "autocomplete=\"section-scmon\" spellcheck=\"false\">\n"
                "<div class=\"input-group-btn\">\n"
                  "<button type=\"button\" class=\"btn btn-default\" data-toggle=\"filedialog\" data-target=\"#select-recfile\" data-input=\"#input-filename\">Select</button>\n"
                "</div>\n"
              "</div>\n"
            "</div>\n"
            "<div class=\"col-sm-9 col-sm-offset-3\" style=\"margin-top:5px;\">\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-start\">Start</button>\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-stop\">Stop</button>\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-reset\">Reset</button>\n"
              "<span class=\"help-block\"><p>Hint: you may change the file on the fly (change + push 'Start' again).</p></span>\n"
            "</div>\n"
            "<div class=\"col-sm-9 col-sm-offset-3\">\n"
              "<samp id=\"cmdres-scmon\" class=\"samp\"></samp>\n"
            "</div>\n"
          "</div>\n"
        "</form>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<div class=\"filedialog\" id=\"select-recfile\" data-options='{\n"
      "\"title\": \"Select recording file\",\n"
      "\"path\": \"/sd/recordings/\",\n"
      "\"quicknav\": [\"/sd/\", \"/sd/recordings/\"]\n"
    "}' />\n"
    "\n"
    "<script>\n"
    "\n"
    "/**\n"
     "* Form handling\n"
     "*/\n"
    "\n"
    "$(\"#cmd-start\").on(\"click\", function(){\n"
      "loadcmd(\"xrt mon start \" + $(\"#input-filename\").val(), \"#cmdres-scmon\");\n"
    "});\n"
    "$(\"#cmd-stop\").on(\"click\", function(){\n"
      "loadcmd(\"xrt mon stop\", \"#cmdres-scmon\");\n"
    "});\n"
    "$(\"#cmd-reset\").on(\"click\", function(){\n"
      "loadcmd(\"xrt mon reset\", \"#cmdres-scmon\");\n"
    "});\n"
    "\n"
    "/**\n"
     "* Dyno chart\n"
     "*/\n"
    "\n"
    "var dynochart;\n"
    "\n"
    "function get_dyno_data() {\n"
      "var data = {\n"
        "bat_pwr_drv: metrics[\"xrt.s.b.pwr.drv\"],\n"
        "bat_pwr_rec: metrics[\"xrt.s.b.pwr.rec\"],\n"
        "mot_trq_drv: metrics[\"xrt.s.m.trq.drv\"],\n"
        "mot_trq_rec: metrics[\"xrt.s.m.trq.rec\"],\n"
      "};\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart.series[0].setData(data.bat_pwr_drv);\n"
      "dynochart.series[1].setData(data.bat_pwr_rec);\n"
      "dynochart.series[2].setData(data.mot_trq_drv);\n"
      "dynochart.series[3].setData(data.mot_trq_rec);\n"
    "}\n"
    "\n"
    "function init_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart = Highcharts.chart('dynochart', {\n"
        "chart: {\n"
          "type: 'spline',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"xrt.s.b.pwr.drv\"] != null\n"
                 "|| update[\"xrt.s.b.pwr.rec\"] != null\n"
                 "|| update[\"xrt.s.m.trq.drv\"] != null\n"
                 "|| update[\"xrt.s.m.trq.rec\"] != null)\n"
                  "update_dyno_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'x',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
        "},\n"
        "xAxis: {\n"
          "minTickInterval: 5,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "gridLineWidth: 1,\n"
        "},\n"
        "plotOptions: {\n"
          "series: {\n"
            "pointStart: 1,\n"
          "},\n"
          "spline: {\n"
            "marker: {\n"
              "enabled: false,\n"
            "},\n"
          "},\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "crosshairs: true,\n"
          "useHTML: true,\n"
          "headerFormat: '<table>' +\n"
            "'<tr><td class=\"tt-header\">Speed:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.key} kph</b></td></tr>',\n"
          "pointFormat:\n"
            "'<tr><td class=\"tt-color-{point.colorIndex}\">{series.name}:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.y}</b></td></tr>',\n"
          "footerFormat: '</table>',\n"
          "valueDecimals: 1,\n"
        "},\n"
        "yAxis: [{\n"
          "title: { text: \"Power [kW]\" },\n"
          "labels: { format: \"{value:.0f}\" },\n"
          "min: 0,\n"
          "minTickInterval: 1,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "showFirstLabel: false,\n"
        "},{\n"
          "title: { text: \"Torque [Nm]\" },\n"
          "labels: { format: \"{value:.0f}\" },\n"
          "min: 0,\n"
          "minTickInterval: 5,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "showFirstLabel: false,\n"
          "opposite: true,\n"
        "}],\n"
        "responsive: {\n"
          "rules: [{\n"
            "condition: { maxWidth: 500 },\n"
            "chartOptions: {\n"
              "yAxis: [{\n"
                "title: { text: null },\n"
              "},{\n"
                "title: { text: null },\n"
              "}],\n"
            "},\n"
          "}],\n"
        "},\n"
        "series: [{\n"
          "name: 'Max Drive Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_drv,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Recup Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_rec,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Drive Torque',\n"
          "tooltip: { valueSuffix: ' Nm' },\n"
          "data: data.mot_trq_drv,\n"
          "yAxis: 1,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Recup Torque',\n"
          "tooltip: { valueSuffix: ' Nm' },\n"
          "data: data.mot_trq_rec,\n"
          "yAxis: 1,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#dynochart').data('chart', dynochart).addClass('has-chart');"
    "}\n"
    "\n"
    "/**\n"
     "* Chart initialization\n"
     "*/\n"
    "\n"
    "function init_charts() {\n"
      "init_dyno_chart();\n"
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
    "</script>\n"
  );

  PAGE_HOOK("body.post");
  c.done();
}


/**
 * GetDashboardConfig: Twizy specific dashboard setup
 */
void OvmsVehicleRenaultTwizy::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 = 
    "yAxis: [{"
      // Speed:
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 70, className: 'green-band' },"
        "{ from: 70, to: 100, className: 'yellow-band' },"
        "{ from: 100, to: 120, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 45, max: 60,"
      "plotBands: ["
        "{ from: 45, to: 47.5, className: 'red-band' },"
        "{ from: 47.5, to: 50, className: 'yellow-band' },"
        "{ from: 50, to: 60, className: 'green-band' }]"
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
      "min: -10, max: 30,"
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
}

/**
 * WebExtDashboard: add profile switcher to dashboard
 */
PageResult_t OvmsVehicleRenaultTwizy::WebExtDashboard(PageEntry_t& p, PageContext_t& c, const std::string& hook)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance();
  SevconClient* sc = twizy ? twizy->GetSevconClient() : NULL;
  if (!sc)
    return PageResult_OK;
  
  if (hook == "body.pre") {
    c.print(
      "<style>\n"
      "#loadmenu .btn-group-lg>.btn, #loadmenu .btn-lg { padding: 10px 3px; overflow: hidden; font-weight: bold; }\n"
      "#loadmenu .btn-default.base { background-color: #fffca8; }\n"
      "#loadmenu .btn-default.base:hover,\n"
      "#loadmenu .btn-default.base:focus { background-color: #fffa62; }\n"
      "#loadmenu .unsaved > *:after { content: \"*\"; }\n"
      "#loadmenu { margin:10px 8px 0; }\n"
      ".fullscreened #loadmenu { margin:10px 10px 0; }\n"
      "#loadres { min-height: 44px; margin-top: 10px; font-size: 16px; }\n"
      "</style>\n"
      "\n"
      "<div class=\"receiver\" id=\"loadmenu\" style=\"display:none\">\n"
        "<form action=\"/xrt/drivemode\" target=\"#loadres\" method=\"post\">\n"
        "<input type=\"hidden\" name=\"info\" value=\"off\">\n"
          "<div class=\"btn-group btn-group-justified btn-longtouch\">\n");
    print_loadmenu_buttons(c, sc->m_drivemode);
    c.print(
          "</div>\n"
        "</form>\n"
        "<samp id=\"loadres\" class=\"text-center\"/>\n"
      "</div>\n"
      "\n"
      "<script>\n"
      "(function(){\n"
        "var $btn = $('#loadmenu .btn');\n"
        "$('#loadmenu').on('msg:metrics', function(ev, update) {\n"
          "var user = update[\"xrt.cfg.user\"], base = update[\"xrt.cfg.base\"], unsaved = update[\"xrt.cfg.unsaved\"];\n"
          "if (user != null) {\n"
            "$btn.filter('.btn-warning').removeClass('btn-warning').addClass('btn-default');\n"
            "$btn.filter('[value='+user+']').removeClass('btn-default').addClass('btn-warning');\n"
          "}\n"
          "if (base != null)\n"
            "$btn.removeClass('base').filter('[value='+base+']').addClass('base');\n"
          "if (unsaved === true)\n"
            "$btn.removeClass('unsaved').filter('.btn-warning').addClass('unsaved');\n"
          "else if (unsaved === false)\n"
            "$btn.removeClass('unsaved');\n"
        "});\n"
        "$('#main').one('load', function(ev) {\n"
          "if (!loggedin) {\n"
            "$('#loadmenu .btn').prop('disabled', true);\n"
            "$('#loadmenu').append('<a class=\"btn btn-default btn-lg btn-block\" target=\"#main\" href=\"/login?uri=/dashboard\">LOGIN</a>');\n"
          "}\n"
          "$('#loadmenu').appendTo('#panel-dashboard .panel-body').show();\n"
        "});\n"
      "})();\n"
      "</script>\n");
  }

  return PageResult_OK;
}


/**
 * WebProfileEditor: Tuning Profile Editor (URL /xrt/profed)
 */
void OvmsVehicleRenaultTwizy::WebProfileEditor(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  PAGE_HOOK("body.pre");
  
  c.print(
    "<style>\n"
    ".form-inline .form-control.slider-value {\n"
      "width: 70px;\n"
    "}\n"
    ".radio-list {\n"
      "height: 313px;\n"
      "overflow-y: auto;\n"
      "overflow-x: hidden;\n"
      "padding-right: 15px;\n"
    "}\n"
    ".radio-list .radio {\n"
      "overflow: hidden;\n"
    "}\n"
    ".radio-list .key {\n"
      "min-width: 20px;\n"
      "display: inline-block;\n"
      "text-align: center;\n"
      "margin: 0 10px;\n"
    "}\n"
    ".radio-list kbd {\n"
      "min-width: 60px;\n"
      "display: inline-block;\n"
      "text-align: center;\n"
      "margin: 0 20px 0 10px;\n"
    "}\n"
    ".radio-list .radio label {\n"
      "width: 100%;\n"
      "text-align: left;\n"
      "padding: 8px 30px;\n"
    "}\n"
    ".radio-list .radio label.active {\n"
      "background-color: #337ab7;\n"
      "color: #fff;\n"
      "outline: none;\n"
    "}\n"
    ".radio-list .radio label.active input {\n"
      "outline: none;\n"
    "}\n"
    ".night .radio-list .radio label:hover {\n"
      "color: #fff;\n"
    "}\n"
    "</style>\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Tuning Profile <span id=\"headkey\">Editor</span></div>\n"
      "<div class=\"panel-body\">\n"
        "<form action=\"#\">\n"
        "<div class=\"form-group\">\n"
          "<div class=\"flex-group\">\n"
            "<button type=\"button\" class=\"btn btn-default action-new\">New</button>\n"
            "<button type=\"button\" class=\"btn btn-default action-load\">Load…</button>\n"
            "<input type=\"hidden\" id=\"input-key\" name=\"key\" value=\"\">\n"
            "<input type=\"hidden\" id=\"input-base64-reset\" name=\"base64-reset\" value=\"\">\n"
            "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"Base64 profile code\"\n"
              "name=\"base64\" id=\"input-base64\" value=\"\" autocapitalize=\"none\" autocorrect=\"off\"\n"
              "autocomplete=\"off\" spellcheck=\"false\">\n"
          "</div>\n"
        "</div>\n"
        "<ul class=\"nav nav-tabs\">\n"
          "<li class=\"active\"><a data-toggle=\"tab\" href=\"#tab-drive\" aria-expanded=\"true\">Drive</a></li>\n"
          "<li class=\"\"><a data-toggle=\"tab\" href=\"#tab-recup\" aria-expanded=\"false\">Recup</a></li>\n"
          "<li class=\"\"><a data-toggle=\"tab\" href=\"#tab-ramps\" aria-expanded=\"false\">Ramps</a></li>\n"
        "</ul>\n"
        "<div class=\"tab-content\">\n"
          "<div id=\"tab-drive\" class=\"tab-pane section-drive active in\">\n"
            "<fieldset id=\"part-speed\">\n"
              "<legend>Speed</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-speed\">Max speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"speed\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-warn\">Warn speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"warn\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-power\">\n"
              "<legend>Power</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-current\">Current:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"current\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-torque\">Torque:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"torque\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-power_low\">Power low:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"power_low\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-power_high\">Power high:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"power_high\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-drive\">\n"
              "<legend>Drive</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-drive\">Drive level:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"drive\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-autodrive_ref\">Auto 100% ref:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"autodrive_ref\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-autodrive_minprc\">Auto min level:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"autodrive_minprc\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-tsmapd\">\n"
              "<legend>Torque/Speed-Map: Drive</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_prc1\">Trq level 1:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_prc1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_spd1\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_spd1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_prc2\">Trq level 2:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_prc2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_spd2\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_spd2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_prc3\">Trq level 3:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_prc3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_spd3\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_spd3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_prc4\">Trq level 4:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_prc4\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsd_spd4\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsd_spd4\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
          "</div>\n"
          "<div id=\"tab-recup\" class=\"tab-pane section-recup\">\n"
            "<fieldset id=\"part-recup\">\n"
              "<legend>Recup</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-neutral\">Neutral level:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"neutral\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-brake\">Brake level:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"brake\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-autorecup_ref\">Auto 100% ref:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"autorecup_ref\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-autorecup_minprc\">Auto min level:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"autorecup_minprc\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-tsmapn\">\n"
              "<legend>Torque/Speed-Map: Neutral</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_prc1\">Trq level 1:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_prc1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_spd1\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_spd1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_prc2\">Trq level 2:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_prc2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_spd2\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_spd2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_prc3\">Trq level 3:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_prc3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_spd3\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_spd3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_prc4\">Trq level 4:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_prc4\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsn_spd4\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsn_spd4\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-tsmapb\">\n"
              "<legend>Torque/Speed-Map: Brake</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_prc1\">Trq level 1:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_prc1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_spd1\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_spd1\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_prc2\">Trq level 2:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_prc2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_spd2\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_spd2\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_prc3\">Trq level 3:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_prc3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_spd3\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_spd3\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_prc4\">Trq level 4:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_prc4\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-tsb_spd4\">…at speed:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"tsb_spd4\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
          "</div>\n"
          "<div id=\"tab-ramps\" class=\"tab-pane section-ramps\">\n"
            "<fieldset id=\"part-rampsglobal\">\n"
              "<legend>General</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramplimit_accel\">Limit up:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramplimit_accel\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramplimit_decel\">Limit down:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramplimit_decel\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-smooth\">Smoothing:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"smooth\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
            "<fieldset id=\"part-ramps\">\n"
              "<legend>Ramp Speeds</legend>\n"
              "<div class=\"form-horizontal\">\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramp_start\">Start/reverse:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramp_start\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramp_accel\">Accelerate:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramp_accel\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramp_decel\">Decelerate:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramp_decel\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramp_neutral\">Neutral:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramp_neutral\" /></div>\n"
                "</div>\n"
                "<div class=\"form-group\">\n"
                  "<label class=\"control-label col-sm-2\" for=\"input-ramp_brake\">Brake:</label>\n"
                  "<div class=\"col-sm-10\"><div class=\"form-control slider\" id=\"ramp_brake\" /></div>\n"
                "</div>\n"
              "</div>\n"
            "</fieldset>\n"
          "</div>\n"
        "</div>\n"
        "<br>\n"
        "<div class=\"form-horizontal\">\n"
          "<div class=\"form-group\">\n"
            "<label class=\"control-label col-sm-2\" for=\"input-label\">Label:</label>\n"
            "<div class=\"col-sm-10\">\n"
              "<input type=\"text\" class=\"form-control\" placeholder=\"Short button label\"\n"
                "name=\"label\" id=\"input-label\" value=\"\" autocapitalize=\"none\" autocorrect=\"off\"\n"
                "autocomplete=\"off\" spellcheck=\"false\">\n"
            "</div>\n"
          "</div>\n"
          "<div class=\"form-group\">\n"
            "<label class=\"control-label col-sm-2\" for=\"input-title\">Title:</label>\n"
            "<div class=\"col-sm-10\">\n"
              "<input type=\"text\" class=\"form-control\" placeholder=\"optional title/name\"\n"
                "name=\"title\" id=\"input-title\" value=\"\" autocapitalize=\"none\" autocorrect=\"off\"\n"
                "autocomplete=\"off\" spellcheck=\"false\">\n"
            "</div>\n"
          "</div>\n"
        "</div>\n"
        "<br>\n"
        "<div class=\"text-center\">\n"
          "<button type=\"button\" class=\"btn btn-default action-reset\">Reset</button>\n"
          "<button type=\"button\" class=\"btn btn-default action-saveas\">Save as…</button>\n"
          "<button type=\"button\" class=\"btn btn-primary action-save\">Save</button>\n"
        "</div>\n"
        "</form>\n"
      "</div>\n"
    "</div>\n"
    "<div id=\"key-dialog\" />\n"
    "<script>\n"
    "(function(){\n"
      "// init sliders:\n"
      "$('#speed').slider({ unit:'kph', min:6, max:120, default:80, value:80, checked:false });\n"
      "$('#warn').slider({ unit:'kph', min:6, max:120, default:89, value:89, checked:false });\n"
      "$('#current').slider({ unit:'%', min:10, max:123, default:100, value:100, checked:false });\n"
      "$('#torque').slider({ unit:'%', min:10, max:130, default:100, value:100, checked:false });\n"
      "$('#power_low').slider({ unit:'%', min:10, max:139, default:100, value:100, checked:false });\n"
      "$('#power_high').slider({ unit:'%', min:10, max:130, default:100, value:100, checked:false });\n"
      "$('#drive').slider({ unit:'%', min:10, max:100, default:100, value:100, checked:false });\n"
      "$('#autodrive_ref').slider({ unit:'kW', min:0, max:25, step:0.1, default:0, value:0, checked:false });\n"
      "$('#autodrive_minprc').slider({ unit:'%', min:0, max:100, default:0, value:0, checked:false });\n"
      "$('#tsd_prc1').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsd_spd1').slider({ unit:'kph', min:0, max:120, default:33, value:33, checked:false });\n"
      "$('#tsd_prc2').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsd_spd2').slider({ unit:'kph', min:0, max:120, default:39, value:39, checked:false });\n"
      "$('#tsd_prc3').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsd_spd3').slider({ unit:'kph', min:0, max:120, default:50, value:50, checked:false });\n"
      "$('#tsd_prc4').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsd_spd4').slider({ unit:'kph', min:0, max:120, default:66, value:66, checked:false });\n"
      "$('#neutral').slider({ unit:'%', min:0, max:100, default:18, value:18, checked:false });\n"
      "$('#brake').slider({ unit:'%', min:0, max:100, default:18, value:18, checked:false });\n"
      "$('#autorecup_ref').slider({ unit:'kW', min:0, max:25, step:0.1, default:0, value:0, checked:false });\n"
      "$('#autorecup_minprc').slider({ unit:'%', min:0, max:100, default:0, value:0, checked:false });\n"
      "$('#tsn_prc1').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsn_spd1').slider({ unit:'kph', min:0, max:120, default:33, value:33, checked:false });\n"
      "$('#tsn_prc2').slider({ unit:'%', min:0, max:100, default:80, value:80, checked:false });\n"
      "$('#tsn_spd2').slider({ unit:'kph', min:0, max:120, default:39, value:39, checked:false });\n"
      "$('#tsn_prc3').slider({ unit:'%', min:0, max:100, default:50, value:50, checked:false });\n"
      "$('#tsn_spd3').slider({ unit:'kph', min:0, max:120, default:50, value:50, checked:false });\n"
      "$('#tsn_prc4').slider({ unit:'%', min:0, max:100, default:20, value:20, checked:false });\n"
      "$('#tsn_spd4').slider({ unit:'kph', min:0, max:120, default:66, value:66, checked:false });\n"
      "$('#tsb_prc1').slider({ unit:'%', min:0, max:100, default:100, value:100, checked:false });\n"
      "$('#tsb_spd1').slider({ unit:'kph', min:0, max:120, default:33, value:33, checked:false });\n"
      "$('#tsb_prc2').slider({ unit:'%', min:0, max:100, default:80, value:80, checked:false });\n"
      "$('#tsb_spd2').slider({ unit:'kph', min:0, max:120, default:39, value:39, checked:false });\n"
      "$('#tsb_prc3').slider({ unit:'%', min:0, max:100, default:50, value:50, checked:false });\n"
      "$('#tsb_spd3').slider({ unit:'kph', min:0, max:120, default:50, value:50, checked:false });\n"
      "$('#tsb_prc4').slider({ unit:'%', min:0, max:100, default:20, value:20, checked:false });\n"
      "$('#tsb_spd4').slider({ unit:'kph', min:0, max:120, default:66, value:66, checked:false });\n"
      "$('#ramplimit_accel').slider({ unit:'%', min:1, max:100, default:30, value:30, checked:false });\n"
      "$('#ramplimit_decel').slider({ unit:'%', min:0, max:100, default:30, value:30, checked:false });\n"
      "$('#smooth').slider({ unit:'%', min:0, max:100, default:70, value:70, checked:false });\n"
      "$('#ramp_start').slider({ unit:'‰', min:1, max:250, default:40, value:40, checked:false });\n"
      "$('#ramp_accel').slider({ unit:'%', min:1, max:100, default:25, value:25, checked:false });\n"
      "$('#ramp_decel').slider({ unit:'%', min:0, max:100, default:20, value:20, checked:false });\n"
      "$('#ramp_neutral').slider({ unit:'%', min:0, max:100, default:40, value:40, checked:false });\n"
      "$('#ramp_brake').slider({ unit:'%', min:0, max:100, default:40, value:40, checked:false });\n"
      "// profile handling:\n"
      "const keys = [\n"
        "\"checksum\",\n"
        "\"speed\",\n"
        "\"warn\",\n"
        "\"torque\",\n"
        "\"power_low\",\n"
        "\"power_high\",\n"
        "\"drive\",\n"
        "\"neutral\",\n"
        "\"brake\",\n"
        "\"tsd_spd1\",\n"
        "\"tsd_spd2\",\n"
        "\"tsd_spd3\",\n"
        "\"tsd_spd4\",\n"
        "\"tsd_prc1\",\n"
        "\"tsd_prc2\",\n"
        "\"tsd_prc3\",\n"
        "\"tsd_prc4\",\n"
        "\"tsn_spd1\",\n"
        "\"tsn_spd2\",\n"
        "\"tsn_spd3\",\n"
        "\"tsn_spd4\",\n"
        "\"tsn_prc1\",\n"
        "\"tsn_prc2\",\n"
        "\"tsn_prc3\",\n"
        "\"tsn_prc4\",\n"
        "\"tsb_spd1\",\n"
        "\"tsb_spd2\",\n"
        "\"tsb_spd3\",\n"
        "\"tsb_spd4\",\n"
        "\"tsb_prc1\",\n"
        "\"tsb_prc2\",\n"
        "\"tsb_prc3\",\n"
        "\"tsb_prc4\",\n"
        "\"ramp_start\",\n"
        "\"ramp_accel\",\n"
        "\"ramp_decel\",\n"
        "\"ramp_neutral\",\n"
        "\"ramp_brake\",\n"
        "\"smooth\",\n"
        "\"brakelight_on\",\n"
        "\"brakelight_off\",\n"
        "\"ramplimit_accel\",\n"
        "\"ramplimit_decel\",\n"
        "\"autorecup_minprc\",\n"
        "\"autorecup_ref\",\n"
        "\"autodrive_minprc\",\n"
        "\"autodrive_ref\",\n"
        "\"current\",\n"
      "];\n"
      "const scale = {\n"
        "\"autodrive_ref\": 0.1,\n"
        "\"autorecup_ref\": 0.1,\n"
      "};\n"
      "var profile = {};\n"
      "var plist = [\n"
        "{ label: \"–\", title: \"Working Set\" },\n"
        "{ label: \"PWR\", title: \"Power\" },\n"
        "{ label: \"ECO\", title: \"Economy\" },\n"
        "{ label: \"ICE\", title: \"Winter\" },\n"
      "];\n"
      "// load profile list:\n"
      "var plistloader = loadcmd('config list xrt').then(function(data) {\n"
        "var lines = data.split('\\n'), line, i, key;\n"
        "for (i = 0; i < lines.length; i++) {\n"
          "line = lines[i].match(/profile([0-9]{2})\\.?([^:]*): (.*)/);\n"
          "if (line && line.length == 4) {\n"
            "key = Number(line[1]);\n"
            "if (key < 1 || key > 99) continue;\n"
            "if (!plist[key]) plist[key] = {};\n"
            "plist[key][line[2]||\"profile\"] = line[3];\n"
          "}\n"
        "}\n"
      "});\n"
      "// current control → power ranges:\n"
      "function currentControl(on, trigger) {\n"
    "		if (on) {\n"
    "			$(\"#input-torque, #input-power_low, #input-power_high\").slider({ max: 254 });\n"
    "		} else {\n"
    "			$(\"#input-torque, #input-power_high\").slider({ max: 130 });\n"
    "			$(\"#input-power_low\").slider({ max: 139 });\n"
    "		}\n"
      "}\n"
      "$('#input-current').on('change', function(ev) {\n"
        "currentControl(this.checked);\n"
        "$(\"#input-torque, #input-power_low, #input-power_high\").trigger('change');\n"
      "});\n"
      "// load profile into sliders:\n"
      "function loadProfile() {\n"
        "currentControl(profile[\"current\"] >= 0);\n"
        "$.map(profile, function(val, key) {\n"
          "$('#input-'+key).slider({ value: (val >= 0) ? (val * (scale[key]||1)) : null, checked: (val >= 0) });\n"
        "});\n"
      "}\n"
      "// calculate profile checksum:\n"
      "function calcChecksum() {\n"
        "var checksum, i;\n"
        "checksum = 0x0101;\n"
        "for (i=1; i<keys.length; i++)\n"
          "checksum += profile[keys[i]] + 1;\n"
        "if ((checksum & 0x0ff) == 0)\n"
          "checksum >>= 8;\n"
        "return (checksum & 0x0ff) - 1;\n"
      "}\n"
      "// load a base64 string into profile:\n"
      "function loadBase64(base64) {\n"
        "var bin = atob(base64);\n"
        "$.map(keys, function(key, i) { profile[key] = (bin.charCodeAt(i)||0) - 1; });\n"
        "if (profile[\"checksum\"] == calcChecksum()) {\n"
          "loadProfile();\n"
          "return true;\n"
        "} else {\n"
          "confirmdialog(\"Profile Error\", \"Invalid base64 code (checksum mismatch)\", [\"Close\"]);\n"
          "return false;\n"
        "}\n"
      "}\n"
      "// make a base64 string from profile:\n"
      "function makeBase64() {\n"
        "profile[\"checksum\"] = calcChecksum();\n"
        "var u8 = new Uint8Array(keys.length);\n"
        "$.map(keys, function(key, i) { u8[i] = profile[key] + 1; });\n"
        "return btoa(String.fromCharCode.apply(null, u8));\n"
      "}\n"
      "// load a profile:\n"
      "function loadKey(key) {\n"
        "if (key < 0 || key > 99) return;\n"
        "plistloader.then(function() {\n"
          "$('#input-label').val(plist[key] && plist[key][\"label\"] || \"\");\n"
          "$('#input-title').val(plist[key] && plist[key][\"title\"] || \"\");\n"
          "loadcmd('xrt cfg get ' + key).fail(function(request, textStatus, errorThrown) {\n"
            "confirmdialog(\"Load Profile\", xhrErrorInfo(request, textStatus, errorThrown), [\"Close\"]);\n"
          "}).done(function(res) {\n"
            "var base64;\n"
            "if (res.match(/error/i))\n"
              "base64 = \"AQ\";\n"
            "else\n"
              "base64 = res.substr(res.lastIndexOf(' ')+1);\n"
            "if (loadBase64(base64)) {\n"
              "$('#input-key').val(key);\n"
              "$('#input-base64, #input-base64-reset').val(base64);\n"
              "$('#headkey').text('#' + key);\n"
            "}\n"
          "});\n"
        "});\n"
      "}\n"
      "// save profile:\n"
      "function saveKey(key) {\n"
        "if (key < 0 || key > 99) return;\n"
        "var base64 = $('#input-base64').val(),\n"
          "label = $('#input-label').val().replace(/\"/g, '\\\\\"'),\n"
          "title = $('#input-title').val().replace(/\"/g, '\\\\\"'),\n"
          "ckey = 'profile' + ((key<10)?'0':'') + key;\n"
        "var cmd = 'xrt cfg set ' + key + ' ' + base64 + ' \"' + label + '\" \"' + title + '\"';\n"
        "loadcmd(cmd).fail(function(request, textStatus, errorThrown) {\n"
          "confirmdialog(\"Save Profile\", xhrErrorInfo(request, textStatus, errorThrown), [\"Close\"]);\n"
        "}).done(function(res) {\n"
          "confirmdialog(\"Save Profile\", res, [\"Close\"]);\n"
          "if (!res.match(/error/i)) {\n"
            "if (key > 0) {\n"
              "if (!plist[key]) plist[key] = {};\n"
              "plist[key][\"label\"] = label;\n"
              "plist[key][\"title\"] = title;\n"
            "}\n"
            "$('#input-key').val(key);\n"
            "$('#input-base64-reset').val(base64);\n"
            "$('#headkey').text('#' + key);\n"
          "}\n"
        "});\n"
      "}\n"
      "// base64 input:\n"
      "$('#input-base64').on('change', function(ev) {\n"
        "var base64 = $(this).val() || \"AQ\";\n"
        "if (loadBase64(base64))\n"
          "$('#input-base64-reset').val(base64);\n"
      "});\n"
      "// update profile & base64 on slider changes:\n"
      "$('.slider-value').on('change', function(ev) {\n"
        "var key = this.name;\n"
        "var val = Math.max(this.min, Math.min(this.max, 1*this.value));\n"
        "profile[key] = this.checked ? (val / (scale[key]||1)) : -1;\n"
        "var base64 = makeBase64();\n"
        "$('#input-base64').val(base64);\n"
      "});\n"
      "// prep key dialog:\n"
      "$('#key-dialog').dialog({\n"
        "show: false,\n"
        "onShow: function(input) {\n"
          "var $this = $(this);\n"
          "$this.addClass(\"loading\");\n"
          "plistloader.then(function(data) {\n"
            "var curkey = $('#input-key').val() || 0, i, label, title;\n"
            "$plist = $('<div class=\"radio-list\" data-toggle=\"buttons\" />');\n"
            "for (i = 0; i <= Math.min(99, plist.length + 2); i++) {\n"
              "if (plist[i] && (i==0 || plist[i].profile)) {\n"
                "label = plist[i].label || \"–\";\n"
                "title = plist[i].title || (plist[i].profile.substr(0, 10) + \"…\");\n"
              "} else {\n"
                "label = \"–\";\n"
                "title = \"–new–\";\n"
              "}\n"
              "$plist.append('<div class=\"radio\"><label class=\"btn\">' +\n"
                "'<input type=\"radio\" name=\"key\" value=\"' + i + '\"><span class=\"key\">' +\n"
                "((i==0) ? \"WS\" : (\"#\" + ((i<10)?'0':'') + i)) + '</span> <kbd>' +\n"
                "encode_html(label) + '</kbd> ' + encode_html(title) + '</label></div>');\n"
            "}\n"
            "$this.find('.modal-body').append($plist).find('input[value=\"'+curkey+'\"]')\n"
              ".prop(\"checked\", true).parent().addClass(\"active\");\n"
            "$plist\n"
              ".on('dblclick', 'label.btn', function(ev) { $this.dialog('triggerButton', 1); })\n"
              ".on('keypress', function(ev) { if (ev.which==13) $this.dialog('triggerButton', 1); });\n"
            "$this.removeClass(\"loading\");\n"
          "});\n"
        "},\n"
        "onShown: function(input) {\n"
          "$(this).find('.btn.active').focus();\n"
        "},\n"
        "onHide: function(input) {\n"
          "var $this = $(this), dlg = $this.data(\"dialog\");\n"
          "var key = $this.find('input[name=\"key\"]:checked').val();\n"
          "if (key !== undefined && input.button && input.button.index)\n"
            "dlg.options.onAction.call(this, key);\n"
        "},\n"
      "});\n"
      "// buttons:\n"
      "$('.action-new').on('click', function(ev) {\n"
        "$('.slider').slider({ value: null });\n"
        "$('#headkey').text('Editor');\n"
        "$('#input-base64, #input-label, #input-title').val('').trigger('change');\n"
      "});\n"
      "$('.action-reset').on('click', function(ev) {\n"
        "$('#input-base64').val($('#input-base64-reset').val()).trigger('change');\n"
      "});\n"
      "$('.action-load').on('click', function(ev) {\n"
        "$('#key-dialog').dialog({\n"
          "show: true, title: 'Load Profile', body: '',\n"
          "buttons: [{ label: 'Cancel', btnClass: 'default' },{ label: 'Load', btnClass: 'primary' }],\n"
          "onAction: function(key) { loadKey(key) },\n"
        "});\n"
      "});\n"
      "$('.action-saveas').on('click', function(ev) {\n"
        "$('#key-dialog').dialog({\n"
          "show: true, title: 'Save Profile', body: '',\n"
          "buttons: [{ label: 'Cancel', btnClass: 'default' },{ label: 'Save', btnClass: 'primary' }],\n"
          "onAction: function(key) { saveKey(key) },\n"
        "});\n"
      "});\n"
      "$('.action-save').on('click', function(ev) {\n"
        "var key = $('#input-key').val();\n"
        "if (key === \"\")\n"
          "$('.action-saveas').trigger('click');\n"
        "else\n"
          "saveKey(key);\n"
      "});\n"
      "// start: load profile / open load dialog:\n"
      "if (page.params[\"key\"] !== undefined)\n"
        "loadKey(page.params[\"key\"]);\n"
      "else\n"
        "$('.action-load').trigger('click');\n"
    "})();\n"
    "</script>\n"
  );
  
  PAGE_HOOK("body.post");
  c.done();
}


/**
 * WebDrivemodeConfig: Drivemode Button Configuration (URL /xrt/dmconfig)
 */
void OvmsVehicleRenaultTwizy::WebDrivemodeConfig(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  PAGE_HOOK("body.pre");
  
  c.print(
    "<style>\n"
    ".fullscreened .panel-single .panel-body {\n"
      "padding: 10px;\n"
    "}\n"
    ".btn-group-lg>.btn,\n"
    ".btn-lg {\n"
      "padding: 10px 3px;\n"
      "overflow: hidden;\n"
    "}\n"
    "#loadmenu .btn {\n"
      "font-weight: bold;\n"
    "}\n"
    ".btn-group.btn-group-lg.exchange {\n"
      "width: 2%;\n"
    "}\n"
    "#editor .btn:hover {\n"
      "background-color: #e6e6e6;\n"
    "}\n"
    "#editor .btn-group-lg>.btn,\n"
    "#editor .btn-lg {\n"
      "padding: 10px 0px;\n"
      "font-size: 15px;\n"
    "}\n"
    "#editor .exchange .btn {\n"
      "font-weight: bold;\n"
    "}\n"
    ".night #editor .btn {\n"
      "color: #000;\n"
      "background: #888;\n"
    "}\n"
    ".night #editor .btn.focus,\n"
    ".night #editor .btn:focus,\n"
    ".night #editor .btn:hover {\n"
      "background-color: #e0e0e0;\n"
    "}\n"
    ".radio-list {\n"
      "height: 313px;\n"
      "overflow-y: auto;\n"
      "overflow-x: hidden;\n"
      "padding-right: 15px;\n"
    "}\n"
    ".radio-list .radio {\n"
      "overflow: hidden;\n"
    "}\n"
    ".radio-list .key {\n"
      "min-width: 20px;\n"
      "display: inline-block;\n"
      "text-align: center;\n"
      "margin: 0 10px;\n"
    "}\n"
    ".radio-list kbd {\n"
      "min-width: 60px;\n"
      "display: inline-block;\n"
      "text-align: center;\n"
      "margin: 0 20px 0 10px;\n"
    "}\n"
    ".radio-list .radio label {\n"
      "width: 100%;\n"
      "text-align: left;\n"
      "padding: 8px 30px;\n"
    "}\n"
    ".radio-list .radio label.active {\n"
      "background-color: #337ab7;\n"
      "color: #fff;\n"
      "outline: none;\n"
    "}\n"
    ".radio-list .radio label.active input {\n"
      "outline: none;\n"
    "}\n"
    ".night .radio-list .radio label:hover {\n"
      "color: #fff;\n"
    "}\n"
    "</style>\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Drivemode Button Configuration</div>\n"
      "<div class=\"panel-body\">\n"
        "<form action=\"#\">\n"
        "<p id=\"info\">Loading button configuration…</p>\n"
        "<div id=\"loadmenu\" class=\"btn-group btn-group-justified\"></div>\n"
        "<div id=\"editor\" class=\"btn-group btn-group-justified\">\n"
          "<div class=\"btn-group btn-group-lg add back\">\n"
            "<button type=\"button\" class=\"btn\" title=\"Add button\">✚</button>\n"
          "</div>\n"
        "</div>\n"
        "<br>\n"
        "<br>\n"
        "<div class=\"text-center\">\n"
          "<button type=\"button\" class=\"btn btn-default\" onclick=\"reloadpage()\">Reset</button>\n"
          "<button type=\"button\" class=\"btn btn-primary action-save\">Save</button>\n"
        "</div>\n"
        "</form>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<a class=\"btn btn-sm btn-default\" target=\"#main\" href=\"/dashboard\">Dashboard</a>\n"
        "<a class=\"btn btn-sm btn-default\" target=\"#main\" href=\"/xrt/drivemode\">Drivemode</a>\n"
      "</div>\n"
    "</div>\n"
    "<div id=\"key-dialog\" />\n"
    "<script>\n"
    "(function(){\n"
      "var $menu = $('#loadmenu'), $edit = $('#editor'), $back = $edit.find('.back');\n"
      "var pbuttons = [0,1,2,3];\n"
      "var plist = [\n"
        "{ label: \"STD\", title: \"Standard\" },\n"
        "{ label: \"PWR\", title: \"Power\" },\n"
        "{ label: \"ECO\", title: \"Economy\" },\n"
        "{ label: \"ICE\", title: \"Winter\" },\n"
      "];\n"
      "// load profile list & button config:\n"
      "$('.panel').addClass(\"loading\");\n"
      "var plistloader = loadcmd('config list xrt').then(function(data) {\n"
        "var lines = data.split('\\n'), line, i, key;\n"
        "for (i = 0; i < lines.length; i++) {\n"
          "line = lines[i].match(/profile([0-9]{2})\\.?([^:]*): (.*)/);\n"
          "if (line && line.length == 4) {\n"
            "key = Number(line[1]);\n"
            "if (key < 1 || key > 99) continue;\n"
            "if (!plist[key]) plist[key] = {};\n"
            "plist[key][line[2]||\"profile\"] = line[3];\n"
            "continue;\n"
          "}\n"
          "line = lines[i].match(/profile_buttons: (.*)/);\n"
          "if (line && line.length == 2) {\n"
            "try {\n"
              "pbuttons = JSON.parse(\"[\" + line[1] + \"]\");\n"
            "} catch (e) {\n"
              "console.error(e);\n"
            "}\n"
          "}\n"
        "}\n"
      "});\n"
      "// prep key dialog:\n"
      "$('#key-dialog').dialog({\n"
        "show: false,\n"
        "title: \"Select Profile\",\n"
        "buttons: [{ label: 'Cancel', btnClass: 'default' },{ label: 'Select', btnClass: 'primary' }],\n"
        "onShow: function(input) {\n"
          "var $this = $(this), dlg = $this.data(\"dialog\");\n"
          "$this.addClass(\"loading\");\n"
          "plistloader.then(function(data) {\n"
            "var curkey = dlg.options.key || 0, i, label, title;\n"
            "$plist = $('<div class=\"radio-list\" data-toggle=\"buttons\" />');\n"
            "for (i = 0; i <= Math.min(99, plist.length-1); i++) {\n"
              "if (plist[i] && (i==0 || plist[i].profile)) {\n"
                "label = plist[i].label || \"–\";\n"
                "title = plist[i].title || (plist[i].profile.substr(0, 10) + \"…\");\n"
              "} else {\n"
                "label = \"–\";\n"
                "title = \"–new–\";\n"
              "}\n"
              "$plist.append('<div class=\"radio\"><label class=\"btn\">' +\n"
                "'<input type=\"radio\" name=\"key\" value=\"' + i + '\"><span class=\"key\">' +\n"
                "((i==0) ? \"STD\" : (\"#\" + ((i<10)?'0':'') + i)) + '</span> <kbd>' +\n"
                "encode_html(label) + '</kbd> ' + encode_html(title) + '</label></div>');\n"
            "}\n"
            "$this.find('.modal-body').html($plist).find('input[value=\"'+curkey+'\"]')\n"
              ".prop(\"checked\", true).parent().addClass(\"active\");\n"
            "$plist\n"
              ".on('dblclick', 'label.btn', function(ev) { $this.dialog('triggerButton', 1); })\n"
              ".on('keypress', function(ev) { if (ev.which==13) $this.dialog('triggerButton', 1); });\n"
            "$this.removeClass(\"loading\");\n"
          "});\n"
        "},\n"
        "onShown: function(input) {\n"
          "$(this).find('.btn.active').focus();\n"
        "},\n"
        "onHide: function(input) {\n"
          "var $this = $(this), dlg = $this.data(\"dialog\");\n"
          "var key = $this.find('input[name=\"key\"]:checked').val();\n"
          "if (key !== undefined && input.button && input.button.index)\n"
            "dlg.options.onAction.call(this, key);\n"
        "},\n"
      "});\n"
      "// profile selection:\n"
      "$('#loadmenu').on('click', 'button', function(ev) {\n"
        "var $this = $(this);\n"
        "$('#key-dialog').dialog({\n"
          "show: true,\n"
          "key: $this.val(),\n"
          "onAction: function(key) {\n"
            "var prof = plist[key] || {};\n"
            "$this.val(key);\n"
            "$this.attr(\"title\", prof.title || \"\");\n"
            "$this.text(prof.label || (\"#\"+((key<10)?\"0\":\"\")+key));\n"
          "},\n"
        "});\n"
      "});\n"
      "// create buttons:\n"
      "function addButton(key, front) {\n"
        "var prof = plist[key] || {};\n"
        "if ($menu[0].childElementCount == 0) {\n"
          "$back.before('\\\n"
            "<div class=\"btn-group btn-group-lg add front\">\\\n"
              "<button type=\"button\" class=\"btn\" title=\"Add button\">✚</button>\\\n"
            "</div>\\\n"
            "<div class=\"btn-group btn-group-lg remove\">\\\n"
              "<button type=\"button\" class=\"btn\" title=\"Remove button\">✖</button>\\\n"
            "</div>');\n"
        "} else {\n"
          "$back.before('\\\n"
            "<div class=\"btn-group btn-group-lg exchange\">\\\n"
              "<button type=\"button\" class=\"btn\" title=\"Exchange buttons\">⇄</button>\\\n"
            "</div>\\\n"
            "<div class=\"btn-group btn-group-lg remove\">\\\n"
              "<button type=\"button\" class=\"btn\" title=\"Remove button\">✖</button>\\\n"
            "</div>');\n"
        "}\n"
        "var $btn = $('\\\n"
          "<div class=\"btn-group btn-group-lg\">\\\n"
            "<button type=\"button\" value=\"{key}\" class=\"btn btn-default\" title=\"{title}\">{label}</button>\\\n"
          "</div>'\n"
          ".replace(\"{key}\", key)\n"
          ".replace(\"{title}\", encode_html(prof.title || \"\"))\n"
          ".replace(\"{label}\", encode_html(prof.label || \"#\"+((key<10)?\"0\":\"\")+key)));\n"
        "if (front)\n"
          "$menu.prepend($btn);\n"
        "else\n"
          "$menu.append($btn);\n"
      "}\n"
      "plistloader.then(function() {\n"
        "var key, prof;\n"
        "for (var i = 0; i < pbuttons.length; i++) {\n"
          "addButton(pbuttons[i]);\n"
        "}\n"
        "$('.panel').removeClass(\"loading\");\n"
        "$('#info').text(\"Click button to select profile:\");\n"
      "});\n"
      "// editor buttons:\n"
      "$('#editor').on('click', '.add .btn', function(ev) {\n"
        "addButton(0, $(this).parent().hasClass(\"front\"));\n"
      "}).on('click', '.remove .btn', function(ev) {\n"
        "var $this = $(this), pos = $edit.find('.btn').index(this) >> 1;\n"
        "$($menu.children().get(pos)).detach();\n"
        "if (pos > 0 || $menu[0].childElementCount == 0)\n"
          "$this.parent().prev().detach();\n"
        "else if ($menu[0].childElementCount != 0)\n"
          "$this.parent().next().detach();\n"
        "$this.parent().detach();\n"
      "}).on('click', '.exchange .btn', function(ev) {\n"
        "var pos = $edit.find('.btn').index(this) >> 1;\n"
        "if (pos > 0) $($menu.children().get(pos)).insertBefore($($menu.children().get(pos-1)));\n"
      "});\n"
      "// save:\n"
      "$('.action-save').on('click', function(ev) {\n"
        "pbuttons = [];\n"
        "$menu.find('.btn').each(function() { pbuttons.push(this.value) });\n"
        "loadcmd('config set xrt profile_buttons ' + pbuttons.toString())\n"
          ".fail(function(request, textStatus, errorThrown) {\n"
            "confirmdialog(\"Save Configuration\", xhrErrorInfo(request, textStatus, errorThrown), [\"Close\"]);\n"
          "})\n"
          ".done(function(res) {\n"
            "confirmdialog(\"Save Configuration\", res, [\"Close\"]);\n"
          "});\n"
      "});\n"
    "})();\n"
    "</script>\n"
  );
  
  PAGE_HOOK("body.post");
  c.done();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
