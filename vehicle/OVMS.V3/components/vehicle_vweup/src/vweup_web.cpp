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

#include "vehicle_vweup.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleVWeUP::WebInit()
{
  // vehicle menu:
  MyWebServer.RegisterPage("/vwup/hardware", "Hardware",         WebCfgHardware,                      PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/vwup/features", "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVWeUP::WebDeInit()
{
  MyWebServer.DeregisterPage("/vwup/hardware");
  MyWebServer.DeregisterPage("/vwup/features");
}

/**
 * WebCfgFeatures: configure general parameters (URL /vwup/config)
 */
void OvmsVehicleVWeUP::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  bool canwrite;
  std::string modelyear;

  if (c.method == "POST") {
    // process form submission:
    modelyear = c.getvar("modelyear");
    canwrite  = (c.getvar("canwrite") == "yes");

    // check:
    if (!modelyear.empty()) {
      int n = atoi(modelyear.c_str());
      if (n < 2013)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2013</li>";
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("vwup", "modelyear", modelyear);
      MyConfig.SetParamValueBool("vwup", "canwrite",   canwrite);

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
  else {
    // read configuration:
    modelyear = MyConfig.GetParamValue("vwup", "modelyear", STR(DEFAULT_MODEL_YEAR));
    canwrite  = MyConfig.GetParamValueBool("vwup", "canwrite", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Settings");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR),
    "<p>This sets some parameters that differ for pre 2020 models. I.e. kWh of battery.</p>",
    "min=\"2013\" step=\"1\"");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, climate control depends on this.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

/**
 * WebCfgHardware: setup how connexted to the vehicle (URL /vwup/config)
 */
void OvmsVehicleVWeUP::WebCfgHardware(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string how_connected;


  if (c.method == "POST") {
    // process form submission:
    how_connected = c.getvar("how_connected");

    // check:
    if (how_connected != "0") {
        error += "<li data-input=\"hw_connected\">At the moment only T26A is implemented.</li>";
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("vwup", "how_connected",   how_connected);

      c.head(200);
      c.alert("success", "<p class=\"lead\">VW e-Up hardware configuration saved.</p>");
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
    how_connected = MyConfig.GetParamValue("vwup", "how_connected", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "VW e-Up hardware configuration");
  c.form_start(p.uri);

  c.print("<br>This configuration page is a placeholder without function.<br>At the moment only T26A is implemented.<br><br>");

  c.input_radiobtn_start("Connection type", "how_connected");
  c.input_radiobtn_option("how_connected", "T26A", "0", how_connected == "0");
  c.input_radiobtn_option("how_connected", "T26A + Bluetooth", "1", how_connected == "1");
  c.input_radiobtn_option("how_connected", "OBD", "1", how_connected == "2");
  c.input_radiobtn_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
