/**
 * Project:      Open Vehicle Monitor System
 * Module:       Nissan Leaf Webserver
 *
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
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
#include "ovms_peripherals.h"

#include "vehicle_nissanleaf.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleNissanLeaf::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xnl/features", "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xnl/battery",  "Battery config",   WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/bms/cellmon",  "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleNissanLeaf::WebDeInit()
{
  MyWebServer.DeregisterPage("/xnl/features");
  MyWebServer.DeregisterPage("/xnl/battery");
  MyWebServer.DeregisterPage("/bms/cellmon");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xnl/config)
 */
void OvmsVehicleNissanLeaf::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  bool canwrite;
  bool socnewcar;
  bool sohnewcar;
  bool ze1;
  std::string modelyear;
  std::string cabintempoffset;
  std::string speeddivisor;
  std::string maxgids;
  std::string newcarah;
  std::string cfg_ev_request_port;

  if (c.method == "POST") {
    // process form submission:
    modelyear           = c.getvar("modelyear");
    cabintempoffset     = c.getvar("cabintempoffset");
    speeddivisor        = c.getvar("speeddivisor");
    cfg_ev_request_port = c.getvar("cfg_ev_request_port");
    maxgids             = c.getvar("maxgids");
    newcarah            = c.getvar("newcarah");
    socnewcar           = (c.getvar("socnewcar") == "yes");
    sohnewcar           = (c.getvar("sohnewcar") == "yes");
    canwrite            = (c.getvar("canwrite") == "yes");
    ze1                 = (c.getvar("ze1") == "yes");

    // check:
    if (!modelyear.empty()) {
      int n = atoi(modelyear.c_str());
      if (n < 2011)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2011</li>";
    }
    
    if (cabintempoffset.empty()) {
      error += "<li data-input=\"cabintempoffset\">Cabin Temperature Offset can not be empty</li>";
    }

    if (speeddivisor.empty()) {
      error += "<li data-input=\"speeddivisor\">Speed Divisor cannot be empty</li>";
    }

    if (cfg_ev_request_port.empty()) {
      error += "<li data-input=\"cfg_ev_request_port\">EV SYSTEM ACTIVATION REQUEST Pin field cannot be empty</li>";
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xnl", "modelyear", modelyear);
      MyConfig.SetParamValue("xnl", "cabintempoffset", cabintempoffset);
      MyConfig.SetParamValue("xnl", "speeddivisor", speeddivisor);
      MyConfig.SetParamValue("xnl", "cfg_ev_request_port", cfg_ev_request_port);
      MyConfig.SetParamValue("xnl", "maxGids",   maxgids);
      MyConfig.SetParamValue("xnl", "newCarAh",  newcarah);
      MyConfig.SetParamValueBool("xnl", "soc.newcar", socnewcar);
      MyConfig.SetParamValueBool("xnl", "soh.newcar", sohnewcar);
      MyConfig.SetParamValueBool("xnl", "canwrite",   canwrite);
      MyConfig.SetParamValueBool("xnl", "ze1", ze1);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf feature configuration saved.</p>");
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
    modelyear           = MyConfig.GetParamValue("xnl", "modelyear", STR(DEFAULT_MODEL_YEAR));
    cabintempoffset     = MyConfig.GetParamValue("xnl", "cabintempoffset", STR(DEFAULT_CABINTEMP_OFFSET));
    speeddivisor        = MyConfig.GetParamValue("xnl", "speeddivisor", STR(DEFAULT_SPEED_DIVISOR));
    cfg_ev_request_port = MyConfig.GetParamValue("xnl", "cfg_ev_request_port", STR(DEFAULT_PIN_EV));
    maxgids             = MyConfig.GetParamValue("xnl", "maxGids", STR(GEN_1_NEW_CAR_GIDS));
    newcarah            = MyConfig.GetParamValue("xnl", "newCarAh", STR(GEN_1_NEW_CAR_AH));
    socnewcar           = MyConfig.GetParamValueBool("xnl", "soc.newcar", false);
    sohnewcar           = MyConfig.GetParamValueBool("xnl", "soh.newcar", false);
    canwrite            = MyConfig.GetParamValueBool("xnl", "canwrite", false);
    ze1                 = MyConfig.GetParamValueBool("xnl", "ze1", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("General");

  c.input_checkbox("ZE1 model", "ze1", ze1, "<p>ZE1 models use a slightly different CAN structure to AZE0 or ZE0 cars</p>");

  c.input_radio_start("SOC Display", "socnewcar");
  c.input_radio_option("socnewcar", "from dashboard display",   "no",  socnewcar == false);
  c.input_radio_option("socnewcar", "relative to fixed value:", "yes", socnewcar == true);
  c.input_radio_end("");
  c.input("number", NULL, "maxgids", maxgids.c_str(), "Default: " STR(GEN_1_NEW_CAR_GIDS),
      "<p>Enter the maximum GIDS value when fully charged. Default values are " STR(GEN_1_NEW_CAR_GIDS) " (24kWh) or " STR(GEN_1_30_NEW_CAR_GIDS) " (30kWh) or " STR(GEN_2_40_NEW_CAR_GIDS) " (40kWh) or " STR(GEN_2_62_NEW_CAR_GIDS) " (62kWh)</p>",
      "min=\"1\" step=\"1\"", "GIDS");

  c.input_radio_start("SOH Display", "sohnewcar");
  c.input_radio_option("sohnewcar", "from dashboard display",   "no",  sohnewcar == false);
  c.input_radio_option("sohnewcar", "relative to fixed value:", "yes", sohnewcar == true);
  c.input_radio_end("");
  c.input("number", NULL, "newcarah", newcarah.c_str(), "Default: " STR(GEN_1_NEW_CAR_AH),
      "<p>This is the usable capacity of your battery when new. Default values are " STR(GEN_1_NEW_CAR_AH) " (24kWh) or " STR(GEN_1_30_NEW_CAR_AH) " (30kWh) or " STR(GEN_2_40_NEW_CAR_AH) " (40kWh) or " STR(GEN_2_62_NEW_CAR_AH) " (62kWh)</p>",
      "min=\"1\" step=\"1\"", "Ah");
  c.input("number", "Cabin Temperature Offset", "cabintempoffset", cabintempoffset.c_str(), "Default: " STR(DEFAULT_CABINTEMP_OFFSET),
      "<p>This allows an offset adjustment to the cabin temperature sensor readings in Celcius.</p>",
      "step=\"0.1\"", "");
  c.input("number", "Speed Divisor", "speeddivisor", speeddivisor.c_str(), "Default: " STR(DEFAULT_SPEED_DIVISOR),
      "<p>This allows the speed readings to be adjusted (which also affects the trip odometer), e.g. if wheel diameter has changed. This can be estimated by comparing trip odometer to GPS track distance.</p>"
      "<p>The resulting speed will be much lower than the speedometer speed, as the speedometer display is deliberately set ~10% higher.</p>",
      "min=\"1\" step=\"1\"", "");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions like remote heating depend on this.</p>");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
    "<p>This determines the format of CAN write messages as it differs slightly between model years.</p>",
    "min=\"2011\" step=\"1\"", "");
  c.input_select_start("Pin for EV SYSTEM ACTIVATION REQUEST", "cfg_ev_request_port");
  c.input_select_option("SW_12V (DA26 pin 18)", "1", cfg_ev_request_port == "1");
  c.input_select_option("EGPIO_2", "3", cfg_ev_request_port == "3");
  c.input_select_option("EGPIO_3", "4", cfg_ev_request_port == "4");
  c.input_select_option("EGPIO_4", "5", cfg_ev_request_port == "5");
  c.input_select_option("EGPIO_5", "6", cfg_ev_request_port == "6");
  c.input_select_option("EGPIO_6", "7", cfg_ev_request_port == "7");
  c.input_select_option("EGPIO_7", "8", cfg_ev_request_port == "8");
  c.input_select_option("EGPIO_8", "9", cfg_ev_request_port == "9");
  c.input_select_end(
    "<p>The 2011-2012 LEAF needs a +12V signal to the TCU harness to use remote commands."
    " Default is SW_12V. See documentation before making changes here.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * WebCfgBattery: configure battery parameters (URL /xnl/battery)
 */
void OvmsVehicleNissanLeaf::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  //  suffrangecalc     Sufficient range calulation method [ideal/est] (Default: ideal
  //  socdrop          	Allowed drop in SOC [%] (Default: 0=none)
  //  rangedrop        	Allowed drop in range [km] (Default: 0=none)
  std::string suffrange, suffrangecalc, suffsoc, rangedrop, socdrop, minrange, minsoc;
  //  chgnoteonly        Whether to control charging or not [0/1] (Default: 0)
  bool chgnoteonly;

  if (c.method == "POST") {
    // process form submission:
    suffrange         = c.getvar("suffrange");
    suffrangecalc     = c.getvar("suffrangecalc");
    suffsoc           = c.getvar("suffsoc");
    rangedrop         = c.getvar("rangedrop");
    socdrop           = c.getvar("socdrop");
    minrange          = c.getvar("minrange");
    minsoc            = c.getvar("minsoc");
    chgnoteonly       = (c.getvar("chgnoteonly") == "yes");

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
    if (!rangedrop.empty()) {
      float n = atof(rangedrop.c_str());
      if (n < 0)
        error += "<li data-input=\"rangedrop\">Allowed range drop invalid, must be &ge; 0</li>";
    }
    if (!socdrop.empty()) {
      float n = atof(socdrop.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"socdrop\">Allowed SOC drop invalid, must be 0…100</li>";
    }
    if (!minrange.empty()) {
      float n = atof(minrange.c_str());
      if (n < 0)
        error += "<li data-input=\"minrange\">Minimum range invalid, must be &ge; 0</li>";
    }
    if (!minsoc.empty()) {
      float n = atof(minsoc.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"minsoc\">Minimum SOC invalid, must be 0…100</li>";
    }
    
    if (error == "") {
      // store:
      MyConfig.SetParamValue("xnl", "suffrange", suffrange);
      MyConfig.SetParamValue("xnl", "suffrangecalc", suffrangecalc);
      MyConfig.SetParamValue("xnl", "suffsoc", suffsoc);
      MyConfig.SetParamValue("xnl", "rangedrop", rangedrop);
      MyConfig.SetParamValue("xnl", "socdrop", socdrop);
      MyConfig.SetParamValue("xnl", "minrange", minrange);
      MyConfig.SetParamValue("xnl", "minsoc", minsoc);
      MyConfig.SetParamValueBool("xnl", "autocharge", !chgnoteonly);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf battery setup saved.</p>");
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
    suffrangecalc = MyConfig.GetParamValue("xnl", "suffrangecalc", "ideal");
    suffrange = MyConfig.GetParamValue("xnl", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xnl", "suffsoc", "0");
    rangedrop = MyConfig.GetParamValue("xnl", "rangedrop", "0");
    socdrop = MyConfig.GetParamValue("xnl", "socdrop", "0");
    minrange = MyConfig.GetParamValue("xnl", "minrange", "0");
    minsoc = MyConfig.GetParamValue("xnl", "minsoc", "0");
    chgnoteonly = !MyConfig.GetParamValueBool("xnl", "autocharge", true);
    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.alert("info",
    "<p>This section allows to configure automatic charge control based on the range and/or "
    "state of charge (SOC).</p><p>The charging will be automatically stopped when sufficient range or SOC is reached."
    " <br>Likewise the charging will be started again if the range or SOC drops more than allowed drop.</p>");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, 500, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");
  
  c.input_slider("Allowed range drop", "rangedrop", 3, "km",
    atof(rangedrop.c_str()) > 0, atof(rangedrop.c_str()), 0, 0, 500, 1,
    "<p>Default 0=none. Notify/start charge when the range drops more than this "
    "below Sufficient range after the charging has finished.</p>");
  
  c.input_radio_start("Sufficient range estimation method", "suffrangecalc");
  c.input_radio_option("suffrangecalc", "Ideal",   "ideal",  suffrangecalc == "ideal");
  c.input_radio_option("suffrangecalc", "Standard", "est", suffrangecalc == "est");
  c.input_radio_end("");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");
  c.input_slider("Allowed SOC drop", "socdrop", 3, "%",
    atof(socdrop.c_str()) > 0, atof(socdrop.c_str()), 0, 0, 100, 1,
    "<p>Default 0=none. Notify/start charge when SOC drops more than this "
    "below Sufficient SOC after the charging has finished.</p>");
  
  c.input_checkbox("Notify only", "chgnoteonly", chgnoteonly,
    "<p>Select this if you only want to receive notification when the range or state of charge"
    " is outside your defined parameters. And don't want the charging to be stopped or started automatically.</p>");

  c.fieldset_end();
  
  c.fieldset_start("V2X control");

  c.input_slider("Minimum range", "minrange", 3, "km",
    atof(minrange.c_str()) > 0, atof(minrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop discharge when reaching this level.</p>");

  c.input_slider("Minimum SOC", "minsoc", 3, "%",
    atof(minsoc.c_str()) > 0, atof(minsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop discharge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * GetDashboardConfig: Nissan Leaf specific dashboard setup
 */
void OvmsVehicleNissanLeaf::GetDashboardConfig(DashboardConfig& cfg)
{
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 135, 5);
  speed_dash.AddBand("green", 0, 70);
  speed_dash.AddBand("yellow", 70, 100);
  speed_dash.AddBand("red", 100, 135);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(260, 400);
  voltage_dash.AddBand("red", 260, 305);
  voltage_dash.AddBand("yellow", 305, 355);
  voltage_dash.AddBand("green", 355, 400);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 300);
  eff_dash.AddBand("green", 0, 120);
  eff_dash.AddBand("yellow", 120, 250);
  eff_dash.AddBand("red", 250, 300);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-20, 50);
  power_dash.AddBand("violet", -20, 0);
  power_dash.AddBand("green", 0, 10);
  power_dash.AddBand("yellow", 10, 25);
  power_dash.AddBand("red", 25, 50);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ",Celcius);
  charget_dash.SetMinMax(20, 80);
  charget_dash.SetTick(20);
  charget_dash.AddBand("normal", 20, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ",Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 40);
  batteryt_dash.AddBand("red", 40, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ",Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ",Celcius);
  motort_dash.SetMinMax(50, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 50, 110);
  motort_dash.AddBand("red", 110, 125);

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
