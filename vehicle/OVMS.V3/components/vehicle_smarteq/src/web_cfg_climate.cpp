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

#include "vehicle_smarteq.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebCfgClimateSchedule: configure climate schedule (URL /xsq/climateschedule)
 */
void OvmsVehicleSmartEQ::WebCfgClimateSchedule(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  const char* day_names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  const char* day_full[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  bool precondition = false;

  if (c.method == "POST") {
    // Read enable/disable checkbox
    precondition = (c.getvar("enabled") == "yes");
    MyConfig.SetParamValueBool("vehicle", "climate.precondition", precondition);

    // Process form submission
    for (int i = 0; i < 7; i++) {
      std::string enabled_key = std::string("enabled_") + day_names[i];
      std::string time_key = std::string("time_") + day_names[i];
      
      bool enabled = (c.getvar(enabled_key) == "yes");
      std::string times = c.getvar(time_key);
      
      std::string config_key = std::string("climate.schedule.") + day_names[i];
      
      if (!enabled || times.empty()) {
        // Clear schedule for this day
        MyConfig.DeleteInstance("vehicle", config_key);
        continue;
      }
      
      // Validate time format: HH:MM[/duration][,HH:MM[/duration],...]
      bool valid = true;
      size_t start = 0;
      size_t end = times.find(',');
      
      while (start != std::string::npos && valid) {
        std::string entry = (end == std::string::npos) ? 
                            times.substr(start) : 
                            times.substr(start, end - start);
        
        // Split by '/' for optional duration
        size_t slash_pos = entry.find('/');
        std::string time_part = (slash_pos != std::string::npos) ?
                                entry.substr(0, slash_pos) : entry;
        
        // Validate HH:MM format
        size_t colon_pos = time_part.find(':');
        if (colon_pos == std::string::npos || colon_pos == 0) {
          valid = false;
          error += "<li>" + std::string(day_full[i]) + ": invalid time format (use HH:MM)</li>";
          break;
        }
        
        int hour = atoi(time_part.substr(0, colon_pos).c_str());
        int min = atoi(time_part.substr(colon_pos + 1).c_str());
        
        if (hour < 0 || hour > 23 || min < 0 || min > 59) {
          valid = false;
          error += "<li>" + std::string(day_full[i]) + ": time out of range (00:00-23:59)</li>";
          break;
        }
        
        // Validate optional duration if present
        if (slash_pos != std::string::npos) {
          int duration = atoi(entry.substr(slash_pos + 1).c_str());
          if (duration != 5 && duration != 10 && duration != 15) {
            valid = false;
            error += "<li>" + std::string(day_full[i]) + ": invalid duration (use 5, 10, or 15)</li>";
            break;
          }
        }
        
        // Move to next time
        if (end == std::string::npos)
          break;
        start = end + 1;
        end = times.find(',', start);
      }
      
      if (valid) {
        MyConfig.SetParamValue("vehicle", config_key, times);
      }
    }
    
    if (error.empty()) {
      info = "<p class=\"lead\">Success!</p><p>Climate schedule configuration saved.</p>";
      c.head(200);
      c.alert("success", info.c_str());
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }
    
    // Output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // Read current enable state
    precondition = MyConfig.GetParamValueBool("vehicle", "climate.precondition", false);
    c.head(200);
  }

  // Generate form:
  c.panel_start("primary", "Scheduled precondition Configuration");
  c.form_start(p.uri);

  // Add global enable/disable switch
  c.fieldset_start("Global Settings");
  c.input_checkbox("Enable scheduled precondition", "precondition", precondition,
    "<p>Master switch: activate automatic precondition based on configured schedules.</p>");
  c.fieldset_end();
  
  c.print(
    "<p class=\"lead\">Configure automatic precondition activation times for each day of the week.</p>"
    "<p>For multiple times per day, separate them with commas (e.g., <code>07:30,17:45</code>).</p>"
    "<p>Optionally specify duration in minutes using <code>/duration</code> (e.g., <code>07:30/10,17:45/15</code>).</p>"
  );

  c.fieldset_start("Weekly Schedule");

  // Create schedule inputs for each day
  for (int i = 0; i < 7; i++) {
    std::string config_key = std::string("climate.schedule.") + day_names[i];
    std::string schedule = MyConfig.GetParamValue("vehicle", config_key);
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
             "value=\"%s\" placeholder=\"HH:MM[/duration] or HH:MM,HH:MM\"%s "
             "pattern=\"[0-2][0-9]:[0-5][0-9](/([5]|10|15))?(,[0-2][0-9]:[0-5][0-9](/([5]|10|15))?)*\">",
             day_names[i], _attr(schedule), enabled ? "" : " disabled");
    c.print("</div>");
    c.printf("<span class=\"help-block\">Example: 07:30/10 or 07:00,17:30/15</span>");
    c.print("</div>");
    c.print("</div>");
  }

  c.fieldset_end();

  // Show current time and next scheduled event
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  if (timeinfo != NULL) {
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
    int next_duration = 5; // Default duration

    // Search all days for the next scheduled event
    for (int day_offset = 0; day_offset < 7; day_offset++) {
      int check_day = (current_day + day_offset) % 7;
      int check_day_index = (check_day == 0) ? 6 : check_day - 1;
      
      std::string config_key = std::string("climate.schedule.") + day_names[check_day_index];
      std::string schedule = MyConfig.GetParamValue("vehicle", config_key);
      
      if (schedule.empty())
        continue;

      // Parse times for this day
      size_t start = 0;
      size_t end = schedule.find(',');
      
      while (start != std::string::npos) {
        std::string entry = (end == std::string::npos) ?
                            schedule.substr(start) :
                            schedule.substr(start, end - start);
        
        // Split by '/' for time/duration
        size_t slash_pos = entry.find('/');
        std::string time_part = (slash_pos != std::string::npos) ?
                                entry.substr(0, slash_pos) : entry;
        int duration = 5; // default
        if (slash_pos != std::string::npos) {
          duration = atoi(entry.substr(slash_pos + 1).c_str());
        }
        
        size_t colon_pos = time_part.find(':');
        if (colon_pos != std::string::npos && colon_pos > 0) {
          int hour = atoi(time_part.substr(0, colon_pos).c_str());
          int min = atoi(time_part.substr(colon_pos + 1).c_str());
          
          int total_min = check_day * 24 * 60 + hour * 60 + min;
          
          // Adjust for week wrap
          if (day_offset > 0)
            total_min += day_offset * 24 * 60;
          
          if (total_min > current_total_min && total_min < next_total_min) {
            next_total_min = total_min;
            next_day = check_day;
            next_hour = hour;
            next_min = min;
            next_duration = duration;
          }
        }
        
        if (end == std::string::npos)
          break;
        start = end + 1;
        end = schedule.find(',', start);
      }
    }

    if (next_day >= 0) {
      int next_day_index = (next_day == 0) ? 6 : next_day - 1;
      c.printf("<br><strong>Next scheduled:</strong> %s %02d:%02d (%d minutes)",
               day_full[next_day_index],
               next_hour,
               next_min,
               next_duration);
    } else {
      c.print("<br><strong>Next scheduled:</strong> No schedule configured");
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

  PAGE_HOOK("body.post");
  c.done();
}
#endif //CONFIG_OVMS_COMP_WEBSERVER
