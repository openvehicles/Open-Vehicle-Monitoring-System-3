/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
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

#include "ovms_webserver.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * HandleCfgAutoInit: configure auto init (URL /cfg/autostart)
 */
void OvmsWebServer::HandleCfgAutoInit(PageEntry_t& p, PageContext_t& c)
{
  std::string error, warn;
  bool init, ext12v, modem, server_v2, server_v3;
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    bool scripting;
  #endif
  bool dbc;
  #ifdef CONFIG_OVMS_COMP_MAX7317
    bool egpio;
    std::string egpio_ports;
  #endif //CONFIG_OVMS_COMP_MAX7317
  std::string vehicle_type, obd2ecu, wifi_mode, wifi_ssid_client, wifi_ssid_ap;

  if (c.method == "POST") {
    // process form submission:
    init = (c.getvar("init") == "yes");
    dbc = (c.getvar("dbc") == "yes");
    ext12v = (c.getvar("ext12v") == "yes");
    #ifdef CONFIG_OVMS_COMP_MAX7317
      egpio = (c.getvar("egpio") == "yes");
      egpio_ports = c.getvar("egpio_ports");
    #endif //CONFIG_OVMS_COMP_MAX7317
    modem = (c.getvar("modem") == "yes");
    server_v2 = (c.getvar("server_v2") == "yes");
    server_v3 = (c.getvar("server_v3") == "yes");
    #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      scripting = (c.getvar("scripting") == "yes");
    #endif
    vehicle_type = c.getvar("vehicle_type");
    obd2ecu = c.getvar("obd2ecu");
    wifi_mode = c.getvar("wifi_mode");
    wifi_ssid_ap = c.getvar("wifi_ssid_ap");
    wifi_ssid_client = c.getvar("wifi_ssid_client");

    // check:
    if (wifi_mode == "ap" || wifi_mode == "apclient") {
      if (wifi_ssid_ap.empty())
        wifi_ssid_ap = "OVMS";
      if (MyConfig.GetParamValue("wifi.ap", wifi_ssid_ap).empty()) {
        if (MyConfig.GetParamValue("password", "module").empty())
          error += "<li data-input=\"wifi_ssid_ap\">Wifi AP mode invalid: no password set for SSID!</li>";
        else
          warn += "<li data-input=\"wifi_ssid_ap\">Wifi AP network has no password → uses module password.</li>";
      }
    }
    if (wifi_mode == "client" || wifi_mode == "apclient") {
      if (wifi_ssid_client.empty()) {
        // check for defined client SSIDs:
        OvmsConfigParam* param = MyConfig.CachedParam("wifi.ssid");
        int cnt = 0;
        for (auto const& kv : param->m_map) {
          if (kv.second != "")
            cnt++;
        }
        if (cnt == 0) {
          error += "<li data-input=\"wifi_ssid_client\">Wifi client scan mode invalid: no SSIDs defined!</li>";
        }
      }
      else if (MyConfig.GetParamValue("wifi.ssid", wifi_ssid_client).empty()) {
        error += "<li data-input=\"wifi_ssid_client\">Wifi client mode invalid: no password set for SSID!</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValueBool("auto", "init", init);
      MyConfig.SetParamValueBool("auto", "dbc", dbc);
      MyConfig.SetParamValueBool("auto", "ext12v", ext12v);
      #ifdef CONFIG_OVMS_COMP_MAX7317
        MyConfig.SetParamValueBool("auto", "egpio", egpio);
        MyConfig.SetParamValue("egpio", "monitor.ports", egpio_ports);
      #endif //CONFIG_OVMS_COMP_MAX7317
      MyConfig.SetParamValueBool("auto", "modem", modem);
      MyConfig.SetParamValueBool("auto", "server.v2", server_v2);
      MyConfig.SetParamValueBool("auto", "server.v3", server_v3);
      #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
        MyConfig.SetParamValueBool("auto", "scripting", scripting);
      #endif
      MyConfig.SetParamValue("auto", "vehicle.type", vehicle_type);
      MyConfig.SetParamValue("auto", "obd2ecu", obd2ecu);
      MyConfig.SetParamValue("auto", "wifi.mode", wifi_mode);
      MyConfig.SetParamValue("auto", "wifi.ssid.ap", wifi_ssid_ap);
      MyConfig.SetParamValue("auto", "wifi.ssid.client", wifi_ssid_client);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Auto start configuration saved.</p>");
      if (warn != "") {
        warn = "<p class=\"lead\">Warning:</p><ul class=\"warnlist\">" + warn + "</ul>";
        c.alert("warning", warn.c_str());
      }
      if (c.getvar("action") == "save-reboot")
        OutputReboot(p, c);
      else
        OutputHome(p, c);
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
    init = MyConfig.GetParamValueBool("auto", "init", true);
    dbc = MyConfig.GetParamValueBool("auto", "dbc", false);
    ext12v = MyConfig.GetParamValueBool("auto", "ext12v", false);
    #ifdef CONFIG_OVMS_COMP_MAX7317
      egpio = MyConfig.GetParamValueBool("auto", "egpio", false);
      egpio_ports = MyConfig.GetParamValue("egpio", "monitor.ports");
    #endif //CONFIG_OVMS_COMP_MAX7317
    modem = MyConfig.GetParamValueBool("auto", "modem", false);
    server_v2 = MyConfig.GetParamValueBool("auto", "server.v2", false);
    server_v3 = MyConfig.GetParamValueBool("auto", "server.v3", false);
    #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      scripting = MyConfig.GetParamValueBool("auto", "scripting", true);
    #endif
    vehicle_type = MyConfig.GetParamValue("auto", "vehicle.type");
    obd2ecu = MyConfig.GetParamValue("auto", "obd2ecu");
    wifi_mode = MyConfig.GetParamValue("auto", "wifi.mode", "ap");
    wifi_ssid_ap = MyConfig.GetParamValue("auto", "wifi.ssid.ap");
    if (wifi_ssid_ap.empty())
      wifi_ssid_ap = "OVMS";
    wifi_ssid_client = MyConfig.GetParamValue("auto", "wifi.ssid.client");

    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Auto start configuration");
  c.form_start(p.uri);

  c.input_checkbox("Enable auto start", "init", init,
    "<p>Note: if a crash occurs within 10 seconds after powering the module, autostart will be temporarily"
    " disabled. You may need to use the USB shell to access the module and fix the config.</p>");

  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    c.input_checkbox("Enable Javascript engine (Duktape)", "scripting", scripting,
      "<p>Enable execution of Javascript on the module (plugins, commands, event handlers).</p>");
  #endif

  c.input_checkbox("Autoload DBC files", "dbc", dbc,
    "<p>Enable to autoload DBC files (for reverse engineering).</p>");

  #ifdef CONFIG_OVMS_COMP_MAX7317
    c.input_checkbox("Start EGPIO monitor", "egpio", egpio,
      "<p>Enable to monitor EGPIO input ports and generate metrics and events from changes.</p>");
    c.input_text("EGPIO ports", "egpio_ports", egpio_ports.c_str(), "Ports to monitor",
      "<p>Enter list of port numbers (0…9) to monitor, separated by spaces.</p>");
  #endif //CONFIG_OVMS_COMP_MAX7317

  c.input_checkbox("Power on external 12V", "ext12v", ext12v,
    "<p>Enable to provide 12V to external devices connected to the module (i.e. ECU displays).</p>");

  c.input_select_start("Wifi mode", "wifi_mode");
  c.input_select_option("Access point", "ap", (wifi_mode == "ap"));
  c.input_select_option("Client mode", "client", (wifi_mode == "client"));
  c.input_select_option("Access point + Client", "apclient", (wifi_mode == "apclient"));
  c.input_select_option("Off", "off", (wifi_mode.empty() || wifi_mode == "off"));
  c.input_select_end();

  c.input_select_start("… access point SSID", "wifi_ssid_ap");
  OvmsConfigParam* param = MyConfig.CachedParam("wifi.ap");
  if (param->m_map.find(wifi_ssid_ap) == param->m_map.end())
    c.input_select_option(wifi_ssid_ap.c_str(), wifi_ssid_ap.c_str(), true);
  for (auto const& kv : param->m_map)
    c.input_select_option(kv.first.c_str(), kv.first.c_str(), (kv.first == wifi_ssid_ap));
  c.input_select_end();

  c.input_select_start("… client mode SSID", "wifi_ssid_client");
  param = MyConfig.CachedParam("wifi.ssid");
  c.input_select_option("Any known SSID (scan mode)", "", wifi_ssid_client.empty());
  for (auto const& kv : param->m_map)
    c.input_select_option(kv.first.c_str(), kv.first.c_str(), (kv.first == wifi_ssid_client));
  c.input_select_end();

  c.input_checkbox("Start modem", "modem", modem);

  c.input_select_start("Vehicle type", "vehicle_type");
  c.input_select_option("&mdash;", "", vehicle_type.empty());
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    c.input_select_option(k->second.name, k->first, (vehicle_type == k->first));
  c.input_select_end();

  c.input_select_start("Start OBD2ECU", "obd2ecu");
  c.input_select_option("&mdash;", "", obd2ecu.empty());
  c.input_select_option("can1", "can1", obd2ecu == "can1");
  c.input_select_option("can2", "can2", obd2ecu == "can2");
  c.input_select_option("can3", "can3", obd2ecu == "can3");
  c.input_select_option("can4", "can4", obd2ecu == "can4");
  c.input_select_end(
    "<p>OBD2ECU translates OVMS to OBD2 metrics, i.e. to drive standard ECU displays</p>");

  c.input_checkbox("Start server V2", "server_v2", server_v2);
  c.input_checkbox("Start server V3", "server_v3", server_v3);

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save\">Save</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save-reboot\">Save &amp; reboot</button> "
      "</div>"
    "</div>");
  c.form_end();
  c.panel_end();
  c.done();
}
