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
 * HandleCfgWifi: configure wifi networks (URL /cfg/wifi)
 */

void OvmsWebServer::HandleCfgWifi(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
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
  auto lock = MyConfig.Lock();
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
    max = param->m_instances.size();
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
    for (auto const& kv : param->m_instances) {
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
  auto lock = MyConfig.Lock();
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
    param->SetMap(newmap);

    // set new autostart ssid:
    if (ssid_autostart != "")
      MyConfig.SetParamValue("auto", "wifi.ssid." + prefix, ssid_autostart);
  }
}
