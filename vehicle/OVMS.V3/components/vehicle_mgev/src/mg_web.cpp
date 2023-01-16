/**
 * Project:      Open Vehicle Monitor System
 * Module:      MG ZS EV
 *
 * (C) 2021 Peter Harry <peter.harry56@gmail.com>
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
 * (C) 2018	    Nikolay Shishkov <nshishkov@yahoo.com>
 * (C) 2018	    Geir Øyvind Væidalo <geir@validalo.net>
 * (C) 2017     Michael Balzer <dexter@dexters-web.de>
 * (C) 2018-2020 Tamás Kovács (KommyKT)
 *
 *Changes:
 ;    1.0.0  Initial release:
 ;       - Dashboard modifications
 ;    1.0.1
 ;       - Dashboard modification from 80 cell charge_state
 ;       - Add Ideal range to settings
 ;       - Add 80 cell support for settings
 ;    1.0.4
 ;       - Commands fix
 ;    1.0.6
 ;       - Remove SOH settings
 ;       - Remove ideal range settings
 ;
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

#include <stdio.h>
#include <string>
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_notify.h"

#include "vehicle_mgev.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleMgEv::WebInit()
{
    // vehicle menu:
    MyWebServer.RegisterPage("/xmg/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/xmg/battery",  "Battery config",   WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
    //MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/bms/metrics_charger", "Charging Metrics", WebDispChgMetrics, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleMgEv::WebDeInit()
{
  MyWebServer.DeregisterPage("/xmg/features");
  MyWebServer.DeregisterPage("/bms/metrics_charger");
  MyWebServer.DeregisterPage("/xmg/battery");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xmg/config)
 */
void OvmsVehicleMgEv::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
    std::string error;
    //When we have more versions, need to change this to int and change from checkbox to select
    bool updatedbms = DEFAULT_BMS_VERSION == 1 ? true : false;
    
    if (c.method == "POST") {
        updatedbms = (c.getvar("updatedbms") == "yes");
        
        if (error == "") {
          // store:
          //"Updated" BMS is version 1 (corresponding to BMSDoDLimits array element). "Original" BMS is version 0 (corresponding to BMSDoDLimits array element)
          MyConfig.SetParamValueInt("xmg", "bms.version", updatedbms ? 1 : 0);
          
          c.head(200);
          c.alert("success", "<p class=\"lead\">MG ZS EV / MG5 feature configuration saved.</p>");
          MyWebServer.OutputHome(p, c);
          c.done();
          return;
        }
        // output error, return to form:
        error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
        c.head(400);
        c.alert("danger", error.c_str());
    } else {
        // read configuration:
        switch (MyConfig.GetParamValueInt("xmg", "bms.version", DEFAULT_BMS_VERSION))
        {
          case 0:
            //"Updated" BMS is version 0 (corresponding to BMSDoDLimits array element)
            updatedbms = false;
            break;
          case 1:
            //"Original" BMS is version 1 (corresponding to BMSDoDLimits array element)
            updatedbms = true;
            break;
        }
        c.head(200);
    }
    // generate form:
    c.panel_start("primary", "MG ZS EV / MG5 feature configuration");
    c.form_start(p.uri);

    c.fieldset_start("General");
    //When we have more versions, need to change this to select and updatedbms to int
    c.input_checkbox("Updated BMS Firmware", "updatedbms", updatedbms,
      "<p>Select this if you have BMS Firmware later than Jan 2021</p>");
    c.fieldset_end();
    c.print("<hr>");
    c.input_button("default", "Save");
    c.form_end();
    c.panel_end();
    c.done();
}

/**
 * WebCfgBattery: configure battery parameters (URL /xmg/battery)
 */
void OvmsVehicleMgEv::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  suffsoc              Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange            Sufficient range [km] (Default: 0=disabled)
  std::string suffrange, suffsoc, units_distance;
  float max_range;
  std::string units;
  units_distance = MyConfig.GetParamValue("vehicle", "units.distance");
  
  if (units_distance == "K")
  {
      max_range = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers);
      units = "km";
  }
  else
  {
      max_range = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Miles);
      units = "miles";
  }

  if (c.method == "POST") {
    // process form submission:
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");

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

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xmg", "suffrange", suffrange);
      MyConfig.SetParamValue("xmg", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">MG battery setup saved.</p>");
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
    suffrange = MyConfig.GetParamValue("xmg", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xmg", "suffsoc", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "MG battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, units.c_str(),
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, max_range, 1,
    "<p>Default 0=off. Notify when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * GetDashboardConfig: MG ZS EV specific dashboard setup
 */
void OvmsVehicleMgEv::GetDashboardConfig(DashboardConfig& cfg)
{
    // Speed:
    dash_gauge_t speed_dash(NULL,Kph);
    speed_dash.SetMinMax(0, 135, 5);
    speed_dash.AddBand("green", 0, 60);
    speed_dash.AddBand("yellow", 60, 100);
    speed_dash.AddBand("red", 100, 135);

    // Voltage:
    dash_gauge_t voltage_dash(NULL,Volts);
    voltage_dash.SetMinMax(340, 460);
    voltage_dash.AddBand("red", 340, 380);
    voltage_dash.AddBand("yellow", 380, 420);
    voltage_dash.AddBand("green", 410, 460);

    // SOC:
    dash_gauge_t soc_dash("SOC ",Percentage);
    soc_dash.SetMinMax(10, 100);
    soc_dash.AddBand("red", 10, 15.5);
    soc_dash.AddBand("yellow", 15.5, 25);
    soc_dash.AddBand("green", 25, 100);

    // Efficiency:
    dash_gauge_t eff_dash(NULL,WattHoursPK);
    eff_dash.SetMinMax(0, 600);
    eff_dash.AddBand("green", 0, 200);
    eff_dash.AddBand("yellow", 200, 400);
    eff_dash.AddBand("red", 400, 600);

    // Power:
    dash_gauge_t power_dash(NULL,kW);
    power_dash.SetMinMax(-30, 150);
    power_dash.AddBand("violet", -30, 0);
    power_dash.AddBand("green", 0, 50);
    power_dash.AddBand("yellow", 50, 100);
    power_dash.AddBand("red", 100, 150);

    // Charger temperature:
    dash_gauge_t charget_dash("CHG ",Celcius);
    charget_dash.SetMinMax(-10, 55);
    charget_dash.SetTick(20);
    charget_dash.AddBand("normal", -10, 40);
    charget_dash.AddBand("red", 40, 55);

    // Battery temperature:
    dash_gauge_t batteryt_dash("BAT ",Celcius);
    batteryt_dash.SetMinMax(-15, 65);
    batteryt_dash.SetTick(25);
    batteryt_dash.AddBand("red", -15, 0);
    batteryt_dash.AddBand("normal", 0, 40);
    batteryt_dash.AddBand("red", 40, 65);

    // Inverter temperature:
    dash_gauge_t invertert_dash("PEM ",Celcius);
    invertert_dash.SetMinMax(-10, 55);
    invertert_dash.SetTick(20);
    invertert_dash.AddBand("normal", -10, 40);
    invertert_dash.AddBand("red", 40, 55);

    // Motor temperature:
    dash_gauge_t motort_dash("MOT ",Celcius);
    motort_dash.SetMinMax(20, 100);
    motort_dash.SetTick(25);
    motort_dash.AddBand("normal", 20, 75);
    motort_dash.AddBand("red", 75, 100);

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
}

/**
 * WebPlugin to display charging metrics
 */
void OvmsVehicleMgEv::WebDispChgMetrics(PageEntry_t &p, PageContext_t &c)
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
      "<div class=\"panel-heading\">MG ZS EV Charging Metrics</div>"
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
            "<div class=\"metric progress\" data-metric=\"xmg.v.soc.raw\" data-prec=\"1\">"
              "<div class=\"progress-bar progress-bar-info value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                "aria-valuemin=\"0\" aria-valuemax=\"100\" style=\"width:0%\">"
                "<div>"
                  "<span class=\"label\">SoC RAW</span>"
                  "<span class=\"value\">?</span>"
                  "<span class=\"unit\">%</span>"
                "</div>"
              "</div>"
            "</div>"
          "</div>"
          "<div class=\"clearfix\">"
            "<h6 class=\"metric-head\">Current Status:</h6>"
            "<div class=\"metric number\" data-metric=\"v.b.range.est\" data-prec=\"0\" data-scale=\"0.621371192\">"
              "<span class=\"label\">Range</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">miles</span>"
            "</div>"
            "<div class=\"metric number\" data-metric=\"v.b.range.est\" data-prec=\"0\">"
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
            "<div class=\"metric number\" data-metric=\"v.p.odometer\" data-prec=\"0\" data-scale=\"0.621371192\">"
              "<span class=\"label\">Odometer</span>"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">miles</span>"
              "</div>""<div class=\"metric number\" data-metric=\"v.p.odometer\" data-prec=\"0\">"
              "<span class=\"value\">?</span>"
              "<span class=\"unit\">km</span>"
            "</div>"
          "</div>"

          "<h4>Battery</h4>" // voltage ranges span values for both generations for now (ToDo: switch dynamically)

          "<div class=\"clearfix\">"
            "<div class=\"metric progress\" data-metric=\"v.b.voltage\" data-prec=\"1\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\" aria-valuenow=\"0\""
                "aria-valuemin=\"340\" aria-valuemax=\"455\" style=\"width:0%\">"
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
            "<div class=\"metric number\" data-metric=\"v.b.p.voltage.stddev\" data-prec=\"3\">"
              "<span class=\"label\">Cell deviation</span>"
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
                "aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"32\" style=\"width:0%\">"
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
            "</div>"/*
            "<div class=\"metric progress\" data-metric=\"xvu.c.dc.u1\" data-prec=\"0\">"
              "<div class=\"progress-bar value-low text-left\" role=\"progressbar\""
                "aria-valuenow=\"0\" aria-valuemin=\"340\" aria-valuemax=\"455\" style=\"width:0%\">"
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
            "</div>"*/
          "</div>"

        "</div>" // receiver
      "</div>" // panel-body
    "</div>" // panel
  );

  PAGE_HOOK("body.post");
  c.done();
}
#endif //CONFIG_OVMS_COMP_WEBSERVER
