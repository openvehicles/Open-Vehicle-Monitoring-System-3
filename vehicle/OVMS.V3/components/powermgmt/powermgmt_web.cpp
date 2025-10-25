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
#include "ovms_config.h"
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
  std::string error;
  bool enabled;
  std::string modemoff_delay, wifioff_delay, b12v_shutdown_delay;

  if (c.method == "POST")
    {
    // process form submission:
    enabled = (c.getvar("enabled") == "yes");
#ifdef CONFIG_OVMS_COMP_CELLULAR
    modemoff_delay = c.getvar("modemoff_delay");
#endif
    wifioff_delay = c.getvar("wifioff_delay");
    b12v_shutdown_delay = c.getvar("12v_shutdown_delay");

    // check values
    if (!modemoff_delay.empty())
      {
      if (modemoff_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid Modem off delay!</li>";

      if (wifioff_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid WiFi off delay!</li>";

      if (b12v_shutdown_delay.find_first_not_of("0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Invalid shutdown delay!</li>";
      }


    if (error == "")
      {
      // store:
      MyConfig.SetParamValueBool("power", "enabled", enabled);
#ifdef CONFIG_OVMS_COMP_CELLULAR
      MyConfig.SetParamValue("power", "modemoff_delay", modemoff_delay);
#endif
      MyConfig.SetParamValue("power", "wifioff_delay", wifioff_delay);
      MyConfig.SetParamValue("power", "12v_shutdown_delay", b12v_shutdown_delay);

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
#endif
    wifioff_delay = MyConfig.GetParamValue("power", "wifioff_delay", STR(POWERMGMT_WIFIOFF_DELAY));
    b12v_shutdown_delay = MyConfig.GetParamValue("power", "12v_shutdown_delay", STR(POWERMGMT_12V_SHUTDOWN_DELAY));

    c.head(200);
    }

  // generate form
  c.panel_start("primary", "Power management configuration");
  c.form_start(p.uri);

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
