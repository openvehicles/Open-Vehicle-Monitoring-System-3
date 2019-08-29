/**
 * Project:      Open Vehicle Monitor System
 * Module:       Volt Ampera Webserver
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

// #include "ovms_log.h"
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_voltampera.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleVoltAmpera::WebInit()
  {
  // vehicle menu:
  MyWebServer.RegisterPage("/xva/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);

  // TODO: Battery monitoring
  //MyWebServer.RegisterPage("/xva/battmon", "Battery Monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  }

void OvmsVehicleVoltAmpera::WebCleanup()
  {
  MyWebServer.DeregisterPage("/xva/features");

  // TODO: Battery monitoring
  //MyWebServer.DeregisterPage("/xva/battmon");
  }

/**
 * WebCfgFeatures: configure general parameters (URL /xva/config)
 */
void OvmsVehicleVoltAmpera::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
  {
  std::string error;
  bool preheat_override_BCM;
  bool extended_wakeup;
  std::string preheat_max_time;

  if (c.method == "POST")
    {
    // process form submission:
    preheat_override_BCM = (c.getvar("preheat_override_BCM") == "yes");
    preheat_max_time = c.getvar("preheat_max_time");
    extended_wakeup = (c.getvar("extended_wakeup") == "yes");

    // check values
    if (!preheat_max_time.empty()) 
      {
      int n = atoi(preheat_max_time.c_str());
      if (n < 1 || n > 30)
        error += "<li data-input=\"preheat_max_time\">Preheat maximum time out of range (1…30) mins</li>";
      }

    if (error == "")
      {
      // store:
      MyConfig.SetParamValueBool("xva", "preheat.override_bcm", preheat_override_BCM);
      MyConfig.SetParamValue("xva", "preheat.max_time", preheat_max_time);
      MyConfig.SetParamValueBool("xva", "extended_wakeup", extended_wakeup);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Volt/Ampera feature configuration saved.</p>");
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
    preheat_override_BCM = MyConfig.GetParamValueBool("xva", "preheat.override_bcm", false);
    preheat_max_time = MyConfig.GetParamValue("xva", "preheat.max_time", STR(VA_PREHEAT_MAX_TIME_DEFAULT)); 
    extended_wakeup = MyConfig.GetParamValueBool("xva", "extended_wakeup", false);

    c.head(200);
    }

  // generate form
  c.panel_start("primary", "Volt/Ampera feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("SWCAN");
  c.print("<p>SWCAN module support: ");
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  c.print("enabled");
#else
  c.print("disabled");
#endif
  c.print("</p>");  
  c.input_checkbox("Enable Extended Wake Up sequence", "extended_wakeup", extended_wakeup,
    "<p>Waking up only the Body Control Module and HVAC module should be enough for our purposes. If for some reason the Remote Start / Preheating "
    "or other Onstar functions do not work, please enable the Extended Wakeup feature which wakes up all the known modules and mimics better the wakeup sequence "
    "that the MyVoltStar application uses. However there's a longer lag (few seconds) before actions take place. </p>");
  c.fieldset_end();

  c.fieldset_start("HVAC / Preheating / Remote Start");
  c.input_checkbox("Enable BCM overriding when invoking Remote Start", "preheat_override_BCM", preheat_override_BCM,
    "<p>Normally Remote start is invoked via key fob or by sending CAN messages that mimic those sent by Onstar module. However this does not "
    "seem to work on certain models (2014 Ampera). By enabling this option the OVMS takes control of the preheating/precooling "
    "and overrides the BCM by sending Remote Start CAN messages. Allows us also to set the maximum preheating time to longer than 20 minutes.</p>"
    "<p>Warning! Currently does not enable the 14V Auxiliary Module, so using this option may cause charge depletion of the 12V battery unless "
    "charging cable is connected!</p>"
    );
  c.input("number", "Maximum preheating/precooling time", "preheat_max_time", preheat_max_time.c_str(), "Default: 20 minutes",
    "<p>Maximum preheating time allowed. Note! This only applies when BCM overriding is enabled.</p>",
    "min=\"1\" step=\"1\" max=\"30\"", "minutes");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
  }

#endif //CONFIG_OVMS_COMP_WEBSERVER
