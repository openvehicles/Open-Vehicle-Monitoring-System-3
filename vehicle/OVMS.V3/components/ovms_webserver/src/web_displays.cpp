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
// static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <dirent.h>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"
#include "ovms_housekeeping.h"
#include "ovms_peripherals.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleDashboard:
 */
void OvmsWebServer::HandleDashboard(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicle *vehicle = MyVehicleFactory.ActiveVehicle();
  if (!vehicle) {
    c.head(400);
    c.alert("danger", "<p class=\"lead\">Error: no active vehicle!</p>");
    c.done();
    return;
  }
  
  // get dashboard configuration:
  DashboardConfig cfg;
  vehicle->GetDashboardConfig(cfg);

  // output dashboard:
  const char* content =
    "<style>"
    ""
    "@media (max-width: 767px) {"
      ".panel-single .panel-body {"
        "padding: 2px;"
      "}"
    "}"
    ""
    ".highcharts-plot-band, .highcharts-pane {"
      "fill-opacity: 0;"
    "}"
    ""
    ".speed-default-pane {"
      "fill: #95a222;"
    "}"
    ".night .speed-default-pane {"
      "fill: #151515;"
      "fill-opacity: 0.5;"
    "}"
    ""
    ".speed-outer-pane {"
    "	fill: #EFEFEF;"
    "}"
    ".speed-middle-pane {"
    "	stroke-width: 1px;"
    "	stroke: #AAA;"
    "}"
    ".night .highcharts-pane.speed-middle-pane {"
      "stroke-width: 2px;"
    "}"
    ".speed-inner-pane {"
    "	fill: #DDDDDD;"
    "}"
    ""
    ".highcharts-plot-band.border {"
      "stroke: #666666;"
      "stroke-width: 1px;"
    "}"
    ""
    ".green-band {"
    "	fill: #55BF3B;"
    "	fill-opacity: 0.4;"
    "}"
    ".yellow-band {"
    "	fill: #DDDF0D;"
    "	fill-opacity: 0.5;"
    "}"
    ".red-band {"
    "	fill: #DF5353;"
    "	fill-opacity: 0.6;"
    "}"
    ".violet-band {"
      "fill: #9622ff;"
      "fill-opacity: 0.6;"
    "}"
    ".night .violet-band {"
      "fill: #9622ff;"
      "fill-opacity: 0.8;"
    "}"
    ""
    ".highcharts-gauge-series .highcharts-pivot {"
      "stroke-width: 1px;"
      "stroke: #757575;"
      "fill-opacity: 1;"
      "fill: black;"
    "}"
    ""
    ".highcharts-gauge-series.auxgauge .highcharts-pivot {"
      "fill-opacity: 1;"
      "fill: #fff;"
      "stroke-width: 0;"
    "}"
    ".night .highcharts-gauge-series.auxgauge .highcharts-pivot {"
      "fill: #000;"
    "}"
    ""
    ".highcharts-gauge-series .highcharts-dial {"
      "fill: #d80000;"
      "stroke: #000;"
      "stroke-width: 0.5px;"
    "}"
    ""
    ".highcharts-yaxis-grid .highcharts-grid-line,"
    ".highcharts-yaxis-grid .highcharts-minor-grid-line {"
    "	display: none;"
    "}"
    ""
    ".highcharts-yaxis .highcharts-tick {"
    "	stroke-width: 2px;"
    "	stroke: #666666;"
    "}"
    ".night .highcharts-yaxis .highcharts-tick {"
      "stroke: #e0e0e0;"
    "}"
    ""
    ".highcharts-yaxis .highcharts-minor-tick {"
      "stroke-width: 1.8px;"
      "stroke: #00000085;"
      "stroke-dasharray: 6;"
      "stroke-dashoffset: -4.8;"
      "stroke-linecap: round;"
    "}"
    ".night .highcharts-yaxis .highcharts-minor-tick {"
      "stroke: #ffffff85;"
    "}"
    ""
    ".highcharts-axis-labels {"
      "fill: #000;"
      "font-weight: bold;"
      "font-size: 0.9em;"
    "}"
    ".night .highcharts-axis-labels {"
      "fill: #ddd;"
    "}"
    ""
    ".highcharts-data-label text {"
      "fill: #333333;"
    "}"
    ".night .highcharts-data-label text {"
      "fill: #fff;"
    "}"
    ""
    ".highcharts-axis-labels.speed {"
      "font-size: 1.2em;"
    "}"
    ""
    ".dashboard .highcharts-data-label text {"
      "font-size: 2em;"
    "}"
    ""
    ".dashboard .overlay {"
      "position: absolute;"
      "z-index: 10;"
      "top: 62%;"
      "text-align: center;"
      "left: 0%;"
      "right: 0%;"
    "}"
    ".dashboard .underlay {"
      "position: absolute;"
      "z-index: 0;"
      "width: 100%;"
      "height: 100%;"
      "overflow: hidden;"
    "}"
    ".dashboard .value {"
      "color: #04282b;"
      "background: #97b597;"
      "padding: 2px 5px;"
      "margin-right: 0.2em;"
      "text-align: center;"
      "border: 1px inset #c3c3c380;"
      "font-family: \"Monaco\", \"Menlo\", \"Consolas\", \"QuickType Mono\", \"Lucida Console\", \"Roboto Mono\", \"Ubuntu Mono\", \"DejaVu Sans Mono\", \"Droid Sans Mono\", monospace;"
      "display: inline-block;"
    "}"
    ".night .dashboard .value {"
      "color: #fff;"
      "background: #252525;"
    "}"
    ".dashboard .unit {"
      "color: #666;"
      "font-size: 12px;"
    "}"
    ".dashboard .range-value .value {"
      "margin-left: 10px;"
      "width: 100px;"
    "}"
    ".dashboard .energy-value {"
      "margin-top: 4px;"
    "}"
    ".dashboard .energy-value .value {"
      "margin-left: 18px;"
      "width: 100px;"
      "font-size: 12px;"
    "}"
    ".dashboard .underlay .value {"
      "font-size: 12px;"
      "width: 32px;"
      "padding: 2px 1px 0 0;"
      "margin: 0;"
      "position: absolute;"
    "}"
    ".dashboard .underlay > div {"
      "position: absolute;"
      "display: inline-block;"
    "}"
    ".dashboard .voltage-value .value {"
      "left: 80px;"
      "top: 40px;"
    "}"
    ".dashboard .soc-value .value {"
      "left: 80px;"
      "top: 40px;"
    "}"
    ".dashboard .consumption-value .value {"
      "left: -112px;"
      "top: 40px;"
    "}"
    ".dashboard .power-value .value {"
      "left: -112px;"
      "top: 40px;"
    "}"
    "@media (min-width: 0px) and (max-width: 404px) {"
      ".dashboard .voltage-value { left: -20%; top: 20%; }"
      ".dashboard .soc-value { left: -20%; top: 60%; }"
      ".dashboard .consumption-value { left: 120%; top: 20%; }"
      ".dashboard .power-value { left: 120%; top: 60%; }"
    "}"
    "@media (min-width: 405px) and (max-width: 454px) {"
      ".dashboard .voltage-value { left: -15%; top: 20%; }"
      ".dashboard .soc-value { left: -15%; top: 60%; }"
      ".dashboard .consumption-value { left: 115%; top: 20%; }"
      ".dashboard .power-value { left: 115%; top: 60%; }"
    "}"
    "@media (min-width: 455px) and (max-width: 604px) {"
      ".dashboard .voltage-value { left: -10%; top: 20%; }"
      ".dashboard .soc-value { left: -10%; top: 60%; }"
      ".dashboard .consumption-value { left: 110%; top: 20%; }"
      ".dashboard .power-value { left: 110%; top: 60%; }"
    "}"
    "@media (min-width: 605px) {"
      ".dashboard .voltage-value { left: 0%; top: 20%; }"
      ".dashboard .soc-value { left: 0%; top: 60%; }"
      ".dashboard .consumption-value { left: 100%; top: 20%; }"
      ".dashboard .power-value { left: 100%; top: 60%; }"
    "}"
    "</style>"
    ""
    "<div class=\"panel panel-primary\" id=\"panel-dashboard\">"
      "<div class=\"panel-heading\">Dashboard</div>"
      "<div class=\"panel-body\">"
        "<div class=\"receiver get-window-resize\" id=\"livestatus\">"
          "<div class=\"dashboard\" style=\"position: relative; width: 100%; height: 300px; margin: 0 auto\">"
            "<div class=\"overlay\">"
              "<div class=\"range-value\"><span class=\"value\">▼0 ▲0</span><span class=\"unit\">km</span></div>"
              "<div class=\"energy-value\"><span class=\"value\">▲0.0 ▼0.0</span><span class=\"unit\">kWh</span></div>"
            "</div>"
            "<div class=\"underlay\">"
              "<div class=\"voltage-value\"><span class=\"value\">0</span></div>"
              "<div class=\"soc-value\"><span class=\"value\">0</span></div>"
              "<div class=\"consumption-value\"><span class=\"value\">0</span></div>"
              "<div class=\"power-value\"><span class=\"value\">0</span></div>"
            "</div>"
            "<div id=\"gaugeset1\" style=\"width: 100%; height: 100%;\"></div>"
          "</div>"
        "</div>"
      "</div>"
    "</div>"
    ""
    "<script type=\"text/javascript\">"
    ""
    "var gaugeset1;"
    ""
    "function get_dashboard_data() {"
      "var rmin = metrics[\"v.b.range.est\"]||0, rmax = metrics[\"v.b.range.ideal\"]||0;\n"
      "var euse = metrics[\"v.b.energy.used\"]||0, erec = metrics[\"v.b.energy.recd\"]||0;\n"
      "var voltage = metrics[\"v.b.voltage\"]||0, soc = metrics[\"v.b.soc\"]||0;\n"
      "var consumption = metrics[\"v.b.consumption\"]||0, power = metrics[\"v.b.power\"]||0;\n"
      "euse = Math.floor(euse*10)/10; erec = Math.floor(erec*10)/10;"
      "if (rmin > rmax) { var x = rmin; rmin = rmax; rmax = x; }"
      "var md = {"
        "range: { value: \"▼\" + rmin.toFixed(0) + \" ▲\" + rmax.toFixed(0) },"
        "energy: { value: \"▲\" + euse.toFixed(1) + \" ▼\" + erec.toFixed(1) },"
        "voltage: { value: voltage.toFixed(0) },"
        "soc: { value: soc.toFixed(0) },"
        "consumption: { value: consumption.toFixed(0) },"
        "power: { value: power.toFixed(0) },"
        "series: ["
          "{ data: [metrics[\"v.p.speed\"]] },"
          "{ data: [metrics[\"v.b.voltage\"]] },"
          "{ data: [metrics[\"v.b.soc\"]] },"
          "{ data: [metrics[\"v.b.consumption\"]] },"
          "{ data: [metrics[\"v.b.power\"]] },"
          "{ data: [metrics[\"v.c.temp\"]] },"
          "{ data: [metrics[\"v.b.temp\"]] },"
          "{ data: [metrics[\"v.i.temp\"]] },"
          "{ data: [metrics[\"v.m.temp\"]] }],"
      "};"
      "return md;"
    "}"
    ""
    "function update_dashboard() {"
      "var md = get_dashboard_data();"
      "$('.range-value .value').text(md.range.value);"
      "$('.energy-value .value').text(md.energy.value);"
      "$('.voltage-value .value').text(md.voltage.value);"
      "$('.soc-value .value').text(md.soc.value);"
      "$('.consumption-value .value').text(md.consumption.value);"
      "$('.power-value .value').text(md.power.value);"
      "gaugeset1.update({ series: md.series });"
    "}"
    ""
    "function init_gaugeset1() {"
      "var chart_config = {"
        "chart: {"
          "type: 'gauge',"
          "spacing: [0, 0, 0, 0],"
          "margin: [0, 0, 0, 0],"
          "animation: { duration: 0, easing: 'swing' },"
        "},"
        "title: { text: null },"
        "credits: { enabled: false },"
        "tooltip: { enabled: false },"
        ""
        "pane: ["
          "{ startAngle: -125, endAngle: 125, center: ['50%', '45%'], size: '80%' }, /* Speed */"
          "{ startAngle: 70, endAngle: 110, center: ['-20%', '20%'], size: '100%' }, /* Voltage */"
          "{ startAngle: 70, endAngle: 110, center: ['-20%', '60%'], size: '100%' }, /* SOC */"
          "{ startAngle: -110, endAngle: -70, center: ['120%', '20%'], size: '100%' }, /* Efficiency */"
          "{ startAngle: -110, endAngle: -70, center: ['120%', '60%'], size: '100%' }, /* Power */"
          "{ startAngle: -45, endAngle: 45, center: ['20%', '100%'], size: '30%' }, /* Charger temperature */"
          "{ startAngle: -45, endAngle: 45, center: ['40%', '100%'], size: '30%' }, /* Battery temperature */"
          "{ startAngle: -45, endAngle: 45, center: ['60%', '100%'], size: '30%' }, /* Inverter temperature */"
          "{ startAngle: -45, endAngle: 45, center: ['80%', '100%'], size: '30%' }], /* Motor temperature */"
        ""
        "responsive: {"
          "rules: [{"
            "condition: { minWidth: 0, maxWidth: 400 },"
            "chartOptions: {"
              "pane: ["
                "{ size: '60%' }, /* Speed */"
                "{ center: ['-20%', '20%'] }, /* Voltage */"
                "{ center: ['-20%', '60%'] }, /* SOC */"
                "{ center: ['120%', '20%'] }, /* Efficiency */"
                "{ center: ['120%', '60%'] }, /* Power */"
                "{ center: ['15%', '100%']   , size: '25%' }, /* Charger temperature */"
                "{ center: ['38.33%', '100%'], size: '25%' }, /* Battery temperature */"
                "{ center: ['61.66%', '100%'], size: '25%' }, /* Inverter temperature */"
                "{ center: ['85%', '100%']   , size: '25%' }], /* Motor temperature */"
              "yAxis: [{ labels: { step: 1 } }], /* Speed */"
            "},"
          "},{"
            "condition: { minWidth: 401, maxWidth: 450 },"
            "chartOptions: {"
              "pane: ["
                "{ size: '70%' }, /* Speed */"
                "{ center: ['-15%', '20%'] }, /* Voltage */"
                "{ center: ['-15%', '60%'] }, /* SOC */"
                "{ center: ['115%', '20%'] }, /* Efficiency */"
                "{ center: ['115%', '60%'] }, /* Power */"
                "{ center: ['15%', '100%']   , size: '27.5%' }, /* Charger temperature */"
                "{ center: ['38.33%', '100%'], size: '27.5%' }, /* Battery temperature */"
                "{ center: ['61.66%', '100%'], size: '27.5%' }, /* Inverter temperature */"
                "{ center: ['85%', '100%']   , size: '27.5%' }], /* Motor temperature */"
            "},"
          "},{"
            "condition: { minWidth: 451, maxWidth: 600 },"
            "chartOptions: {"
              "pane: ["
                "{ size: '80%' }, /* Speed */"
                "{ center: ['-10%', '20%'] }, /* Voltage */"
                "{ center: ['-10%', '60%'] }, /* SOC */"
                "{ center: ['110%', '20%'] }, /* Efficiency */"
                "{ center: ['110%', '60%'] }], /* Power */"
            "},"
          "},{"
            "condition: { minWidth: 601 },"
            "chartOptions: {"
              "pane: ["
                "{ size: '85%' }, /* Speed */"
                "{ center: ['0%', '20%'] }, /* Voltage */"
                "{ center: ['0%', '60%'] }, /* SOC */"
                "{ center: ['100%', '20%'] }, /* Efficiency */"
                "{ center: ['100%', '60%'] }], /* Power */"
            "},"
          "}]"
        "},"
        ""
        "yAxis: [{"
          "/* Speed axis: */"
          "pane: 0, className: 'speed', title: { text: 'km/h' },"
          "reversed: false,"
          "minorTickInterval: 'auto', minorTickLength: 5, minorTickPosition: 'inside',"
          "tickPixelInterval: 30, tickPosition: 'inside', tickLength: 13,"
          "labels: { step: 2, distance: -28, x: 0, y: 5, zIndex: 2 },"
        "},{"
          "/* Voltage axis: */"
          "pane: 1, className: 'voltage', title: { text: 'Volt', align: 'low', x: 85, y: 35, },"
          "reversed: true,"
          "minorTickInterval: 'auto', minorTickLength: 5, minorTickPosition: 'inside',"
          "tickPixelInterval: 30, tickPosition: 'inside', tickLength: 13,"
          "labels: { step: 1, distance: -25, x: 0, y: 5, zIndex: 2 },"
        "},{"
          "/* SOC axis: */"
          "pane: 2, className: 'soc', title: { text: 'SOC', align: 'low', x: 82.5, y: 35 },"
          "reversed: true,"
          "minorTickInterval: 'auto', minorTickLength: 5, minorTickPosition: 'inside',"
          "tickPixelInterval: 30, tickPosition: 'inside', tickLength: 13,"
          "labels: { step: 1, distance: -25, x: 0, y: 5, zIndex: 2 },"
        "},{"
          "/* Efficiency axis: */"
          "pane: 3, className: 'efficiency', title: { text: 'Wh/km', align: 'low', x: -116, y: 35 },"
          "reversed: false,"
          "minorTickInterval: 'auto', minorTickLength: 5, minorTickPosition: 'inside',"
          "tickPixelInterval: 30, tickPosition: 'inside', tickLength: 13,"
          "labels: { step: 1, distance: -25, x: 0, y: 5, zIndex: 2 },"
        "},{"
          "/* Power axis: */"
          "pane: 4, className: 'power', title: { text: 'kW', align: 'low', x: -105, y: 35 },"
          "reversed: false,"
          "minorTickInterval: 'auto', minorTickLength: 5, minorTickPosition: 'inside',"
          "tickPixelInterval: 30, tickPosition: 'inside', tickLength: 13,"
          "labels: { step: 1, distance: -25, x: 0, y: 5, zIndex: 2 },"
        "},{"
          "/* Charger temperature axis: */"
          "pane: 5, className: 'temp-charger', title: { text: 'CHG °C', y: 10 },"
          "tickPosition: 'inside', tickLength: 10, minorTickInterval: null,"
          "labels: { step: 1, distance: 3, x: 0, y: 0, zIndex: 2 },"
        "},{"
          "/* Battery temperature axis: */"
          "pane: 6, className: 'temp-battery', title: { text: 'BAT °C', y: 10 },"
          "tickPosition: 'inside', tickLength: 10, minorTickInterval: null,"
          "labels: { step: 1, distance: 3, x: 0, y: 0, zIndex: 2 },"
        "},{"
          "/* Inverter temperature axis: */"
          "pane: 7, className: 'temp-inverter', title: { text: 'PEM °C', y: 10 },"
          "tickPosition: 'inside', tickLength: 10, minorTickInterval: null,"
          "labels: { step: 1, distance: 3, x: 0, y: 0, zIndex: 2 },"
        "},{"
          "/* Motor temperature axis: */"
          "pane: 8, className: 'temp-motor', title: { text: 'MOT °C', y: 10 },"
          "tickPosition: 'inside', tickLength: 10, minorTickInterval: null,"
          "labels: { step: 1, distance: 3, x: 0, y: 0, zIndex: 2 },"
        "}],"
        ""
        "plotOptions: {"
          "gauge: {"
            "dataLabels: { enabled: false },"
            "overshoot: 1"
          "}"
        "},"
        "series: [{"
          "yAxis: 0, name: 'Speed', className: 'speed fullgauge', data: [0],"
          "pivot: { radius: '10' },"
          "dial: { radius: '88%', topWidth: 1, baseLength: '20%', baseWidth: 10, rearLength: '20%' },"
        "},{"
          "yAxis: 1, name: 'Voltage', className: 'voltage auxgauge', data: [0],"
          "pivot: { radius: '85' },"
          "dial: { radius: '95%', baseWidth: 5, baseLength: '90%' },"
        "},{"
          "yAxis: 2, name: 'SOC', className: 'soc auxgauge', data: [0],"
          "pivot: { radius: '85' },"
          "dial: { radius: '95%', baseWidth: 5, baseLength: '90%' },"
        "},{"
          "yAxis: 3, name: 'Efficiency', className: 'efficiency auxgauge', data: [0],"
          "pivot: { radius: '85' },"
          "dial: { radius: '95%', baseWidth: 5, baseLength: '90%' },"
        "},{"
          "yAxis: 4, name: 'Power', className: 'power auxgauge', data: [0],"
          "pivot: { radius: '85' },"
          "dial: { radius: '95%', baseWidth: 5, baseLength: '90%' },"
        "},{"
          "yAxis: 5, name: 'Charger temperature', className: 'temp-charger tempgauge', data: [0],"
          "dial: { radius: '90%', baseWidth: 3, baseLength: '90%' },"
        "},{"
          "yAxis: 6, name: 'Battery temperature', className: 'temp-battery tempgauge', data: [0],"
          "dial: { radius: '90%', baseWidth: 3, baseLength: '90%' },"
        "},{"
          "yAxis: 7, name: 'Inverter temperature', className: 'temp-inverter tempgauge', data: [0],"
          "dial: { radius: '90%', baseWidth: 3, baseLength: '90%' },"
        "},{"
          "yAxis: 8, name: 'Motor temperature', className: 'temp-motor tempgauge', data: [0],"
          "dial: { radius: '90%', baseWidth: 3, baseLength: '90%' },"
        "}]"
      "};"
      ""
      "/* Inject vehicle config: */"
      "for (var i = 0; i < chart_config.yAxis.length; i++) {"
        "$.extend(chart_config.yAxis[i], vehicle_config.yAxis[i]);"
      "}"
      ""
      "gaugeset1 = Highcharts.chart('gaugeset1', chart_config,"
        "function (chart) {"
          "chart.update({ chart: { animation: { duration: 1000, easing: 'swing' } } });"
          "$('#livestatus').on(\"msg:metrics\", function(e, update){"
            "update_dashboard();"
          "}).on(\"window-resize\", function(e){"
            "chart.reflow();"
          "});"
        "}"
      ");"
      "$('#gaugeset1').data('chart', gaugeset1).addClass('has-chart');"
    "}"
    ""
    "function init_charts() {"
      "init_gaugeset1();"
    "}"
    ""
    "if (window.Highcharts) {"
      "init_charts();"
    "} else {"
      "$.ajax({"
        "url: \"" URL_ASSETS_CHARTS_JS "\","
        "dataType: \"script\","
        "cache: true,"
        "success: function(){ init_charts(); }"
      "});"
    "}"
    ""
    "</script>";
  
  c.head(200);
  PAGE_HOOK("body.pre");
  c.printf(
    "<script type=\"text/javascript\">"
    "var vehicle_config = {"
      "%s"
    "};"
    "</script>"
    , cfg.gaugeset1.c_str());
  new HttpDataSender(c.nc, (const uint8_t*)content, strlen(content));
}


/**
 * HandleBmsCellMonitor: display cell voltages & temperatures
 * 
 * Note: this is not enabled by default, as some vehicles do not provide BMS data.
 * To enable, include this in the vehicle init:
 *   MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
 * You can change the URL path, title, menu association and authentication as you like.
 * For a clean shutdown, add
 *   MyWebServer.DeregisterPage("/bms/cellmon");
 * …in your vehicle cleanup.
 */
void OvmsWebServer::HandleBmsCellMonitor(PageEntry_t& p, PageContext_t& c)
{
  float stemwidth_v = 0.5, stemwidth_t = 0.5;
  float
    volt_warn_def = BMS_DEFTHR_VWARN, volt_alert_def = BMS_DEFTHR_VALERT,
    volt_maxgrad_def = BMS_DEFTHR_VMAXGRAD, volt_maxsddev_def = BMS_DEFTHR_VMAXSDDEV,
    temp_warn_def = BMS_DEFTHR_TWARN, temp_alert_def = BMS_DEFTHR_TALERT;
  float
    volt_warn = 0, volt_alert = 0,
    volt_maxgrad = 0, volt_maxsddev = 0,
    temp_warn = 0, temp_alert = 0;
  bool alerts_enabled = true;

  // get vehicle BMS configuration:
  OvmsVehicle* vehicle = MyVehicleFactory.ActiveVehicle();
  if (vehicle) {
    int readings_v = vehicle->BmsGetCellArangementVoltage();
    if (readings_v) {
      stemwidth_v   = 0.1 + 20.0 / readings_v;  // 14 → 1.5 … 96 → 0.3
    }
    int readings_t = vehicle->BmsGetCellArangementTemperature();
    if (readings_t) {
      stemwidth_t   = 0.1 + 10.0 / readings_t;  //  7 → 1.5 … 96 → 0.4
    }
    vehicle->BmsGetCellDefaultThresholdsVoltage(&volt_warn_def, &volt_alert_def, &volt_maxgrad_def, &volt_maxsddev_def);
    vehicle->BmsGetCellDefaultThresholdsTemperature(&temp_warn_def, &temp_alert_def);
  }

  if (c.method == "POST") {
    // process form submission:
    
    volt_warn = atof(c.getvar("volt_warn").c_str()) / 1000;
    if (volt_warn > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.warn", volt_warn);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.warn", "");

    volt_alert = atof(c.getvar("volt_alert").c_str()) / 1000;
    if (volt_alert > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.alert", volt_alert);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.alert", "");

    volt_maxgrad = atof(c.getvar("volt_maxgrad").c_str()) / 1000;
    if (volt_maxgrad > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.maxgrad", volt_maxgrad);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.maxgrad", "");

    volt_maxsddev = atof(c.getvar("volt_maxsddev").c_str()) / 1000;
    if (volt_maxsddev > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.voltage.maxsddev", volt_maxsddev);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.voltage.maxsddev", "");

    temp_warn = atof(c.getvar("temp_warn").c_str());
    if (temp_warn > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.temp.warn", temp_warn);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.temp.warn", "");

    temp_alert = atof(c.getvar("temp_alert").c_str());
    if (temp_alert > 0)
      MyConfig.SetParamValueFloat("vehicle", "bms.dev.temp.alert", temp_alert);
    else
      MyConfig.SetParamValue("vehicle", "bms.dev.temp.alert", "");

    MyConfig.SetParamValueBool("vehicle", "bms.alerts.enabled", c.getvar("alerts_enabled") == "yes");
    
    if (c.getvar("action") == "save-reset" && vehicle)
      vehicle->BmsResetCellStats();
    
    c.head(200);
    c.alert("success", "<p>Saved.</p>");
    c.print("<script>after(2, function(){ $('.modal').modal('hide'); });</script>");
    c.done();
    return;
  }

  // read config:
  volt_warn = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.warn");
  volt_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.alert");
  volt_maxgrad = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.maxgrad");
  volt_maxsddev = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.maxsddev");
  temp_warn = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.warn");
  temp_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.alert");
  alerts_enabled = MyConfig.GetParamValueBool("vehicle", "bms.alerts.enabled", true);
  
  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<div class=\"panel panel-primary panel-single receiver\" id=\"livestatus\">\n"
      "<div class=\"panel-heading\">BMS Cell Monitor</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"row\">\n"
          "<div id=\"voltchart\" style=\"width: 100%; max-width: 100%; height: 45vh; min-height: 280px; margin: 0 auto\"></div>\n"
          "<div id=\"tempchart\" style=\"width: 100%; max-width: 100%; height: 25vh; min-height: 160px; margin: 0 auto\"></div>\n"
        "</div>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<table class=\"table table-bordered table-condensed\">\n"
          "<tbody>\n"
            "<tr>\n"
              "<th>Voltage</th>\n"
              "<td>\n"
              "<div class=\"metric number\" data-prec=\"3\" data-metric=\"v.b.p.voltage.avg\"><span class=\"label\">Cell avg</span><span class=\"value\">–</span><span class=\"unit\">V</span></div>\n"
              "<div class=\"metric number\" data-prec=\"3\" data-metric=\"v.b.p.voltage.min\"><span class=\"label\">Cell min</span><span class=\"value\">–</span><span class=\"unit\">V</span></div>\n"
              "<div class=\"metric number\" data-prec=\"3\" data-metric=\"v.b.p.voltage.max\"><span class=\"label\">Cell max</span><span class=\"value\">–</span><span class=\"unit\">V</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-scale=\"1000\" data-metric=\"v.b.p.voltage.stddev\"><span class=\"label\">StdDev</span><span class=\"value\">–</span><span class=\"unit\">mV</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-scale=\"1000\" data-metric=\"v.b.p.voltage.stddev.max\"><span class=\"label\">StdDev max</span><span class=\"value\">–</span><span class=\"unit\">mV</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-scale=\"1000\" data-metric=\"v.b.p.voltage.grad\"><span class=\"label\">Gradient</span><span class=\"value\">–</span><span class=\"unit\">mV</span></div>\n"
              "</td>\n"
            "</tr>\n"
            "<tr>\n"
              "<th>Temperature</th>\n"
              "<td>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.p.temp.avg\"><span class=\"label\">Cell avg</span><span class=\"value\">–</span><span class=\"unit\">°C</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.p.temp.min\"><span class=\"label\">Cell min</span><span class=\"value\">–</span><span class=\"unit\">°C</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.p.temp.max\"><span class=\"label\">Cell max</span><span class=\"value\">–</span><span class=\"unit\">°C</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.p.temp.stddev\"><span class=\"label\">StdDev</span><span class=\"value\">–</span><span class=\"unit\">°C</span></div>\n"
              "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.p.temp.stddev.max\"><span class=\"label\">StdDev max</span><span class=\"value\">–</span><span class=\"unit\">°C</span></div>\n"
              "</td>\n"
            "</tr>\n"
          "</tbody>\n"
        "</table>\n"
        "<button class=\"btn btn-default\" data-toggle=\"modal\" data-target=\"#cfg-dialog\">Alert config</button>\n"
        "<button class=\"btn btn-default\" data-cmd=\"bms reset\" data-target=\"#output\" data-watchcnt=\"0\">Reset min/max</button>\n"
        "<samp id=\"output\" class=\"samp-inline\"></samp>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<div class=\"modal fade\" id=\"cfg-dialog\" role=\"dialog\" data-backdrop=\"true\" data-keyboard=\"true\">\n"
      "<div class=\"modal-dialog modal-lg\">\n"
        "<div class=\"modal-content\">\n"
          "<div class=\"modal-header\">\n"
            "<button type=\"button\" class=\"close\" data-dismiss=\"modal\">&times;</button>\n"
            "<h4 class=\"modal-title\">Alert configuration</h4>\n"
          "</div>\n"
          "<div class=\"modal-body\">\n");

  // Alert configuration form:

  std::ostringstream sval, sdef;
  sval << std::fixed;
  sdef << std::fixed;
  
  c.form_start(p.uri, "#cfg-result");

  c.input_info("Thresholds",
    "<p>Deviation warnings and alerts are triggered by cell deviation from the current average.</p>"
    "<p>You may need to change the thresholds to adapt to the quality and age of your battery.</p>");

  sval << std::setprecision(0);
  sdef << std::setprecision(0);
  sval.str(""); if (volt_warn > 0) sval << volt_warn * 1000;
  sdef.str(""); sdef << "Default: " << volt_warn_def * 1000;
  c.input("number", "Voltage warning", "volt_warn", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"1\" step=\"1\"", "mV");
  sval.str(""); if (volt_alert > 0) sval << volt_alert * 1000;
  sdef.str(""); sdef << "Default: " << volt_alert_def * 1000;
  c.input("number", "Voltage alert", "volt_alert", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"1\" step=\"1\"", "mV");

  sval << std::setprecision(1);
  sdef << std::setprecision(1);
  sval.str(""); if (volt_maxgrad > 0) sval << volt_maxgrad * 1000;
  sdef.str(""); sdef << "Default: " << volt_maxgrad_def * 1000;
  c.input("number", "Max valid gradient", "volt_maxgrad", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0\" step=\"0.1\"", "mV");
  sval.str(""); if (volt_maxsddev > 0) sval << volt_maxsddev * 1000;
  sdef.str(""); sdef << "Default: " << volt_maxsddev_def * 1000;
  c.input("number", "Max stddev deviation", "volt_maxsddev", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0\" step=\"0.1\"", "mV");

  sval << std::setprecision(1);
  sdef << std::setprecision(1);
  sval.str(""); if (temp_warn > 0) sval << temp_warn;
  sdef.str(""); sdef << "Default: " << temp_warn_def;
  c.input("number", "Temperature warning", "temp_warn", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0.1\" step=\"0.1\"", "°C");
  sval.str(""); if (temp_alert > 0) sval << temp_alert;
  sdef.str(""); sdef << "Default: " << temp_alert_def;
  c.input("number", "Temperature alert", "temp_alert", sval.str().c_str(), sdef.str().c_str(),
    NULL, "min=\"0.1\" step=\"0.1\"", "°C");

  c.input_checkbox("Enable alert notifications", "alerts_enabled", alerts_enabled);

  c.print(
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save\">Save</button> "
        "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"save-reset\">Save &amp; reset alerts</button> "
      "</div>"
    "</div>"
    "<div class=\"form-group\">"
      "<div class=\"col-sm-offset-3 col-sm-9\">"
        "<div id=\"cfg-result\" class=\"modal-autoclear\"></div>"
      "</div>"
    "</div>");
  c.form_end();

  c.print(
          "</div>\n"
          "<div class=\"modal-footer\">\n"
            "<button type=\"button\" class=\"btn btn-default\" data-dismiss=\"modal\">Close</button>\n"
          "</div>\n"
        "</div>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "\n"
    "<style>\n"
    "\n"
    ".highcharts-boxplot-stem {\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".highcharts-point {\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".night .highcharts-columnrange-series .highcharts-point {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    ".night .highcharts-legend-item text {\n"
      "fill: #dddddd;\n"
    "}\n"
    "\n"
    ".highcharts-plot-line {\n"
      "fill: none;\n"
      "stroke: #3625c3;\n"
      "stroke-width: 2px;\n"
    "}\n"
    ".night .highcharts-plot-line {\n"
      "stroke: #728def;\n"
    "}\n"
    ".highcharts-plot-band {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
    "}\n"
    ".night .highcharts-plot-band {\n"
      "fill: #728def;\n"
      "fill-opacity: 0.2;\n"
    "}\n"
    "\n"
    ".plot-line-sdmaxlo {\n"
      "stroke: #3625c3;\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".plot-line-sdmaxhi {\n"
      "stroke: #3625c3;\n"
      "stroke-width: 1px;\n"
    "}\n"
    ".night .plot-line-sdmaxlo {\n"
      "stroke: #728def;\n"
    "}\n"
    ".night .plot-line-sdmaxhi {\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    ".highcharts-plot-line-label {\n"
      "fill: #888;\n"
      "font-size: 10px;\n"
    "}\n"
    "\n"
    ".highcharts-color-1 {\n"
      "fill: #ffb500;\n"
      "stroke: #cc950e;\n"
    "}\n"
    ".highcharts-color-2 {\n"
      "fill: #ff3400;\n"
      "stroke: #ff3400;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
      "stroke: #3625c3;\n"
    "}\n"
    ".night #voltchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #728def;\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-boxplot-stem {\n"
      "fill: #63cc7a;\n"
      "fill-opacity: 1;\n"
      "stroke: #63cc7a;\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".night #voltchart .highcharts-boxplot-stem {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    "#voltchart .highcharts-boxplot-median {\n"
      "stroke: #193e07;\n"
      "stroke-width: 5px;\n"
      "stroke-linecap: round;\n"
    "}\n"
    ".night #voltchart .highcharts-boxplot-median {\n"
      "stroke: #afff45;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #3625c3;\n"
      "fill-opacity: 0.1;\n"
      "stroke: #3625c3;\n"
    "}\n"
    ".night #tempchart .highcharts-point.highcharts-color-0 {\n"
      "fill: #728def;\n"
      "stroke: #728def;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-boxplot-stem {\n"
      "fill: #da9073;\n"
      "fill-opacity: 1;\n"
      "stroke: #da9073;\n"
      "stroke-opacity: 1;\n"
    "}\n"
    ".night #tempchart .highcharts-boxplot-stem {\n"
      "fill-opacity: 0.6;\n"
      "stroke-opacity: 0.6;\n"
    "}\n"
    "\n"
    "#tempchart .highcharts-boxplot-median {\n"
      "stroke: #7d1313;\n"
      "stroke-width: 5px;\n"
      "stroke-linecap: round;\n"
    "}\n"
    ".night #tempchart .highcharts-boxplot-median {\n"
      "stroke: #fdd02e;\n"
    "}\n");
  
  c.printf(
    "#voltchart .highcharts-boxplot-stem {\n"
      "stroke-width: %.1f%%;\n"
    "}\n"
    "#tempchart .highcharts-boxplot-stem {\n"
      "stroke-width: %.1f%%;\n"
    "}\n"
    "</style>\n"
    , stemwidth_v, stemwidth_t);

  c.print(
    "<script>\n"
    "\n"
    "/**\n"
     "* Cell voltage chart\n"
     "*/\n"
    "\n"
    "var voltchart;\n"
    "\n"
    "// get_volt_data: build boxplot dataset from metrics\n"
    "function get_volt_data() {\n"
      "var data = { cells: [], volts: [], devmax: [], voltmean: 0, sdlo: 0, sdhi: 0, sdmaxlo: 0, sdmaxhi: 0 };\n"
      "var cnt = metrics[\"v.b.c.voltage\"] ? metrics[\"v.b.c.voltage\"].length : 0;\n"
      "if (cnt == 0)\n"
        "return data;\n"
      "var i, act, min, max, devmax, dalert, dlow, dhigh;\n"
      "data.voltmean = metrics[\"v.b.p.voltage.avg\"] || 0;\n"
      "data.sdlo = data.voltmean - (metrics[\"v.b.p.voltage.stddev\"] || 0);\n"
      "data.sdhi = data.voltmean + (metrics[\"v.b.p.voltage.stddev\"] || 0);\n"
      "data.sdmaxlo = data.voltmean - (metrics[\"v.b.p.voltage.stddev.max\"] || 0);\n"
      "data.sdmaxhi = data.voltmean + (metrics[\"v.b.p.voltage.stddev.max\"] || 0);\n"
      "for (i=0; i<cnt; i++) {\n"
        "act = metrics[\"v.b.c.voltage\"][i];\n"
        "min = metrics[\"v.b.c.voltage.min\"][i] || act;\n"
        "max = metrics[\"v.b.c.voltage.max\"][i] || act;\n"
        "devmax = metrics[\"v.b.c.voltage.dev.max\"][i] || 0;\n"
        "dalert = metrics[\"v.b.c.voltage.alert\"][i] || 0;\n"
        "if (devmax > 0) {\n"
          "dlow = data.voltmean;\n"
          "dhigh = data.voltmean + devmax;\n"
        "} else {\n"
          "dlow = data.voltmean + devmax;\n"
          "dhigh = data.voltmean;\n"
        "}\n"
        "data.cells.push(i+1);\n"
        "data.volts.push([min,act,act,act,max]);\n"
        "data.devmax.push({ x:i, low:dlow, high:dhigh, colorIndex:dalert,\n"
            "id: ((devmax < 0) ? '▼ ' : '▲ ') + Math.abs(devmax * 1000).toFixed(0) + ' mV' });\n"
      "}\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_volt_chart() {\n"
      "var data = get_volt_data();\n"
      "voltchart.xAxis[0].setCategories(data.cells);\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-mean');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-mean', className: 'plot-line-mean', value: data.voltmean, zIndex: 3 });\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-sdmaxlo');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 });\n"
      "voltchart.yAxis[0].removePlotLine('plot-line-sdmaxhi');\n"
      "voltchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } });\n"
      "voltchart.yAxis[0].removePlotBand('plot-band-sd');\n"
      "voltchart.yAxis[0].addPlotBand({ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi });\n"
      "voltchart.series[0].setData(data.volts);\n"
      "voltchart.series[1].setData(data.devmax);\n"
    "}\n"
    "\n"
    "function init_volt_chart() {\n"
      "var data = get_volt_data();\n"
      "voltchart = Highcharts.chart('voltchart', {\n"
        "chart: {\n"
          "type: 'boxplot',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"v.b.c.voltage\"] != null\n"
                 "|| update[\"v.b.c.voltage.min\"] != null\n"
                 "|| update[\"v.b.c.voltage.max\"] != null\n"
                 "|| update[\"v.b.c.voltage.dev.max\"] != null\n"
                 "|| update[\"v.b.c.voltage.alert\"] != null)\n"
                  "update_volt_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'y',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "enabled: true,\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
          "margin: 2,\n"
          "padding: 2,\n"
        "},\n"
        "xAxis: { categories: data.cells },\n"
        "yAxis: {\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.2f}V\" },\n"
          "minTickInterval: 0.01,\n"
          "minorTickInterval: 'auto',\n"
          "plotLines: [\n"
            "{ id: 'plot-line-mean', className: 'plot-line-mean', value: data.voltmean, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } },\n"
          "],\n"
          "plotBands: [{ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi }],\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "padding: 4,\n"
          "positioner: function (labelWidth, labelHeight, point) {\n"
            "return {\n"
              "x: voltchart.plotLeft + Math.min(voltchart.plotWidth - labelWidth, Math.max(0, point.plotX - labelWidth/2)),\n"
              "y: 0 };\n"
          "},\n"
          "headerFormat: 'Voltage #{point.key}: ',\n"
          "pointFormatter: function () {\n"
            "if (this.series.index == 0) {\n"
              "return '<b>' + this.median.toFixed(3) + ' V</b>'\n"
                "+ '  [' + this.low.toFixed(3) + ' – ' + this.high.toFixed(3) + ']<br/>';\n"
            "} else {\n"
              "return 'Max deviation: <b>' + this.id + '</b>';\n"
            "}\n"
          "},\n"
        "},\n"
        "series: [{\n"
          "name: 'Voltage',\n"
          "zIndex: 1,\n"
          "data: data.volts,\n"
          "whiskerLength: 0,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max deviation',\n"
          "zIndex: 0,\n"
          "type: 'columnrange',\n"
          "data: data.devmax,\n"
          "maxPointWidth: 35,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#voltchart').data('chart', voltchart).addClass('has-chart');"
    "}\n"
    "\n"
    "\n"
    "/**\n"
     "* Cell temperature chart\n"
     "*/\n"
    "\n"
    "var tempchart;\n"
    "\n"
    "// get_temp_data: build boxplot dataset from metrics\n"
    "function get_temp_data() {\n"
      "var data = { cells: [], temps: [], devmax: [], tempmean: 0, sdlo: 0, sdhi: 0, sdmaxlo: 0, sdmaxhi: 0 };\n"
      "var cnt = metrics[\"v.b.c.temp\"] ? metrics[\"v.b.c.temp\"].length : 0;\n"
      "if (cnt == 0)\n"
        "return data;\n"
      "var i, act, min, max, devmax, dalert, dlow, dhigh;\n"
      "data.tempmean = metrics[\"v.b.p.temp.avg\"] || 0;\n"
      "data.sdlo = data.tempmean - (metrics[\"v.b.p.temp.stddev\"] || 0);\n"
      "data.sdhi = data.tempmean + (metrics[\"v.b.p.temp.stddev\"] || 0);\n"
      "data.sdmaxlo = data.tempmean - (metrics[\"v.b.p.temp.stddev.max\"] || 0);\n"
      "data.sdmaxhi = data.tempmean + (metrics[\"v.b.p.temp.stddev.max\"] || 0);\n"
      "for (i=0; i<cnt; i++) {\n"
        "act = metrics[\"v.b.c.temp\"][i];\n"
        "min = metrics[\"v.b.c.temp.min\"][i] || act;\n"
        "max = metrics[\"v.b.c.temp.max\"][i] || act;\n"
        "devmax = metrics[\"v.b.c.temp.dev.max\"][i] || 0;\n"
        "dalert = metrics[\"v.b.c.temp.alert\"][i] || 0;\n"
        "if (devmax > 0) {\n"
          "dlow = data.tempmean;\n"
          "dhigh = data.tempmean + devmax;\n"
        "} else {\n"
          "dlow = data.tempmean + devmax;\n"
          "dhigh = data.tempmean;\n"
        "}\n"
        "data.cells.push(i+1);\n"
        "data.temps.push([min,act,act,act,max]);\n"
        "data.devmax.push({ x:i, low:dlow, high:dhigh, colorIndex:dalert,\n"
            "id: ((devmax < 0) ? '▼ ' : '▲ ') + Math.abs(devmax).toFixed(1) + ' °C' });\n"
      "}\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_temp_chart() {\n"
      "var data = get_temp_data();\n"
      "tempchart.xAxis[0].setCategories(data.cells);\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-mean');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-mean', className: 'plot-line-mean', value: data.tempmean, zIndex: 3 });\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-sdmaxlo');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 });\n"
      "tempchart.yAxis[0].removePlotLine('plot-line-sdmaxhi');\n"
      "tempchart.yAxis[0].addPlotLine({ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } });\n"
      "tempchart.yAxis[0].removePlotBand('plot-band-sd');\n"
      "tempchart.yAxis[0].addPlotBand({ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi });\n"
      "tempchart.series[0].setData(data.temps);\n"
      "tempchart.series[1].setData(data.devmax);\n"
    "}\n"
    "\n"
    "function init_temp_chart() {\n"
      "var data = get_temp_data();\n"
      "tempchart = Highcharts.chart('tempchart', {\n"
        "chart: {\n"
          "type: 'boxplot',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"v.b.c.temp\"] != null\n"
                 "|| update[\"v.b.c.temp.min\"] != null\n"
                 "|| update[\"v.b.c.temp.max\"] != null\n"
                 "|| update[\"v.b.c.temp.dev.max\"] != null\n"
                 "|| update[\"v.b.c.temp.alert\"] != null)\n"
                  "update_temp_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'y',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "enabled: true,\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
          "margin: 2,\n"
          "padding: 2,\n"
        "},\n"
        "xAxis: { categories: data.cells },\n"
        "yAxis: {\n"
          "title: { text: null },\n"
          "labels: { format: \"{value:.0f} °C\" },\n"
          "minTickInterval: 1,\n"
          "minorTickInterval: 'auto',\n"
          "plotLines: [\n"
            "{ id: 'plot-line-mean', className: 'plot-line-mean', value: data.tempmean, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxlo', className: 'plot-line-sdmaxlo', value: data.sdmaxlo, zIndex: 3 },\n"
            "{ id: 'plot-line-sdmaxhi', className: 'plot-line-sdmaxhi', value: data.sdmaxhi, zIndex: 3, label: { text: 'Max Std Dev' } },\n"
          "],\n"
          "plotBands: [{ id: 'plot-band-sd', className: 'plot-band-sd', from: data.sdlo, to: data.sdhi }],\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "padding: 4,\n"
          "positioner: function (labelWidth, labelHeight, point) {\n"
            "return {\n"
              "x: tempchart.plotLeft + Math.min(tempchart.plotWidth - labelWidth, Math.max(0, point.plotX - labelWidth/2)),\n"
              "y: 0 };\n"
          "},\n"
          "headerFormat: 'Temperature #{point.key}: ',\n"
          "pointFormatter: function () {\n"
            "if (this.series.index == 0) {\n"
              "return '<b>' + this.median.toFixed(1) + ' °C</b>'\n"
                "+ '  [' + this.low.toFixed(1) + ' – ' + this.high.toFixed(1) + ']<br/>';\n"
            "} else {\n"
              "return 'Max deviation: <b>' + this.id + '</b>';\n"
            "}\n"
          "},\n"
        "},\n"
        "series: [{\n"
          "name: 'Temperature',\n"
          "zIndex: 1,\n"
          "data: data.temps,\n"
          "whiskerLength: 0,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max deviation',\n"
          "zIndex: 0,\n"
          "type: 'columnrange',\n"
          "data: data.devmax,\n"
          "maxPointWidth: 35,\n"
          "animation: {\n"
            "duration: 100,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#tempchart').data('chart', tempchart).addClass('has-chart');"
    "}\n"
    "\n"
    "\n"
    "/**\n"
     "* Chart initialization\n"
     "*/\n"
    "\n"
    "function init_charts() {\n"
      "init_volt_chart();\n"
      "init_temp_chart();\n"
    "}\n"
    "\n"
    "if (window.Highcharts) {\n"
      "init_charts();\n"
    "} else {\n"
      "$.ajax({\n"
        "url: \"" URL_ASSETS_CHARTS_JS "\",\n"
        "dataType: \"script\",\n"
        "cache: true,\n"
        "success: function(){ init_charts(); }\n"
      "});\n"
    "}\n"
    "\n"
    "</script>\n");
  
  PAGE_HOOK("body.post");
  c.done();
}

/**
 * HandleCfgBrakelight: configure vehicle brake light control
 * 
 * Note: this is not enabled by default, as most vehicles do not need this.
 * To enable, include this in the vehicle init:
 *   MyWebServer.RegisterPage("/cfg/brakelight", "Brake light control", OvmsWebServer::HandleCfgBrakelight, PageMenu_Vehicle, PageAuth_Cookie);
 * You can change the URL path, title, menu association and authentication as you like.
 * For a clean shutdown, add
 *   MyWebServer.DeregisterPage("/cfg/brakelight");
 * …in your vehicle cleanup.
 */
void OvmsWebServer::HandleCfgBrakelight(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  bool enable, ignftbrk;
  std::string smooth_acc, smooth_bat, port, level_on, level_off, basepwr;

  if (c.method == "POST") {
    // process form submission:
    enable = (c.getvar("enable") == "yes");
    smooth_acc = c.getvar("smooth_acc");
    smooth_bat = c.getvar("smooth_bat");
    port = c.getvar("port");
    level_on = c.getvar("level_on");
    level_off = c.getvar("level_off");
    ignftbrk = (c.getvar("ignftbrk") == "yes");
    basepwr = c.getvar("basepwr");

    // validate:
    if (smooth_acc != "") {
      int v = atof(smooth_acc.c_str());
      if (v < 0) {
        error += "<li data-input=\"smooth_acc\">Acceleration smoothing must be greater or equal 0.</li>";
      }
    }
    if (smooth_bat != "") {
      int v = atof(smooth_bat.c_str());
      if (v < 0) {
        error += "<li data-input=\"smooth_bat\">Battery power smoothing must be greater or equal 0.</li>";
      }
    }
    if (port != "") {
      int v = atoi(port.c_str());
      if (v == 2 || v < 1 || v > 9) {
        error += "<li data-input=\"port\">Port must be one of 1, 3…9</li>";
      }
    }
    if (level_on != "") {
      int v = atof(level_on.c_str());
      if (v < 0) {
        error += "<li data-input=\"level_on\">Activation level must be greater or equal 0.</li>";
      }
    }
    if (level_off != "") {
      int v = atof(level_off.c_str());
      if (v < 0) {
        error += "<li data-input=\"level_off\">Deactivation level must be greater or equal 0.</li>";
      }
    }

    if (basepwr != "") {
      int v = atof(basepwr.c_str());
      if (v < 0) {
        error += "<li data-input=\"basepwr\">Base power level must be greater or equal 0.</li>";
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("vehicle", "accel.smoothing", smooth_acc);
      MyConfig.SetParamValue("vehicle", "batpwr.smoothing", smooth_bat);
      MyConfig.SetParamValueBool("vehicle", "brakelight.enable", enable);
      MyConfig.SetParamValue("vehicle", "brakelight.port", port);
      MyConfig.SetParamValue("vehicle", "brakelight.on", level_on);
      MyConfig.SetParamValue("vehicle", "brakelight.off", level_off);
      MyConfig.SetParamValueBool("vehicle", "brakelight.ignftbrk", ignftbrk);
      MyConfig.SetParamValue("vehicle", "brakelight.basepwr", basepwr);

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
    // read configuration:
    smooth_acc = MyConfig.GetParamValue("vehicle", "accel.smoothing");
    smooth_bat = MyConfig.GetParamValue("vehicle", "batpwr.smoothing");
    enable = MyConfig.GetParamValueBool("vehicle", "brakelight.enable", false);
    port = MyConfig.GetParamValue("vehicle", "brakelight.port", "1");
    level_on = MyConfig.GetParamValue("vehicle", "brakelight.on");
    level_off = MyConfig.GetParamValue("vehicle", "brakelight.off");
    ignftbrk = MyConfig.GetParamValueBool("vehicle", "brakelight.ignftbrk", false);
    basepwr = MyConfig.GetParamValue("vehicle", "brakelight.basepwr");
    c.head(200);
  }

  // generate form:
  c.panel_start("primary", "Regen Brake Light");
  c.form_start(p.uri);

  c.input_slider("Acceleration smoothing", "smooth_acc", 3, NULL,
    -1, smooth_acc.empty() ? 2.0 : atof(smooth_acc.c_str()), 2.0, 0.0, 10.0, 0.1,
    "<p>Speed/acceleration smoothing eliminates road bump and gear box backlash noise.</p>"
    "<p>Lower value = higher sensitivity. Set to zero if your vehicle data is already smoothed.</p>");

  c.input_slider("Battery power smoothing", "smooth_bat", 3, NULL,
    -1, smooth_bat.empty() ? 2.0 : atof(smooth_bat.c_str()), 2.0, 0.0, 10.0, 0.1,
    "<p>Battery power smoothing eliminates road bump and gear box backlash noise.</p>"
    "<p>Lower value = higher sensitivity. Set to zero if your vehicle data is already smoothed.</p>");

  c.print("<hr>");

  c.input_radiobtn_start("Regen brake signal", "enable");
  c.input_radiobtn_option("enable", "OFF", "no", !enable);
  c.input_radiobtn_option("enable", "ON", "yes", enable);
  c.input_radiobtn_end();

  c.input_select_start("… control port", "port");
  c.input_select_option("SW_12V (DA26 pin 18)", "1", port == "1");
  c.input_select_option("EGPIO_2", "3", port == "3");
  c.input_select_option("EGPIO_3", "4", port == "4");
  c.input_select_option("EGPIO_4", "5", port == "5");
  c.input_select_option("EGPIO_5", "6", port == "6");
  c.input_select_option("EGPIO_6", "7", port == "7");
  c.input_select_option("EGPIO_7", "8", port == "8");
  c.input_select_option("EGPIO_8", "9", port == "9");
  c.input_select_end();

  c.input_slider("… activation level", "level_on", 3, "m/s²",
    -1, level_on.empty() ? 1.3 : atof(level_on.c_str()), 1.3, 0.0, 3.0, 0.1,
    "<p>Deceleration threshold to activate regen brake light.</p>"
    "<p>Under UN regulation 13H, brake light illumination is required for decelerations &gt;1.3 m/s².</p>");

  c.input_slider("… deactivation level", "level_off", 3, "m/s²",
    -1, level_off.empty() ? 0.7 : atof(level_off.c_str()), 0.7, 0.0, 3.0, 0.1,
    "<p>Deceleration threshold to deactivate regen brake light.</p>"
    "<p>Under UN regulation 13H, brake lights must not be illuminated for decelerations &le;0.7 m/s².</p>");

  c.input_slider("… base power range", "basepwr", 3, "kW",
    -1, basepwr.empty() ? 0 : atof(basepwr.c_str()), 0, 0.0, 5.0, 0.1,
    "<p>Battery power range around zero (+/-) ignored for the regen signal. Raise to desensitize the signal"
    " for minor power levels (if the vehicle supports the battery power metric).</p>");

  c.input_checkbox("<strong>Ignore foot brake</strong>", "ignftbrk", ignftbrk,
    "<p>Create regen signal independent of the foot brake status.</p>");

  c.print("<hr>");

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>The OVMS generated regenerative braking signal needs a hardware modification to drive your"
    " vehicle brake lights. See your OVMS vehicle manual section for details on how to do this.</p>"
    "<p>The regen brake signal is activated when the deceleration level exceeds the configured threshold,"
    " the speed is above 1 m/s (3.6 kph / 2.2 mph) and the battery power is negative (charging).</p>"
    "<p>The signal is deactivated when deceleration drops below the deactivation level, speed drops"
    " below 1 m/s or battery power indicates discharging."
    " The signal will be held activated for at least 500 ms.</p>");

  c.done();
}
