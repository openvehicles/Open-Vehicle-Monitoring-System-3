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

#include "vehicle_renaultzoe_ph2.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

void OvmsVehicleRenaultZoePh2::WebCfgCommon(PageEntry_t &p, PageContext_t &c)
{
  std::string error, rangeideal, battcapacity, auto_12v_threshold;
  bool UseBMScalculation;
  bool vCanEnabled;
  bool auto_12v_recharge_enabled;
  bool coming_home_enabled;
  bool remote_climate_enabled;

  if (c.method == "POST")
  {
    rangeideal = c.getvar("rangeideal");
    battcapacity = c.getvar("battcapacity");
    auto_12v_threshold = c.getvar("auto_12v_threshold");
    UseBMScalculation = (c.getvar("UseBMScalculation") == "no");
    vCanEnabled = (c.getvar("vCanEnabled") == "yes");
    auto_12v_recharge_enabled = (c.getvar("auto_12v_recharge_enabled") == "yes");
    coming_home_enabled = (c.getvar("coming_home_enabled") == "yes");
    remote_climate_enabled = (c.getvar("remote_climate_enabled") == "yes");

    if (!rangeideal.empty())
    {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 500)
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 80…500 km</li>";
    }
    if (!auto_12v_threshold.empty())
    {
      float v = atof(auto_12v_threshold.c_str());
      if (v < 11.0 || v > 14.0)
        error += "<li data-input=\"auto_12v_threshold\">12V threshold must be between 11.0…14.0 V</li>";
    }
    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xrz2", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xrz2", "battcapacity", battcapacity);
      MyConfig.SetParamValue("xrz2", "auto_12v_threshold", auto_12v_threshold);
      MyConfig.SetParamValueBool("xrz2", "UseBMScalculation", UseBMScalculation);
      MyConfig.SetParamValueBool("xrz2", "vCanEnabled", vCanEnabled);
      MyConfig.SetParamValueBool("xrz2", "auto_12v_recharge_enabled", auto_12v_recharge_enabled);
      MyConfig.SetParamValueBool("xrz2", "coming_home_enabled", coming_home_enabled);
      MyConfig.SetParamValueBool("xrz2", "remote_climate_enabled", remote_climate_enabled);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Renault Zoe Ph2 setup saved.</p>");
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
    rangeideal = MyConfig.GetParamValue("xrz2", "rangeideal", "350");
    battcapacity = MyConfig.GetParamValue("xrz2", "battcapacity", "52000");
    auto_12v_threshold = MyConfig.GetParamValue("xrz2", "auto_12v_threshold", "12.4");
    UseBMScalculation = MyConfig.GetParamValueBool("xrz2", "UseBMScalculation", false);
    vCanEnabled = MyConfig.GetParamValueBool("xrz2", "vCanEnabled", false);
    auto_12v_recharge_enabled = MyConfig.GetParamValueBool("xrz2", "auto_12v_recharge_enabled", false);
    coming_home_enabled = MyConfig.GetParamValueBool("xrz2", "coming_home_enabled", false);
    remote_climate_enabled = MyConfig.GetParamValueBool("xrz2", "remote_climate_enabled", false);
    c.head(200);
  }

  c.panel_start("primary", "Renault Zoe Ph2 Setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery size and ideal range");

  c.input_radio_start("Battery size", "battcapacity");
  c.input_radio_option("battcapacity", "ZE40 (41kWh)", "41000", battcapacity == "41000");
  c.input_radio_option("battcapacity", "ZE50 (52kWh)", "52000", battcapacity == "52000");
  c.input_radio_end("");

  c.input_slider("Range Ideal", "rangeideal", 3, "km", -1, atoi(rangeideal.c_str()), 350, 80, 500, 1,
                 "<p>Default 350km. Ideal Range...</p>");
  c.fieldset_end();

  c.fieldset_start("Battery energy calculation");

  c.input_radio_start("Which energy calculation?", "UseBMScalculation");
  c.input_radio_option("UseBMScalculation", "OVMS energy calculation", "yes", UseBMScalculation == false);
  c.input_radio_option("UseBMScalculation", "BMS-based calculation", "no", UseBMScalculation == true);
  c.input_radio_end("");

  c.fieldset_end();

  c.fieldset_start("Additional CAN Bus settings");

  c.input_checkbox("CAN1 interface is connected to Zoes V-CAN?", "vCanEnabled", vCanEnabled,
    "<p>If you connect the CAN1 interface of the OVMS module directly to the V-CAN (BCM or CGW tapping required) you can trigger ECU wake-up, pre-heat function, "
    "locking/unlocking functions and change configuration with DDT commands. Additonal all energy related metrics are used from Zoes ECUs instead of calculation within OVMS. </p>");

  c.input_checkbox("Enable coming home function (V-CAN required)", "coming_home_enabled", coming_home_enabled,
    "<p>When enabled, the headlights will automatically turn on for ~30 seconds after locking the car, if the headlights were on before locking. "
    "This provides lighting when leaving your car at night.</p>");

  c.input_checkbox("Enable remote climate trigger via key fob (V-CAN required)", "remote_climate_enabled", remote_climate_enabled,
    "<p>When enabled, pressing the 'Remote Lighting' button on your key fob will trigger the climate control (pre-heat/pre-cool). "
    "This allows you to start climate control using your key fob instead of the app.</p>");

  c.fieldset_end();

  c.fieldset_start("12V Battery Auto-Recharge (V-CAN required)");

  c.input_checkbox("Enable automatic 12V battery recharge when locked", "auto_12v_recharge_enabled", auto_12v_recharge_enabled,
    "<p>When enabled, the DCDC converter will be activated automatically to recharge the 12V battery when the vehicle is locked and the voltage drops below the threshold. "
    "This feature requires V-CAN connection.</p>");

  c.input_slider("12V voltage threshold", "auto_12v_threshold", 3, "V", -1, atof(auto_12v_threshold.c_str()), 12.4, 11.0, 14.0, 0.1,
    "<p>Default 12.4V. The DCDC converter will be activated when the 12V battery voltage drops below this threshold.</p>");

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
void OvmsVehicleRenaultZoePh2::WebInit()
{
  MyWebServer.RegisterPage("/xrz2/battmon", "BMS View", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz2/settings", "Setup", WebCfgCommon, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz2/preconditioning", "Preconditioning Schedule", OvmsWebServer::HandleCfgPreconditionSchedule, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleRenaultZoePh2::WebDeInit()
{
  MyWebServer.DeregisterPage("/xrz2/battmon");
  MyWebServer.DeregisterPage("/xrz2/settings");
  MyWebServer.DeregisterPage("/xrz2/preconditioning");
}

#endif // CONFIG_OVMS_COMP_WEBSERVER
