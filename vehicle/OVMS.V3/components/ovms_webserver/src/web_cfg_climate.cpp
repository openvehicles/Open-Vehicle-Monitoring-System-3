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

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * HandleCfgPreconditionSchedule: configure climate precondition schedule
 * 
 * Note: this is not enabled by default, as some vehicles do not provide climate control.
 * To enable, include something like this in your vehicle init:
 *   MyWebServer.RegisterPage("/xxx/climateprecondition", "Climate Preconditioning",
 *      OvmsWebServer::HandleCfgPreconditionSchedule, PageMenu_Vehicle, PageAuth_Cookie);
 * (with "xxx" = your vehicle namespace)
 * You can change the URL path, title, menu association and authentication as you like.
 * For a clean shutdown, add
 *   MyWebServer.DeregisterPage("/xxx/climateprecondition");
 * …in your vehicle cleanup.
 */
void OvmsWebServer::HandleCfgPreconditionSchedule(PageEntry_t& p, PageContext_t& c)
{
  std::string error, info;
  const char* day_names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  const char* day_full[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  bool precondition = false;

  if (c.method == "POST") {
    // Check if this is a copy operation
    std::string action = c.getvar("action");
    
    if (action == "clearall") 
      {
      // Delete all schedules immediately
      for (int i = 0; i < 7; i++) 
        {
        std::string config_key = std::string("climate.schedule.") + day_names[i];
        MyConfig.DeleteInstance("vehicle", config_key);
        }
      info = "<p class=\"lead\">Success!</p><p>All climate schedules have been cleared.</p>";
      } 
    else if (action == "copy") {
      // Handle copy operation
      std::string source_day = c.getvar("copy_source");
      std::string target_days_str = c.getvar("copy_targets");
      
      if (!source_day.empty() && !target_days_str.empty()) {
        // Get source schedule
        std::string source_config_key = std::string("climate.schedule.") + source_day;
        std::string source_schedule = MyConfig.GetParamValue("vehicle", source_config_key);
        
        if (source_schedule.empty()) {
          error = "<li>No schedule configured for " + std::string(day_full[std::find(day_names, day_names + 7, source_day) - day_names]) + "</li>";
        } else {
          // Parse target days (comma-separated)
          int copied_count = 0;
          std::string copied_days;
          size_t start = 0;
          size_t comma_pos = target_days_str.find(',');
          
          while (start != std::string::npos) {
            std::string target_day = (comma_pos == std::string::npos) ?
                                     target_days_str.substr(start) :
                                     target_days_str.substr(start, comma_pos - start);
            
            // Trim whitespace
            size_t first = target_day.find_first_not_of(" \t");
            size_t last = target_day.find_last_not_of(" \t");
            if (first != std::string::npos && last != std::string::npos)
              target_day = target_day.substr(first, last - first + 1);
            
            // Skip source day
            if (target_day != source_day && !target_day.empty()) {
              std::string target_config_key = std::string("climate.schedule.") + target_day;
              MyConfig.SetParamValue("vehicle", target_config_key, source_schedule);
              
              // Find day index for display name
              auto it = std::find(day_names, day_names + 7, target_day);
              if (it != day_names + 7) {
                if (copied_count > 0) copied_days += ", ";
                copied_days += day_full[it - day_names];
                copied_count++;
              }
            }
            
            if (comma_pos == std::string::npos)
              break;
            start = comma_pos + 1;
            comma_pos = target_days_str.find(',', start);
          }
          
          if (copied_count > 0) {
            std::ostringstream oss;
            oss << "<p class=\"lead\">Success!</p><p>Schedule '" << source_schedule 
                << "' copied to " << copied_count << " day(s): " << copied_days << "</p>";
            info = oss.str();
          } else {
            error = "<li>No target days selected or source day was in target list</li>";
          }
        }
      } else {
        error = "<li>Please select both source day and target days for copy operation</li>";
      }
    } else {
      // Handle normal schedule save
      // Read enable/disable checkbox
      precondition = (c.getvar("precondition") == "yes");
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
            if (duration < 5 || duration > 30) {
              valid = false;
              error += "<li>" + std::string(day_full[i]) + ": invalid duration (use 5 to 30)</li>";
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
      }
    }
    
    if (!error.empty()) {
      error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
      c.head(400);
      c.alert("danger", error.c_str());
    } else if (!info.empty()) {
      c.head(200);
      c.alert("success", info.c_str());
      OutputHome(p, c);
      c.done();
      return;
    }
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
             "pattern=\"[0-2]?[0-9]:[0-5][0-9](/([5-9]|[12][0-9]|30))?(,[0-2]?[0-9]:[0-5][0-9](/([5-9]|[12][0-9]|30))?)*\">",
             day_names[i], _attr(schedule), enabled ? "" : " disabled");
    c.print("</div>");
    c.printf("<span class=\"help-block\">Example: 7:30/10 or 7:00,17:30/15</span>");
    c.print("</div>");
    c.print("</div>");
  } 

  // "Clear All" button at the top of the schedule section
  c.print(
    "<div class=\"form-group\">"
    "<div class=\"col-sm-9 col-sm-offset-3\">"
    "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"clearall\" "
    "onclick=\"return confirm('Are you sure you want to clear ALL schedules? This cannot be undone.');\">"
    "Clear All Schedules"
    "</button>"
    "<div class=\"clearfix\"></div>"
    "</div>"
    "</div>"
  );

  c.input_button("default", "Save", "action", "save");

  c.fieldset_end();

  // Add Copy section
  c.fieldset_start("Copy Schedule", "copy-section");
  c.print(
    "<p>Copy a configured schedule from one day to multiple other days.</p>"
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Copy from:</label>"
    "<div class=\"col-sm-9\">"
    "<select class=\"form-control\" name=\"copy_source\" id=\"copy_source\">"
    "<option value=\"\">-- Select source day --</option>"
  );
  
  for (int i = 0; i < 7; i++) {
    std::string config_key = std::string("climate.schedule.") + day_names[i];
    std::string schedule = MyConfig.GetParamValue("vehicle", config_key);
    if (!schedule.empty()) {
      c.printf("<option value=\"%s\">%s (%s)</option>", 
               day_names[i], day_full[i], _html(schedule));
    }
  }
  
  c.print(
    "</select>"
    "</div>"
    "</div>"
    "<div class=\"form-group\">"
    "<label class=\"control-label col-sm-3\">Copy to:</label>"
    "<div class=\"col-sm-9\">"
  );
  
  for (int i = 0; i < 7; i++) {
    c.printf(
      "<div class=\"checkbox\">"
      "<label>"
      "<input type=\"checkbox\" name=\"copy_target_%s\" value=\"%s\" class=\"copy-target\"> %s"
      "</label>"
      "</div>",
      day_names[i], day_names[i], day_full[i]
    );
  }
  
  c.print(
    "<input type=\"hidden\" name=\"copy_targets\" id=\"copy_targets\" value=\"\">"
    "</div>"
    "</div>"
    "<div class=\"form-group\">"
    "<div class=\"col-sm-9 col-sm-offset-3\">"
    "<button type=\"submit\" class=\"btn btn-default\" name=\"action\" value=\"copy\" id=\"btn_copy\">"
    "Copy Schedule"
    "</button>"
    "</div>"
    "</div>"
  );
  
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
    int next_total_min = current_total_min + 10000;
    int next_duration = 5;

    for (int day_offset = 0; day_offset < 7; day_offset++) {
      int check_day = (current_day + day_offset) % 7;
      int check_day_index = (check_day == 0) ? 6 : check_day - 1;
      
      std::string config_key = std::string("climate.schedule.") + day_names[check_day_index];
      std::string schedule = MyConfig.GetParamValue("vehicle", config_key);
      
      if (schedule.empty())
        continue;

      size_t start = 0;
      size_t end = schedule.find(',');
      
      while (start != std::string::npos) {
        std::string entry = (end == std::string::npos) ?
                            schedule.substr(start) :
                            schedule.substr(start, end - start);
        
        size_t slash_pos = entry.find('/');
        std::string time_part = (slash_pos != std::string::npos) ?
                                entry.substr(0, slash_pos) : entry;
        int duration = 5;
        if (slash_pos != std::string::npos) {
          duration = atoi(entry.substr(slash_pos + 1).c_str());
        }
        
        size_t colon_pos = time_part.find(':');
        if (colon_pos != std::string::npos && colon_pos > 0) {
          int hour = atoi(time_part.substr(0, colon_pos).c_str());
          int min = atoi(time_part.substr(colon_pos + 1).c_str());
          
          int total_min = check_day * 24 * 60 + hour * 60 + min;
          
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

  c.form_end();
  c.panel_end();

  // Add JavaScript for UX and copy functionality
  c.print(
    "<script>\n"
    "$(document).ready(function() {\n"
    "  // Initialize disabled state on page load\n"
    "  $('input[type=checkbox][name^=enabled_]').each(function() {\n"
    "    var day = this.name.replace('enabled_', '');\n"
    "    var timeInput = $('input[name=time_' + day + ']');\n"
    "    timeInput.prop('disabled', !this.checked);\n"
    "  });\n"
    "  \n"
    "  // Handle 'Clear All' button\n"
    "  $('#btn_clear_all').click(function() {\n"
    "    if (confirm('Are you sure you want to clear ALL schedules? This cannot be undone.')) {\n"
    "      $('input[type=checkbox][name^=enabled_]').prop('checked', false).trigger('change');\n"
    "      $('input[type=text][name^=time_]').val('').prop('disabled', true);\n"
    "      alert('All schedules cleared. Click \"Save\" to apply changes.');\n"
    "    }\n"
    "  });\n"
    "  \n"
    "  // Handle copy functionality\n"
    "  $('#btn_copy').click(function(e) {\n"
    "    var source = $('#copy_source').val();\n"
    "    if (!source) {\n"
    "      e.preventDefault();\n"
    "      alert('Please select a source day to copy from');\n"
    "      return false;\n"
    "    }\n"
    "    \n"
    "    // Collect selected target days\n"
    "    var targets = [];\n"
    "    $('.copy-target:checked').each(function() {\n"
    "      targets.push($(this).val());\n"
    "    });\n"
    "    \n"
    "    if (targets.length === 0) {\n"
    "      e.preventDefault();\n"
    "      alert('Please select at least one target day');\n"
    "      return false;\n"
    "    }\n"
    "    \n"
    "    $('#copy_targets').val(targets.join(','));\n"
    "  });\n"
    "  \n"
    "  // Disable copy button if no source selected\n"
    "  $('#copy_source').change(function() {\n"
    "    $('#btn_copy').prop('disabled', !$(this).val());\n"
    "  }).trigger('change');\n"
    "});\n"
    "</script>\n"
  );

  PAGE_HOOK("body.post");
  c.done();
}
