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

#include "vweup.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleVWeUpAll::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xua/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xua/climate", "Climate control", WebCfgClimate, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xua/metrics_std", "Standard Metrics", WebDispStdMetrics, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVWeUpAll::WebDeInit()
{
  MyWebServer.DeregisterPage("/xua/features");
  MyWebServer.DeregisterPage("/xua/climate");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xua/config)
 */
void OvmsVehicleVWeUpAll::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  std::string error;
  std::string modelyear;
  bool canwrite;
  bool con_obd;
  bool con_t26;

  if (c.method == "POST")
  {
    // process form submission:
    modelyear = c.getvar("modelyear");
    con_obd = (c.getvar("con_obd") == "yes");
    con_t26 = (c.getvar("con_t26") == "yes");
    canwrite = (c.getvar("canwrite") == "yes");

    // check:
    if (!modelyear.empty())
    {
      int n = atoi(modelyear.c_str());
      if (n < 2013)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2013</li>";
    }

    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xua", "modelyear", modelyear);
      MyConfig.SetParamValueBool("xua", "con_obd", con_obd);
      MyConfig.SetParamValueBool("xua", "con_t26", con_t26);
      MyConfig.SetParamValueBool("xua", "canwrite", canwrite);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up feature configuration saved.</p>");
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
    modelyear = MyConfig.GetParamValue("xua", "modelyear", STR(DEFAULT_MODEL_YEAR));
    con_obd = MyConfig.GetParamValueBool("xua", "con_obd", true);
    con_t26 = MyConfig.GetParamValueBool("xua", "con_t26", true);
    canwrite = MyConfig.GetParamValueBool("xua", "canwrite", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Settings");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
          "<p>Necessary for distinction of vehicle generation, e.g. concerning battery capacity.</p>",
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
                   "<p>Controls overall CAN write access, climate control depends on this.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebCfgClimate: setup how connexted to the vehicle (URL /xua/config)
 */
void OvmsVehicleVWeUpAll::WebCfgClimate(PageEntry_t &p, PageContext_t &c)
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
      MyConfig.SetParamValue("xua", "cc_temp", cc_temp);

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
    cc_temp = MyConfig.GetParamValue("xua", "cc_temp", "21");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up climate control configuration");
  c.form_start(p.uri);

  c.print("<br>This page offers remote climate configuration.<br>The target temperature for the cabin can be set here.<br><br>");

  c.fieldset_start("Climate control");

  c.input_select_start("Cabin target temperature", "cc_temp");
  c.input_select_option("18", "18", cc_temp == "18");
  c.input_select_option("19", "19", cc_temp == "19");
  c.input_select_option("20", "20", cc_temp == "20");
  c.input_select_option("21", "21", cc_temp == "21");
  c.input_select_option("22", "22", cc_temp == "22");
  c.input_select_option("23", "23", cc_temp == "23");
  c.input_select_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebPlugin to display standard metrics
 */
void OvmsVehicleVWeUpAll::WebDispStdMetrics(PageEntry_t &p, PageContext_t &c)
{
  std::string cmd, output;

  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<div class=\"panel panel-primary\">"
        "<div class=\"panel-heading\">VW e-Up Metrics</div>"
        "<div class=\"panel-body\">"

            "<div class=\"receiver\">"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.b.soc\" data-prec=\"2\">"
                        "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\" aria-valuemin=\"0\""
                            "aria-valuemax=\"100\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">SoC</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">%</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                    "<div class=\"metric progress\" data-metric=\"xua.b.soc\" data-prec=\"2\">"
                        "<div class=\"progress-bar progress-bar-info value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                            "aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">SoC (absolute)</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">%</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric number\" data-metric=\"v.b.energy.used.total\" data-prec=\"3\">"
                        "<span class=\"label\">TOTALS:&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbspUsed</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">kWh</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"v.b.energy.recd.total\" data-prec=\"3\">"
                        "<span class=\"label\">Charged</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">kWh</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"v.p.odometer\" data-prec=\"1\">"
                        "<span class=\"label\">Distance</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">km</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"v.b.range.est\" data-prec=\"0\">"
                        "<span class=\"label\">Range</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">km</span>"
                    "</div>"
                "</div>"

                "<h4>Battery</h4>"

                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.b.voltage\" data-prec=\"1\">"
                        "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                            "aria-valuemin=\"276\" aria-valuemax=\"418\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Voltage</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">V</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                    "<div class=\"metric progress\" data-metric=\"v.b.current\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-danger value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-200\" aria-valuemax=\"200\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Current</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">A</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                    "<div class=\"metric progress\" data-metric=\"v.b.power\" data-prec=\"3\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-70\" aria-valuemax=\"70\" style=\"width:0%\">"
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
                    "<div class=\"metric number\" data-metric=\"xua.b.cell.delta\" data-prec=\"3\">"
                        "<span class=\"label\">Cell delta</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">V</span>"
                    "</div>"
                "</div>"

                "<h4>AC Charger</h4>"

                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"xua.c.ac.p\" data-prec=\"3\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"8\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">AC Power</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">kW</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                    "<div class=\"metric progress\" data-metric=\"xua.c.dc.p\" data-prec=\"3\">"
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
                "<div class=\"clearfix\">"
                    "<div class=\"metric number\" data-metric=\"v.c.efficiency\" data-prec=\"1\">"
                        "<span class=\"label\">Efficiency (total)</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">%</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.c.eff.calc\" data-prec=\"1\">"
                        "<span class=\"label\">Efficiency (charger)</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">%</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.c.loss.calc\" data-prec=\"3\">"
                        "<span class=\"label\">Loss (charger)</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">kW</span>"
                    "</div>"
                "</div>"

                "<h4>Next Maintenance</h4>"

                "<div class=\"clearfix\">"
                    "<div class=\"metric number\" data-metric=\"xua.v.m.d\" data-prec=\"0\">"
                        "<span class=\"label\">Distance</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">km</span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.v.m.t\" data-prec=\"0\">"
                        "<span class=\"label\">Time</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\">d</span>"
                    "</div>"
                "</div>"
                
                "<h4>Temperatures</h4>"

                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.e.temp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Ambient</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.b.temp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Battery</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.e.cabintemp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Cabin</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.c.temp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Charger</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.i.temp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Inverter</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"
                "<div class=\"clearfix\">"
                    "<div class=\"metric progress\" data-metric=\"v.m.temp\" data-prec=\"1\">"
                        "<div class=\"progress-bar progress-bar-warning value-low text-left\" role=\"progressbar\""
                            "aria-valuenow=\"0\" aria-valuemin=\"-20\" aria-valuemax=\"60\" style=\"width:0%\">"
                            "<div>"
                                "<span class=\"label\">Motor</span>"
                                "<span class=\"value\">?</span>"
                                "<span class=\"unit\">°C</span>"
                            "</div>"
                        "</div>"
                    "</div>"

                "<h4>TPMS</h4>"

                "<div class=\"clearfix\">"
                    "<div class=\"metric number\" data-metric=\"xua.v.tp.d.fl\" data-prec=\"0\">"
                        "<span class=\"label\">Diffusion FL</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\"></span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.v.tp.d.fr\" data-prec=\"0\">"
                        "<span class=\"label\">Diffusion FR</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\"></span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.v.tp.d.rl\" data-prec=\"0\">"
                        "<span class=\"label\">Diffusion RL</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\"></span>"
                    "</div>"
                    "<div class=\"metric number\" data-metric=\"xua.v.tp.d.rr\" data-prec=\"0\">"
                        "<span class=\"label\">Diffusion RR</span>"
                        "<span class=\"value\">?</span>"
                        "<span class=\"unit\"></span>"
                    "</div>"
                "</div>"
                
            "</div>"
        "</div>"
    "</div>"
   );
    PAGE_HOOK("body.post");
    c.done();
}