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
 * HandleCfgLogging: configure logging (URL /cfg/logging)
 */
void OvmsWebServer::HandleCfgLogging(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
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
      param->SetMap(pmap);

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
    pmap = param->GetMap();

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
