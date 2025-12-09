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

  output = ExecuteCommand("tpms map get");
  // Show TPMS panel only if we have actual data (not just "0,-1" empty sections)
  // Format: wheel_count,names...,pressure_count,data,validity,...
  // Empty example: 4,FL,FR,RL,RR,0,-1,0,-1,0,-1,0,-1,0,-1 (all data sections are 0,-1)
  if (!output.empty() &&
      output.find(",0,-1,0,-1,0,-1,0,-1,") == std::string::npos)
    {
    c.print(
      "</div>"
      "<div class=\"col-sm-6 col-lg-4\">");
    c.panel_start("primary", "TPMS");
    output = ExecuteCommand("tpms status");
    c.printf("<samp class=\"monitor\" data-updcmd=\"tpms status\" data-events=\"\\.tpms\\.\">%s</samp>", _html(output));
    c.panel_end();
    }

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
