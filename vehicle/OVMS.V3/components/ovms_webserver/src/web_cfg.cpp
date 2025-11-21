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
#ifdef CONFIG_OVMS_COMP_CELLULAR
static const char *TAG = "webserver";
#endif

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

#ifdef CONFIG_OVMS_COMP_PUSHOVER
#include "pushover.h"
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
    else {
      // "network restart", "wifi reconnect"
      OutputReconnect(p, c, NULL, cmd.c_str());
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
            "<th>GPS</th>"
            "<td>"
              "<div class=\"metric number\" data-metric=\"v.p.satcount\"><span class=\"value\">?</span><span class=\"unit\">Satellites</span></div>"
              "<div class=\"metric number\" data-metric=\"v.p.gpssq\"><span class=\"value\">?</span><span class=\"unit\">%</span></div>"
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
  c.printf("<samp class=\"monitor\" data-updcmd=\"location status\" data-events=\"gps.lock|gps.sq|location\">%s</samp>", _html(output));
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
  c.printf("<samp id=\"boot-status-cmdres\">%s</samp>", _html(output));
  c.print("<hr>");
  output = ExecuteCommand("ota status nocheck");
  c.printf("<samp>%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"reboot\">Reboot</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#boot-status-cmdres\" data-cmd=\"boot clear\nboot status\">Clear counters</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Network");
  output = ExecuteCommand("network status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"network status\" data-events=\"^network\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"network restart\">Restart network</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Wifi");
  output = ExecuteCommand("wifi status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"wifi status\" data-events=\"\\.wifi\\.\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"wifi reconnect\">Reconnect Wifi</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"wifi restart\">Restart Wifi</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" name=\"action\" value=\"wifi mode apclient\">Start AP+client mode</button></li>"
    "</ul>");

  c.print(
    "</div>"
    "<div class=\"col-sm-6 col-lg-4\">");

  c.panel_start("primary", "Cellular Modem");
  output = ExecuteCommand("cellular status");
  c.printf("<samp class=\"monitor\" data-updcmd=\"cellular status\" data-events=\"\\.modem\\.\">%s</samp>", _html(output));
  c.panel_end(
    "<ul class=\"list-inline\">"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"power cellular on\">Start modem</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"power cellular off\">Stop modem</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"cellular gps start\">Start GPS</button></li>"
      "<li><button type=\"button\" class=\"btn btn-default btn-sm\" data-target=\"#modem-cmdres\" data-cmd=\"cellular gps stop\">Stop GPS</button></li>"
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
      "if (event.startsWith(\"ticker\") || event.startsWith(\"clock\"))"
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
  std::string type = c.getvar("type");
  bool javascript = (type == "js");
  std::string output = c.getvar("output");
  extram::string command;
  c.getvar("command", command);

#ifndef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (javascript) {
    c.head(400);
    c.print("ERROR: Javascript support disabled");
    c.done();
    return;
  }
#endif

  if (!javascript && command.length() > 2000) {
    c.head(400);
    c.print("ERROR: command too long (max 2000 chars)");
    c.done();
    return;
  }

  // Note: application/octet-stream default instead of text/plain is a workaround for an *old*
  //  Chrome/Webkit bug: chunked text/plain is always buffered for the first 1024 bytes.
  if (output == "text") {
    c.head(200,
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Cache-Control: no-cache");
  } else if (output == "json") {
    c.head(200,
      "Content-Type: application/json; charset=utf-8\r\n"
      "Cache-Control: no-cache");
  } else if (output == "binary") {
    // As Safari on iOS still doesn't support responseType=ArrayBuffer on XMLHttpRequests,
    // this is meant to be used for binary data transmissions.
    // Based on Marcus Granado's approach, see…
    // https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/Sending_and_Receiving_Binary_Data
    c.head(200,
      "Content-Type: application/octet-stream; charset=x-user-defined\r\n"
      "Cache-Control: no-cache");
  } else {
    c.head(200,
      "Content-Type: application/octet-stream; charset=utf-8\r\n"
      "Cache-Control: no-cache");
  }

  if (command.empty())
    c.done();
  else
    new HttpCommandStream(c.nc, command, javascript);
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
    ".log { font-size: 87%; color: gray; }"
    ".log.log-I { color: green; }"
    ".log.log-W { color: darkorange; }"
    ".log.log-E { color: red; }"
    "</style>");

  c.panel_start("primary panel-minpad", "Shell"
    "<div class=\"pull-right checkbox\" style=\"margin: 0;\">"
      "<label><input type=\"checkbox\" id=\"logmonitor\" checked accesskey=\"L\"> <u>L</u>og Monitor</label>"
    "</div>");

  c.printf(
    "<pre class=\"receiver get-window-resize\" id=\"output\">%s</pre>"
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
    "<script>(function(){"
    "var $output = $('#output'), $command = $('#input-command');"
    "var add_output = function(addhtml) {"
      "var autoscroll = ($output.get(0).scrollTop + $output.innerHeight()) >= $output.get(0).scrollHeight;"
      "$output.append(addhtml);"
      "if (autoscroll) $output.scrollTop($output.get(0).scrollHeight);"
    "};"
    "var htmsg = \"\";"
    "for (msg of loghist)"
      "htmsg += '<div class=\"log log-'+msg[0]+'\">'+encode_html(unwrapLogLine(msg))+'</div>';"
    "$output.html(htmsg);"
    "$output.on(\"msg:log\", function(ev, msg){"
      "if (!$(\"#logmonitor\").prop(\"checked\")) return;"
      "var autoscroll = ($output.get(0).scrollTop + $output.innerHeight()) >= $output.get(0).scrollHeight;"
      "htmsg = '<div class=\"log log-'+msg[0]+'\">'+encode_html(unwrapLogLine(msg))+'</div>';"
      "if ($(\"html\").hasClass(\"loading\"))"
        "$output.find(\"strong:last-of-type\").before(htmsg);"
      "else "
        "$output.append(htmsg);"
      "if (autoscroll) $output.scrollTop($output.get(0).scrollHeight);"
    "});"
    "$output.on(\"window-resize\", function(){"
      "var $this = $(this);"
      "var pad = Number.parseInt($this.parent().css(\"padding-top\")) + Number.parseInt($this.parent().css(\"padding-bottom\"));"
      "var h = $(window).height() - $this.offset().top - pad - 81;"
      "if ($(window).width() <= 767) h += 27;"
      "if ($(\"body\").hasClass(\"fullscreened\")) h -= 4;"
      "$this.height(h);"
      "$this.scrollTop($this.get(0).scrollHeight);"
    "}).trigger(\"window-resize\");"
    "$(\"#shellform\").on(\"submit\", function(event){"
      "var command = $command.val();"
      "$output.scrollTop($output.get(0).scrollHeight);"
      "if (command && !$(\"html\").hasClass(\"loading\")) {"
        "var data = $(this).serialize();"
        "var lastlen = 0, xhr, timeouthd, timeout = 20;"
        "if (/^(test |ota |co .* scan)/.test(command)) timeout = 60;"
        "var checkabort = function(){ if (xhr.readyState != 4) xhr.abort(\"timeout\"); };"
        "xhr = $.ajax({ \"type\": \"post\", \"url\": \"/api/execute\", \"data\": data,"
          "\"timeout\": 0,"
          "\"beforeSend\": function(){"
            "$(\"html\").addClass(\"loading\");"
            "add_output(\"<strong>OVMS#</strong>&nbsp;<kbd>\""
              "+ $(\"<div/>\").text(command).html() + \"</kbd><br>\");"
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
              "add_output($(\"<div/>\").text(addtext).html());"
              "window.clearTimeout(timeouthd);"
              "timeouthd = window.setTimeout(checkabort, timeout*1000);"
            "},"
          "},"
          "\"success\": function(response, xhrerror, request){"
            "var addtext = response.substring(lastlen);"
            "add_output($(\"<div/>\").text(addtext).html());"
          "},"
          "\"error\": function(response, xhrerror, httperror){"
            "var txt;"
            "if (response.status == 401 || response.status == 403)"
              "txt = \"Session expired. <a class=\\\"btn btn-sm btn-default\\\" href=\\\"javascript:reloadpage()\\\">Login</a>\";"
            "else if (response.status >= 400)"
              "txt = \"Error \" + response.status + \" \" + response.statusText;"
            "else"
              "txt = \"Request \" + (xhrerror||\"failed\") + \", please retry\";"
            "add_output('<div class=\"bg-danger\">'+txt+'</div>');"
          "},"
        "});"
        "if (shellhist.indexOf(command) >= 0)"
          "shellhist.splice(shellhist.indexOf(command), 1);"
        "shellhpos = shellhist.push(command) - 1;"
      "}"
      "event.stopPropagation();"
      "return false;"
    "});"
    "$command.on(\"keydown\", function(ev){"
      "if (ev.key == \"ArrowUp\") {"
        "shellhpos = (shellhist.length + shellhpos - 1) % shellhist.length;"
        "$command.val(shellhist[shellhpos]);"
        "return false;"
      "}"
      "else if (ev.key == \"ArrowDown\") {"
        "shellhpos = (shellhist.length + shellhpos + 1) % shellhist.length;"
        "$command.val(shellhist[shellhpos]);"
        "return false;"
      "}"
      "else if (ev.key == \"Escape\") {"
        "shellhpos = 0;"
        "$command.val('');"
        "return false;"
      "}"
      "else if (ev.key == \"PageUp\") {"
        "$output.scrollTop($output.scrollTop() - $output.height());"
        "return false;"
      "}"
      "else if (ev.key == \"PageDown\") {"
        "$output.scrollTop($output.scrollTop() + $output.height());"
        "return false;"
      "}"
      "else if (ev.key == \"Home\") {"
        "if ($command.get(0).selectionEnd == 0) {"
          "$output.scrollTop(0);"
        "}"
      "}"
      "else if (ev.key == \"End\") {"
        "if ($command.get(0).selectionEnd == $command.get(0).value.length) {"
          "$output.scrollTop($output.get(0).scrollHeight);"
        "}"
      "}"
    "});"
    "$('#logmonitor').on('change', function(ev){ $command.focus(); });"
    "$command.val(shellhist[shellhpos]||'').focus();"
    "})()</script>");

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

#ifdef CONFIG_OVMS_COMP_CELLULAR
/**
 * HandleCfgModem: configure APN & cellular modem features & GPS (URL /cfg/modem)
 */
void OvmsWebServer::HandleCfgModem(PageEntry_t& p, PageContext_t& c)
{
  std::string apn, apn_user, apn_pass, network_dns, pincode, error, gps_parkpause, gps_parkreactivate, gps_parkreactlock, vehicle_stream, model, modem_net_type, modem_net_types_avail;
  bool enable_gps, enable_gpstime, enable_net, enable_sms, wrongpincode, gps_parkreactawake;
  float cfg_sq_good, cfg_sq_bad;

  if (c.method == "POST") {
    // process form submission:
    apn = c.getvar("apn");
    apn_user = c.getvar("apn_user");
    apn_pass = c.getvar("apn_pass");
    pincode = c.getvar("pincode");
    network_dns = c.getvar("network_dns");
    enable_net = (c.getvar("enable_net") == "yes");
    enable_sms = (c.getvar("enable_sms") == "yes");
    enable_gps = (c.getvar("enable_gps") == "yes");
    enable_gpstime = (c.getvar("enable_gpstime") == "yes");
    gps_parkpause = c.getvar("gps_parkpause");
    gps_parkreactivate = c.getvar("gps_parkreactivate");
    gps_parkreactlock = c.getvar("gps_parkreactlock");    
    gps_parkreactawake = (c.getvar("gps_awake_start") == "yes");
    vehicle_stream = c.getvar("vehicle_stream");
    cfg_sq_good = atof(c.getvar("cfg_sq_good").c_str());
    cfg_sq_bad = atof(c.getvar("cfg_sq_bad").c_str());
    modem_net_type = c.getvar("modem_net_type");

    if (cfg_sq_bad >= cfg_sq_good)
      {
      error += "<li data-input=\"cfg_sq_bad\">'Bad' signal level must be lower than 'good' level.</li>";
      }
    else 
      {
      MyConfig.SetParamValue("modem", "apn", apn);
      MyConfig.SetParamValue("modem", "apn.user", apn_user);
      MyConfig.SetParamValue("modem", "apn.password", apn_pass);
      if ( MyConfig.GetParamValueBool("modem","wrongpincode") && (MyConfig.GetParamValue("modem","pincode") != pincode) )
        {
        ESP_LOGI(TAG,"New SIM card PIN code entered. Cleared wrong_pin_code flag");
        MyConfig.SetParamValueBool("modem", "wrongpincode", false);
        }
      MyConfig.SetParamValue("modem", "pincode", pincode);
      MyConfig.SetParamValue("network", "dns", network_dns);
      if (modem_net_type != "undef") MyConfig.SetParamValue("modem", "net.type", modem_net_type);
      MyConfig.SetParamValueBool("modem", "enable.net", enable_net);
      MyConfig.SetParamValueBool("modem", "enable.sms", enable_sms);
      MyConfig.SetParamValueBool("modem", "enable.gps", enable_gps);
      MyConfig.SetParamValueBool("modem", "enable.gpstime", enable_gpstime);
      MyConfig.SetParamValue("modem", "gps.parkpause", gps_parkpause);
      MyConfig.SetParamValue("modem", "gps.parkreactivate", gps_parkreactivate);
      MyConfig.SetParamValue("modem", "gps.parkreactlock", gps_parkreactlock);
      MyConfig.SetParamValueBool("modem", "gps.parkreactawake", gps_parkreactawake);
      if (vehicle_stream == "0")
        MyConfig.DeleteInstance("vehicle", "stream");
      else
        MyConfig.SetParamValue("vehicle", "stream", vehicle_stream);
      MyConfig.SetParamValueFloat("network", "modem.sq.good", cfg_sq_good);
      MyConfig.SetParamValueFloat("network", "modem.sq.bad", cfg_sq_bad);
    }

    if (error == "")
      {
      c.head(200);
      c.alert("success", "<p class=\"lead\">Modem configured.</p>");
      OutputHome(p, c);
      c.done();
      return;
      }
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } 
  else
  {
    c.head(200);
  }

  // read configuration:
  apn = MyConfig.GetParamValue("modem", "apn");
  apn_user = MyConfig.GetParamValue("modem", "apn.user");
  apn_pass = MyConfig.GetParamValue("modem", "apn.password");
  pincode = MyConfig.GetParamValue("modem", "pincode");

  modem* m_modem=MyPeripherals->m_cellular_modem;
  model = m_modem && m_modem->m_driver ? m_modem->m_model : "auto";
  modem_net_type = MyConfig.GetParamValue("modem", "net.type","auto");
  modem_net_types_avail = m_modem && m_modem->m_driver ? m_modem->m_driver->GetNetTypes() : "auto"; 

  wrongpincode = MyConfig.GetParamValueBool("modem", "wrongpincode",false);
  network_dns = MyConfig.GetParamValue("network", "dns");
  enable_net = MyConfig.GetParamValueBool("modem", "enable.net", true);
  enable_sms = MyConfig.GetParamValueBool("modem", "enable.sms", true);
  enable_gps = MyConfig.GetParamValueBool("modem", "enable.gps", false);
  enable_gpstime = MyConfig.GetParamValueBool("modem", "enable.gpstime", false);
  gps_parkpause = MyConfig.GetParamValue("modem", "gps.parkpause","0");
  gps_parkreactivate = MyConfig.GetParamValue("modem", "gps.parkreactivate","0");
  gps_parkreactlock = MyConfig.GetParamValue("modem", "gps.parkreactlock","5");
  gps_parkreactawake = MyConfig.GetParamValueBool("modem", "gps.parkreactawake", false);
  vehicle_stream = MyConfig.GetParamValue("vehicle", "stream","0");
  cfg_sq_good = MyConfig.GetParamValueFloat("network", "modem.sq.good", -93);
  cfg_sq_bad = MyConfig.GetParamValueFloat("network", "modem.sq.bad", -95);

  // generate form:
  c.panel_start("primary", "Cellular modem configuration");
  c.form_start(p.uri);

  std::string info;
  std::string iccid = StdMetrics.ms_m_net_mdm_iccid->AsString();
  if (!iccid.empty()) {
    info = "<code class=\"autoselect\">" + iccid + "</code>";
  } else {
    info =
      "<div class=\"receiver\">"
        "<code class=\"autoselect\" data-metric=\"m.net.mdm.iccid\">(power cellular modem on to read)</code>"
        "&nbsp;"
        "<button class=\"btn btn-default\" data-cmd=\"power cellular on\" data-target=\"#pso\">Power cellular modem on</button>"
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
  c.input_text("SIM card PIN code", "pincode", pincode.c_str(), "",
    wrongpincode ? "<p style=\"color: red\">Wrong PIN code entered previously!</p>" : "<p>Not needed for Hologram SIM cards</p>");

  c.fieldset_start("Internet");
  c.input_checkbox("Enable IP networking", "enable_net", enable_net);
  c.input_text("APN", "apn", apn.c_str(), NULL,
    "<p>For Hologram, use APN <code>hologram</code> with empty username &amp; password</p>");
  c.input_text("…username", "apn_user", apn_user.c_str());
  c.input_text("…password", "apn_pass", apn_pass.c_str());
  c.input_text("DNS", "network_dns", network_dns.c_str(), "optional fixed DNS servers (space separated)",
    "<p>Set this to i.e. <code>8.8.8.8 8.8.4.4</code> (Google public DNS) if you encounter problems with your network provider DNS</p>");

  if ( model != "auto")
  {
    c.input_radiobtn_start("Network type preference", "modem_net_type");
    if (modem_net_types_avail.find(modem_net_type) == string::npos) modem_net_type = "auto";
    c.input_radiobtn_option("modem_net_type", "auto", "auto", modem_net_type == "auto");
    if (modem_net_types_avail.find("2G") != string::npos) c.input_radiobtn_option("modem_net_type", "2G (GSM)", "2G", modem_net_type == "2G");
    if (modem_net_types_avail.find("3G") != string::npos) c.input_radiobtn_option("modem_net_type", "3G (UMTS)", "3G", modem_net_type == "3G");
    if (modem_net_types_avail.find("4G") != string::npos) c.input_radiobtn_option("modem_net_type", "4G (LTE)", "4G", modem_net_type == "4G");
    if (modem_net_types_avail.find("5G") != string::npos) c.input_radiobtn_option("modem_net_type", "5G", "5G", modem_net_type == "5G");
    c.input_radiobtn_end(
    "<p>Automatic mode should work in most cases. In case of frequent net losses, it can help to limit the network type.</p>"
    "<p>The 3G standard has been switched off in most areas around the world. 2G/GSM is often still available.</p>");
  }
  else{
    c.input_info("Network type preference", "Modem model not identified yet");
    modem_net_type="undef";
  }

    c.fieldset_end();

  c.fieldset_start("Features");
  c.input_checkbox("Enable SMS", "enable_sms", enable_sms);
  c.input_checkbox("Enable GPS", "enable_gps", enable_gps);
  c.input_checkbox("Use GPS time", "enable_gpstime", enable_gpstime);
  c.fieldset_end();

  c.fieldset_start("Cellular client options");
  c.input_slider("Good signal level", "cfg_sq_good", 3, "dBm", -1, cfg_sq_good, -93.0, -128.0, 0.0, 0.1,
    "<p>Threshold for usable cellular signal strength</p>");
  c.input_slider("Bad signal level", "cfg_sq_bad", 3, "dBm", -1, cfg_sq_bad, -95.0, -128.0, 0.0, 0.1,
    "<p>Threshold for unusable cellular signal strength</p>");
  c.fieldset_end();

  c.fieldset_start("GPS parking and tracking");
  c.input("number", "GPS park pause", "gps_parkpause", gps_parkpause.c_str(), "Default: 0 = disabled",
    "<p>Auto pause GPS when parking for longer than this time / 0 = no auto pausing</p>"
    "<p>Pausing the GPS subsystem can help to avoid draining the 12V battery, see"
    " <a target=\"_blank\" href=\"https://docs.openvehicles.com/en/latest/userguide/warnings.html#average-power-usage\">user manual</a>"
    " for details.</p>",
    "min=\"0\" step=\"5\"", "Seconds");
  c.input("number", "GPS park re-activate", "gps_parkreactivate", gps_parkreactivate.c_str(), "Default: 0 = disabled",
    "<p>Auto re-activate GPS after parking for longer than this time / 0 = no auto re-activation</p>",
    "min=\"0\" step=\"5\"", "Minutes");
  c.input("number", "GPS lock time", "gps_parkreactlock", gps_parkreactlock.c_str(), "Default: 5",
    "<p>by default, GPS lock for 5 minutes until automatic shutdown during parking time</p>",
    "min=\"5\" step=\"1\"", "Minutes");  
  c.input_checkbox("Start GPS when Car awakes", "gps_awake_start", gps_parkreactawake,
    "<p>GPS is switched on for the GPS lock time when the GPS parking pause is active and the car wakes up.</p>"
    "<p>This reduces time to first GPS fix, but increases power consumption when Car is awake.</p>");
  c.input("number", "Location streaming", "vehicle_stream", vehicle_stream.c_str(), "Default: 0",
    "<p>While driving send location updates to server every n seconds, 0 = use default update interval</p>"
    "<p>from server configuration. Same as App feature #8.</p>",
    "min=\"0\" step=\"1\"", "Seconds");
  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
#endif


/**
 * HandleCfgPushover: Configure pushover notifications (URL /cfg/pushover)
 */
#ifdef CONFIG_OVMS_COMP_PUSHOVER
void OvmsWebServer::HandleCfgPushover(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  OvmsConfigParam* param = MyConfig.CachedParam("pushover");
  ConfigParamMap pmap;
  int i, max;
  char buf[100];
  std::string name, msg, pri;

  if (c.method == "POST") {
    // process form submission:
    pmap["enable"] = (c.getvar("enable") == "yes") ? "yes" : "no";
    pmap["user_key"] = c.getvar("user_key");
    pmap["token"] = c.getvar("token");

    // validate:
    //if (server.length() == 0)
    //  error += "<li data-input=\"server\">Server must not be empty</li>";
    if (pmap["enable"]=="yes")
      {
      if (pmap["user_key"].length() == 0)
        error += "<li data-input=\"user_key\">User key must not be empty</li>";
      if (pmap["user_key"].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">User key may only contain ASCII letters and digits</li>";
      if (pmap["token"].length() == 0)
        error += "<li data-input=\"token\">Token must not be empty</li>";
      if (pmap["token"].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos)
        error += "<li data-input=\"user_key\">Token may only contain ASCII letters and digits</li>";
      }

    pmap["sound.normal"] = c.getvar("sound.normal");
    pmap["sound.high"] = c.getvar("sound.high");
    pmap["sound.emergency"] = c.getvar("sound.emergency");
    pmap["expire"] = c.getvar("expire");
    pmap["retry"] = c.getvar("retry");

    // read notification type/subtypes and their priorities
    max = atoi(c.getvar("npmax").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "nfy_%d", i);
      name = c.getvar(buf);
      if (name == "") continue;
      sprintf(buf, "np_%d", i);
      pri = c.getvar(buf);
      if (pri == "") continue;
      snprintf(buf, sizeof(buf), "np.%s", name.c_str());
      pmap[buf] = pri;
    }

    // read events, their messages and priorities
    max = atoi(c.getvar("epmax").c_str());
    for (i = 1; i <= max; i++) {
      sprintf(buf, "en_%d", i);
      name = c.getvar(buf);
      if (name == "") continue;
      sprintf(buf, "em_%d", i);
      msg = c.getvar(buf);
      sprintf(buf, "ep_%d", i);
      pri = c.getvar(buf);
      if (pri == "") continue;
      snprintf(buf, sizeof(buf), "ep.%s", name.c_str());
      pri.append("/");
      pri.append(msg);
      pmap[buf] = pri;
    }

    if (error == "") {
      if (c.getvar("action") == "save") {
        // save:
        param->m_map.clear();
        param->m_map = std::move(pmap);
        param->Save();

        c.head(200);
        c.alert("success", "<p class=\"lead\">Pushover connection configured.</p>");
        OutputHome(p, c);
        c.done();
        return;
      } else if (c.getvar("action") == "test")
      {
        std::string reply;
        std::string popup;
        c.head(200);
        c.alert("success", "<p class=\"lead\">Sending message</p>");
        if (!MyPushoverClient.SendMessageOpt(
            c.getvar("user_key"),
            c.getvar("token"),
            c.getvar("test_message"),
            atoi(c.getvar("test_priority").c_str()),
            c.getvar("test_sound"),
            atoi(c.getvar("retry").c_str()),
            atoi(c.getvar("expire").c_str()),
            true /* receive server reply as reply/pushover-type notification */ ))
        {
          c.alert("danger", "<p class=\"lead\">Could not send test message!</p>");
        }
      }
    }
    else {
      // output error, return to form:
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.head(400);
      c.alert("danger", error.c_str());
    }

  }
  else {
    // read configuration:
    pmap = param->m_map;

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Pushover server configuration");
  c.form_start(p.uri);

  c.printf("<div><p>Please visit <a href=\"https://pushover.net\">Pushover web site</a> to create an account (identified by a <b>user key</b>) "
    " and then register OVMS as an application in order to receive an application <b>token</b>.<br>"
    "Install Pushover iOS/Android application and specify your user key. <br>Finally enter both the user key and the application token here and test connectivity.<br>"
    "To receive specific notifications and events, configure them below.</p></div>" );

  c.input_checkbox("Enable Pushover connectivity", "enable", pmap["enable"] == "yes");
  c.input_text("User key", "user_key", pmap["user_key"].c_str(), "Enter user key (alphanumerical key consisting of around 30 characters");
  c.input_text("Token", "token", pmap["token"].c_str(), "Enter token (alphanumerical key consisting of around 30 characters");

  auto gen_options_priority = [&c](std::string priority) {
    c.printf(
        "<option value=\"-2\" %s>Lowest</option>"
        "<option value=\"-1\" %s>Low</option>"
        "<option value=\"0\" %s>Normal</option>"
        "<option value=\"1\" %s>High</option>"
        "<option value=\"2\" %s>Emergency</option>"
      , (priority=="-2") ? "selected" : ""
      , (priority=="-1") ? "selected" : ""
      , (priority=="0"||priority=="") ? "selected" : ""
      , (priority=="1") ? "selected" : ""
      , (priority=="2") ? "selected" : "");
  };

  auto gen_options_sound = [&c](std::string sound) {
    c.printf(
        "<option value=\"pushover\" %s>Pushover (default)</option>"
        "<option value=\"bike\" %s>Bike</option>"
        "<option value=\"bugle\" %s>Bugle</option>"
        "<option value=\"cashregister\" %s>Cashregister</option>"
        "<option value=\"classical\" %s>Classical</option>"
        "<option value=\"cosmic\" %s>Cosmic</option>"
        "<option value=\"falling\" %s>Falling</option>"
        "<option value=\"gamelan\" %s>Gamelan</option>"
        "<option value=\"incoming\" %s>Incoming</option>"
        "<option value=\"intermission\" %s>Intermission</option>"
        "<option value=\"magic\" %s>Magic</option>"
        "<option value=\"mechanical\" %s>Mechanical</option>"
        "<option value=\"pianobar\" %s>Piano bar</option>"
        "<option value=\"siren\" %s>Siren</option>"
        "<option value=\"spacealarm\" %s>Space alarm</option>"
        "<option value=\"tugboat\" %s>Tug boat</option>"
        "<option value=\"alien\" %s>Alien alarm (long)</option>"
        "<option value=\"climb\" %s>Climb (long)</option>"
        "<option value=\"persistent\" %s>Persistent (long)</option>"
        "<option value=\"echo\" %s>Pushover Echo (long)</option>"
        "<option value=\"updown\" %s>Up Down (long)</option>"
        "<option value=\"none\" %s>None (silent)</option>"
      , (sound=="pushover") || (sound=="") ? "selected" : ""
      , (sound=="bike") ? "selected" : ""
      , (sound=="bugle") ? "selected" : ""
      , (sound=="cashregister") ? "selected" : ""
      , (sound=="classical") ? "selected" : ""
      , (sound=="cosmic") ? "selected" : ""
      , (sound=="falling") ? "selected" : ""
      , (sound=="gamelan") ? "selected" : ""
      , (sound=="incoming") ? "selected" : ""
      , (sound=="intermission") ? "selected" : ""
      , (sound=="magic") ? "selected" : ""
      , (sound=="mechanical") ? "selected" : ""
      , (sound=="pianobar") ? "selected" : ""
      , (sound=="siren") ? "selected" : ""
      , (sound=="spacealarm") ? "selected" : ""
      , (sound=="tugboat") ? "selected" : ""
      , (sound=="alien") ? "selected" : ""
      , (sound=="climb") ? "selected" : ""
      , (sound=="persistent") ? "selected" : ""
      , (sound=="echo") ? "selected" : ""
      , (sound=="updown") ? "selected" : ""
      , (sound=="none") ? "selected" : "");
  };

  c.input_select_start("Normal priority sound", "sound.normal");
  gen_options_sound(pmap["sound.normal"]);
  c.input_select_end();
  c.input_select_start("High priority sound", "sound.high");
  gen_options_sound(pmap["sound.high"]);
  c.input_select_end();
  c.input_select_start("Emergency priority sound", "sound.emergency");
  gen_options_sound(pmap["sound.emergency"]);
  c.input_select_end();

  c.input("number", "Retry", "retry", pmap["retry"].c_str(), "Default: 30",
    "<p>Time period after which new notification is sent if emergency priority message is not acknowledged.</p>",
    "min=\"30\" step=\"1\"", "secs");
  c.input("number", "Expiration", "expire", pmap["expire"].c_str(), "Default: 1800",
    "<p>Time period after an emergency priority message will expire (and will not cause a new notification) if the message is not acknowledged.</p>",
    "min=\"0\" step=\"1\"", "secs");

  // Test message area
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Test connection</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<tbody>"
            "<tr>"
              "<td width=\"40%\">");

  c.input_text("Message", "test_message", c.getvar("test_message").c_str(), "Enter test message");
  c.print("</td><td width=\"25%\">");
  c.input_select_start("Priority", "test_priority");
  gen_options_priority(c.getvar("test_priority") != "" ? c.getvar("test_priority") : "0");
  c.input_select_end();
  c.print("</td><td width=\"25%\">");

  c.input_select_start("Sound", "test_sound");
  gen_options_sound( c.getvar("test_sound") != "" ? c.getvar("test_sound").c_str() : pmap["sound.normal"]);
  c.input_select_end();
  c.print("</td><td width=\"10%\">");
  c.input_button("default", "Send", "action", "test");
  c.printf(
          "</td></tr>"
        "</tbody>"
      "</table>"
    "</div>"
    "</div>"
    "</div>");


  // Input area for Notifications
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Notification filtering</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<thead>"
            "<tr>"
              "<th width=\"10%\"></th>"
              "<th width=\"45%\">Type/Subtype</th>"
              "<th width=\"45%\">Priority</th>"
            "</tr>"
          "</thead>"
          "<tbody>");


  max = 0;
  for (auto &kv: pmap) {
    if (!startsWith(kv.first, "np."))
      continue;
    max++;
    name = kv.first.substr(3);
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"nfy_%d\" value=\"%s\" placeholder=\"Enter notification type/subtype\""
              " autocomplete=\"section-notification-type\"></td>"
            "<td width=\"20%%\"><select class=\"form-control\" name=\"np_%d\" size=\"1\">"
      , max, _attr(name)
      , max);
    gen_options_priority(kv.second);
    c.print(
            "</select></td>"
          "</tr>");
  }

  c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow_nfy(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
      "<input type=\"hidden\" name=\"npmax\" value=\"%d\">"
    "</div>"
    "<p>Enter the type of notification (for example <i>\"alert\"</i> or <i>\"info\"</i>) or more specifically the type/subtype tuple (for example <i>\"alert/alarm.sounding\"</i>). "
    " If a notification matches multiple filters, only the more specific will be used. "
    "For more complete listing, see <a href=\"https://docs.openvehicles.com/en/latest/userguide/notifications.html\">OVMS User Guide</a></p>"
    "</div>"
    "</div>"
    , max);


  // Input area for Events
  c.print(
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Event filtering</label>"
    "<div class=\"col-sm-9\">"
      "<div class=\"table-responsive\">"
        "<table class=\"table\">"
          "<thead>"
            "<tr>"
              "<th width=\"10%\"></th>"
              "<th width=\"20%\">Event</th>"
              "<th width=\"55%\">Message</th>"
              "<th width=\"15%\">Priority</th>"
            "</tr>"
          "</thead>"
          "<tbody>");


  max = 0;
  for (auto &kv: pmap) {
    if (!startsWith(kv.first, "ep."))
      continue;
    max++;
    // Priority and message is saved as "priority/message" tuple (eg. "-1/this is a message")
    name = kv.first.substr(3);
    if (kv.second[1]=='/') {
      pri = kv.second.substr(0,1);
      msg = kv.second.substr(2);
    } else
    if (kv.second[2]=='/') {
      pri = kv.second.substr(0,2);
      msg = kv.second.substr(3);
    } else continue;
    c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"en_%d\" value=\"%s\" placeholder=\"Enter event name\""
              " autocomplete=\"section-event-name\"></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"em_%d\" value=\"%s\" placeholder=\"Enter message\""
              " autocomplete=\"section-event-message\"></td>"
            "<td><select class=\"form-control\" name=\"ep_%d\" size=\"1\">"
      , max, _attr(name)
      , max, _attr(msg)
      , max);
    gen_options_priority(pri);
    c.print(
            "</select></td>"
          "</tr>");
  }

  c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-success\" onclick=\"addRow_ev(this)\"><strong>✚</strong></button></td>"
            "<td></td>"
            "<td></td>"
          "</tr>"
        "</tbody>"
      "</table>"
      "<input type=\"hidden\" name=\"epmax\" value=\"%d\">"
    "</div>"
    "<p>Enter the event name (for example <i>\"vehicle.locked\"</i> or <i>\"vehicle.alert.12v.on\"</i>). "
    "For more complete listing, see <a href=\"https://docs.openvehicles.com/en/latest/userguide/events.html\">OVMS User Guide</a></p>"
    "</div>"
    "</div>"
    , max);


  c.input_button("default", "Save","action","save");
  c.form_end();

  c.print(
    "<script>"
    "function delRow(el){"
      "$(el).parent().parent().remove();"
    "}"
    "function addRow_nfy(el){"
      "var counter = $('input[name=npmax]');"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"nfy_' + nr + '\" placeholder=\"Enter type/subtype\""
              " autocomplete=\"section-notification-type\"></td>"
            "<td><select class=\"form-control\" name=\"np_' + nr + '\" size=\"1\">"
              "<option value=\"-2\">Lowest</option>"
              "<option value=\"-1\">Low</option>"
              "<option value=\"0\" selected>Normal</option>"
              "<option value=\"1\">High</option>"
              "<option value=\"2\">Emergency</option>"
            "</select></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "function addRow_ev(el){"
      "var counter = $('input[name=epmax]');"
      "var nr = Number(counter.val()) + 1;"
      "var row = $('"
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\"><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"en_' + nr + '\" placeholder=\"Enter event name\""
              " autocomplete=\"section-event-name\"></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"em_' + nr + '\" placeholder=\"Enter message\""
              " autocomplete=\"section-event-message\"></td>"
            "<td><select class=\"form-control\" name=\"ep_' + nr + '\" size=\"1\">"
              "<option value=\"-2\">Lowest</option>"
              "<option value=\"-1\">Low</option>"
              "<option value=\"0\" selected>Normal</option>"
              "<option value=\"1\">High</option>"
              "<option value=\"2\">Emergency</option>"
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
  bool tls;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    tls = (c.getvar("tls") == "yes");
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
      MyConfig.SetParamValueBool("server.v2", "tls", tls);
      MyConfig.SetParamValue("server.v2", "port", port);
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      if (password != "")
        MyConfig.SetParamValue("password","server.v2", password);
      MyConfig.SetParamValue("server.v2", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v2", "updatetime.idle", updatetime_idle);

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
  std::string updatetime_connected, updatetime_idle, updatetime_on;
  std::string updatetime_charging, updatetime_awake, updatetime_sendall, updatetime_keepalive;
  std::string metrics_priority, metrics_include, metrics_exclude, metrics_immediately, metrics_exclude_immediately;
  std::string queue_sendall, queue_modified;
  bool tls, legacy_event_topic, updatetime_priority, updatetime_immediately;

  if (c.method == "POST") {
    // process form submission:
    server = c.getvar("server");
    tls = (c.getvar("tls") == "yes");
    legacy_event_topic = (c.getvar("legacy_event_topic") == "yes");
    user = c.getvar("user");
    password = c.getvar("password");
    port = c.getvar("port");
    topic_prefix = c.getvar("topic_prefix");
    updatetime_connected = c.getvar("updatetime_connected");
    updatetime_idle = c.getvar("updatetime_idle");
    updatetime_on = c.getvar("updatetime_on");
    updatetime_charging = c.getvar("updatetime_charging");
    updatetime_awake = c.getvar("updatetime_awake");
    updatetime_sendall = c.getvar("updatetime_sendall");
    updatetime_keepalive = c.getvar("updatetime_keepalive");
    updatetime_priority = (c.getvar("updatetime_priority") == "yes");
    updatetime_immediately = (c.getvar("updatetime_immediately") == "yes");
    metrics_priority = c.getvar("metrics_priority");
    metrics_include = c.getvar("metrics_include");
    metrics_exclude = c.getvar("metrics_exclude");
    metrics_immediately = c.getvar("metrics_immediately");
    metrics_exclude_immediately = c.getvar("metrics_exclude_immediately");
    queue_sendall = c.getvar("queue_sendall");
    queue_modified = c.getvar("queue_modified");

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
    if (updatetime_on != "") {
      if (atoi(updatetime_on.c_str()) < 1) {
        error += "<li data-input=\"updatetime_on\">Update interval (on) must be at least 1 second</li>";
      }
    }
    if (updatetime_charging != "") {
      if (atoi(updatetime_charging.c_str()) < 1) {
        error += "<li data-input=\"updatetime_charging\">Update interval (charging) must be at least 1 second</li>";
      }
    }
    if (updatetime_awake != "") {
      if (atoi(updatetime_awake.c_str()) < 1) {
        error += "<li data-input=\"updatetime_awake\">Update interval (awake) must be at least 1 second</li>";
      }
    }
    if (updatetime_sendall != "") {
      if (atoi(updatetime_sendall.c_str()) < 60) {
        error += "<li data-input=\"updatetime_sendall\">Update interval (sendall) must be at least 60 seconds</li>";
      }
    }
    if (updatetime_keepalive != "") {
      if (atoi(updatetime_keepalive.c_str()) < 60) {
        error += "<li data-input=\"updatetime_keepalive\">Keepalive interval must be at least 60 seconds</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("server.v3", "server", server);
      MyConfig.SetParamValueBool("server.v3", "tls", tls);
      MyConfig.SetParamValueBool("server.v3", "events.legacy_topic", legacy_event_topic);
      MyConfig.SetParamValue("server.v3", "user", user);
      if (password != "")
        MyConfig.SetParamValue("password", "server.v3", password);
      MyConfig.SetParamValue("server.v3", "port", port);
      MyConfig.SetParamValue("server.v3", "topic.prefix", topic_prefix);
      MyConfig.SetParamValue("server.v3", "updatetime.connected", updatetime_connected);
      MyConfig.SetParamValue("server.v3", "updatetime.idle", updatetime_idle);
      MyConfig.SetParamValue("server.v3", "updatetime.on", updatetime_on);
      MyConfig.SetParamValue("server.v3", "updatetime.charging", updatetime_charging);
      MyConfig.SetParamValue("server.v3", "updatetime.awake", updatetime_awake);
      MyConfig.SetParamValue("server.v3", "updatetime.sendall", updatetime_sendall);
      MyConfig.SetParamValue("server.v3", "updatetime.keepalive", updatetime_keepalive);
      MyConfig.SetParamValueBool("server.v3", "updatetime.priority", updatetime_priority);
      MyConfig.SetParamValueBool("server.v3", "updatetime.immediately", updatetime_immediately);
      MyConfig.SetParamValue("server.v3", "metrics.priority", metrics_priority);
      MyConfig.SetParamValue("server.v3", "metrics.include", metrics_include);
      MyConfig.SetParamValue("server.v3", "metrics.exclude", metrics_exclude);
      MyConfig.SetParamValue("server.v3", "metrics.include.immediately", metrics_immediately);
      MyConfig.SetParamValue("server.v3", "metrics.exclude.immediately", metrics_exclude_immediately);
      MyConfig.SetParamValue("server.v3", "queue.sendall", queue_sendall);
      MyConfig.SetParamValue("server.v3", "queue.modified", queue_modified);

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
    tls = MyConfig.GetParamValueBool("server.v3", "tls", false);
    legacy_event_topic = MyConfig.GetParamValueBool("server.v3", "events.legacy_topic", true);
    user = MyConfig.GetParamValue("server.v3", "user");
    password = MyConfig.GetParamValue("password", "server.v3");
    port = MyConfig.GetParamValue("server.v3", "port");
    topic_prefix = MyConfig.GetParamValue("server.v3", "topic.prefix");
    updatetime_connected = MyConfig.GetParamValue("server.v3", "updatetime.connected");
    updatetime_idle = MyConfig.GetParamValue("server.v3", "updatetime.idle");
    updatetime_on = MyConfig.GetParamValue("server.v3", "updatetime.on");
    updatetime_charging = MyConfig.GetParamValue("server.v3", "updatetime.charging");
    updatetime_awake = MyConfig.GetParamValue("server.v3", "updatetime.awake");
    updatetime_sendall = MyConfig.GetParamValue("server.v3", "updatetime.sendall");
    updatetime_keepalive = MyConfig.GetParamValue("server.v3", "updatetime.keepalive", "1740");
    updatetime_priority = MyConfig.GetParamValueBool("server.v3", "updatetime.priority", false);
    updatetime_immediately = MyConfig.GetParamValueBool("server.v3", "updatetime.immediately", false);
    metrics_priority = MyConfig.GetParamValue("server.v3", "metrics.priority");
    metrics_include = MyConfig.GetParamValue("server.v3", "metrics.include");
    metrics_exclude = MyConfig.GetParamValue("server.v3", "metrics.exclude");
    metrics_immediately = MyConfig.GetParamValue("server.v3", "metrics.include.immediately");
    metrics_exclude_immediately = MyConfig.GetParamValue("server.v3", "metrics.exclude.immediately");
    queue_sendall = MyConfig.GetParamValue("server.v3", "queue.sendall");
    queue_modified = MyConfig.GetParamValue("server.v3", "queue.modified");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Server V3 (MQTT) configuration");
  c.form_start(p.uri);

  c.input_text("Server", "server", server.c_str(), "Enter host name or IP address",
    "<p>Public OVMS V3 servers (MQTT brokers):</p>"
    "<ul>"
      "<li><code>api.openvehicles.com</code> <a href=\"https://www.openvehicles.com/user/register\" target=\"_blank\">Registration</a></li>"
      "<li><code>ovms.dexters-web.de</code> <a href=\"https://dexters-web.de/?action=NewAccount\" target=\"_blank\">Registration</a></li>"
      "<li><a href=\"https://github.com/mqtt/mqtt.github.io/wiki/public_brokers\" target=\"_blank\">More public MQTT brokers</a></li>"
    "</ul>");
  c.input_checkbox("Enable TLS", "tls", tls,
    "<p>Note: enable transport layer security (encryption) if your server supports it.</p>");
  c.input_checkbox("Enable legacy event topic", "legacy_event_topic", legacy_event_topic,
    "In addition to MQTT-style topics, also publish on <i>&lt;prefix&gt;</i>/event.");
  c.input_text("Port", "port", port.c_str(), "optional, default: 1883 (no TLS) / 8883 (TLS)");
  c.input_text("Username", "user", user.c_str(), "Enter user login name",
    NULL, "autocomplete=\"section-serverv3 username\"");
  c.input_password("Password", "password", "", "Enter user password, empty = no change",
    NULL, "autocomplete=\"section-serverv3 current-password\"");
  c.input_text("Topic Prefix", "topic_prefix", topic_prefix.c_str(),
    "optional, default: ovms/<username>/<vehicle id>/");

  c.fieldset_start("Update intervals");
  c.input("number", "…idle", "updatetime_idle", updatetime_idle.c_str(), "default: 600", "default: 600, update interval when client not connected", "min=\"1\" max=\"1200\" step=\"1\"", "seconds");
  c.input("number", "…on", "updatetime_on", updatetime_on.c_str(), "default: 5", "default: 5, update interval when Car is on", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…charging", "updatetime_charging", updatetime_charging.c_str(), "default: 10", "default: 10, update interval when Car is charging", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…awake", "updatetime_awake", updatetime_awake.c_str(), "default: 60", "default: 60, update interval when Car is awake", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…connected", "updatetime_connected", updatetime_connected.c_str(), "default: 10", "default: 10, update interval when App/Client is connected", "min=\"1\" max=\"600\" step=\"1\"", "seconds");
  c.input("number", "…sendall", "updatetime_sendall", updatetime_sendall.c_str(), "default: 1200", "default: 1200", "min=\"60\" max=\"3600\" step=\"1\"", "seconds");
  c.input("number", "…keepalive", "updatetime_keepalive", updatetime_keepalive.c_str(), "default: 1740",
    "<p>default: 1740. Keepalive defines how often PINGREQs should be sent if there's inactivity. "
    "It should be set slightly shorter than the network's NAT timeout "
    "and the timeout of your MQTT server. If these are unknown you can use trial "
    "and error. Symptoms of keepalive being too high are a lack of metric updates "
    "after a certain point, or &quot;Disconnected from OVMS Server V3&quot; "
    "appearing in the log.</p>",
    "min=\"60\" max=\"3600\" step=\"1\"", "seconds");  
  c.fieldset_end();
  
  c.fieldset_start("Update Configuration");
  c.input_checkbox("prioritize metrics", "updatetime_priority", updatetime_priority,
    "<p>Metrics should be updated before other MQTT traffic when Car is awake. This can contribute to smoother tracking on V3 servers.</p>"
    "<p><strong>Note:</strong> The update interval corresponds to the <strong>...on</strong> setting!</p>");
  c.input_checkbox("Update metrics immediately", "updatetime_immediately", updatetime_immediately,
    "<p>Metrics should be sent immediately when they change.</p>"
    "<p><strong>Note:</strong> This setting significantly increases data transfer!</p>");
  c.input("number", "queue size sendall", "queue_sendall", queue_sendall.c_str(), "default: 100", "default: 100", "min=\"1\" max=\"500\" step=\"1\"", "size");
  c.input("number", "queue size modified", "queue_modified", queue_modified.c_str(), "default: 150", "default: 150", "min=\"1\" max=\"500\" step=\"1\"", "size");
  c.input_text("priority metrics", "metrics_priority", metrics_priority.c_str(), NULL,
    "<p>default priority: v.p.latitude, v.p.longitude, v.p.altitude, v.p.speed, v.p.gpsspeed, m.time.utc</br>"
    "additional comma-separated list of metrics to prioritize when Car is awake, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics include", "metrics_include", metrics_include.c_str(), NULL,
    "<p>comma-separated list of metrics to include update, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics exclude", "metrics_exclude", metrics_exclude.c_str(), NULL,
    "<p>comma-separated list of metrics to exclude update, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics include immediate", "metrics_immediately", metrics_immediately.c_str(), "set which ones update immediately",
    "<p>comma-separated list of metrics to send immediately when they change, wildcard supported e.g. v.c.*, m.net.*</p>");
  c.input_text("metrics exclude immediate", "metrics_exclude_immediately", metrics_exclude_immediately.c_str(), "set which ones not update immediately",
    "<p>comma-separated list of metrics to <strong>not</strong> send immediately when they change, wildcard supported e.g. v.c.*, m.net.*</p>");
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
 * HandleCfgNotifications: configure notifications & data logging (URL /cfg/notifications)
 */
void OvmsWebServer::HandleCfgNotifications(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string vehicle_minsoc, vehicle_stream;
  std::string log_trip_storetime, log_trip_minlength, log_grid_storetime;
  bool report_trip_enable;
  std::string report_trip_minlength;

  if (c.method == "POST") {
    // process form submission:
    vehicle_minsoc = c.getvar("vehicle_minsoc");
    vehicle_stream = c.getvar("vehicle_stream");
    log_trip_storetime = c.getvar("log_trip_storetime");
    log_trip_minlength = c.getvar("log_trip_minlength");
    log_grid_storetime = c.getvar("log_grid_storetime");
    report_trip_enable = (c.getvar("report_trip_enable") == "yes");
    report_trip_minlength = c.getvar("report_trip_minlength");

    if (vehicle_minsoc != "") {
      if (atoi(vehicle_minsoc.c_str()) < 0 || atoi(vehicle_minsoc.c_str()) > 100) {
        error += "<li data-input=\"vehicle_minsoc\">Min SOC must be in the range 0…100 %</li>";
      }
    }
    if (vehicle_stream != "") {
      if (atoi(vehicle_stream.c_str()) < 0 || atoi(vehicle_stream.c_str()) > 60) {
        error += "<li data-input=\"vehicle_stream\">GPS log interval must be in the range 0…60 seconds</li>";
      }
    }

    if (log_trip_storetime != "") {
      if (atoi(log_trip_storetime.c_str()) < 0 || atoi(log_trip_storetime.c_str()) > 365) {
        error += "<li data-input=\"log_trip_storetime\">Trip history log storage time must be in the range 0…365 days</li>";
      }
    }
    if (log_trip_minlength != "") {
      if (atoi(log_trip_minlength.c_str()) < 0) {
        error += "<li data-input=\"log_trip_minlength\">Trip min length must not be negative</li>";
      }
    }
    if (log_grid_storetime != "") {
      if (atoi(log_grid_storetime.c_str()) < 0 || atoi(log_grid_storetime.c_str()) > 365) {
        error += "<li data-input=\"log_grid_storetime\">Grid history log storage time must be in the range 0…365 days</li>";
      }
    }

    if (report_trip_minlength != "") {
      if (atoi(report_trip_minlength.c_str()) < 0) {
        error += "<li data-input=\"report_trip_minlength\">Trip report min length must not be negative</li>";
      }
    }

    if (error == "") {
      // success:
      if (vehicle_minsoc == "0")
        MyConfig.DeleteInstance("vehicle", "minsoc");
      else
        MyConfig.SetParamValue("vehicle", "minsoc", vehicle_minsoc);
      if (vehicle_stream == "0")
        MyConfig.DeleteInstance("vehicle", "stream");
      else
        MyConfig.SetParamValue("vehicle", "stream", vehicle_stream);

      if (log_trip_storetime == "")
        MyConfig.DeleteInstance("notify", "log.trip.storetime");
      else
        MyConfig.SetParamValue("notify", "log.trip.storetime", log_trip_storetime);
      if (log_trip_minlength == "")
        MyConfig.DeleteInstance("notify", "log.trip.minlength");
      else
        MyConfig.SetParamValue("notify", "log.trip.minlength", log_trip_minlength);
      if (log_grid_storetime == "")
        MyConfig.DeleteInstance("notify", "log.grid.storetime");
      else
        MyConfig.SetParamValue("notify", "log.grid.storetime", log_grid_storetime);

      MyConfig.SetParamValueBool("notify", "report.trip.enable", report_trip_enable);
      if (report_trip_minlength == "")
        MyConfig.DeleteInstance("notify", "report.trip.minlength");
      else
        MyConfig.SetParamValue("notify", "report.trip.minlength", report_trip_minlength);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Notifications configured.</p>");
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
    vehicle_minsoc = MyConfig.GetParamValue("vehicle", "minsoc");
    vehicle_stream = MyConfig.GetParamValue("vehicle", "stream");
    log_trip_storetime = MyConfig.GetParamValue("notify", "log.trip.storetime");
    log_trip_storetime = MyConfig.GetParamValue("notify", "log.trip.storetime");
    log_trip_minlength = MyConfig.GetParamValue("notify", "log.trip.minlength");
    log_grid_storetime = MyConfig.GetParamValue("notify", "log.grid.storetime");
    report_trip_enable = MyConfig.GetParamValueBool("notify", "report.trip.enable");
    report_trip_minlength = MyConfig.GetParamValue("notify", "report.trip.minlength");

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Notifications &amp; Data Logging");
  c.form_start(p.uri);

  c.fieldset_start("Vehicle Monitoring");

  c.input_slider("Location streaming", "vehicle_stream", 3, "sec",
    -1, atoi(vehicle_stream.c_str()), 0, 0, 60, 1,
    "<p>While driving send location updates to server every n seconds, 0 = use default update interval "
    "from server configuration. Same as App feature #8.</p>");

  c.input_slider("Minimum SOC", "vehicle_minsoc", 3, "%",
    -1, atoi(vehicle_minsoc.c_str()), 0, 0, 100, 1,
    "<p>Send an alert when SOC drops below this level, 0 = off. Same as App feature #9.</p>");

  c.fieldset_end();

  c.fieldset_start("Vehicle Reports");

  c.input_checkbox("Enable trip report", "report_trip_enable", report_trip_enable,
    "<p>This will send a textual report on driving statistics after each trip.</p>");
  c.input("number", "Report min trip length", "report_trip_minlength", report_trip_minlength.c_str(), "Default: 0.2 km",
    "<p>Only trips over at least this distance will produce a report. If your vehicle does not support the "
    "<code>v.p.trip</code> metric, set this to 0.</p>",
    "min=\"0\" step=\"0.1\"", "km");

  c.fieldset_end();

  c.fieldset_start("Data Log Storage Times");

  c.input("number", "Trip history log", "log_trip_storetime", log_trip_storetime.c_str(), "Default: empty/0 = disabled",
    "<p>Empty/0 = disabled. If enabled, the trip log receives one entry per trip, "
    "see <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html#trip-history-log"
    "\">user manual</a> for details.</p>",
    "min=\"0\" max=\"365\" step=\"1\"", "days");
  c.input("number", "Trip min length", "log_trip_minlength", log_trip_minlength.c_str(), "Default: 0.2 km",
    "<p>Only trips over at least this distance will be logged. If your vehicle does not support the "
    "<code>v.p.trip</code> metric, set this to 0.</p>",
    "min=\"0\" step=\"0.1\"", "km");

  c.input("number", "Grid history log", "log_grid_storetime", log_grid_storetime.c_str(), "Default: empty/0 = disabled",
    "<p>Empty/0 = disabled. If enabled, the grid log receives one entry per charge/generator state change, "
    "see <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html#grid-history-log"
    "\">user manual</a> for details.</p>",
    "min=\"0\" max=\"365\" step=\"1\"", "days");

  c.fieldset_end();

  c.hr();
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>Data logs are sent to the server (V2/V3). A V2 server may store the logs as requested by you "
    "for up to 365 days, depending on the server configuration. You can download the log tables "
    "in CSV format from the server using the standard server APIs or the browser UI provided "
    "by the server.</p>"
    "<p>An MQTT (V3) server normally won't store data records for longer than a few days, possibly "
    "hours. You need to fetch them as soon as possible. There are automated MQTT tools available "
    "for this purpose.</p>"
    "<p>See <a target=\"_blank\" href=\""
    "https://docs.openvehicles.com/en/latest/userguide/notifications.html"
    "\">user manual</a> for details on notifications.</p>"
    );
  c.done();
}


/**
 * HandleCfgWebServer: configure web server (URL /cfg/webserver)
 */
void OvmsWebServer::HandleCfgWebServer(PageEntry_t& p, PageContext_t& c)
{
  std::string error, warn;
  std::string ws_txqueuesize, docroot, auth_domain, auth_file;
  bool enable_files, enable_dirlist, auth_global;
  extram::string tls_cert, tls_key;

  if (c.method == "POST") {
    // process form submission:
    ws_txqueuesize = c.getvar("ws_txqueuesize");
    docroot = c.getvar("docroot");
    auth_domain = c.getvar("auth_domain");
    auth_file = c.getvar("auth_file");
    enable_files = (c.getvar("enable_files") == "yes");
    enable_dirlist = (c.getvar("enable_dirlist") == "yes");
    auth_global = (c.getvar("auth_global") == "yes");
    c.getvar("tls_cert", tls_cert);
    c.getvar("tls_key", tls_key);

    // validate:
    if (ws_txqueuesize != "" && atoi(ws_txqueuesize.c_str()) < 10) {
      error += "<li data-input=\"ws_txqueuesize\">TX queue size must be at least 10</li>";
    }
    if (docroot != "" && docroot[0] != '/') {
      error += "<li data-input=\"docroot\">Document root must start with '/'</li>";
    }
    if (docroot == "/" || docroot == "/store" || docroot == "/store/" || startsWith(docroot, "/store/ovms_config")) {
      warn += "<li data-input=\"docroot\">Document root <code>" + docroot + "</code> may open access to OVMS configuration files, consider using a sub directory</li>";
    }
    if (!tls_cert.empty() && !startsWith(tls_cert, "-----BEGIN CERTIFICATE-----")) {
      error += "<li data-input=\"tls_cert\">TLS certificate must be in PEM CERTIFICATE format</li>";
    }
    if (!tls_key.empty() && !startsWith(tls_key, "-----BEGIN PRIVATE KEY-----")) {
      error += "<li data-input=\"tls_key\">TLS private key must be in PEM PRIVATE KEY format</li>";
    }
    if (tls_cert.empty() != tls_key.empty()) {
      error += "<li data-input=\"tls_cert,tls_key\">Both TLS certificate and private key must be given</li>";
    }

    // save TLS files:
    if (error == "") {
      if (tls_cert.empty()) {
        unlink("/store/tls/webserver.crt");
        unlink("/store/tls/webserver.key");
      }
      else {
        if (save_file("/store/tls/webserver.crt", tls_cert) != 0) {
          error += "<li data-input=\"tls_cert\">Error saving TLS certificate: ";
          error += strerror(errno);
          error += "</li>";
        }
        if (save_file("/store/tls/webserver.key", tls_key) != 0) {
          error += "<li data-input=\"tls_key\">Error saving TLS private key: ";
          error += strerror(errno);
          error += "</li>";
        }
      }
    }

    if (error == "") {
      // success:
      if (ws_txqueuesize == "")   MyConfig.DeleteInstance("http.server", "ws.txqueuesize");
      else                        MyConfig.SetParamValue("http.server", "ws.txqueuesize", ws_txqueuesize);
      if (docroot == "")          MyConfig.DeleteInstance("http.server", "docroot");
      else                        MyConfig.SetParamValue("http.server", "docroot", docroot);
      if (auth_domain == "")      MyConfig.DeleteInstance("http.server", "auth.domain");
      else                        MyConfig.SetParamValue("http.server", "auth.domain", auth_domain);
      if (auth_file == "")        MyConfig.DeleteInstance("http.server", "auth.file");
      else                        MyConfig.SetParamValue("http.server", "auth.file", auth_file);

      MyConfig.SetParamValueBool("http.server", "enable.files", enable_files);
      MyConfig.SetParamValueBool("http.server", "enable.dirlist", enable_dirlist);
      MyConfig.SetParamValueBool("http.server", "auth.global", auth_global);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Webserver configuration saved.</p>"
        "<p>Note: if you changed the TLS certificate or key, you need to reboot"
        " the module to activate the change.</p>");
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
    ws_txqueuesize = MyConfig.GetParamValue("http.server", "ws.txqueuesize");
    docroot = MyConfig.GetParamValue("http.server", "docroot");
    auth_domain = MyConfig.GetParamValue("http.server", "auth.domain");
    auth_file = MyConfig.GetParamValue("http.server", "auth.file");
    enable_files = MyConfig.GetParamValueBool("http.server", "enable.files", true);
    enable_dirlist = MyConfig.GetParamValueBool("http.server", "enable.dirlist", true);
    auth_global = MyConfig.GetParamValueBool("http.server", "auth.global", true);
    load_file("/store/tls/webserver.crt", tls_cert);
    load_file("/store/tls/webserver.key", tls_key);

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Webserver configuration");
  c.form_start(p.uri);

  c.fieldset_start("File Access");

  c.input_checkbox("Enable file access", "enable_files", enable_files,
    "<p>If enabled, paths not handled by the webserver itself are mapped to files below the web root path.</p>"
    "<p>Example: <code>&lt;img src=\"/icons/smiley.png\"&gt;</code> → file <code>/sd/icons/smiley.png</code>"
    " (if root path is <code>/sd</code>)</p>");
  c.input_text("Root path", "docroot", docroot.c_str(), "Default: /sd");
  c.input_checkbox("Enable directory listings", "enable_dirlist", enable_dirlist);

  c.input_checkbox("Enable global file auth", "auth_global", auth_global,
    "<p>If enabled, file access is globally protected by the admin password (if set).</p>"
    "<p>Disabling allows public access to directories without an auth file.</p>"
    "<p>To protect a directory, you can e.g. copy the default auth file:"
    " <code class=\"autoselect\">vfs cp /store/.htpasswd /sd/…/.htpasswd</code></p>");
  c.input_text("Directory auth file", "auth_file", auth_file.c_str(), "Default: .htpasswd",
    "<p>Note: sub directories do <u>not</u> inherit the parent auth file.</p>");
  c.input_text("Auth domain/realm", "auth_domain", auth_domain.c_str(), "Default: ovms");

  c.fieldset_end();

  c.fieldset_start("WebSocket Connection");

  c.input("number", "TX job queue size", "ws_txqueuesize", ws_txqueuesize.c_str(), "10-250, default: 50",
    "<p>The queue buffers metrics updates, events, logs, streaming data etc. to be sent to a connected "
    "browser / websocket client, each client has a separate queue.</p>"
    "<p>If you have many and frequent updates or a slow client connection, you can try raising "
    "the queue size to avoid dropped updates due to job queue overflows.</p>"
    "<p>Note: changes only affect new connections (reload browser window to reconnect).</p>",
    "min=\"10\" max=\"250\" step=\"10\"");

  c.fieldset_end();

  c.fieldset_start("Encryption");

  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">TLS certificate:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN CERTIFICATE-----&#13;&#10;...&#13;&#10;-----END CERTIFICATE-----\"\n"
          "rows=\"5\" id=\"input-tls_cert\" name=\"tls_cert\">%s</textarea>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(tls_cert).c_str());
  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">TLS private key:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN PRIVATE KEY-----&#13;&#10;...&#13;&#10;-----END PRIVATE KEY-----\"\n"
          "rows=\"5\" id=\"input-tls_key\" name=\"tls_key\">%s</textarea>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(tls_key).c_str());

  c.fieldset_end();

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>To enable encryption (https, wss) you need to install a TLS certificate + key. Public"
    " certification authorities (CAs) won't issue certificates for private hosts and IP addresses,"
    " so we recommend to create a self-signed TLS certificate for your module. Use a maximum key"
    " size of 2048 bit for acceptable performance.</p>"
    "<p><u>Example/template using OpenSSL</u>:</p>"
    "<samp class=\"autoselect\">openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \\\n"
    "&nbsp;&nbsp;-keyout ovms.key -out ovms.crt -subj \"/CN=ovmsname.local\" \\\n"
    "&nbsp;&nbsp;-addext \"subjectAltName=IP:192.168.4.1,IP:192.168.2.101\"</samp>"
    "<p>Change the name and add more IPs as needed. The command produces two files in your current"
    " directory, <code>ovms.crt</code> and <code>ovms.key</code>. Copy their contents into the"
    " respective fields above.</p>"
    "<p><u>Note</u>: as this is a self-signed certificate, you will need to explicitly allow the"
    " browser to access the module on the first https connect.</p>");
  c.done();
}


/**
 * HandleCfgWifi: configure wifi networks (URL /cfg/wifi)
 */

void OvmsWebServer::HandleCfgWifi(PageEntry_t& p, PageContext_t& c)
{
  bool cfg_bad_reconnect, cfg_reboot_no_ip, cfg_ap2client_enabled, cfg_ap2client_notify;
  float cfg_sq_good, cfg_sq_bad;
  int cfg_ap2client_timeout;

  if (c.method == "POST") {
    std::string warn, error;

    // process form submission:
    UpdateWifiTable(p, c, "ap", "wifi.ap", warn, error, 8);
    UpdateWifiTable(p, c, "client", "wifi.ssid", warn, error, 0);

    cfg_sq_good           = atof(c.getvar("cfg_sq_good").c_str());
    cfg_sq_bad            = atof(c.getvar("cfg_sq_bad").c_str());
    cfg_bad_reconnect     = (c.getvar("cfg_bad_reconnect") == "yes");
    cfg_reboot_no_ip      = (c.getvar("cfg_reboot_no_ip") == "yes");
    cfg_ap2client_timeout = atof((c.getvar("cfg_ap2client_timeout")).c_str());
    cfg_ap2client_enabled = (c.getvar("cfg_ap2client_enabled") == "yes");
    cfg_ap2client_notify  = (c.getvar("cfg_ap2client_notify") == "yes");

    if (cfg_sq_bad >= cfg_sq_good) {
      error += "<li data-input=\"cfg_sq_bad\">'Bad' signal level must be lower than 'good' level.</li>";
    } else {
      if (cfg_sq_good == -87)
        MyConfig.DeleteInstance("network", "wifi.sq.good");
      else
        MyConfig.SetParamValueFloat("network", "wifi.sq.good", cfg_sq_good);
      if (cfg_sq_bad == -89)
        MyConfig.DeleteInstance("network", "wifi.sq.bad");
      else
        MyConfig.SetParamValueFloat("network", "wifi.sq.bad", cfg_sq_bad);
      if (!cfg_bad_reconnect)
        MyConfig.DeleteInstance("network", "wifi.bad.reconnect");
      else
        MyConfig.SetParamValueBool("network", "wifi.bad.reconnect", cfg_bad_reconnect);
      if (!cfg_reboot_no_ip)
        MyConfig.DeleteInstance("network", "reboot.no.ip");
      else
        MyConfig.SetParamValueBool("network", "reboot.no.ip", cfg_reboot_no_ip);
      if (cfg_ap2client_timeout == 30)    // default is 30 minutes
        MyConfig.DeleteInstance("network", "wifi.ap2client.timeout");
      else
        MyConfig.SetParamValueInt("network", "wifi.ap2client.timeout", cfg_ap2client_timeout);
      if (!cfg_ap2client_enabled)         // default is disabled
        MyConfig.DeleteInstance("network", "wifi.ap2client.enable");
      else
        MyConfig.SetParamValueBool("network", "wifi.ap2client.enable", cfg_ap2client_enabled);
      if (!cfg_ap2client_notify)          // default is disabled
        MyConfig.DeleteInstance("network", "wifi.ap2client.notify");
      else
        MyConfig.SetParamValueBool("network", "wifi.ap2client.notify", cfg_ap2client_notify);
    }

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
    cfg_sq_good             = MyConfig.GetParamValueFloat("network", "wifi.sq.good", -87);
    cfg_sq_bad              = MyConfig.GetParamValueFloat("network", "wifi.sq.bad", -89);
    cfg_bad_reconnect       = MyConfig.GetParamValueBool("network", "wifi.bad.reconnect", false);
    cfg_reboot_no_ip        = MyConfig.GetParamValueBool("network", "reboot.no.ip", false);
    cfg_ap2client_timeout   = MyConfig.GetParamValueInt("network", "wifi.ap2client.timeout", 30);
    cfg_ap2client_enabled   = MyConfig.GetParamValueBool("network", "wifi.ap2client.enable", false);
    cfg_ap2client_notify    = MyConfig.GetParamValueBool("network", "wifi.ap2client.notify", false);
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Wifi configuration");
  c.printf(
    "<form class=\"form-horizontal\" method=\"post\" action=\"%s\" target=\"#main\">"
    , _attr(p.uri));

  c.fieldset_start("Access point networks");
  OutputWifiTable(p, c, "ap", "wifi.ap", MyConfig.GetParamValue("auto", "wifi.ssid.ap", "OVMS"));
  c.fieldset_end();

  c.fieldset_start("Wifi »Access point + client« mode timeout");
  c.input_checkbox("Enable Wifi »Access point + client« mode timeout", "cfg_ap2client_enabled", cfg_ap2client_enabled,
    "<p>Enable = Switch from Wifi AP+client to client only mode when no stations were connected to the AP for the given timeout.</p>");
  c.input_slider("Timeout", "cfg_ap2client_timeout", 3, "min", -1, cfg_ap2client_timeout, 30, 1, 120, 1); 
  c.input_checkbox("Send notification on AP+client mode timeout", "cfg_ap2client_notify", cfg_ap2client_notify);
  c.print(
    "<div class=\"form-group\"><div class=\"col-sm-12\"><div class=\"help-block\">"
      "<p>Shutting down the module's Wifi access point reduces power consumption (see <a target=\"_blank\""
      " href=\"https://docs.openvehicles.com/en/latest/userguide/warnings.html#average-power-usage\">User Guide</a>)"
      " and frees bandwidth for standard (client) connections.</p>"
      "<p>Note: switching the mode implies a client reconnect. To re-enable the AP after a timeout, issue shell command"
      " <kbd>wifi mode apclient</kbd> or <kbd>wifi restart</kbd>, or reboot/restart the module.</p>"
    "</div></div></div>");
  c.fieldset_end();

  c.fieldset_start("Wifi client networks");
  OutputWifiTable(p, c, "client", "wifi.ssid", MyConfig.GetParamValue("auto", "wifi.ssid.client"));
  c.fieldset_end();

  c.fieldset_start("Wifi client options");
  c.input_slider("Good signal level", "cfg_sq_good", 3, "dBm", -1, cfg_sq_good, -87.0, -128.0, 0.0, 0.1,
    "<p>Threshold for usable wifi signal strength</p>");
  c.input_slider("Bad signal level", "cfg_sq_bad", 3, "dBm", -1, cfg_sq_bad, -89.0, -128.0, 0.0, 0.1,
    "<p>Threshold for unusable wifi signal strength</p>");
  c.input_checkbox("Immediate disconnect/reconnect", "cfg_bad_reconnect", cfg_bad_reconnect,
    "<p>Check to immediately look for better access points when signal level gets bad."
    " Default is to stay with the current AP as long as possible.</p>");
  c.input_checkbox("Reboot when no IP is aquired after 5 minutes with a good connection", "cfg_reboot_no_ip", cfg_reboot_no_ip,
    "<p>Reboot device when there is good signal but the connection fails to obtain an IP address after 5 minutes."
    " Takes into consideration both wifi and cellular connections.</p>");
  c.fieldset_end();

  c.print(
    "<hr>"
    "<button type=\"submit\" class=\"btn btn-default center-block\">Save</button>"
    "</form>"
    "<style>\n"
    ".table>tbody>tr.active>td, .table>tbody>tr.active:hover>td {\n"
      "background-color: #bed2e3;\n"
      "cursor: pointer;\n"
    "}\n"
    ".night .table>tbody>tr.active>td {\n"
      "background-color: #293746;\n"
      "cursor: pointer;\n"
    "}\n"
    "</style>\n"
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
            "' + ((pfx == 'client') ? '"
              "<td><div class=\"input-group\">"
                "<input type=\"text\" class=\"form-control\" name=\"' + pfx + '_ssid_' + nr + '\" placeholder=\"SSID\""
                " id=\"' + pfx + '_ssid_' + nr + '\" autocomplete=\"section-wifi-' + pfx + ' username\">"
                "<div class=\"input-group-btn\">"
                  "<button type=\"button\" class=\"btn btn-default action-wifiscan\" data-target=\"#' + pfx + '_ssid_' + nr + '\">"
                    "<span class=\"hidden-xs\">Select</span><span class=\"hidden-sm hidden-md hidden-lg\">⊙</span>"
                  "</button>"
                "</div></div></td>"
            "' : '"
              "<td><input type=\"text\" class=\"form-control\" name=\"' + pfx + '_ssid_' + nr + '\" placeholder=\"SSID\""
                " autocomplete=\"section-wifi-' + pfx + ' username\"></td>"
            "') + '"
            "<td><input type=\"password\" class=\"form-control\" name=\"' + pfx + '_pass_' + nr + '\" placeholder=\"Passphrase\""
              " autocomplete=\"section-wifi-' + pfx + ' current-password\"></td>"
          "</tr>"
        "');"
      "$(el).parent().parent().before(row).prev().find(\"input\").first().focus();"
      "counter.val(nr);"
    "}"
    "$('#panel-wifi-configuration').on('click', '.action-wifiscan', function(){\n"
      "var tgt = $(this).data(\"target\");\n"
      "var $tgt = $(tgt);\n"
      "var ssid_sel = \"\";\n"
      "var $dlg = $('<div id=\"wifiscan-dialog\" />').dialog({\n"
        "size: 'lg',\n"
        "title: 'Select Wifi Network',\n"
        "body:\n"
          "'<p id=\"wifiscan-info\">Scanning, please wait…</p>'+\n"
          "'<table id=\"wifiscan-list\" class=\"table table-condensed table-border table-striped table-hover\">'+\n"
          "'<thead><tr>'+\n"
            "'<th class=\"col-xs-4\">AP SSID</th>'+\n"
            "'<th class=\"col-xs-4 hidden-xs\">MAC Address</th>'+\n"
            "'<th class=\"col-xs-1 text-center\">Chan</th>'+\n"
            "'<th class=\"col-xs-1 text-center\">RSSI</th>'+\n"
            "'<th class=\"col-xs-2 hidden-xs\">Auth</th>'+\n"
          "'</tr></thead><tbody/></table>',\n"
        "buttons: [\n"
          "{ label: 'Cancel' },\n"
          "{ label: 'Select', btnClass: 'primary', action: function(input) { $tgt.val(ssid_sel); } },\n"
        "],\n"
      "});\n"
      "var $tb = $dlg.find('#wifiscan-list > tbody'), $info = $dlg.find('#wifiscan-info');\n"
      "$tb.on(\"click\", \"tr\", function(ev) {\n"
        "var $tr = $(this);\n"
        "$tr.addClass(\"active\").siblings().removeClass(\"active\");\n"
        "ssid_sel = $tr.children().first().text();\n"
        "if (ssid_sel == '<HIDDEN>') ssid_sel = '';\n"
      "}).on(\"dblclick\", \"tr\", function(ev) {\n"
        "$dlg.modal(\"hide\");\n"
        "$tgt.val(ssid_sel);\n"
      "});\n"
      "$dlg.addClass(\"loading\");\n"
      "loadcmd(\"wifi scan -j\").then(function(output) {\n"
        "$dlg.removeClass(\"loading\");\n"
        "var res = JSON.parse(output);\n"
        "if (res.error) {\n"
          "$info.text(res.error);\n"
        "} else if (res.list.length == 0) {\n"
          "$info.text(\"Sorry, no networks found.\");\n"
        "} else {\n"
          "var i, ap;\n"
          "$info.text(\"Available networks sorted by signal strength:\");\n"
          "for (i = 0; i < res.list.length; i++) {\n"
            "ap = res.list[i];\n"
            "$('<tr><td>'+encode_html(ap.ssid)+'</td><td class=\"hidden-xs\">'+ap.bssid+\n"
              "'</td><td class=\"text-center\">'+ap.chan+'</td><td class=\"text-center\">'+ap.rssi+\n"
              "'</td><td class=\"hidden-xs\">'+ap.auth+'</td></tr>').appendTo($tb);\n"
          "}\n"
        "}\n"
      "});\n"
    "});\n"
    "</script>");

  c.panel_end(
    "<p>Note: set the Wifi mode and default networks on the"
    " <a href=\"/cfg/autostart\" target=\"#main\">Autostart configuration page</a>.</p>");
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
            "<th width=\"10%%\"></th>"
            "<th width=\"45%%\">SSID</th>"
            "<th width=\"45%%\">Passphrase</th>"
          "</tr>"
        "</thead>"
        "<tbody>"
    , _attr(prefix), max);

  auto gen_row = [&c,&pos,&pos_autostart,&prefix,&ssid]() {
    if (prefix == "client") {
      // client entry: add network scanner/selector
      c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\" %s><strong>✖</strong></button></td>"
            "<td><div class=\"input-group\">"
              "<input type=\"text\" class=\"form-control\" name=\"%s_ssid_%d\" placeholder=\"SSID\" value=\"%s\""
              " id=\"%s_ssid_%d\" autocomplete=\"section-wifi-%s username\">"
              "<div class=\"input-group-btn\">"
                "<button type=\"button\" class=\"btn btn-default action-wifiscan\" data-target=\"#%s_ssid_%d\">"
                  "<span class=\"hidden-xs\">Select</span><span class=\"hidden-sm hidden-md hidden-lg\">⊙</span>"
                "</button>"
              "</div></div></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"%s_pass_%d\" placeholder=\"no change\"></td>"
          "</tr>"
      , (pos == pos_autostart) ? "disabled title=\"Current autostart network\"" : ""
      , _attr(prefix), pos, _attr(ssid)
      , _attr(prefix), pos, _attr(prefix)
      , _attr(prefix), pos
      , _attr(prefix), pos);
    } else {
      // ap entry:
      c.printf(
          "<tr>"
            "<td><button type=\"button\" class=\"btn btn-danger\" onclick=\"delRow(this)\" %s><strong>✖</strong></button></td>"
            "<td><input type=\"text\" class=\"form-control\" name=\"%s_ssid_%d\" value=\"%s\" autocomplete=\"section-wifi-%s username\"></td>"
            "<td><input type=\"password\" class=\"form-control\" name=\"%s_pass_%d\" placeholder=\"no change\"></td>"
          "</tr>"
      , (pos == pos_autostart) ? "disabled title=\"Current autostart network\"" : ""
      , _attr(prefix), pos, _attr(ssid), _attr(prefix)
      , _attr(prefix), pos);
    }
  };

  if (c.method == "POST") {
    for (pos = 1; pos <= max; pos++) {
      sprintf(buf, "%s_ssid_%d", prefix.c_str(), pos);
      ssid = c.getvar(buf);
      gen_row();
    }
  }
  else {
    for (auto const& kv : param->m_map) {
      pos++;
      ssid = kv.first;
      if (endsWith(ssid, ".ovms.staticip"))
        continue;
      if (ssid == autostart_ssid)
        pos_autostart = pos;
      gen_row();
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
    if (param->IsDefined(ssid + ".ovms.staticip"))
      newmap[ssid + ".ovms.staticip"] = param->GetValue(ssid + ".ovms.staticip");
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


#ifdef CONFIG_OVMS_COMP_OTA
/**
 * HandleCfgFirmware: OTA firmware update & boot setup (URL /cfg/firmware)
 */
void OvmsWebServer::HandleCfgFirmware(PageEntry_t& p, PageContext_t& c)
{
  std::string cmdres, mru;
  std::string action;
  ota_info info;
  bool auto_enable, auto_allow_modem;
  std::string auto_hour, server, tag, hardware;
  std::string output;
  std::string version;
  const char *what;
  char buf[132];

  if (c.method == "POST") {
    // process form submission:
    bool error = false, showform = true, reboot = false;
    action = c.getvar("action");

    auto_enable = (c.getvar("auto_enable") == "yes");
    auto_allow_modem = (c.getvar("auto_allow_modem") == "yes");
    auto_hour = c.getvar("auto_hour");
    server = c.getvar("server");
    tag = c.getvar("tag");
    hardware = GetOVMSProduct();

    if (action.substr(0,3) == "set") {
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
        MyConfig.SetParamValueBool("ota", "auto.allow.modem", auto_allow_modem);
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
    auto_allow_modem = MyConfig.GetParamValueBool("ota", "auto.allow.modem", false);
    auto_hour = MyConfig.GetParamValue("ota", "auto.hour", "2");
    server = MyConfig.GetParamValue("ota", "server");
    tag = MyConfig.GetParamValue("ota", "tag");
    hardware = GetOVMSProduct();
    // generate form:
    c.head(200);
  }

  // read status:
  MyOTA.GetStatus(info);

  c.panel_start("primary", "Firmware setup &amp; update");

  c.input_info("Firmware version", info.version_firmware.c_str());
  output = info.version_server;
  output.append(" <button type=\"button\" class=\"btn btn-default\" data-toggle=\"modal\" data-target=\"#version-dialog\">Version info</button>"
                " <button type=\"button\" class=\"btn btn-default action-update-now\">Update now</button>");
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
    "<p>Strongly recommended: if enabled, the module will perform automatic firmware updates within the hour of day specified.</p>");
  c.input("number", "Auto update hour of day", "auto_hour", auto_hour.c_str(), "0-23, default: 2", NULL, "min=\"0\" max=\"23\" step=\"1\"");
  c.input_checkbox("…allow via modem", "auto_allow_modem", auto_allow_modem,
    "<p>Automatic updates are normally only done if a wifi connection is available at the time. Before allowing updates via modem, be aware a single firmware image has a size of around 3 MB, which may lead to additional costs on your data plan.</p>");
  c.print(
    "<datalist id=\"server-list\">"
      "<option value=\"https://api.openvehicles.com/firmware/ota\">"
      "<option value=\"https://ovms.dexters-web.de/firmware/ota\">"
    "</datalist>"
    "<datalist id=\"tag-list\">"
      "<option value=\"main\">"
      "<option value=\"eap\">"
      "<option value=\"edge\">"
    "</datalist>"
    );
  c.input_text("Update server", "server", server.c_str(), "Specify or select from list (clear to see all options)",
    "<p>Default is <code>https://api.openvehicles.com/firmware/ota</code>.</p>",
    "list=\"server-list\"");
  c.input_text("Version tag", "tag", tag.c_str(), "Specify or select from list (clear to see all options)",
    "<p>Default is <code>main</code> for standard releases. Use <code>eap</code> (early access program) for stable or <code>edge</code> for bleeding edge developer builds.</p>",
    "list=\"tag-list\"");
  c.input_info("Hardware version", hardware.c_str());

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
        "<p>The module is running in wifi AP mode without cellular modem, so flashing from a public server is currently not possible.</p>"
        "<p>You can still flash from an AP network local IP address (<code>192.168.4.x</code>).</p>");
    }
    else if (netif_default->name[0] == 'p' && netif_default->name[1] == 'p') {
      c.alert("warning",
        "<p class=\"lead\"><strong>Using cellular modem connection for internet.</strong></p>"
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
      "$(\".action-update-now\").on(\"click\", function(ev){"
        "var server = $(\"input[name=server]\").val() || \"https://api.openvehicles.com/firmware/ota\";"
        "var tag = $(\"input[name=tag]\").val() || \"main\";"
        "var hardware = \"" + hardware + "\";"
        "var url = server.replace(/\\/$/, \"\") + \"/\" + hardware + \"/\" + tag + \"/ovms3.bin\";"
        "$(\"#output\").text(\"Processing… (do not interrupt, may take some minutes)\\n\");"
        "setloading(\"#flash-dialog\", true);"
        "$(\"#flash-dialog\").modal(\"show\");"
        "loadcmd(\"ota flash http \" + url, \"+#output\").done(function(resp){"
          "setloading(\"#flash-dialog\", false);"
        "});"
        "ev.stopPropagation();"
        "return false;"
      "});"
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
    if (c.getvar("file_syncperiod") != "")
      pmap["file.syncperiod"] = c.getvar("file_syncperiod");

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
        download += " <a class=\"btn btn-default\" target=\"_blank\" href=\"" + webdir + "/#sc=0&amp;so=1\">Open directory</a>";
    }
  } else {
    download = "You can access your logs with the browser if the path is in your webserver root (" + docroot + ").";
  }
  c.input_info("Download", download.c_str());

  c.input("number", "Sync period", "file_syncperiod", pmap["file.syncperiod"].c_str(), "Default: 3",
    "<p>How often to flush log buffer to SD: 0 = never/auto, &lt;0 = every n messages, &gt;0 = after n/2 seconds idle</p>",
    "min=\"-1\" step=\"1\"");
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
  std::string flatbed_dist, flatbed_time, valet_dist, valet_time;

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
    flatbed_dist = c.getvar("flatbed.dist");
    flatbed_time = c.getvar("flatbed.interval");
    valet_dist = c.getvar("valet.dist");
    valet_time = c.getvar("valet.interval");
    if (error == "") {
      // save:
      param->m_map.clear();
      param->m_map = std::move(pmap);
      param->Save();

      MyConfig.SetParamValue("vehicle", "flatbed.alarmdistance", flatbed_dist);
      MyConfig.SetParamValue("vehicle", "flatbed.alarminterval", flatbed_time);
      MyConfig.SetParamValue("vehicle", "valet.alarmdistance", valet_dist);
      MyConfig.SetParamValue("vehicle", "valet.alarminterval", valet_time);

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

    flatbed_dist = MyConfig.GetParamValue("vehicle", "flatbed.alarmdistance");
    flatbed_time = MyConfig.GetParamValue("vehicle", "flatbed.alarminterval");
    valet_dist   = MyConfig.GetParamValue("vehicle", "valet.alarmdistance");
    valet_time   = MyConfig.GetParamValue("vehicle", "valet.alarminterval");

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
    "<ul class=\"nav nav-tabs\">"
      "<li class=\"active\"><a data-toggle=\"tab\" href=\"#tab-locations\">User Locations</a></li>"
      "<li><a data-toggle=\"tab\" href=\"#tab-system-locations\">System Locations</a></li>"
    "</ul>"
    "<div class=\"tab-content\">"
      "<div id=\"tab-locations\" class=\"tab-pane fade in active section-locations\">");



  c.print(
    "<div class=\"table-responsive list-editor receiver\" id=\"loced\">"
      "<table class=\"table form-table\">"
        "<colgroup>"
          "<col style=\"width:10%\">"
          "<col style=\"width:90%\">"
        "</colgroup>"
        "<template>"
          "<tr class=\"list-item mode-ITEM_MODE\">\n"
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
              "<div class=\"form-group\">\n"
                "<label class=\"control-label col-sm-2\">Scripts:</label>\n"
                "<div class=\"col-sm-10\">\n"
                  "<button type=\"button\" class=\"btn btn-default edit-scripts\" data-edit=\"location.enter.{name}\">Entering</button>\n"
                  "<button type=\"button\" class=\"btn btn-default edit-scripts\" data-edit=\"location.leave.{name}\">Leaving</button>\n"
                "</div>\n"
              "</div>\n"
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
    "</div>\n"
  );

  c.print(
      "</div>"
      "<div id=\"tab-system-locations\" class=\"tab-pane fade section-system-locations\">");

  c.input("number", "Flatbed Alert Distance", "flatbed.dist", flatbed_dist.c_str(), "Default: 500 m",
      "<p>Distance from the parked location that the car needs to have moved before a 'flat-bed' alert is raised, 0 = disable flatbed alerts.</p>",
      "min=\"0\" step=\"1\"", "m");
  c.input("number", "Flatbed Alert Interval", "flatbed.interval", flatbed_time.c_str(), "Default: 15 min",
      "Interval between repeated flatbed alerts, 0 = disable alert repetition",
      "min=\"1\" step=\"1\"", "min");

  c.input("number", "Valet Alert Distance", "valet.dist", valet_dist.c_str(), "Default: 0 (disabled)",
      "<p>Distance from the location Valet Mode was enabled that the car needs to have travelled before an alert is raised, 0 = disable valet alerts.</p>",
      "min=\"0\" step=\"1\"", "m");
  c.input("number", "Valet Alert Interval", "valet.interval", valet_time.c_str(), "Default: 15 min",
      "Interval between repeated valet alerts, 0 = disable alert repetition",
      "min=\"1\" step=\"1\"", "min");

  c.print(
    "</div>\n"
    "<br/>\n"
    "<div class=\"form-group\">\n"
    "<div class=\"text-center\">\n"
      "<button type=\"reset\" class=\"btn btn-default\">Reset</button>\n"
      "<button type=\"submit\" class=\"btn btn-primary\">Save</button>\n"
    "</div></div>\n"
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
        "$('#loced button.list-item-add').data('preset', preset);"
      "}"
    "}).trigger('msg:metrics', metrics);"
    "$('#loced').on('click', 'button.edit-scripts', function(evt) {\n"
      "var $this = $(this), $tr = $this.closest('tr');\n"
      "var name = $tr.find('input[name^=\"name_\"]').val();\n"
      "var dir = '/store/events/' + $this.data('edit').replace(\"{name}\", name) + '/';\n"
      "var changed = ($('#loced').find('.mode-add').length != 0);\n"
      "if (!changed)\n"
        "$('#loced input').each(function() { changed = changed || ($(this).val() != $(this).attr(\"value\")); });\n"
      "if (name == \"\") {\n"
        "confirmdialog(\"Error\", \"<p>Scripts are bound to the location name, please specify it.</p>\", [\"OK\"]);\n"
      "} else if (!changed) {\n"
        "loaduri('#main', 'get', '/edit', { \"path\": dir });\n"
      "} else {\n"
        "confirmdialog(\"Discard changes?\", \"Loading the editor will discard your list changes.\", [\"Cancel\", \"OK\"], function(ok) {\n"
          "if (ok) loaduri('#main', 'get', '/edit', { \"path\": dir });\n"
        "});\n"
      "}\n"
    "});\n"
  );

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
      "var suggest = '/sd/backup/cfg-' + new Date().toISOString().substr(2,8).replaceAll('-','') + '.zip';\n"
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
            "loadcmd(\"config backup \" + zip.path + ((password) ? \" \" + password : \"\"), \"#log\", 120).done(function(output) {\n"
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
            "}, 120).always(function(data, status) {\n"
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
  std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    auto size = file.tellg();
    if (size > 0) {
      content.resize(size, '\0');
      file.seekg(0);
      file.read(&content[0], size);
    }
  }

  c.printf(
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
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-wrap\" title=\"Wrap long lines\">⇌</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-smaller\">&minus;</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-larger\">&plus;</button>\n"
            "</div>\n"
            "<textarea class=\"form-control fullwidth font-monospace\" rows=\"20\"\n"
              "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
              "id=\"input-content\" name=\"content\">%s</textarea>\n"
          "</div>\n"
          , c.encode_html(content).c_str());

  c.print(
          "<div class=\"text-center\">\n"
            "<button type=\"reset\" class=\"btn btn-default\">Reset</button>\n"
            "<button type=\"button\" class=\"btn btn-default action-cancel\">Cancel</button>\n"
            "<button type=\"submit\" class=\"btn btn-primary action-save\">Save</button>\n"
          "</div>\n"
        "</form>\n"
      "</div>\n"
    "</div>\n"
    "<script>\n"
    "$('.action-cancel').on('click', function(ev) {\n"
      "loaduri('#main', 'get', '/cfg/plugins');\n"
    "});\n"
    "/* textarea controls */\n"
    "$('.tac-wrap').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "$this.toggleClass(\"active\");\n"
      "$ta.css(\"white-space\", $this.hasClass(\"active\") ? \"pre-wrap\" : \"pre\");\n"
      "if (!supportsTouch) $ta.focus();\n"
    "});\n"
    "$('.tac-smaller').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "var fs = parseInt($ta.css(\"font-size\"));\n"
      "$ta.css(\"font-size\", (fs-1)+\"px\");\n"
      "if (!supportsTouch) $ta.focus();\n"
    "});\n"
    "$('.tac-larger').on('click', function(ev) {\n"
      "var $this = $(this), $ta = $this.parent().next();\n"
      "var fs = parseInt($ta.css(\"font-size\"));\n"
      "$ta.css(\"font-size\", (fs+1)+\"px\");\n"
      "if (!supportsTouch) $ta.focus();\n"
    "});\n"
    "/* remember textarea config: */\n"
    "window.prefs = $.extend({ plugineditor: { height: '300px', wrap: false, fontsize: '13px' } }, window.prefs);\n"
    "$('#input-content').css(\"height\", prefs.plugineditor.height).css(\"font-size\", prefs.plugineditor.fontsize);\n"
    "if (prefs.plugineditor.wrap) $('.tac-wrap').trigger('click');\n"
    "$('#input-content, .textarea-control').on('click', function(ev) {\n"
      "$ta = $('#input-content');\n"
      "prefs.plugineditor.height = $ta.css(\"height\");\n"
      "prefs.plugineditor.fontsize = $ta.css(\"font-size\");\n"
      "prefs.plugineditor.wrap = $('.tac-wrap').hasClass(\"active\");\n"
    "});\n"
    "</script>\n"
    );
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
  content = stripcr(content);
  mkpath("/store/plugin");
  std::string path = "/store/plugin/" + key;
  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (file.is_open()) {
    file.write(content.data(), content.size());
  }
  if (file.fail()) {
    error += "<li>Error writing to <code>" + c.encode_html(path) + "</code>: " + strerror(errno) + "</li>";
    return false;
  }

  return true;
}

void OvmsWebServer::HandleCfgPlugins(PageEntry_t& p, PageContext_t& c)
{
  std::string cnt = c.getvar("cnt");
  std::string key = c.getvar("key");
  std::string error, info;

  if (c.method == "POST") {
    if (cnt != "") {
      if (SavePluginList(p, c, error)) {
        info = "<p class=\"lead\">Plugin registration saved.</p>"
          "<script>after(0.5, reloadmenu)</script>";
      }
    }
    else if (key != "") {
      if (SavePluginEditor(p, c, error)) {
        info = "<p class=\"lead\">Plugin <code>" + c.encode_html(key) + "</code> saved.</p>"
          "<script>after(0.5, reloadmenu)</script>";
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


/**
 * HandleEditor: simple text file editor
 */
void OvmsWebServer::HandleEditor(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  std::string path = c.getvar("path");
  extram::string content;

  if (MyConfig.ProtectedPath(path)) {
    c.head(400);
    c.alert("danger", "<p class=\"lead\">Error: protected path</p>");
    c.done();
    return;
  }

  if (c.method == "POST")
  {
    bool got_content = c.getvar("content", content);
    content = stripcr(content);

    if (path == "" || path.front() != '/' || path.back() == '/') {
      error += "<li>Missing or invalid path</li>";
    }
    else if (!got_content) {
      error += "<li>Missing content</li>";
    }
    else {
      // create path:
      size_t n = path.rfind('/');
      if (n != 0 && n != std::string::npos) {
        std::string dir = path.substr(0, n);
        if (!path_exists(dir)) {
          if (mkpath(dir) != 0)
            error += "<li>Error creating path <code>" + c.encode_html(dir) + "</code>: " + strerror(errno) + "</li>";
          else
            info += "<p class=\"lead\">Path <code>" + c.encode_html(dir) + "</code> created.</p>";
        }
      }
      // write file:
      if (error == "") {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (file.is_open())
          file.write(content.data(), content.size());
        if (file.fail()) {
          error += "<li>Error writing to <code>" + c.encode_html(path) + "</code>: " + strerror(errno) + "</li>";
        } else {
          info += "<p class=\"lead\">File <code>" + c.encode_html(path) + "</code> saved.</p>";
          MyEvents.SignalEvent("system.vfs.file.changed", (void*)path.c_str(), path.size()+1);
        }
      }
    }
  }
  else
  {
    if (path == "") {
      path = "/store/";
    } else if (path.back() != '/') {
      // read file:
      std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
      if (file.is_open()) {
        auto size = file.tellg();
        if (size > 0) {
          content.resize(size, '\0');
          file.seekg(0);
          file.read(&content[0], size);
        }
      }
    }
  }

  // output:
  if (error != "") {
    error = "<p class=\"lead\">Error:</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  } else {
    c.head(200);
    if (info != "")
      c.alert("success", info.c_str());
  }

  c.printf(
    "<style>\n"
    "#input-content {\n"
      "resize: vertical;\n"
    "}\n"
    "#output {\n"
      "height: 200px;\n"
      "resize: vertical;\n"
      "white-space: pre-wrap;\n"
    "}\n"
    ".action-group > div > div {\n"
      "margin-bottom: 10px;\n"
    "}\n"
    "@media (max-width: 767px) {\n"
      ".action-group > div > div {\n"
        "text-align: center !important;\n"
      "}\n"
    "}\n"
    ".log { font-size: 87%%; color: gray; }\n"
    ".log.log-I { color: green; }\n"
    ".log.log-W { color: darkorange; }\n"
    ".log.log-E { color: red; }\n"
    "</style>\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\">Text Editor</div>\n"
      "<div class=\"panel-body\">\n"
        "<form method=\"post\" action=\"%s\" target=\"#main\">\n"
          "<div class=\"form-group\">\n"
            "<div class=\"flex-group\">\n"
              "<button type=\"button\" class=\"btn btn-default action-open\" accesskey=\"O\"><u>O</u>pen…</button>\n"
              "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"File path\"\n"
                "name=\"path\" id=\"input-path\" value=\"%s\" autocapitalize=\"none\" autocorrect=\"off\"\n"
                "autocomplete=\"off\" spellcheck=\"false\">\n"
            "</div>\n"
          "</div>\n"
    , _attr(p.uri), _attr(path));

#ifdef CONFIG_OVMS_COMP_OBD2ECU
  bool isECUEnabled = MyPeripherals->m_obd2ecu != nullptr;
#else
  bool isECUEnabled = false;
#endif
  c.printf(
          "<div class=\"form-group\">\n"
            "<div class=\"textarea-control pull-right\">\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-wrap\" title=\"Wrap lines\">⇌</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-smaller\">&minus;</button>\n"
              "<button type=\"button\" class=\"btn btn-sm btn-default tac-larger\">&plus;</button>\n"
            "</div>\n"
            "<textarea class=\"form-control fullwidth font-monospace\" rows=\"20\"\n"
              "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
              "id=\"input-content\" name=\"content\">%s</textarea>\n"
          "</div>\n"
          "<div class=\"row action-group\">\n"
            "<div class=\"col-sm-6\">\n"
              "<div class=\"text-left\">\n"
                "<button type=\"button\" class=\"btn btn-default action-script-eval\" accesskey=\"V\">E<u>v</u>aluate JS</button>\n"
                "<button type=\"button\" class=\"btn btn-default action-script-reload\">Reload JS Engine</button>\n"
                "%s"
              "</div>\n"
            "</div>\n"
            "<div class=\"col-sm-6\">\n"
              "<div class=\"text-right\">\n"
                "<button type=\"reset\" class=\"btn btn-default\">Reset</button>\n"
                "<button type=\"button\" class=\"btn btn-default action-reload\">Reload</button>\n"
                "<button type=\"button\" class=\"btn btn-default action-saveas\">Save as…</button>\n"
                "<button type=\"button\" class=\"btn btn-primary action-save\" accesskey=\"S\"><u>S</u>ave</button>\n"
              "</div>\n"
            "</div>\n"
          "</div>\n"
        "</form>\n"
        "<div class=\"filedialog\" id=\"fileselect\" />\n"
        "<pre class=\"receiver\" id=\"output\" style=\"display:none\" />\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<p>Hints: you don't need to save to evaluate Javascript code. See <a target=\"_blank\"\n"
          "href=\"https://docs.openvehicles.com/en/latest/userguide/scripting.html#testing-javascript-modules\"\n"
          ">user guide</a> on how to test a module lib plugin w/o saving and reloading.\n"
          "Use a second session to test a web plugin.</p>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(content).c_str()
    , isECUEnabled ? "<button type=\"button\" class=\"btn btn-default action-script-ecu\">Reload ECU Settings</button>\n" : ""
    );

  c.printf(
    "<script>\n"
    "(function(){\n"
      "var $ta = $('#input-content'), $output = $('#output');\n"
      "var path = $('#input-path').val();\n"
      "var quicknav = ['/sd/', '/store/', '/store/scripts/', '/store/plugin/'];\n"
      "var dir = path.replace(/[^/]*$/, '');\n"
      "if (dir && dir.length > 1 && quicknav.indexOf(dir) < 0)\n"
        "quicknav.push(dir);\n"
      "$(\"#fileselect\").filedialog({\n"
        "\"path\": path,\n"
        "\"quicknav\": quicknav,\n"
      "});\n"
      "$('#input-path').on('keydown', function(ev) {\n"
        "if (ev.which == 13) {\n"
          "ev.preventDefault();\n"
          "return false;\n"
        "}\n"
      "});\n"
      "$('.action-open').on('click', function() {\n"
        "$(\"#fileselect\").filedialog('show', {\n"
          "title: \"Load File\",\n"
          "submit: \"Load\",\n"
          "onSubmit: function(input) {\n"
            "if (input.file)\n"
              "loaduri(\"#main\", \"get\", page.path, { \"path\": input.path });\n"
          "},\n"
        "});\n"
      "});\n"
      "$('.action-saveas').on('click', function() {\n"
        "$(\"#fileselect\").filedialog('show', {\n"
          "title: \"Save File\",\n"
          "submit: \"Save\",\n"
          "onSubmit: function(input) {\n"
            "if (input.file) {\n"
              "$('#input-path').val(input.path);\n"
              "$('form').submit();\n"
            "}\n"
          "},\n"
        "});\n"
      "});\n"
      "$('.action-save').on('click', function() {\n"
        "path = $('#input-path').val();\n"
        "if (path == '' || path.endsWith('/'))\n"
          "$('.action-saveas').click();\n"
        "else\n"
          "$('form').submit();\n"
      "});\n"
      "$('.action-reload').on('click', function() {\n"
        "path = $('#input-path').val();\n"
        "if (path)\n"
          "loaduri(\"#main\", \"get\", page.path, { \"path\": path });\n"
      "});\n"
      "// Reload Javascript engine:\n"
      "$('.action-script-reload').on('click', function() {\n"
        "$('.panel').addClass('loading');\n"
        "$ta.focus();\n"
        "$output.empty().show();\n"
        "loadcmd('script reload', '+#output').then(function(){\n"
          "$('.panel').removeClass('loading');\n"
        "});\n"
      "});\n"
      "%s"
      "// Utility: select textarea line\n"
      "function selectLine(line) {\n"
        "var ta = $ta.get(0);\n"
        "// find line:\n"
        "var txt = ta.value;\n"
        "var lines = txt.split('\\n');\n"
        "if (!lines || lines.length < line) return;\n"
        "var start = 0, end, i;\n"
        "for (i = 0; i < line-1; i++)\n"
          "start += lines[i].length + 1;\n"
        "end = start + lines[i].length;\n"
        "// select & show line:\n"
        "ta.focus();\n"
        "ta.value = txt.substr(0, end);\n"
        "ta.scrollTop = ta.scrollHeight;\n"
        "ta.value = txt;\n"
        "ta.setSelectionRange(start, end);\n"
      "}\n"
      "// Evaluate input as Javascript:\n"
      "$('.action-script-eval').on('click', function() {\n"
        "$('.panel').addClass('loading');\n"
        "$ta.focus();\n"
        "$output.empty().show();\n"
        "loadcmd({ command: $ta.val(), type: 'js' }, '+#output').then(function(output){\n"
          "$('.panel').removeClass('loading');\n"
          "if (output == \"\") {\n"
            "$output.append(\"(OK, no output)\");\n"
            "return;\n"
          "}\n"
          "var hasLine = output.match(/\\(line ([0-9]+)\\)/);\n"
          "if (hasLine && hasLine.length >= 2)\n"
            "selectLine(hasLine[1]);\n"
        "});\n"
      "});\n"
      "$output.on(\"msg:log\", function(ev, msg){\n"
        "if ($output.css(\"display\") == \"none\") return;\n"
        "if (!/^..[0-9()]+ (script|ovms-duk)/.test(msg)) return;\n"
        "var autoscroll = ($output.get(0).scrollTop + $output.innerHeight()) >= $output.get(0).scrollHeight;\n"
        "htmsg = '<div class=\"log log-'+msg[0]+'\">'+encode_html(unwrapLogLine(msg))+'</div>';\n"
        "$output.append(htmsg);\n"
        "if (autoscroll) $output.scrollTop($output.get(0).scrollHeight);\n"
      "});\n"
      "/* textarea controls */\n"
      "$('.tac-wrap').on('click', function(ev) {\n"
        "var $this = $(this);\n"
        "$this.toggleClass(\"active\");\n"
        "$ta.css(\"white-space\", $this.hasClass(\"active\") ? \"pre-wrap\" : \"pre\");\n"
        "if (!supportsTouch) $ta.focus();\n"
      "});\n"
      "$('.tac-smaller').on('click', function(ev) {\n"
        "var $this = $(this);\n"
        "var fs = parseInt($ta.css(\"font-size\"));\n"
        "$ta.css(\"font-size\", (fs-1)+\"px\");\n"
        "if (!supportsTouch) $ta.focus();\n"
      "});\n"
      "$('.tac-larger').on('click', function(ev) {\n"
        "var $this = $(this);\n"
        "var fs = parseInt($ta.css(\"font-size\"));\n"
        "$ta.css(\"font-size\", (fs+1)+\"px\");\n"
        "if (!supportsTouch) $ta.focus();\n"
      "});\n"
      "/* remember textarea config: */\n"
      "window.prefs = $.extend({ texteditor: { height: '300px', wrap: false, fontsize: '13px' } }, window.prefs);\n"
      "$('#input-content').css(\"height\", prefs.texteditor.height).css(\"font-size\", prefs.texteditor.fontsize);\n"
      "if (prefs.texteditor.wrap) $('.tac-wrap').trigger('click');\n"
      "$('#input-content, .textarea-control').on('click', function(ev) {\n"
        "prefs.texteditor.height = $ta.css(\"height\");\n"
        "prefs.texteditor.fontsize = $ta.css(\"font-size\");\n"
        "prefs.texteditor.wrap = $('.tac-wrap').hasClass(\"active\");\n"
      "});\n"
      "/* auto open file dialog: */\n"
      "if (path == dir && $('#input-content').val() == '')\n"
        "$('.action-open').trigger('click');\n"
    "})();\n"
    "</script>\n"
     ,( !isECUEnabled ? "" : "// Reload ECU mappings:\n"
      "$('.action-script-ecu').on('click', function() {\n"
        "$('.panel').addClass('loading');\n"
        "$ta.focus();\n"
        "$output.empty().show();\n"
        "loadcmd('obdii ecu reload', '+#output').then(function(){\n"
          "$('.panel').removeClass('loading');\n"
        "});\n"
      "});\n")
    );

  c.done();
}


/**
 * HandleFile: file load/save API
 *
 *  URL: /api/file
 *
 *  @param method
 *    GET   = load content from path
 *    POST  = save content to path
 *  @param path
 *    Full path to file
 *  @param content
 *    File content for POST
 *
 *  @return
 *    Status: 200 (OK) / 400 (Error)
 *    Body: GET: file content or error message, POST: empty or error message
 */
void OvmsWebServer::HandleFile(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string path = c.getvar("path");
  extram::string content;

  std::string headers =
    "Content-Type: application/octet-stream; charset=utf-8\r\n"
    "Cache-Control: no-cache";

  if (MyConfig.ProtectedPath(path)) {
    c.head(400, headers.c_str());
    c.print("ERROR: Protected path\n");
    c.done();
    return;
  }

  if (c.method == "POST")
  {
    bool got_content = c.getvar("content", content);

    if (path == "" || path.front() != '/' || path.back() == '/') {
      error += "; Missing or invalid path";
    }
    else if (!got_content) {
      error += "; Missing content";
    }
    else {
      // write file:
      if (save_file(path, content) != 0) {
        error += "; Error writing to path: ";
        error += strerror(errno);
      }
    }
  }
  else
  {
    if (path == "") {
      path = "/store/";
    } else if (path.back() != '/') {
      // read file:
      if (load_file(path, content) != 0) {
        error += "; Error reading from path: ";
        error += strerror(errno);
      }
    }
  }

  // output:
  if (!error.empty()) {
    c.head(400, headers.c_str());
    c.print("ERROR: ");
    c.print(error.substr(2)); // skip "; " intro
    c.print("\n");
  } else {
    c.head(200);
    if (c.method == "GET") {
      c.print(content);
    }
  }

  c.done();
}
