/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th Sep 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo
;    (C) 2019       Thomas Heuer @Dimitrie78
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

#include "vehicle_renaultzoe.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleRenaultZoe::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xrz/features", "Features",       WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz/battery",  "Battery config", WebCfgBattery,  PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleRenaultZoe::WebDeInit()
{
  MyWebServer.DeregisterPage("/xrz/features");
  MyWebServer.DeregisterPage("/xrz/battery");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xrz/features)
 */
void OvmsVehicleRenaultZoe::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  bool canwrite;

  if (c.method == "POST") {
    // process form submission:
    canwrite  = (c.getvar("canwrite") == "yes");

    if (error == "") {
      // success:
      MyConfig.SetParamValueBool("xrz", "canwrite",   canwrite);

      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      c.head(200);
      c.alert("success", info.c_str());
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
    canwrite  = MyConfig.GetParamValueBool("xrz", "canwrite", false);
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Renault Zoe feature configuration");
  c.form_start(p.uri);
  
  c.input_checkbox("Enable CAN write(Poll)", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  
  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.done();
}

/**
 * WebCfgBattery: configure battery parameters (URL /xrz/battery)
 */
void OvmsVehicleRenaultZoe::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  rangeideal        Ideal Range (Default: 160km)
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  std::string rangeideal, battcapacity, suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    rangeideal   = c.getvar("rangeideal");
    battcapacity = c.getvar("battcapacity");
    suffrange    = c.getvar("suffrange");
    suffsoc      = c.getvar("suffsoc");

    // check:
    if (!rangeideal.empty()) {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 500)
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 80…500 km</li>";
    }
    if (!battcapacity.empty()) {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 500)
        error += "<li data-input=\"battcapacity\">Battery Capacity must be of 10000…50000 Wh</li>";
    }
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
      MyConfig.SetParamValue("xrz", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xrz", "battcapacity", battcapacity);
      MyConfig.SetParamValue("xrz", "suffrange", suffrange);
      MyConfig.SetParamValue("xrz", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Renault Zoe battery setup saved.</p>");
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
    rangeideal = MyConfig.GetParamValue("xrz", "rangeideal", "160");
    battcapacity = MyConfig.GetParamValue("xrz", "battcapacity", "27000");
    suffrange = MyConfig.GetParamValue("xrz", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xrz", "suffsoc", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Renault Zoe Battery Setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");
  
  c.input_slider("Range Ideal", "rangeideal", 3, "km",
    -1, atoi(rangeideal.c_str()), 160, 80, 500, 1,
    "<p>Default 160km. Ideal Range...</p>");
  
  c.input_slider("Battery Capacity", "battcapacity", 5, "Wh",
    -1, atoi(battcapacity.c_str()), 27000, 10000, 50000, 100,
    "<p>Default 27000. Battery Capacity...</p>");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 75, 0, 150, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 80, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER
