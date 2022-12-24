/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2_obd.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

void OvmsVehicleRenaultZoePh2OBD::WebCfgCommon(PageEntry_t& p, PageContext_t& c)
{
  std::string error, rangeideal, battcapacity;
  bool UseBMScalculation;

  if (c.method == "POST") {
    rangeideal            = c.getvar("rangeideal");
    battcapacity          = c.getvar("battcapacity");
    UseBMScalculation     = (c.getvar("UseBMScalculation") == "no");

    if (!rangeideal.empty()) {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 500)
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 80…500 km</li>";
    }
    if (error == "") {
      // store:
      MyConfig.SetParamValue("xrz2o", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xrz2o", "battcapacity", battcapacity);
      MyConfig.SetParamValueBool("xrz2o", "UseBMScalculation", UseBMScalculation);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Renault Zoe Ph2 battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    rangeideal        = MyConfig.GetParamValue("xrz2o", "rangeideal", "350");
    battcapacity      = MyConfig.GetParamValue("xrz2o", "battcapacity", "52000");
    UseBMScalculation = MyConfig.GetParamValueBool("xrz2o", "UseBMScalculation", false);
    c.head(200);
  }

  c.panel_start("primary", "Renault Zoe Ph2 Battery Setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery size and ideal range");

  c.input_radio_start("Battery size", "battcapacity");
  c.input_radio_option("battcapacity", "R240 (22kWh)", "22000", battcapacity == "22000");
  c.input_radio_option("battcapacity", "ZE40 (41kWh)", "41000", battcapacity == "41000");
  c.input_radio_option("battcapacity", "ZE50 (52kWh)", "52000", battcapacity == "52000");
  c.input_radio_end("");

  c.input_slider("Range Ideal", "rangeideal", 3, "km", -1, atoi(rangeideal.c_str()), 350, 80, 500, 1,
    "<p>Default 350km. Ideal Range...</p>");

  c.fieldset_start("Battery energy calculation");

  c.input_radio_start("Which energy calculation?", "UseBMScalculation");
  c.input_radio_option("UseBMScalculation", "OVMS energy calculation", "yes", UseBMScalculation == false);
  c.input_radio_option("UseBMScalculation", "BMS-based calculation", "no", UseBMScalculation == true);
  c.input_radio_end("");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebInit: register pages
 */
void OvmsVehicleRenaultZoePh2OBD::WebInit()
{
  MyWebServer.RegisterPage("/xrz2o/battmon", "BMS View", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz2o/settings", "Setup", WebCfgCommon,  PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleRenaultZoePh2OBD::WebDeInit()
{
  MyWebServer.DeregisterPage("/xrz2o/battmon");
  MyWebServer.DeregisterPage("/xrz2o/settings");
}


#endif //CONFIG_OVMS_COMP_WEBSERVER
