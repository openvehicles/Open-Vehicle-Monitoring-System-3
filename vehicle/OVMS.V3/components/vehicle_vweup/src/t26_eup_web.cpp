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

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "t26_eup.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * WebInit: register pages
 */
void OvmsVehicleVWeUpT26::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/xut/features", "Features", WebCfgFeatures, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xut/climate", "Climate control", WebCfgClimate, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVWeUpT26::WebDeInit()
{
  MyWebServer.DeregisterPage("/xut/features");
  MyWebServer.DeregisterPage("/xut/climate");
}

/**
 * WebCfgFeatures: configure general parameters (URL /xut/config)
 */
void OvmsVehicleVWeUpT26::WebCfgFeatures(PageEntry_t &p, PageContext_t &c)
{
  std::string error;
  bool canwrite;
  std::string modelyear;

  if (c.method == "POST")
  {
    // process form submission:
    modelyear = c.getvar("modelyear");
    canwrite = (c.getvar("canwrite") == "yes");

    // check:
    if (!modelyear.empty())
    {
      int n = atoi(modelyear.c_str());
      if (n < 2013)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2013</li>";
    }

    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xut", "modelyear", modelyear);
      MyConfig.SetParamValueBool("xut", "canwrite", canwrite);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up feature configuration saved.</p>");
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
    modelyear = MyConfig.GetParamValue("xut", "modelyear", STR(DEFAULT_MODEL_YEAR));
    canwrite = MyConfig.GetParamValueBool("xut", "canwrite", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Settings");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
          "<p>This sets some parameters that differ for pre 2020 models. I.e. kWh of battery.<br><br>This parameter can also be set in the app under FEATURES 20.</p>",
          "min=\"2013\" step=\"1\"");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
                   "<p>Controls overall CAN write access, climate control depends on this.<br><br>This parameter can also be set in the app under FEATURES 15.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebCfgClimate: setup how connexted to the vehicle (URL /xut/config)
 */
void OvmsVehicleVWeUpT26::WebCfgClimate(PageEntry_t &p, PageContext_t &c)
{
  std::string error;
  std::string cc_temp;

  if (c.method == "POST")
  {
    // process form submission:
    cc_temp = c.getvar("cc_temp");

    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xut", "cc_temp", cc_temp);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up climate control configuration saved.</p>");
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
    cc_temp = MyConfig.GetParamValue("xut", "cc_temp", "21");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up climate control configuration");
  c.form_start(p.uri);

  c.print("<br>This page offers remote climate configuration.<br>The target temperature for the cabin can be set here.<br><br>");

  c.fieldset_start("Climate control");

  c.input_select_start("Cabin target temperature", "cc_temp");
  c.input_select_option("18", "18", cc_temp == "18");
  c.input_select_option("19", "19", cc_temp == "19");
  c.input_select_option("20", "20", cc_temp == "20");
  c.input_select_option("21", "21", cc_temp == "21");
  c.input_select_option("22", "22", cc_temp == "22");
  c.input_select_option("23", "23", cc_temp == "23");
  c.input_select_end();

  c.print("<br><br>This parameter can also be set in the app under FEATURES 21.<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
