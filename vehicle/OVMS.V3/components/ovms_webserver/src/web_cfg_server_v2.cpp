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
  std::string error;
  std::string server, vehicleid, password, port;
  std::string updatetime_connected, updatetime_idle;
  bool tls;
  bool ios_tpms_workaround;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    tls = (c.getvar("tls") == "yes");
    vehicleid = c.getvar("vehicleid");
    password = c.getvar("password");
    port = c.getvar("port");
    updatetime_connected = c.getvar("updatetime_connected");
    updatetime_idle = c.getvar("updatetime_idle");
    ios_tpms_workaround = (c.getvar("ios_tpms_workaround") == "yes");

    // validate:
    if (port != "") {
      if (port.find_first_not_of("0123456789") != std::string::npos
          || atoi(port.c_str()) < 0 || atoi(port.c_str()) > 65535) {
        error += "<li data-input=\"port\">Port must be an integer value in the range 0…65535</li>";
      }
    }
    if (vehicleid.length() == 0)
      error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
    if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
      error += "<li data-input=\"vehicleid\">Vehicle ID may only contain ASCII letters, digits and '-'</li>";
    if (updatetime_connected != "") {
      if (atoi(updatetime_connected.c_str()) < 1) {
        error += "<li data-input=\"updatetime_connected\">Update interval (connected) must be at least 1 second</li>";
      }
    }
    if (updatetime_idle != "") {
      if (atoi(updatetime_idle.c_str()) < 1) {
        error += "<li data-input=\"updatetime_idle\">Update interval (idle) must be at least 1 second</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("server.v2", "server", server);
      MyConfig.SetParamValueBool("server.v2", "tls", tls);
      MyConfig.SetParamValue("server.v2", "port", port);
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      if (password != "")
        MyConfig.SetParamValue("password","server.v2", password);
      MyConfig.SetParamValue("server.v2", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v2", "updatetime.idle", updatetime_idle);
      MyConfig.SetParamValueBool("server.v2", "workaround.ios_tpms_display", ios_tpms_workaround);

      std::string info = "<p class=\"lead\">Server V2 (MP) connection configured.</p>"
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
    // read configuration:
    server = MyConfig.GetParamValue("server.v2", "server");
    tls = MyConfig.GetParamValueBool("server.v2", "tls", false);
    vehicleid = MyConfig.GetParamValue("vehicle", "id");
    password = MyConfig.GetParamValue("password", "server.v2");
    port = MyConfig.GetParamValue("server.v2", "port");
    updatetime_connected = MyConfig.GetParamValue("server.v2", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v2", "updatetime.idle");
    ios_tpms_workaround = MyConfig.GetParamValueBool("server.v2", "workaround.ios_tpms_display", true);  // default enabled, effective only for iOS OVMS app 1.8.6

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V2 (MP) configuration");
  c.form_start(p.uri);

  c.input_text("Server", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V2 servers:</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
    "</ul>");
  c.input_checkbox("Enable TLS", "tls", tls,
    "<p>Note: enable transport layer security (encryption) if your server supports it (all public OVMS servers do).</p>");
  c.input_text("Port", "port", port.c_str(), "optional, default: 6867 (no TLS) / 6870 (TLS)");
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    NULL, "autocomplete=\"section-serverv2 username\"");
  c.input_password("Server password", "password", "", "empty = no change",
    "<p>Note: enter the password for the <strong>vehicle ID</strong>, <em>not</em> your user account password</p>",
    "autocomplete=\"section-serverv2 current-password\"");

  c.fieldset_start("Update intervals");
  c.input_text("…connected", "updatetime_connected", updatetime_connected.c_str(),
    "optional, in seconds, default: 60");
  c.input_text("…idle", "updatetime_idle", updatetime_idle.c_str(),
    "optional, in seconds, default: 600");
  c.fieldset_end();

  c.fieldset_start("Workarounds");
  c.input_checkbox("iOS TPMS display fix", "ios_tpms_workaround", ios_tpms_workaround,
    "<p>Workaround for legacy iOS App, hiding zero/negative temperatures to keep other TPMS data visible.</p>"
    "<p>With the fix enabled, use command <code>tpms</code> to show actual temperatures at/below zero.</p>");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif // CONFIG_OVMS_COMP_SERVER_V2
#endif // CONFIG_OVMS_COMP_SERVER