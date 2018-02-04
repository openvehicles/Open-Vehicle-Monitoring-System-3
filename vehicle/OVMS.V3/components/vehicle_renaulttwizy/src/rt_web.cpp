/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy Webserver
 * 
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

// #include "ovms_log.h"
// static const char *TAG = "v-twizy";

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_renaulttwizy.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleRenaultTwizy::WebInit()
{
  MyWebServer.RegisterPage("/xrt/drivemode", "Drivemode", WebConsole, PageMenu_Main, PageAuth_Cookie);
}


/**
 * WebConsole: (/xrt/drivemode)
 */
void OvmsVehicleRenaultTwizy::WebConsole(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance();
  SevconClient* sc = twizy ? twizy->GetSevconClient() : NULL;
  if (!sc) {
    c.error(404, "Twizy/SEVCON module not loaded");
    return;
  }
  
  if (c.method == "POST") {
    std::string output, arg;
    
    if ((arg = c.getvar("load")) != "") {
      // execute profile switch:
      output = MyWebServer.ExecuteCommand("xrt cfg load " + arg);
      output += MyWebServer.ExecuteCommand("xrt cfg info");
      
      // output result:
      c.head(200);
      c.printf("%s", _html(output));
      c.printf(
        "<script>"
          "$('#loadmenu btn').removeClass('base unsaved btn-warning').addClass('btn-default');"
          "$('#prof-%d').removeClass('btn-default').addClass('btn-warning %s');"
          , sc->m_drivemode.profile_user
          , sc->m_drivemode.unsaved ? "unsaved" : "");
      if (sc->m_drivemode.profile_user != sc->m_drivemode.profile_cfgmode) {
        c.printf(
          "$('#prof-%d').addClass('base');", sc->m_drivemode.profile_cfgmode);
      }
      c.printf(
        "</script>");
      c.done();
      return;
    }
    else {
      c.error(400, "Bad request");
      return;
    }
  }
  
  // output status page:
  
  ostringstream buf;
  
  buf << "Active profile: ";
  if (sc->m_drivemode.profile_user == sc->m_drivemode.profile_cfgmode) {
    buf << "#" << sc->m_drivemode.profile_user;
  }
  else {
    buf << "base #" << sc->m_drivemode.profile_cfgmode;
    buf << ", live #" << sc->m_drivemode.profile_user;
  }
  
  buf << "\n\n" << MyWebServer.ExecuteCommand("xrt cfg info");
  
  // profile labels (todo: make configurable):
  const char* proflabel[4] = { "STD", "PWR", "ECO", "ICE" };
  
  c.head(200);
  c.printf(
    "<style>"
    ".btn-default.base { background-color: #fffca8; }"
    ".btn-default.base:hover, .btn-default.base:focus { background-color: #fffa62; }"
    ".unsaved > *:after { content: \"*\"; }"
    "</style>");
  c.panel_start("primary", "Drivemode");
  c.printf(
    "<samp id=\"loadres\">%s</samp>", _html(buf.str()));
  c.printf(
    "<div id=\"loadmenu\" class=\"center-block\"><ul class=\"list-inline\">");
  for (int prof=0; prof<=3; prof++) {
    c.printf(
      "<li><a id=\"prof-%d\" class=\"btn btn-lg btn-%s %s%s\" data-method=\"post\" target=\"#loadres\" href=\"?load=%d\"><strong>%s</strong></a></li>"
      , prof
      , (sc->m_drivemode.profile_user == prof) ? "warning" : "default"
      , (sc->m_drivemode.profile_cfgmode == prof) ? "base" : ""
      , (sc->m_drivemode.profile_user == prof && sc->m_drivemode.unsaved) ? "unsaved" : ""
      , prof
      , proflabel[prof]
      );
  }
  c.printf(
    "</ul></div>");
  c.panel_end();
  c.done();
}
