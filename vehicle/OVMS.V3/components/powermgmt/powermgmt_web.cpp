/**
 * Project:      Open Vehicle Monitor System
 * Module:       Power Management Webserver
 *
 * (c) 2019      Marko Juhanne
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

#include "ovms_log.h"
//static const char *TAG = "powermgmt";
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_webserver.h"
#include "powermgmt.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void powermgmt::WebInit()
  {
  MyWebServer.RegisterPage("/cfg/powermgmt", "Power management", WebCfgPowerManagement, PageMenu_Config, PageAuth_Cookie);
  }

void powermgmt::WebCleanup()
  {
  MyWebServer.DeregisterPage("/cfg/powermgmt");
  }

/**
 * WebCfgPowerManagement: configure general parameters (URL /cfg/powermgmt)
 */
 void powermgmt::WebCfgPowerManagement(PageEntry_t& p, PageContext_t& c)
  {
  auto lock = MyConfig.Lock();
  std::string error;
  bool enabled;
  bool modem_wifi_fallback = false;
  bool powersaving_enabled = false;
  std::string modemoff_delay, modemoff_wifi_delay, wifioff_delay, b12v_shutdown_delay;
  std::string powersaving_days, powersaving_deepsleep_days, powersaving_target, powersaving_interval, powersaving_duration;

  if (c.method == "POST")
    {
    // process form submission:
    enabled = (c.getvar("enabled") == "yes");
#ifdef CONFIG_OVMS_COMP_CELLULAR
    modemoff_delay = c.getvar("modemoff_delay");
#ifdef CONFIG_OVMS_COMP_WIFI
    modemoff_wifi_delay = c.getvar("modemoff_wifi_delay");
    modem_wifi_fallback = (c.getvar("modem_wifi_fallback") == "yes");
#endif
#endif
    wifioff_delay = c.getvar("wifioff_delay");
    b12v_shutdown_delay = c.getvar("12v_shutdown_delay");
    powersaving_enabled = (c.getvar("powersaving_enabled") == "yes");
    powersaving_days = c.getvar("powersaving_days");
    powersaving_deepsleep_days = c.getvar("powersaving_deepsleep_days");
    powersaving_target = c.getvar("powersaving_target");
    powersaving_interval = c.getvar("powersaving_interval");
    powersaving_duration = c.getvar("powersaving_duration");

    // check values
    if (!modemoff_delay.empty())
      {
      if (modemoff_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid Modem off delay!</li>";

      if (modemoff_wifi_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid Modem off (WiFi) delay!</li>";

      if (wifioff_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid WiFi off delay!</li>";

      if (b12v_shutdown_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid shutdown delay!</li>";

      if (powersaving_days.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid powersaving days!</li>";

      if (powersaving_deepsleep_days.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid deep sleep limit days!</li>";

      if (powersaving_interval.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid powersaving interval!</li>";

      if (powersaving_duration.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid powersaving duration!</li>";
      }


    if (error == "")
      {
      // store:
      MyConfig.SetParamValueBool("power", "enabled", enabled);
#ifdef CONFIG_OVMS_COMP_CELLULAR
      MyConfig.SetParamValue("power", "modemoff_delay", modemoff_delay);
#ifdef CONFIG_OVMS_COMP_WIFI
      MyConfig.SetParamValue("power", "modemoff_wifi_delay", modemoff_wifi_delay);
      MyConfig.SetParamValueBool("power", "modem_wifi_fallback", modem_wifi_fallback);
#endif
#endif
      MyConfig.SetParamValue("power", "wifioff_delay", wifioff_delay);
      MyConfig.SetParamValue("power", "12v_shutdown_delay", b12v_shutdown_delay);
      MyConfig.SetParamValueBool("power", "powersaving.enabled", powersaving_enabled);
      MyConfig.SetParamValue("power", "powersaving.days", powersaving_days);
      MyConfig.SetParamValue("power", "powersaving.deepsleep_days", powersaving_deepsleep_days);
      MyConfig.SetParamValue("power", "powersaving.target", powersaving_target);
      MyConfig.SetParamValue("power", "powersaving.interval", powersaving_interval);
      MyConfig.SetParamValue("power", "powersaving.duration", powersaving_duration);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Power management configuration saved.</p>");
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
    enabled = MyConfig.GetParamValueBool("power", "enabled", false);
#ifdef CONFIG_OVMS_COMP_CELLULAR
    modemoff_delay = MyConfig.GetParamValue("power", "modemoff_delay", STR(POWERMGMT_MODEMOFF_DELAY));
#ifdef CONFIG_OVMS_COMP_WIFI
    modemoff_wifi_delay = MyConfig.GetParamValue("power", "modemoff_wifi_delay", STR(POWERMGMT_MODEMOFF_WIFI_DELAY));
    modem_wifi_fallback = MyConfig.GetParamValueBool("power", "modem_wifi_fallback", false);
#endif
#endif
    wifioff_delay = MyConfig.GetParamValue("power", "wifioff_delay", STR(POWERMGMT_WIFIOFF_DELAY));
    b12v_shutdown_delay = MyConfig.GetParamValue("power", "12v_shutdown_delay", STR(POWERMGMT_12V_SHUTDOWN_DELAY));
    powersaving_enabled = MyConfig.GetParamValueBool("power", "powersaving.enabled", false);
    powersaving_days = MyConfig.GetParamValue("power", "powersaving.days", STR(POWERMGMT_POWERSAVING_DAYS));
    powersaving_deepsleep_days = MyConfig.GetParamValue("power", "powersaving.deepsleep_days", STR(POWERMGMT_POWERSAVING_DEEPSLEEP_DAYS));
    powersaving_target = MyConfig.GetParamValue("power", "powersaving.target", STR(POWERMGMT_POWERSAVING_TARGET));
    powersaving_interval = MyConfig.GetParamValue("power", "powersaving.interval", STR(POWERMGMT_POWERSAVING_INTERVAL));
    powersaving_duration = MyConfig.GetParamValue("power", "powersaving.duration", STR(POWERMGMT_POWERSAVING_DURATION));

    c.head(200);
    }

  // generate form
  c.panel_start("primary", "Power management configuration");

  float v12 = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float v12_calc = (v12 > 1.0f) ? v12 : 12.6f;
  bool chg12 = StandardMetrics.ms_v_env_charging12v->AsBool();
  powermgmt_consumption_t cons;
  MyPowerMgmt.GetEstimatedConsumption(cons);
  float est_w = v12_calc * (cons.total_ma / 1000.0f);

  const char* wifi_pm = "n/a";
  const char* wifi_state = "disabled";
  const char* modem_pm = "n/a";
  const char* modem_state = "idle";
  const char* gps_pm = "n/a";

  if (MyPeripherals)
    {
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals->m_esp32wifi)
      {
      wifi_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_esp32wifi->GetPowerMode());
      esp32wifi_mode_t wmode = MyPeripherals->m_esp32wifi->GetMode();
      if (wmode == ESP32WIFI_MODE_AP)
        wifi_state = "AP";
      else if (wmode == ESP32WIFI_MODE_APCLIENT)
        wifi_state = MyNetManager.m_connected_wifi ? "AP+Client (connected)" : "AP+Client (disconnected)";
      else if (wmode == ESP32WIFI_MODE_CLIENT)
        wifi_state = MyNetManager.m_connected_wifi ? "Client (connected)" : "Client (disconnected)";
      else if (wmode == ESP32WIFI_MODE_SCAN)
        wifi_state = "Scan";
      else
        wifi_state = "off";
      }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
    if (MyPeripherals->m_cellular_modem)
      {
      modem_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_cellular_modem->GetPowerMode());
      modem_state = MyNetManager.m_connected_modem ? "PPP connected" : "idle";
      gps_pm = (MyPeripherals->m_cellular_modem->GetPowerMode() == On &&
                MyPeripherals->m_cellular_modem->m_nmea != NULL) ? "on" : "off";
      }
#endif
    }
  
  c.printf(
    "<div class=\"well well-sm\">"
    "<b>Current Power Status:</b><br>"
    "12V Battery: <b>%.2f V</b> (%s)",
    v12, chg12 ? "charging" : "not charging");
  if (StandardMetrics.ms_v_bat_12v_current->IsDefined())
    {
    c.printf(" | Vehicle 12V Current: <b>%.2f A</b>",
      StandardMetrics.ms_v_bat_12v_current->AsFloat());
    }
  c.printf(
    " | Timers: Not charging: <b>%u min</b>, WiFi modem-off: <b>%u min</b>, 12V alert: <b>%u min</b><br>"
    "Flags: modem off: <b>%s</b> | modem off wifi connected: <b>%s</b> | wifi off: <b>%s</b>"
    "<table class=\"table table-condensed\" style=\"margin-top:8px;margin-bottom:6px;\">"
    "<thead><tr><th>Consumer</th><th>State</th><th>Est. Current</th><th>Est. Power</th></tr></thead>"
    "<tbody>"
    "<tr><td>Base System</td><td>on</td><td>~%.0f mA</td><td>~%.2f W</td></tr>",
    MyPowerMgmt.GetNotChargingTimer() / 60,
    MyPowerMgmt.GetModemOffWifiTimer() / 60,
    MyPowerMgmt.Get12VAlertTimer() / 60,
    MyPowerMgmt.IsModemOff() ? "yes" : "no",
    MyPowerMgmt.IsModemOffWifi() ? "yes" : "no",
    MyPowerMgmt.IsWifiOff() ? "yes" : "no",
    cons.base_ma, v12_calc * (cons.base_ma / 1000.0f));

  float ah_day = (cons.total_ma / 1000.0f) * 24.0f;
  float ah_week = ah_day * 7.0f;
  float ah_month = ah_day * 30.0f;

  c.printf(
    "<tr><td>WiFi</td><td>%s (%s)</td><td>~%.0f mA</td><td>~%.2f W</td></tr>"
    "<tr><td>Cellular Modem</td><td>%s (%s)</td><td>~%.0f mA</td><td>~%.2f W</td></tr>"
    "<tr><td>GPS (NMEA)</td><td>%s</td><td>~%.0f mA</td><td>~%.2f W</td></tr>"
    "</tbody>"
    "<tfoot>"
    "<tr><th>Total Estimated OVMS Draw</th><th></th><th>~%.0f mA</th><th>~%.2f W</th></tr>"
    "<tr><th colspan=\"4\">Estimated Consumption: Day: %.2f Ah | Week: %.2f Ah | Month: %.2f Ah</th></tr>"
    "</tfoot>"
    "</table>"
    "<small class=\"text-muted\">⚠ Note: The OVMS consumption values are rough estimates calculated from active peripheral power states "
    "(Base System, WiFi, Modem, GPS). The OVMS v3 hardware has no dedicated current shunt sensor, so actual power draw may vary.</small>"
    "</div>",
    wifi_pm, wifi_state, cons.wifi_ma, v12_calc * (cons.wifi_ma / 1000.0f),
    modem_pm, modem_state, cons.modem_ma, v12_calc * (cons.modem_ma / 1000.0f),
    gps_pm, cons.gps_ma, v12_calc * (cons.gps_ma / 1000.0f),
    cons.total_ma, est_w, ah_day, ah_week, ah_month);

  c.form_start(p.uri);
  c.print("<hr>");
  c.print("<h3>Power saving settings</h3>");

  c.input_checkbox("Enable automatic power management", "enabled", enabled,
    "<p>Most electric cars have a small 12V battery which could get depleted if OVMS is left active for long periods (weeks) without charging. "
    "12V battery usually is charged concurrently with the larger main battery or when the drive train is enabled and the Auxiliary Power Module provides "
    "electricity from the main battery to 12V peripherals. If power saving features are enabled, some modules can be switched off after certain time period of "
    "inactivity (non charging)</p>");
  c.fieldset_end();

#ifdef CONFIG_OVMS_COMP_CELLULAR
  c.input("number", "Delay before modem is turned off", "modemoff_delay", modemoff_delay.c_str(),
    "Default: " STR(POWERMGMT_MODEMOFF_DELAY) " hours",
    "<p>0 = disabled</p>",
    "min=\"0\" step=\"1\"", "hours");
#ifdef CONFIG_OVMS_COMP_WIFI
  c.input("number", "Delay before modem is turned off (when WiFi connected)", "modemoff_wifi_delay", modemoff_wifi_delay.c_str(),
    "Default: " STR(POWERMGMT_MODEMOFF_WIFI_DELAY) " minutes",
    "<p>0 = disabled. Turns off the cellular modem after the specified minutes of active WiFi connection while not charging the 12V battery. "
    "The modem is turned back on when 12V battery charging starts.</p>",
    "min=\"0\" step=\"1\"", "minutes");
  c.input_checkbox("Turn on modem when WiFi disconnects (fallback)", "modem_wifi_fallback", modem_wifi_fallback,
    "<p>If enabled, the cellular modem will be powered back on automatically when the WiFi connection is lost (as long as the general modem off delay has not been exceeded).</p>");
#endif
#endif

  c.input("number", "Delay before WiFi is turned off", "wifioff_delay", wifioff_delay.c_str(),
    "Default: " STR(POWERMGMT_WIFIOFF_DELAY) " hours",
    "<p>0 = disabled</p>",
    "min=\"0\" step=\"1\"", "hours");

  c.input("number", "Delay before OVMS is shut down (after initial 12V battery level alert)", "12v_shutdown_delay", b12v_shutdown_delay.c_str(),
    "Default: " STR(POWERMGMT_12V_SHUTDOWN_DELAY) " minutes",
    "<p>0 = disabled</p>"
    "<p><b class=\"text-danger\">⚠</b> This depends on a proper 12V calibration "
    "(see <a href=\"/cfg/vehicle\" target=\"#main\">vehicle configuration</a>).</p>"
    "<p>If 12V battery is depleted under certain threshold, an alarm is set. OVMS waits this time period during which user can begin charging the batteries. "
    "If this period is exceeded without canceled alarm, OVMS will be shut down (sleep) to prevent further battery depletion.</p>"
    "<p>The module will then check the 12V level once per minute, and automatically reboot when the voltage has ben restored.</p>",
    "min=\"0\" step=\"1\"", "minutes");

  c.print("<hr>");
  c.print("<h3>Periodic Power Saving (Long-term Inactivity)</h3>");
  c.input_checkbox("Enable periodic power saving", "powersaving_enabled", powersaving_enabled,
    "<p>After the specified days of vehicle inactivity (tracked independently of 12V trickle charging), periodically perform the selected action until vehicle is turned on again.</p>");

  c.input("number", "Inactivity threshold (days)", "powersaving_days", powersaving_days.c_str(),
    "Default: " STR(POWERMGMT_POWERSAVING_DAYS) " days", "", "min=\"1\" step=\"1\"", "days");

  c.input_radiobtn_start("Periodic power saving target / action", "powersaving_target");
  c.input_radiobtn_option("powersaving_target", "Cellular Modem", "modem", powersaving_target == "modem");
  c.input_radiobtn_option("powersaving_target", "WiFi", "wifi", powersaving_target == "wifi");
  c.input_radiobtn_option("powersaving_target", "Both Modem & WiFi", "both", powersaving_target == "both" || powersaving_target.empty());
  c.input_radiobtn_option("powersaving_target", "Deep Sleep", "deepsleep", powersaving_target == "deepsleep");
  c.input_radiobtn_end("");

  c.input("number", "Deep sleep max duration (days)", "powersaving_deepsleep_days", powersaving_deepsleep_days.c_str(),
    "Default: " STR(POWERMGMT_POWERSAVING_DEEPSLEEP_DAYS) " days (0 = unlimited)",
    "<p>0 = unlimited. Limits the deep sleep mode to the specified number of days, after which periodic power saving continues with 'Both Modem &amp; WiFi' until the vehicle is turned on.</p>",
    "min=\"0\" step=\"1\"", "days");

  c.input("number", "Duty-cycle interval", "powersaving_interval", powersaving_interval.c_str(),
    "Default: " STR(POWERMGMT_POWERSAVING_INTERVAL) " hours", "", "min=\"1\" step=\"1\"", "hours");

  c.input("number", "Duty-cycle duration (or deep sleep duration)", "powersaving_duration", powersaving_duration.c_str(),
    "Default: " STR(POWERMGMT_POWERSAVING_DURATION) " minutes", "", "min=\"1\" step=\"1\"", "minutes");

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>See <a href=\"/cfg/vehicle\" target=\"#main\">vehicle configuration</a> for automatic shutdown"
    " (deep sleep) when the 12V level gets too low, independent of the 12V alert state.</p>"
    "<p>See <a href=\"/cfg/cellular\" target=\"#main\">cellular configuration</a> (if modem installed)"
    " for automatic GPS pausing when parking.</p>"
    "<p>See <a href=\"/cfg/wifi\" target=\"#main\">Wifi configuration</a>"
    " for automatic access point (AP) mode switch-off when no stations are connected.</p>"
  );
  c.done();
  }

#endif //CONFIG_OVMS_COMP_WEBSERVER
