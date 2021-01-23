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

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

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
}

/**
 * WebCfgFeatures: configure general parameters (URL /xvu/config)
 */
void OvmsVehicleVWeUp::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  std::string error, warn;
  std::string modelyear;
  std::string cell_interval_drv, cell_interval_chg, cell_interval_awk;
  bool canwrite;
  bool con_obd;
  bool con_t26;
  bool do_reload = false;

  if (c.method == "POST")
  {
    // process form submission:
    modelyear = c.getvar("modelyear");
    con_obd = (c.getvar("con_obd") == "yes");
    con_t26 = (c.getvar("con_t26") == "yes");
    canwrite = (c.getvar("canwrite") == "yes");
    cell_interval_drv = c.getvar("cell_interval_drv");
    cell_interval_chg = c.getvar("cell_interval_chg");
    cell_interval_awk = c.getvar("cell_interval_awk");

    // check:
    if (!modelyear.empty())
    {
      int n = atoi(modelyear.c_str());
      if (n < 2013)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2013</li>";
    }

    if (!con_obd && !con_t26) {
      warn += "<li><b>No connection type enabled.</b> You won't be able to get live data.</li>";
    }

    if (error == "")
    {
      do_reload =
        MyConfig.GetParamValue("xvu", "modelyear", STR(DEFAULT_MODEL_YEAR)) != modelyear ||
        MyConfig.GetParamValueBool("xvu", "con_obd", true) != con_obd ||
        MyConfig.GetParamValueBool("xvu", "con_t26", true) != con_t26;

      // store:
      MyConfig.SetParamValue("xvu", "modelyear", modelyear);
      MyConfig.SetParamValueBool("xvu", "con_obd", con_obd);
      MyConfig.SetParamValueBool("xvu", "con_t26", con_t26);
      MyConfig.SetParamValueBool("xvu", "canwrite", canwrite);

      if (cell_interval_drv == "15")
        MyConfig.DeleteInstance("xvu", "cell_interval_drv");
      else
        MyConfig.SetParamValue("xvu", "cell_interval_drv", cell_interval_drv);
      if (cell_interval_chg == "60")
        MyConfig.DeleteInstance("xvu", "cell_interval_chg");
      else
        MyConfig.SetParamValue("xvu", "cell_interval_chg", cell_interval_chg);
      if (cell_interval_awk == "60")
        MyConfig.DeleteInstance("xvu", "cell_interval_awk");
      else
        MyConfig.SetParamValue("xvu", "cell_interval_awk", cell_interval_awk);

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
    // read configuration:
    modelyear = MyConfig.GetParamValue("xvu", "modelyear", STR(DEFAULT_MODEL_YEAR));
    con_obd = MyConfig.GetParamValueBool("xvu", "con_obd", true);
    con_t26 = MyConfig.GetParamValueBool("xvu", "con_t26", true);
    canwrite = MyConfig.GetParamValueBool("xvu", "canwrite", false);
    cell_interval_drv = MyConfig.GetParamValue("xvu", "cell_interval_drv");
    cell_interval_chg = MyConfig.GetParamValue("xvu", "cell_interval_chg");
    cell_interval_awk = MyConfig.GetParamValue("xvu", "cell_interval_awk");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Settings");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
    "<p>This sets some parameters that differ for pre 2020 models. I.e. kWh of battery.</p>"
    "<p>This parameter can also be set in the app under FEATURES 20.</p>",
    "min=\"2013\" step=\"1\"");
  c.fieldset_end();

  c.fieldset_start("Connection Types");
  c.input_checkbox("OBD2", "con_obd", con_obd,
    "<p>CAN1 connected to OBD2 port?</p>");
  c.input_checkbox("T26A", "con_t26", con_t26,
    "<p>CAN3 connected to T26A plug underneath passenger seat?</p>");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, OBD2 and climate control depends on this.</p>"
    "<p>This parameter can also be set in the app under FEATURES 15.</p>");
  c.fieldset_end();

  c.fieldset_start("BMS Cell Monitoring", "needs-con-obd");
  c.input_slider("Update interval driving", "cell_interval_drv", 3, "s",
    -1, cell_interval_drv.empty() ? 15 : atof(cell_interval_drv.c_str()),
    15, 0, 300, 1,
    "<p>Default 15 seconds, 0=off.</p>");
  c.input_slider("Update interval charging", "cell_interval_chg", 3, "s",
    -1, cell_interval_chg.empty() ? 60 : atof(cell_interval_chg.c_str()),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
  c.input_slider("Update interval awake", "cell_interval_awk", 3, "s",
    -1, cell_interval_awk.empty() ? 60 : atof(cell_interval_awk.c_str()),
    60, 0, 300, 1,
    "<p>Default 60 seconds, 0=off.</p>");
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

  if (c.method == "POST")
  {
    // process form submission:
    cc_temp = c.getvar("cc_temp");

    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xvu", "cc_temp", cc_temp);

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
    cc_temp = MyConfig.GetParamValue("xvu", "cc_temp", "21");

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
    -1, cc_temp.empty() ? 22 : atof(cc_temp.c_str()),
    22, 15, 30, 1,
    "<p>Default 22 &#8451;, 15=Lo, 30=Hi.</p><br><p>This parameter can also be set in the app under FEATURES 21.</p>");

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
                "aria-valuenow=\"0\" aria-valuemin=\"-10\" aria-valuemax=\"150\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">Current</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.b.power\" data-prec=\"3\">"
              "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"-4\" aria-valuemax=\"60\" style=\"width:0%\">"
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

          "<h4>AC Charger</h4>"

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"v.c.voltage\" data-prec=\"0\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"240\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">AC Voltage</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">V</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.c.current\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"16\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">AC Current</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">A</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"v.c.power\" data-prec=\"3\">"
              "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"8\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">AC Power</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">kW</span>"
                "</div>"
              "</div>"
            "</div>"
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.u1\" data-prec=\"0\">"
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
                  "<span class=\"label\">DC Current</span>"
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
            "<div class=\"metric number\" data-metric=\"v.c.efficiency\" data-prec=\"1\">"
              "<span class=\"label\">Efficiency (total)</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">%</span>"
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

        "</div>" // receiver
      "</div>" // panel-body
    "</div>" // panel
  );

  PAGE_HOOK("body.post");
  c.done();
}
