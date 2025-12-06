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
