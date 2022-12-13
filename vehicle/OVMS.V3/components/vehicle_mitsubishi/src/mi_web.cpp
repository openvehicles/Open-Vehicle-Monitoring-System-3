/**
 * Project:      Open Vehicle Monitor System
 * Module:       Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn Webserver
 *
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
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_mitsubishi.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleMitsubishi::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xmi/features", "Settings", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebCfgFeatures: configure general parameters (URL /xmi/config)
 */
void OvmsVehicleMitsubishi::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error,soh,ideal;
  bool oldheater,newcell;

  if (c.method == "POST")
  {
    // process form submission:
    oldheater = (c.getvar("oldheater") == "yes");
    newcell = (c.getvar("newcell") == "yes");
    // check:
    if (error == "")
    {
      // store:
      MyConfig.SetParamValueBool("xmi", "oldheater", oldheater);
      MyConfig.SetParamValueBool("xmi","newcell",newcell);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Trio feature configuration saved.</p>");
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
    oldheater = MyConfig.GetParamValueBool("xmi", "oldheater", false);
    newcell = MyConfig.GetParamValueBool("xmi","newcell", false);
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Trio feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Heater");
  c.input_checkbox("Heater 1. generation", "oldheater", oldheater,
    "<p>Check this, if you have an early Mitshubishi i-MiEV (2011 and before). Testing which heater intalled in car, before using the car, compare to a batt temp, both temps should be nearly the same.</p>");
  c.fieldset_start("Cell");
  c.input_checkbox("80 cell car","newcell",newcell,"<p>Check this, if you have a Peugeot iOn or Citroen C-Zero with the following VIN: if first two char is VF and the eight char is Y expamle <b>VF</b>31NZK<b>Y</b>Z*******.</p><p><b><font color='red'>You must restart the module if checkbox checked for proper operation!</font></b></p>");
  c.fieldset_end();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * GetDashboardConfig: Mitsubishi i-MiEV Citroen C-Zero, Peugeot iOn specific dashboard setup
 */
void OvmsVehicleMitsubishi::GetDashboardConfig(DashboardConfig& cfg)
{
  OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 135, 5);
  speed_dash.AddBand("green", 0, 65);
  speed_dash.AddBand("yellow", 65, 100);
  speed_dash.AddBand("red", 100, 135);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  if(!trio->cfg_newcell)
  {
    // Voltage:
    voltage_dash.SetMinMax(294, 363);
    voltage_dash.AddBand("red", 294, 317);
    voltage_dash.AddBand("yellow", 317, 341);
    voltage_dash.AddBand("green", 341, 363);
  } else {
    // Voltage:
    voltage_dash.SetMinMax(240, 330);
    voltage_dash.AddBand("red", 240, 280);
    voltage_dash.AddBand("yellow", 280, 310);
    voltage_dash.AddBand("green", 310, 330);
  }

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(10, 100);
  soc_dash.AddBand("red", 10, 15.5);
  soc_dash.AddBand("yellow", 15.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 300);
  eff_dash.AddBand("green", 0, 120);
  eff_dash.AddBand("yellow", 120, 250);
  eff_dash.AddBand("red", 250, 300);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-30, 65);
  power_dash.AddBand("violet", -30, 0);
  power_dash.AddBand("green", 0, 16);
  power_dash.AddBand("yellow", 16, 40);
  power_dash.AddBand("red", 40, 65);

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
  cfg.gaugeset1 = str.str();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
