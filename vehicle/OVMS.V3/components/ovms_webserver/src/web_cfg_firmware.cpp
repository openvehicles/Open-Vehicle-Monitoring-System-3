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
#ifdef CONFIG_OVMS_COMP_SDCARD
  bool sd_buffer;
  std::string sd_minspace;
#endif
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
#ifdef CONFIG_OVMS_COMP_SDCARD
    sd_buffer = (c.getvar("sd_buffer") == "yes");
    sd_minspace = c.getvar("sd_minspace");
#endif

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
#ifdef CONFIG_OVMS_COMP_SDCARD
        MyConfig.SetParamValueBool("ota", "sd.buffer", sd_buffer);
        MyConfig.SetParamValue("ota", "sd.minspace", sd_minspace);
#endif
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
#ifdef CONFIG_OVMS_COMP_SDCARD
    sd_buffer = MyConfig.GetParamValueBool("ota", "sd.buffer", false);
    sd_minspace = MyConfig.GetParamValue("ota", "sd.minspace", "50");
#endif
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

#ifdef CONFIG_OVMS_COMP_SDCARD
  c.print("<hr>");
  c.input_checkbox("Buffer OTA downloads on SD card", "sd_buffer", sd_buffer,
    "<p>If enabled, OTA updates will download the firmware to the SD card first, then flash from there."
    " This reduces RAM pressure during the flash process and provides a local backup of the firmware file.</p>");
  c.input("number", "SD card reserve (MB)", "sd_minspace", sd_minspace.c_str(), "default: 50",
    "<p>Minimum free space to keep on the SD card after downloading (in MB).</p>",
    "min=\"0\" max=\"10000\" step=\"1\"");
#endif

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
