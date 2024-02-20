/**
 * Project:      Open Vehicle Monitor System
 * Module:       Hyundai Ioniq Webserver
 *
 * (c) 2022 Michael Geddes <frog@bunyip.wheelycreek.net>
 *
 * ----- Kona/Kia Module -----
 * (c) 2019 Geir Øyvind Væidalo <geir@validalo.net>
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

//static const char *TAG = "v-kianiroev";
#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <sstream>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_hyundai_ioniq5.h"
#include "../../vehicle_kiasoulev/src/kia_common.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsHyundaiIoniqEv::WebInit()
{
  XARM("OvmsHyundaiIoniqEv::WebInit");
  // vehicle menu:
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/Auxbattery", "Aux battery monitor", WebAuxBattery, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xkn/battery", "Battery config", WebCfgBattery, PageMenu_Vehicle, PageAuth_Cookie);
  XDISARM;
}


/**
 * WebCfgFeatures: configure general parameters (URL /xkn/config)
 */
void OvmsHyundaiIoniqEv::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  XARM("OvmsHyundaiIoniqEv::WebCfgFeatures");
  std::string error;
#ifdef XIQ_CAN_WRITE
  bool canwrite;
#endif
  bool consoleKilometers;
  bool leftDrive;
  int charge_delay_ccs, charge_delay_type2;

  if (c.method == "POST") {
    // process form submission:
#ifdef XIQ_CAN_WRITE
    canwrite = (c.getvar("canwrite") == "yes");
#endif
    consoleKilometers = (c.getvar("consoleKilometers") == "yes");
    leftDrive = (c.getvar("leftDrive") == "yes");
    charge_delay_ccs = atoi(c.getvar("delayccs").c_str());
    if (charge_delay_ccs < 0 || charge_delay_ccs > 60)
      error = "CSS Delay Out of Range";
    charge_delay_type2 = atoi(c.getvar("delaytype2").c_str());
    if (charge_delay_type2 < 0 || charge_delay_type2 > 60)
    {
      if (error == "")
        error = "Type2 Delay Out of Range";
    }

    if (error == "") {
      // store:
#ifdef XIQ_CAN_WRITE
      MyConfig.SetParamValueBool("xiq", "canwrite", canwrite);
#endif
      MyConfig.SetParamValueBool("xiq", "consoleKilometers", consoleKilometers);
      MyConfig.SetParamValueBool("xiq", "leftDrive", leftDrive);

      if (charge_delay_ccs == 0)
        MyConfig.SetParamValue("xiq", "notify.charge.delay.ccs", "");
      else
        MyConfig.SetParamValueInt("xiq", "notify.charge.delay.ccs", charge_delay_ccs);
      if (charge_delay_type2 == 0)
        MyConfig.SetParamValue("xiq", "notify.charge.delay.type2", "");
      else
        MyConfig.SetParamValueInt("xiq", "notify.charge.delay.type2", charge_delay_type2);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Hyundai Ioniq 5 / Kia EV6 feature configuration saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      XDISARM;
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
#ifdef XIQ_CAN_WRITE
    canwrite = MyConfig.GetParamValueBool("xiq", "canwrite", false);
#endif
    consoleKilometers = MyConfig.GetParamValueBool("xiq", "consoleKilometers", true);
    leftDrive = MyConfig.GetParamValueBool("xiq", "leftDrive", true);

    charge_delay_ccs = MyConfig.GetParamValueInt("xiq", "notify.charge.delay.ccs",0);
    charge_delay_type2 = MyConfig.GetParamValueInt("xiq", "notify.charge.delay.type2",0);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Hyundai Ioniq 5 / Kia EV6 feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("General");
#ifdef XIQ_CAN_WRITE
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, all control functions depend on this.</p>");
#endif
  c.input_checkbox("Console odometer in Kilometers", "consoleKilometers", consoleKilometers,
    "<p>Enable for cars with console odometer in Kilometers, disable for Miles</p>");
  c.input_checkbox("Left hand drive", "leftDrive", leftDrive,
    "<p>Enable for left hand drive cars, disable for right hand drive.</p>");
  c.fieldset_end();

  c.fieldset_start("Charge Notify Delays");
  c.input_slider("CCS Delay", "delayccs", 1, "seconds", charge_delay_ccs>0, charge_delay_ccs?charge_delay_ccs:15,
    15, 1, 60, 1, "<p>Seconds after DC charging starts before notifying user</p>");
  c.input_slider("Type 2 Delay", "delaytype2", 1, "seconds", charge_delay_type2>0, charge_delay_type2?charge_delay_type2:10,
    10, 1, 60, 1, "<p>Seconds after AC charge starts before notifying user</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  XDISARM;
}


/**
 * WebCfgBattery: configure battery parameters (URL /xkn/battery)
 */
void OvmsHyundaiIoniqEv::WebCfgBattery(PageEntry_t &p, PageContext_t &c)
{
  XARM("OvmsHyundaiIoniqEv::WebCfgBattery");
  std::string error;
  //    cap_act_kwh     Battery capacity override in wH
  //  suffsoc           Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange         Sufficient range [km] (Default: 0=disabled)
  //  maxrange          Maximum ideal range at 20 °C [km] (Default: 160)
  std::string cap_act_kwh, maxrange, suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    cap_act_kwh = c.getvar("cap_act_kwh");
    maxrange = c.getvar("maxrange");
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");

    // check:
    if (!cap_act_kwh.empty()) {
      float n = atof(cap_act_kwh.c_str());
      if (n < 1) {
        error += "<li data-input=\"cap_act_kwh\">Battery kWh capacity must be &ge; 1</li>";
      } else if (n > 10000) {
        error += "<li data-input=\"cap_act_kwh\">Battery capacity is in kWh!</li>";
        cap_act_kwh = string_format("%g", n/1000);
      }
    }
    if (!maxrange.empty()) {
      float n = atof(maxrange.c_str());
      if (n < 1) {
        error += "<li data-input=\"maxrange\">Maximum range invalid, must be &ge; 1</li>";
      }
    }
    if (!suffrange.empty()) {
      float n = atof(suffrange.c_str());
      if (n < 0) {
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
      }
    }
    if (!suffsoc.empty()) {
      float n = atof(suffsoc.c_str());
      if (n < 0) {
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
      }
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xiq", "cap_act_kwh", cap_act_kwh);
      MyConfig.SetParamValue("xiq", "maxrange", maxrange);
      MyConfig.SetParamValue("xiq", "suffrange", suffrange);
      MyConfig.SetParamValue("xiq", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Hyundai Ioniq 5 / Kia EV6 battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      XDISARM;
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    cap_act_kwh = MyConfig.GetParamValue("xiq", "cap_act_kwh", "");
    if (!cap_act_kwh.empty()) {
      float n = atof(cap_act_kwh.c_str());
      if (n > 10000)
        cap_act_kwh = string_format("%g", n/1000);
    }

    maxrange = MyConfig.GetParamValue("xiq", "maxrange", STR(CFG_DEFAULT_MAXRANGE));
    suffrange = MyConfig.GetParamValue("xiq", "suffrange", "");
    suffsoc = MyConfig.GetParamValue("xiq", "suffsoc", "");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Hyundai Ioniq 5/Kia EV6 battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery properties");

  c.input("number", "Override Battery capacity", "cap_act_kwh", cap_act_kwh.c_str(), "",
    "<p>This overrides the calculated usable battery capacity of your battery when new.</p>",
    "min=\"1\" step=\"0.1\"", "kWh");

  c.input("number", "Maximum drive range", "maxrange", maxrange.c_str(), "Default: " STR(CFG_DEFAULT_MAXRANGE),
    "<p>The range you normally get at 100% SOC and 20 °C.</p>",
    "min=\"1\" step=\"1\"", "km");

  c.fieldset_end();

  c.fieldset_start("Charge control");

  bool rangeempty = suffrange.empty();
  c.input_slider("Sufficient range", "suffrange", 3, "km",
    !rangeempty, rangeempty ? 0 : atof(suffrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");
  rangeempty = suffsoc.empty();
  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    !rangeempty, rangeempty ? 0 : atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  XDISARM;
}

/**
 * GetDashboardConfig: Hyundai Ioniq specific dashboard setup
 */
void OvmsHyundaiIoniqEv::GetDashboardConfig(DashboardConfig &cfg)
{
  // Speed:
  dash_gauge_t speed_dash(NULL, Kph);
  speed_dash.SetMinMax(0, 170, 5);
  speed_dash.AddBand("green", 0, 70, 5);
  speed_dash.AddBand("yellow", 70, 120, 5);
  speed_dash.AddBand("red", 120, 170, 5);

  // Voltage:
  dash_gauge_t voltage_dash(NULL, Volts);
  voltage_dash.SetMinMax(300, 810);
  voltage_dash.AddBand("red", 350, 450);
  voltage_dash.AddBand("yellow", 450, 500);
  voltage_dash.AddBand("green", 500, 800);

  // SOC:
  dash_gauge_t soc_dash("SOC ", Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green",  25,  100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL, WattHoursPK);
  // Efficency has some inverse relationships .. so choose values that work either way first
  eff_dash.SetMinMax(50, 300);
  // Then force the minimum to zero (whichever way round);
  eff_dash.ZeroMin();
  eff_dash.AddBand("green", 50, 150);
  eff_dash.AddBand("yellow", 150, 250);
  eff_dash.AddBand("red", 250, 300);

  // Power:
  dash_gauge_t power_dash(NULL, kW);
  power_dash.SetMinMax(-30, 30);
  power_dash.AddBand("violet", -10, 0);
  power_dash.AddBand("green", 0, 15);
  power_dash.AddBand("yellow", 15, 25);
  power_dash.AddBand("red", 25, 30);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ", Celcius);
  charget_dash.SetMinMax(10, 80);
  charget_dash.SetTick(20, 5);
  charget_dash.AddBand("normal", 10, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ", Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25, 5);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 50);
  batteryt_dash.AddBand("red", 50, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ", Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20, 5);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ", Celcius);
  motort_dash.SetMinMax(10, 100);
  motort_dash.SetTick(25, 5);
  motort_dash.AddBand("normal", 10, 70);
  motort_dash.AddBand("red", 70, 100);

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
