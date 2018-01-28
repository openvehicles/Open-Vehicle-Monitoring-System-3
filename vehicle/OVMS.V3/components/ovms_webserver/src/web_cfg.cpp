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

// #include "ovms_log.h"
// static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleStatus: show status overview
 */
void OvmsWebServer::HandleStatus(PageEntry_t& p, PageContext_t& c)
{
  std::string output;
  
  c.head(200);
  
  mg_printf_http_chunk(c.nc, "<div class=\"row\"><div class=\"col-md-6\">");
  
  c.panel_start("primary", "Vehicle Status");
  output = ExecuteCommand("stat");
  mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  output = ExecuteCommand("location status");
  mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  c.panel_end();
  
  mg_printf_http_chunk(c.nc, "</div><div class=\"col-md-6\">");

  c.panel_start("primary", "Server Status");
  output = ExecuteCommand("server v2 status");
  if (!startsWith(output, "Unrecognised"))
    mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  output = ExecuteCommand("server v3 status");
  if (!startsWith(output, "Unrecognised"))
    mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  c.panel_end();
  
  mg_printf_http_chunk(c.nc, "</div></div>");
  mg_printf_http_chunk(c.nc, "<div class=\"row\"><div class=\"col-md-6\">");
  
  c.panel_start("primary", "Wifi Status");
  output = ExecuteCommand("wifi status");
  mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  c.panel_end();
  
  mg_printf_http_chunk(c.nc, "</div><div class=\"col-md-6\">");
  
  c.panel_start("primary", "Modem Status");
  output = ExecuteCommand("simcom status");
  mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  c.panel_end();
  
  mg_printf_http_chunk(c.nc, "</div></div>");
  mg_printf_http_chunk(c.nc, "<div class=\"row\"><div class=\"col-md-12\">");
  
  c.panel_start("primary", "Firmware Status");
  output = ExecuteCommand("ota status");
  mg_printf_http_chunk(c.nc, "<samp>%s</samp>", _html(output));
  c.panel_end();
  
  mg_printf_http_chunk(c.nc, "</div></div");
  
  c.done();
}


/**
 * HandleCommand: execute command, send output (plain text)
 */
void OvmsWebServer::HandleCommand(PageEntry_t& p, PageContext_t& c)
{
  std::string command = c.getvar("command");
  std::string output = ExecuteCommand(command);
  c.head(200,
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Cache-Control: no-cache");
  c.print(output);
  c.done();
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
  
  mg_printf_http_chunk(c.nc,
    "<pre id=\"output\">%s</pre>"
    "<form id=\"shellform\" method=\"post\" action=\"#\">"
      "<div class=\"input-group\">"
        "<label class=\"input-group-addon hidden-xs\" for=\"input-command\">OVMS&nbsp;&gt;&nbsp;</label>"
        "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"Enter command\" name=\"command\" id=\"input-command\" value=\"%s\">"
        "<div class=\"input-group-btn\">"
          "<button type=\"submit\" class=\"btn btn-default\">Execute</button>"
        "</div>"
      "</div>"
    "</form>"
    "<script>"
    "$(window).on(\"resize\", function(){"
      "$(\"#output\").height($(window).height()-220);"
      "$(\"#output\").scrollTop($(\"#output\").get(0).scrollHeight);"
    "}).trigger(\"resize\");"
    "$(\"#shellform\").on(\"submit\", function(){"
      "var data = $(this).serialize();"
      "var command = $(\"#input-command\").val();"
      "var output = $(\"#output\");"
      "$.ajax({ \"type\": \"post\", \"url\": \"/api/execute\", \"data\": data,"
        "\"timeout\": 5000,"
        "\"beforeSend\": function(){"
          "$(\"html\").addClass(\"loading\");"
          "output.html(output.html() + \"<strong>OVMS&nbsp;&gt;&nbsp;</strong><kbd>\""
            "+ $(\"<div/>\").text(command).html() + \"</kbd><br>\");"
          "output.scrollTop(output.get(0).scrollHeight);"
        "},"
        "\"complete\": function(){"
          "$(\"html\").removeClass(\"loading\");"
        "},"
        "\"success\": function(response){"
          "output.html(output.html() + $(\"<div/>\").text(response).html());"
          "output.scrollTop(output.get(0).scrollHeight);"
        "},"
        "\"error\": function(response, status, httperror){"
          "var resptext = response.responseText || httperror || status;"
          "output.html(output.html() + $(\"<div/>\").text(resptext).html());"
          "output.scrollTop(output.get(0).scrollHeight);"
        "},"
      "});"
      "event.stopPropagation();"
      "return false;"
    "});"
    "$(\"#input-command\").focus();"
    "</script>"
    , _html(output.c_str()), _attr(command.c_str()));
  
  c.panel_end();
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
    if (newpass1 == "")
      error += "<li data-input=\"newpass1\">New password may not be empty</li>";
    if (newpass2 != newpass1)
      error += "<li data-input=\"newpass2\">Passwords do not match</li>";
    
    if (error == "") {
      // success:
      MyConfig.SetParamValue("password", "module", newpass1);
      info += "<li>New module &amp; admin password has been set.</li>";
      
      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      c.head(200);
      c.alert("success", info.c_str());
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
  c.panel_start("primary", "Change module &amp; admin password");
  c.form_start(p.uri);
  c.input_password("Old password", "oldpass", "");
  c.input_password("New password", "newpass1", "");
  c.input_password("…repeat", "newpass2", "");
  c.input_button("default", "Submit");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleCfgVehicle: configure vehicle type & identity (URL: /cfg/vehicle)
 */
void OvmsWebServer::HandleCfgVehicle(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  std::string vehicleid, vehicletype;
  
  if (c.method == "POST") {
    // process form submission:
    vehicleid = c.getvar("vehicleid");
    vehicletype = c.getvar("vehicletype");
    
    if (vehicleid.length() == 0)
      error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
    if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
      error += "<li data-input=\"vehicleid\">Vehicle ID may only contain upper case letters and digits</li>";
    
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
      MyConfig.SetParamValue("vehicle", "type", vehicletype);
      
      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      info += "<script>$(\"#menu\").load(\"/menu\")</script>";
      c.head(200);
      c.alert("success", info.c_str());
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
    vehicletype = MyConfig.GetParamValue("vehicle", "type");
    c.head(200);
  }
  
  // generate form:
  c.panel_start("primary", "Vehicle configuration");
  c.form_start(p.uri);
  c.input_select_start("Vehicle type", "vehicletype");
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    c.input_select_option(k->second.name, k->first, (vehicletype == k->first));
  c.input_select_end();
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str());
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}


