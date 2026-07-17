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
  auto lock = MyConfig.Lock();
  std::string error, rangeideal, battcapacity, auto_12v_threshold;
  std::string dcdc_soc_limit, dcdc_time_limit, dcdc_after_ignition_time;
  std::string auto_ptc_temp_min, auto_ptc_temp_max;
  bool UseBMScalculation;
  bool vCanEnabled;
  bool auto_12v_recharge_enabled;
  bool coming_home_enabled;
  bool remote_climate_enabled;
  bool alt_unlock_cmd;
  bool auto_ptc_enabled;
  bool pv_charge_notification;

  if (c.method == "POST")
  {
    rangeideal = c.getvar("rangeideal");
    battcapacity = c.getvar("battcapacity");
    auto_12v_threshold = c.getvar("auto_12v_threshold");
    dcdc_soc_limit = c.getvar("dcdc_soc_limit");
    dcdc_time_limit = c.getvar("dcdc_time_limit");
    dcdc_after_ignition_time = c.getvar("dcdc_after_ignition_time");
    auto_ptc_temp_min = c.getvar("auto_ptc_temp_min");
    auto_ptc_temp_max = c.getvar("auto_ptc_temp_max");
    UseBMScalculation = (c.getvar("UseBMScalculation") == "no");
    vCanEnabled = (c.getvar("vCanEnabled") == "yes");
    auto_12v_recharge_enabled = (c.getvar("auto_12v_recharge_enabled") == "yes");
    coming_home_enabled = (c.getvar("coming_home_enabled") == "yes");
    remote_climate_enabled = (c.getvar("remote_climate_enabled") == "yes");
    alt_unlock_cmd = (c.getvar("alt_unlock_cmd") == "yes");
    auto_ptc_enabled = (c.getvar("auto_ptc_enabled") == "yes");
    pv_charge_notification = (c.getvar("pv_charge_notification") == "yes");

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
    if (!dcdc_soc_limit.empty())
    {
      int v = atoi(dcdc_soc_limit.c_str());
      if (v < 5 || v > 100)
        error += "<li data-input=\"dcdc_soc_limit\">DCDC SOC limit must be between 5…100%</li>";
    }
    if (!dcdc_time_limit.empty())
    {
      int v = atoi(dcdc_time_limit.c_str());
      if (v < 0 || v > 999)
        error += "<li data-input=\"dcdc_time_limit\">DCDC time limit must be between 0…999 minutes</li>";
    }
    if (!dcdc_after_ignition_time.empty())
    {
      int v = atoi(dcdc_after_ignition_time.c_str());
      if (v < 0 || v > 60)
        error += "<li data-input=\"dcdc_after_ignition_time\">After-ignition time must be between 0…60 minutes</li>";
    }
    if (!auto_ptc_temp_min.empty())
    {
      float v = atof(auto_ptc_temp_min.c_str());
      if (v < -20.0 || v > 30.0)
        error += "<li data-input=\"auto_ptc_temp_min\">PTC min temperature must be between -20.0…30.0°C</li>";
    }
    if (!auto_ptc_temp_max.empty())
    {
      float v = atof(auto_ptc_temp_max.c_str());
      if (v < -20.0 || v > 30.0)
        error += "<li data-input=\"auto_ptc_temp_max\">PTC max temperature must be between -20.0…30.0°C</li>";
    }
    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xrz2", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xrz2", "battcapacity", battcapacity);
      MyConfig.SetParamValue("xrz2", "auto_12v_threshold", auto_12v_threshold);
      MyConfig.SetParamValue("xrz2", "dcdc_soc_limit", dcdc_soc_limit);
      MyConfig.SetParamValue("xrz2", "dcdc_time_limit", dcdc_time_limit);
      MyConfig.SetParamValue("xrz2", "dcdc_after_ignition_time", dcdc_after_ignition_time);
      MyConfig.SetParamValue("xrz2", "auto_ptc_temp_min", auto_ptc_temp_min);
      MyConfig.SetParamValue("xrz2", "auto_ptc_temp_max", auto_ptc_temp_max);
      MyConfig.SetParamValueBool("xrz2", "UseBMScalculation", UseBMScalculation);
      MyConfig.SetParamValueBool("xrz2", "vCanEnabled", vCanEnabled);
      MyConfig.SetParamValueBool("xrz2", "auto_12v_recharge_enabled", auto_12v_recharge_enabled);
      MyConfig.SetParamValueBool("xrz2", "coming_home_enabled", coming_home_enabled);
      MyConfig.SetParamValueBool("xrz2", "remote_climate_enabled", remote_climate_enabled);
      MyConfig.SetParamValueBool("xrz2", "alt_unlock_cmd", alt_unlock_cmd);
      MyConfig.SetParamValueBool("xrz2", "auto_ptc_enabled", auto_ptc_enabled);
      MyConfig.SetParamValueBool("xrz2", "pv_charge_notification", pv_charge_notification);

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
    dcdc_soc_limit = MyConfig.GetParamValue("xrz2", "dcdc_soc_limit", "80");
    dcdc_time_limit = MyConfig.GetParamValue("xrz2", "dcdc_time_limit", "0");
    dcdc_after_ignition_time = MyConfig.GetParamValue("xrz2", "dcdc_after_ignition_time", "0");
    auto_ptc_temp_min = MyConfig.GetParamValue("xrz2", "auto_ptc_temp_min", "9.5");
    auto_ptc_temp_max = MyConfig.GetParamValue("xrz2", "auto_ptc_temp_max", "20.0");
    UseBMScalculation = MyConfig.GetParamValueBool("xrz2", "UseBMScalculation", false);
    vCanEnabled = MyConfig.GetParamValueBool("xrz2", "vCanEnabled", false);
    auto_12v_recharge_enabled = MyConfig.GetParamValueBool("xrz2", "auto_12v_recharge_enabled", false);
    coming_home_enabled = MyConfig.GetParamValueBool("xrz2", "coming_home_enabled", false);
    remote_climate_enabled = MyConfig.GetParamValueBool("xrz2", "remote_climate_enabled", false);
    alt_unlock_cmd = MyConfig.GetParamValueBool("xrz2", "alt_unlock_cmd", false);
    auto_ptc_enabled = MyConfig.GetParamValueBool("xrz2", "auto_ptc_enabled", false);
    pv_charge_notification = MyConfig.GetParamValueBool("xrz2", "pv_charge_notification", false);
    c.head(200);
  }

  c.panel_start("primary", "Renault Zoe Ph2 Setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery Configuration");
  c.print("<p class='help'>Configure your battery size and range parameters.</p>");

  c.input_radio_start("Battery size", "battcapacity");
  c.input_radio_option("battcapacity", "ZE40 (41kWh)", "41000", battcapacity == "41000");
  c.input_radio_option("battcapacity", "ZE50 (52kWh)", "52000", battcapacity == "52000");
  c.input_radio_end("");

  c.input_slider("Ideal range", "rangeideal", 3, "km", -1, atoi(rangeideal.c_str()), 350, 80, 500, 1,
                 "<p>Expected range when battery is fully charged. Default: 350km</p>");

  c.input_radio_start("Energy calculation method", "UseBMScalculation");
  c.input_radio_option("UseBMScalculation", "OVMS calculation (recommended for BMS firmware ≤530)", "yes", UseBMScalculation == false);
  c.input_radio_option("UseBMScalculation", "BMS calculation (recommended for newer firmware)", "no", UseBMScalculation == true);
  c.input_radio_end("<p>Use OVMS calculation if your BMS firmware is version 530 or lower to avoid energy miscalculation issues.</p>");

  c.fieldset_end();

  c.fieldset_start("V-CAN Connection");
  c.print("<p class='help'><strong>V-CAN connection unlocks advanced features:</strong> Remote climate control, door lock/unlock, "
          "diagnostics, and enhanced metrics. Connect CAN1 to the vehicle's main CAN bus (BCM or gateway tap required).</p>");

  c.input_checkbox("V-CAN connected to CAN1", "vCanEnabled", vCanEnabled,
    "<p><strong>Enable this if you have tapped into the Zoe's V-CAN bus.</strong> OBD-only users should leave this disabled.</p>");

  c.fieldset_end();

  c.fieldset_start("V-CAN Features");
  c.print("<p class='help'>These features require V-CAN connection to be enabled above.</p>");

  c.input_checkbox("Coming home lights", "coming_home_enabled", coming_home_enabled,
    "<p>Automatically turn on headlights for 30 seconds when locking the car (if headlights were on before locking).</p>");

  c.input_checkbox("Climate control via key fob", "remote_climate_enabled", remote_climate_enabled,
    "<p>Repurpose the 'Remote Lighting' button on your key fob to trigger climate control instead of lights.</p>");

  c.input_checkbox("Alternative unlock command", "alt_unlock_cmd", alt_unlock_cmd,
    "<p>Use an alternative unlock command if your Zoe does not respond to the standard unlock command. "
    "Try enabling this if remote unlock via OVMS doesn't work.</p>");

  c.fieldset_end();

  c.fieldset_start("Charging Notifications");
  c.print("<p class='help'>Configure charging notification behavior.</p>");

  c.input_checkbox("PV/Solar charging mode", "pv_charge_notification", pv_charge_notification,
    "<p>Suppress individual charge start/stop notifications when using a PV-controlled wallbox. "
    "Instead, receive a single summary notification when you unplug the cable, showing total kWh charged across all sessions.</p>");

  c.fieldset_end();

  c.fieldset_start("12V Battery Management");
  c.print("<p class='help'>Automatic and manual DC/DC converter control to keep your 12V battery healthy. Requires V-CAN connection.</p>");

  c.input_checkbox("Automatic recharge when locked", "auto_12v_recharge_enabled", auto_12v_recharge_enabled,
    "<p>Automatically start DC/DC converter when 12V battery voltage drops below threshold (while locked and not charging).</p>");

  c.input_slider("Voltage threshold", "auto_12v_threshold", 3, "V", -1, atof(auto_12v_threshold.c_str()), 12.4, 11.0, 14.0, 0.1,
    "<p>Start auto-recharge when voltage drops below this level. Default: 12.4V</p>");

  c.input_slider("After-ignition keep-alive", "dcdc_after_ignition_time", 3, "min", -1, atoi(dcdc_after_ignition_time.c_str()), 0, 0, 60, 1,
    "<p>Keep DC/DC running for this many minutes after turning off ignition. Reduces contactor wear on short stops. "
    "Default: 0 (disabled)</p>");

  c.fieldset_end();

  c.fieldset_start("Manual DC/DC Control Limits");
  c.print("<p class='help'>Safety limits when manually enabling DC/DC with 'xrz2 dcdc enable'. Requires V-CAN connection.</p>");

  c.input_slider("SOC stop limit", "dcdc_soc_limit", 3, "%", -1, atoi(dcdc_soc_limit.c_str()), 80, 5, 100, 1,
    "<p>Stop DC/DC when battery reaches this SOC level. Default: 80%</p>");

  c.input_slider("Time limit", "dcdc_time_limit", 3, "min", -1, atoi(dcdc_time_limit.c_str()), 0, 0, 999, 1,
    "<p>Stop DC/DC after this many minutes. Set to 0 for unlimited (stops at SOC limit only). Default: 0 (unlimited)</p>");

  c.fieldset_end();

  c.fieldset_start("HVAC Auto-Enable PTC");
  c.print("<p class='help'>Automatically enable PTC heaters based on outside temperature. Requires V-CAN connection.</p>");

  c.input_checkbox("Auto-enable PTC heaters", "auto_ptc_enabled", auto_ptc_enabled,
    "<p>Automatically enable PTC heaters when outside temperature is in the configured range (ignition ON, not charging).</p>");

  c.input_slider("Minimum temperature", "auto_ptc_temp_min", 3, "°C", -1, atof(auto_ptc_temp_min.c_str()), 9.5, -20.0, 30.0, 0.5,
    "<p>Enable PTCs when outside temperature is above this value. Default: 9.5°C</p>");

  c.input_slider("Maximum temperature", "auto_ptc_temp_max", 3, "°C", -1, atof(auto_ptc_temp_max.c_str()), 20.0, -20.0, 30.0, 0.5,
    "<p>Enable PTCs when outside temperature is below this value. Default: 20.0°C</p>");

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
