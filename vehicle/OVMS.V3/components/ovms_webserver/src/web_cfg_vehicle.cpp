/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
;
;    Changes:
;    1.0  Initial release - Climate schedule configuration
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
#include <string.h>
#include <stdio.h>
#include <sstream> 
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * HandleCfgVehicle: configure vehicle type & identity (URL: /cfg/vehicle)
 */
void OvmsWebServer::HandleCfgVehicle(PageEntry_t& p, PageContext_t& c)
{
  OvmsVehicle *vehicle = MyVehicleFactory.ActiveVehicle();
  bool vehicle_changed = false;
  bool read_config = false;

  std::string error, info;
  std::string vehicleid, vehicletype, vehiclename, timezone, timezone_region, pin;
  std::string bat12v_factor, bat12v_ref, bat12v_alert, bat12v_shutdown, bat12v_shutdown_delay, bat12v_wakeup, bat12v_wakeup_interval;

  std::map<metric_group_t,std::string> units_values;
  metric_group_list_t unit_groups;
  OvmsMetricGroupConfigList(unit_groups);

  // TPMS variables
  std::vector<std::string> wheels;
  std::vector<std::string> wheelnames;
  std::vector<int> tpms_map;
  if (vehicle) {
    wheels = vehicle->GetTpmsLayout();
    wheelnames = vehicle->GetTpmsLayoutNames();
  }

  if (c.method == "POST") {
    // process form submission:
    vehicleid = c.getvar("vehicleid");
    vehicletype = c.getvar("vehicletype");
    vehiclename = c.getvar("vehiclename");
    timezone = c.getvar("timezone");
    timezone_region = c.getvar("timezone_region");
    
    for ( auto grpiter = unit_groups.begin(); grpiter != unit_groups.end(); ++grpiter) {
      std::string name = OvmsMetricGroupName(*grpiter);
      std::string cfg = "units_";
      cfg += name;
      units_values[*grpiter] = c.getvar(cfg);
    }

    bat12v_factor = c.getvar("bat12v_factor");
    bat12v_ref = c.getvar("bat12v_ref");
    bat12v_alert = c.getvar("bat12v_alert");
    bat12v_shutdown = c.getvar("bat12v_shutdown");
    bat12v_shutdown_delay = c.getvar("bat12v_shutdown_delay");
    bat12v_wakeup = c.getvar("bat12v_wakeup");
    bat12v_wakeup_interval = c.getvar("bat12v_wakeup_interval");
    pin = c.getvar("pin");

    if (vehicleid.length() == 0)
      error += "<li data-input=\"vehicleid\">Vehicle ID must not be empty</li>";
    if (vehicleid.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
      error += "<li data-input=\"vehicleid\">Vehicle ID may only contain ASCII letters, digits and '-'</li>";

    if (error == "" && StdMetrics.ms_v_type->AsString() != vehicletype) {
      MyVehicleFactory.SetVehicle(vehicletype.c_str());
      vehicle = MyVehicleFactory.ActiveVehicle();
      if (!vehicle) {
        error += "<li data-input=\"vehicletype\">Cannot set vehicle type <code>" + vehicletype + "</code></li>";
      }
      else {
        info += "<li>New vehicle type <code>" + vehicletype + "</code> has been set.</li>";
        vehicle_changed = true;
      }
    }

    if (error == "") {
      // success:
      MyConfig.SetParamValue("vehicle", "id", vehicleid);
      MyConfig.SetParamValue("auto", "vehicle.type", vehicletype);
      MyConfig.SetParamValue("vehicle", "name", vehiclename);
      MyConfig.SetParamValue("vehicle", "timezone", timezone);
      MyConfig.SetParamValue("vehicle", "timezone_region", timezone_region);
    
      #ifdef CONFIG_OVMS_COMP_TPMS
      if (!vehicle_changed && vehicle && vehicle->UsesTpmsSensorMapping()) {
        for (int i = 0; i < wheels.size(); i++) {
          MyConfig.SetParamValue("vehicle", std::string("tpms.")+str_tolower(wheels[i]), c.getvar(std::string("tpms_")+wheels[i]));
        }
      }
      #endif
      
      for ( auto grpiter = unit_groups.begin(); grpiter != unit_groups.end(); ++grpiter) {
        std::string name = OvmsMetricGroupName(*grpiter);
        std::string value = units_values[*grpiter];
        OvmsMetricSetUserConfig(*grpiter, value);
      }

      MyConfig.SetParamValue("system.adc", "factor12v", bat12v_factor);
      MyConfig.SetParamValue("vehicle", "12v.ref", bat12v_ref);
      MyConfig.SetParamValue("vehicle", "12v.alert", bat12v_alert);
      MyConfig.SetParamValue("vehicle", "12v.shutdown", bat12v_shutdown);
      MyConfig.SetParamValue("vehicle", "12v.shutdown_delay", bat12v_shutdown_delay);
      MyConfig.SetParamValue("vehicle", "12v.wakeup", bat12v_wakeup);
      MyConfig.SetParamValue("vehicle", "12v.wakeup_interval", bat12v_wakeup_interval);
      if (!pin.empty())
        MyConfig.SetParamValue("password", "pin", pin);

      info = "<p class=\"lead\">Success!</p><ul class=\"infolist\">" + info + "</ul>";
      info += "<script>$(\"title\").data(\"moduleid\", \"" + c.encode_html(vehicleid)
        + "\");$(\"#menu\").load(\"/menu\")</script>";
      c.head(200);
      c.alert("success", info.c_str());
      // stay on page if vehicle was changed, for additional elements like TPMS mapping
      if (vehicle_changed) {
        read_config = true;
      }
      else {
        OutputHome(p, c);
        c.done();
        return;
      }
    }
    else {
      // output error, return to form:
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.head(400);
      c.alert("danger", error.c_str());
    }
  }
  else {
    read_config = true;
    c.head(200);
  }

  if (read_config) {
    // read configuration:
    vehicleid = MyConfig.GetParamValue("vehicle", "id");
    vehicletype = MyConfig.GetParamValue("auto", "vehicle.type");
    vehiclename = MyConfig.GetParamValue("vehicle", "name");
    timezone = MyConfig.GetParamValue("vehicle", "timezone");
    timezone_region = MyConfig.GetParamValue("vehicle", "timezone_region");
    
    // read TPMS mapping
    if (vehicle && vehicle->UsesTpmsSensorMapping()) {
      if (vehicle_changed) {
        wheels = vehicle->GetTpmsLayout();
        wheelnames = vehicle->GetTpmsLayoutNames();
      }
      tpms_map.resize(wheels.size());
      for (int i = 0; i < wheels.size(); i++) {
        tpms_map[i] = MyConfig.GetParamValueInt("vehicle", std::string("tpms.")+str_tolower(wheels[i]), i);
      }
    }
    
    for ( auto grpiter = unit_groups.begin(); grpiter != unit_groups.end(); ++grpiter)
      units_values[*grpiter] = OvmsMetricGetUserConfig(*grpiter);
    bat12v_factor = MyConfig.GetParamValue("system.adc", "factor12v");
    bat12v_ref = MyConfig.GetParamValue("vehicle", "12v.ref");
    bat12v_alert = MyConfig.GetParamValue("vehicle", "12v.alert");
    bat12v_shutdown = MyConfig.GetParamValue("vehicle", "12v.shutdown");
    bat12v_shutdown_delay = MyConfig.GetParamValue("vehicle", "12v.shutdown_delay");
    bat12v_wakeup = MyConfig.GetParamValue("vehicle", "12v.wakeup");
    bat12v_wakeup_interval = MyConfig.GetParamValue("vehicle", "12v.wakeup_interval");
  }

  // generate form:
  c.panel_start("primary", "Vehicle configuration");
  c.form_start(p.uri);

  c.print(
    "<ul class=\"nav nav-tabs\">"
      "<li class=\"active\"><a data-toggle=\"tab\" href=\"#tab-vehicle\">Vehicle</a></li>"
      "<li><a data-toggle=\"tab\" href=\"#tab-bat12v\">12V Monitor</a></li>");
    // TPMS Tab: only if sensor mapping is used by the vehicle
    if (vehicle && vehicle->UsesTpmsSensorMapping()) {
    c.print(
      "<li><a data-toggle=\"tab\" href=\"#tab-tpms\">TPMS</a></li>");
  }
  c.print(
    "</ul>"
    "<div class=\"tab-content\">"
      "<div id=\"tab-vehicle\" class=\"tab-pane fade in active section-vehicle\">");

  c.input_select_start("Vehicle type", "vehicletype");
  c.input_select_option("&mdash;", "", vehicletype.empty());
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    c.input_select_option(k->second.name, k->first, (vehicletype == k->first));
  c.input_select_end();
  c.input_text("Vehicle ID", "vehicleid", vehicleid.c_str(), "Use ASCII letters, digits and '-'",
    "<p>Note: this is also the <strong>vehicle account ID</strong> for server connections.</p>");
  c.input_text("Vehicle name", "vehiclename", vehiclename.c_str(), "optional, the name of your car");

  c.printf(
    "<div class=\"form-group\">"
      "<label class=\"control-label col-sm-3\" for=\"input-timezone_select\">Time zone:</label>"
      "<div class=\"col-sm-9\">"
        "<input type=\"hidden\" name=\"timezone_region\" id=\"input-timezone_region\" value=\"%s\">"
        "<select class=\"form-control\" id=\"input-timezone_select\">"
          "<option>-Custom-</option>"
        "</select>"
      "</div>"
    "</div>"
    "<div class=\"form-group\">"
      "<div class=\"col-sm-9 col-sm-offset-3\">"
        "<input type=\"text\" class=\"form-control font-monospace\" placeholder=\"optional, default UTC\" name=\"timezone\" id=\"input-timezone\" value=\"%s\">"
        "<span class=\"help-block\"><p>Web links: <a target=\"_blank\" href=\"https://remotemonitoringsystems.ca/time-zone-abbreviations.php\">Example Timezone Strings</a>, <a target=\"_blank\" href=\"https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html\">Glibc manual</a></p></span>"
      "</div>"
    "</div>"
    , _attr(timezone_region)
    , _attr(timezone));

  for ( auto grpiter = unit_groups.begin(); grpiter != unit_groups.end(); ++grpiter) {
    std::string name = OvmsMetricGroupName(*grpiter);
    metric_unit_set_t group_units;
    if (OvmsMetricGroupUnits(*grpiter,group_units)) {
      bool use_select = group_units.size() > 3;
      std::string cfg = "units_";
      cfg += name;
      std::string value = units_values[*grpiter];
      if (use_select)
        c.input_select_start(OvmsMetricGroupLabel(*grpiter), cfg.c_str() );
      else
        c.input_radiobtn_start(OvmsMetricGroupLabel(*grpiter), cfg.c_str() );

      bool checked = value.empty();
      if (use_select)
        c.input_select_option( "Default", "", checked);
      else
        c.input_radiobtn_option(cfg.c_str(), "Default", "", checked);
      for (auto unititer = group_units.begin(); unititer != group_units.end(); ++unititer) {
        const char* unit_name = OvmsMetricUnitName(*unititer);
        const char* unit_label = OvmsMetricUnitLabel(*unititer);
        checked = value == unit_name;
        if (use_select)
          c.input_select_option( unit_label, unit_name, checked);
        else
          c.input_radiobtn_option(cfg.c_str(), unit_label, unit_name, checked);
      }
      if (use_select)
        c.input_select_end();
      else
        c.input_radiobtn_end();
    }
  }

  c.input_password("PIN", "pin", "", "empty = no change",
    "<p>Vehicle PIN code used for unlocking etc.</p>", "autocomplete=\"section-vehiclepin new-password\"");

  c.print(
      "</div>"
      "<div id=\"tab-bat12v\" class=\"tab-pane fade section-bat12v\">");

  c.input_info("12V reading",
      "<div class=\"receiver clearfix\">"
        "<div class=\"metric number\" id=\"display-bat12v_voltage\">"
          "<span class=\"value\">?</span>"
          "<span class=\"unit\">V</span>"
        "</div>"
      "</div>");
  c.input_slider("12V calibration", "bat12v_factor", 6, NULL,
    -1, bat12v_factor.empty() ? 195.7 : atof(bat12v_factor.c_str()), 195.7, 175.0, 225.0, 0.1,
    "<p>Adjust the calibration so the voltage displayed above matches your real voltage.</p>");

  c.fieldset_start("Alert");
  c.input("number", "12V reference", "bat12v_ref", bat12v_ref.c_str(), "Default: 12.6",
    "<p>The nominal resting voltage level of your 12V battery when fully charged.</p>",
    "min=\"10\" max=\"15\" step=\"0.1\"", "V");
  c.input("number", "12V alert threshold", "bat12v_alert", bat12v_alert.c_str(), "Default: 1.6",
    "<p>If the actual voltage drops this far below the maximum of configured and measured reference"
    " level, an alert is sent.</p>",
    "min=\"0\" max=\"3\" step=\"0.1\"", "V");
  c.fieldset_end();

  c.fieldset_start("Shutdown");
  c.input("number", "12V shutdown", "bat12v_shutdown", bat12v_shutdown.c_str(), "Default: disabled",
    "<p>If the voltage drops to/below this level, the module will enter deep sleep and wait for the voltage to recover to the wakeup level.</p>"
    "<p>Recommended shutdown level for standard lead acid batteries: not less than 10.5 V</p>",
    "min=\"10\" max=\"15\" step=\"0.1\"", "V");
  c.input("number", "Shutdown delay", "bat12v_shutdown_delay", bat12v_shutdown_delay.c_str(), "Default: 2",
    "<p>The 12V shutdown condition needs to be present for at least this long to actually cause a shutdown "
    "(0 = shutdown on first detection, check is done once per minute).</p>",
    "min=\"0\" max=\"60\" step=\"1\"", "Minutes");
  c.input("number", "12V wakeup", "bat12v_wakeup", bat12v_wakeup.c_str(), "Default: any",
    "<p>The minimum voltage level to allow restarting the module after a 12V shutdown.</p>"
    "<p>Recommended minimum level for standard lead acid batteries: not less than 11.0 V",
    "min=\"10\" max=\"15\" step=\"0.1\"", "V");
  c.input("number", "Wakeup test interval", "bat12v_wakeup_interval", bat12v_wakeup_interval.c_str(), "Default: 60",
    "<p>Voltage test interval after shutdown in seconds. Lowering this means faster detection of voltage recovery "
    "at the cost of higher energy usage (each test needs ~3 seconds of CPU uptime).</p>",
    "min=\"1\" max=\"300\" step=\"1\"", "Seconds");
  c.fieldset_end();

  c.print(
      "</div>");

  // TPMS Tab: only if sensor mapping is used by the vehicle
  if (vehicle && vehicle->UsesTpmsSensorMapping()) {
    c.print(
      "<div id=\"tab-tpms\" class=\"tab-pane fade section-tpms\">");

    c.fieldset_start("TPMS Sensor Mapping");
    c.printf(
      "<div class=\"form-control-static\">"
        "<p>The %s does not have a fixed TPMS sensor to wheel assignment, but"
        " instead assigns sensors individually per vehicle. The OVMS doesn't yet"
        " know how to decode the mapping, so you need to do this manually.</p>"
        "<p>Use this editor to select the actual wheel per nominal sensor position."
        " To determine the actual wheels, try lowering/raising the pressure of <u>one"
        " wheel at a time</u>, and watch which OVMS wheel pressure gets changed.</p>"
        "<p>Note: you may need to take a short trip to get a TPMS update. <b>Take care"
        " not to drive on too low/high pressure.</b></p>"
      "</div>",
      MyVehicleFactory.ActiveVehicleName());

    std::string label, code;
    for (int i = 0; i < wheels.size(); i++) {
      label = wheelnames[i] + " Sensor";
      code = std::string("tpms_") + wheels[i];
      c.input_select_start(label.c_str(), code.c_str());
      for (int j = 0; j < wheels.size(); j++) {
        label = wheelnames[j] + " Wheel";
        code = string_format("%d", j);
        c.input_select_option(label.c_str(), code.c_str(), tpms_map[i] == j);
      }
      c.input_select_end();
    }

    c.fieldset_end();

    c.print(
      "</div>");
  }

  c.print(
    "</div>"
    "<br>");

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  c.print(
    "<script>"
    "(function(){"
      "$.getJSON(\"" URL_ASSETS_ZONES_JSON "\", function(data) {"
        "var items = [];"
        "var region = $('#input-timezone_region').val();"
        "$.each(data, function(key, val) {"
          "items.push('<option data-tz=\"' + val + '\"' + (key==region ? ' selected' : '') + '>' + key + '</option>');"
        "});"
        "$('#input-timezone_select').append(items.join(''));"
        "$('#input-timezone_select').on('change', function(ev){"
          "var opt = $(this).find('option:selected');"
          "$('#input-timezone_region').val(opt.val());"
          "var tz = opt.data(\"tz\");"
          "if (tz) {"
            "$('#input-timezone').val(tz);"
            "$('#input-timezone').prop('readonly', true);"
          "} else {"
            "$('#input-timezone').prop('readonly', false);"
          "}"
        "}).trigger('change');"
      "});"
      "var $bat12v_factor = $('#input-bat12v_factor'),"
        "$bat12v_display = $('#display-bat12v_voltage .value'),"
        "oldfactor = $bat12v_factor.val() || 195.7;"
      "var updatecalib = function(){"
        "var newfactor = $bat12v_factor.val() || 195.7;"
        "var voltage = metrics['v.b.12v.voltage'] * oldfactor / newfactor;"
        "$bat12v_display.text(Number(voltage).toFixed(2));"
      "};"
      "$bat12v_factor.on(\"input change\", updatecalib);"
      "$(\".receiver\").on(\"msg:metrics\", updatecalib).trigger(\"msg:metrics\");"
    "})()"
    "</script>");

  c.done();
}
