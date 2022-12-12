/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Hyundai Ioniq vFL
;    Date:          2022-12-08
;
;    Changes:
;    1.0  Initial release
;
;    (c) 2022       Michael Balzer <dexter@dexters-web.de>
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#define _GLIBCXX_USE_C99 // to enable std::stoi etc.
#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_hyundai_ioniqvfl.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebCfgFeatures: configure general parameters (URL /xhi/features)
 */
void OvmsVehicleHyundaiVFL::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  ConfigParamMap nmap = MyConfig.GetParamMap("xhi");

  if (c.method == "POST")
  {
    // process form submission:
    nmap["range.ideal"] = c.getvar("range_ideal");
    nmap["range.user"] = c.getvar("range_user");
    nmap["tpms.pressure.warn"] = c.getvar("tpms_pressure_warn");
    nmap["tpms.pressure.alert"] = c.getvar("tpms_pressure_alert");
    nmap["tpms.temp.warn"] = c.getvar("tpms_temp_warn");
    nmap["tpms.temp.alert"] = c.getvar("tpms_temp_alert");

    // store:
    MyConfig.SetParamMap("xhi", nmap);

    c.head(200);
    c.alert("success", "<p class=\"lead\">Hyundai Ioniq vFL Feature Configuration saved.</p>");
    MyWebServer.OutputHome(p, c);
    c.done();
    return;
  }
  else
  {
    c.head(200);
  }

  // generate form:

  c.panel_start("primary receiver", "Hyundai Ioniq vFL Feature Configuration");
  c.form_start(p.uri);

  c.fieldset_start("Range Estimation");
  c.input_slider("Ideal range", "range_ideal", 3, "km", -1,
    nmap["range.ideal"].empty() ? 200 : std::stof(nmap["range.ideal"]), 200, 0, 300, 1,
    "<p>Ideal maximum range of a new battery.</p>");
  c.input_slider("User range", "range_user", 3, "km", -1,
    nmap["range.user"].empty() ? 200 : std::stof(nmap["range.user"]), 200, 0, 300, 1,
    "<p>Typical user maximum range, automatically adapts to the driving style.</p>"
    "<p>Current metric: "
    "<b><span class=\"metric\" data-metric=\"xhi.b.range.user\" data-prec=\"1\">?</span></b>"
    "&nbsp;<span class=\"unit\">km</span></p>");
  c.fieldset_end();

  c.fieldset_start("Tyre Monitoring (TPMS)");
  c.input_slider("Pressure warning", "tpms_pressure_warn", 3, "kPa", -1,
    nmap["tpms.pressure.warn"].empty() ? 230 : std::stof(nmap["tpms.pressure.warn"]), 230, 150, 350, 1);
  c.input_slider("Pressure alert", "tpms_pressure_alert", 3, "kPa", -1,
    nmap["tpms.pressure.alert"].empty() ? 220 : std::stof(nmap["tpms.pressure.alert"]), 220, 150, 350, 1,
    "<p>Warning/alert will be triggered by a tyre pressure at or below these thresholds.</p>");
  c.input_slider("Temperature warning", "tpms_temp_warn", 3, "°C", -1,
    nmap["tpms.temp.warn"].empty() ? 90 : std::stof(nmap["tpms.temp.warn"]), 90, 50, 150, 1);
  c.input_slider("Temperature alert", "tpms_temp_alert", 3, "°C", -1,
    nmap["tpms.temp.alert"].empty() ? 100 : std::stof(nmap["tpms.temp.alert"]), 100, 50, 150, 1,
    "<p>Warning/alert will be triggered by a tyre temperature at or above these thresholds.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();

  c.panel_end();
  c.done();
}
