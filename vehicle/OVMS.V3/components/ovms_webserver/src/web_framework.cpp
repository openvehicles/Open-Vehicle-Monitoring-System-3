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
//static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_housekeeping.h"
#include "buffered_shell.h"
#include "metrics_standard.h"
#include "vehicle.h"


// Embedded asset files (see component.mk):

extern const uint8_t script_js_gz_start[]     asm("_binary_script_js_gz_start");
extern const uint8_t script_js_gz_end[]       asm("_binary_script_js_gz_end");
extern const uint8_t style_css_gz_start[]     asm("_binary_style_css_gz_start");
extern const uint8_t style_css_gz_end[]       asm("_binary_style_css_gz_end");
extern const uint8_t favicon_png_start[]      asm("_binary_favicon_png_start");
extern const uint8_t favicon_png_end[]        asm("_binary_favicon_png_end");


/**
 * encode_html: HTML encode value
 *   "  →  &quot;
 *   '  →  &#x27;   (Note: instead of &apos; for IE compatibility)
 *   <  →  &lt;
 *   >  →  &gt;
 *   &  →  &amp;
 */
std::string PageContext::encode_html(const char* text) {
	std::string buf;
	for (int i=0; i<strlen(text); i++) {
		if (text[i] == '\"')
			buf += "&quot;";
		else if (text[i] == '\'')
			buf += "&#x27;";
		else if(text[i] == '<')
			buf += "&lt;";
		else if(text[i] == '>')
			buf += "&gt;";
		else if(text[i] == '&')
			buf += "&amp;";
		else
			buf += text[i];
  }
	return buf;
}

std::string PageContext::encode_html(std::string text) {
  return encode_html(text.c_str());
}

#define _attr(text) (encode_html(text).c_str())
#define _html(text) (encode_html(text).c_str())


std::string PageContext::getvar(const char* name, size_t maxlen /*=200*/) {
  char varbuf[maxlen];
  mg_get_http_var(&hm->body, name, varbuf, sizeof(varbuf));
  return std::string(varbuf);
}


/**
 * HTML generation utils (Bootstrap widgets)
 */

void PageContext::error(int code, const char* text) {
  mg_http_send_error(nc, code, text);
}

void PageContext::head(int code, const char* headers /*=NULL*/) {
  if (!headers) {
    headers =
      "Content-Type: text/html; charset=utf-8\r\n"
      "Cache-Control: no-cache";
  }
  mg_send_head(nc, code, -1, headers);
}

void PageContext::print(const std::string text) {
  mg_send_http_chunk(nc, text.data(), text.size());
}

void PageContext::print(const char* text) {
  mg_send_http_chunk(nc, text, strlen(text));
}

void PageContext::printf(const char *fmt, ...) {
  char* buf = NULL;
  int len;
  va_list ap;
  va_start(ap, fmt);
  len = vasprintf(&buf, fmt, ap);
  va_end(ap);
  if (len >= 0)
    mg_send_http_chunk(nc, buf, len);
  if (buf)
    free(buf);
}

void PageContext::done() {
  mg_send_http_chunk(nc, "", 0);
}

void PageContext::panel_start(const char* type, const char* title) {
  mg_printf_http_chunk(nc,
    "<div class=\"panel panel-%s\">"
      "<div class=\"panel-heading\">%s</div>"
      "<div class=\"panel-body\">"
    , _attr(type), title);
}

void PageContext::panel_end(const char* footer) {
  mg_printf_http_chunk(nc, footer[0]
    ? "</div><div class=\"panel-footer\">%s</div></div>"
    : "</div></div>"
    , footer);
}

void PageContext::form_start(const char* action) {
  mg_printf_http_chunk(nc,
    "<form class=\"form-horizontal\" method=\"post\" action=\"%s\" target=\"#main\">"
    , _attr(action));
}

void PageContext::form_end() {
  mg_printf_http_chunk(nc, "</form>");
}

void PageContext::input(const char* type, const char* label, const char* name, const char* value,
    const char* placeholder /*=NULL*/, const char* helptext /*=NULL*/, const char* moreattrs /*=NULL*/,
    const char* unit /*=NULL*/) {
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-%s\">%s:</label>"
      "<div class=\"col-sm-9\">"
        "%s" // unit (input-group)
        "<input type=\"%s\" class=\"form-control\" placeholder=\"%s%s\" name=\"%s\" id=\"input-%s\" value=\"%s\" %s>"
        "%s%s%s" // unit
        "%s%s%s" // helptext
      "</div>"
    "</div>"
    , _attr(name), label
    , unit ? "<div class=\"input-group\">" : ""
    , _attr(type)
    , placeholder ? "" : "Enter "
    , placeholder ? _attr(placeholder) : _attr(label)
    , _attr(name), _attr(name), _attr(value)
    , moreattrs ? moreattrs : ""
    , unit ? "<div class=\"input-group-addon input-unit\">" : ""
    , unit ? unit : ""
    , unit ? "</div></div>" : ""
    , helptext ? "<span class=\"help-block\">" : ""
    , helptext ? helptext : ""
    , helptext ? "</span>" : ""
    );
}

void PageContext::input_text(const char* label, const char* name, const char* value,
    const char* placeholder /*=NULL*/, const char* helptext /*=NULL*/, const char* moreattrs /*=NULL*/) {
  input("text", label, name, value, placeholder, helptext, moreattrs);
}

void PageContext::input_password(const char* label, const char* name, const char* value,
    const char* placeholder /*=NULL*/, const char* helptext /*=NULL*/, const char* moreattrs /*=NULL*/) {
  input("password", label, name, value, placeholder, helptext, moreattrs);
}

void PageContext::input_select_start(const char* label, const char* name) {
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-%s\">%s:</label>"
      "<div class=\"col-sm-9\">"
        "<select class=\"form-control\" size=\"1\" name=\"%s\" id=\"input-%s\">"
    , _attr(name), label, _attr(name), _attr(name));
}

void PageContext::input_select_option(const char* label, const char* value, bool selected) {
  mg_printf_http_chunk(nc,
    "<option value=\"%s\"%s>%s</option>"
    , _attr(value), selected ? " selected" : "", label);
}

void PageContext::input_select_end(const char* helptext /*=NULL*/) {
  mg_printf_http_chunk(nc, "</select>%s%s%s</div></div>"
    , helptext ? "<span class=\"help-block\">" : ""
    , helptext ? helptext : ""
    , helptext ? "</span>" : "");
}

void PageContext::input_radio_start(const char* label, const char* name) {
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-%s\">%s:</label>"
      "<div class=\"col-sm-9\">"
        "<div id=\"input-%s\" class=\"btn-group\" data-toggle=\"buttons\">"
    , _attr(name), label, _attr(name));
}

void PageContext::input_radio_option(const char* name, const char* label, const char* value, bool selected) {
  mg_printf_http_chunk(nc,
    "<label class=\"btn btn-default %s\">"
      "<input type=\"radio\" name=\"%s\" value=\"%s\" %s autocomplete=\"off\"> %s"
    "</label>"
    , selected ? "active" : ""
    , _attr(name), _attr(value)
    , selected ? "checked" : ""
    , label);
}

void PageContext::input_radio_end(const char* helptext /*=NULL*/) {
  mg_printf_http_chunk(nc, "</div>%s%s%s</div></div>"
    , helptext ? "<span class=\"help-block\">" : ""
    , helptext ? helptext : ""
    , helptext ? "</span>" : "");
}

void PageContext::input_checkbox(const char* label, const char* name, bool value,
    const char* helptext /*=NULL*/) {
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<div class=\"col-sm-9 col-sm-offset-3\">"
        "<div class=\"checkbox\">"
          "<label><input type=\"checkbox\" name=\"%s\" value=\"yes\" %s> %s</label>"
        "</div>"
        "%s%s%s"
      "</div>"
    "</div>"
    , _attr(name), value ? "checked" : "", label
    , helptext ? "<span class=\"help-block\">" : ""
    , helptext ? helptext : ""
    , helptext ? "</span>" : ""
    );
}

void PageContext::input_slider(const char* label, const char* name, int size, const char* unit,
    int enabled, double value, double defval, double min, double max, double step /*=1*/,
    const char* helptext /*=NULL*/) {
  int width = 50 + size * 10;
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-%s\">%s:</label>"
      "<div class=\"col-sm-9\">"
        "<div class=\"form-control slider\">"
          "<div class=\"slider-control form-inline\">"
            "<input id=\"input-%s\" class=\"slider-enable\" type=\"%s\" %s> "
            "<input class=\"form-control slider-value\" %s type=\"number\" name=\"%s\" style=\"width:%dpx;\" value=\"%g\" min=\"%g\" max=\"%g\" step=\"%g\"> "
            "%s%s%s"
            "<input class=\"btn btn-default slider-down\" %s type=\"button\" value=\"➖\"> "
            "<input class=\"btn btn-default slider-up\" %s type=\"button\" value=\"➕\">"
          "</div>"
          "<input class=\"slider-input\" %s type=\"range\" value=\"%g\" min=\"%g\" max=\"%g\" step=\"%g\" data-default=\"%g\">"
        "</div>"
        "%s%s%s"
      "</div>"
    "</div>"
    , _attr(name)
    , label
    , _attr(name)
    , (enabled < 0) ? "hidden" : "checkbox" // -1 => no checkbox
    , (enabled > 0) ? "checked" : ""
    , (enabled == 0) ? "disabled" : ""
    , _attr(name)
    , width
    , value, min, max, step
    , unit ? "<span class=\"slider-unit\">" : ""
    , unit ? unit : ""
    , unit ? "</span> " : ""
    , (enabled == 0) ? "disabled" : ""
    , (enabled == 0) ? "disabled" : ""
    , (enabled == 0) ? "disabled" : ""
    , value, min, max, step, defval
    , helptext ? "<span class=\"help-block\">" : ""
    , helptext ? helptext : ""
    , helptext ? "</span>" : ""
    );
}

void PageContext::input_button(const char* type, const char* label) {
  mg_printf_http_chunk(nc,
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-%s\">%s</button>"
      "</div>"
    "</div>"
    , _attr(type), label);
}

void PageContext::alert(const char* type, const char* text) {
  mg_printf_http_chunk(nc,
    "<div class=\"alert alert-%s\">%s</div>"
    , _attr(type), text);
}

void PageContext::fieldset_start(const char* title) {
  mg_printf_http_chunk(nc,
    "<fieldset><legend>%s</legend>"
    , title);
}

void PageContext::fieldset_end() {
  mg_printf_http_chunk(nc, "</fieldset>");
}

void PageContext::hr() {
  mg_printf_http_chunk(nc, "<hr>");
}


/**
 * CreateMenu:
 */
std::string OvmsWebServer::CreateMenu(PageContext_t& c)
{
  std::string main, config, vehicle;
  
  // collect menu items:
  for (PageEntry* e : MyWebServer.m_pagemap) {
    if (e->menu == PageMenu_Main)
      main += "<li><a href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
    else if (e->menu == PageMenu_Config)
      config += "<li><a href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
    else if (e->menu == PageMenu_Vehicle)
      vehicle += "<li><a href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
  }
  
  // assemble menu:
  std::string menu =
    "<ul class=\"nav navbar-nav\">"
      + main;
  if (vehicle != "") {
    std::string vehiclename = MyVehicleFactory.ActiveVehicleName();
    menu +=
      "<li class=\"dropdown\" id=\"menu-vehicle\">"
        "<a href=\"#\" class=\"dropdown-toggle\" data-toggle=\"dropdown\" role=\"button\" aria-haspopup=\"true\" aria-expanded=\"false\">"
        + vehiclename +
        " <span class=\"caret\"></span></a>"
        "<ul class=\"dropdown-menu\">"
          + vehicle +
        "</ul>"
      "</li>";
  }
  menu +=
      "<li class=\"dropdown\" id=\"menu-cfg\">"
        "<a href=\"#\" class=\"dropdown-toggle\" data-toggle=\"dropdown\" role=\"button\" aria-haspopup=\"true\" aria-expanded=\"false\">Config <span class=\"caret\"></span></a>"
        "<ul class=\"dropdown-menu\">"
          + config +
        "</ul>"
      "</li>"
    "</ul>"
    "<ul class=\"nav navbar-nav navbar-right\">"
      "<li class=\"hidden-xs\"><a href=\"#\" class=\"toggle-night\">◐</a></li>"
      + std::string(c.session
      ? "<li><a href=\"/logout\" target=\"#main\">Logout</a></li>"
      : "<li><a href=\"/login\" target=\"#main\">Login</a></li>") +
    "</ul>";

  return menu;
}


/**
 * HandleHome: show home / intro menu
 */

void OvmsWebServer::OutputHome(PageEntry_t& p, PageContext_t& c)
{
  std::string main, config, vehicle;
  
  // collect menu items:
  for (PageEntry* e : MyWebServer.m_pagemap) {
    if (e->menu == PageMenu_Main)
      main += "<li><a class=\"btn btn-default\" href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
    else if (e->menu == PageMenu_Config)
      config += "<li><a class=\"btn btn-default\" href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
    else if (e->menu == PageMenu_Vehicle)
      vehicle += "<li><a class=\"btn btn-default\" href=\"" + std::string(e->uri) + "\" target=\"#main\">" + std::string(e->label) + "</a></li>";
  }
  
  c.panel_start("primary", "Home");
  
  c.printf(
    "<fieldset><legend>Main menu</legend>"
    "<ul class=\"list-inline\">%s</ul>"
    "</fieldset>"
    , main.c_str());

  if (vehicle != "") {
    const char* vehiclename = MyVehicleFactory.ActiveVehicleName();
    mg_printf_http_chunk(c.nc,
      "<fieldset><legend>%s</legend>"
      "<ul class=\"list-inline\">%s</ul>"
      "</fieldset>"
      , vehiclename, vehicle.c_str());
  }
  
  c.printf(
    "<fieldset><legend>Configuration</legend>"
    "<ul class=\"list-inline\">%s</ul>"
    "</fieldset>"
    , config.c_str());

  c.panel_end();
  
  // check admin password, show warning if unset:
  if (MyConfig.GetParamValue("password", "module").empty()) {
    c.alert("warning", "<p><strong>Warning:</strong> no admin password set. Web access is open to the public.</p>");
  }
  
  // check auto init, show warning if disabled:
  if (!MyConfig.GetParamValueBool("auto", "init") && MyHousekeeping && !MyHousekeeping->m_autoinit) {
    c.alert("warning", "<p><strong>Warning:</strong> auto start disabled. Check auto start configuration.</p>");
  }
}

void OvmsWebServer::HandleHome(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  c.alert("info", "<p class=\"lead\">Welcome to the OVMS web console.</p>");
  OutputHome(p, c);
  c.done();
}


/**
 * HandleRoot: output AJAX framework & main menu
 */
void OvmsWebServer::HandleRoot(PageEntry_t& p, PageContext_t& c)
{
  std::string menu = CreateMenu(c);
  
  // output page framework:
  c.head(200);
  c.print(
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
      "<head>"
        "<meta charset=\"utf-8\">"
        "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<meta name=\"mobile-web-app-capable\" content=\"yes\">"
        "<title>OVMS Console</title>"
        "<link rel=\"stylesheet\" href=\"/assets/style.css\">"
        "<link rel=\"shortcut icon\" sizes=\"192x192\" href=\"/apple-touch-icon.png\">"
        "<link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\">"
      "</head>"
      "<body>"
        "<nav id=\"nav\" class=\"navbar navbar-inverse navbar-fixed-top\">"
          "<div class=\"container-fluid\">"
            "<div class=\"navbar-header\">"
              "<button type=\"button\" class=\"navbar-toggle collapsed\" data-toggle=\"collapse\" data-target=\"#menu\" aria-expanded=\"false\" aria-controls=\"menu\">"
                "<span class=\"sr-only\">Toggle navigation</span>"
                "<span class=\"icon-bar\"></span>"
                "<span class=\"icon-bar\"></span>"
                "<span class=\"icon-bar\"></span>"
              "</button>"
              "<button type=\"button\" class=\"navbar-toggle collapsed toggle-night\">◐</button>"
              "<a class=\"navbar-brand\" href=\"/home\" target=\"#main\" title=\"Home\"><img alt=\"OVMS\" src=\"/apple-touch-icon.png\"></a>"
            "</div>"
            "<div role=\"menu\" id=\"menu\" class=\"navbar-collapse collapse\">");
  c.print(menu);
  c.print(
            "</div>"
          "</div>"
        "</nav>"
        "<div role=\"main\" id=\"main\" class=\"container-fluid\">"
        "</div>"
        "<script src=\"/assets/script.js\"></script>"
      "</body>"
    "</html>");
  c.done();
}


/**
 * HandleMenu: output main menu
 */
void OvmsWebServer::HandleMenu(PageEntry_t& p, PageContext_t& c)
{
  std::string menu = CreateMenu(c);
  c.head(200);
  mg_send_http_chunk(c.nc, menu.data(), menu.length());
  c.done();
}


/**
 * HandleAsset: output gzip assets
 * Note: no check for Accept-Encoding, we can't unzip & a modern browser is required anyway
 */
void OvmsWebServer::HandleAsset(PageEntry_t& p, PageContext_t& c)
{
  const uint8_t* data = NULL;
  size_t size;
  time_t mtime;
  const char* type;
  bool gzip_encoded = true;
  
  if (c.uri == "/assets/style.css") {
    data = style_css_gz_start;
    size = style_css_gz_end - style_css_gz_start;
    mtime = MTIME_ASSETS_STYLE_CSS;
    type = "text/css";
  }
  else if (c.uri == "/assets/script.js") {
    data = script_js_gz_start;
    size = script_js_gz_end - script_js_gz_start;
    mtime = MTIME_ASSETS_SCRIPT_JS;
    type = "application/javascript";
  }
  else if (c.uri == "/favicon.ico" || c.uri == "/apple-touch-icon.png") {
    data = favicon_png_start;
    size = favicon_png_end - favicon_png_start;
    mtime = MTIME_ASSETS_FAVICON_PNG;
    type = "image/png";
    gzip_encoded = false;
  }
  else {
    mg_http_send_error(c.nc, 404, "Not found");
    return;
  }
  
  char etag[50], current_time[50], last_modified[50];
  time_t t = (time_t) mg_time();
  snprintf(etag, sizeof(etag), "\"%lx.%" INT64_FMT "\"", (unsigned long) mtime, (int64_t) size);
  strftime(current_time, sizeof(current_time), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));
  strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&mtime));
  
  mg_send_response_line(c.nc, 200, NULL);
  mg_printf(c.nc,
    "Date: %s\r\n"
    "Last-Modified: %s\r\n"
    "Content-Type: %s\r\n"
    "%s"
    "Transfer-Encoding: chunked\r\n"
    "Etag: %s\r\n"
    "\r\n"
    , current_time
    , last_modified
    , type
    , gzip_encoded ? "Content-Encoding: gzip\r\n" : ""
    , etag);
  
  // start chunked transfer:
  new HttpDataSender(c.nc, data, size);
}
