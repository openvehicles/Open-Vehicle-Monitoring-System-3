/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd July 2025
;
;    (C) 2025       Carsten Schmiemann
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

#include "vehicle_niu_gtevo.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

void OvmsVehicleNiuGTEVO::WebCfgCommon(PageEntry_t &p, PageContext_t &c)
{
  std::string error, rangeideal, soh_user, cap_user;
  
  if (c.method == "POST")
  {
    rangeideal = c.getvar("rangeideal");
    soh_user = c.getvar("soh_user");

    if (!rangeideal.empty())
    {
      int v = atoi(rangeideal.c_str());
      if (v < 10 || v > 100)
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 10…100 km</li>";
    }

    if (!soh_user.empty())
    {
      int v = atoi(soh_user.c_str());
      if (v < 30 || v > 100)
        error += "<li data-input=\"rangeideal\">User SoH must be of 30…100 %</li>";
    }

    if (!cap_user.empty())
    {
      int v = atoi(cap_user.c_str());
      if (v < 10 || v > 200)
        error += "<li data-input=\"cap_user\">Capacity of a battery must be 10..200 Ah</li>";
    }

    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xnevo", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xnevo", "soh_user", soh_user);
      MyConfig.SetParamValue("xnevo", "cap_user", cap_user);

      c.head(200);
      c.alert("success", "<p class=\"lead\">NIU GT EVO battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else
  {
    // read configuration:
    rangeideal = MyConfig.GetParamValue("xnevo", "rangeideal", "70");
    soh_user = MyConfig.GetParamValue("xnevo", "soh_user", "100");
    cap_user = MyConfig.GetParamValue("xnevo", "cap_user", "26");

    c.head(200);
  }

  c.panel_start("primary", "NIU GT EVO Setup");
  c.form_start(p.uri);

  c.fieldset_start("Ideal range");

  c.input_slider("Range Ideal", "rangeideal", 3, "km", -1, atoi(rangeideal.c_str()), 70, 10, 100, 1,
                 "<p>Set the ideal range you achieve by daily driving your GT EVO.</p>");
  c.fieldset_end();

  c.fieldset_start("User SoH");

  c.input_slider("State of Health", "soh_user", 3, "%", -1, atoi(soh_user.c_str()), 100, 10, 100, 1,
                 "<p>Set the State of Health of your batteries. The BMS is recalculating its SoH only with fully discharging till power off, which is not very usable.</p>");
  c.fieldset_end();

  c.fieldset_start("User battery capacity");

  c.input_slider("Battery capacity", "cap_user", 3, "%", -1, atoi(cap_user.c_str()), 26, 10, 200, 1,
                 "<p>Set the capacity of your battery. It is useful if you build a custom battery pack only. The original ones has 26Ah.</p>");
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
void OvmsVehicleNiuGTEVO::WebInit()
{
  MyWebServer.RegisterPage("/xnevo/battmon", "BMS View", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xnevo/settings", "Setup", WebCfgCommon, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleNiuGTEVO::WebDeInit()
{
  MyWebServer.DeregisterPage("/xnevo/battmon");
  MyWebServer.DeregisterPage("/xnevo/settings");
}

#endif // CONFIG_OVMS_COMP_WEBSERVER
