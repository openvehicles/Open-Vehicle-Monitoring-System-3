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

#include <dirent.h>
#include "ovms_webserver.h"
#include "ovms_version.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

#ifdef CONFIG_OVMS_COMP_OTA
#include "ovms_ota.h"
#include "ovms_utils.h"

/**
 * HandleCfgFirmware: OTA firmware update & boot setup (URL /cfg/firmware)
 */
void OvmsWebServer::HandleCfgFirmware(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string cmdres, mru;
  std::string action;
  ota_info info;
  bool auto_enable, auto_allow_modem;
  std::string auto_hour, server, tag, hardware;
  std::string output;
  std::string version;
  const char *what;
  char buf[132];

  // Background update-check (AJAX): the slow part of this page is the OTA
  // "available version" lookup, which makes a blocking HTTP request to the
  // update server. The page is rendered without it (GetStatus check_update=false)
  // and the browser fetches the result here asynchronously. Returns JSON:
  //   { "version": "<server version>", "update": <bool>, "changelog": "<text>" }
  if (c.getvar("action") == "updatecheck") {
    MyOTA.GetStatus(info, true);    // performs the blocking server version check
    c.head(200,
      "Content-Type: application/json; charset=utf-8\r\n"
      "Cache-Control: no-cache");
    c.printf("{\"version\":\"%s\",\"update\":%s,\"changelog\":\"%s\"}",
      json_encode(info.version_server).c_str(),
      info.update_available ? "true" : "false",
      json_encode(info.changelog_server).c_str());
    c.done();
    return;
  }

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
  // Skip the server "available version" check here (it does a blocking HTTP
  // request); the page fetches it in the background via ?action=updatecheck.
  MyOTA.GetStatus(info, false);
  bool has_factory = ovms_partition_table_has_factory();

  c.panel_start("primary", "Firmware setup &amp; update");

  // Wrap the status rows in a form-horizontal so their labels are right-aligned
  // and vertically aligned with their values, matching the tabbed sections below
  // (Bootstrap only styles .control-label that way inside .form-horizontal):
  c.print("<div class=\"form-horizontal\">");
  c.input_info("Firmware version", info.version_firmware.c_str());
  // The server version is filled in asynchronously (see updatecheck script below):
  output = "<span id=\"ota-server-version\" class=\"text-muted\">checking for updates&hellip;</span>";
  output.append(" <button type=\"button\" class=\"btn btn-default\" data-toggle=\"modal\" data-target=\"#version-dialog\" id=\"ota-versioninfo-btn\" disabled>Version info</button>"
                " <button type=\"button\" class=\"btn btn-default action-update-now\">Update now</button>");
  // Emit the "…available" row directly (not via input_info) so its label can take
  // extra top padding: this row's value has buttons (taller than text) that push
  // the version number down to mid-row, so the label is dropped to line up with it.
  c.print("<div class=\"form-group\">"
            "<label class=\"control-label col-sm-3\" style=\"padding-top:12px\">…available:</label>"
            "<div class=\"col-sm-9\"><div class=\"form-control-static\">");
  c.print(output);
  c.print("</div></div></div>");
  c.print("</div>");

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
  if (has_factory) {
    what = "Factory image";
    version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_FACTORY);
    if (version != "") {
      snprintf(buf, sizeof(buf), "%s (%s)", what, version.c_str());
      what = buf;
    }
    c.input_select_option(what, "factory", (info.partition_boot == "factory"));
  }
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
    "<p>Default is <code>main</code> for standard user releases. Use <code>eap</code> (early access program) for "
    "stable (beta test) developer builds, or <code>edge</code> for bleeding edge developer builds. "
    "→<a target=\"_blank\" href=\"https://docs.openvehicles.com/en/latest/userguide/ota.html\">User Guide</a></p>"
    "<div class=\"alert alert-warning\"><b class=\"text-danger\">⚠</b> "
    "<b>Developer builds are meant for development and testing, not for regular daily use!</b> "
    "Developer versions may contain potentially harmful bugs and experiments. When running a developer version, it’s required you "
    "closely follow discussions on the newly developed features, help in testing them by providing feedback, and <b>closely watch "
    "your device and vehicle for any unusual behaviour.</b></div>",
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
    // Left-align the list markers with the column's other text (drop the default
    // ~40px <ol> indent) instead of indenting them off to the right:
    "<ol style=\"padding-left:0; list-style-position:inside\">"
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

  if (has_factory) {
    c.panel_end(
      "<p>The module can store up to three firmware images in a factory and two OTA partitions.</p>"
      "<p>Flashing from web or file writes alternating to the OTA partitions, the factory partition remains unchanged.</p>"
      "<p>You can flash the factory partition via USB, see developer manual for details.</p>");
  }
  else {
    c.panel_end(
      "<p>The module can store up to two firmware images in a the two OTA partitions.</p>"
      "<p>Flashing from web or file writes alternating to the OTA partitions.</p>");
  }

  // Title & changelog are filled in asynchronously by the updatecheck script:
  c.print(
    "<div class=\"modal fade\" id=\"version-dialog\" role=\"dialog\" data-backdrop=\"static\" data-keyboard=\"false\">"
      "<div class=\"modal-dialog modal-lg\">"
        "<div class=\"modal-content\">"
          "<div class=\"modal-header\">"
            "<button type=\"button\" class=\"close\" data-dismiss=\"modal\">&times;</button>"
            "<h4 class=\"modal-title\">Version info <span id=\"ota-version-title\"></span></h4>"
          "</div>"
          "<div class=\"modal-body\">"
            "<pre id=\"ota-changelog\">Loading&hellip;</pre>"
          "</div>"
          "<div class=\"modal-footer\">"
            "<button type=\"button\" class=\"btn btn-default\" data-dismiss=\"modal\">Close</button>"
          "</div>"
        "</div>"
      "</div>"
    "</div>");

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
      // Fetch the server's available version in the background so the page itself
      // loads instantly (the check is a blocking HTTP request to the OTA server):
      "$.ajax({ url: \"" + p.uri + "?action=updatecheck\", dataType: \"json\", timeout: 90000 })"
        ".done(function(d){"
          "var v = (d.version || \"\").trim();"
          "var s = $(\"#ota-server-version\").removeClass(\"text-muted\");"
          "if (!v) { s.addClass(\"text-warning\").text(\"no response from update server\");"
            "$(\"#ota-changelog\").text(\"No response from the update server.\"); return; }"
          "s.text(v);"
          "if (d.update) s.addClass(\"text-success\").append(\" \\u2014 update available\");"
          "$(\"#ota-version-title\").text(v);"
          "$(\"#ota-changelog\").text((d.changelog || \"\").trim() || \"(no changelog provided)\");"
          "$(\"#ota-versioninfo-btn\").prop(\"disabled\", false);"
        "})"
        ".fail(function(){"
          "$(\"#ota-server-version\").removeClass(\"text-muted\").addClass(\"text-warning\").text(\"update check failed\");"
          "$(\"#ota-changelog\").text(\"Update check failed (no network, or the update server is unreachable).\");"
        "});"
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
