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
  MyWebServer.RegisterPage("/xrt/battmon", "Battery monitor", WebBattMon, PageMenu_Vehicle, PageAuth_Cookie);
  
  // main menu:
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
    kd_threshold = MyConfig.GetParamValue("xrt", "kd_threshold", XSTR(CFG_DEFAULT_KD_THRESHOLD));
    kd_compzero = MyConfig.GetParamValue("xrt", "kd_compzero", XSTR(CFG_DEFAULT_KD_COMPZERO));
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
    cap_nom_ah = MyConfig.GetParamValue("xrt", "cap_nom_ah", XSTR(CFG_DEFAULT_CAPACITY));
    cap_act_prc = MyConfig.GetParamValue("xrt", "cap_act_prc", "100");
    maxrange = MyConfig.GetParamValue("xrt", "maxrange", XSTR(CFG_DEFAULT_MAXRANGE));
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
  
  c.input("number", "Nominal capacity", "cap_nom_ah", cap_nom_ah.c_str(), "Default: " XSTR(CFG_DEFAULT_CAPACITY),
    "<p>This is the usable capacity of your battery when new.</p>",
    "min=\"1\" step=\"0.1\"", "Ah");
  c.input("number", "Actual capacity", "cap_act_prc", cap_act_prc.c_str(), "Default: 100",
    NULL, "min=\"1\" max=\"120\" step=\"0.01\"", "%");
  
  c.input("number", "Maximum drive range", "maxrange", maxrange.c_str(), "Default: " XSTR(CFG_DEFAULT_MAXRANGE),
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
  c.print(
    "<style>"
    ".btn-default.base { background-color: #fffca8; }"
    ".btn-default.base:hover, .btn-default.base:focus { background-color: #fffa62; }"
    ".unsaved > *:after { content: \"*\"; }"
    "</style>");
  
  c.panel_start("primary panel-single", "Drivemode");
  
  c.printf(
    "<samp id=\"loadres\">%s</samp>", _html(buf.str()));
  
  c.print(
    "<div id=\"livestatus\" class=\"receiver\">"
      "<div class=\"row\">"
        "<div class=\"col-sm-2 hidden-xs\"><label>Throttle:</label></div>"
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
  c.print(
    "</ul></div>");
  
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
 * WebBattMon: (/xrt/battmon)
 */
void OvmsVehicleRenaultTwizy::WebBattMon(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  c.print(
    "<div class=\"panel panel-primary panel-single\">"
    "<div class=\"panel-heading\">Twizy Battery Monitor</div>"
    "<div class=\"panel-body\">"
      "<div class=\"receiver\" id=\"livestatus\">"
        "<div id=\"cellvolts\" style=\"width: 100%; max-width: 800px; height: 45vh; min-height: 265px; margin: 0 auto\"></div>"
        "<div id=\"celltemps\" style=\"width: 100%; max-width: 800px; height: 25vh; min-height: 145px; margin: 0 auto\"></div>"
      "</div>"
    "</div>"
    "<div class=\"panel-footer\">"
      "<button class=\"btn btn-default\" data-cmd=\"xrt batt reset\" data-target=\"#output\" data-watchcnt=\"0\">Reset min/max</button>"
      "<samp id=\"output\" class=\"samp-inline\"></samp>"
    "</div>"
    "</div>"
    ""
    "<style>"
    ""
    "#cellvolts .highcharts-boxplot-box {"
      "fill: #90e8a3;"
      "fill-opacity: 0.6;"
      "stroke: #90e8a3;"
      "stroke-opacity: 0.6;"
    "}"
    ".night #cellvolts .highcharts-boxplot-box {"
      "fill-opacity: 0.4;"
    "}"
    ""
    "#cellvolts .highcharts-boxplot-median {"
      "stroke: #193e07;"
      "stroke-width: 5px;"
      "stroke-linecap: round;"
    "}"
    ".night #cellvolts .highcharts-boxplot-median {"
      "stroke: #afff45;"
    "}"
    ""
    "#cellvolts .highcharts-plot-line {"
      "fill: none;"
      "stroke: #3625c3;"
      "stroke-width: 1px;"
      "stroke-dasharray: 5;"
    "}"
    ".night #cellvolts .highcharts-plot-line {"
      "stroke: #728def;"
    "}"
    ""
    "#celltemps .highcharts-boxplot-box {"
      "fill: #eabe85;"
      "fill-opacity: 0.6;"
      "stroke: #eabe85;"
      "stroke-opacity: 0.6;"
    "}"
    ".night #celltemps .highcharts-boxplot-box {"
      "fill-opacity: 0.4;"
    "}"
    ""
    "#celltemps .highcharts-boxplot-median {"
      "stroke: #7d1313;"
      "stroke-width: 5px;"
      "stroke-linecap: round;"
    "}"
    ".night #celltemps .highcharts-boxplot-median {"
      "stroke: #fdd02e;"
    "}"
    ""
    "#celltemps .highcharts-plot-line {"
      "fill: none;"
      "stroke: #3625c3;"
      "stroke-width: 1px;"
      "stroke-dasharray: 5;"
    "}"
    ".night #celltemps .highcharts-plot-line {"
      "stroke: #728def;"
    "}"
    ""
    "</style>"
    ""
    "<script type=\"text/javascript\">"
    ""
    "var cellvolts;"
    ""
    "function get_cell_data() {"
      "var data = { cells: [], volts: [], vdevs: [], voltmean: 0 };"
      "var i, sum=0, act, min, max, base;"
      "for (i=0; i<metrics[\"xrt.b.cell.cnt\"]; i++) {"
        "base = \"xrt.b.cell.\" + ((i<9) ? \"0\"+(i+1) : (i+1)) + \".volt.\";"
        "act = metrics[base+\"act\"];"
        "min = metrics[base+\"min\"];"
        "max = metrics[base+\"max\"];"
        "sum += act;"
        "data.cells.push(i+1);"
        "data.volts.push([min,min,act,max,max]);"
        "data.vdevs.push(metrics[base+\"maxdev\"]);"
      "}"
      "data.voltmean = sum / metrics[\"xrt.b.cell.cnt\"];"
      "return data;"
    "}"
    ""
    "function update_cell_chart() {"
      "var data = get_cell_data();"
      "cellvolts.xAxis[0].setCategories(data.cells);"
      "cellvolts.yAxis[0].removePlotLine('plot-line-mean');"
      "cellvolts.yAxis[0].addPlotLine({ id: 'plot-line-mean', value: data.voltmean });"
      "cellvolts.series[0].setData(data.volts);"
    "}"
    ""
    "function init_cellvolts() {"
      "var data = get_cell_data();"
      "cellvolts = Highcharts.chart('cellvolts', {"
        "chart: {"
          "type: 'boxplot',"
          "events: {"
            "load: function () {"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){"
                "if (/xrt\\.b\\.cell/.test(Object.keys(update).join()))"
                  "update_cell_chart();"
              "});"
            "}"
          "},"
          "zoomType: 'y',"
          "panning: true,"
          "panKey: 'ctrl',"
        "},"
        "title: { text: null },"
        "credits: { enabled: false },"
        "legend: { enabled: false },"
        "xAxis: { categories: data.cells },"
        "yAxis: {"
          "title: { text: null },"
          "labels: { format: \"{value:.2f}V\" },"
          "minTickInterval: 0.01,"
          "plotLines: [{ id: 'plot-line-mean', value: data.voltmean }],"
          "minorTickInterval: 'auto',"
        "},"
        "series: [{"
          "name: 'Voltage',"
          "data: data.volts,"
          "tooltip: {"
            "headerFormat: '<em>Cell #{point.key}</em><br/>',"
            "pointFormat: ''"
              "+ '● <b>Act: {point.median:.3f}V</b><br/>'"
              "+ '▲ Max: {point.high:.3f}V<br/>'"
              "+ '▼ Min: {point.low:.3f}V<br/>'"
          "},"
          "whiskerLength: 0,"
          "animation: {"
            "duration: 300,"
            "easing: 'easeOutExpo'"
          "},"
        "}]"
      "});"
    "}"
    ""
    ""
    "var celltemps;"
    ""
    "function get_cmod_data() {"
      "var data = { cmods: [], temps: [], tdevs: [], tempmean: 0 };"
      "var i, sum=0, act, min, max, base;"
      "for (i=0; i<metrics[\"xrt.b.cmod.cnt\"]; i++) {"
        "base = \"xrt.b.cmod.\" + ((i<9) ? \"0\"+(i+1) : (i+1)) + \".temp.\";"
        "act = metrics[base+\"act\"];"
        "min = metrics[base+\"min\"];"
        "max = metrics[base+\"max\"];"
        "sum += act;"
        "data.cmods.push(i+1);"
        "data.temps.push([min,min,act,max,max]);"
        "data.tdevs.push(metrics[base+\"maxdev\"]);"
      "}"
      "data.tempmean = sum / metrics[\"xrt.b.cmod.cnt\"];"
      "return data;"
    "}"
    ""
    "function update_cmod_chart() {"
      "var data = get_cmod_data();"
      "celltemps.xAxis[0].setCategories(data.cmods);"
      "celltemps.yAxis[0].removePlotLine('plot-line-mean');"
      "celltemps.yAxis[0].addPlotLine({ id: 'plot-line-mean', value: data.tempmean });"
      "celltemps.series[0].setData(data.temps);"
    "}"
    ""
    "function init_celltemps() {"
      "var data = get_cmod_data();"
      "celltemps = Highcharts.chart('celltemps', {"
        "chart: {"
          "type: 'boxplot',"
          "events: {"
            "load: function () {"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){"
                "if (/xrt\\.b\\.cmod/.test(Object.keys(update).join()))"
                  "update_cmod_chart();"
              "});"
            "}"
          "},"
          "zoomType: 'y',"
          "panning: true,"
          "panKey: 'ctrl',"
        "},"
        "title: { text: null },"
        "credits: { enabled: false },"
        "legend: { enabled: false },"
        "xAxis: { categories: data.cmods },"
        "yAxis: {"
          "title: { text: null },"
          "labels: { format: \"{value:.0f} °C\" },"
          "minTickInterval: 1,"
          "plotLines: [{ id: 'plot-line-mean', value: data.tempmean }],"
          "minorTickInterval: 'auto',"
        "},"
        "series: [{"
          "name: 'Temperature',"
          "data: data.temps,"
          "tooltip: {"
            "headerFormat: '<em>Module #{point.key}</em><br/>',"
            "pointFormat: ''"
              "+ '● <b>Act: {point.median:.0f}°C</b><br/>'"
              "+ '▲ Max: {point.high:.0f}°C<br/>'"
              "+ '▼ Min: {point.low:.0f}°C<br/>'"
          "},"
          "whiskerLength: 0,"
          "animation: {"
            "duration: 300,"
            "easing: 'easeOutExpo'"
          "},"
        "}]"
      "});"
    "}"
    ""
    ""
    "function init_charts() {"
      "init_cellvolts();"
      "init_celltemps();"
    "}"
    ""
    "if (window.Highcharts) {"
      "init_charts();"
    "} else {"
      "$.ajax({"
        "url: \"/assets/charts.js\","
        "dataType: \"script\","
        "cache: true,"
        "success: function(){ init_charts(); }"
      "});"
    "}"
    ""
    "</script>");
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
        "{ from: 50, to: 100, className: 'green-band' }]"
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
