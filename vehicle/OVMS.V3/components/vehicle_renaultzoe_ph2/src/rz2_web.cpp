/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_renaultzoe_ph2.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

void OvmsVehicleRenaultZoePh2::WebCfgCommon(PageEntry_t &p, PageContext_t &c)
{
  std::string error, rangeideal, battcapacity, auto_12v_threshold;
  bool UseBMScalculation;
  bool vCanEnabled;
  bool auto_12v_recharge_enabled;
  bool coming_home_enabled;
  bool remote_climate_enabled;

  if (c.method == "POST")
  {
    rangeideal = c.getvar("rangeideal");
    battcapacity = c.getvar("battcapacity");
    auto_12v_threshold = c.getvar("auto_12v_threshold");
    UseBMScalculation = (c.getvar("UseBMScalculation") == "no");
    vCanEnabled = (c.getvar("vCanEnabled") == "yes");
    auto_12v_recharge_enabled = (c.getvar("auto_12v_recharge_enabled") == "yes");
    coming_home_enabled = (c.getvar("coming_home_enabled") == "yes");
    remote_climate_enabled = (c.getvar("remote_climate_enabled") == "yes");

    if (!rangeideal.empty())
    {
      int v = atoi(rangeideal.c_str());
      if (v < 90 || v > 500)
        error += "<li data-input=\"rangeideal\">Range Ideal must be of 80…500 km</li>";
    }
    if (!auto_12v_threshold.empty())
    {
      float v = atof(auto_12v_threshold.c_str());
      if (v < 11.0 || v > 14.0)
        error += "<li data-input=\"auto_12v_threshold\">12V threshold must be between 11.0…14.0 V</li>";
    }
    if (error == "")
    {
      // store:
      MyConfig.SetParamValue("xrz2", "rangeideal", rangeideal);
      MyConfig.SetParamValue("xrz2", "battcapacity", battcapacity);
      MyConfig.SetParamValue("xrz2", "auto_12v_threshold", auto_12v_threshold);
      MyConfig.SetParamValueBool("xrz2", "UseBMScalculation", UseBMScalculation);
      MyConfig.SetParamValueBool("xrz2", "vCanEnabled", vCanEnabled);
      MyConfig.SetParamValueBool("xrz2", "auto_12v_recharge_enabled", auto_12v_recharge_enabled);
      MyConfig.SetParamValueBool("xrz2", "coming_home_enabled", coming_home_enabled);
      MyConfig.SetParamValueBool("xrz2", "remote_climate_enabled", remote_climate_enabled);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Renault Zoe Ph2 setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else
  {
    // read configuration:
    rangeideal = MyConfig.GetParamValue("xrz2", "rangeideal", "350");
    battcapacity = MyConfig.GetParamValue("xrz2", "battcapacity", "52000");
    auto_12v_threshold = MyConfig.GetParamValue("xrz2", "auto_12v_threshold", "12.4");
    UseBMScalculation = MyConfig.GetParamValueBool("xrz2", "UseBMScalculation", false);
    vCanEnabled = MyConfig.GetParamValueBool("xrz2", "vCanEnabled", false);
    auto_12v_recharge_enabled = MyConfig.GetParamValueBool("xrz2", "auto_12v_recharge_enabled", false);
    coming_home_enabled = MyConfig.GetParamValueBool("xrz2", "coming_home_enabled", false);
    remote_climate_enabled = MyConfig.GetParamValueBool("xrz2", "remote_climate_enabled", false);
    c.head(200);
  }

  c.panel_start("primary", "Renault Zoe Ph2 Setup");
  c.form_start(p.uri);

  c.fieldset_start("Battery size and ideal range");

  c.input_radio_start("Battery size", "battcapacity");
  c.input_radio_option("battcapacity", "ZE40 (41kWh)", "41000", battcapacity == "41000");
  c.input_radio_option("battcapacity", "ZE50 (52kWh)", "52000", battcapacity == "52000");
  c.input_radio_end("");

  c.input_slider("Range Ideal", "rangeideal", 3, "km", -1, atoi(rangeideal.c_str()), 350, 80, 500, 1,
                 "<p>Default 350km. Ideal Range...</p>");
  c.fieldset_end();

  c.fieldset_start("Battery energy calculation");

  c.input_radio_start("Which energy calculation?", "UseBMScalculation");
  c.input_radio_option("UseBMScalculation", "OVMS energy calculation", "yes", UseBMScalculation == false);
  c.input_radio_option("UseBMScalculation", "BMS-based calculation", "no", UseBMScalculation == true);
  c.input_radio_end("");

  c.fieldset_end();

  c.fieldset_start("Additional CAN Bus settings");

  c.input_checkbox("CAN1 interface is connected to Zoes V-CAN?", "vCanEnabled", vCanEnabled,
    "<p>If you connect the CAN1 interface of the OVMS module directly to the V-CAN (BCM or CGW tapping required) you can trigger ECU wake-up, pre-heat function, "
    "locking/unlocking functions and change configuration with DDT commands. Additonal all energy related metrics are used from Zoes ECUs instead of calculation within OVMS. </p>");

  c.input_checkbox("Enable coming home function (V-CAN required)", "coming_home_enabled", coming_home_enabled,
    "<p>When enabled, the headlights will automatically turn on for ~30 seconds after locking the car, if the headlights were on before locking. "
    "This provides lighting when leaving your car at night.</p>");

  c.input_checkbox("Enable remote climate trigger via key fob (V-CAN required)", "remote_climate_enabled", remote_climate_enabled,
    "<p>When enabled, pressing the 'Remote Lighting' button on your key fob will trigger the climate control (pre-heat/pre-cool). "
    "This allows you to start climate control using your key fob instead of the app.</p>");

  c.fieldset_end();

  c.fieldset_start("12V Battery Auto-Recharge (V-CAN required)");

  c.input_checkbox("Enable automatic 12V battery recharge when locked", "auto_12v_recharge_enabled", auto_12v_recharge_enabled,
    "<p>When enabled, the DCDC converter will be activated automatically to recharge the 12V battery when the vehicle is locked and the voltage drops below the threshold. "
    "This feature requires V-CAN connection.</p>");

  c.input_slider("12V voltage threshold", "auto_12v_threshold", 3, "V", -1, atof(auto_12v_threshold.c_str()), 12.4, 11.0, 14.0, 0.1,
    "<p>Default 12.4V. The DCDC converter will be activated when the 12V battery voltage drops below this threshold.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

void OvmsVehicleRenaultZoePh2::WebCfgPreclimate(PageEntry_t &p, PageContext_t &c)
{
  std::string error;
  const char* day_names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  const char* day_full[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

  if (c.method == "POST")
  {
    for (int i = 0; i < 7; i++)
    {
      std::string enabled_var = std::string("enabled_") + day_names[i];
      std::string time_var = std::string("time_") + day_names[i];

      bool enabled = (c.getvar(enabled_var) == "yes");
      std::string times = c.getvar(time_var);

      if (enabled && !times.empty())
      {
        // Validate time format
        bool valid = true;
        size_t start = 0;
        size_t end = times.find(',');

        while (start != std::string::npos && valid)
        {
          std::string single_time = (end == std::string::npos) ?
                                     times.substr(start) :
                                     times.substr(start, end - start);

          // Trim whitespace
          size_t first = single_time.find_first_not_of(" \t");
          size_t last = single_time.find_last_not_of(" \t");
          if (first != std::string::npos)
            single_time = single_time.substr(first, (last - first + 1));

          size_t colon_pos = single_time.find(':');
          if (colon_pos != std::string::npos && colon_pos > 0)
          {
            int hour = atoi(single_time.substr(0, colon_pos).c_str());
            int min = atoi(single_time.substr(colon_pos + 1).c_str());

            if (hour < 0 || hour > 23 || min < 0 || min > 59)
            {
              error += "<li data-input=\"time_" + std::string(day_names[i]) +
                       "\">Invalid time for " + std::string(day_full[i]) +
                       ": hour must be 0-23, minute must be 0-59</li>";
              valid = false;
            }
          }
          else
          {
            error += "<li data-input=\"time_" + std::string(day_names[i]) +
                     "\">Invalid time format for " + std::string(day_full[i]) +
                     ": use HH:MM</li>";
            valid = false;
          }

          if (end == std::string::npos)
            break;
          start = end + 1;
          end = times.find(',', start);
        }

        if (valid)
        {
          MyConfig.SetParamValue("xrz2.preclimate", day_names[i], times);
        }
      }
      else
      {
        // Clear schedule for this day
        MyConfig.DeleteInstance("xrz2.preclimate", day_names[i]);
      }
    }

    if (error == "")
    {
      c.head(200);
      c.alert("success", "<p class=\"lead\">Pre-climate schedules saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else
  {
    c.head(200);
  }

  c.panel_start("primary", "Scheduled Pre-Climate Configuration");
  c.form_start(p.uri);

  c.print(
    "<p class=\"lead\">Configure automatic pre-climate activation times for each day of the week.</p>"
    "<p>For multiple times per day, separate them with commas (e.g., <code>07:30,17:45</code>).</p>"
    "<p><strong>Note:</strong> V-CAN connection required. Pre-climate will only activate if the vehicle is locked and battery SoC is above 15%.</p>"
  );

  c.fieldset_start("Weekly Schedule");

  // Create schedule inputs for each day
  for (int i = 0; i < 7; i++)
  {
    std::string schedule = MyConfig.GetParamValue("xrz2.preclimate", day_names[i]);
    bool enabled = !schedule.empty();

    c.print("<div class=\"form-group\">");
    c.printf("<label class=\"control-label col-sm-3\">%s:</label>", day_full[i]);
    c.print("<div class=\"col-sm-9\">");
    c.print("<div class=\"input-group\">");
    c.print("<span class=\"input-group-addon\">");
    c.printf("<input type=\"checkbox\" name=\"enabled_%s\" value=\"yes\"%s "
             "onchange=\"this.form.time_%s.disabled=!this.checked\">",
             day_names[i], enabled ? " checked" : "", day_names[i]);
    c.print("</span>");
    c.printf("<input type=\"text\" class=\"form-control\" name=\"time_%s\" "
             "value=\"%s\" placeholder=\"HH:MM or HH:MM,HH:MM\"%s "
             "pattern=\"[0-2][0-9]:[0-5][0-9](,[0-2][0-9]:[0-5][0-9])*\">",
             day_names[i], _attr(schedule), enabled ? "" : " disabled");
    c.print("</div>");
    c.printf("<span class=\"help-block\">Example: 07:30 or 07:00,17:30</span>");
    c.print("</div>");
    c.print("</div>");
  }

  c.fieldset_end();

  // Show current time and next scheduled event
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  if (timeinfo != NULL)
  {
    c.print("<div class=\"alert alert-info\">");
    c.printf("<strong>Current time:</strong> %s %02d:%02d",
             day_full[timeinfo->tm_wday == 0 ? 6 : timeinfo->tm_wday - 1],
             timeinfo->tm_hour,
             timeinfo->tm_min);

    // Find next scheduled event
    int current_day = timeinfo->tm_wday;
    int current_hour = timeinfo->tm_hour;
    int current_min = timeinfo->tm_min;
    int current_total_min = current_day * 24 * 60 + current_hour * 60 + current_min;

    int next_day = -1;
    int next_hour = -1;
    int next_min = -1;
    int next_total_min = current_total_min + 10000; // Start with large value

    // Search all days for the next scheduled event
    for (int day_offset = 0; day_offset < 7; day_offset++)
    {
      int check_day = (current_day + day_offset) % 7;
      int day_index = (check_day == 0) ? 6 : check_day - 1; // Convert Sunday=0 to array index

      std::string schedule = MyConfig.GetParamValue("xrz2.preclimate", day_names[day_index]);
      if (!schedule.empty())
      {
        // Parse times in this day's schedule
        size_t start = 0;
        size_t end = schedule.find(',');

        while (start != std::string::npos)
        {
          std::string time_str = (end == std::string::npos) ?
                                 schedule.substr(start) :
                                 schedule.substr(start, end - start);

          size_t colon_pos = time_str.find(':');
          if (colon_pos != std::string::npos)
          {
            int hour = atoi(time_str.substr(0, colon_pos).c_str());
            int min = atoi(time_str.substr(colon_pos + 1).c_str());

            // Validate time range before using
            if (hour >= 0 && hour < 24 && min >= 0 && min < 60)
            {
              int event_total_min = check_day * 24 * 60 + hour * 60 + min;

              // If this is in the future and earlier than our current next event
              if (event_total_min > current_total_min && event_total_min < next_total_min)
              {
                next_total_min = event_total_min;
                next_day = check_day;
                next_hour = hour;
                next_min = min;
              }
            }
          }

          if (end == std::string::npos)
            break;
          start = end + 1;
          end = schedule.find(',', start);
        }
      }
    }

    if (next_day >= 0)
    {
      int day_index = (next_day == 0) ? 6 : next_day - 1;
      c.printf("<br><strong>Next scheduled:</strong> %s at %02d:%02d",
               day_full[day_index], next_hour, next_min);
    }

    c.print("</div>");
  }

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();

  // Add JavaScript for better UX
  c.print(
    "<script>\n"
    "$(document).ready(function() {\n"
    "  // Initialize disabled state on page load\n"
    "  $('input[type=checkbox][name^=enabled_]').each(function() {\n"
    "    var day = this.name.replace('enabled_', '');\n"
    "    var timeInput = $('input[name=time_' + day + ']');\n"
    "    timeInput.prop('disabled', !this.checked);\n"
    "  });\n"
    "});\n"
    "</script>\n"
  );

  c.done();
}

/**
 * WebInit: register pages
 */
void OvmsVehicleRenaultZoePh2::WebInit()
{
  MyWebServer.RegisterPage("/xrz2/battmon", "BMS View", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz2/settings", "Setup", WebCfgCommon, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xrz2/preclimate", "Pre-Climate Schedule", WebCfgPreclimate, PageMenu_Vehicle, PageAuth_Cookie);
}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleRenaultZoePh2::WebDeInit()
{
  MyWebServer.DeregisterPage("/xrz2/battmon");
  MyWebServer.DeregisterPage("/xrz2/settings");
  MyWebServer.DeregisterPage("/xrz2/preclimate");
}

#endif // CONFIG_OVMS_COMP_WEBSERVER
