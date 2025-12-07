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

#include <fstream>
#include <sstream>
#include "ovms_webserver.h"
#include "ovms_peripherals.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

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
