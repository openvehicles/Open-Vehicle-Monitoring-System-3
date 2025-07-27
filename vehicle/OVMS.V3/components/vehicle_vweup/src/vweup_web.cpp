/**
 * Project:      Open Vehicle Monitor System
 * Module:       Webserver for VW e-Up (KCAN / OBD)
 *
 * (c) 2020  sharkcow <sharkcow@gmx.de>
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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR < 4
#define _GLIBCXX_USE_C99 // to enable std::stoi etc.
#endif
#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_vweup.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleVWeUp::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xvu/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xvu/climate", "Climate control", WebCfgClimate, PageMenu_Vehicle, PageAuth_Cookie);
  if (HasOBD()) {
    // only useful with OBD metrics:
    MyWebServer.RegisterPage("/xvu/metrics_charger", "Charging Metrics", WebDispChgMetrics, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xvu/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xvu/battsoh", "Battery Health", WebDispBattHealth, PageMenu_Vehicle, PageAuth_Cookie);
  }
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVWeUp::WebDeInit()
{
  MyWebServer.DeregisterPage("/xvu/features");
  MyWebServer.DeregisterPage("/xvu/climate");
  MyWebServer.DeregisterPage("/xvu/metrics_charger");
  MyWebServer.DeregisterPage("/xvu/battmon");
  MyWebServer.DeregisterPage("/xvu/battsoh");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xvu/config)
 */
void OvmsVehicleVWeUp::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  ConfigParamMap pmap = MyConfig.GetParamMap("xvu");
  ConfigParamMap nmap = pmap;
  std::string error, warn;
  bool do_reload = false;
  int climit = 16;

  if (c.method == "POST")
  {
    // process form submission:
    nmap["modelyear"] = c.getvar("modelyear");
    nmap["con_obd"] = (c.getvar("con_obd") == "yes") ? "yes" : "no";
    nmap["con_t26"] = (c.getvar("con_t26") == "yes") ? "yes" : "no";
    nmap["canwrite"] = (c.getvar("canwrite") == "yes") ? "yes" : "no";
    nmap["bms.autoreset"] = (c.getvar("bms.autoreset") == "yes") ? "yes" : "no";
    nmap["cell_interval_drv"] = c.getvar("cell_interval_drv");
    nmap["cell_interval_chg"] = c.getvar("cell_interval_chg");
    nmap["cell_interval_awk"] = c.getvar("cell_interval_awk");
    nmap["bat.soh.source"] = c.getvar("bat.soh.source");
    nmap["ctp.maxpower"] = c.getvar("ctp_maxpower");
    nmap["chg_climit"] = c.getvar("chg_climit");
    nmap["chg_soclimit"] = c.getvar("chg_soclimit");
    nmap["chg_autostop"] = (c.getvar("chg_autostop") == "yes") ? "yes" : "no";
    nmap["chg_workaround"] = (c.getvar("chg_workaround") == "yes") ? "yes" : "no";

    // check:
    if (nmap["modelyear"] != "")
    {
      int n = std::stoi(nmap["modelyear"]);
      if (n < 2013)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2013</li>";
    }

    if (nmap["con_obd"] != "yes" && nmap["con_t26"] != "yes") {
      warn += "<li><b>No connection type enabled.</b> You won't be able to get live data.</li>";
    }

    if (std::stoi(nmap["cell_interval_awk"]) > 0 && std::stoi(nmap["cell_interval_awk"]) < 30) {
      warn += "<li><b>BMS update intervals below 30 seconds in awake may keep the car awake indefinitely!</b></li>";
    }

    if (error == "")
    {
      do_reload =
        nmap["modelyear"] != pmap["modelyear"]  ||
        nmap["con_obd"]   != pmap["con_obd"]    ||
        nmap["con_t26"]   != pmap["con_t26"];

      // store:
      MyConfig.SetParamMap("xvu", nmap);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up feature configuration saved.</p>");
      if (warn != "") {
        warn = "<p class=\"lead\">Warning:</p><ul class=\"warnlist\">" + warn + "</ul>";
        c.alert("warning", warn.c_str());
      }
      if (do_reload)
        MyWebServer.OutputReconnect(p, c, "Module reconfiguration in progress…");
      else
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
    // fill in defaults:
    if (nmap["modelyear"] == "")
      nmap["modelyear"] = STR(DEFAULT_MODEL_YEAR);
    if (std::stoi(nmap["modelyear"]) > 2019)
      climit = 32;
    if (nmap["con_obd"] == "")
      nmap["con_obd"] = "yes";
    if (nmap["con_t26"] == "")
      nmap["con_t26"] = "yes";
    if (nmap["bat.soh.source"] == "")
      nmap["bat.soh.source"] = "vw";
    if (nmap["chg_autostop"] == "" || nmap["chg_autostop"] == "0")
      nmap["chg_autostop"] = "no";
    else if (nmap["chg_autostop"] == "1")
      nmap["chg_autostop"] = "yes";

    c.head(200);
  }

  // generate form:

  c.panel_start("primary receiver", "VW e-Up feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Settings");
  c.input("number", "Model year", "modelyear", nmap["modelyear"].c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
    "<p>This sets some parameters that differ for pre 2020 models. I.e. kWh of battery.</p>"
    "<p>This parameter can also be set in the app under FEATURES 20.</p>",
    "min=\"2013\" step=\"1\"");
  c.fieldset_end();

  c.fieldset_start("Connection Types");
  c.input_checkbox("OBD2", "con_obd", strtobool(nmap["con_obd"]),
    "<p>CAN1 connected to OBD2 port?</p>");
  c.input_checkbox("T26A", "con_t26", strtobool(nmap["con_t26"]),
    "<p>CAN3 connected to T26A plug underneath passenger seat?</p>");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", strtobool(nmap["canwrite"]),
    "<p>Controls overall CAN write access, OBD2 and climate control depends on this.</p>"
    "<p>This parameter can also be set in the app under FEATURES 15.</p>");
  c.fieldset_end();

  c.fieldset_start("Charge Control");
  c.input_slider("SoC limit", "chg_soclimit", 3, "%",
    -1, nmap["chg_soclimit"].empty() ? 80 : std::stof(nmap["chg_soclimit"]),
    80, 0, 99, 1,
    "<p>Used if no timer mode limits are available, i.e. without OBD connection or without timer schedule.</p>"
    "<p>Value 0 disables the limit.</p>");

  c.input_slider("Charge current limit", "chg_climit", 3, "Amps",
    -1, nmap["chg_climit"].empty()? climit : std::stof(nmap["chg_climit"]),
    climit, 4, climit, 1,
    "<p>Set charge current limit in vehicle (may be reduced further by charging equipment!)</p>");

  c.input_radiobtn_start("Charge limit mode", "chg_autostop");
  c.input_radiobtn_option("chg_autostop", "Notify", "no", nmap["chg_autostop"] == "no");
  c.input_radiobtn_option("chg_autostop", "Stop", "yes", nmap["chg_autostop"] == "yes");
  c.input_radiobtn_end(
    "<p>Only notify or stop charge when SoC limit is reached?</p>"
    "<p>This parameter can also be set in the app under FEATURES 6.</p>");

  c.input_checkbox("Enable charge control workaround", "chg_workaround", strtobool(nmap["chg_workaround"]),
    "<p></p>"
    "<p style=\"color:Tomato;\"><strong>WARNING: In rare cases, this feature might leave the car in a state where it is unable to charge.</strong></p>"
    "<p>This can only be fixed by changing the charge current via a working OVMS or Maps&More.<br>"
    "Only enable this feature if you are aware of the risk and are capable of fixing the issue!<br>"
    "Without this feature, charge control may not work reliably.</p>"
    );

  c.fieldset_end();

  c.fieldset_start("Charge Time Prediction");
  c.input_slider("Standard power level", "ctp_maxpower", 3, "kW",
    -1, nmap["ctp.maxpower"].empty() ? 0 : std::stof(nmap["ctp.maxpower"]),
    0, 0, 30, 0.1,
    "<p>Used to estimate charge times while not charging, default 0 = unlimited (except by car).</p>"
    "<p>Note: this needs to be the power level at the battery, i.e. after losses.</p>"
    "<p>Typical values: 6.5 kW for 2-phase charging, 3.2 kW for 1-phase, 2 kW for ICCB/Schuko.</p>");
  c.fieldset_end();

  c.fieldset_start("Battery Health", "needs-con-obd");
  c.input_radiobtn_start("SOH source", "bat.soh.source");
  c.input_radiobtn_option("bat.soh.source", "Official VW SOH", "vw", (nmap["bat.soh.source"] == "vw"));
  c.input_radiobtn_option("bat.soh.source", "Charge capacity", "charge", (nmap["bat.soh.source"] == "charge"));
  c.input_radiobtn_option("bat.soh.source", "Range estimation", "range", (nmap["bat.soh.source"] == "range"));
  c.input_radiobtn_end(
    "<p><b>Official VW SOH</b> "
    "(currently <span class=\"metric\" data-metric=\"xvu.b.soh.vw\" data-prec=\"1\">?</span>%) "
    "taken directly via OBD from ECU 8C PID 74 CB</p>"
    "<p><b>Charge capacity SOH</b> "
    "(currently <span class=\"metric\" data-metric=\"xvu.b.soh.charge\" data-prec=\"1\">?</span>%) "
    "needs a couple of full charges to settle but tends to be more precise.</p>"
    "<p><b>Range estimation SOH</b> "
    "(currently <span class=\"metric\" data-metric=\"xvu.b.soh.range\" data-prec=\"1\">?</span>%) "
    "is available immediately when switching the car on with at least 70% SOC, but needs a high SOC "
    "for accuracy and is more temperature dependent.</p><p>For more details, see <a target=\"_blank\" "
    "href=\"https://docs.openvehicles.com/en/latest/components/vehicle_vweup/docs/index_obd.html#battery-capacity-soh\""
    ">Battery Capacity &amp; SOH</a>.</p>");
  c.fieldset_end();

  c.fieldset_start("BMS Cell Monitoring", "needs-con-obd");
  c.input_slider("Update interval driving", "cell_interval_drv", 3, "s",
    -1, nmap["cell_interval_drv"].empty() ? 15 : std::stof(nmap["cell_interval_drv"]),
    15, 0, 300, 1,
    "<p>Default 15 seconds, 0=off.</p>");
  c.input_slider("Update interval charging", "cell_interval_chg", 3, "s",
    -1, nmap["cell_interval_chg"].empty() ? 60 : std::stof(nmap["cell_interval_chg"]),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
  c.input_slider("Update interval awake", "cell_interval_awk", 3, "s",
    -1, nmap["cell_interval_awk"].empty() ? 60 : std::stof(nmap["cell_interval_awk"]),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off. Note: an interval below 30 seconds may keep the car awake indefinitely.</p>");
  c.input_checkbox("Auto reset", "bms.autoreset", strtobool(nmap["bms.autoreset"]),
    "<p>Reset cell statistics automatically between driving &amp; charging?</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();

  // Hide/show connection specific features:
  c.print(
    "<script>\n"
    "$('[name=con_obd]').on('change', function() {\n"
      "if ($(this).prop('checked'))\n"
        "$('.needs-con-obd').slideDown();\n"
      "else\n"
        "$('.needs-con-obd').hide();\n"
    "}).trigger('change');\n"
    "</script>\n");

  c.panel_end();
  c.done();
}

/**
 * WebCfgClimate: setup how connexted to the vehicle (URL /xvu/config)
 */
void OvmsVehicleVWeUp::WebCfgClimate(PageEntry_t &p, PageContext_t &c)
{
  std::string error;
  std::string cc_temp;
  ConfigParamMap pmap = MyConfig.GetParamMap("xvu");
  ConfigParamMap nmap = pmap;

  if (c.method == "POST")
  {
    // process form submission:
    cc_temp = c.getvar("cc_temp");
    nmap["cc_temp"] = c.getvar("cc_temp");
    nmap["cc_onbat"] = (c.getvar("cc_onbat") == "yes") ? "yes" : "no";

    if (error == "")
    {
      // store:
      //MyConfig.SetParamValue("xvu", "cc_temp", cc_temp);
      MyConfig.SetParamMap("xvu", nmap);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up climate control configuration saved.</p>");
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
//    cc_temp = MyConfig.GetParamValue("xvu", "cc_temp", "21");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up climate control configuration");
  c.form_start(p.uri);

  c.print(
    "<p>This page offers remote climate configuration.</p><br>"
    "<p>The target temperature for the cabin can be set here (15 to 30 &#8451;).</p><br>");

  c.fieldset_start("Climate control");

  c.input_slider("Cabin target temperature", "cc_temp", 3, "&#8451;",
    -1, nmap["cc_temp"].empty() ? 22 : std::stof(nmap["cc_temp"]),
    22, 15, 30, 1,
    "<p>Default 22 &#8451;, 15=Lo, 30=Hi.</p><br><p>This parameter can also be set in the app under FEATURES 21.</p>");
  
  c.fieldset_start("CC on Battery");
  c.input_checkbox("Enable Climate Control on Battery", "cc_onbat", strtobool(nmap["cc_onbat"]),
    "<p>This parameter can also be set in the app under FEATURES 22.</p>");
  c.fieldset_end();

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebPlugin to display charging metrics
 */
void OvmsVehicleVWeUp::WebDispChgMetrics(PageEntry_t &p, PageContext_t &c)
{
  std::string cmd, output;

  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<style>\n"
    ".wide-metrics .metric.number .label, .wide-metrics .metric.text .label {\n"
      "min-width: 12em;\n"
    "}\n"
    ".wide-metrics .metric.number .value {\n"
      "min-width: 6em;\n"
    "}\n"
    "h6.metric-head {\n"
      "margin-bottom: 0;\n"
      "color: #676767;\n"
      "font-size: 15px;\n"
    "}\n"
    ".night h6.metric-head {\n"
      "color: unset;\n"
    "}\n"
    "</style>\n"
    "<div class=\"panel panel-primary\">"
      "<div class=\"panel-heading\">VW e-Up Charging Metrics</div>"
      "<div class=\"panel-body\">"
        "<div class=\"receiver\">"

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"v.b.soc\" data-prec=\"1\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\" aria-valuemin=\"0\""
                "aria-valuemax=\"100\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">SoC</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">%</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"xvu.b.soc.abs\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-info value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                "aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">SoC absolute</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">%</span>"
                "</div>"
              "</div>"
            "</div>"
          "</div>"
          "<div class=\"clearfix\">"
            "<h6 class=\"metric-head\">Current Status:</h6>"
            "<div class=\"metric number\" data-metric=\"v.b.range.est\" data-prec=\"0\">"
              "<span class=\"label\">Range</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">km</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.c.kwh\" data-prec=\"2\">"
              "<span class=\"label\">Charged</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kWh</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.cac\" data-prec=\"2\">"
              "<span class=\"label\">Capacity</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">Ah</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.soh\" data-prec=\"2\">"
              "<span class=\"label\">SOH</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">%</span>"
            "</div>"
          "</div>"
          "<div class=\"clearfix\">"
            "<h6 class=\"metric-head\">Totals:</h6>"
            "<div class=\"metric number\" data-metric=\"v.b.energy.used.total\" data-prec=\"2\">"
              "<span class=\"label\">Used</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kWh</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.energy.recd.total\" data-prec=\"2\">"
              "<span class=\"label\">Charged</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kWh</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.coulomb.used.total\" data-prec=\"2\">"
              "<span class=\"label\">Used</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">Ah</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.coulomb.recd.total\" data-prec=\"2\">"
              "<span class=\"label\">Charged</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">Ah</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.p.odometer\" data-prec=\"0\">"
              "<span class=\"label\">Odometer</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">km</span>"
            "</div>"
          "</div>"

          "<h4>Battery</h4>" // voltage ranges span values for both generations for now (ToDo: switch dynamically)

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"v.b.voltage\" data-prec=\"1\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                "aria-valuemin=\"275\" aria-valuemax=\"420\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Voltage</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">V</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.b.current\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"-120\" aria-valuemax=\"240\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Current</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.b.power\" data-prec=\"3\">"
              "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"-40\" aria-valuemax=\"80\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Power</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">kW</span>"
                "</div>"
              "</div>"
            "</div>"
          "</div>"
          "<div class=\"clearfix\">"
            "<div class=\"metric number\" data-metric=\"v.b.temp\" data-prec=\"1\">"
              "<span class=\"label\">Temp</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">°C</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"xvu.b.cell.delta\" data-prec=\"3\">"
              "<span class=\"label\">Cell delta</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">V</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.p.voltage.min\" data-prec=\"3\">"
              "<span class=\"label\">Cell min</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">V</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.p.voltage.max\" data-prec=\"3\">"
              "<span class=\"label\">Cell max</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">V</span>"
            "</div>"
          "</div>"

          "<h4>AC/DC Charge Input</h4>"

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"v.c.voltage\" data-prec=\"1\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"420\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Voltage</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">V</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.c.current\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"120\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Current</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.c.power\" data-prec=\"3\">"
              "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"40\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Power</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">kW</span>"
                "</div>"
              "</div>"
            "</div>"
          "</div>"

          "<div class=\"clearfix wide-metrics\">"
            "<div class=\"metric number\" data-metric=\"v.c.efficiency\" data-prec=\"1\">"
              "<span class=\"label\">Efficiency (total)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">%</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.c.kwh.grid\" data-prec=\"2\">"
              "<span class=\"label\">Charged (grid)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kWh</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.c.kwh.grid.total\" data-prec=\"1\">"
              "<span class=\"label\">Charged total (grid)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kWh</span>"
            "</div>"
          "</div>"

          "<h4>AC Charger</h4>"

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.u1\" data-prec=\"1\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"275\" aria-valuemax=\"420\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">DC Voltage</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">V</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.i1\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"9\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">DC Current 1</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.i2\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"9\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">DC Current 2</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.p\" data-prec=\"3\">"
              "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"8\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">DC Power</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">kW</span>"
                "</div>"
              "</div>"
            "</div>"
          "</div>"

          "<div class=\"clearfix wide-metrics\">"
            "<div class=\"metric number\" data-metric=\"v.c.temp\" data-prec=\"1\">"
              "<span class=\"label\">Temp</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">°C</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"xvu.c.eff.calc\" data-prec=\"1\">"
              "<span class=\"label\">Efficiency (charger)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">%</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"xvu.c.loss.calc\" data-prec=\"3\">"
              "<span class=\"label\">Loss (charger)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">kW</span>"
            "</div>"
          "</div>"

        "</div>" // receiver
      "</div>" // panel-body
    "</div>" // panel
  );

  PAGE_HOOK("body.post");
  c.done();
}


/**
 * WebPlugin to display battery health (SOH)
 */
void OvmsVehicleVWeUp::WebDispBattHealth(PageEntry_t &p, PageContext_t &c)
{
  int vweup_modelyear = MyConfig.GetParamValueInt("xvu", "modelyear", DEFAULT_MODEL_YEAR);
  
  c.head(200);
  PAGE_HOOK("body.pre");

  c.printf(
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Battery Health</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"receiver\" id=\"receiver-battsoh\">\n"
          "<table class=\"table table-bordered table-condensed\">\n"
            "<tbody>\n"
              "<tr>\n"
                "<th>SOH</th>\n"
                "<td>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvu.b.soh.vw\">\n"
                  "<span class=\"label\">Official VW</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">%%</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvu.b.soh.charge\">\n"
                  "<span class=\"label\">Charge capacity</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">%%</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvu.b.soh.range\">\n"
                  "<span class=\"label\">Range estimation</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">%%</span>\n"
                "</div>\n"
                "</td>\n"
              "</tr>\n"
              "<tr>\n"
                "<th>Ah</th>\n"
                "<td>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.cac\">\n"
                  "<span class=\"label\">Net capacity</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">Ah</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.coulomb.recd.total\">\n"
                  "<span class=\"label\">Charged total</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">Ah</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"0\" id=\"cycles-ah\">\n"
                  "<span class=\"label\">Charge cycles</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\"></span>\n"
                "</div>\n"
                "</td>\n"
              "</tr>\n"
              "<tr>\n"
                "<th>kWh</th>\n"
                "<td>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.capacity\">\n"
                  "<span class=\"label\">Net capacity</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">kWh</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.energy.recd.total\">\n"
                  "<span class=\"label\">Charged total</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\">kWh</span>\n"
                "</div>\n"
                "<div class=\"metric number\" data-prec=\"0\" id=\"cycles-kwh\">\n"
                  "<span class=\"label\">Charge cycles</span>\n"
                  "<span class=\"value\">0.0</span>\n"
                  "<span class=\"unit\"></span>\n"
                "</div>\n"
                "</td>\n"
              "</tr>\n"
            "</tbody>\n"
          "</table>\n"
          "<h4>Cell Level</h4>\n"
          "<div class=\"metric chart\" data-metric=\"xvu.b.c.soh,xvu.b.soh.vw\" style=\"width: 100%%; max-width: 100%%; height: 45vh; min-height: 280px; margin: 0 auto\">\n"
            "<div class=\"chart-box barchart\" id=\"cell-soh\"/>\n"
          "</div>\n"
          "<h4>Cell Module History</h4>\n"
          "<div class=\"metric chart\" data-metric=\"xvu.b.hist.soh.mod.01,xvu.b.hist.soh.mod.02,xvu.b.hist.soh.mod.03,xvu.b.hist.soh.mod.04,xvu.b.hist.soh.mod.05,xvu.b.hist.soh.mod.06,xvu.b.hist.soh.mod.07,xvu.b.hist.soh.mod.08,xvu.b.hist.soh.mod.09,xvu.b.hist.soh.mod.10,xvu.b.hist.soh.mod.11,xvu.b.hist.soh.mod.12,xvu.b.hist.soh.mod.13,xvu.b.hist.soh.mod.14,xvu.b.hist.soh.mod.15,xvu.b.hist.soh.mod.16,xvu.b.hist.soh.mod.17\" style=\"width: 100%%; max-width: 100%%; height: 45vh; min-height: 280px; margin: 0 auto\">\n"
            "<div class=\"chart-box linechart\" id=\"cmod-hist\"/>\n"
          "</div>\n"
          "<div>Hint: hover/long click on legend to highlight, click to toggle</div>\n"
        "</div>\n"
      "</div>\n"
    "</div>\n"
    "<style>\n"
    ".metric.number {\n"
      "display: flex;\n"
      "align-items: baseline;\n"
    "}\n"
    ".metric.number .label {\n"
      "min-width: 10em;\n"
    "}\n"
    ".metric.number .label:after {\n"
      "content: ':';\n"
    "}\n"
    ".metric.number .value {\n"
      "min-width: 5em;\n"
    "}\n"
    ".plot-line-packsoh {\n"
      "stroke: #0074ff;\n"
      "stroke-width: 2px;\n"
    "}\n"
    ".highcharts-series.cmod-soh-avg > .highcharts-graph {\n"
      "stroke-width: 15px;\n"
      "opacity: 0.6;\n"
    "}\n"
    "</style>\n"
    "<script>\n"
    "(function(){\n"
      "/* Car model properties */\n"
      "var car = { cap_gross_ah: %.1f, cap_gross_kwh: %.1f };\n"
      "/* Metrics derived displays */\n"
      "$('#receiver-battsoh').on('msg:metrics', function(ev, upd) {\n"
        "if (upd[\"v.b.coulomb.recd.total\"]) {\n"
          "$(\"#cycles-ah .value\").text(Number(upd[\"v.b.coulomb.recd.total\"] / car.cap_gross_ah).toFixed(1));\n"
        "}\n"
        "if (upd[\"v.b.energy.recd.total\"]) {\n"
          "$(\"#cycles-kwh .value\").text(Number(upd[\"v.b.energy.recd.total\"] / car.cap_gross_kwh).toFixed(1));\n"
        "}\n"
      "});\n"
      "/* Init cell SOH chart */\n"
      "$(\"#cell-soh\").chart({\n"
        "chart: {\n"
          "type: 'column',\n"
          "animation: { duration: 500, easing: 'easeOutExpo' },\n"
          "zoomType: 'xy',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "tooltip: {\n"
          "enabled: true,\n"
          "shared: true,\n"
          "headerFormat: 'Cell #{point.key}:<br/>',\n"
          "pointFormat: '{series.name}: <b>{point.y}</b><br/>',\n"
          "valueSuffix: \" %%\"\n"
        "},\n"
        "legend: { enabled: true },\n"
        "xAxis: {\n"
          "categories: []\n"
        "},\n"
        "yAxis: [{\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.0f}%%\" },\n"
          "min: 75, max: 110,\n"
          "minTickInterval: 0.1,\n"
          "minorTickInterval: 'auto',\n"
        "}],\n"
        "series: [{\n"
          "name: 'SOH', data: [50],\n"
          "className: 'cell-soh',\n"
          "animation: { duration: 0 },\n"
        "}],\n"
        "/* Update method: */\n"
        "onUpdate: function(update) {\n"
          "let m_packsoh = metrics[\"xvu.b.soh.vw\"] || 100;\n"
          "let m_cellsoh = metrics[\"xvu.b.c.soh\"] || [];\n"
          "// Create categories (cell numbers) & rounded values:\n"
          "let cat = [], val0 = [];\n"
          "for (let i = 0; i < m_cellsoh.length; i++) {\n"
            "cat.push(i+1);\n"
            "val0.push(Number((m_cellsoh[i]||0).toFixed(1)));\n"
          "}\n"
          "// Update chart:\n"
          "this.xAxis[0].setCategories(cat);\n"
          "this.yAxis[0].removePlotLine('plot-line-packsoh');\n"
          "this.yAxis[0].addPlotLine({ id: 'plot-line-packsoh', className: 'plot-line-packsoh', value: m_packsoh, zIndex: 3 });\n"
          "this.series[0].setData(val0);\n"
          "let yext = this.yAxis[0].getExtremes();\n"
          "let ymin = Math.max(0, Math.floor((yext.dataMin-10) / 5) * 5);\n"
          "let ymax = Math.min(110, Math.ceil((yext.dataMax+1) / 5) * 5);\n"
          "this.yAxis[0].update({ min: ymin, max: ymax });\n"
          "this.yAxis[0].setExtremes(ymin, ymax);\n"
        "},\n"
      "});\n"
      "/* Init cell module history chart */\n"
      "$(\"#cmod-hist\").chart({\n"
        "chart: {\n"
          "type: 'line',\n"
          "animation: { duration: 500, easing: 'easeOutExpo' },\n"
          "zoomType: 'xy',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "tooltip: {\n"
          "enabled: true,\n"
          "shared: true,\n"
          "headerFormat: '<u>{point.key}</u>:<br/>',\n"
          "pointFormat: '{series.name}: <b>{point.y}</b><br/>',\n"
          "valueSuffix: \" %%\"\n"
        "},\n"
        "legend: { enabled: true },\n"
        "xAxis: {\n"
          "categories: []\n"
        "},\n"
        "yAxis: [{\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.0f}%%\" },\n"
          "min: 75, max: 110,\n"
          "minTickInterval: 0.1,\n"
          "minorTickInterval: 'auto',\n"
        "}],\n"
        "series: [],\n"
        "/* Update method: */\n"
        "onUpdate: function(update) {\n"
          "// Create module series & average:\n"
          "let series = [], avg = [];\n"
          "for (let i = 1; i <= 17; i++) {\n"
            "let modkey = Number(i).toString().padStart(2,'0');\n"
            "let modhist = metrics[\"xvu.b.hist.soh.mod.\"+modkey] || [];\n"
            "if (modhist.length > 0) {\n"
              "series.push({\n"
                "name: modkey, data: modhist,\n"
                "className: 'cmod-soh', zIndex: i,\n"
                "animation: { duration: 0 },\n"
              "});\n"
              "// prep avg:\n"
              "modhist.forEach((el,i) => { avg[i] = (avg[i]||0) + el });\n"
            "}\n"
          "}\n"
          "// finish avg:\n"
          "if (series.length) {\n"
            "avg.forEach((el,i) => { avg[i] = Number((avg[i] / series.length).toFixed(1)) });\n"
            "series.unshift({\n"
              "name: \"Avg\", data: avg,\n"
              "className: 'cmod-soh-avg', zIndex: 100,\n"
              "animation: { duration: 0 },\n"
            "});\n"
          "}\n"
          "// Create categories (cell numbers) & rounded values:\n"
          "let date = new Date();\n"
          "let cat = [];\n"
          "for (let i = avg.length-1; i >= 0; i--) {\n"
            "cat[i] = date.toISOString().substr(0,7);\n"
            "date.setMonth(date.getMonth() - 3);\n"
          "}\n"
          "// Update chart:\n"
          "this.xAxis[0].setCategories(cat);\n"
          "this.update({ \"series\": series }, true, true);\n"
          "let yext = this.yAxis[0].getExtremes();\n"
          "let ymin = Math.max(0, Math.floor((yext.dataMin-10) / 5) * 5);\n"
          "let ymax = Math.min(110, Math.ceil((yext.dataMax+1) / 5) * 5);\n"
          "this.yAxis[0].update({ min: ymin, max: ymax });\n"
          "this.yAxis[0].setExtremes(ymin, ymax);\n"
        "},\n"
      "});\n"
    "})();\n"
    "</script>\n"
    ,
    /* Car model properties */
    /* cap_gross_ah: */     (vweup_modelyear > 2019) ? 120.0 : 50.0,
    /* cap_gross_kwh: */    (vweup_modelyear > 2019) ? 36.8 : 18.7
  );

  PAGE_HOOK("body.post");
  c.done();
}


/**
 * GetDashboardConfig: VW eUp specific dashboard setup
 */
void OvmsVehicleVWeUp::GetDashboardConfig(DashboardConfig& cfg)
{
  // Get model specific battery voltage range:
  int cells = (vweup_modelyear > 2019) ? 84 : 102;
  float
    bat_volt_max      = cells * 4.2,
    bat_volt_yellow   = cells * 3.5,
    bat_volt_red      = cells * 3.4,
    bat_volt_min      = cells * 3.3;

  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 140, 5);
  speed_dash.AddBand("green", 0, 100);
  speed_dash.AddBand("yellow", 100, 120);
  speed_dash.AddBand("red", 120, 140);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(bat_volt_min, bat_volt_max);
  voltage_dash.AddBand("red", bat_volt_min, bat_volt_red);
  voltage_dash.AddBand("yellow", bat_volt_red, bat_volt_yellow);
  voltage_dash.AddBand("green", bat_volt_yellow, bat_volt_max);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 300);
  eff_dash.AddBand("green", 0, 150);
  eff_dash.AddBand("yellow", 150, 250);
  eff_dash.AddBand("red", 250, 300);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-40, 80);
  power_dash.AddBand("violet", -40, 0);
  power_dash.AddBand("green", 0, 40);
  power_dash.AddBand("yellow", 40, 60);
  power_dash.AddBand("red", 60, 80);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ",Celcius);
  charget_dash.SetMinMax(20, 80);
  charget_dash.SetTick(20);
  charget_dash.AddBand("normal", 20, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ",Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 50);
  batteryt_dash.AddBand("red", 50, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ",Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ",Celcius);
  motort_dash.SetMinMax(50, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 50, 110);
  motort_dash.AddBand("red", 110, 125);

  std::ostringstream str;
  str << "yAxis: ["
      << speed_dash << "," // Speed
      << voltage_dash << "," // Voltage
      << soc_dash << "," // SOC
      << eff_dash << "," // Efficiency
      << power_dash << "," // Power
      << charget_dash << "," // Charger temperature
      << batteryt_dash << "," // Battery temperature
      << invertert_dash << "," // Inverter temperature
      << motort_dash // Motor temperature
      << "]";
  cfg.gaugeset1 = str.str();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
