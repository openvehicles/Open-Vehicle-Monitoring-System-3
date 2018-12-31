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
  // vehicle menu:
  MyWebServer.RegisterPage("/xrt/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/battery", "Battery config", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/battmon", "Battery monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrt/scmon", "Sevcon monitor", WebSevconMon, PageMenu_Vehicle, PageAuth_Cookie);

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
  // profile labels (todo: make configurable):
  const char* proflabel[4] = { "STD", "PWR", "ECO", "ICE" };
  
  for (int prof=0; prof<=3; prof++) {
    c.printf(
        "<div class=\"btn-group btn-group-lg\">\n"
          "<button type=\"submit\" name=\"load\" value=\"%d\" id=\"prof-%d\" class=\"btn btn-%s %s%s\"><strong>%s</strong></button>\n"
        "</div>\n"
      , prof, prof
      , (drivemode.profile_user == prof) ? "warning" : "default"
      , (drivemode.profile_cfgmode == prof) ? "base" : ""
      , (drivemode.profile_user == prof && drivemode.unsaved) ? "unsaved" : ""
      , proflabel[prof]
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
    "<style>"
    ".btn-default.base { background-color: #fffca8; }"
    ".btn-default.base:hover, .btn-default.base:focus { background-color: #fffa62; }"
    ".unsaved > *:after { content: \"*\"; }"
    ".fullscreened .panel-single .panel-body { padding: 10px; }"
    "</style>");
  
  c.panel_start("primary", "Drivemode");
  
  c.printf(
    "<samp id=\"loadres\">%s</samp>", _html(buf.str()));
  
  c.print(
    "<div id=\"livestatus\" class=\"receiver\">"
      "<div class=\"row\">"
        "<div class=\"col-sm-2\"><label>Throttle:</label></div>"
        "<div class=\"col-sm-10\">"
          "<div class=\"progress\" data-metric=\"v.e.throttle\" data-prec=\"0\">"
            "<div id=\"pb-throttle\" class=\"progress-bar progress-bar-success no-transition text-left\" role=\"progressbar\""
              " aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
              "<div><span class=\"label hidden visible-xs-inline\">Throttle:&nbsp;</span><span class=\"value\">0</span><span class=\"unit\">%</span></div>"
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
  
  c.panel_end();
  
  c.print(
    "<script>"
    "$(\".receiver\").on(\"msg:metrics\", function(e, update){"
      "$(this).find(\"[data-metric]\").each(function(){"
        "var el = $(this), metric = el.data(\"metric\"), prec = el.data(\"prec\");"
        "if (el.hasClass(\"progress\")) {"
          "var pb = $(this.firstElementChild), min = pb.attr(\"aria-valuemin\"), max = pb.attr(\"aria-valuemax\");"
          "var vp = (metrics[metric]-min) / (max-min) * 100;"
          "var vf = (prec != undefined) ? Number(metrics[metric]).toFixed(prec) : metrics[metric];"
          "pb.css(\"width\", vp+\"%\").attr(\"aria-valuenow\", vp);"
          "pb.find(\".value\").text(vf);"
        "} else {"
          "el.text(metrics[metric]);"
        "}"
      "});"
    "}).trigger(\"msg:metrics\");"
    "$(\"#livestatus\").on(\"msg:event\", function(e, event){"
      "if (event == \"vehicle.kickdown.engaged\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-success progress-bar-warning\").addClass(\"progress-bar-danger\");"
      "else if (event == \"vehicle.kickdown.releasing\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-success progress-bar-danger\").addClass(\"progress-bar-warning\");"
      "else if (event == \"vehicle.kickdown.released\")"
        "$(\"#pb-throttle\").removeClass(\"progress-bar-warning progress-bar-danger\").addClass(\"progress-bar-success\");"
    "});"
    "</script>");
  
  c.done();
}


/**
 * WebSevconMon: (/xrt/scmon)
 */
void OvmsVehicleRenaultTwizy::WebSevconMon(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);

  c.print(
    "<style type=\"text/css\">\n"
    ".metric { float:left; }\n"
    ".metric .value { display:inline-block; width:5em; text-align:right; }\n"
    ".metric .unit { display:inline-block; width:3em; }\n"
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
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.p.speed\">?</span><span class=\"unit\">kph</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.m.rpm\">?</span><span class=\"unit\">rpm</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.e.throttle\">?</span><span class=\"unit\">%thr</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.e.footbrake\">?</span><span class=\"unit\">%brk</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Torque</th>\n"
                  "<td>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.trq.limit\">?</span><span class=\"unit\">Nm<sub>lim</sub></span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.trq.demand\">?</span><span class=\"unit\">Nm<sub>dem</sub></span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.trq.act\">?</span><span class=\"unit\">Nm</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Battery</th>\n"
                  "<td>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.vlt.bat\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.current\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.power\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.vlt.cap\">?</span><span class=\"unit\">V<sub>cap</sub></span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Motor</th>\n"
                  "<td>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.vlt.act\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.cur.act\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.pwr.act\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.vlt.mod\">?</span><span class=\"unit\">%mod</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Slip</th>\n"
                  "<td>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.frq.slip\">?</span><span class=\"unit\">rad/s</span></div>\n"
                    "<div class=\"metric\"><span class=\"value\" data-metric=\"xrt.i.frq.output\">?</span><span class=\"unit\">rad/s</span></div>\n"
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
     "* Metrics updates\n"
     "*/\n"
    "\n"
    "$(\"#livestatus\").on(\"msg:metrics\", function(e, update){\n"
      "$(this).find(\"[data-metric]\").each(function(){\n"
        "var val = update[$(this).data(\"metric\")];\n"
        "if (val != null)\n"
          "$(this).text(Number(val).toFixed(1));\n"
      "});\n"
    "}).trigger(\"msg:metrics\", metrics);\n"
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
      "#loadmenu { margin:10px 8px 0; }\n"
      ".fullscreened #loadmenu { margin:10px 10px 0; }\n"
      "</style>\n"
      "\n"
      "<div id=\"loadmenu\" style=\"display:none\">\n"
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
      "$('#main').one('load', function(ev) {\n"
        "if (!loggedin) {\n"
          "$('#loadmenu .btn').prop('disabled', true);\n"
          "$('#loadmenu').append('<a class=\"btn btn-default btn-lg btn-block\" target=\"#main\" href=\"/login?uri=/dashboard\">LOGIN</a>');\n"
        "}\n"
        "$('#loadmenu').appendTo('.panel-dashboard .panel-body').show();\n"
      "});\n"
      "</script>\n");
  }

  return PageResult_OK;
}
