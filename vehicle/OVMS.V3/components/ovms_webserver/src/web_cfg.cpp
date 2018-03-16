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
// static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"
#include "ovms_housekeeping.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleStatus: show status overview
 */
void OvmsWebServer::HandleStatus(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);

  if ((cmd = c.getvar("cmd")) != "") {
    output = ExecuteCommand(cmd);
    output =
      "<samp>" + c.encode_html(output) + "</samp>" +
      "<p><a class=\"btn btn-default\" target=\"#main\" href=\"/status\">Reload status</a></p>";
    c.alert("info", output.c_str());
  }

  c.print(
    "<div id=\"livestatus\" class=\"receiver\">"
    "<div class=\"row\">"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Live");
  c.print(
    "<div class=\"table-responsive\">"
      "<table class=\"table table-bordered table-condensed\">"
        "<tbody>"
          "<tr>"
            "<th>Module</th>"
            "<td>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"m.freeram\">?</span><span class=\"unit\">bytes free</span></div>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"m.tasks\">?</span><span class=\"unit\">tasks running</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>Network</th>"
            "<td>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"m.net.provider\">?</span><span class=\"unit\" data-metric=\"m.net.type\">?</span></div>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"m.net.sq\">?</span><span class=\"unit\">dBm</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>Main battery</th>"
            "<td>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.soc\">?</span><span class=\"unit\">%</span></div>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.voltage\">?</span><span class=\"unit\">V</span></div>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.current\">?</span><span class=\"unit\">A</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>12V battery</th>"
            "<td>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.12v.voltage\">?</span><span class=\"unit\">V</span></div>"
              "<div class=\"metric\"><span class=\"value\" data-metric=\"v.b.12v.current\">?</span><span class=\"unit\">A</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>Events</th>"
            "<td>"
              "<ul id=\"eventlog\" class=\"list-unstyled\">"
              "</ul>"
            "</td>"
          "</tr>"
        "</tbody>"
      "</table>"
    "</div>"
    );
  c.panel_end();

  c.print(
    "</div>"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Vehicle");
  output = ExecuteCommand("stat");
  c.printf("<samp class=\"monitor\" id=\"vehicle-status\" data-updcmd=\"stat\">%s</samp>", _html(output));
  output = ExecuteCommand("location status");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#vehicle-status\" data-cmd=\"charge start\">Start charge</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#vehicle-status\" data-cmd=\"charge stop\">Stop charge</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "</div>"
    "<div class=\"row\">"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Server");
  output = ExecuteCommand("server v2 status");
  if (!startsWith(output, "Unrecognised"))
    c.printf("<samp class=\"monitor\" id=\"server-v2\" data-updcmd=\"server v2 status\">%s</samp>", _html(output));
  output = ExecuteCommand("server v3 status");
  if (!startsWith(output, "Unrecognised"))
    c.printf("<samp class=\"monitor\" id=\"server-v3\" data-updcmd=\"server v3 status\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-v2\" data-cmd=\"server v2 start\">Start V2</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-v2\" data-cmd=\"server v2 stop\">Stop V2</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-v3\" data-cmd=\"server v3 start\">Start V3</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-v3\" data-cmd=\"server v3 stop\">Stop V3</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Wifi");
  output = ExecuteCommand("wifi status");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end();

  c.print(
    "</div>"
    "</div>"
    "<div class=\"row\">"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Network");
  output = ExecuteCommand("network status");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end();

  c.print(
    "</div>"
    "<div class=\"col-md-6\">");

  c.panel_start("primary", "Module");
  output = ExecuteCommand("boot status");
  c.printf("<samp>%s</samp>", _html(output));
  c.print("<hr>");
  output = ExecuteCommand("ota status");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end();

  c.print(
    "</div>"
    "</div>"
    "</div>"
    "<script>"
    "$(\"#livestatus\").on(\"msg:metrics\", function(e, update){"
      "$(this).find(\"[data-metric]\").each(function(){"
        "$(this).text(metrics[$(this).data(\"metric\")]);"
      "});"
    "}).trigger(\"msg:metrics\");"
    "$(\"#livestatus\").on(\"msg:event\", function(e, event){"
      "if (event.startsWith(\"ticker\"))"
        "return;"
      "var list = $(\"#eventlog\");"
      "if (list.children().size() >= 5)"
        "list.children().get(0).remove();"
      "var now = (new Date().toLocaleTimeString());"
      "list.append($('<li class=\"event\"><code class=\"time\">[' + now + ']</code><span class=\"type\">' + event + '</span></li>'));"
    "});"
    "</script>"
    );

  c.done();
}


/**
 * HandleCommand: execute command, stream output
 */
void OvmsWebServer::HandleCommand(PageEntry_t& p, PageContext_t& c)
{
  std::string command = c.getvar("command");

  c.head(200,
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Cache-Control: no-cache");
  
  if (command.empty())
    c.done();
  else
    new HttpCommandStream(c.nc, command);
}


/**
 * HandleShell: command shell
 */
void OvmsWebServer::HandleShell(PageEntry_t& p, PageContext_t& c)
{
  std::string command = c.getvar("command");
  std::string output;

  if (command != "")
    output = ExecuteCommand(command);

  // generate form:
  c.head(200);
  c.panel_start("primary", "Shell");

  c.printf(
    "<pre class=\"get-window-resize\" id=\"output\">%s</pre>"
    "<form id=\"shellform\" method=\"post\" action=\"#\">"
      "<div class=\"input-group\">"
        "<label class=\"input-group-addon hidden-xs\" for=\"input-command\">OVMS&nbsp;&gt;&nbsp;</label>"
        "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"Enter command\" name=\"command\" id=\"input-command\" value=\"%s\" autocomplete=\"section-shell\">"
        "<div class=\"input-group-btn\">"
          "<button type=\"submit\" class=\"btn btn-default\">Execute</button>"
        "</div>"
      "</div>"
    "</form>"
    , _html(output.c_str()), _attr(command.c_str()));

  c.print(
    "<script>"
    "$(\"#output\").on(\"window-resize\", function(event){"
      "$(\"#output\").height($(window).height()-220);"
      "$(\"#output\").scrollTop($(\"#output\").get(0).scrollHeight);"
    "}).trigger(\"window-resize\");"
    "$(\"#shellform\").on(\"submit\", function(event){"
      "if (!$(\"html\").hasClass(\"loading\")) {"
        "var data = $(this).serialize();"
        "var command = $(\"#input-command\").val();"
        "var output = $(\"#output\");"
        "var lastlen = 0, xhr, timeouthd, timeout = 20;"
        "if (/^(test |ota |co .* scan)/.test(command)) timeout = 60;"
        "var checkabort = function(){ if (xhr.readyState != 4) xhr.abort(\"timeout\"); };"
        "xhr = $.ajax({ \"type\": \"post\", \"url\": \"/api/execute\", \"data\": data,"
          "\"timeout\": 0,"
          "\"beforeSend\": function(){"
            "$(\"html\").addClass(\"loading\");"
            "output.html(output.html() + \"<strong>OVMS&nbsp;&gt;&nbsp;</strong><kbd>\""
              "+ $(\"<div/>\").text(command).html() + \"</kbd><br>\");"
            "output.scrollTop(output.get(0).scrollHeight);"
            "timeouthd = window.setTimeout(checkabort, timeout*1000);"
          "},"
          "\"complete\": function(){"
            "window.clearTimeout(timeouthd);"
            "$(\"html\").removeClass(\"loading\");"
          "},"
          "\"xhrFields\": {"
            "onprogress: function(e){"
              "var response = e.currentTarget.response;"
              "var addtext = response.substring(lastlen);"
              "lastlen = response.length;"
              "output.html(output.html() + $(\"<div/>\").text(addtext).html());"
              "output.scrollTop(output.get(0).scrollHeight);"
              "window.clearTimeout(timeouthd);"
              "timeouthd = window.setTimeout(checkabort, timeout*1000);"
            "},"
          "},"
          "\"error\": function(response, status, httperror){"
            "var resptext = response.responseText || httperror+\"\\n\" || status+\"\\n\";"
            "output.html(output.html() + $(\"<div/>\").text(resptext).html());"
            "output.scrollTop(output.get(0).scrollHeight);"
          "},"
        "});"
        "if (shellhist.indexOf(command) >= 0)"
          "shellhist.splice(shellhist.indexOf(command), 1);"
        "shellhpos = shellhist.push(command) - 1;"
      "}"
      "event.stopPropagation();"
      "return false;"
    "});"
    "$(\"#input-command\").on(\"keydown\", function(ev){"
      "if (ev.key == \"ArrowUp\") {"
        "shellhpos = (shellhist.length + shellhpos - 1) % shellhist.length;"
        "$(this).val(shellhist[shellhpos]);"
        "return false;"
      "}"
      "else if (ev.key == \"ArrowDown\") {"
        "shellhpos = (shellhist.length + shellhpos + 1) % shellhist.length;"
        "$(this).val(shellhist[shellhpos]);"
        "return false;"
      "}"
      "else if (ev.key == \"PageUp\") {"
        "var o = $(\"#output\");"
        "o.scrollTop(o.scrollTop() - o.height());"
        "return false;"
      "}"
      "else if (ev.key == \"PageDown\") {"
        "var o = $(\"#output\");"
        "o.scrollTop(o.scrollTop() + o.height());"
        "return false;"
      "}"
    "});"
    "$(\"#input-command\").focus();"
    "</script>");

  c.panel_end();
  c.done();
}


/**
 * HandleCfgPassword: change admin password
 */

std::string pwgen(int length)
{
  const char cs1[] = "abcdefghijklmnopqrstuvwxyz";
  const char cs2[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const char cs3[] = "!@#$%^&*()_-=+;:,.?";
  std::string res;
  for (int i=0; i < length; i++) {
    double r = drand48();
    if (r > 0.4)
      res.push_back((char)cs1[(int)(drand48()*(sizeof(cs1)-1))]);
    else if (r > 0.2)
      res.push_back((char)cs2[(int)(drand48()*(sizeof(cs2)-1))]);
    else
      res.push_back((char)cs3[(int)(drand48()*(sizeof(cs3)-1))]);
  }
  return res;
}

void OvmsWebServer::HandleCfgPassword(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  std::string oldpass, newpass1, newpass2;

  if (c.method == "POST") {
    // process form submission:
    oldpass = c.getvar("oldpass");
    newpass1 = c.getvar("newpass1");
    newpass2 = c.getvar("newpass2");

    if (oldpass != MyConfig.GetParamValue("password", "module"))
      error += "<li data-input=\"oldpass\">Old password is not correct</li>";
    if (newpass1 == oldpass)
      error += "<li data-input=\"newpass1\">New password identical to old password</li>";
    if (newpass1.length() < 8)
      error += "<li data-input=\"newpass1\">New password must have at least 8 characters</li>";
    if (newpass2 != newpass1)
      error += "<li data-input=\"newpass2\">Passwords do not match</li>";

    if (error == "") {
      // success:
      if (MyConfig.GetParamValue("password", "module") == MyConfig.GetParamValue("wifi.ap", "OVMS")) {
        MyConfig.SetParamValue("wifi.ap", "OVMS", newpass1);
        info += "<li>New Wifi AP password for network <code>OVMS</code> has been set.</li>";
      }
      
      MyConfig.SetParamValue("password", "module", newpass1);
      info += "<li>New module &amp; admin password has been set.</li>";
      
      MyConfig.SetParamValueBool("password", "changed", true);
      
      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
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
    c.head(200);
  }
  
  // show password warning:
  if (MyConfig.GetParamValue("password", "module").empty()) {
    c.alert("danger",
      "<p><strong>Warning:</strong> no admin password set. <strong>Web access is open to the public.</strong></p>"
      "<p>Please change your password now.</p>");
  }
  else if (MyConfig.GetParamValueBool("password", "changed") == false) {
    c.alert("danger",
      "<p><strong>Warning:</strong> default password has not been changed yet. <strong>Web access is open to the public.</strong></p>"
      "<p>Please change your password now.</p>");
  }
  
  // create some random passwords:
  std::ostringstream pwsugg;
  srand48(StdMetrics.ms_m_monotonic->AsInt() * StdMetrics.ms_m_freeram->AsInt());
  pwsugg << "<p>Inspiration:";
  for (int i=0; i<5; i++)
    pwsugg << " <code>" << c.encode_html(pwgen(12)) << "</code>";
  pwsugg << "</p>";
  
  // generate form:
  c.panel_start("primary", "Change module &amp; admin password");
  c.form_start(p.uri);
  c.input_password("Old password", "oldpass", "",
    NULL, NULL, "autocomplete=\"section-login current-password\"");
  c.input_password("New password", "newpass1", "",
    "Enter new password, min. 8 characters", pwsugg.str().c_str(), "autocomplete=\"section-login new-password\"");
  c.input_password("…repeat", "newpass2", "",
    "Repeat new password", NULL, "autocomplete=\"section-login new-password\"");
  c.input_button("default", "Submit");
  c.form_end();
  c.panel_end(
    (MyConfig.GetParamValue("password", "module") == MyConfig.GetParamValue("wifi.ap", "OVMS"))
    ? "<p>Note: this changes both the module and the Wifi access point password for network <code>OVMS</code>, as they are identical right now.</p>"
      "<p>You can set a separate Wifi password on the Wifi configuration page.</p>"
    : NULL);
  
  c.alert("info",
    "<p>Note: if you lose your password, you may need to erase your configuration to restore access to the module.</p>"
    "<p>To set a new password via console, registered App or SMS, execute command <kbd>config set password module …</kbd>.</p>");
  c.done();
}


/**
 * HandleCfgVehicle: configure vehicle type & identity (URL: /cfg/vehicle)
 */
void OvmsWebServer::HandleCfgVehicle(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  std::string vehicleid, vehicletype, vehiclename, timezone, units_distance;

  if (c.method == "POST") {
    // process form submission:
    vehicleid = c.getvar("vehicleid");
    vehicletype = c.getvar("vehicletype");
    vehiclename = c.getvar("vehiclename");
    timezone = c.getvar("timezone");
    units_distance = c.getvar("units_distance");

    if (vehicleid.length() == 0)
      error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
    if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
      error += "<li data-input=\"vehicleid\">Vehicle ID may only contain ASCII letters, digits and '-'</li>";

    if (error == "" && StdMetrics.ms_v_type->AsString() != vehicletype) {
      MyVehicleFactory.SetVehicle(vehicletype.c_str());
      if (!MyVehicleFactory.ActiveVehicle())
        error += "<li data-input=\"vehicletype\">Cannot set vehicle type <code>" + vehicletype + "</code></li>";
      else
        info += "<li>New vehicle type <code>" + vehicletype + "</code> has been set.</li>";
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      MyConfig.SetParamValue("auto", "vehicle.type", vehicletype);
      MyConfig.SetParamValue("vehicle", "name", vehiclename);
      MyConfig.SetParamValue("vehicle", "timezone", timezone);
      MyConfig.SetParamValue("vehicle", "units.distance", units_distance);

      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      info += "<script>$(\"#menu\").load(\"/menu\")</script>";
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
    vehicleid = MyConfig.GetParamValue("vehicle", "id");
    vehicletype = MyConfig.GetParamValue("auto", "vehicle.type");
    vehiclename = MyConfig.GetParamValue("vehicle", "name");
    timezone = MyConfig.GetParamValue("vehicle", "timezone");
    units_distance = MyConfig.GetParamValue("vehicle", "units.distance");
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Vehicle configuration");
  c.form_start(p.uri);
  c.input_select_start("Vehicle type", "vehicletype");
  c.input_select_option("&mdash;", "", vehicletype.empty());
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    c.input_select_option(k->second.name, k->first, (vehicletype == k->first));
  c.input_select_end();
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    "<p>Note: this is also the <strong>vehicle account ID</strong> for server connections.</p>");
  c.input_text("Vehicle name", "vehiclename", vehiclename.c_str(), "optional, the name of your car");
  c.input_text("Time zone", "timezone", timezone.c_str(), "optional, default UTC");
  c.input_select_start("Distance units", "units_distance");
  c.input_select_option("Kilometers", "K", units_distance == "K");
  c.input_select_option("Miles", "M", units_distance == "M");
  c.input_select_end();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgModem: configure APN & modem features (URL /cfg/modem)
 */
void OvmsWebServer::HandleCfgModem(PageEntry_t& p, PageContext_t& c)
{
  std::string apn, apn_user, apn_pass, network_dns;
  bool enable_gps, enable_gpstime, enable_net, enable_sms;

  if (c.method == "POST") {
    // process form submission:
    apn = c.getvar("apn");
    apn_user = c.getvar("apn_user");
    apn_pass = c.getvar("apn_pass");
    network_dns = c.getvar("network_dns");
    enable_net = (c.getvar("enable_net") == "yes");
    enable_sms = (c.getvar("enable_sms") == "yes");
    enable_gps = (c.getvar("enable_gps") == "yes");
    enable_gpstime = (c.getvar("enable_gpstime") == "yes");

    MyConfig.SetParamValue("modem", "apn", apn);
    MyConfig.SetParamValue("modem", "apn.user", apn_user);
    MyConfig.SetParamValue("modem", "apn.password", apn_pass);
    MyConfig.SetParamValue("network", "dns", network_dns);
    MyConfig.SetParamValueBool("modem", "enable.net", enable_net);
    MyConfig.SetParamValueBool("modem", "enable.sms", enable_sms);
    MyConfig.SetParamValueBool("modem", "enable.gps", enable_gps);
    MyConfig.SetParamValueBool("modem", "enable.gpstime", enable_gpstime);

    c.head(200);
    c.alert("success", "<p class=\"lead\">Modem configured.</p>");
    OutputHome(p, c);
    c.done();
    return;
  }

  // read configuration:
  apn = MyConfig.GetParamValue("modem", "apn");
  apn_user = MyConfig.GetParamValue("modem", "apn.user");
  apn_pass = MyConfig.GetParamValue("modem", "apn.password");
  network_dns = MyConfig.GetParamValue("network", "dns");
  enable_net = MyConfig.GetParamValueBool("modem", "enable.net", true);
  enable_sms = MyConfig.GetParamValueBool("modem", "enable.sms", true);
  enable_gps = MyConfig.GetParamValueBool("modem", "enable.gps", false);
  enable_gpstime = MyConfig.GetParamValueBool("modem", "enable.gpstime", false);

  // generate form:
  c.head(200);
  c.panel_start("primary", "Modem configuration");
  c.form_start(p.uri);

  c.fieldset_start("Internet");
  c.input_checkbox("Enable IP networking", "enable_net", enable_net);
  c.input_text("APN", "apn", apn.c_str(), NULL,
    "<p>For Hologram, use APN <code>hologram</code> with empty username &amp; password</p>");
  c.input_text("…username", "apn_user", apn_user.c_str());
  c.input_text("…password", "apn_pass", apn_pass.c_str());
  c.input_text("DNS", "network_dns", network_dns.c_str(), "optional fixed DNS servers (space separated)",
    "<p>Set this to i.e. <code>8.8.8.8 8.8.4.4</code> (Google public DNS) if you encounter problems with your network provider DNS</p>");
  c.fieldset_end();

  c.fieldset_start("Features");
  c.input_checkbox("Enable SMS", "enable_sms", enable_sms);
  c.input_checkbox("Enable GPS", "enable_gps", enable_gps);
  c.input_checkbox("Use GPS time", "enable_gpstime", enable_gpstime,
    "<p>Note: GPS &amp; GPS time support can be left disabled, vehicles will activate them as needed</p>");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgServerV2: configure server v2 connection (URL /cfg/server/v2)
 */
void OvmsWebServer::HandleCfgServerV2(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string server, vehicleid, password, port;
  std::string updatetime_connected, updatetime_idle;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    vehicleid = c.getvar("vehicleid");
    password = c.getvar("password");
    port = c.getvar("port");
    updatetime_connected = c.getvar("updatetime_connected");
    updatetime_idle = c.getvar("updatetime_idle");

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
      MyConfig.SetParamValue("server.v2", "port", port);
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      if (password != "")
        MyConfig.SetParamValue("server.v2", "password", password);
      MyConfig.SetParamValue("server.v2", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v2", "updatetime.idle", updatetime_idle);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Server V2 (MP) connection configured.</p>");
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
    vehicleid = MyConfig.GetParamValue("vehicle", "id");
    password = MyConfig.GetParamValue("server.v2", "password");
    port = MyConfig.GetParamValue("server.v2", "port");
    updatetime_connected = MyConfig.GetParamValue("server.v2", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v2", "updatetime.idle");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V2 (MP) configuration");
  c.form_start(p.uri);

  c.input_text("Host", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V2 servers:</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
    "</ul>");
  c.input_text("Port", "port", port.c_str(), "optional, default: 6867");
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    NULL, "autocomplete=\"section-serverv2 username\"");
  c.input_password("Vehicle password", "password", "", "empty = no change",
    "<p>Note: enter the password for the <strong>vehicle ID account</strong>, <em>not</em> your user account password</p>",
    "autocomplete=\"section-serverv2 current-password\"");

  c.fieldset_start("Update intervals");
  c.input_text("…connected", "updatetime_connected", updatetime_connected.c_str(),
    "optional, in seconds, default: 60");
  c.input_text("…idle", "updatetime_idle", updatetime_idle.c_str(),
    "optional, in seconds, default: 600");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgServerV3: configure server v3 connection (URL /cfg/server/v3)
 */
void OvmsWebServer::HandleCfgServerV3(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string server, user, password, port;
  std::string updatetime_connected, updatetime_idle;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    user = c.getvar("user");
    password = c.getvar("password");
    port = c.getvar("port");
    updatetime_connected = c.getvar("updatetime_connected");
    updatetime_idle = c.getvar("updatetime_idle");

    // validate:
    if (port != "") {
      if (port.find_first_not_of("0123456789") != std::string::npos
          || atoi(port.c_str()) < 0 || atoi(port.c_str()) > 65535) {
        error += "<li data-input=\"port\">Port must be an integer value in the range 0…65535</li>";
      }
    }
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
      MyConfig.SetParamValue("server.v3", "server", server);
      MyConfig.SetParamValue("server.v3", "user", user);
      if (password != "")
        MyConfig.SetParamValue("server.v3", "password", password);
      MyConfig.SetParamValue("server.v3", "port", port);
      MyConfig.SetParamValue("server.v3", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v3", "updatetime.idle", updatetime_idle);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Server V3 (MQTT) connection configured.</p>");
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
    server = MyConfig.GetParamValue("server.v3", "server");
    user = MyConfig.GetParamValue("server.v3", "user");
    password = MyConfig.GetParamValue("server.v3", "password");
    port = MyConfig.GetParamValue("server.v3", "port");
    updatetime_connected = MyConfig.GetParamValue("server.v3", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v3", "updatetime.idle");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V3 (MQTT) configuration");
  c.form_start(p.uri);

  c.input_text("Host", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V3 servers (MQTT brokers):</p>"
    "<ul>"
      "<li><code>io.adafruit.com</code> <a href=\"https://accounts.adafruit.com/users/sign_in\" target=\"_blank\">Registration</a></li>"
      "<li><a href=\"https://github.com/mqtt/mqtt.github.io/wiki/public_brokers\" target=\"_blank\">More public MQTT brokers</a></li>"
    "</ul>");
  c.input_text("Port", "port", port.c_str(), "optional, default: 1883");
  c.input_text("Username", "user", user.c_str(), "Enter user login name",
    NULL, "autocomplete=\"section-serverv3 username\"");
  c.input_password("Password", "password", "", "Enter user password, empty = no change",
    NULL, "autocomplete=\"section-serverv3 current-password\"");

  c.fieldset_start("Update intervals");
  c.input_text("…connected", "updatetime_connected", updatetime_connected.c_str(),
    "optional, in seconds, default: 60");
  c.input_text("…idle", "updatetime_idle", updatetime_idle.c_str(),
    "optional, in seconds, default: 600");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgWebServer: configure web server (URL /cfg/webserver)
 */
void OvmsWebServer::HandleCfgWebServer(PageEntry_t& p, PageContext_t& c)
{
  std::string error, warn;
  std::string docroot, auth_domain, auth_file;
  bool enable_files, enable_dirlist, auth_global;

  if (c.method == "POST") {
    // process form submission:
    docroot = c.getvar("docroot");
    auth_domain = c.getvar("auth_domain");
    auth_file = c.getvar("auth_file");
    enable_files = (c.getvar("enable_files") == "yes");
    enable_dirlist = (c.getvar("enable_dirlist") == "yes");
    auth_global = (c.getvar("auth_global") == "yes");

    // validate:
    if (docroot != "" && docroot[0] != '/') {
      error += "<li data-input=\"docroot\">Document root must start with '/'</li>";
    }
    if (docroot == "/" || docroot == "/store" || docroot == "/store/" || startsWith(docroot, "/store/ovms_config")) {
      warn += "<li data-input=\"docroot\">Document root <code>" + docroot + "</code> may open access to OVMS configuration files, consider using a sub directory</li>";
    }

    if (error == "") {
      // success:
      if (docroot == "")      MyConfig.DeleteInstance("http.server", "docroot");
      else                    MyConfig.SetParamValue("http.server", "docroot", docroot);
      if (auth_domain == "")  MyConfig.DeleteInstance("http.server", "auth.domain");
      else                    MyConfig.SetParamValue("http.server", "auth.domain", auth_domain);
      if (auth_file == "")    MyConfig.DeleteInstance("http.server", "auth.file");
      else                    MyConfig.SetParamValue("http.server", "auth.file", auth_file);

      MyConfig.SetParamValueBool("http.server", "enable.files", enable_files);
      MyConfig.SetParamValueBool("http.server", "enable.dirlist", enable_dirlist);
      MyConfig.SetParamValueBool("http.server", "auth.global", auth_global);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Webserver configuration saved.</p>");
      if (warn != "") {
        warn = "<p class=\"lead\">Warning:</p><ul class=\"warnlist\">" + warn + "</ul>";
        c.alert("warning", warn.c_str());
      }
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
    docroot = MyConfig.GetParamValue("http.server", "docroot");
    auth_domain = MyConfig.GetParamValue("http.server", "auth.domain");
    auth_file = MyConfig.GetParamValue("http.server", "auth.file");
    enable_files = MyConfig.GetParamValueBool("http.server", "enable.files", true);
    enable_dirlist = MyConfig.GetParamValueBool("http.server", "enable.dirlist", true);
    auth_global = MyConfig.GetParamValueBool("http.server", "auth.global", true);

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Webserver configuration");
  c.form_start(p.uri);

  c.input_checkbox("Enable file access", "enable_files", enable_files);
  c.input_text("Root path", "docroot", docroot.c_str(), "Default: /sd");
  c.input_checkbox("Enable directory listings", "enable_dirlist", enable_dirlist);

  c.input_checkbox("Enable global file auth", "auth_global", auth_global,
    "<p>If enabled, file access is globally protected by the admin password (if set).</p>"
    "<p>Disabling allows public access to directories without an auth file.</p>");
  c.input_text("Directory auth file", "auth_file", auth_file.c_str(), "Default: .htaccess",
    "<p>Note: sub directories do not inherit the parent auth file.</p>");
  c.input_text("Auth domain/realm", "auth_domain", auth_domain.c_str(), "Default: ovms");

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgWifi: configure wifi networks (URL /cfg/wifi)
 */

void OvmsWebServer::HandleCfgWifi(PageEntry_t& p, PageContext_t& c)
{
  if (c.method == "POST") {
    std::string warn;

    // process form submission:
    UpdateWifiTable(p, c, "ap", "wifi.ap", warn);
    UpdateWifiTable(p, c, "cl", "wifi.ssid", warn);

    c.head(200);
    c.alert("success", "<p class=\"lead\">Wifi configuration saved.</p>");
    if (warn != "") {
      warn = "<p class=\"lead\">Warning:</p><ul class=\"warnlist\">" + warn + "</ul>";
      c.alert("warning", warn.c_str());
    }
    OutputHome(p, c);
    c.done();
    return;
  }

  // generate form:
  c.head(200);
  c.panel_start("primary", "Wifi configuration");
  c.printf(
    "<form method=\"post\" action=\"%s\" target=\"#main\">"
    , _attr(p.uri));

  c.fieldset_start("Access point networks");
  OutputWifiTable(p, c, "ap", "wifi.ap");
  c.fieldset_end();

  c.fieldset_start("Wifi client networks");
  OutputWifiTable(p, c, "cl", "wifi.ssid");
  c.fieldset_end();

  c.print(
    "<hr>"
    "<button type=\"submit\" class=\"btn btn-default center-block\">Save</button>"
    "</form>"
    "<script>"
    "function delRow(el){"
      "$(el).parent().parent().remove();"
    "}"
    "function addRow(el){"
      "var counter = $(el).parents('table').first().prev();"
      "var pfx = counter.attr(\"name\");"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"' + pfx + '_ssid_' + nr + '\" placeholder=\"SSID\""
              " autocomplete=\"section-wifi-' + pfx + ' username\"></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"' + pfx + '_pass_' + nr + '\" placeholder=\"Passphrase\""
              " autocomplete=\"section-wifi-' + pfx + ' current-password\"></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "</script>");

  c.panel_end();
  c.done();
}

void OvmsWebServer::OutputWifiTable(PageEntry_t& p, PageContext_t& c, const std::string prefix, const std::string paramname)
{
  OvmsConfigParam* param = MyConfig.CachedParam(paramname);

  c.printf(
    "<div class=\"table-responsive\">"
      "<input type=\"hidden\" name=\"%s\" value=\"%d\">"
      "<table class=\"table\">"
        "<thead>"
          "<tr>"
            "<th width=\"10%\"></th>"
            "<th width=\"45%\">SSID</th>"
            "<th width=\"45%\">Passphrase</th>"
          "</tr>"
        "</thead>"
        "<tbody>"
    , _attr(prefix), param->m_map.size());

  int pos = 0;
  for (auto const& kv : param->m_map) {
    pos++;
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"%s_ssid_%d\" value=\"%s\"></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"%s_pass_%d\" placeholder=\"no change\"></td>"
          "</tr>"
      , _attr(prefix), pos, _attr(kv.first)
      , _attr(prefix), pos);
  }

  c.print(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
    "</div>");
}

void OvmsWebServer::UpdateWifiTable(PageEntry_t& p, PageContext_t& c, const std::string prefix, const std::string paramname, std::string& warn)
{
  OvmsConfigParam* param = MyConfig.CachedParam(paramname);
  int i, max;
  std::string ssid, pass;
  char buf[50];
  ConfigParamMap newmap;

  max = atoi(c.getvar(prefix.c_str()).c_str());

  for (i = 1; i <= max; i++) {
    sprintf(buf, "%s_ssid_%d", prefix.c_str(), i);
    ssid = c.getvar(buf);
    if (ssid == "")
      continue;
    sprintf(buf, "%s_pass_%d", prefix.c_str(), i);
    pass = c.getvar(buf);
    if (pass == "")
      pass = param->GetValue(ssid);
    if (pass == "")
      warn += "<li>SSID <code>" + ssid + "</code> has no password</li>";
    newmap[ssid] = pass;
  }

  param->m_map.clear();
  param->m_map = std::move(newmap);
  param->Save();
}


/**
 * HandleCfgAutoInit: configure auto init (URL /cfg/autostart)
 */
void OvmsWebServer::HandleCfgAutoInit(PageEntry_t& p, PageContext_t& c)
{
  std::string error, warn;
  bool init, ext12v, modem, server_v2, server_v3;
  std::string vehicle_type, obd2ecu, wifi_mode, wifi_ssid_client, wifi_ssid_ap;

  if (c.method == "POST") {
    // process form submission:
    init = (c.getvar("init") == "yes");
    ext12v = (c.getvar("ext12v") == "yes");
    modem = (c.getvar("modem") == "yes");
    server_v2 = (c.getvar("server_v2") == "yes");
    server_v3 = (c.getvar("server_v3") == "yes");
    vehicle_type = c.getvar("vehicle_type");
    obd2ecu = c.getvar("obd2ecu");
    wifi_mode = c.getvar("wifi_mode");
    wifi_ssid_ap = c.getvar("wifi_ssid_ap");
    wifi_ssid_client = c.getvar("wifi_ssid_client");

    // store:
    MyConfig.SetParamValueBool("auto", "init", init);
    MyConfig.SetParamValueBool("auto", "ext12v", ext12v);
    MyConfig.SetParamValueBool("auto", "modem", modem);
    MyConfig.SetParamValueBool("auto", "server.v2", server_v2);
    MyConfig.SetParamValueBool("auto", "server.v3", server_v3);
    MyConfig.SetParamValue("auto", "vehicle.type", vehicle_type);
    MyConfig.SetParamValue("auto", "obd2ecu", obd2ecu);
    MyConfig.SetParamValue("auto", "wifi.mode", wifi_mode);
    MyConfig.SetParamValue("auto", "wifi.ssid.ap", wifi_ssid_ap);
    MyConfig.SetParamValue("auto", "wifi.ssid.client", wifi_ssid_client);

    c.head(200);
    c.alert("success", "<p class=\"lead\">Auto start configuration saved.</p>");
    OutputHome(p, c);
    c.done();
    return;
  }

  // read configuration:
  init = MyConfig.GetParamValueBool("auto", "init", true);
  ext12v = MyConfig.GetParamValueBool("auto", "ext12v", false);
  modem = MyConfig.GetParamValueBool("auto", "modem", false);
  server_v2 = MyConfig.GetParamValueBool("auto", "server.v2", false);
  server_v3 = MyConfig.GetParamValueBool("auto", "server.v3", false);
  vehicle_type = MyConfig.GetParamValue("auto", "vehicle.type");
  obd2ecu = MyConfig.GetParamValue("auto", "obd2ecu");
  wifi_mode = MyConfig.GetParamValue("auto", "wifi.mode", "ap");
  wifi_ssid_ap = MyConfig.GetParamValue("auto", "wifi.ssid.ap");
  if (wifi_ssid_ap.empty())
    wifi_ssid_ap = "OVMS";
  wifi_ssid_client = MyConfig.GetParamValue("auto", "wifi.ssid.client");

  // generate form:
  c.head(200);

  c.panel_start("primary", "Auto start configuration");
  c.form_start(p.uri);

  c.input_checkbox("Enable auto start", "init", init,
    "<p>Note: if a crash or reboot occurs within 10 seconds after powering the module, "
    "this option will automatically be disabled and need to be re-enabled manually.</p>");

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

  c.input_checkbox("Start modem", "modem", modem,
    "<p>Note: a vehicle module may start the modem as necessary, independantly of this option.</p>");

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
  c.input_select_end(
    "<p>OBD2ECU translates OVMS to OBD2 metrics, i.e. to drive standard ECU displays</p>");

  c.input_checkbox("Start server V2", "server_v2", server_v2);
  c.input_checkbox("Start server V3", "server_v3", server_v3);

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
