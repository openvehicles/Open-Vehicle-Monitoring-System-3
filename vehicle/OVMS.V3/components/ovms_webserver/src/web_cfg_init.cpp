/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2018       Michael Balzer
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

#include "ovms_log.h"
static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <dirent.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"
#include "ovms_boot.h"
#include "ovms_housekeeping.h"
#include "ovms_peripherals.h"

#ifdef CONFIG_OVMS_COMP_OTA
#include "ovms_ota.h"
#endif

#if defined(CONFIG_OVMS_COMP_SERVER) && defined(CONFIG_OVMS_COMP_SERVER_V2)
#include "ovms_server_v2.h"
#endif

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleCfgInit: /cfg/init: setup wizard dispatcher
 */
void OvmsWebServer::HandleCfgInit(PageEntry_t& p, PageContext_t& c)
{
  std::string step = MyConfig.GetParamValue("module", "init");
  if (step.empty())
    step = "1";
  ESP_LOGI(TAG, "CfgInit enter %s", step.c_str());

  c.head(200);

  if (c.method == "POST") {
    if (c.getvar("action") == "abort") {
      ESP_LOGI(TAG, "CfgInit abort");
      CfgInitSetStep("done");
      c.alert("info",
        "<p class=\"lead\">Setup wizard aborted.</p>"
        "<p>Use the configuration menu to do a manual setup. Check the user guide for instructions.</p>");
      OutputHome(p, c);
      c.done();
      return;
    }
    else if (c.getvar("action") == "skip") {
      char sc = step[0];
      if (sc == '5')
        step = "done";
      else
        step = sc+1;
      ESP_LOGI(TAG, "CfgInit skip to %s", step.c_str());
      CfgInitSetStep(step);
      c.method = "GET";
    }
  }
  
  // todo… use function pointers, esp. if we need 'back' option
  if (startsWith(step, "1")) {
    if (!(step = MyWebServer.CfgInit1(p, c, step)).empty()) {
      ESP_LOGI(TAG, "CfgInit next: %s", step.c_str());
      c.method = "GET";
    }
  }
  if (startsWith(step, "2")) {
    if (!(step = MyWebServer.CfgInit2(p, c, step)).empty()) {
      ESP_LOGI(TAG, "CfgInit next: %s", step.c_str());
      c.method = "GET";
    }
  }
  if (startsWith(step, "3")) {
    if (!(step = MyWebServer.CfgInit3(p, c, step)).empty()) {
      ESP_LOGI(TAG, "CfgInit next: %s", step.c_str());
      c.method = "GET";
    }
  }
  if (startsWith(step, "4")) {
    if (!(step = MyWebServer.CfgInit4(p, c, step)).empty()) {
      ESP_LOGI(TAG, "CfgInit next: %s", step.c_str());
      c.method = "GET";
    }
  }
  if (startsWith(step, "5")) {
    if (!(step = MyWebServer.CfgInit5(p, c, step)).empty()) {
      ESP_LOGI(TAG, "CfgInit next: %s", step.c_str());
      c.method = "GET";
    }
  }

  if (step == "done") {
    c.alert("success",
      "<p class=\"lead\">Setup wizard completed.</p>"
      "<p>Your OVMS module is now ready to use. Please have a look at the menus for more options.</p>");
    OutputHome(p, c);
  }
  
  c.done();
}


/**
 * CfgInit state process
 */

void OvmsWebServer::CfgInitStartup()
{
  std::string step = MyConfig.GetParamValue("module", "init");
  if (!step.empty() && step != "done") {
    if (MyBoot.GetEarlyCrashCount() > 0) {
      // early crash: restore default access
      ESP_LOGE(TAG, "CfgInit: early crash detected at step %s", step.c_str());
      CfgInitSetStep("restore", 1);
    }
    else {
      ESP_LOGI(TAG, "CfgInit: continue at step %s after reboot", step.c_str());
      m_init_timeout = 5;
    }
  }
}

std::string OvmsWebServer::CfgInitSetStep(std::string step, int timeout /*=0*/)
{
  ESP_LOGI(TAG, "CfgInitSetStep: step %s timeout %d", step.c_str(), timeout);
  MyConfig.SetParamValue("module", "init", step);
  MyWebServer.m_init_timeout = timeout;
  return step;
}

void OvmsWebServer::CfgInitTicker()
{
  if (m_init_timeout == 0)
    return;
  std::string step = MyConfig.GetParamValue("module", "init");
  if (step.empty() || step == "done") {
    m_init_timeout = 0;
    return;
  }
  if (m_init_timeout % 5 == 0)
    ESP_LOGI(TAG, "CfgInitTicker: step %s timeout in %d seconds", step.c_str(), m_init_timeout);
  if (--m_init_timeout != 0)
    return;
  ESP_LOGI(TAG, "CfgInitTicker: step %s timeout", step.c_str());

  if (step == "1.test.start") {
    // reconfigure wifi AP:
    std::string ap_ssid = MyConfig.GetParamValue("vehicle", "id");
    std::string ap_pass = MyConfig.GetParamValue("wifi.ap", ap_ssid);
    if (ap_ssid.empty() || ap_pass.length() < 8) {
      ESP_LOGE(TAG, "CfgInitTicker: step 1: wifi AP config invalid");
      CfgInitSetStep("restore", 1);
    }
    else {
      ESP_LOGI(TAG, "CfgInitTicker: step 1: starting wifi AP '%s'", ap_ssid.c_str());
      MyPeripherals->m_esp32wifi->StartAccessPointMode(ap_ssid, ap_pass);
      CfgInitSetStep("1.test.connect", 300);
    }
  }
  else if (step == "1.test.connect") {
    // timeout waiting for user:
    ESP_LOGE(TAG, "CfgInitTicker: step 1: timeout waiting for user at new AP");
    CfgInitSetStep("restore", 1);
  }
  else if (step == "restore") {
    // revert to factory default AP & password:
    ESP_LOGE(TAG, "CfgInitTicker: restoring default access, AP 'OVMS'/'OVMSinit'");
    MyConfig.DeleteInstance("password", "module");
    MyConfig.DeleteInstance("auto", "wifi.mode");
    MyConfig.DeleteInstance("auto", "wifi.ssid.ap");
    MyConfig.DeleteInstance("auto", "wifi.ssid.client");
    MyConfig.DeleteInstance("wifi.ap", "OVMS");
    MyConfig.DeleteInstance("network", "dns");
    std::string ssid = MyConfig.GetParamValue("vehicle", "id");
    MyConfig.DeleteInstance("wifi.ap", ssid);
    MyPeripherals->m_esp32wifi->StartAccessPointMode("OVMS", "OVMSinit");
    CfgInitSetStep("1.fail");
  }
  
  else if (step == "2.test.start") {
    // reconfigure wifi to APCLIENT:
    std::string ap_ssid = MyConfig.GetParamValue("vehicle", "id");
    std::string ap_pass = MyConfig.GetParamValue("wifi.ap", ap_ssid);
    if (ap_ssid.empty() || ap_pass.length() < 8) {
      ESP_LOGE(TAG, "CfgInitTicker: step 2: wifi AP config invalid, return to step 1");
      CfgInitSetStep("restore", 1);
      return;
    }
    std::string cl_ssid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    std::string cl_pass = MyConfig.GetParamValue("wifi.ssid", cl_ssid);
    if (cl_ssid.empty() || cl_pass.empty()) {
      ESP_LOGE(TAG, "CfgInitTicker: step 2: wifi client config invalid");
      MyConfig.DeleteInstance("auto", "wifi.mode");
      MyConfig.DeleteInstance("auto", "wifi.ssid.client");
      MyConfig.DeleteInstance("network", "dns");
      CfgInitSetStep("2.fail.connect");
    }
    else {
      ESP_LOGI(TAG, "CfgInitTicker: step 2: starting wifi APCLIENT '%s' / '%s'", ap_ssid.c_str(), cl_ssid.c_str());
      MyPeripherals->m_esp32wifi->StartAccessPointClientMode(ap_ssid, ap_pass, cl_ssid, cl_pass);
      CfgInitSetStep("2.test.connect", 20);
    }
  }
  else if (step == "2.test.connect") {
    // test wifi client connection:
    std::string ssid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    if (!MyNetManager.m_connected_wifi || MyPeripherals->m_esp32wifi->GetSSID() != ssid) {
      ESP_LOGI(TAG, "CfgInitTicker: step 2: wifi connection to '%s' failed, reverting to AP mode", ssid.c_str());
      MyConfig.DeleteInstance("auto", "wifi.mode");
      MyConfig.DeleteInstance("auto", "wifi.ssid.client");
      MyConfig.DeleteInstance("network", "dns");
      std::string ap_ssid = MyConfig.GetParamValue("vehicle", "id");
      std::string ap_pass = MyConfig.GetParamValue("wifi.ap", ap_ssid);
      MyPeripherals->m_esp32wifi->StartAccessPointMode(ap_ssid, ap_pass);
      CfgInitSetStep("2.fail.connect");
    }
#ifdef CONFIG_OVMS_COMP_OTA
    else {
      ota_info info;
      MyOTA.GetStatus(info, true);
      if (info.version_server.empty()) {
        ESP_LOGI(TAG, "CfgInitTicker: step 2: server connection failed");
        CfgInitSetStep("2.fail.server");
      }
      else {
        ESP_LOGI(TAG, "CfgInitTicker: step 2: success, proceeding to step 3");
        CfgInitSetStep("3");
      }
    }
#endif
  }

  else if (step == "4.test.start") {
    std::string vehicletype = MyConfig.GetParamValue("auto", "vehicle.type");
    std::string server = MyConfig.GetParamValue("server.v2", "server");
    if (StdMetrics.ms_v_type->AsString() != vehicletype) {
      // stage 1: configure vehicle type:
      ESP_LOGI(TAG, "CfgInitTicker: step 4: setting vehicle type '%s'", vehicletype.c_str());
      MyVehicleFactory.SetVehicle(vehicletype.c_str());
      if (!MyVehicleFactory.ActiveVehicle()) {
        ESP_LOGE(TAG, "CfgInitTicker: step 4: cannot set vehicle type '%s'", vehicletype.c_str());
        MyConfig.DeleteInstance("auto", "vehicle.type");
        CfgInitSetStep("4.fail.vehicle");
        return;
      }
      CfgInitSetStep("4.test.start", 3); // stage 2 after 3 seconds
    }
#if defined(CONFIG_OVMS_COMP_SERVER) && defined(CONFIG_OVMS_COMP_SERVER_V2)
    else {
      // stage 2: start/stop V2 server:
      if (MyOvmsServerV2) {
        ESP_LOGI(TAG, "CfgInitTicker: step 4: stop server v2");
        delete MyOvmsServerV2;
        MyOvmsServerV2 = NULL;
      }
      if (!server.empty()) {
        ESP_LOGI(TAG, "CfgInitTicker: step 4: start server v2 for host '%s'", server.c_str());
        MyOvmsServerV2 = new OvmsServerV2("oscv2");
      }
      CfgInitSetStep("4.test.connect");
    }
#endif
  }

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
  else if (step == "5.test.start") {
    if (!MyPeripherals || !MyPeripherals->m_simcom) {
      ESP_LOGE(TAG, "CfgInitTicker: step 5: modem not available");
      CfgInitSetStep("done");
    }
    if (MyPeripherals->m_simcom->GetPowerMode() != On) {
      ESP_LOGI(TAG, "CfgInitTicker: step 5: modem power on");
      MyPeripherals->m_simcom->SetPowerMode(On);
    }
    else {
      ESP_LOGI(TAG, "CfgInitTicker: step 5: modem enter state NetStart");
      MyPeripherals->m_simcom->SendSetState1(simcom::NetStart);
      
    }
  }
#endif
}


/**
 * CfgInit1: Secure module
 *  - enter AP SSID & password
 *  - set AP network, restart AP
 *  - if user doesn't come back within 5 minutes: revert AP net to OVMS/OVMSinit
 *  - else: configure autostart, module password & vehicle auth
 */
std::string OvmsWebServer::CfgInit1(PageEntry_t& p, PageContext_t& c, std::string step)
{
  std::string error;
  std::string moduleid, newpass1, newpass2;

  if (c.method == "POST") {
    
    // check input:
    moduleid = c.getvar("moduleid");
    newpass1 = c.getvar("newpass1");
    newpass2 = c.getvar("newpass2");

    if (moduleid.empty())
      error += "<li data-input=\"moduleid\">The module ID must not be empty</li>";
    if (moduleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
      error += "<li data-input=\"moduleid\">Module ID may only contain ASCII letters, digits and '-'</li>";
    if (newpass1.length() < 8)
      error += "<li data-input=\"newpass1\">New password must have at least 8 characters</li>";
    if (newpass2 != newpass1)
      error += "<li data-input=\"newpass2\">Passwords do not match</li>";

    if (error == "") {
      // OK, start test:
      ESP_LOGI(TAG, "CfgInit1: starting test: AP '%s'", moduleid.c_str());
      MyConfig.SetParamValue("vehicle", "id", moduleid);
      MyConfig.SetParamValue("password", "module", newpass1);
      MyConfig.SetParamValue("wifi.ap", moduleid, newpass1);
      MyConfig.SetParamValue("auto", "wifi.mode", "ap");
      MyConfig.SetParamValue("auto", "wifi.ssid.ap", moduleid);
      step = CfgInitSetStep("1.test.start", 5);
    }
    else {
      // output error, return to form:
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.alert("danger", error.c_str());
      step = CfgInitSetStep("1");
    }
  }
  else {
    
    moduleid = MyConfig.GetParamValue("vehicle", "id");
    
    if (step == "1.test.connect") {
      if (MyPeripherals->m_esp32wifi->GetAPSSID() == moduleid) {
        // OK, user connected to new AP: proceed to step 2
        ESP_LOGI(TAG, "CfgInit1: AP test succeeded");
        CfgInitSetStep("2");
        return "2";
      }
      step = CfgInitSetStep("1.fail");
    }
  }

  // display:
  
  if (startsWith(step, "1.test")) {
    c.printf(
      "<div class=\"alert alert-info\">"
        "<p class=\"lead\">What happens now?</p>"
        "<p>The OVMS Wifi access point now gets <strong>reconfigured</strong> to the new module ID and password,"
        " so you will now lose connection to the module and need to reconnect using the new ID and password.</p>"
        "<p>Open your Wifi configuration now and connect to the new Wifi network <code>%s</code>,"
        " then reload this page.</p>"
        "<p><a class=\"btn btn-default\" href=\"javascript:location.reload(true)\">Reload page</a></p>"
        "<p>If you can't reconnect, <strong>don't panic</strong>.</p>"
        "<p>The module will revert to the factory default configuration if you don't reconnect within 5 minutes.</p>"
        "<p>Alternatively, you may simply unplug the module and start again.</p>"
      "</div>"
      , moduleid.c_str());
    return "";
  }
  
  if (step == "1.fail") {
    ESP_LOGI(TAG, "CfgInit1: AP test failed");
    if (MyBoot.GetEarlyCrashCount() > 0) {
      c.alert("warning",
        "<p class=\"lead\">Crash reboot detected!</p>"
        "<p><strong>Sorry, this really should not happen.</strong></p>"
        "<p>Please check if the problem persists, if so note your configuration inputs"
        " and ask for support on the OpenVehicles forum.</p>"
        "<p>You can also try a different confiuration, as the problem is most probably"
        " related to your specific setup.</p>");
    }
    else {
      c.alert("warning",
        "<p class=\"lead\">Something went wrong.</p>"
        "<p>You did not connect to the new access point. Please check your ID and password and try again.</p>");
    }
  }

  // Wizard status:
  c.alert("info",
    "<p class=\"lead\">Quick setup wizard</p>"
    "<ol>"
      "<li><strong>Secure module access</strong></li>"
      "<li>Connect module to internet (via Wifi)</li>"
      "<li>Update to latest firmware version</li>"
      "<li>Configure vehicle type and server</li>"
      "<li>Configure modem (if equipped)</li>"
    "</ol>");
  
  // create some random passwords:
  std::ostringstream pwsugg;
  srand48(StdMetrics.ms_m_monotonic->AsInt() * StdMetrics.ms_m_freeram->AsInt());
  pwsugg << "<p>Inspiration:";
  for (int i=0; i<5; i++)
    pwsugg << " <code class=\"autoselect\">" << c.encode_html(pwgen(12)) << "</code>";
  pwsugg << "</p>";
  
  // generate form:
  c.panel_start("primary", "Step 1/5: Secure Module");
  
  c.print(
    "<p>Your module may currently be accessed by anyone in Wifi range using the default"
    " password, so we first should do something about that.</p>"
    "<p>This setup step will change both the Wifi access point and the module admin"
    " password, so the module is secured for both local and remote access.</p>");
  
  c.form_start(p.uri);
  c.input_text("Module ID", "moduleid", moduleid.c_str(), "Use ASCII letters, digits and '-'",
    "<p>Enter a unique personal module/vehicle ID, e.g. your nickname or vehicle license plate number.</p>"
    "<p>If already registered at an OVMS server, use the vehicle ID as registered.</p>"
    "<p>The ID will be used for the Wifi access point, check if it is free to use within your Wifi range.</p>",
    "autocomplete=\"section-module username\"");
  c.input_password("Module password", "newpass1", "",
    "Enter a password, min. 8 characters", pwsugg.str().c_str(), "autocomplete=\"section-module new-password\"");
  c.input_password("Repeat password", "newpass2", "",
    "Repeat password to verify entry", NULL, "autocomplete=\"section-module new-password\"");
  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"proceed\">Proceed</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"skip\">Skip step</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"abort\">Abort setup</button> "
      "</div>"
    "</div>");
  c.form_end();
  
  c.panel_end();
  
  c.alert("info",
    "<p class=\"lead\">What will happen next?</p>"
    "<p>The OVMS Wifi access point will be <strong>reconfigured</strong> to the new module ID and password,"
    " so you will lose connection to the module and need to reconnect using the new ID and password.</p>"
    "<p>If you can't reconnect, <strong>don't panic</strong>.</p>"
    "<p>The module will revert to the old configuration if you don't reconnect within 5 minutes.</p>"
    "<p>Alternatively, you may simply unplug the module and start again.</p>");
  
  return "";
}


/**
 * CfgInit2: Connect to Wifi network
 *  - TODO: show list of SSIDs from scan
 *  - Select/input SSID & password
 *  - Test connection
 */
std::string OvmsWebServer::CfgInit2(PageEntry_t& p, PageContext_t& c, std::string step)
{
  std::string error;
  std::string ssid, pass, dns;

  if (c.method == "POST") {
    
    // check input:
    ssid = c.getvar("ssid");
    pass = c.getvar("pass");
    if (pass.empty())
      pass = MyConfig.GetParamValue("wifi.ssid", ssid);
    dns = c.getvar("dns");

    if (ssid.empty())
      error += "<li data-input=\"ssid\">The Wifi network ID must not be empty</li>";
    if (pass.empty())
      error += "<li data-input=\"pass\">The Wifi network password must not be empty</li>";

    if (error == "") {
      // OK, start test:
      MyConfig.SetParamValue("wifi.ssid", ssid, pass);
      MyConfig.SetParamValue("auto", "wifi.mode", "apclient");
      MyConfig.SetParamValue("auto", "wifi.ssid.client", ssid);
      MyConfig.SetParamValue("network", "dns", dns);
      c.alert("success", "<p class=\"lead\">OK.</p>");
      step = CfgInitSetStep("2.test.start", 5);
    }
    else {
      // output error, return to form:
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.alert("danger", error.c_str());
      step = CfgInitSetStep("2");
    }
  }
  else {
    ssid = MyConfig.GetParamValue("auto", "wifi.ssid.client");
    pass = MyConfig.GetParamValue("wifi.ssid", ssid);
    dns = MyConfig.GetParamValue("network", "dns");
  }

  // display:
  
  if (startsWith(step, "2.test")) {
    c.printf(
      "<div class=\"alert alert-info\">"
        "<p class=\"lead\">What happens now?</p>"
        "<p>The OVMS now tries to connect to the Wifi network <code>%s</code>, if successful"
        " then tries to contact the OpenVehicles server to check the internet access.</p>"
        "<p>Please be patient, this can take up to 2 minutes.</p>"
        "<p><strong>Note:</strong> the OVMS Wifi access point will continue serving network <code>%s</code>, but you"
        " may <strong>lose connection</strong> to the module and <strong>need to reconnect manually</strong>.</p>"
        "<p>Have an eye on your Wifi association to see if/when you need to reconnect manually.</p>"
      "</div>"
      , ssid.c_str()
      , MyConfig.GetParamValue("auto", "wifi.ssid.ap").c_str());
    OutputReconnect(p, c, "Testing internet connection…");
    return "";
  }
  
  if (step == "2.fail.connect") {
    c.printf(
      "<div class=\"alert alert-warning\">"
        "<p class=\"lead\">Wifi connection failed!</p>"
        "<p>The module could not connect to the Wifi network <code>%s</code>.</p>"
        "<p>Please check SSID &amp; password, then try again.</p>"
      "</div>"
      , ssid.c_str());
  }
  else if (step == "2.fail.server") {
    c.printf(
      "<div class=\"alert alert-warning\">"
        "<p class=\"lead\">Server access failed!</p>"
        "<p>The module could not retrieve the latest version info from the OpenVehicles"
        " server using Wifi network <code>%s</code>.</p>"
        "<p>This may be due to a network or a temporary server problem.</p>"
        "<p>Please check the network and try again.</p>"
      "</div>"
      , ssid.c_str());
  }
  
  // Wizard status:
  c.alert("info",
    "<p class=\"lead\">Quick setup wizard</p>"
    "<ol>"
      "<li>Secure module access <span class=\"checkmark\">✔</span></li>"
      "<li><strong>Connect module to internet (via Wifi)</strong></li>"
      "<li>Update to latest firmware version</li>"
      "<li>Configure vehicle type and server</li>"
      "<li>Configure modem (if equipped)</li>"
    "</ol>");
  
  // generate form:
  c.panel_start("primary", "Step 2/5: Connect to Internet");
  
  c.print(
    "<p>Your module now needs an internet connection to download the latest firmware"
    " version from the OpenVehicles server.</p>");
  
  c.form_start(p.uri);

  c.input_text("Wifi network SSID", "ssid", ssid.c_str(), "Enter Wifi SSID",
    "<p>Please enter a Wifi network that provides access to the internet.</p>",
    "autocomplete=\"section-wifi-client username\"");
  c.input_password("Wifi network password", "pass", "",
    (pass.empty()) ? "Enter Wifi password" : "Empty = no change (retry)",
    NULL, "autocomplete=\"section-wifi-client current-password\"");

  c.input_radio_start("DNS server", "dns");
  c.input_radio_option("dns", "Use default DNS of the Wifi network", "" , dns == "");
  c.input_radio_option("dns", "Use Google public DNS (<code>8.8.8.8 8.8.4.4</code>)", "8.8.8.8 8.8.4.4" , dns == "8.8.8.8 8.8.4.4");
  c.input_radio_option("dns", "Use Cloudflare public DNS (<code>1.1.1.1</code>)", "1.1.1.1" , dns == "1.1.1.1");
  c.input_radio_end(
    "<p>If the server connection fails with your default DNS, try one of the other options.</p>");

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"proceed\">Proceed</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"skip\">Skip step</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"abort\">Abort setup</button> "
      "</div>"
    "</div>");
  c.form_end();
  
  c.panel_end();
  
  c.printf(
    "<div class=\"alert alert-info\">"
      "<p class=\"lead\">What will happen next?</p>"
      "<p>The OVMS will <strong>try to connect</strong> to the Wifi network and then try to"
      " contact the OpenVehicles server to check the internet access.</p>"
      "<p>The OVMS access point will continue serving network <code>%s</code>, but you may"
      " <strong>temporarily lose connection</strong> to the network and need to reconnect manually.</p>"
      "<p>If you can't reconnect, <strong>don't panic</strong>.</p>"
      "<p>The module will revert to the old configuration if you don't reconnect within 5 minutes.</p>"
      "<p>Alternatively, you may simply unplug the module and start again.</p>"
    "</div>"
    , MyConfig.GetParamValue("auto", "wifi.ssid.ap").c_str());
  
  return "";
}


/**
 * CfgInit3: Update firmware
 */
std::string OvmsWebServer::CfgInit3(PageEntry_t& p, PageContext_t& c, std::string step)
{
#ifdef CONFIG_OVMS_COMP_OTA
  std::string error;
  std::string action;
  std::string server;
  ota_info info;

  if (c.method == "POST") {
    action = c.getvar("action");
    if (action == "reboot") {
      // reboot, then proceed to step 4:
      CfgInitSetStep("4");
      OutputReboot(p, c);
      return "";
    }
    else if (action == "change") {
      step = CfgInitSetStep("3");
    }
    else {
      // set server, start update:
      server = c.getvar("server");
      if (!server.empty())
        MyConfig.SetParamValue("ota", "server", server);
      step = CfgInitSetStep("3.update");
    }
  }

  if (server.empty())
    server = MyConfig.GetParamValue("ota", "server");
  if (server.empty())
    server = "api.openvehicles.com/firmware/ota";
  
  MyOTA.GetStatus(info, true);

  // check if update has been successful:
  if (step == "3.update") {
    if (!info.version_server.empty() && !info.update_available) {
      ESP_LOGI(TAG, "CfgInit3: firmware up to date, proceeding to step 4");
      c.alert("success", "<p class=\"lead\">Firmware up to date.</p>");
      return CfgInitSetStep("4");
    }
  }

  // output:
  
  if (info.version_server.empty()) {
    c.printf(
      "<div class=\"alert alert-danger\">"
        "<p class=\"lead\">Update server currently unavailable!</p>"
        "<p>Retrieving the firmware version from server <code>%s</code> failed.</p>"
        "<p>Please select another server or try again later.</p>"
      "</div>"
      , server.c_str());
    // back to input:
    step = CfgInitSetStep("3");
  }
  
  if (step == "3.update") {
    // Flash & reboot:
    c.print(
      "<div class=\"modal fade\" id=\"flash-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
        "<div class=\"modal-dialog modal-lg\">"
          "<div class=\"modal-content\">"
            "<div class=\"modal-header\">"
              "<h4 class=\"modal-title\">Flashing…</h4>"
            "</div>"
            "<div class=\"modal-body\">"
              "<div class=\"alert alert-info\">"
                "<p class=\"lead\">What happens now?</p>"
                "<p>The module now downloads and installs the latest firmware version from the update server.</p>"
                "<p>This will normally take about 30-60 seconds, depending on your internet speed.</p>"
                "<p>When finished (\"flash was successful\"), click \"Reboot now\" to restart the module"
                " using the new firmware.</p>"
                "<p>After reboot and relogin, the setup will automatically continue.</p>"
                "<p><strong>Don't panic</strong> if something goes wrong and the module gets stuck in the update.</p>"
                "<p>You can simply unplug and restart the module, it will show the update form again so you can choose to"
                " retry, try another server or skip the update.</p>"
              "</div>"
              "<pre id=\"output\"></pre>"
            "</div>"
            "<div class=\"modal-footer\">"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"reboot\">Reboot now</button>"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"change\">Change server</button>"
            "</div>"
          "</div>"
        "</div>"
      "</div>"
      "<script>"
        "function setloading(sel, on){"
          "$(sel+\" button\").prop(\"disabled\", on);"
          "if (on) $(sel).addClass(\"loading\");"
          "else $(sel).removeClass(\"loading\");"
        "}"
        "after(0.1, function(){"
          "$(\"#output\").text(\"Processing… (do not interrupt, may take some minutes)\\n\");"
          "setloading(\"#flash-dialog\", true);"
          "$(\"#flash-dialog\").modal(\"show\");"
          "loadcmd(\"ota flash http\", \"+#output\").done(function(resp){"
            "setloading(\"#flash-dialog\", false);"
          "});"
        "});"
        "$(\"#flash-dialog button[name=action]\").on(\"click\", function(ev){"
          "$(\"#flash-dialog\").removeClass(\"fade\").modal(\"hide\");"
          "loaduri(\"#main\", \"post\", \"/cfg/init\", { \"action\": $(this).val() });"
        "});"
      "</script>");
    return "";
  }

  // Wizard status:
  c.alert("info",
    "<p class=\"lead\">Quick setup wizard</p>"
    "<ol>"
      "<li>Secure module access <span class=\"checkmark\">✔</span></li>"
      "<li>Connect module to internet (via Wifi) <span class=\"checkmark\">✔</span></li>"
      "<li><strong>Update to latest firmware version</strong></li>"
      "<li>Configure vehicle type and server</li>"
      "<li>Configure modem (if equipped)</li>"
    "</ol>");
  
  // form:
  c.panel_start("primary", "Step 3/5: Update Firmware");
  c.form_start(p.uri);
  c.input_radio_start("Update server", "server");
  c.input_radio_option("server", "Asia-Pacific (openvehicles.com)",
    "api.openvehicles.com/firmware/ota" , server == "api.openvehicles.com/firmware/ota");
  c.input_radio_option("server", "Europe (dexters-web.de)",
    "ovms.dexters-web.de/firmware/ota", server == "ovms.dexters-web.de/firmware/ota");
  c.input_radio_end();
  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"proceed\">Proceed</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"skip\">Skip step</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"abort\">Abort setup</button> "
      "</div>"
    "</div>");
  c.form_end();
  c.panel_end();
  
  c.alert("info",
    "<p class=\"lead\">What will happen next?</p>"
    "<p>The module will check the update server for a new firmware version, and automatically download"
    " and install it if an update is available.</p>"
    "<p><strong>Don't panic</strong> if something goes wrong and the module gets stuck in the update.</p>"
    "<p>You can simply unplug and restart the module, it will show this form again so you can choose to"
    " retry or try another server.</p>");
  
  return "";
#else
    ESP_LOGI(TAG, "CfgInit3: OTA disabled, proceeding to step 4");
    c.alert("danger", "<p class=\"lead\">Firmware cannot be updated, OTA is disabled.</p>");
    return CfgInitSetStep("4");
#endif
}


/**
 * CfgInit4: vehicle & server
 *  - TODO: server account auto create API (should only need an email address)
 */
std::string OvmsWebServer::CfgInit4(PageEntry_t& p, PageContext_t& c, std::string step)
{
  std::string error, info;
  std::string vehicletype, units_distance;
  std::string server, vehicleid, password;

  if (c.method == "POST") {
    if (startsWith(step, "4.test")) {
      // action from test dialog:
      if (c.getvar("action") == "keep") {
        if (!MyConfig.GetParamValue("server.v2", "server").empty())
          MyConfig.SetParamValueBool("auto", "server.v2", true);
        return CfgInitSetStep("5");
      }
      else if (c.getvar("action") == "change") {
        step = CfgInitSetStep("4");
        c.method = "GET";
      }
    }
    else {
      // process form input:
      vehicletype = c.getvar("vehicletype");
      units_distance = c.getvar("units_distance");
      server = c.getvar("server");
      vehicleid = c.getvar("vehicleid");
      password = c.getvar("password");
      if (password.empty())
        password = MyConfig.GetParamValue("password", "module");

      if (vehicleid.length() == 0)
        error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
      if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
        error += "<li data-input=\"vehicleid\">Vehicle ID may only contain ASCII letters, digits and '-'</li>";

      // configure vehicle:
      MyConfig.SetParamValue("vehicle", "units.distance", units_distance);
      MyConfig.SetParamValue("auto", "vehicle.type", vehicletype);

      // configure server:
      MyConfig.SetParamValue("server.v2", "server", server);
      MyConfig.SetParamValueBool("server.v2", "tls", true);
      if (error == "" && !server.empty()) {
        MyConfig.SetParamValue("vehicle", "id", vehicleid);
        MyConfig.SetParamValue("server.v2", "password", password);
      }

      if (error == "") {
        // start test:
        step = CfgInitSetStep("4.test.start", 3);
      }
      else {
        // output error, return to form:
        error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
        c.alert("danger", error.c_str());
        step = CfgInitSetStep("4");
      }
    }
  }
  
  if (c.method == "GET") {
    // read configuration:
    vehicleid = MyConfig.GetParamValue("vehicle", "id");
    vehicletype = MyConfig.GetParamValue("auto", "vehicle.type");
    units_distance = MyConfig.GetParamValue("vehicle", "units.distance", "K");
    server = MyConfig.GetParamValue("server.v2", "server");
    password = MyConfig.GetParamValue("server.v2", "password");
    
    // default data server = ota server:
    if (server.empty()) {
      server = MyConfig.GetParamValue("ota", "server");
      if (startsWith(server, "ovms.dexters-web.de"))
        server = "ovms.dexters-web.de";
      else
        server = "api.openvehicles.com";
    }
  }

  // output:
  
  if (startsWith(step, "4.test")) {
    c.print(
      "<div class=\"modal fade\" id=\"test-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
        "<div class=\"modal-dialog modal-lg\">"
          "<div class=\"modal-content\">"
            "<div class=\"modal-header\">"
              "<h4 class=\"modal-title\">Testing Vehicle &amp; Server</h4>"
            "</div>"
            "<div class=\"modal-body\">"
              "<div class=\"alert alert-info\">"
                "<p class=\"lead\">What happens now?</p>"
                "<p>The module now configures the vehicle type, then tries to connect to the OVMS data server.</p>"
                "<p>This may take a few seconds, the progress is shown below.</p>"
              "</div>"
              "<pre class=\"monitor\" data-updcmd=\"vehicle status\" data-events=\"vehicle.type\" data-updcnt=\"-1\" data-updint=\"5\"></pre>"
              "<pre class=\"monitor\" data-updcmd=\"server v2 status\" data-events=\"server.v2\" data-updcnt=\"-1\" data-updint=\"5\"></pre>"
            "</div>"
            "<div class=\"modal-footer\">"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"keep\">Keep &amp; proceed</button>"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"change\">Change configuration</button>"
            "</div>"
          "</div>"
        "</div>"
      "</div>"
      "<script>"
        "after(0.1, function(){"
          "monitorInit();"
          "$(\"#test-dialog\").modal(\"show\");"
        "});"
        "$(\"#test-dialog button[name=action]\").on(\"click\", function(ev){"
          "$(\"#test-dialog\").removeClass(\"fade\").modal(\"hide\");"
          "loaduri(\"#main\", \"post\", \"/cfg/init\", { \"action\": $(this).val() });"
        "});"
      "</script>");
    return "";
  }
  
  if (step == "4.fail.vehicle") {
    c.printf(
      "<div class=\"alert alert-warning\">"
        "<p class=\"lead\">Vehicle type setup failed!</p>"
        "<p><strong>Sorry, this really should not happen.</strong></p>"
        "<p>Please use a generic vehicle type like the demonstration vehicle or OBDII for the moment"
        " and ask for support on the OpenVehicles forum.</p>"
      "</div>");
  }
  
  // Wizard status:
  c.alert("info",
    "<p class=\"lead\">Quick setup wizard</p>"
    "<ol>"
      "<li>Secure module access <span class=\"checkmark\">✔</span></li>"
      "<li>Connect module to internet (via Wifi) <span class=\"checkmark\">✔</span></li>"
      "<li>Update to latest firmware version <span class=\"checkmark\">✔</span></li>"
      "<li><strong>Configure vehicle type and server</strong></li>"
      "<li>Configure modem (if equipped)</li>"
    "</ol>");
  
  // form:
  c.panel_start("primary", "Step 4/5: Vehicle &amp; Server");
  c.form_start(p.uri);

  c.input_select_start("Vehicle type", "vehicletype");
  c.input_select_option("&mdash;", "", vehicletype.empty());
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    c.input_select_option(k->second.name, k->first, (vehicletype == k->first));
  c.input_select_end();

  c.input_radiobtn_start("Distance units", "units_distance");
  c.input_radiobtn_option("units_distance", "Kilometers", "K", units_distance == "K");
  c.input_radiobtn_option("units_distance", "Miles", "M", units_distance == "M");
  c.input_radiobtn_end();

  c.input_radio_start("OVMS data server", "server");
  c.input_radio_option("server", "No server connection", "" , server == "");
  c.input_radio_option("server",
    "Asia-Pacific (openvehicles.com) <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a>",
    "api.openvehicles.com" , server == "api.openvehicles.com");
  c.input_radio_option("server",
    "Europe (dexters-web.de) <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a>",
    "ovms.dexters-web.de", server == "ovms.dexters-web.de");
  c.input_radio_end();

  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    NULL, "autocomplete=\"section-serverv2 username\"");
  c.input_password("Server password", "password", "", "empty = use module password",
    "<p>Note: enter the password for the <strong>vehicle ID</strong>, <em>not</em> your user account password</p>",
    "autocomplete=\"section-serverv2 current-password\"");

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"proceed\">Proceed</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"skip\">Skip step</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"abort\">Abort setup</button> "
      "</div>"
    "</div>");
  c.form_end();
  c.panel_end();
  
  c.alert("info",
    "<p class=\"lead\">What will happen next?</p>"
    "<p>The module will configure itself for your vehicle type.</p>"
    "<p>If you have selected a data server, it will try to connect to the server using the"
    " login (ID) and password and display the results.</p>"
    "<p>The progress is shown live and can be cancelled any time.</p>");

  return "";
}


/**
 * CfgInit5: modem
 */
std::string OvmsWebServer::CfgInit5(PageEntry_t& p, PageContext_t& c, std::string step)
{
  std::string error, info;
  bool modem = false, gps = false;
  std::string apn, apn_user, apn_pass;

  if (c.method == "POST") {
    if (startsWith(step, "5.test")) {
      // action from test dialog:
      if (c.getvar("action") == "keep") {
        MyConfig.SetParamValueBool("auto", "modem", true);
        return CfgInitSetStep("done");
      }
      else if (c.getvar("action") == "change") {
        step = CfgInitSetStep("5");
        c.method = "GET";
      }
    }
    else {
      // process form input:
      modem = c.getvar("modem") == "yes";
      apn = c.getvar("apn");
      apn_user = c.getvar("apn_user");
      apn_pass = c.getvar("apn_pass");
      gps = c.getvar("gps") == "yes";

      MyConfig.SetParamValue("modem", "apn", apn);
      MyConfig.SetParamValue("modem", "apn.user", apn_user);
      MyConfig.SetParamValue("modem", "apn.password", apn_pass);
      
      MyConfig.SetParamValueBool("modem", "enable.gps", gps);
      MyConfig.SetParamValueBool("modem", "enable.gpstime", gps);
      
      if (modem) {
        // start test:
        ESP_LOGI(TAG, "CfgInit5: starting test");
        step = CfgInitSetStep("5.test.start", 3);
      }
      else {
        ESP_LOGI(TAG, "CfgInit5: modem disabled");
        MyConfig.SetParamValueBool("auto", "modem", false);
        return CfgInitSetStep("done");
      }
    }
  }
  
  if (c.method == "GET") {
    // read configuration:
    modem = MyConfig.GetParamValueBool("auto", "modem", false);
    apn = MyConfig.GetParamValue("modem", "apn");
    apn_user = MyConfig.GetParamValue("modem", "apn.user");
    apn_pass = MyConfig.GetParamValue("modem", "apn.password");
    gps = MyConfig.GetParamValueBool("modem", "enable.gps", false);

    // default: hologram
    if (apn.empty())
      apn = "hologram";
  }

  // output:
  
  if (startsWith(step, "5.test")) {
    c.print(
      "<div class=\"modal fade\" id=\"test-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
        "<div class=\"modal-dialog modal-lg\">"
          "<div class=\"modal-content\">"
            "<div class=\"modal-header\">"
              "<h4 class=\"modal-title\">Testing Modem APN</h4>"
            "</div>"
            "<div class=\"modal-body\">"
              "<div class=\"alert alert-info\">"
                "<p class=\"lead\">What happens now?</p>"
                "<p>The modem now tries to register with the mobile network.</p>"
                "<p>This may take up to a few minutes depending on your mobile provider and the"
                " SIM activation status. The progress is shown below.</p>"
                "<p>The modem is connected when <strong>State: NetMode</strong> and <strong>PPP connected</strong>"
                " have been established.</p>"
              "</div>"
              "<pre class=\"monitor\" data-updcmd=\"simcom status\" data-events=\"system.modem\" data-updcnt=\"-1\" data-updint=\"5\"></pre>"
            "</div>"
            "<div class=\"modal-footer\">"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"keep\">Keep &amp; proceed</button>"
              "<button type=\"button\" class=\"btn btn-default\" name=\"action\" value=\"change\">Change configuration</button>"
            "</div>"
          "</div>"
        "</div>"
      "</div>"
      "<script>"
        "after(0.1, function(){"
          "monitorInit();"
          "$(\"#test-dialog\").modal(\"show\");"
        "});"
        "$(\"#test-dialog button[name=action]\").on(\"click\", function(ev){"
          "$(\"#test-dialog\").removeClass(\"fade\").modal(\"hide\");"
          "loaduri(\"#main\", \"post\", \"/cfg/init\", { \"action\": $(this).val() });"
        "});"
      "</script>");
    return "";
  }
  
  // Wizard status:
  c.alert("info",
    "<p class=\"lead\">Quick setup wizard</p>"
    "<ol>"
      "<li>Secure module access <span class=\"checkmark\">✔</span></li>"
      "<li>Connect module to internet (via Wifi) <span class=\"checkmark\">✔</span></li>"
      "<li>Update to latest firmware version <span class=\"checkmark\">✔</span></li>"
      "<li>Configure vehicle type and server <span class=\"checkmark\">✔</span></li>"
      "<li><strong>Configure modem (if equipped)</strong></li>"
    "</ol>");
  
  // form:
  c.panel_start("primary", "Step 5/5: Modem");
  c.form_start(p.uri);

  std::string iccid = StdMetrics.ms_m_net_mdm_iccid->AsString();
  if (!iccid.empty()) {
    info = "<code class=\"autoselect\">" + iccid + "</code>";
  } else {
    info =
      "<div class=\"receiver\">"
        "<code class=\"autoselect\" data-metric=\"m.net.mdm.iccid\">(power modem on to read)</code>"
        "&nbsp;"
        "<button class=\"btn btn-default\" data-cmd=\"power simcom on\" data-target=\"#pso\">Power modem on</button>"
        "&nbsp;"
        "<samp id=\"pso\" class=\"samp-inline\"></samp>"
      "</div>"
      "<script>"
      "$(\".receiver\").on(\"msg:metrics\", function(e, update){"
        "$(this).find(\"[data-metric]\").each(function(){"
          "if (metrics[$(this).data(\"metric\")] != \"\" && $(this).text() != metrics[$(this).data(\"metric\")])"
            "$(this).text(metrics[$(this).data(\"metric\")]);"
        "});"
      "}).trigger(\"msg:metrics\");"
      "</script>";
  }
  c.input_info("SIM ICCID", info.c_str());

  c.input_checkbox("Enable modem", "modem", modem);
  c.input_text("APN", "apn", apn.c_str(), NULL,
    "<p>For Hologram, use APN <code>hologram</code> with empty username &amp; password</p>");
  c.input_text("APN username", "apn_user", apn_user.c_str());
  c.input_text("APN password", "apn_pass", apn_pass.c_str());

  c.input_checkbox("Enable GPS", "gps", gps,
    "<p>This enables the modem GPS receiver. Use this if your car does not provide GPS.</p>");

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"proceed\">Proceed</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"skip\">Skip step</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"abort\">Abort setup</button> "
      "</div>"
    "</div>");
  c.form_end();
  c.panel_end();
  
  c.alert("info",
    "<p class=\"lead\">What will happen next?</p>"
    "<p>The modem will try to register with the mobile network using the APN data you enter.</p>"
    "<p>This may take up to a few minutes depending on your mobile provider and the SIM activation"
    " status &mdash; if you haven't activated your SIM yet, do so before starting the test.</p>"
    "<p>The progress is shown live and can be cancelled any time.</p>");

  return "";
}
