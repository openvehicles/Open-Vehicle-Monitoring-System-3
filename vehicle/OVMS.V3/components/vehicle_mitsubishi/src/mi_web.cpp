/**
 * Project:      Open Vehicle Monitor System
 * Module:       Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn Webserver
 *
 * (c) 2018 Tamás Kovács
 * (c) 2018	Nikolay Shishkov <nshishkov@yahoo.com>
 * (c) 2018	Geir Øyvind Væidalo <geir@validalo.net>
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
  std::string error,soh;
  bool oldheater;

  if (c.method == "POST") {
    // process form submission:
    oldheater = (c.getvar("oldheater") == "yes");
    soh = c.getvar("soh");
    // check:
    if (!soh.empty()) {
      int n = atoi(soh.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"soh\">SOH out of range (0…100)</li>";
    }
    // check:
    if (error == "") {
      // store:
      MyConfig.SetParamValueBool("xmi", "oldheater", oldheater);
      MyConfig.SetParamValue("xmi", "soh", soh);

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
  else {
    // read configuration:
    oldheater = MyConfig.GetParamValueBool("xmi", "oldheater", false);
    soh = MyConfig.GetParamValue("xmi","soh","100");
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Trio feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Heater");
  c.input_checkbox("Heater 1. generation", "oldheater", oldheater,
    "<p>Check this, if you have an early Mitshubishi i-MiEV (2011 and before). Testing which heater intalled in car, before using the car, compare to a batt temp, both temps should be nearly the same.</p>");
  c.fieldset_start("SOH");
  c.input_slider("SOH", "soh", 3, NULL,
    -1, atof(soh.c_str()), 100, 0, 100, 1,
    "<p>Default 100, you can set your battery SOH to 'calibrate' ideal range, now not supported automatic SOH detection, you can see soh valule on another app (such as CaniOn (free only Android), or EVBatMon (paid, iOS and Android))</p>");
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
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 135,"
      "plotBands: ["
        "{ from: 0, to: 65, className: 'green-band' },"
        "{ from: 65, to: 100, className: 'yellow-band' },"
        "{ from: 100, to: 135, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 294, max: 362,"
      "plotBands: ["
        "{ from: 194, to: 317, className: 'red-band' },"
        "{ from: 317, to: 341, className: 'yellow-band' },"
        "{ from: 341, to: 362, className: 'green-band' }]"
    "},{"
      // SOC:
      "min: 10, max: 100,"
      "plotBands: ["
        "{ from: 10, to: 15.5, className: 'red-band' },"
        "{ from: 15.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency:
      "min: 0, max: 300,"
      "plotBands: ["
        "{ from: 0, to: 120, className: 'green-band' },"
        "{ from: 120, to: 250, className: 'yellow-band' },"
        "{ from: 250, to: 300, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -30, max: 65,"
      "plotBands: ["
        "{ from: -30, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 16, className: 'green-band' },"
        "{ from: 16, to: 40, className: 'yellow-band' },"
        "{ from: 40, to: 65, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: -10, max: 55, tickInterval: 20,"
      "plotBands: ["
        "{ from: -10, to: 40, className: 'normal-band border' },"
        "{ from: 40, to: 55, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 25,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 40, className: 'normal-band border' },"
        "{ from: 40, to: 65, className: 'red-band border' }]"
    "},{"
      // Inverter temperature:
      "min: -10, max: 55, tickInterval: 20,"
      "plotBands: ["
        "{ from: -10, to: 40, className: 'normal-band border' },"
        "{ from: 40, to: 55, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 20, max: 100, tickInterval: 25,"
      "plotBands: ["
        "{ from: 20, to: 75, className: 'normal-band border' },"
        "{ from: 75, to: 100, className: 'red-band border' }]"
    "}]";
}
