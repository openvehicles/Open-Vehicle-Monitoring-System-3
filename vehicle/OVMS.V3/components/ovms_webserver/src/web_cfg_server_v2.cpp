/*
;    Project:       Open Vehicle Monitor System
;    Date:          December 2025
;;
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
#include "ovms_utils.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V2

#include "ovms_server_v2.h"

/**
 * HandleCfgServerV2: configure server v2 connection (URL /cfg/server/v2)
 */
void OvmsWebServer::HandleCfgServerV2(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  ConfigParamMap map = MyConfig.GetParamMap("server.v2");
  std::string vehicleid = MyConfig.GetParamValue("vehicle", "id");
  std::string password;
  std::string error;

  if (c.method == "POST") {
    // process form submission:
    map["server"] = c.getvar("server");
    map["tls"] = (c.getvar("tls") == "yes") ? "yes" : "no";
    vehicleid = c.getvar("vehicleid");
    password = c.getvar("password");
    map["port"] = c.getvar("port");
    map["updatetime.connected"] = c.getvar("updatetime_connected");
    map["updatetime.idle"] = c.getvar("updatetime_idle");
    map["updatetime.probe"] = c.getvar("updatetime_probe");
    map["timeout.rx"] = c.getvar("timeout_rx");
    map["liveness.enabled"] = (c.getvar("liveness_enabled") == "yes") ? "yes" : "no";
    map["workaround.ios_tpms_display"] = (c.getvar("ios_tpms_workaround") == "yes") ? "yes" : "no";
    map["workaround.legacy_field_enc"] = (c.getvar("legacy_field_enc") == "yes") ? "yes" : "no";

    // validate:
    if (!map["port"].empty()) {
      if (map["port"].find_first_not_of("0123456789") != std::string::npos
          || atoi(map["port"].c_str()) < 0 || atoi(map["port"].c_str()) > 65535) {
        error += "<li data-input=\"port\">Port must be an integer value in the range 0…65535</li>";
      }
    }
    if (vehicleid.length() == 0)
      error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
    if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
      error += "<li data-input=\"vehicleid\">Vehicle ID may only contain ASCII letters, digits and '-'</li>";
    if (!map["updatetime.connected"].empty()) {
      if (atoi(map["updatetime.connected"].c_str()) < 1) {
        error += "<li data-input=\"updatetime_connected\">Update interval (connected) must be at least 1 second</li>";
      }
    }
    if (!map["updatetime.idle"].empty()) {
      if (atoi(map["updatetime.idle"].c_str()) < 1) {
        error += "<li data-input=\"updatetime_idle\">Update interval (idle) must be at least 1 second</li>";
      }
    }
    if (!map["updatetime.probe"].empty()) {
      if (atoi(map["updatetime.probe"].c_str()) < 10) {
        error += "<li data-input=\"updatetime_probe\">Liveness probe interval must be at least 10 seconds</li>";
      }
    }
    if (!map["timeout.rx"].empty()) {
      int rxtimeout = atoi(map["timeout.rx"].c_str());
      if (map["timeout.rx"].find_first_not_of("0123456789") != std::string::npos
          || (rxtimeout != 0 && rxtimeout < 120)) {
        error += "<li data-input=\"timeout_rx\">Network RX timeout must be 0 (disabled) or at least 120 seconds</li>";
      }
    }

    if (error.empty()) {
      // remove empty optional instances before storing:
      if (map["port"].empty())
        map.erase("port");
      if (map["updatetime.connected"].empty())
        map.erase("updatetime.connected");
      if (map["updatetime.idle"].empty())
        map.erase("updatetime.idle");
      if (map["updatetime.probe"].empty())
        map.erase("updatetime.probe");
      if (map["timeout.rx"].empty())
        map.erase("timeout.rx");

      // success:
      MyConfig.SetParamMap("server.v2", map);
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      if (!password.empty())
        MyConfig.SetParamValue("password", "server.v2", password);

      std::string info =
        "<p class=\"lead\">Server V2 (MP) connection configured.</p>"
        "<pre class=\"monitor\" id=\"cfg-server-v2-status\" data-updcmd=\"server v2 status\" data-updcnt=\"1\" data-events=\"server.v2\"></pre>"
        "<ul class=\"list-inline\">"
          "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-cmd=\"server v2 start\">Start</button></li>"
          "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-cmd=\"server v2 stop\">Stop</button></li>"
          "<li><a class=\"btn btn-default btn-sm\" href=\"/cfg/server/v2\" target=\"#main\">Edit</a></li>"
        "</ul>"
        "<script>$(\"title\").data(\"moduleid\", \"" + c.encode_html(vehicleid) + "\");</script>";

      c.head(200);
      c.alert("success", info.c_str());
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
    // fill in defaults:
    if (map["updatetime.probe"].empty())
      map["updatetime.probe"] = "60";
    if (map["timeout.rx"].empty())
      map["timeout.rx"] = "960";
    if (map.find("workaround.ios_tpms_display") == map.end())
      map["workaround.ios_tpms_display"] = "yes";  // default enabled, effective only for iOS OVMS app 1.8.6

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V2 (MP) configuration");
  c.form_start(p.uri);

  c.input_text("Server", "server", map["server"].c_str(), "Enter host name or IP address",
    "<p>Public OVMS V2 servers:</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
    "</ul>");
  c.input_checkbox("Enable TLS", "tls", strtobool(map["tls"]),
    "<p>Note: enable transport layer security (encryption) if your server supports it (all public OVMS servers do).</p>");
  c.input_text("Port", "port", map["port"].c_str(), "optional, default: 6867 (no TLS) / 6870 (TLS)");
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    NULL, "autocomplete=\"section-serverv2 username\"");
  c.input_password("Server password", "password", "", "empty = no change",
    "<p>Note: enter the password for the <strong>vehicle ID</strong>, <em>not</em> your user account password</p>",
    "autocomplete=\"section-serverv2 current-password\"");

  c.fieldset_start("Update intervals");
  c.input_text("…connected", "updatetime_connected", map["updatetime.connected"].c_str(),
    "optional, in seconds, default: 60");
  c.input_text("…idle", "updatetime_idle", map["updatetime.idle"].c_str(),
    "optional, in seconds, default: 600");
  c.fieldset_end();

  c.fieldset_start("Liveness check");
  c.input_checkbox("Enable V2 liveness check", "liveness_enabled", strtobool(map["liveness.enabled"]),
    "<p>Sends an MP ping after inbound traffic has been silent for the probe interval,"
    " then reconnects after another 30 seconds of silence.</p>");
  c.input_text("Probe interval", "updatetime_probe", map["updatetime.probe"].c_str(),
    "default: 60",
    "<p>Time in seconds of inactivity before sending an MP ping to the server.</p>"
    "<p>Note: the server may not respond to pings if it is busy, so the liveness check will only trigger a reconnect after another 30 seconds of silence.</p>");
  c.input_text("Network RX timeout", "timeout_rx", map["timeout.rx"].c_str(),
    "default: 960",
    "<p>Restarts the network connection if no inbound traffic has been received within this timeout.</p>"
    "<p>Liveness check enable/disable has no effect of this setting.</p>"
    "<p>in seconds, 0 = disabled, minimum: 120, default: 960</p>");
  c.fieldset_end();

  c.fieldset_start("Workarounds");
  c.input_checkbox("iOS TPMS display fix", "ios_tpms_workaround", strtobool(map["workaround.ios_tpms_display"]),
    "<p>Workaround for legacy iOS App, hiding zero/negative temperatures to keep other TPMS data visible.</p>"
    "<p>With the fix enabled, use command <code>tpms</code> to show actual temperatures at/below zero.</p>");
  c.input_checkbox("Legacy field encoding", "legacy_field_enc", strtobool(map["workaround.legacy_field_enc"]),
    "<p>Workaround for legacy V2/MP Apps/clients cutting off command responses and text notifications"
    " at the first comma.</p><p>If enabled, all commas in these payloads are changed into semicolons.</p>");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif // CONFIG_OVMS_COMP_SERVER_V2
#endif // CONFIG_OVMS_COMP_SERVER
