/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform — web UI
   Date:          5th June 2026

   (C) 2026       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_webserver.h"
#include "ovms_utils.h"
#include "metrics_standard.h"

#include "vehicle_toyota_etnga.h"

// A charge-report storage location: a short label (used in links/UI) and its directory.
struct etnga_report_loc { const char* label; const char* dir; };

// Report locations to scan, in precedence order (first wins on a same-name collision):
// the SD card (only when mounted) then internal flash. The write side picks one of
// these (see OvmsVehicleToyotaETNGA::ChargeReportDir()); reading both keeps reports
// visible after the SD card is added or removed. Kept local to avoid a non-static
// call from these static handlers.
static std::vector<etnga_report_loc> etnga_report_locs()
{
    std::vector<etnga_report_loc> locs;
    struct stat st;
    if (stat("/sd", &st) == 0 && S_ISDIR(st.st_mode))
        locs.push_back({ "sd", "/sd/charge-reports" });
    locs.push_back({ "store", "/store/charge-reports" });
    return locs;
}

// Percent-encode a string for use as a URL query-parameter value (RFC 3986 unreserved
// characters pass through). encode_html() is the wrong codec for hrefs: current report
// filenames are plain timestamps, but any future filename scheme with reserved
// characters would silently produce broken links.
static std::string etnga_url_encode(const std::string& s)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

// Map a location label back to its directory, or "" if the label is unknown.
static std::string etnga_report_dir_for(const std::string& loc)
{
    if (loc == "sd")    return "/sd/charge-reports";
    if (loc == "store") return "/store/charge-reports";
    return "";
}

// Friendly name of the active vehicle ("Subaru Solterra" / "Toyota bZ4X") for page titles;
// falls back to the platform name if unavailable.
static std::string etnga_vehicle_name()
{
    const char* n = MyVehicleFactory.ActiveVehicleName();
    return (n && *n) ? std::string(n) : std::string("Toyota e-TNGA");
}

// WebInit: register the e-TNGA pages in the vehicle menu.
// Shared by all e-TNGA vehicles (Subaru Solterra, Toyota bZ4X).
void OvmsVehicleToyotaETNGA::WebInit()
{
    MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xte/charge", "Charging monitor", WebDispChgMetrics, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xte/reports", "Charge reports", WebChargeReports, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xte/report", "Charge report", WebChargeReport, PageMenu_None, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xte/config", "Configuration", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
}

void OvmsVehicleToyotaETNGA::WebDeInit()
{
    MyWebServer.DeregisterPage("/bms/cellmon");
    MyWebServer.DeregisterPage("/xte/charge");
    MyWebServer.DeregisterPage("/xte/reports");
    MyWebServer.DeregisterPage("/xte/report");
    MyWebServer.DeregisterPage("/xte/config");
}

// WebCfgFeatures: configure the e-TNGA TPMS alert thresholds and the battery capacity
// reference (config namespace "xte"). These are otherwise only settable from the shell
// `config` command.
void OvmsVehicleToyotaETNGA::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
    std::string error;
    char buf[32];

    if (c.method == "POST") {
        float p_warn  = atof(c.getvar("tpms_p_warn").c_str());
        float p_alert = atof(c.getvar("tpms_p_alert").c_str());
        float t_warn  = atof(c.getvar("tpms_t_warn").c_str());
        float t_alert = atof(c.getvar("tpms_t_alert").c_str());
        // Blank means "derive from the detected pack", stored as 0. Read the raw strings so a
        // cleared field is distinguishable from a typed 0 -- both mean auto, but only a blank
        // field should stay blank on redisplay.
        std::string nom_ah_s   = c.getvar("bat_nominal_ah");
        std::string nom_volt_s = c.getvar("bat_nominal_volt");
        float nom_ah   = nom_ah_s.empty()   ? 0.0f : atof(nom_ah_s.c_str());
        float nom_volt = nom_volt_s.empty() ? 0.0f : atof(nom_volt_s.c_str());

        // Validate: pressures positive; alert at/below warn (low pressure is worse),
        // temperature alert at/above warn (high temperature is worse).
        if (p_warn <= 0 || p_alert <= 0)
            error += "<li>Tyre pressure thresholds must be greater than zero.</li>";
        if (p_alert > p_warn)
            error += "<li>Pressure alert should be at or below the warning threshold (lower pressure is worse).</li>";
        if (t_alert < t_warn)
            error += "<li>Temperature alert should be at or above the warning threshold (higher temperature is worse).</li>";
        if (nom_ah < 0 || nom_volt < 0)
            error += "<li>Battery nominal capacity and voltage cannot be negative. Leave a field empty to derive it from the detected pack.</li>";

        if (error == "") {
            // Hold the config lock across all writes. OvmsConfig::Transaction is a
            // recursive lock that commits only as the OUTERMOST lock is released, so the
            // Save() operations coalesce into a single config-store write instead of
            // one per setter. Scoped so the lock is released before the response is built.
            {
                auto lock = MyConfig.Lock();
                MyConfig.SetParamValueFloat("xte", "tpms.pressure.warn",  p_warn);
                MyConfig.SetParamValueFloat("xte", "tpms.pressure.alert", p_alert);
                MyConfig.SetParamValueFloat("xte", "tpms.temp.warn",      t_warn);
                MyConfig.SetParamValueFloat("xte", "tpms.temp.alert",     t_alert);
                MyConfig.SetParamValueFloat("xte", "bat.nominal.ah",      nom_ah);
                MyConfig.SetParamValueFloat("xte", "bat.nominal.volt",    nom_volt);
            }

            c.head(200);
            std::string saved = "<p class=\"lead\">" + etnga_vehicle_name() + " configuration saved.</p>";
            c.alert("success", saved.c_str());
        } else {
            error = "<p class=\"lead\">Error:</p><ul class=\"errorlist\">" + error + "</ul>";
            c.head(400);
            c.alert("danger", error.c_str());
        }
    } else {
        c.head(200);
    }

    // Read current config (defaults mirror etnga_tpms.cpp):
    float p_warn  = MyConfig.GetParamValueFloat("xte", "tpms.pressure.warn",  240.0f);
    float p_alert = MyConfig.GetParamValueFloat("xte", "tpms.pressure.alert", 220.0f);
    float t_warn  = MyConfig.GetParamValueFloat("xte", "tpms.temp.warn",       90.0f);
    float t_alert = MyConfig.GetParamValueFloat("xte", "tpms.temp.alert",     100.0f);

    std::string cfgtitle = etnga_vehicle_name() + " configuration";
    c.panel_start("primary", cfgtitle.c_str());
    c.form_start(p.uri);

    c.fieldset_start("TPMS alert thresholds");

    snprintf(buf, sizeof(buf), "%.0f", p_warn);
    c.input("number", "Pressure warning", "tpms_p_warn", buf, "Default: 240",
        "<p>Warn when a tyre drops to this pressure.</p>", "min=\"0\" step=\"1\"", "kPa");
    snprintf(buf, sizeof(buf), "%.0f", p_alert);
    c.input("number", "Pressure alert", "tpms_p_alert", buf, "Default: 220",
        "<p>Alert when a tyre drops to this pressure (at or below the warning level).</p>", "min=\"0\" step=\"1\"", "kPa");
    snprintf(buf, sizeof(buf), "%.0f", t_warn);
    c.input("number", "Temperature warning", "tpms_t_warn", buf, "Default: 90",
        "<p>Warn when a tyre reaches this temperature.</p>", "step=\"1\"", "&deg;C");
    snprintf(buf, sizeof(buf), "%.0f", t_alert);
    c.input("number", "Temperature alert", "tpms_t_alert", buf, "Default: 100",
        "<p>Alert when a tyre reaches this temperature (at or above the warning level).</p>", "step=\"1\"", "&deg;C");

    c.fieldset_end();

    c.fieldset_start("Battery capacity reference");

    // What the module is actually using right now, and where it came from. Worth showing:
    // the whole state-of-health figure hangs on this number matching the pack, and a wrong
    // value fails silently -- it just produces a believable percentage.
    OvmsVehicleToyotaETNGA* v = (OvmsVehicleToyotaETNGA*) MyVehicleFactory.ActiveVehicle();
    int   cells    = v ? v->m_pack_cells : 0;
    float eff_ah   = v ? v->PackNominalAh()   : 0.0f;
    float eff_volt = v ? v->PackNominalVolt() : 0.0f;

    std::string status;
    if (cells == 0) {
        status = "<p>Pack not identified yet &mdash; the cell count is read from the battery ECU, "
                 "which only answers while the car is on or charging.</p>";
    } else {
        snprintf(buf, sizeof(buf), "%d", cells);
        status = std::string("<p>Detected pack: <strong>") + buf + " cells</strong>. ";
        if (eff_ah > 0.0f) {
            snprintf(buf, sizeof(buf), "%.1f", eff_ah);
            status += std::string("In use: <strong>") + buf + " Ah</strong>";
            if (eff_volt > 0.0f) {
                snprintf(buf, sizeof(buf), "%.0f", eff_volt);
                status += std::string(" / <strong>") + buf + " V</strong>";
            }
            status += ".</p>";
        } else {
            status += "No reference capacity is known for this pack, so <code>v.b.soh</code> and "
                      "<code>v.b.capacity</code> are not published. Set the values below to enable "
                      "them.</p>";
        }
    }
    c.print(status.c_str());

    // Blank rather than "0" when unset, so the field reads as "automatic" rather than as a
    // real zero.
    float nom_ah   = MyConfig.GetParamValueFloat("xte", "bat.nominal.ah",   0.0f);
    float nom_volt = MyConfig.GetParamValueFloat("xte", "bat.nominal.volt", 0.0f);
    if (nom_ah > 0.0f) snprintf(buf, sizeof(buf), "%.1f", nom_ah); else buf[0] = 0;
    c.input("number", "Nominal capacity", "bat_nominal_ah", buf, "Automatic",
        "<p>The as-new capacity of this pack, used as the reference for state of health "
        "(<code>v.b.soh</code> = measured &divide; this). Leave empty to derive it from the "
        "detected pack &mdash; only the 96-cell pack is known, so other packs must set it here.</p>"
        "<p><strong>This must match your actual pack.</strong> A wrong value still produces a "
        "believable percentage: reading the same battery against a 195 Ah reference instead of "
        "201.1 Ah moves state of health by 3 points. A result above 100% means this value is "
        "wrong, not that the battery is better than new &mdash; it is shown unclamped so the "
        "mismatch is visible.</p>", "min=\"0\" step=\"0.1\"", "Ah");
    if (nom_volt > 0.0f) snprintf(buf, sizeof(buf), "%.0f", nom_volt); else buf[0] = 0;
    c.input("number", "Nominal voltage", "bat_nominal_volt", buf, "Automatic",
        "<p>Nominal pack voltage, used only to express capacity in kWh "
        "(<code>v.b.capacity</code>). Leave empty to derive it from the detected pack.</p>",
        "min=\"0\" step=\"1\"", "V");

    c.fieldset_end();

    c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
    c.done();
}

// WebDispChgMetrics: live charging monitor. Auto-selects an AC or DC layout from the active
// session (idle when not charging). Metric panels auto-update via the framework data-metric
// mechanism; the chart + state history are seeded from the in-memory session buffers and updated
// live on msg:metrics. The whole body is wrapped in a ".receiver" element (id "chgmon") so the
// framework delivers metric updates to it.
void OvmsVehicleToyotaETNGA::WebDispChgMetrics(PageEntry_t& p, PageContext_t& c)
{
    OvmsVehicleToyotaETNGA* v = (OvmsVehicleToyotaETNGA*) MyVehicleFactory.ActiveVehicle();
    bool in_session = (v && v->m_charge_session.in_session);
    bool dc = (in_session && v->m_charge_session.is_dc);

    c.head(200);
    PAGE_HOOK("body.pre");

    c.print(
        "<style>\n"
        "h6.metric-head { margin-bottom: 0; color: #676767; font-size: 15px; }\n"
        ".night h6.metric-head { color: unset; }\n"
        "#chghist td, #chghist th { padding: .1rem .5rem; font-size: 13px; }\n"
        "</style>\n");

    if (!in_session) {
        c.print(
            "<div class=\"panel panel-primary\">"
              "<div class=\"panel-heading\">");
        c.print(etnga_vehicle_name() + " charging monitor");
        c.print(
              "</div>"
              "<div class=\"panel-body\">"
                "<p>No active charge session.</p>"
                "<p><a class=\"btn btn-default\" href=\"/#/xte/reports\">View saved charge reports</a></p>"
              "</div>"
            "</div>");
        PAGE_HOOK("body.post");
        c.done();
        return;
    }

    c.print(
        "<div class=\"panel panel-primary receiver\" id=\"chgmon\">"
          "<div class=\"panel-heading\">");
    c.print(etnga_vehicle_name() + (dc ? " — DC fast charging" : " — AC charging"));
    c.print(
          "</div>"
          "<div class=\"panel-body\">");

    if (dc) WebChgRenderDc(c, v);
    else    WebChgRenderAc(c, v);

    WebChgChartJs(c, v, dc);
    WebChgStateHistoryJs(c, v, dc);

    c.print(
          "</div>"
        "</div>");

    PAGE_HOOK("body.post");
    c.done();
}

// AC charging metric panels (data-metric widgets auto-update). No station-max (DC-only PID).
void OvmsVehicleToyotaETNGA::WebChgRenderAc(PageContext_t& c, OvmsVehicleToyotaETNGA* v)
{
    c.print(
        "<div class=\"clearfix\">"
          "<div class=\"metric progress\" data-metric=\"v.b.soc\" data-prec=\"1\">"
            "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
              "<div><span class=\"label\">SoC</span><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
            "</div>"
          "</div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Charger (AC)</h6>"
          "<div class=\"metric text\" data-metric=\"v.c.state\"><span class=\"label\">State</span><span class=\"value\">?</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.power\" data-prec=\"3\"><span class=\"label\">Delivered</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"xte.v.c.gridpower\" data-prec=\"3\"><span class=\"label\">Grid input</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.voltage\" data-prec=\"1\"><span class=\"label\">Voltage</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.current\" data-prec=\"1\"><span class=\"label\">Current</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Battery</h6>"
          "<div class=\"metric number\" data-metric=\"v.b.power\" data-prec=\"3\"><span class=\"label\">Power</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.voltage\" data-prec=\"0\"><span class=\"label\">Voltage</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.current\" data-prec=\"1\"><span class=\"label\">Current</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.temp\" data-prec=\"1\"><span class=\"label\">Temp</span><span class=\"value\">?</span><span class=\"unit\">&deg;C</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Session energy</h6>"
          "<div class=\"metric number\" data-metric=\"v.c.kwh\" data-prec=\"3\"><span class=\"label\">Charged</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.kwh.grid\" data-prec=\"3\"><span class=\"label\">From grid</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
          "<div class=\"metric number\" data-metric=\"xte.v.e.hvac.kwh\" data-prec=\"3\"><span class=\"label\">Cabin (My Room)</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
        "</div>");
}

// DC fast-charging metric panels. Adds station + car-permitted; live HLC state shown as text
// (decoded client-side in WebChgStateHistoryJs's label map via a dedicated span updated there).
void OvmsVehicleToyotaETNGA::WebChgRenderDc(PageContext_t& c, OvmsVehicleToyotaETNGA* v)
{
    c.print(
        "<div class=\"clearfix\">"
          "<div class=\"metric progress\" data-metric=\"v.b.soc\" data-prec=\"1\">"
            "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
              "<div><span class=\"label\">SoC</span><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
            "</div>"
          "</div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Handshake</h6>"
          "<div class=\"metric text\"><span class=\"label\">HLC state</span><span class=\"value\" id=\"hlcstate\">?</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Station</h6>"
          "<div class=\"metric number\" data-metric=\"xte.v.c.dc.maxpower\" data-prec=\"1\"><span class=\"label\">Max power</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"xte.v.c.dc.maxvoltage\" data-prec=\"0\"><span class=\"label\">Max V</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
          "<div class=\"metric number\" data-metric=\"xte.v.c.dc.maxcurrent\" data-prec=\"0\"><span class=\"label\">Max A</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.voltage\" data-prec=\"1\"><span class=\"label\">Present V</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
          "<div class=\"metric number\" data-metric=\"v.c.current\" data-prec=\"1\"><span class=\"label\">Present A</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Car</h6>"
          "<div class=\"metric number\" data-metric=\"v.c.power\" data-prec=\"3\"><span class=\"label\">Delivered</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          // Permitted is the |0x16A1| charge limit; the metric is stored signed (negative while
          // charging), so it's updated from JS as an absolute value (id=permkw) like the chart does,
          // rather than bound raw via data-metric — keeps the on-screen number positive.
          "<div class=\"metric number\"><span class=\"label\">Permitted</span><span class=\"value\" id=\"permkw\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"xte.v.c.tgtcurrent\" data-prec=\"0\"><span class=\"label\">Target</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Battery</h6>"
          "<div class=\"metric number\" data-metric=\"v.b.power\" data-prec=\"3\"><span class=\"label\">Power</span><span class=\"value\">?</span><span class=\"unit\">kW</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.voltage\" data-prec=\"0\"><span class=\"label\">Voltage</span><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.current\" data-prec=\"1\"><span class=\"label\">Current</span><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
          "<div class=\"metric number\" data-metric=\"v.b.temp\" data-prec=\"1\"><span class=\"label\">Temp</span><span class=\"value\">?</span><span class=\"unit\">&deg;C</span></div>"
        "</div>"

        "<div class=\"clearfix\">"
          "<h6 class=\"metric-head\">Session energy</h6>"
          "<div class=\"metric number\" data-metric=\"v.c.kwh\" data-prec=\"3\"><span class=\"label\">Charged</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
          // My-Room cabin energy integrates over both AC and DC sessions, so show it here too.
          "<div class=\"metric number\" data-metric=\"xte.v.e.hvac.kwh\" data-prec=\"3\"><span class=\"label\">Cabin (My Room)</span><span class=\"value\">?</span><span class=\"unit\">kWh</span></div>"
        "</div>");
}

// Emit the chart container, an inline JS literal backfill of the in-memory sample buffer, and the
// Highcharts init + live-append wiring. Series mirror the report: delivered power, car-permitted
// (|0x16A1|), station-max (DC only), and SOC on a fixed 0-100 right axis. Power axis fixed per type.
void OvmsVehicleToyotaETNGA::WebChgChartJs(PageContext_t& c, OvmsVehicleToyotaETNGA* v, bool dc)
{
    // Build the backfill literal from the session sample buffer: [t_s, kw, soc, sta_max, car_perm].
    std::string init = "[";
    const std::vector<ChargeSessionState::Sample>& s = v->m_charge_session.svg;
    char b[96];
    for (size_t i = 0; i < s.size(); i++) {
        snprintf(b, sizeof(b), "%s[%d,%.3f,%d,%.2f,%.2f]",
                 (i ? "," : ""), s[i].t_s, s[i].kw, s[i].soc, s[i].sta_max, s[i].car_perm);
        init += b;
    }
    init += "]";

    int elapsed = StandardMetrics.ms_m_monotonic->AsInt() - v->m_charge_session.start_monotonic;
    if (elapsed < 0) elapsed = 0;

    c.print("<h6 class=\"metric-head\">Power &amp; SOC</h6>\n"
            "<div id=\"chgchart\" style=\"width:100%;height:300px\"></div>\n");

    c.printf(
        "<script>\n"
        "(function(){\n"
        "  var DC=%d, PMAX=DC?150:11;\n"
        "  var INIT=%s;\n"
        "  var loadT=Date.now()/1000, baseT=%d;\n"
        "  var deliv=[],perm=[],sta=[],soc=[];\n"
        "  for(var i=0;i<INIT.length;i++){var t=INIT[i][0]/60;\n"
        "    deliv.push([t,INIT[i][1]]); soc.push([t,INIT[i][2]]);\n"
        "    if(DC) sta.push([t,INIT[i][3]]); perm.push([t,Math.abs(INIT[i][4])]);}\n"
        "  var chart=null;\n"
        "  function mv(m){var x=metrics[m]; return (x==null)?null:parseFloat(x);}\n"
        "  function build(){\n"
        "    var series=[{name:'Delivered kW',data:deliv,color:'#0066cc',zIndex:3},\n"
        "                {name:'Car permitted',data:perm,color:'#00aa00',dashStyle:'ShortDash'}];\n"
        "    if(DC) series.push({name:'Station max',data:sta,color:'#ee8800',dashStyle:'ShortDash'});\n"
        "    series.push({name:'SOC %%',data:soc,color:'#3399cc',yAxis:1});\n"
        "    chart=Highcharts.chart('chgchart',{\n"
        "      chart:{type:'line',animation:false},title:{text:null},credits:{enabled:false},\n"
        "      xAxis:{title:{text:'minutes'}},\n"
        "      yAxis:[{title:{text:'kW'},min:0,max:PMAX},\n"
        "             {title:{text:'SOC %%'},min:0,max:100,opposite:true}],\n"
        "      tooltip:{shared:true,valueDecimals:1},\n"
        "      plotOptions:{series:{marker:{enabled:false}}},\n"
        "      series:series});\n"
        "  }\n"
        "  var pm=null;\n"
        "  function showPerm(){if(DC&&pm!=null)$('#permkw').text(Math.abs(pm).toFixed(2));}\n"
        "  function onUpd(){\n"
        "    pm=mv('xte.v.c.permpower'); showPerm();\n"   // update the Permitted panel field even before the chart builds
        "    if(!chart) return;\n"
        "    var t=baseT/60+(Date.now()/1000-loadT)/60;\n"
        "    var d=mv('v.c.power'),sc=mv('v.b.soc'),sm=mv('xte.v.c.dc.maxpower');\n"
        "    var cap=600, sidx=DC?3:2;\n"
        "    if(d!=null) chart.series[0].addPoint([t,d],false,chart.series[0].data.length>cap);\n"
        "    chart.series[1].addPoint([t,pm==null?0:Math.abs(pm)],false,chart.series[1].data.length>cap);\n"
        "    if(DC) chart.series[2].addPoint([t,sm==null?0:sm],false,chart.series[2].data.length>cap);\n"
        "    chart.series[sidx].addPoint([t,sc],true,chart.series[sidx].data.length>cap);\n"
        "  }\n"
        "  function init(){build(); pm=mv('xte.v.c.permpower'); showPerm(); $('#chgmon').on('msg:metrics',onUpd);}\n"
        "  if(window.Highcharts) init();\n"
        "  else $.ajax({url:window.assets.charts_js,dataType:'script',cache:true,success:init});\n"
        "})();\n"
        "</script>\n",
        dc ? 1 : 0, init.c_str(), elapsed);
}
// Emit the state-history table, a backfill literal of the in-memory event log, and JS that seeds
// the table then appends a row whenever the watched state metric changes (xte.v.c.hlcstate on DC,
// xte.v.c.ac.opstatus on AC). On DC it also keeps the #hlcstate header span current. Labels come from
// inline maps so no extra string metric is needed.
void OvmsVehicleToyotaETNGA::WebChgStateHistoryJs(PageContext_t& c, OvmsVehicleToyotaETNGA* v, bool dc)
{
    // Backfill literal from the event log: [relative_seconds, "label"].
    std::string evt = "[";
    const std::vector<std::pair<int,const char*>>& ev = v->m_charge_session.events;
    int start = v->m_charge_session.start_monotonic;
    char b[160];
    for (size_t i = 0; i < ev.size(); i++) {
        int rel = ev[i].first - start; if (rel < 0) rel = 0;
        snprintf(b, sizeof(b), "%s[%d,\"%s\"]", (i ? "," : ""), rel, ev[i].second);
        evt += b;
    }
    evt += "]";

    int elapsed = StandardMetrics.ms_m_monotonic->AsInt() - start;
    if (elapsed < 0) elapsed = 0;
    int curstate = dc ? v->m_v_charge_hlc->AsInt() : v->m_v_charge_ac_op->AsInt();

    c.print(
        "<h6 class=\"metric-head\">Charging state history</h6>\n"
        "<table id=\"chghist\" class=\"table table-condensed\"><thead><tr><th>Time</th><th>State</th></tr></thead><tbody></tbody></table>\n");

    c.printf(
        "<script>\n"
        "(function(){\n"
        "  var DC=%d;\n"
        "  var EVT=%s;\n"
        "  var loadT=Date.now()/1000, baseT=%d, lastState=%d;\n"
        "  var HLC={0:'SLAC',1:'SDP',2:'App-protocol negotiation',3:'Session setup',4:'Service discovery',"
        "5:'Service detail',6:'Payment service selection',7:'Certificate installation',8:'Certificate update',"
        "9:'Payment details',10:'Authorization',11:'Charge-parameter discovery',12:'Cable check',13:'Precharge',"
        "14:'Power delivery (start)',15:'Current demand',16:'Power delivery (stop)',17:'Welding detection',"
        "18:'Session stop',255:'Unconnected'};\n"
        "  var ACOP={1:'Startup',2:'Running',3:'Finishing'};\n"
        "  function fmt(sec){var m=Math.floor(sec/60),s=Math.floor(sec%%60);return m+':'+(s<10?'0':'')+s;}\n"
        "  function row(sec,label){$('#chghist tbody').append('<tr><td>'+fmt(sec)+'</td><td>'+label+'</td></tr>');}\n"
        "  for(var i=0;i<EVT.length;i++) row(EVT[i][0],EVT[i][1]);\n"
        "  if(DC){var hn=HLC[lastState]; if(hn) $('#hlcstate').text(hn);}\n"
        "  function onUpd(){\n"
        "    var m=DC?metrics['xte.v.c.hlcstate']:metrics['xte.v.c.ac.opstatus'];\n"
        "    if(m==null) return; m=parseInt(m);\n"
        "    if(DC){var hn=HLC[m]; if(hn) $('#hlcstate').text(hn);}\n"
        "    if(m===lastState) return; lastState=m;\n"
        "    var label=DC?('HLC: '+(HLC[m]||('state 0x'+m.toString(16)))):(ACOP[m]?('AC: '+ACOP[m]):null);\n"
        "    if(label==null) return;\n"
        "    row(baseT+(Date.now()/1000-loadT),label);\n"
        "  }\n"
        "  $('#chgmon').on('msg:metrics',onUpd);\n"
        "})();\n"
        "</script>\n",
        dc ? 1 : 0, evt.c_str(), elapsed, curstate);
}

// WebChargeReports: index of saved charge-session reports (newest first).
// Each entry links to WebChargeReport, which streams the stored HTML file.
void OvmsVehicleToyotaETNGA::WebChargeReports(PageEntry_t& p, PageContext_t& c)
{
    c.head(200);
    std::string rtitle = etnga_vehicle_name() + " charge reports";
    c.panel_start("primary", rtitle.c_str());

    // Collect .html reports from every storage location. A given session writes to
    // exactly one location, so on the near-impossible event that the same timestamp
    // filename exists in both, the first location scanned (SD) wins.
    std::vector<std::pair<std::string,const char*>> files;   // (filename, location label)
    std::vector<etnga_report_loc> locs = etnga_report_locs();
    for (size_t l = 0; l < locs.size(); l++) {
        DIR* dir = opendir(locs[l].dir);
        if (!dir)
            continue;
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.size() <= 5 || name.compare(name.size() - 5, 5, ".html") != 0)
                continue;
            bool seen = false;
            for (size_t i = 0; i < files.size(); i++)
                if (files[i].first == name) { seen = true; break; }
            if (!seen)
                files.push_back(std::make_pair(name, locs[l].label));
        }
        closedir(dir);
    }

    if (files.empty()) {
        c.print("<p>No charge reports yet. A report is written at the end of each charging session.</p>");
    } else {
        // Newest first: timestamp-prefixed filenames sort chronologically regardless of location.
        std::sort(files.rbegin(), files.rend());
        c.print("<ul class=\"list-unstyled\">");
        for (size_t i = 0; i < files.size(); i++) {
            const char* loc = files[i].second;
            std::string stem = files[i].first.substr(0, files[i].first.size() - 5);   // strip ".html"
            // hrefs take URL-encoding; the visible link text takes HTML-encoding.
            std::string html_url = etnga_url_encode(files[i].first);
            std::string csv_url  = etnga_url_encode(stem + ".csv");
            std::string html_txt = c.encode_html(files[i].first);
            c.printf("<li><a href=\"/xte/report?file=%s&amp;loc=%s\" target=\"_blank\">%s</a> "
                     "&nbsp;<a href=\"/xte/report?file=%s&amp;loc=%s\">csv</a> "
                     "<span class=\"text-muted\">[%s]</span></li>",
                     html_url.c_str(), loc, html_txt.c_str(), csv_url.c_str(), loc, loc);
        }
        c.print("</ul>");
    }

    c.panel_end();
    c.done();
}

// WebChargeReport: stream one saved report as raw HTML or CSV download, via ?file=<name>.
void OvmsVehicleToyotaETNGA::WebChargeReport(PageEntry_t& p, PageContext_t& c)
{
    std::string file = c.getvar("file");

    // Validate: a .html or .csv basename only — reject path traversal.
    bool is_html = (file.size() > 5 && file.compare(file.size()-5, 5, ".html") == 0);
    bool is_csv  = (file.size() > 4 && file.compare(file.size()-4, 4, ".csv")  == 0);
    bool valid = (is_html || is_csv)
                 && file.find('/') == std::string::npos
                 && file.find("..") == std::string::npos;
    if (!valid) {
        c.head(400, "Content-Type: text/plain; charset=utf-8");
        c.print("Invalid report name\n");
        c.done();
        return;
    }

    // Locate the file. Prefer the location named by ?loc=; fall back to scanning every
    // location (older links, or a loc that no longer holds the file) so reports stay
    // reachable across SD insert/remove.
    extram::string content;
    bool loaded = false;
    std::string locdir = etnga_report_dir_for(c.getvar("loc"));
    if (!locdir.empty())
        loaded = (load_file(locdir + "/" + file, content) == 0);
    if (!loaded) {
        std::vector<etnga_report_loc> locs = etnga_report_locs();
        for (size_t l = 0; l < locs.size() && !loaded; l++)
            loaded = (load_file(std::string(locs[l].dir) + "/" + file, content) == 0);
    }
    if (!loaded) {
        c.head(404, "Content-Type: text/plain; charset=utf-8");
        c.print("Report not found\n");
        c.done();
        return;
    }

    if (is_csv)
        c.head(200, "Content-Type: text/csv; charset=utf-8\r\nContent-Disposition: attachment");
    else
        c.head(200, "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-cache");

    // Stream the body in XFER_CHUNK_SIZE pieces rather than appending the whole file to the
    // connection send buffer. A session CSV can reach several MB, and buffering it whole held
    // three concurrent copies (the load_file target, the print() argument, and the mbuf).
    // Ownership passes to the sender, which frees the body and itself when the transfer ends
    // and emits the terminating chunk — so there is deliberately no c.done() here.
    extram::string* body = new extram::string();
    body->swap(content);
    new HttpExtRamStringSender(c.nc, body);
}

#endif // CONFIG_OVMS_COMP_WEBSERVER
