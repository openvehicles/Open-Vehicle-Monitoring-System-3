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
#include <fstream>
#include <dirent.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"
#include "ovms_housekeeping.h"
#include "ovms_peripherals.h"
#include "ovms_version.h"

#ifdef CONFIG_OVMS_COMP_OTA
#include "ovms_ota.h"
#endif

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleStatus: show status overview
 */
void OvmsWebServer::HandleStatus(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);

  if (c.method == "POST") {
    cmd = c.getvar("action");
    if (cmd == "reboot") {
      OutputReboot(p, c);
      c.done();
      return;
    }
  }

  PAGE_HOOK("body.pre");
  c.print(
    "<div id=\"livestatus\" class=\"receiver\">"
    "<div class=\"row flex\">"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Live");
  c.print(
    "<div class=\"table-responsive\">"
      "<table class=\"table table-bordered table-condensed\">"
        "<tbody>"
          "<tr>"
            "<th>Module</th>"
            "<td>"
              "<div class=\"metric number\" data-metric=\"m.freeram\"><span class=\"value\">?</span><span class=\"unit\">bytes free</span></div>"
              "<div class=\"metric number\" data-metric=\"m.tasks\"><span class=\"value\">?</span><span class=\"unit\">tasks running</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>Network</th>"
            "<td>"
              "<div class=\"metric text\" data-metric=\"m.net.provider\"><span class=\"value\">?</span><span class=\"metric unit\" data-metric=\"m.net.type\">?</span></div>"
              "<div class=\"metric number\" data-metric=\"m.net.sq\"><span class=\"value\">?</span><span class=\"unit\">dBm</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>Main battery</th>"
            "<td>"
              "<div class=\"metric number\" data-metric=\"v.b.soc\" data-prec=\"1\"><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
              "<div class=\"metric number\" data-metric=\"v.b.voltage\" data-prec=\"1\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
              "<div class=\"metric number\" data-metric=\"v.b.current\" data-prec=\"1\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
            "</td>"
          "</tr>"
          "<tr>"
            "<th>12V battery</th>"
            "<td>"
              "<div class=\"metric number\" data-metric=\"v.b.12v.voltage\" data-prec=\"1\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>"
              "<div class=\"metric number\" data-metric=\"v.b.12v.current\" data-prec=\"1\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>"
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
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Vehicle");
  output = ExecuteCommand("stat");
  c.printf("<samp class=\"monitor\" id=\"vehicle-status\" data-updcmd=\"stat\" data-events=\"vehicle.charge\">%s</samp>", _html(output));
  output = ExecuteCommand("location status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"location status\" data-events=\"gps.lock|location\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#vehicle-cmdres\" data-cmd=\"charge start\">Start charge</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#vehicle-cmdres\" data-cmd=\"charge stop\">Stop charge</button></li>"
      "<li><samp id=\"vehicle-cmdres\" class=\"samp-inline\"></samp></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Server");
  output = ExecuteCommand("server v2 status");
  if (!startsWith(output, "Unrecognised"))
    c.printf("<samp class=\"monitor\" id=\"server-v2\" data-updcmd=\"server v2 status\" data-events=\"server.v2\">%s</samp>", _html(output));
  output = ExecuteCommand("server v3 status");
  if (!startsWith(output, "Unrecognised"))
    c.printf("<samp class=\"monitor\" id=\"server-v3\" data-updcmd=\"server v3 status\" data-events=\"server.v3\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-cmdres\" data-cmd=\"server v2 start\">Start V2</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-cmdres\" data-cmd=\"server v2 stop\">Stop V2</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-cmdres\" data-cmd=\"server v3 start\">Start V3</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#server-cmdres\" data-cmd=\"server v3 stop\">Stop V3</button></li>"
      "<li><samp id=\"server-cmdres\" class=\"samp-inline\"></samp></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "SD Card");
  output = ExecuteCommand("sd status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"sd status\" data-events=\"^sd\\.\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#sd-cmdres\" data-cmd=\"sd mount\">Mount</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#sd-cmdres\" data-cmd=\"sd unmount 0\">Unmount</button></li>"
      "<li><samp id=\"sd-cmdres\" class=\"samp-inline\"></samp></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Module");
  output = ExecuteCommand("boot status");
  c.printf("<samp>%s</samp>", _html(output));
  c.print("<hr>");
  output = ExecuteCommand("ota status nocheck");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"reboot\">Reboot</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Network");
  output = ExecuteCommand("network status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"network status\" data-events=\"^network\">%s</samp>", _html(output));
  c.panel_end();

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Wifi");
  output = ExecuteCommand("wifi status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"wifi status\" data-events=\"^system.wifi\">%s</samp>", _html(output));
  c.panel_end();

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Modem");
  output = ExecuteCommand("simcom status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"simcom status\" data-events=\"system.modem\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"power simcom on\">Start modem</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"power simcom off\">Stop modem</button></li>"
      "<li><samp id=\"modem-cmdres\" class=\"samp-inline\"></samp></li>"
    "</ul>");

  c.print(
    "</div>"
    "</div>"
    "<script>"
    "$(\"button[name=action]\").on(\"click\", function(ev){"
      "loaduri(\"#main\", \"post\", \"/status\", { \"action\": $(this).val() });"
    "});"
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

  PAGE_HOOK("body.post");
  c.done();
}


/**
 * HandleCommand: execute command, stream output
 */
void OvmsWebServer::HandleCommand(PageEntry_t& p, PageContext_t& c)
{
  std::string command = c.getvar("command", 2000);
  std::string output = c.getvar("output");

  // Note: application/octet-stream default instead of text/plain is a workaround for an *old*
  //  Chrome/Webkit bug: chunked text/plain is always buffered for the first 1024 bytes.
  if (output == "text") {
    c.head(200,
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Cache-Control: no-cache");
  } else {
    c.head(200,
      "Content-Type: application/octet-stream; charset=utf-8\r\n"
      "Cache-Control: no-cache");
  }

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
  std::string command = c.getvar("command", 2000);
  std::string output;

  if (command != "")
    output = ExecuteCommand(command);

  // generate form:
  c.head(200);
  PAGE_HOOK("body.pre");
  c.panel_start("primary panel-minpad", "Shell");

  c.printf(
    "<pre class=\"get-window-resize\" id=\"output\">%s</pre>"
    "<form id=\"shellform\" method=\"post\" action=\"#\">"
      "<div class=\"input-group\">"
        "<label class=\"input-group-addon hidden-xs\" for=\"input-command\">OVMS#</label>"
        "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"Enter command\" name=\"command\" id=\"input-command\" value=\"%s\" autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"section-shell\" spellcheck=\"false\">"
        "<div class=\"input-group-btn\">"
          "<button type=\"submit\" class=\"btn btn-default\">Execute</button>"
        "</div>"
      "</div>"
    "</form>"
    , _html(output.c_str()), _attr(command.c_str()));

  c.print(
    "<style>"
    ".fullscreened #output {"
      "border: 0 none;"
    "}"
    "@media (max-width: 767px) {"
      "#output {"
        "border: 0 none;"
      "}"
    "}"
    "</style>"
    "<script>"
    "$(window).on(\"resize\", function(){"
      "var pad = Number.parseInt($(\"#output\").parent().css(\"padding-top\")) + Number.parseInt($(\"#output\").parent().css(\"padding-bottom\"));"
      "var h = $(window).height() - $(\"#output\").offset().top - pad - 73;"
      "if ($(window).width() <= 767) h += 27;"
      "if ($(\"body\").hasClass(\"fullscreened\")) h -= 4;"
      "$(\"#output\").height(h);"
      "$(\"#output\").scrollTop($(\"#output\").get(0).scrollHeight);"
    "}).trigger(\"resize\");"
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
            "output.html(output.html() + \"<strong>OVMS#</strong>&nbsp;<kbd>\""
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
              "if (e.currentTarget.status != 200)"
                "return;"
              "var response = e.currentTarget.response;"
              "var addtext = response.substring(lastlen);"
              "lastlen = response.length;"
              "output.html(output.html() + $(\"<div/>\").text(addtext).html());"
              "output.scrollTop(output.get(0).scrollHeight);"
              "window.clearTimeout(timeouthd);"
              "timeouthd = window.setTimeout(checkabort, timeout*1000);"
            "},"
          "},"
          "\"error\": function(response, xhrerror, httperror){"
            "var txt;"
            "if (response.status == 401 || response.status == 403)"
              "txt = \"Session expired. <a class=\\\"btn btn-sm btn-default\\\" href=\\\"javascript:reloadpage()\\\">Login</a>\";"
            "else if (response.status >= 400)"
              "txt = \"Error \" + response.status + \" \" + response.statusText;"
            "else"
              "txt = \"Request \" + (xhrerror||\"failed\") + \", please retry\";"
            "output.html(output.html() + '<div class=\"bg-danger\">'+txt+'</div>');"
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
  PAGE_HOOK("body.post");
  c.done();
}


/**
 * HandleCfgPassword: change admin password
 */

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

  // create some random passwords:
  std::ostringstream pwsugg;
  srand48(StdMetrics.ms_m_monotonic->AsInt() * StdMetrics.ms_m_freeram->AsInt());
  pwsugg << "<p>Inspiration:";
  for (int i=0; i<5; i++)
    pwsugg << " <code class=\"autoselect\">" << c.encode_html(pwgen(12)) << "</code>";
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
  std::string vehicleid, vehicletype, vehiclename, timezone, timezone_region, units_distance, pin;

  if (c.method == "POST") {
    // process form submission:
    vehicleid = c.getvar("vehicleid");
    vehicletype = c.getvar("vehicletype");
    vehiclename = c.getvar("vehiclename");
    timezone = c.getvar("timezone");
    timezone_region = c.getvar("timezone_region");
    units_distance = c.getvar("units_distance");
    pin = c.getvar("pin");

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
      MyConfig.SetParamValue("vehicle", "timezone_region", timezone_region);
      MyConfig.SetParamValue("vehicle", "units.distance", units_distance);
      if (!pin.empty())
        MyConfig.SetParamValue("password", "pin", pin);

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
    timezone_region = MyConfig.GetParamValue("vehicle", "timezone_region");
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

  c.printf(
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-timezone_select\">Time zone:</label>"
      "<div class=\"col-sm-9\">"
        "<input type=\"hidden\" name=\"timezone_region\" id=\"input-timezone_region\" value=\"%s\">"
        "<select class=\"form-control\" id=\"input-timezone_select\">"
          "<option>-Custom-</option>"
        "</select>"
      "</div>"
    "</div>"
    "<div class=\"form-group\">"
      "<div class=\"col-sm-9 col-sm-offset-3\">"
        "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"optional, default UTC\" name=\"timezone\" id=\"input-timezone\" value=\"%s\">"
        "<span class=\"help-block\"><p>Web links: <a target=\"_blank\" href=\"https://remotemonitoringsystems.ca/time-zone-abbreviations.php\">Example Timezone Strings</a>, <a target=\"_blank\" href=\"https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html\">Glibc manual</a></p></span>"
      "</div>"
    "</div>"
    , _attr(timezone_region)
    , _attr(timezone));

  c.input_radiobtn_start("Distance units", "units_distance");
  c.input_radiobtn_option("units_distance", "Kilometers", "K", units_distance == "K");
  c.input_radiobtn_option("units_distance", "Miles", "M", units_distance == "M");
  c.input_radiobtn_end();
  c.input_password("PIN", "pin", "", "empty = no change",
    "<p>Vehicle PIN code used for unlocking etc.</p>", "autocomplete=\"section-vehiclepin new-password\"");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.print(
    "<script>"
    "$.getJSON(\"" URL_ASSETS_ZONES_JSON "\", function(data) {"
      "var items = [];"
      "var region = $('#input-timezone_region').val();"
      "$.each(data, function(key, val) {"
        "items.push('<option data-tz=\"' + val + '\"' + (key==region ? ' selected' : '') + '>' + key + '</option>');"
      "});"
      "$('#input-timezone_select').append(items.join(''));"
      "$('#input-timezone_select').on('change', function(ev){"
        "var opt = $(this).find('option:selected');"
        "$('#input-timezone_region').val(opt.val());"
        "var tz = opt.data(\"tz\");"
        "if (tz) {"
          "$('#input-timezone').val(tz);"
          "$('#input-timezone').prop('readonly', true);"
        "} else {"
          "$('#input-timezone').prop('readonly', false);"
        "}"
      "}).trigger('change');"
    "});"
    "</script>");

  c.done();
}


#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
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

  std::string info;
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
  c.input_checkbox("Use GPS time", "enable_gpstime", enable_gpstime);
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif


#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V2
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

  c.input_text("Server", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V2 servers:</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
    "</ul>");
  c.input_text("Port", "port", port.c_str(), "optional, default: 6867");
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

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif

#ifdef CONFIG_OVMS_COMP_SERVER_V3
/**
 * HandleCfgServerV3: configure server v3 connection (URL /cfg/server/v3)
 */
void OvmsWebServer::HandleCfgServerV3(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string server, user, password, port, topic_prefix;
  std::string updatetime_connected, updatetime_idle;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    user = c.getvar("user");
    password = c.getvar("password");
    port = c.getvar("port");
    topic_prefix = c.getvar("topic_prefix");
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
        MyConfig.SetParamValue("password", "server.v3", password);
      MyConfig.SetParamValue("server.v3", "port", port);
      MyConfig.SetParamValue("server.v3", "topic_prefix", topic_prefix);
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
    password = MyConfig.GetParamValue("password", "server.v3");
    port = MyConfig.GetParamValue("server.v3", "port");
    topic_prefix = MyConfig.GetParamValue("server.v3", "topic_prefix");
    updatetime_connected = MyConfig.GetParamValue("server.v3", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v3", "updatetime.idle");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V3 (MQTT) configuration");
  c.form_start(p.uri);

  c.input_text("Server", "server", server.c_str(), "Enter host name or IP address",
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
  c.input_text("Topic Prefix", "topic_prefix", topic_prefix.c_str(),
    "optional, default: ovms/<username>/<vehicle id>/");

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
#endif
#endif


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
    std::string warn, error;

    // process form submission:
    UpdateWifiTable(p, c, "ap", "wifi.ap", warn, error, 8);
    UpdateWifiTable(p, c, "client", "wifi.ssid", warn, error, 0);

    if (error == "") {
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

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Wifi configuration");
  c.printf(
    "<form method=\"post\" action=\"%s\" target=\"#main\">"
    , _attr(p.uri));

  c.fieldset_start("Access point networks");
  OutputWifiTable(p, c, "ap", "wifi.ap", MyConfig.GetParamValue("auto", "wifi.ssid.ap", "OVMS"));
  c.fieldset_end();

  c.fieldset_start("Wifi client networks");
  OutputWifiTable(p, c, "client", "wifi.ssid", MyConfig.GetParamValue("auto", "wifi.ssid.client"));
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

void OvmsWebServer::OutputWifiTable(PageEntry_t& p, PageContext_t& c, const std::string prefix, const std::string paramname, const std::string autostart_ssid)
{
  OvmsConfigParam* param = MyConfig.CachedParam(paramname);
  int pos = 0, pos_autostart = 0, max;
  char buf[50];
  std::string ssid, pass;

  if (c.method == "POST") {
    max = atoi(c.getvar(prefix.c_str()).c_str());
    sprintf(buf, "%s_autostart", prefix.c_str());
    pos_autostart = atoi(c.getvar(buf).c_str());
  }
  else {
    max = param->m_map.size();
  }

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
    , _attr(prefix), max);

  if (c.method == "POST") {
    for (pos = 1; pos <= max; pos++) {
      sprintf(buf, "%s_ssid_%d", prefix.c_str(), pos);
      ssid = c.getvar(buf);
      c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\" %s><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"%s_ssid_%d\" value=\"%s\"></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"%s_pass_%d\" placeholder=\"no change\"></td>"
          "</tr>"
      , (pos == pos_autostart) ? "disabled title=\"Current autostart network\"" : ""
      , _attr(prefix), pos, _attr(ssid)
      , _attr(prefix), pos);
    }
  }
  else {
    for (auto const& kv : param->m_map) {
      pos++;
      ssid = kv.first;
      if (ssid == autostart_ssid)
        pos_autostart = pos;
      c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\" %s><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"%s_ssid_%d\" value=\"%s\"></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"%s_pass_%d\" placeholder=\"no change\"></td>"
          "</tr>"
      , (pos == pos_autostart) ? "disabled title=\"Current autostart network\"" : ""
      , _attr(prefix), pos, _attr(ssid)
      , _attr(prefix), pos);
    }
  }

  c.print(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>");
  c.printf(
      "<input type=\"hidden\" name=\"%s_autostart\" value=\"%d\">"
    "</div>"
    , _attr(prefix), pos_autostart);
}

void OvmsWebServer::UpdateWifiTable(PageEntry_t& p, PageContext_t& c, const std::string prefix, const std::string paramname,
  std::string& warn, std::string& error, int pass_minlen)
{
  OvmsConfigParam* param = MyConfig.CachedParam(paramname);
  int i, max, pos_autostart;
  std::string ssid, pass, ssid_autostart;
  char buf[50];
  ConfigParamMap newmap;

  max = atoi(c.getvar(prefix.c_str()).c_str());
  sprintf(buf, "%s_autostart", prefix.c_str());
  pos_autostart = atoi(c.getvar(buf).c_str());

  for (i = 1; i <= max; i++) {
    sprintf(buf, "%s_ssid_%d", prefix.c_str(), i);
    ssid = c.getvar(buf);
    if (ssid == "") {
      if (i == pos_autostart)
        error += "<li>Autostart SSID may not be empty</li>";
      continue;
    }
    sprintf(buf, "%s_pass_%d", prefix.c_str(), i);
    pass = c.getvar(buf);
    if (pass == "")
      pass = param->GetValue(ssid);
    if (pass == "") {
      if (i == pos_autostart)
        error += "<li>Autostart SSID <code>" + ssid + "</code> has no password</li>";
      else
        warn += "<li>SSID <code>" + ssid + "</code> has no password</li>";
    }
    else if (pass.length() < pass_minlen) {
      sprintf(buf, "%d", pass_minlen);
      error += "<li>SSID <code>" + ssid + "</code>: password is too short (min " + buf + " chars)</li>";
    }
    newmap[ssid] = pass;
    if (i == pos_autostart)
      ssid_autostart = ssid;
  }

  if (error == "") {
    // save new map:
    param->m_map.clear();
    param->m_map = std::move(newmap);
    param->Save();

    // set new autostart ssid:
    if (ssid_autostart != "")
      MyConfig.SetParamValue("auto", "wifi.ssid." + prefix, ssid_autostart);
  }
}


/**
 * HandleCfgAutoInit: configure auto init (URL /cfg/autostart)
 */
void OvmsWebServer::HandleCfgAutoInit(PageEntry_t& p, PageContext_t& c)
{
  std::string error, warn;
  bool init, ext12v, modem, server_v2, server_v3;
#ifdef CONFIG_OVMS_COMP_RE_TOOLS
  bool dbc;
#endif
  std::string vehicle_type, obd2ecu, wifi_mode, wifi_ssid_client, wifi_ssid_ap;

  if (c.method == "POST") {
    // process form submission:
    init = (c.getvar("init") == "yes");
#ifdef CONFIG_OVMS_COMP_RE_TOOLS
    dbc = (c.getvar("dbc") == "yes");
#endif
    ext12v = (c.getvar("ext12v") == "yes");
    modem = (c.getvar("modem") == "yes");
    server_v2 = (c.getvar("server_v2") == "yes");
    server_v3 = (c.getvar("server_v3") == "yes");
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
        if (wifi_mode == "apclient") {
          error += "<li data-input=\"wifi_ssid_client\">Wifi client scan mode not supported for AP+Client!</li>";
        }
        else {
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
      }
      else if (MyConfig.GetParamValue("wifi.ssid", wifi_ssid_client).empty()) {
        error += "<li data-input=\"wifi_ssid_client\">Wifi client mode invalid: no password set for SSID!</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValueBool("auto", "init", init);
#ifdef CONFIG_OVMS_COMP_RE_TOOLS
      MyConfig.SetParamValueBool("auto", "dbc", dbc);
#endif
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
#ifdef CONFIG_OVMS_COMP_RE_TOOLS
    dbc = MyConfig.GetParamValueBool("auto", "dbc", false);
#endif
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

    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Auto start configuration");
  c.form_start(p.uri);

  c.input_checkbox("Enable auto start", "init", init,
    "<p>Note: if a crash occurs within 10 seconds after powering the module, autostart will be temporarily"
    " disabled. You may need to use the USB shell to access the module and fix the config.</p>");

#ifdef CONFIG_OVMS_COMP_RE_TOOLS
  c.input_checkbox("Autoload DBC files", "dbc", dbc,
    "<p>Enable to autoload DBC files (for reverse engineering).</p>");
#endif

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


#ifdef CONFIG_OVMS_COMP_OTA
/**
 * HandleCfgFirmware: OTA firmware update & boot setup (URL /cfg/firmware)
 */
void OvmsWebServer::HandleCfgFirmware(PageEntry_t& p, PageContext_t& c)
{
  std::string cmdres, mru;
  std::string action;
  ota_info info;
  bool auto_enable;
  std::string auto_hour, server, tag;
  std::string output;
  std::string version;
  const char *what;
  char buf[132];

  if (c.method == "POST") {
    // process form submission:
    bool error = false, showform = true, reboot = false;
    action = c.getvar("action");

    auto_enable = (c.getvar("auto_enable") == "yes");
    auto_hour = c.getvar("auto_hour");
    server = c.getvar("server");
    tag = c.getvar("tag");

    if (action.substr(0,3) == "set") {

      if (startsWith(server, "https:")) {
        error = true;
        output += "<p>Sorry, https not yet supported!</p>";
      }

      info.partition_boot = c.getvar("boot_old");
      std::string partition_boot = c.getvar("boot");
      if (partition_boot != info.partition_boot) {
        cmdres = ExecuteCommand("ota boot " + partition_boot);
        if (cmdres.find("Error:") != std::string::npos)
          error = true;
        output += "<p><samp>" + cmdres + "</samp></p>";
      }
      else {
        output += "<p>Boot partition unchanged.</p>";
      }

      if (!error) {
        MyConfig.SetParamValueBool("auto", "ota", auto_enable);
        MyConfig.SetParamValue("ota", "auto.hour", auto_hour);
        MyConfig.SetParamValue("ota", "server", server);
        MyConfig.SetParamValue("ota", "tag", tag);
      }

      if (!error && action == "set-reboot")
        reboot = true;
    }
    else if (action == "reboot") {
      reboot = true;
    }
    else {
      error = true;
      output = "<p>Unknown action.</p>";
    }

    // output result:
    if (error) {
      output = "<p class=\"lead\">Error!</p>" + output;
      c.head(400);
      c.alert("danger", output.c_str());
    }
    else {
      c.head(200);
      output = "<p class=\"lead\">OK!</p>" + output;
      c.alert("success", output.c_str());
      if (reboot)
        OutputReboot(p, c);
      if (reboot || !showform) {
        c.done();
        return;
      }
    }
  }
  else {
    // read config:
    auto_enable = MyConfig.GetParamValueBool("auto", "ota", true);
    auto_hour = MyConfig.GetParamValue("ota", "auto.hour", "2");
    server = MyConfig.GetParamValue("ota", "server");
    tag = MyConfig.GetParamValue("ota", "tag");

    // generate form:
    c.head(200);
  }

  // read status:
  MyOTA.GetStatus(info);

  c.panel_start("primary", "Firmware setup &amp; update");

  c.input_info("Firmware version", info.version_firmware.c_str());
  output = info.version_server;
  output.append(" <button type=\"button\" class=\"btn btn-default\" data-toggle=\"modal\" data-target=\"#version-dialog\">Version info</button>");
  c.input_info("…available", output.c_str());

  c.print(
    "<ul class=\"nav nav-tabs\">"
      "<li class=\"active\"><a data-toggle=\"tab\" href=\"#tab-setup\">Setup</a></li>"
      "<li><a data-toggle=\"tab\" href=\"#tab-flash-http\">Flash <span class=\"hidden-xs\">from</span> web</a></li>"
      "<li><a data-toggle=\"tab\" href=\"#tab-flash-vfs\">Flash <span class=\"hidden-xs\">from</span> file</a></li>"
    "</ul>"
    "<div class=\"tab-content\">"
      "<div id=\"tab-setup\" class=\"tab-pane fade in active section-setup\">");

  c.form_start(p.uri);

  // Boot partition:
  c.input_info("Running partition", info.partition_running.c_str());
  c.printf("<input type=\"hidden\" name=\"boot_old\" value=\"%s\">", _attr(info.partition_boot));
  c.input_select_start("Boot from", "boot");
  what = "Factory image";
  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_FACTORY);
  if (version != "") {
    snprintf(buf, sizeof(buf), "%s (%s)", what, version.c_str());
    what = buf;
  }
  c.input_select_option(what, "factory", (info.partition_boot == "factory"));
  what = "OTA_0 image";
  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_OTA_0);
  if (version != "") {
    snprintf(buf, sizeof(buf), "%s (%s)", what, version.c_str());
    what = buf;
  }
  c.input_select_option(what, "ota_0", (info.partition_boot == "ota_0"));
  what = "OTA_1 image";
  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_OTA_1);
  if (version != "") {
    snprintf(buf, sizeof(buf), "%s (%s)", what, version.c_str());
    what = buf;
  }
  c.input_select_option(what, "ota_1", (info.partition_boot == "ota_1"));
  c.input_select_end();

  // Server & auto update:
  c.print("<hr>");
  c.input_checkbox("Enable auto update", "auto_enable", auto_enable,
    "<p>Strongly recommended: if enabled, the module will perform automatic firmware updates within the hour of day specified, but only if a wifi network is available.</p>");
  c.input("number", "Auto update hour of day", "auto_hour", auto_hour.c_str(), "0-23, default: 2", NULL, "min=\"0\" max=\"23\" step=\"1\"");
  c.print(
    "<datalist id=\"server-list\">"
      "<option value=\"api.openvehicles.com/firmware/ota\">"
      "<option value=\"ovms.dexters-web.de/firmware/ota\">"
    "</datalist>"
    "<datalist id=\"tag-list\">"
      "<option value=\"main\">"
      "<option value=\"eap\">"
      "<option value=\"edge\">"
    "</datalist>"
    );
  c.input_text("Update server", "server", server.c_str(), "Specify or select from list (clear to see all options)",
    "<p>Default is <code>api.openvehicles.com/firmware/ota</code>. Note: currently only http is supported.</p>",
    "list=\"server-list\"");
  c.input_text("Version tag", "tag", tag.c_str(), "Specify or select from list (clear to see all options)",
    "<p>Default is <code>main</code> for standard releases. Use <code>eap</code> (early access program) for stable or <code>edge</code> for bleeding edge developer builds.</p>",
    "list=\"tag-list\"");

  c.print(
    "<hr>"
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"set\">Set</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"set-reboot\">Set &amp; reboot</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"reboot\">Reboot</button> "
      "</div>"
    "</div>");
  c.form_end();

  c.print(
      "</div>"
      "<div id=\"tab-flash-http\" class=\"tab-pane fade section-flash\">");

  // warn about modem / AP connection:
  if (netif_default) {
    if (netif_default->name[0] == 'a' && netif_default->name[1] == 'p') {
      c.alert("warning",
        "<p class=\"lead\"><strong>No internet access.</strong></p>"
        "<p>The module is running in wifi AP mode without modem, so flashing from a public server is currently not possible.</p>"
        "<p>You can still flash from an AP network local IP address (<code>192.168.4.x</code>).</p>");
    }
    else if (netif_default->name[0] == 'p' && netif_default->name[1] == 'p') {
      c.alert("warning",
        "<p class=\"lead\"><strong>Using modem connection for internet.</strong></p>"
        "<p>Downloads from public servers will currently be done via cellular network. Be aware update files are &gt;2 MB, "
        "which may exceed your data plan and need some time depending on your link speed.</p>"
        "<p>You can also flash locally from a wifi network IP address.</p>");
    }
  }

  c.form_start(p.uri);

  // Flash HTTP:
  mru = MyConfig.GetParamValue("ota", "http.mru");
  c.input_text("HTTP URL", "flash_http", mru.c_str(),
    "optional: URL of firmware file",
    "<p>Leave empty to download the latest firmware from the update server. "
    "Note: currently only http is supported.</p>", "list=\"urls\"");

  c.print("<datalist id=\"urls\">");
  if (mru != "")
    c.printf("<option value=\"%s\">", c.encode_html(mru).c_str());
  c.print("</datalist>");

  c.input_button("default", "Flash now", "action", "flash-http");
  c.form_end();

  c.print(
      "</div>"
      "<div id=\"tab-flash-vfs\" class=\"tab-pane fade section-flash\">");

  c.form_start(p.uri);

  // Flash VFS:
  mru = MyConfig.GetParamValue("ota", "vfs.mru");
  c.input_info("Auto flash",
    "<ol>"
      "<li>Place the file <code>ovms3.bin</code> in the SD root directory.</li>"
      "<li>Insert the SD card, wait until the module reboots.</li>"
      "<li>Note: after processing the file will be renamed to <code>ovms3.done</code>.</li>"
    "</ol>");
  c.input_info("Upload",
    "Not yet implemented. Please copy your update file to an SD card and enter the path below.");

  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-flash_vfs\">File path:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<div class=\"input-group\">\n"
          "<input type=\"text\" class=\"form-control\" placeholder=\"Path to firmware file\" name=\"flash_vfs\" id=\"input-flash_vfs\" value=\"%s\" list=\"files\">\n"
          "<div class=\"input-group-btn\">\n"
            "<button type=\"button\" class=\"btn btn-default\" data-toggle=\"filedialog\" data-target=\"#select-firmware\" data-input=\"#input-flash_vfs\">Select</button>\n"
          "</div>\n"
        "</div>\n"
        "<span class=\"help-block\">\n"
          "<p>SD card root: <code>/sd/</code></p>\n"
        "</span>\n"
      "</div>\n"
    "</div>\n"
    , mru.c_str());

  c.print("<datalist id=\"files\">");
  if (mru != "")
    c.printf("<option value=\"%s\">", c.encode_html(mru).c_str());
  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir("/sd")) != NULL) {
    while ((dp = readdir(dir)) != NULL) {
      if (strstr(dp->d_name, ".bin") || strstr(dp->d_name, ".done"))
        c.printf("<option value=\"/sd/%s\">", c.encode_html(dp->d_name).c_str());
    }
    closedir(dir);
  }
  c.print("</datalist>");

  c.input_button("default", "Flash now", "action", "flash-vfs");
  c.form_end();

  c.print(
      "</div>"
    "</div>");

  c.panel_end(
    "<p>The module can store up to three firmware images in a factory and two OTA partitions.</p>"
    "<p>Flashing from web or file writes alternating to the OTA partitions, the factory partition remains unchanged.</p>"
    "<p>You can flash the factory partition via USB, see developer manual for details.</p>");

  c.printf(
    "<div class=\"modal fade\" id=\"version-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
      "<div class=\"modal-dialog modal-lg\">"
        "<div class=\"modal-content\">"
          "<div class=\"modal-header\">"
            "<button type=\"button\" class=\"close\" data-dismiss=\"modal\">&times;</button>"
            "<h4 class=\"modal-title\">Version info %s</h4>"
          "</div>"
          "<div class=\"modal-body\">"
            "<pre>%s</pre>"
          "</div>"
          "<div class=\"modal-footer\">"
            "<button type=\"button\" class=\"btn btn-default\" data-dismiss=\"modal\">Close</button>"
          "</div>"
        "</div>"
      "</div>"
    "</div>"
    , _html(info.version_server)
    , _html(info.changelog_server));

  c.print(
    "<div class=\"modal fade\" id=\"flash-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
      "<div class=\"modal-dialog modal-lg\">"
        "<div class=\"modal-content\">"
          "<div class=\"modal-header\">"
            "<button type=\"button\" class=\"close\" data-dismiss=\"modal\">&times;</button>"
            "<h4 class=\"modal-title\">Flashing…</h4>"
          "</div>"
          "<div class=\"modal-body\">"
            "<pre id=\"output\"></pre>"
          "</div>"
          "<div class=\"modal-footer\">"
            "<button type=\"button\" class=\"btn btn-default action-reboot\">Reboot now</button>"
            "<button type=\"button\" class=\"btn btn-default action-close\">Close</button>"
          "</div>"
        "</div>"
      "</div>"
    "</div>"
    "\n"
    "<div class=\"filedialog\" id=\"select-firmware\" data-options='{\n"
      "\"title\": \"Select firmware file\",\n"
      "\"path\": \"/sd/\",\n"
      "\"quicknav\": [\"/sd/\", \"/sd/firmware/\"],\n"
      "\"filter\": \"\\\\.(bin|done)$\",\n"
      "\"sortBy\": \"date\",\n"
      "\"sortDir\": -1,\n"
      "\"showNewDir\": false\n"
    "}' />\n"
    "\n"
    "<script>\n"
      "function setloading(sel, on){"
        "$(sel+\" button\").prop(\"disabled\", on);"
        "if (on) $(sel).addClass(\"loading\");"
        "else $(sel).removeClass(\"loading\");"
      "}"
      "$(\".section-flash button\").on(\"click\", function(ev){"
        "var action = $(this).attr(\"value\");"
        "if (!action) return;"
        "$(\"#output\").text(\"Processing… (do not interrupt, may take some minutes)\\n\");"
        "setloading(\"#flash-dialog\", true);"
        "$(\"#flash-dialog\").modal(\"show\");"
        "if (action == \"flash-http\") {"
          "var flash_http = $(\"input[name=flash_http]\").val();"
          "loadcmd(\"ota flash http \" + flash_http, \"+#output\").done(function(resp){"
            "setloading(\"#flash-dialog\", false);"
          "});"
        "}"
        "else if (action == \"flash-vfs\") {"
          "var flash_vfs = $(\"input[name=flash_vfs]\").val();"
          "loadcmd(\"ota flash vfs \" + flash_vfs, \"+#output\").done(function(resp){"
            "setloading(\"#flash-dialog\", false);"
          "});"
        "}"
        "else {"
          "$(\"#output\").text(\"Unknown action.\");"
          "setloading(\"#flash-dialog\", false);"
        "}"
        "ev.stopPropagation();"
        "return false;"
      "});"
      "$(\".action-reboot\").on(\"click\", function(ev){"
        "$(\"#flash-dialog\").removeClass(\"fade\").modal(\"hide\");"
        "loaduri(\"#main\", \"post\", \"/cfg/firmware\", { \"action\": \"reboot\" });"
      "});"
      "$(\".action-close\").on(\"click\", function(ev){"
        "$(\"#flash-dialog\").removeClass(\"fade\").modal(\"hide\");"
        "loaduri(\"#main\", \"get\", \"/cfg/firmware\");"
      "});"
    "</script>");

  c.done();
}
#endif


/**
 * HandleCfgLogging: configure logging (URL /cfg/logging)
 */
void OvmsWebServer::HandleCfgLogging(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  OvmsConfigParam* param = MyConfig.CachedParam("log");
  ConfigParamMap pmap;
  int i, max;
  char buf[100];
  std::string file_path, tag, level;

  if (c.method == "POST") {
    // process form submission:

    pmap["file.enable"] = (c.getvar("file_enable") == "yes") ? "yes" : "no";
    if (c.getvar("file_maxsize") != "")
      pmap["file.maxsize"] = c.getvar("file_maxsize");
    if (c.getvar("file_keepdays") != "")
      pmap["file.keepdays"] = c.getvar("file_keepdays");

    file_path = c.getvar("file_path");
    pmap["file.path"] = file_path;
    if (pmap["file.enable"] == "yes" && !startsWith(file_path, "/sd/") && !startsWith(file_path, "/store/"))
      error += "<li data-input=\"file_path\">File must be on '/sd' or '/store'</li>";

    pmap["level"] = c.getvar("level");
    max = atoi(c.getvar("levelmax").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "tag_%d", i);
      tag = c.getvar(buf);
      if (tag == "") continue;
      sprintf(buf, "level_%d", i);
      level = c.getvar(buf);
      if (level == "") continue;
      snprintf(buf, sizeof(buf), "level.%s", tag.c_str());
      pmap[buf] = level;
    }

    if (error == "") {
      // save:
      param->m_map.clear();
      param->m_map = std::move(pmap);
      param->Save();

      c.head(200);
      c.alert("success", "<p class=\"lead\">Logging configuration saved.</p>");
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
    pmap = param->m_map;

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Logging configuration");
  c.form_start(p.uri);

  c.input_checkbox("Enable file logging", "file_enable", pmap["file.enable"] == "yes");
  c.input_text("Log file path", "file_path", pmap["file.path"].c_str(), "Enter path on /sd or /store",
    "<p>Logging to SD card will start automatically on mount. Do not remove the SD card while logging is active.</p>"
    "<p><button type=\"button\" class=\"btn btn-default\" data-target=\"#log-close-result\" data-cmd=\"log close\">Close log file for SD removal</button>"
      "<samp id=\"log-close-result\" class=\"samp-inline\"></samp></p>");

  std::string docroot = MyConfig.GetParamValue("http.server", "docroot", "/sd");
  std::string download;
  if (startsWith(pmap["file.path"], docroot)) {
    std::string webpath = pmap["file.path"].substr(docroot.length());
    download = "<a class=\"btn btn-default\" target=\"_blank\" href=\"" + webpath + "\">Open log file</a>";
    auto p = webpath.find_last_of('/');
    if (p != std::string::npos) {
      std::string webdir = webpath.substr(0, p);
      if (webdir != docroot)
        download += " <a class=\"btn btn-default\" target=\"_blank\" href=\"" + webdir + "/\">Open directory</a>";
    }
  } else {
    download = "You can access your logs with the browser if the path is in your webserver root (" + docroot + ").";
  }
  c.input_info("Download", download.c_str());

  c.input("number", "Max file size", "file_maxsize", pmap["file.maxsize"].c_str(), "Default: 1024",
    "<p>When exceeding the size, the log will be archived suffixed with date &amp; time and a new file will be started. 0 = disable</p>",
    "min=\"0\" step=\"1\"", "kB");
  c.input("number", "Expire time", "file_keepdays", pmap["file.keepdays"].c_str(), "Default: 30",
    "<p>Automatically delete archived log files. 0 = disable</p>",
    "min=\"0\" step=\"1\"", "days");

  auto gen_options = [&c](std::string level) {
    c.printf(
        "<option value=\"none\" %s>No logging</option>"
        "<option value=\"error\" %s>Errors</option>"
        "<option value=\"warn\" %s>Warnings</option>"
        "<option value=\"info\" %s>Info (default)</option>"
        "<option value=\"debug\" %s>Debug</option>"
        "<option value=\"verbose\" %s>Verbose</option>"
      , (level=="none") ? "selected" : ""
      , (level=="error") ? "selected" : ""
      , (level=="warn") ? "selected" : ""
      , (level=="info"||level=="") ? "selected" : ""
      , (level=="debug") ? "selected" : ""
      , (level=="verbose") ? "selected" : "");
  };

  c.input_select_start("Default level", "level");
  gen_options(pmap["level"]);
  c.input_select_end();

  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Component levels:</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<thead>"
            "<tr>"
              "<th width=\"10%\"></th>"
              "<th width=\"45%\">Component</th>"
              "<th width=\"45%\">Level</th>"
            "</tr>"
          "</thead>"
          "<tbody>");

  max = 0;
  for (auto &kv: pmap) {
    if (!startsWith(kv.first, "level."))
      continue;
    max++;
    tag = kv.first.substr(6);
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"tag_%d\" value=\"%s\" placeholder=\"Enter component tag\""
              " autocomplete=\"section-logging-tag\"></td>"
            "<td><select class=\"form-control\" name=\"level_%d\" size=\"1\">"
      , max, _attr(tag)
      , max);
    gen_options(kv.second);
    c.print(
            "</select></td>"
          "</tr>");
  }

  c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
      "<input type=\"hidden\" name=\"levelmax\" value=\"%d\">"
    "</div>"
    "</div>"
    "</div>"
    , max);

  c.input_button("default", "Save");
  c.form_end();

  c.print(
    "<script>"
    "function delRow(el){"
      "$(el).parent().parent().remove();"
    "}"
    "function addRow(el){"
      "var counter = $('input[name=levelmax]');"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"tag_' + nr + '\" placeholder=\"Enter component tag\""
              " autocomplete=\"section-logging-tag\"></td>"
            "<td><select class=\"form-control\" name=\"level_' + nr + '\" size=\"1\">"
              "<option value=\"none\">No logging</option>"
              "<option value=\"error\">Errors</option>"
              "<option value=\"warn\">Warnings</option>"
              "<option value=\"info\" selected>Info (default)</option>"
              "<option value=\"debug\">Debug</option>"
              "<option value=\"verbose\">Verbose</option>"
            "</select></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "</script>");

  c.panel_end();
  c.done();
}


/**
 * HandleCfgLocations: configure GPS locations (URL /cfg/locations)
 */
void OvmsWebServer::HandleCfgLocations(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  OvmsConfigParam* param = MyConfig.CachedParam("locations");
  ConfigParamMap pmap;
  int i, max;
  char buf[100];
  std::string latlon, name;
  int radius;
  double lat, lon;

  if (c.method == "POST") {
    // process form submission:
    max = atoi(c.getvar("loc").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "latlon_%d", i);
      latlon = c.getvar(buf);
      if (latlon == "") continue;
      lat = lon = 0;
      sscanf(latlon.c_str(), "%lf,%lf", &lat, &lon);
      if (lat == 0 || lon == 0)
        error += "<li data-input=\"" + std::string(buf) + "\">Invalid coordinates (enter latitude,longitude)</li>";
      sprintf(buf, "radius_%d", i);
      radius = atoi(c.getvar(buf).c_str());
      if (radius == 0) radius = 100;
      sprintf(buf, "name_%d", i);
      name = c.getvar(buf);
      if (name == "")
        error += "<li data-input=\"" + std::string(buf) + "\">Name must not be empty</li>";
      snprintf(buf, sizeof(buf), "%f,%f,%d", lat, lon, radius);
      pmap[name] = buf;
    }

    if (error == "") {
      // save:
      param->m_map.clear();
      param->m_map = std::move(pmap);
      param->Save();

      c.head(200);
      c.alert("success", "<p class=\"lead\">Locations saved.</p>");
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
    pmap = param->m_map;

    // generate form:
    c.head(200);
  }

  c.print(
    "<style>\n"
    ".list-editor > table {\n"
      "border-bottom: 1px solid #ddd;\n"
    "}\n"
    "</style>\n"
    "\n");

  c.panel_start("primary panel-single", "Locations");
  c.form_start(p.uri);

  c.print(
    "<div class=\"table-responsive list-editor receiver\" id=\"loced\">"
      "<table class=\"table form-table\">"
        "<colgroup>"
          "<col style=\"width:10%\">"
          "<col style=\"width:90%\">"
        "</colgroup>"
        "<template>"
          "<tr class=\"list-item\">"
            "<td><button type=\"button\" class=\"btn btn-danger list-item-del\"><strong>✖</strong></button></td>"
            "<td>"
              "<div class=\"form-group\">"
                "<label class=\"control-label col-sm-2\" for=\"input-latlon_ITEM_ID\">Center:</label>"
                "<div class=\"col-sm-10\">"
                  "<div class=\"input-group\">"
                    "<input type=\"text\" class=\"form-control\" placeholder=\"Latitude,longitude (decimal)\" name=\"latlon_ITEM_ID\" id=\"input-latlon_ITEM_ID\" value=\"ITEM_latlon\">"
                    "<div class=\"input-group-btn\">"
                      "<button type=\"button\" class=\"btn btn-default open-gm\" title=\"Google Maps (new tab)\">GM</button>"
                      "<button type=\"button\" class=\"btn btn-default open-osm\" title=\"OpenStreetMaps (new tab)\">OSM</button>"
                    "</div>"
                  "</div>"
                "</div>"
              "</div>"
              "<div class=\"form-group\">"
                "<label class=\"control-label col-sm-2\" for=\"input-radius_ITEM_ID\">Radius:</label>"
                "<div class=\"col-sm-10\">"
                  "<div class=\"input-group\">"
                    "<input type=\"number\" class=\"form-control\" placeholder=\"Enter radius (m)\" name=\"radius_ITEM_ID\" id=\"input-radius_ITEM_ID\" value=\"ITEM_radius\" min=\"1\" step=\"1\">"
                    "<div class=\"input-group-addon\">m</div>"
                  "</div>"
                "</div>"
              "</div>"
              "<div class=\"form-group\">"
                "<label class=\"control-label col-sm-2\" for=\"input-name_ITEM_ID\">Name:</label>"
                "<div class=\"col-sm-10\">"
                  "<input type=\"text\" class=\"form-control\" placeholder=\"Enter name\" autocomplete=\"name\" name=\"name_ITEM_ID\" id=\"input-name_ITEM_ID\" value=\"ITEM_name\">"
                "</div>"
              "</div>"
            "</td>"
          "</tr>"
        "</template>"
        "<tbody class=\"list-items\">"
        "</tbody>"
        "<tfoot>"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success list-item-add\"><strong>✚</strong></button></td>"
            "<td></td>"
          "</tr>"
        "</tfoot>"
      "</table>"
      "<input type=\"hidden\" class=\"list-item-id\" name=\"loc\" value=\"0\">"
    "</div>"
    "<button type=\"submit\" class=\"btn btn-default center-block\">Save</button>\n"
  );

  c.form_end();

  c.print(
    "<script>"
    "$('#loced').listEditor();"
    "$('#loced').on('click', 'button.open-gm', function(evt) {"
      "var latlon = $(this).parent().prev().val();"
      "window.open('https://www.google.de/maps/search/?api=1&query='+latlon, 'window-gm');"
    "});"
    "$('#loced').on('click', 'button.open-osm', function(evt) {"
      "var latlon = $(this).parent().prev().val();"
      "window.open('https://www.openstreetmap.org/search?query='+latlon, 'window-osm');"
    "});"
    "$('#loced').on('msg:metrics', function(evt, update) {"
      "if ('v.p.latitude' in update || 'v.p.longitude' in update) {"
        "var preset = { latlon: metrics['v.p.latitude']+','+metrics['v.p.longitude'], radius: 100 };"
        "$('#loced button.list-item-add').data('preset', JSON.stringify(preset));"
      "}"
    "}).trigger('msg:metrics', metrics);");

  for (auto &kv: pmap) {
    lat = lon = 0;
    radius = 100;
    name = kv.first;
    sscanf(kv.second.c_str(), "%lf,%lf,%d", &lat, &lon, &radius);
    c.printf(
      "$('#loced').listEditor('addItem', { name: '%s', latlon: '%lf,%lf', radius: %d });"
      , json_encode(name).c_str(), lat, lon, radius);
  }

  c.print("</script>");

  c.panel_end();
  c.done();
}


/**
 * HandleCfgBackup: config backup/restore (URL /cfg/backup)
 */
void OvmsWebServer::HandleCfgBackup(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  c.print(
    "<style>\n"
    "#backupbrowser tbody { height: 186px; }\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\"><span class=\"hidden-xs\">Configuration</span> Backup &amp; Restore</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"filebrowser\" id=\"backupbrowser\" />\n"
        "<div class=\"action-menu text-right\">\n"
          "<button type=\"button\" class=\"btn btn-default pull-left\" id=\"action-newdir\">New dir</button>\n"
          "<button type=\"button\" class=\"btn btn-primary\" id=\"action-backup\" disabled>Create backup</button>\n"
          "<button type=\"button\" class=\"btn btn-primary\" id=\"action-restore\" disabled>Restore backup</button>\n"
        "</div>\n"
        "<pre id=\"log\" style=\"margin-top:15px\"/>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<p>Use this tool to create or restore backups of your system configuration &amp; scripts.\n"
          "User files or directories in <code>/store</code> will not be included or restored.\n"
          "ZIP files are password protected (hint: use 7z to unzip/create on a PC).</p>\n"
        "<p>Note: the module will perform a reboot after successful restore.</p>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<script>\n"
    "(function(){\n"
      "var suggest = '/sd/backup/cfg-' + new Date().toISOString().substr(0,10) + '.zip';\n"
      "var zip = { exists: false, iszip: false };\n"
      "var $panel = $('.panel-body');\n"
    "\n"
      "function updateButtons(enable) {\n"
        "if (enable === false) {\n"
          "$panel.addClass(\"loading disabled\");\n"
          "return;\n"
        "}\n"
        "$panel.removeClass(\"loading disabled\");\n"
        "if (!zip.iszip) {\n"
          "$('#action-backup').prop('disabled', true);\n"
          "$('#action-restore').prop('disabled', true);\n"
        "} else if (zip.exists) {\n"
          "$('#action-backup').prop('disabled', false).text(\"Overwrite backup\");\n"
          "$('#action-restore').prop('disabled', false);\n"
        "} else {\n"
          "$('#action-backup').prop('disabled', false).text(\"Create backup\");\n"
          "$('#action-restore').prop('disabled', true);\n"
        "}\n"
      "};\n"
    "\n"
      "$('#backupbrowser').filebrowser({\n"
        "input: zip,\n"
        "path: suggest,\n"
        "quicknav: ['/sd/', '/sd/backup/', suggest],\n"
        "filter: function(f) { return f.isdir || f.name.match('\\\\.zip$'); },\n"
        "sortBy: 'date',\n"
        "sortDir: -1,\n"
        "onPathChange: function(input) {\n"
          "input.iszip = (input.file.match('\\\\.zip$') != null);\n"
          "if (!input.iszip) {\n"
            "updateButtons();\n"
            "$('#log').html('<div class=\"bg-warning\">Please select/enter a zip file</div>');\n"
          "} else {\n"
            "loadcmd(\"vfs stat \" + input.path).always(function(data) {\n"
              "if (typeof data != \"string\" || data.startsWith(\"Error\")) {\n"
                "input.exists = false;\n"
                "$('#log').empty();\n"
              "} else {\n"
                "input.exists = true;\n"
                "input.url = getPathURL(input.path);\n"
                "$('#log').text(data).append(input.url ? '\\n<a href=\"'+input.url+'\">Download '+input.file+'</a>' : '');\n"
              "}\n"
              "updateButtons();\n"
            "});\n"
          "}\n"
        "},\n"
      "});\n"
    "\n"
      "$('#action-newdir').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('newDir');\n"
      "});\n"
    "\n"
      "$('#action-backup').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('stopLoad');\n"
        "promptdialog(\"password\", \"Create backup\", \"ZIP password / empty = use module password\", [\"Cancel\", \"Create backup\"], function(ok, password) {\n"
          "if (ok) {\n"
            "updateButtons(false);\n"
            "loadcmd(\"config backup \" + zip.path + ((password) ? \" \" + password : \"\"), \"#log\").done(function(output) {\n"
              "if (output.indexOf(\"Error\") < 0) {\n"
                "zip.exists = true;\n"
                "zip.url = getPathURL(zip.path);\n"
                "$('#log').append(zip.url ? '\\n<a href=\"'+zip.url+'\">Download '+zip.file+'</a>' : '');\n"
                "$('#backupbrowser').filebrowser('loadDir');\n"
              "}\n"
            "}).always(function() {\n"
              "updateButtons();\n"
            "});\n"
          "}\n"
        "});\n"
      "});\n"
    "\n"
      "$('#action-restore').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('stopLoad');\n"
        "promptdialog(\"password\", \"Restore backup\", \"ZIP password / empty = use module password\", [\"Cancel\", \"Restore backup\"], function(ok, password) {\n"
          "if (ok) {\n"
            "var rebooting = false;\n"
            "updateButtons(false);\n"
            "loadcmd(\"config restore \" + zip.path + ((password) ? \" \" + password : \"\"), \"#log\", function(msg) {\n"
              "if (msg.text && msg.text.indexOf(\"rebooting\") >= 0) {\n"
                "rebooting = true;\n"
                "msg.request.abort();\n"
              "}\n"
              "return rebooting ? null : standardTextFilter(msg);\n"
            "}).always(function(data, status) {\n"
              "updateButtons();\n"
              "if (rebooting) $('#log').reconnectTicker();\n"
            "});\n"
          "}\n"
        "});\n"
      "});\n"
    "\n"
    "})();\n"
    "</script>\n"
  );
  c.done();
}

/**
 * HandleCfgPlugins: configure/edit web plugins (URL /cfg/plugins)
 */

static void OutputPluginList(PageEntry_t& p, PageContext_t& c)
{
  c.print(
    "<style>\n"
    ".list-editor > table {\n"
      "border-bottom: 1px solid #ddd;\n"
    "}\n"
    ".list-input .form-control,\n"
    ".list-input > .btn-group,\n"
    ".list-input > button {\n"
      "margin-right: 20px;\n"
      "margin-bottom: 5px;\n"
    "}\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Webserver Plugins</div>\n"
      "<div class=\"panel-body\">\n"
        "<form class=\"form-inline\" method=\"post\" action=\"/cfg/plugins\" target=\"#main\">\n"
        "<div class=\"list-editor\" id=\"pluginlist\">\n"
          "<table class=\"table form-table\">\n"
            "<colgroup>\n"
              "<col style=\"width:10%\">\n"
              "<col style=\"width:90%\">\n"
            "</colgroup>\n"
            "<template>\n"
              "<tr class=\"list-item mode-ITEM_MODE\">\n"
                "<td><button type=\"button\" class=\"btn btn-danger list-item-del\"><strong>✖</strong></button></td>\n"
                "<td class=\"list-input\">\n"
                  "<input type=\"hidden\" name=\"mode_ITEM_ID\" value=\"ITEM_MODE\">\n"
                  "<select class=\"form-control list-disabled\" size=\"1\" name=\"type_ITEM_ID\">\n"
                    "<option value=\"page\" data-value=\"ITEM_type\">Page</option>\n"
                    "<option value=\"hook\" data-value=\"ITEM_type\">Hook</option>\n"
                  "</select>\n"
                  "<span><input type=\"text\" required pattern=\"^[a-zA-Z0-9._-]+$\" class=\"form-control font-monospace list-disabled\" placeholder=\"Unique name (letters: a-z/A-Z/0-9/./_/-)\"  title=\"Letters: a-z/A-Z/0-9/./_/-\" name=\"key_ITEM_ID\" id=\"input-key_ITEM_ID\" value=\"ITEM_key\"></span>\n"
                  "<div class=\"btn-group\" data-toggle=\"buttons\">\n"
                    "<label class=\"btn btn-default\">\n"
                      "<input type=\"radio\" name=\"enable_ITEM_ID\" value=\"no\" data-value=\"ITEM_enable\"> OFF\n"
                    "</label>\n"
                    "<label class=\"btn btn-default\">\n"
                      "<input type=\"radio\" name=\"enable_ITEM_ID\" value=\"yes\" data-value=\"ITEM_enable\"> ON\n"
                    "</label>\n"
                  "</div>\n"
                  "<button type=\"button\" class=\"btn btn-default action-edit add-disabled\">Edit</button>\n"
                "</td>\n"
              "</tr>\n"
            "</template>\n"
            "<tbody class=\"list-items\">\n"
            "</tbody>\n"
            "<tfoot>\n"
              "<tr>\n"
                "<td><button type=\"button\" class=\"btn btn-success list-item-add\" data-preset='{ \"type\":\"page\", \"enable\":\"yes\" }'><strong>✚</strong></button></td>\n"
                "<td></td>\n"
              "</tr>\n"
            "</tfoot>\n"
          "</table>\n"
          "<input type=\"hidden\" class=\"list-item-id\" name=\"cnt\" value=\"0\">\n"
        "</div>\n"
        "<div class=\"text-center\">\n"
          "<button type=\"submit\" class=\"btn btn-primary\">Save</button>\n"
        "</div>\n"
        "</form>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<p>You can extend your OVMS web interface by using plugins. A plugin can be attached as a new page or hook into an existing page at predefined places.</p>\n"
        "<p>Plugin content is loaded from <code>/store/plugin</code> (covered by backups). Plugins currently must contain valid HTML (other mime types will be supported in the future).</p>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<script>\n"
    "$('#pluginlist').listEditor().on('click', 'button.action-edit', function(evt) {\n"
      "var $c = $(this).parent();\n"
      "var data = {\n"
        "key: $c.find('input[name^=\"key\"]').val(),\n"
        "type: $c.find('select[name^=\"type\"]').val(),\n"
      "};\n"
      "if ($('.list-item.mode-add').length == 0) {\n"
        "loaduri('#main', 'get', '/cfg/plugins', data);\n"
      "} else {\n"
        "confirmdialog(\"Discard changes?\", \"Loading the editor will discard your list changes.\", [\"Cancel\", \"OK\"], function(ok) {\n"
          "if (ok) loaduri('#main', 'get', '/cfg/plugins', data);\n"
        "});\n"
      "}\n"
    "}).on('list:validate', function(ev) {\n"
      "var valid = true;\n"
      "var keycnt = {};\n"
      "$(this).find('[name^=\"key\"]').each(function(){ keycnt[$(this).val()] = (keycnt[$(this).val()]||0)+1; });\n"
      "$(this).find('.mode-add [name^=\"key\"]').each(function() {\n"
        "if (keycnt[$(this).val()] > 1 || !$(this).val().match($(this).attr(\"pattern\"))) {\n"
          "valid = false;\n"
          "$(this).parent().addClass(\"has-error\");\n"
        "} else {\n"
          "$(this).parent().removeClass(\"has-error\");\n"
        "}\n"
      "});\n"
      "$(this).closest('form').find('button[type=submit]').prop(\"disabled\", !valid);\n"
    "});\n"
    );

  OvmsConfigParam* cp = MyConfig.CachedParam("http.plugin");
  if (cp)
  {
    const ConfigParamMap& pmap = cp->GetMap();
    std::string key, type, enable;
    
    for (auto& kv : pmap)
    {
      if (!endsWith(kv.first, ".enable"))
        continue;
      key = kv.first.substr(0, kv.first.length() - 7);
      type = cp->IsDefined(key+".hook") ? "hook" : "page";
      enable = kv.second;
      c.printf(
        "$('#pluginlist').listEditor('addItem', { key: '%s', type: '%s', enable: '%s' });\n"
        , key.c_str(), type.c_str(), enable.c_str());
    }
  }

  c.print(
    "</script>\n"
    );
}

static bool SavePluginList(PageEntry_t& p, PageContext_t& c, std::string& error)
{
  OvmsConfigParam* cp = MyConfig.CachedParam("http.plugin");
  if (!cp) {
    error += "<li>Internal error: <code>http.plugin</code> config not found</li>";
    return false;
  }

  const ConfigParamMap& pmap = cp->GetMap();
  ConfigParamMap nmap;
  std::string key, type, enable, mode;
  int cnt = atoi(c.getvar("cnt").c_str());
  char buf[20];

  for (int i = 1; i <= cnt; i++)
  {
    sprintf(buf, "_%d", i);
    key = c.getvar(std::string("key")+buf);
    mode = c.getvar(std::string("mode")+buf);
    if (key == "" || mode == "")
      continue;
    type = c.getvar(std::string("type")+buf);
    enable = c.getvar(std::string("enable")+buf);
    
    nmap[key+".enable"] = enable;
    nmap[key+".page"] = (mode=="add") ? "" : cp->GetValue(key+".page");
    if (type == "page") {
      nmap[key+".label"] = (mode=="add") ? "" : cp->GetValue(key+".label");
      nmap[key+".menu"] = (mode=="add") ? "" : cp->GetValue(key+".menu");
      nmap[key+".auth"] = (mode=="add") ? "" : cp->GetValue(key+".auth");
    } else {
      nmap[key+".hook"] = (mode=="add") ? "" : cp->GetValue(key+".hook");
    }
  }

  // deleted:
  for (auto& kv : pmap)
  {
    if (!endsWith(kv.first, ".enable"))
      continue;
    if (nmap.count(kv.first) == 0) {
      key = "/store/plugin/" + kv.first.substr(0, kv.first.length() - 7);
      unlink(key.c_str());
    }
  }

  cp->SetMap(nmap);

  return true;
}

static void OutputPluginEditor(PageEntry_t& p, PageContext_t& c)
{
  std::string key = c.getvar("key");
  std::string type = c.getvar("type");
  std::string page, hook, label, menu, auth;
  extram::string content;

  page = MyConfig.GetParamValue("http.plugin", key+".page");
  if (type == "page") {
    label = MyConfig.GetParamValue("http.plugin", key+".label");
    menu = MyConfig.GetParamValue("http.plugin", key+".menu");
    auth = MyConfig.GetParamValue("http.plugin", key+".auth");
  } else {
    hook = MyConfig.GetParamValue("http.plugin", key+".hook");
  }

  // read plugin content:
  std::string path = "/store/plugin/" + key;
  std::ifstream file(path, std::ios::ate);
  if (file.is_open()) {
    auto size = file.tellg();
    content.resize(size, '\0');
    file.seekg(0);
    file.read(&content[0], size);
  }

  c.printf(
    "<style>\n"
    ".textarea-control .btn {\n"
      "font-weight: bold;\n"
    "}\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Plugin Editor: <code>%s</code></div>\n"
      "<div class=\"panel-body\">\n"
      , _html(key));

  c.printf(
        "<form method=\"post\" action=\"/cfg/plugins\" target=\"#main\">\n"
          "<input type=\"hidden\" name=\"key\" value=\"%s\">\n"
          "<input type=\"hidden\" name=\"type\" value=\"%s\">\n"
          , _attr(key)
          , _attr(type));

  if (type == "page")
  {
    c.printf(
          "<div class=\"form-group\">\n"
            "<label class=\"control-label\" for=\"input-page\">Page:</label>\n"
            "<input type=\"text\" class=\"form-control font-monospace\" id=\"input-page\" placeholder=\"Enter page URI\" name=\"page\" value=\"%s\">\n"
            "<span class=\"help-block\">\n"
              "<p>Note: framework URIs have priority. Use prefix <code>/usr/…</code> to avoid conflicts.</p>\n"
            "</span>\n"
          "</div>\n"
          , _attr(page));
    c.printf(
          "<div class=\"form-group\">\n"
            "<label class=\"control-label\" for=\"input-label\">Label:</label>\n"
            "<input type=\"text\" class=\"form-control\" id=\"input-label\" placeholder=\"Enter menu label / page title\" name=\"label\" value=\"%s\">\n"
          "</div>\n"
          , _attr(label));
    c.printf(
          "<div class=\"row\">\n"
            "<div class=\"form-group col-xs-6\">\n"
              "<label class=\"control-menu\" for=\"input-menu\">Menu:</label>\n"
              "<select class=\"form-control\" id=\"input-menu\" size=\"1\" name=\"menu\">\n"
                "<option %s>None</option>\n"
                "<option %s>Main</option>\n"
                "<option %s>Tools</option>\n"
                "<option %s>Config</option>\n"
                "<option %s>Vehicle</option>\n"
              "</select>\n"
            "</div>\n"
          , (menu == "None") ? "selected" : ""
          , (menu == "Main") ? "selected" : ""
          , (menu == "Tools") ? "selected" : ""
          , (menu == "Config") ? "selected" : ""
          , (menu == "Vehicle") ? "selected" : "");
    c.printf(
            "<div class=\"form-group col-xs-6\">\n"
              "<label class=\"control-auth\" for=\"input-auth\">Authorization:</label>\n"
              "<select class=\"form-control\" id=\"input-auth\" size=\"1\" name=\"auth\">\n"
                "<option %s>None</option>\n"
                "<option %s>Cookie</option>\n"
                "<option %s>File</option>\n"
              "</select>\n"
            "</div>\n"
          "</div>\n"
          , (auth == "None") ? "selected" : ""
          , (auth == "Cookie") ? "selected" : ""
          , (auth == "File") ? "selected" : "");
  }
  else // type == "hook"
  {
    c.printf(
          "<div class=\"form-group\">\n"
            "<label class=\"control-label\" for=\"input-page\">Page:</label>\n"
            "<input type=\"text\" class=\"form-control font-monospace\" id=\"input-page\" placeholder=\"Enter page URI\" name=\"page\" value=\"%s\">\n"
          "</div>\n"
          , _attr(page));
    c.printf(
          "<div class=\"form-group\">\n"
            "<label class=\"control-label\" for=\"input-hook\">Hook:</label>\n"
            "<input type=\"text\" class=\"form-control font-monospace\" id=\"input-hook\" placeholder=\"Enter hook code\" name=\"hook\" value=\"%s\" list=\"hooks\">\n"
            "<datalist id=\"hooks\">\n"
              "<option value=\"body.pre\">\n"
              "<option value=\"body.post\">\n"
            "</datalist>\n"
          "</div>\n"
          , _attr(hook));
  }

  c.printf(
          "<div class=\"form-group\">\n"
            "<label class=\"control-label\" for=\"input-content\">Plugin content:</label>\n"
            "<div class=\"textarea-control pull-right\">\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-wrap\" title=\"Wrap long lines\">☇</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-smaller\">&minus;</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-larger\">&plus;</button>\n"
            "</div>\n"
            "<textarea class=\"form-control font-monospace\" style=\"max-width:100%; min-width:100%; height:300px; white-space:nowrap; font-size:13px;\" id=\"input-content\" rows=\"20\" name=\"content\">%s</textarea>\n"
          "</div>\n"
          , _html(content.c_str()));

  c.print(
          "<div class=\"text-center\">\n"
            "<button type=\"reset\" class=\"btn btn-default\">Reset</button>\n"
            "<button type=\"button\" class=\"btn btn-default action-cancel\">Cancel</button>\n"
            "<button type=\"submit\" class=\"btn btn-primary action-save\">Save</button>\n"
          "</div>\n"
        "</form>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<script>\n"
    "\n"
    "$('.tac-wrap').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "$ta.css(\"white-space\", $this.hasClass(\"active\") ? \"nowrap\" : \"pre-wrap\").focus();\n"
      "$this.toggleClass(\"active\");\n"
    "});\n"
    "$('.tac-smaller').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "var fs = parseInt($ta.css(\"font-size\"));\n"
      "$ta.css(\"font-size\", (fs-1)+\"px\").focus();\n"
    "});\n"
    "$('.tac-larger').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "var fs = parseInt($ta.css(\"font-size\"));\n"
      "$ta.css(\"font-size\", (fs+1)+\"px\").focus();\n"
    "});\n"
    "\n"
    "$('.action-cancel').on('click', function(ev) {\n"
      "loaduri('#main', 'get', '/cfg/plugins');\n"
    "});\n"
    "\n"
    "</script>\n");
}

static bool SavePluginEditor(PageEntry_t& p, PageContext_t& c, std::string& error)
{
  OvmsConfigParam* cp = MyConfig.CachedParam("http.plugin");
  if (!cp) {
    error += "<li>Internal error: <code>http.plugin</code> config not found</li>";
    return false;
  }

  ConfigParamMap nmap = cp->GetMap();
  std::string key = c.getvar("key");
  std::string type = c.getvar("type");
  extram::string content;

  nmap[key+".page"] = c.getvar("page");
  if (type == "page") {
    nmap[key+".label"] = c.getvar("label");
    nmap[key+".menu"] = c.getvar("menu");
    nmap[key+".auth"] = c.getvar("auth");
  } else {
    nmap[key+".hook"] = c.getvar("hook");
  }

  cp->SetMap(nmap);

  // write plugin content:
  c.getvar("content", content);
  mkpath("/store/plugin");
  std::string path = "/store/plugin/" + key;
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  if (file.is_open()) {
    file.write(content.data(), content.size());
  } else {
    error += "<li>Could not save plugin content to <code>" + path + "</code>!</li>";
    return false;
  }

  return true;
}

void OvmsWebServer::HandleCfgPlugins(PageEntry_t& p, PageContext_t& c)
{
  std::string key = c.getvar("key");
  std::string error, info;

  if (c.method == "POST") {
    if (key == "") {
      if (SavePluginList(p, c, error)) {
        info = "<p class=\"lead\">Plugin registration saved.</p>"
          "<script>$(\"#menu\").load(\"/menu\")</script>";
      }
    }
    else {
      if (SavePluginEditor(p, c, error)) {
        info = "<p class=\"lead\">Plugin <code>" + key + "</code> saved.</p>"
          "<script>$(\"#menu\").load(\"/menu\")</script>";
        key = "";
      }
    }
  }

  if (error != "") {
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    c.head(200);
    if (info != "")
      c.alert("success", info.c_str());
  }

  if (key == "")
    OutputPluginList(p, c);
  else
    OutputPluginEditor(p, c);

  c.done();
}
