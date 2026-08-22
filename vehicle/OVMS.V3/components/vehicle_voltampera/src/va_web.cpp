/**
 * Project:      Open Vehicle Monitor System
 * Module:       Volt Ampera Webserver
 *
 * (c) 2019      Marko Juhanne
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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

// #include "ovms_log.h"
// static const char *TAG = "v-voltampera";

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"
#include "ovms_location.h"

#include "vehicle_voltampera.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleVoltAmpera::WebInit()
  {
  MyWebServer.RegisterPage("/xva/controls", "Controls", WebControls, PageMenu_Main, PageAuth_Cookie);
  // vehicle menu:
  MyWebServer.RegisterPage("/xva/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);

  // TODO: Battery monitoring
  //MyWebServer.RegisterPage("/xva/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  }

void OvmsVehicleVoltAmpera::WebCleanup()
  {
  MyWebServer.DeregisterPage("/xva/controls");
  MyWebServer.DeregisterPage("/xva/features");

  // TODO: Battery monitoring
  //MyWebServer.DeregisterPage("/xva/battmon");
  }

/**
 * WebControls: live status and one-click vehicle controls (URL /xva/controls)
 *
 * The page is a front end for the console commands, not a parallel implementation. Everything
 * here just fires the corresponding console command via loadcmd() and shows the reply, so the
 * gating/safeguards in the command implementations apply: notably the 16% SOC refusal
 * on engine stop, and the xva/control.enabled master flag.
 *
 * The status block is a websocket receiver: the framework pushes metric updates into any
 * .metric element inside a .receiver, so the readouts follow the car live without polling.
 */
void OvmsVehicleVoltAmpera::WebControls(PageEntry_t& p, PageContext_t& c)
  {
  c.head(200);

  // The charge-limit marker is light rather than red: red on the orange raw-SoC bar is
  // nearly invisible.
  c.print(
    "<style>"
    ".va-row{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid rgba(127,127,127,.18);}"
    ".va-row:last-child{border-bottom:0;}"
    ".va-row .va-name{flex-grow:1;min-width:0;}"
    ".va-row .va-acts{flex-shrink:0;white-space:nowrap;}"
    ".va-row .va-acts .btn{margin-left:5px;}"
    ".va-chip{display:inline-block;font-size:11px;line-height:1.7;padding:0 8px;border-radius:9px;"
      "border:1px solid rgba(127,127,127,.4);opacity:.9;}"
    ".va-chip.on{border-color:#3c763d;color:#4caf50;}"
    ".va-chip.busy{border-color:#31708f;color:#5bc0de;}"
    ".va-chip.unk{border-style:dashed;opacity:.65;}"
    ".va-sub{font-size:11px;opacity:.6;margin-left:6px;}"
    // bootstrap-theme paints .progress-bar with a background-image gradient, which sits on top
    // of any background-color; without clearing it every bar renders theme blue.
    ".va-bars .progress{position:relative;margin-bottom:8px;}"
    ".va-bars .progress-bar{background-image:none;}"
    ".va-bars .va-raw .progress-bar{background-color:#ff9800;}"
    ".va-bars .va-disp .progress-bar{background-color:#4caf50;}"
    ".va-bars .va-fuel .progress-bar{background-color:#2196f3;}"
    ".va-lim{position:absolute;top:0;bottom:0;width:2px;background:#f2f2f2;z-index:2;}"
    ".va-grp{font-size:11px;text-transform:uppercase;letter-spacing:.08em;opacity:.55;"
      "margin:12px 0 3px;}"
    // .metric.number floats left with a 6em right-aligned label, which flows into one wide
    // row. Re-lay them as label-left / value-right pairs in responsive columns instead.
    // Cap and center the panels themselves, not the inner blocks: without a cap the rows span
    // the whole window on a wide monitor and a label sits hundreds of pixels from its value.
    // Only this page emits the rule, so other pages keep the framework's full-width panels.
    ".panel{max-width:800px;margin-left:auto;margin-right:auto;}"
    // The panel cap above bounds these, so the cells need no cap of their own and leave no
    // gap on the right.
    ".va-stats{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:0 24px;}"
    ".va-stats .metric.number,.va-stats .metric.text{float:none;display:flex;margin:0;"
      "padding:4px 0;align-items:baseline;border-bottom:1px solid rgba(127,127,127,.12);}"
    ".va-stats .metric .label{min-width:0;flex-grow:1;text-align:left;padding:0;}"
    ".va-stats .metric .value{min-width:0;text-align:right;}"
    ".va-stats .metric .unit{min-width:0;margin-left:4px;text-align:left;}"
    "</style>");

  // ---- live status ----------------------------------------------------------------------
  // Three bars, because the car reports two genuinely different state-of-charge scales and
  // reading the charge limit against the wrong one is the mistake this prevents. The limit
  // marker goes on whichever bar the limit is actually measured against, so it follows
  // chargelimit.source rather than sitting on a fixed one and pointing at the wrong scale.
  bool lim_on_raw = (MyConfig.GetParamValue("xva", "chargelimit.source", "raw") != "displayed");
  const char* lim_marker = "<div class=\"va-lim\" id=\"va-lim\" style=\"left:0%;display:none\"></div>";

  c.panel_start("primary", "Status");
  c.print(
    "<div class=\"va-live receiver\" id=\"va-live\">"

      "<div class=\"va-bars\">"
        "<div class=\"metric progress va-raw\" data-metric=\"xva.v.b.soc.raw\" data-prec=\"1\">"
          "<div class=\"progress-bar text-left\" role=\"progressbar\" aria-valuenow=\"0\""
            " aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
            "<div><span class=\"label\">SoC (raw pack)</span>"
            "<span class=\"value\">?</span><span class=\"unit\">%</span></div>"
          "</div>");
  if (lim_on_raw)
    c.print(lim_marker);
  c.print(
        "</div>"
        "<div class=\"metric progress va-disp\" data-metric=\"xva.v.b.soc.displayed\" data-prec=\"1\">"
          "<div class=\"progress-bar text-left\" role=\"progressbar\" aria-valuenow=\"0\""
            " aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
            "<div><span class=\"label\">SoC (dashboard)</span>"
            "<span class=\"value\">?</span><span class=\"unit\">%</span></div>"
          "</div>");
  if (!lim_on_raw)
    c.print(lim_marker);
  c.print(
        "</div>"
        "<div class=\"metric progress va-fuel\" data-metric=\"xva.v.e.fuel\" data-prec=\"0\">"
          "<div class=\"progress-bar text-left\" role=\"progressbar\" aria-valuenow=\"0\""
            " aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
            "<div><span class=\"label\">Fuel</span>"
            "<span class=\"value\">?</span><span class=\"unit\">%</span></div>"
          "</div>"
        "</div>"
      "</div>"

      "<div class=\"va-grp\">Range</div>"
      "<div class=\"va-stats\">"
        "<div class=\"metric number\" data-metric=\"v.b.range.est\" data-prec=\"0\">"
          "<span class=\"label\">EV</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.e.range.fuel\" data-prec=\"0\">"
          "<span class=\"label\">Fuel</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.range.total\" data-prec=\"0\">"
          "<span class=\"label\">Total</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
      "</div>"

      "<div class=\"va-grp\">Charging</div>"
      "<div class=\"va-stats\">"
        "<div class=\"metric text\"><span class=\"label\">State</span>"
          "<span class=\"value\" id=\"st-charge\">?</span></div>"
        "<div class=\"metric number\" data-metric=\"v.c.power\" data-prec=\"2\">"
          "<span class=\"label\">Power</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
        "<div class=\"metric number\" data-metric=\"v.c.kwh\" data-prec=\"2\">"
          "<span class=\"label\">Session</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
        "<div class=\"metric text\" data-metric=\"xva.v.c.level\">"
          "<span class=\"label\">Level</span><span class=\"value\">?</span></div>"
        "<div class=\"metric number\" data-metric=\"v.c.limit.soc\" data-prec=\"0\">"
          "<span class=\"label\">Limit</span><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
      "</div>"

      "<div class=\"va-grp\">Since last charge</div>"
      "<div class=\"va-stats\">"
        "<div class=\"metric number\" data-metric=\"xva.v.dc.dist.total\" data-prec=\"1\">"
          "<span class=\"label\">Distance</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.dc.dist.batt\" data-prec=\"1\">"
          "<span class=\"label\">On battery</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.dc.dist.fuel\" data-prec=\"1\">"
          "<span class=\"label\">On fuel</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.dc.energy.used\" data-prec=\"1\">"
          "<span class=\"label\">Energy</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.dc.fuel.used\" data-prec=\"1\">"
          "<span class=\"label\">Fuel</span><span class=\"value\">?</span><span class=\"unit\">L</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.b.chargecycle_econ\" data-prec=\"1\">"
          "<span class=\"label\">Trip</span><span class=\"value\">?</span><span class=\"unit\">kWh/100km</span></div>"
      "</div>"

      "<div class=\"va-grp\">Health</div>"
      "<div class=\"va-stats\">"
        "<div class=\"metric number\" data-metric=\"v.b.soh\" data-prec=\"0\">"
          "<span class=\"label\">Battery</span><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
        "<div class=\"metric number\" data-metric=\"v.b.capacity\" data-prec=\"1\">"
          "<span class=\"label\">Usable</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
        "<div class=\"metric number\" data-metric=\"v.b.temp\" data-prec=\"0\">"
          "<span class=\"label\">Pack</span><span class=\"value\">?</span><span class=\"unit\">C</span></div>"
        "<div class=\"metric number\" data-metric=\"v.e.temp\" data-prec=\"0\">"
          "<span class=\"label\">Outside</span><span class=\"value\">?</span><span class=\"unit\">C</span></div>"
        "<div class=\"metric number\" data-metric=\"v.b.12v.voltage\" data-prec=\"2\">"
          "<span class=\"label\">12V</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
        "<div class=\"metric number\" data-metric=\"xva.v.e.oil.life\" data-prec=\"1\">"
          "<span class=\"label\">Oil life</span><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
        "<div class=\"metric number\" data-metric=\"v.p.odometer\" data-prec=\"0\">"
          "<span class=\"label\">Odometer</span><span class=\"value\">?</span><span class=\"unit\">km</span></div>"
      "</div>"

    "</div>");
  c.panel_end();

  // ---- controls -------------------------------------------------------------------------
  // One row per thing you can act on: name, state, and BOTH directions, always. State decides
  // which button is highlighted, never whether a button exists: the car stops reporting the
  // moment it sleeps, and a control that disappears or points the wrong way with a stale
  // reading is how you end up unable to shut a window that is standing open.
  c.panel_start("primary", "Controls");
  c.print(
    "<div class=\"va-ctl receiver\" id=\"va-ctl\">"

    "<div class=\"va-row\"><div class=\"va-name\">Doors"
      "<div><span class=\"va-chip\" id=\"ch-lock\">?</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-lock\">Lock</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-unlock\">Unlock</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Climate"
      "<div><span class=\"va-chip\" id=\"ch-hvac\">?</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-clim-on\" data-cmd=\"climatecontrol on\">Start</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-clim-off\" data-cmd=\"climatecontrol off\">Stop</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Windows"
      "<div><span class=\"va-chip\" id=\"ch-win\">?</span><span class=\"va-sub\" id=\"sub-win\"></span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-win-down\">Down</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-win-up\">Up</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Charging"
      "<div><span class=\"va-chip\" id=\"ch-chg\">?</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-chg-start\" data-cmd=\"charge start\">Resume</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-chg-stop\" data-cmd=\"charge stop\">Pause</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Charge to 100% once"
      "<div><span class=\"va-chip\" id=\"ch-ovr\">?</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-chg-full\" data-cmd=\"xva chargelimit override\">Override</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-chg-rearm\" data-cmd=\"xva chargelimit resume\">Re-arm</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Engine"
      "<div><span class=\"va-chip\" id=\"ch-eng\">?</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-eng-on\">On</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-eng-off\">Off</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-eng-rel\" data-cmd=\"xva engine auto\">Auto</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Trunk"
      "<div><span class=\"va-sub\">cannot be closed remotely</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-warning\" id=\"b-trunk\">Release</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Alerts</div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-horn\">Horn</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-flash\" data-cmd=\"xva flash\">Lights</button>"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-locate\">Locate</button>"
      "</div></div>"

    "<div class=\"va-row\"><div class=\"va-name\">Wake"
      "<div><span class=\"va-chip\" id=\"ch-wake\">?</span>"
      "<span class=\"va-sub\">readings are from when it was last awake</span></div></div>"
      "<div class=\"va-acts\">"
        "<button type=\"button\" class=\"btn btn-default\" id=\"b-wake\" data-cmd=\"wakeup\">Wake</button>"
      "</div></div>"

    "</div>"
    "<pre id=\"cmdres\" style=\"margin-top:10px\"></pre>");
  c.panel_end();

  c.print(
    "<script>"
    "function vacmd(cmd){ loadcmd(cmd, \"#cmdres\"); }"
    "var vapin_cache = null;"
    "function vapin(cmd){"
      "if (vapin_cache === null) vapin_cache = prompt(\"Vehicle PIN\");"
      "if (!vapin_cache) { vapin_cache = null; return; }"
      "vacmd(cmd + \" \" + vapin_cache); }"

    "$(\"#va-ctl\").on(\"click\", \"button[data-cmd]\", function(){ vacmd($(this).data(\"cmd\")); });"

    // No data-cmd on these: they need a PIN, a confirmation, or both.
    "$(\"#b-lock\").on(\"click\", function(){ vapin(\"lock\"); });"
    "$(\"#b-unlock\").on(\"click\", function(){ vapin(\"unlock\"); });"
    "$(\"#b-win-down\").on(\"click\", function(){ vapin(\"xva windows down\"); });"
    "$(\"#b-win-up\").on(\"click\", function(){ vapin(\"xva windows up\"); });"
    "$(\"#b-eng-off\").on(\"click\", function(){ vapin(\"xva engine off\"); });"
    "$(\"#b-eng-on\").on(\"click\", function(){"
      "if (confirm(\"Force the internal combustion engine ON? Make sure the car is not in an enclosed space.\"))"
      " vapin(\"xva engine on\"); });"
    "$(\"#b-horn\").on(\"click\", function(){"
      "if (confirm(\"Sound the horn?\")) vacmd(\"xva horn\"); });"
    "$(\"#b-locate\").on(\"click\", function(){"
      "if (confirm(\"Horn and lights together?\")) vacmd(\"xva locate\"); });"
    "$(\"#b-trunk\").on(\"click\", function(){"
      "if (confirm(\"Release the trunk? It cannot be closed again remotely.\")) vapin(\"xva trunk\"); });"

    // Emphasis, not gating: both buttons stay clickable in every state.
    "function lead(a,b){"
      "$(a).removeClass(\"btn-default\").addClass(\"btn-primary\");"
      "$(b).removeClass(\"btn-primary\").addClass(\"btn-default\"); }"
    "function chip(id,txt,cls){ $(id).text(txt).attr(\"class\",\"va-chip\"+(cls?\" \"+cls:\"\")); }"

    "function vaUI(){"
      "var m = metrics, p = m[\"xva.v.e.cmd.pending\"] || \"\";"

      "var lk = m[\"v.e.locked\"];"
      "chip(\"#ch-lock\", p==\"locking\"?\"Locking\":p==\"unlocking\"?\"Unlocking\":lk?\"Locked\":\"Unlocked\","
        "p.indexOf(\"lock\")>=0?\"busy\":lk?\"on\":\"\");"
      "lead(lk?\"#b-unlock\":\"#b-lock\", lk?\"#b-lock\":\"#b-unlock\");"

      "var hv = m[\"v.e.hvac\"];"
      "chip(\"#ch-hvac\", p.indexOf(\"climate\")==0?\"Starting\":hv?\"Running\":\"Off\","
        "p.indexOf(\"climate\")==0?\"busy\":hv?\"on\":\"\");"
      "lead(hv?\"#b-clim-off\":\"#b-clim-on\", hv?\"#b-clim-on\":\"#b-clim-off\");"

      "var w = m[\"xva.v.e.windows\"] || \"\";"
      "var moving = (w==\"opening\"||w==\"closing\"||p.indexOf(\"windows\")==0);"
      "chip(\"#ch-win\", moving?(w==\"closing\"||p==\"windows up\"?\"Closing\":\"Opening\")"
        ":(w==\"open\"?\"Open\":\"Closed\"), moving?\"busy\":(w?\"\":\"unk\"));"
      "var wp=[[\"driver\",m[\"xva.v.e.window.driver\"]],[\"passenger\",m[\"xva.v.e.window.passenger\"]],"
        "[\"rear left\",m[\"xva.v.e.window.rearleft\"]],[\"rear right\",m[\"xva.v.e.window.rearright\"]]];"
      "var op=[]; for (var i=0;i<wp.length;i++){ if (wp[i][1]>0) op.push(wp[i][0]); }"
      "$(\"#sub-win\").text(op.length==0?\"all closed\":op.length==4?\"all open\":op.join(\", \")+\" open\");"
      "if (!moving) lead(w==\"open\"?\"#b-win-up\":\"#b-win-down\", w==\"open\"?\"#b-win-down\":\"#b-win-up\");"

      "var cg = m[\"v.c.charging\"], st = m[\"v.c.state\"] || \"-\", ss = m[\"v.c.substate\"] || \"\";"
      "chip(\"#ch-chg\", ss==\"scheduledstop\"?\"Paused (limit)\":cg?\"Charging\":st, cg?\"on\":\"\");"
      "lead(cg?\"#b-chg-stop\":\"#b-chg-start\", cg?\"#b-chg-start\":\"#b-chg-stop\");"

      "var ov = m[\"xva.v.c.limit.override\"];"
      "chip(\"#ch-ovr\", ov?\"Override on\":\"Armed at \"+(m[\"v.c.limit.soc\"]||\"?\")+\"%\", ov?\"busy\":\"\");"
      "lead(ov?\"#b-chg-rearm\":\"#b-chg-full\", ov?\"#b-chg-full\":\"#b-chg-rearm\");"

      "var em = m[\"xva.v.e.mode\"] || \"auto\", rpm = m[\"v.m.rpm\"] || 0;"
      "chip(\"#ch-eng\", (rpm>0?\"Running \"+rpm+\" rpm\":\"Stopped\")+(em!=\"auto\"?\" (\"+em+\")\":\" (auto)\"),"
        "rpm>0?\"on\":\"\");"
      // Highlight only the active mode: neither forcing the engine on nor off is a suggestion
      // this page should be making.
      "$(\"#b-eng-on,#b-eng-off,#b-eng-rel\").removeClass(\"btn-primary\").addClass(\"btn-default\");"
      "$(em==\"forced-on\"?\"#b-eng-on\":em==\"forced-off\"?\"#b-eng-off\":\"#b-eng-rel\")"
        ".removeClass(\"btn-default\").addClass(\"btn-primary\");"

      "var aw = m[\"v.e.awake\"];"
      "chip(\"#ch-wake\", aw?\"Awake\":\"Asleep\", aw?\"on\":\"unk\");"
      "$(\"#b-wake\").toggleClass(\"btn-primary\", !aw).toggleClass(\"btn-default\", !!aw);"

      // Charge-limit marker. Which bar it lives in was decided server-side from
      // chargelimit.source, so this only has to place it along that bar.
      "var lim = m[\"v.c.limit.soc\"];"
      "if (lim > 0) $(\"#va-lim\").css({left: lim + \"%\", display: \"block\"});"

      "$(\"#st-charge\").text(ss==\"scheduledstop\"?\"paused (limit)\":st);"
    "}"
    "$(\"#va-live,#va-ctl\").on(\"msg:metrics\", function(){ vaUI(); });"
    "$(function(){ vaUI(); });"
    "</script>");

  c.done();
  }

/**
 * WebCfgFeatures: configure general parameters (URL /xva/features)
 */
void OvmsVehicleVoltAmpera::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
  {
  auto lock = MyConfig.Lock();
  std::string error;
  bool preheat_override_BCM;
  bool extended_wakeup;
  bool use_swcan_adapter;
  std::string preheat_max_time;
  std::string range_km;
  std::string bat_capacity;
  bool notify_va_metrics;
  bool control_enabled;
  bool cl_enabled, cl_notify, cl_debug;
  std::string cl_soc, cl_maxdefer, cl_location, cl_source;
  std::string soc_source;

  if (c.method == "POST")
    {
    // process form submission:
    preheat_override_BCM = (c.getvar("preheat_override_BCM") == "yes");
    preheat_max_time = c.getvar("preheat_max_time");
    extended_wakeup = (c.getvar("extended_wakeup") == "yes");
    use_swcan_adapter = (c.getvar("use_swcan_adapter") == "yes");
    range_km = c.getvar("range_km");
    bat_capacity = c.getvar("bat_capacity");
    notify_va_metrics = (c.getvar("notify_va_metrics") == "yes");
    control_enabled = (c.getvar("control_enabled") == "yes");
    cl_enabled = (c.getvar("cl_enabled") == "yes");
    cl_notify = (c.getvar("cl_notify") == "yes");
    cl_debug = (c.getvar("cl_debug") == "yes");
    cl_soc = c.getvar("cl_soc");
    cl_maxdefer = c.getvar("cl_maxdefer");
    cl_location = c.getvar("cl_location");
    soc_source = c.getvar("soc_source");
    cl_source = c.getvar("cl_source");

    // check values
    if (!preheat_max_time.empty())
      {
      int n = atoi(preheat_max_time.c_str());
      if (n < 1 || n > 30)
        error += "<li data-input=\"preheat_max_time\">Maximum run time out of range (1…30) mins</li>";
      }
    if (!cl_soc.empty())
      {
      int n = atoi(cl_soc.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"cl_soc\">Charge limit out of range (0…100 %)</li>";
      }
    if (!cl_maxdefer.empty())
      {
      int n = atoi(cl_maxdefer.c_str());
      if (n < 1 || n > 200)
        error += "<li data-input=\"cl_maxdefer\">Defer attempt limit out of range (1…200)</li>";
      }
    if (!bat_capacity.empty())
      {
      float n = atof(bat_capacity.c_str());
      if (n < 0 || n > 25)
        error += "<li data-input=\"bat_capacity\">Battery capacity out of range (0…25 kWh)</li>";
      }

    if (error == "")
      {
      // store:
      MyConfig.SetParamValueBool("xva", "preheat.override_bcm", preheat_override_BCM);
      MyConfig.SetParamValue("xva", "preheat.max_time", preheat_max_time);
      MyConfig.SetParamValueBool("xva", "extended_wakeup", extended_wakeup);
      MyConfig.SetParamValueBool("xva", "use_swcan_adapter", use_swcan_adapter);
      MyConfig.SetParamValue("xva", "range.km", range_km);
      MyConfig.SetParamValue("xva", "battery.capacity", bat_capacity);
      MyConfig.SetParamValueBool("xva", "notify_va_metrics", notify_va_metrics);
      MyConfig.SetParamValueBool("xva", "control.enabled", control_enabled);
      MyConfig.SetParamValueBool("xva", "chargelimit.enabled", cl_enabled);
      MyConfig.SetParamValue("xva", "chargelimit.soc", cl_soc);
      MyConfig.SetParamValue("xva", "chargelimit.maxdefer", cl_maxdefer);
      MyConfig.SetParamValue("xva", "chargelimit.location", cl_location);
      MyConfig.SetParamValueBool("xva", "chargelimit.notify", cl_notify);
      MyConfig.SetParamValueBool("xva", "chargelimit.debug", cl_debug);
      MyConfig.SetParamValue("xva", "soc.source", soc_source);
      MyConfig.SetParamValue("xva", "chargelimit.source", cl_source);


      c.head(200);
      c.alert("success", "<p class=\"lead\">Volt/Ampera feature configuration saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
      }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
    }
  else 
    {
    // read configuration:
    preheat_override_BCM = MyConfig.GetParamValueBool("xva", "preheat.override_bcm", false);
    preheat_max_time = MyConfig.GetParamValue("xva", "preheat.max_time", STR(VA_PREHEAT_MAX_TIME_DEFAULT));
    extended_wakeup = MyConfig.GetParamValueBool("xva", "extended_wakeup", false);
    use_swcan_adapter = MyConfig.GetParamValueBool("xva", "use_swcan_adapter", false);
    range_km = MyConfig.GetParamValue("xva", "range.km", "0");
    bat_capacity = MyConfig.GetParamValue("xva", "battery.capacity", "0");
    notify_va_metrics = MyConfig.GetParamValueBool("xva", "notify_va_metrics", false);
    control_enabled = MyConfig.GetParamValueBool("xva", "control.enabled", false);
    cl_enabled = MyConfig.GetParamValueBool("xva", "chargelimit.enabled", false);
    cl_soc = MyConfig.GetParamValue("xva", "chargelimit.soc", "80");
    cl_maxdefer = MyConfig.GetParamValue("xva", "chargelimit.maxdefer", "20");
    cl_location = MyConfig.GetParamValue("xva", "chargelimit.location");
    cl_notify = MyConfig.GetParamValueBool("xva", "chargelimit.notify", true);
    cl_debug = MyConfig.GetParamValueBool("xva", "chargelimit.debug", false);
    soc_source = MyConfig.GetParamValue("xva", "soc.source", "displayed");
    cl_source = MyConfig.GetParamValue("xva", "chargelimit.source", "raw");

    c.head(200);
    }

  // generate form
  c.panel_start("primary", "Volt/Ampera feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("SWCAN");
  c.print("<p>SWCAN module support: ");
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  c.print("enabled");
#else
  c.print("disabled");
#endif
  c.print("</p>");
  c.input_checkbox("External SWCAN adapter installed", "use_swcan_adapter", use_swcan_adapter,
    "<p>Use external MCP2515 + TH8056 module as CAN4</p>");
  c.input_checkbox("Enable Extended Wake Up sequence", "extended_wakeup", extended_wakeup,
    "<p>Waking up only the Body Control Module and HVAC module should be enough for our purposes. If for some reason the Remote Start / climate "
    "or other Onstar functions do not work, please enable the Extended Wakeup feature which wakes up all the known modules and mimics better the wakeup sequence "
    "that the MyVoltStar application uses. However there's a longer lag (few seconds) before actions take place. </p>");
  c.fieldset_end();

  // The fieldset is labeled "Remote Start", GM's own name for the feature; it cools the cabin
  // in summer as much as it heats it in winter, so it is climate control rather than
  // preheating. Its config keys are stored under xva/preheat.*.
  c.fieldset_start("Remote Start");
  c.input_checkbox("Enable BCM overriding when invoking Remote Start", "preheat_override_BCM", preheat_override_BCM,
    "<p>Normally Remote start is invoked via key fob or by sending CAN messages that mimic those sent by Onstar module. However this does not "
    "seem to work on certain models (2014 Ampera). By enabling this option the OVMS takes control of the cabin heating/cooling "
    "and overrides the BCM by sending Remote Start CAN messages. Allows us also to set the maximum run time to longer than 20 minutes.</p>"
    "<p>Warning! Currently does not enable the 14V Auxiliary Module, so using this option may cause charge depletion of the 12V battery unless "
    "charging cable is connected!</p>"
    );
  c.input("number", "Maximum run time", "preheat_max_time", preheat_max_time.c_str(), "Default: 20 minutes",
    "<p>How long heating or cooling may run before it is stopped. Note! This only applies when BCM overriding is enabled.</p>",
    "min=\"1\" step=\"1\" max=\"30\"", "minutes");
  c.fieldset_end();

  c.fieldset_start("Charge limit");
  c.input_checkbox("Enable charge limit", "cl_enabled", cl_enabled,
    "<p>Master switch. When off, no charge-control frames are sent at all, whatever the "
    "location setting says. Use this to disable limiting over winter.</p>");
  c.input("number", "Stop charging at", "cl_soc", cl_soc.c_str(), "Default: 80",
    "<p>Charging is paused once the battery reaches this level, and resumed if it falls "
    "more than 2% below it. Read on the scale chosen below.</p>",
    "min=\"0\" step=\"1\" max=\"100\"", "%");

  c.input_select_start("Measured against", "cl_source");
  c.input_select_option("Raw (true pack level)", "raw", cl_source != "displayed");
  c.input_select_option("Displayed (dashboard)", "displayed", cl_source == "displayed");
  c.input_select_end(
    "<p>Raw by default, because capping the charge is a decision about the pack and that is "
    "the scale the factory app uses for the same setting. This is independent of the general "
    "state of charge source under General, so you can watch the dashboard number and still "
    "cap the real level.</p>"
    "<p>The two scales differ by an amount that varies with charge level, so a target means "
    "a different dashboard reading depending on which scale it is measured against. They "
    "happen to track closely around 80%; further from that they do not. See the state of "
    "charge source setting under General for measured pairs.</p>"
    "<p>If the two are set differently, note that apps showing the target next to "
    "<code>v.b.soc</code> are comparing numbers on different scales; "
    "<code>xva chargelimit status</code> always reports both the target and the current "
    "value on the limit's own scale.</p>");

  // A stored location that has since been renamed or deleted would match no option, so the
  // browser would preselect "(anywhere)" and the next save of ANY field on this page would
  // silently widen the limit to everywhere, including public chargers. Keep it listed.
  bool cl_location_known = cl_location.empty();
  for (auto it = MyLocations.m_locations.begin(); it != MyLocations.m_locations.end(); ++it)
    if (cl_location == it->first) cl_location_known = true;

  c.input_select_start("Only at location", "cl_location");
  c.input_select_option("(anywhere)", "", cl_location.empty());
  for (auto it = MyLocations.m_locations.begin(); it != MyLocations.m_locations.end(); ++it)
    c.input_select_option(it->first.c_str(), it->first.c_str(), cl_location == it->first);
  if (!cl_location_known)
    c.input_select_option((cl_location + " (no longer defined)").c_str(),
                          cl_location.c_str(), true);
  c.input_select_end(
    "<p><strong>Inside this location the limit applies</strong> (charging stops at the target "
    "above). <strong>Everywhere else the car charges to 100%</strong>, so a public or "
    "destination charger is never capped. Choose \"(anywhere)\" to apply the limit "
    "regardless of position.</p>"
    "<p>Define locations under Config &rsaquo; Locations. If the GPS fix is unreliable the "
    "limit is <em>not</em> applied, since we cannot tell home from a public charger.</p>");

  c.input("number", "Give up after", "cl_maxdefer", cl_maxdefer.c_str(), "Default: 20",
    "<p>The car re-starts charging on its own, so the limit is re-applied each time. If it "
    "has to fight this many times in one plug-in session it gives up and alerts you, rather "
    "than writing to the bus indefinitely.</p>",
    "min=\"1\" step=\"1\" max=\"200\"", "attempts");
  c.input_checkbox("Notify on pause", "cl_notify", cl_notify,
    "<p>Send a notification when charging is paused, and an alert if the limit gives up.</p>");
  c.input_checkbox("Log every frame (debug)", "cl_debug", cl_debug,
    "<p>Verbose logging of each frame sent. Only useful while validating.</p>");
  c.fieldset_end();

  c.fieldset_start("Vehicle controls");
  c.input_checkbox("Enable vehicle controls", "control_enabled", control_enabled,
    "<p>Required for engine start/stop, trunk and windows on the Controls page. Separate from "
    "the charge limit so you can leave charge control running without arming the actuator "
    "commands.</p>"
    "<p>Engine stop is refused at or below 16% state of charge, to avoid stranding the "
    "pack.</p>");
  c.fieldset_end();

  c.fieldset_start("General");
  c.input("number", "Rated range override", "range_km", range_km.c_str(), "0 = use model default",
    "<p>Overrides the rated range used for the ideal-range calculation.</p>",
    "min=\"0\" step=\"1\" max=\"200\"", "km");
  c.input("number", "Usable battery capacity", "bat_capacity", bat_capacity.c_str(), "0 = derive from the car",
    "<p>Energy available for driving before the engine starts. Left at 0 this is worked "
    "out from the capacity the car reports, so it follows the pack as it ages. Set a value only "
    "if you have measured your own and prefer it.</p>"
    "<p>Related metrics: <code>v.b.cac</code> is the raw amp-hour capacity the car reports, "
    "and <code>v.b.soh</code> the state of health derived from it.</p>",
    "min=\"0\" step=\"0.1\" max=\"25\"", "kWh");
  c.input_select_start("State of charge source", "soc_source");
  c.input_select_option("Displayed (dashboard)", "displayed", soc_source != "raw");
  c.input_select_option("Raw (true pack level)", "raw", soc_source == "raw");
  c.input_select_end(
    "<p>Which scale feeds <code>v.b.soc</code>, and so the app, the server, notifications and "
    "the low-charge alert. Both are always published: <code>xva.v.b.soc.displayed</code> and "
    "<code>xva.v.b.soc.raw</code>.</p>"
    "<p>This does <em>not</em> change the charge limit, which has its own "
    "<em>Measured against</em> setting.</p>"
    "<p>Displayed matches the car's dashboard and is the default; raw is the pack's own "
    "high-resolution figure, and is what the engine-stop safeguard always uses regardless of "
    "this setting.</p>"
    "<p>The two are different scales, not different precisions, and they do not differ by a "
    "fixed amount. Measured on a MY2017 (dash, raw): 20.0/32.4, 50.2/56.8, 78.8/79.4, "
    "95.3/92.7, 99.6/99.9. Raw reads well above the dash at low charge, tracks it closely "
    "near 80, reads below it through the 80s and 90s, then crosses back at the top.</p>");

  c.input_checkbox("Notify on Volt/Ampera metrics", "notify_va_metrics", notify_va_metrics,
    "<p>Send the vehicle-specific metrics with notifications.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  }

#endif //CONFIG_OVMS_COMP_WEBSERVER
