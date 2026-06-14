/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
static const char *TAG = "vehicle";

#include <stdio.h>
#include <algorithm>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <string_writer.h>
#include "vehicle.h"
#include "vehicle_common.h"

int OvmsVehicleFactory::vehicle_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  if (argc == 1)
    return MyVehicleFactory.m_vmap.Validate(writer, argc, argv[0], complete);
  return -1;
  }

void OvmsVehicleFactory::vehicle_module(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyVehicleFactory.ClearVehicle();
    }
  else
    {
    MyVehicleFactory.SetVehicle(argv[0]);
    }
  // output new status:
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->Status(verbosity, writer);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void OvmsVehicleFactory::vehicle_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type            Name");
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    {
    writer->printf("%-15.15s %s\n",k->first,k->second.name);
    }
  }

void OvmsVehicleFactory::vehicle_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->Status(verbosity, writer);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void OvmsVehicleFactory::vehicle_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandWakeup())
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle has been woken");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be woken");
      break;
    default:
      writer->puts("Error: Vehicle wake functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_homelink(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int homelink = atoi(argv[0]);
  if ((homelink<1)||(homelink>3))
    {
    writer->puts("Error: Homelink button should be in range 1..3");
    return;
    }

  int durationms = 1000;
  if (argc>1) durationms= atoi(argv[1]);
  if (durationms < 100)
    {
    writer->puts("Error: Minimum homelink timer duration 100ms");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandHomelink(homelink-1, durationms))
    {
    case OvmsVehicle::Success:
      writer->printf("Homelink #%d activated\n",homelink);
      break;
    case OvmsVehicle::Fail:
      writer->printf("Error: Could not activate homelink #%d\n",homelink);
      break;
    default:
      writer->puts("Error: Homelink functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_climatecontrol(int verbosity, OvmsWriter* writer, bool on)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandClimateControl(on))
    {
    case OvmsVehicle::Success:
      writer->puts("Climate Control ");
      writer->puts(on ? "on" : "off");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Climate Control failed");
      break;
    default:
      writer->puts("Error: Climate Control functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_climatecontrol_on(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  vehicle_climatecontrol(verbosity, writer, true);
  }

void OvmsVehicleFactory::vehicle_climatecontrol_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  vehicle_climatecontrol(verbosity, writer, false);
  }


void OvmsVehicleFactory::vehicle_climate_schedule_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicle* current = MyVehicleFactory.ActiveVehicle();
  if (!current)
  {
    writer->puts("Error: No vehicle module loaded");
    return;
  }

  const char* day = argv[0];
  const char* times = argv[1];

  // Validate day
  const char* valid_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  bool day_valid = false;
  for (int i = 0; i < 7; i++)
  {
    if (strcasecmp(day, valid_days[i]) == 0)
    {
      day_valid = true;
      break;
    }
  }

  if (!day_valid)
  {
    writer->puts("ERROR: Invalid day. Use: mon, tue, wed, thu, fri, sat, sun");
    return;
  }

  // Validate and normalize time format: HH:MM[/duration][,HH:MM[/duration],...]
  std::string time_str(times);
  std::string normalized_schedule;
  size_t start = 0;
  size_t end = time_str.find(',');

  while (start != std::string::npos)
  {
    std::string entry = (end == std::string::npos) ?
                        time_str.substr(start) :
                        time_str.substr(start, end - start);

    // Split by '/' for time/duration
    size_t slash_pos = entry.find('/');
    std::string time_part = (slash_pos != std::string::npos) ?
                            entry.substr(0, slash_pos) : entry;
    
    // Validate time part (HH:MM or H:MM)
    size_t colon_pos = time_part.find(':');
    if (colon_pos == std::string::npos || colon_pos == 0)
    {
      writer->printf("ERROR: Invalid time format '%s'. Use HH:MM or H:MM\n", entry.c_str());
      return;
    }

    int hour = atoi(time_part.substr(0, colon_pos).c_str());
    int min = atoi(time_part.substr(colon_pos + 1).c_str());

    if (hour < 0 || hour > 23 || min < 0 || min > 59)
    {
      writer->printf("ERROR: Invalid time '%s'. Hour must be 0-23, minute must be 0-59\n", time_part.c_str());
      return;
    }

    // Validate duration if present
    int duration = -1;
    if (slash_pos != std::string::npos)
    {
      std::string duration_str = entry.substr(slash_pos + 1);
      duration = atoi(duration_str.c_str());
      if (duration < 5 || duration > 30)
      {
        writer->printf("ERROR: Invalid duration '%s'. Use 5 to 30 minutes\n", duration_str.c_str());
        return;
      }
    }

    // Normalize to HH:MM[/duration] format
    char normalized_entry[16];
    if (duration >= 0)
    {
      snprintf(normalized_entry, sizeof(normalized_entry), "%02d:%02d/%d", hour, min, duration);
    }
    else
    {
      snprintf(normalized_entry, sizeof(normalized_entry), "%02d:%02d", hour, min);
    }

    // Append to normalized schedule
    if (!normalized_schedule.empty())
      normalized_schedule += ",";
    normalized_schedule += normalized_entry;

    // Move to next entry
    if (end == std::string::npos)
      break;
    start = end + 1;
    end = time_str.find(',', start);
  }

  // Save normalized schedule to config
  std::string config_key = std::string("climate.schedule.") + day;
  MyConfig.SetParamValue("vehicle", config_key.c_str(), normalized_schedule.c_str());

  writer->printf("Climate control schedule set for %s: %s\n", day, normalized_schedule.c_str());
  ESP_LOGI(TAG, "Climate control schedule set for %s: %s", day, normalized_schedule.c_str());
}

void OvmsVehicleFactory::vehicle_climate_schedule_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char* day_names[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  const char* day_full[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

  writer->puts("Climate control schedules:");
  writer->puts("==========================");

  bool has_schedules = false;

  for (int i = 0; i < 7; i++)
  {
    std::string config_key = std::string("climate.schedule.") + day_names[i];
    std::string schedule = MyConfig.GetParamValue("vehicle", config_key.c_str());
    
    if (!schedule.empty())
    {
      has_schedules = true;
      writer->printf("%-10s: %s\n", day_full[i], schedule.c_str());
    }
  }

  if (!has_schedules)
  {
    writer->puts("No schedules configured.");
  }

  // Show next scheduled event
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  if (timeinfo != NULL && has_schedules)
  {
    int current_day = timeinfo->tm_wday;
    int current_hour = timeinfo->tm_hour;
    int current_min = timeinfo->tm_min;
    int current_total_min = current_day * 24 * 60 + current_hour * 60 + current_min;

    int next_day = -1;
    int next_hour = -1;
    int next_min = -1;
    int next_duration = 0;
    int next_total_min = current_total_min + 10000;

    for (int day_offset = 0; day_offset < 7; day_offset++)
    {
      int check_day = (current_day + day_offset) % 7;
      int day_index = check_day;

      std::string config_key = std::string("climate.schedule.") + day_names[day_index];
      std::string schedule = MyConfig.GetParamValue("vehicle", config_key.c_str());
      
      if (!schedule.empty())
      {
        size_t start = 0;
        size_t end = schedule.find(',');

        while (start != std::string::npos)
        {
          std::string entry = (end == std::string::npos) ?
                              schedule.substr(start) :
                              schedule.substr(start, end - start);

          size_t slash_pos = entry.find('/');
          std::string time_part = (slash_pos != std::string::npos) ?
                                  entry.substr(0, slash_pos) : entry;
          
          size_t colon_pos = time_part.find(':');
          if (colon_pos != std::string::npos && colon_pos > 0)
          {
            int hour = atoi(time_part.substr(0, colon_pos).c_str());
            int min = atoi(time_part.substr(colon_pos + 1).c_str());
            int duration = 0;
            
            if (slash_pos != std::string::npos)
            {
              duration = atoi(entry.substr(slash_pos + 1).c_str());
            }

            int schedule_total_min = check_day * 24 * 60 + hour * 60 + min;

            if (schedule_total_min > current_total_min && schedule_total_min < next_total_min)
            {
              next_total_min = schedule_total_min;
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
    }

    if (next_day >= 0)
    {
      writer->puts("");
      if (next_duration > 0)
      {
        writer->printf("Next scheduled: %s at %02d:%02d (%d min)\n",
                     day_full[next_day], next_hour, next_min, next_duration);
      }
      else
      {
        writer->printf("Next scheduled: %s at %02d:%02d\n",
                     day_full[next_day], next_hour, next_min);
      }
    }
  }
}

void OvmsVehicleFactory::vehicle_climate_schedule_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char* day = argv[0];

  if (strcasecmp(day, "all") == 0)
  {
    auto lock = MyConfig.Lock();
    const char* day_names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    for (int i = 0; i < 7; i++)
    {
      std::string config_key = std::string("climate.schedule.") + day_names[i];
      MyConfig.DeleteInstance("vehicle", config_key.c_str());
    }
    writer->puts("All climate control schedules cleared");
    ESP_LOGI(TAG, "All climate control schedules cleared");
    return;
  }

  const char* valid_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  bool day_valid = false;
  for (int i = 0; i < 7; i++)
  {
    if (strcasecmp(day, valid_days[i]) == 0)
    {
      day_valid = true;
      break;
    }
  }

  if (!day_valid)
  {
    writer->puts("ERROR: Invalid day. Use: mon, tue, wed, thu, fri, sat, sun, or 'all'");
    return;
  }

  std::string config_key = std::string("climate.schedule.") + day;
  MyConfig.DeleteInstance("vehicle", config_key.c_str());

  writer->printf("Climate control schedule cleared for %s\n", day);
  ESP_LOGI(TAG, "Climate control schedule cleared for %s", day);
}

void OvmsVehicleFactory::vehicle_climate_schedule_copy(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char* source_day = argv[0];
  const char* target_spec = argv[1];
  
  const char* valid_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  
  auto lock = MyConfig.Lock();
  
  // Validate source day
  bool source_valid = false;
  for (int i = 0; i < 7; i++)
  {
    if (strcasecmp(source_day, valid_days[i]) == 0)
    {
      source_valid = true;
      break;
    }
  }
  
  if (!source_valid)
  {
    writer->puts("ERROR: Invalid source day. Use: mon, tue, wed, thu, fri, sat, sun");
    return;
  }
  
  // Get source schedule
  std::string source_config_key = std::string("climate.schedule.") + source_day;
  std::string source_schedule = MyConfig.GetParamValue("vehicle", source_config_key.c_str());
  
  if (source_schedule.empty())
  {
    writer->printf("ERROR: No schedule configured for %s\n", source_day);
    return;
  }
  
  // Parse target specification
  std::vector<std::string> target_days;
  std::string target_str(target_spec);
  
  // Check for range pattern (e.g., "tue-fri" or "mon-sun")
  size_t dash_pos = target_str.find('-');
  if (dash_pos != std::string::npos && dash_pos > 0 && dash_pos < target_str.length() - 1)
  {
    std::string start_day = target_str.substr(0, dash_pos);
    std::string end_day = target_str.substr(dash_pos + 1);
    
    // Find start and end indices
    int start_idx = -1;
    int end_idx = -1;
    for (int i = 0; i < 7; i++)
    {
      if (strcasecmp(start_day.c_str(), valid_days[i]) == 0)
        start_idx = i;
      if (strcasecmp(end_day.c_str(), valid_days[i]) == 0)
        end_idx = i;
    }
    
    if (start_idx < 0 || end_idx < 0)
    {
      writer->printf("ERROR: Invalid day range '%s'\n", target_spec);
      return;
    }
    
    // Add all days in range
    if (start_idx <= end_idx)
    {
      for (int i = start_idx; i <= end_idx; i++)
        target_days.push_back(valid_days[i]);
    }
    else
    {
      // Wrap around (e.g., fri-mon = fri, sat, sun, mon)
      for (int i = start_idx; i < 7; i++)
        target_days.push_back(valid_days[i]);
      for (int i = 0; i <= end_idx; i++)
        target_days.push_back(valid_days[i]);
    }
  }
  else
  {
    // Parse comma-separated list
    size_t start = 0;
    size_t comma_pos = target_str.find(',');
    
    while (start != std::string::npos)
    {
      std::string day = (comma_pos == std::string::npos) ?
                        target_str.substr(start) :
                        target_str.substr(start, comma_pos - start);
      
      // Trim whitespace
      size_t first = day.find_first_not_of(" \t");
      size_t last = day.find_last_not_of(" \t");
      if (first != std::string::npos && last != std::string::npos)
        day = day.substr(first, last - first + 1);
      
      // Validate day
      bool day_valid = false;
      for (int i = 0; i < 7; i++)
      {
        if (strcasecmp(day.c_str(), valid_days[i]) == 0)
        {
          target_days.push_back(valid_days[i]);
          day_valid = true;
          break;
        }
      }
      
      if (!day_valid)
      {
        writer->printf("ERROR: Invalid target day '%s'\n", day.c_str());
        return;
      }
      
      if (comma_pos == std::string::npos)
        break;
      start = comma_pos + 1;
      comma_pos = target_str.find(',', start);
    }
  }
  
  if (target_days.empty())
  {
    writer->puts("ERROR: No valid target days specified");
    return;
  }
  
  // Copy schedule to all target days
  int copied_count = 0;
  for (const auto& day : target_days)
  {
    // Skip source day
    if (strcasecmp(day.c_str(), source_day) == 0)
      continue;
    
    std::string config_key = std::string("climate.schedule.") + day;
    MyConfig.SetParamValue("vehicle", config_key.c_str(), source_schedule);
    copied_count++;
  }
  
  if (copied_count > 0)
  {
    writer->printf("Schedule '%s' copied from %s to %d day(s):\n",
                   source_schedule.c_str(), source_day, copied_count);
    for (const auto& day : target_days)
    {
      if (strcasecmp(day.c_str(), source_day) != 0)
        writer->printf("  %s\n", day.c_str());
    }
    ESP_LOGI(TAG, "Climate schedule copied from %s to %d target days", source_day, copied_count);
  }
  else
  {
    writer->puts("No schedules were copied (source day was in target list)");
  }
}

void OvmsVehicleFactory::vehicle_climate_schedule_enable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  MyConfig.SetParamValueBool("vehicle", "climate.precondition", true);
  writer->puts("Scheduled precondition enabled");
  ESP_LOGI(TAG, "Scheduled precondition enabled");
}

void OvmsVehicleFactory::vehicle_climate_schedule_disable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  MyConfig.SetParamValueBool("vehicle", "climate.precondition", false);
  writer->puts("Scheduled precondition disabled");
  ESP_LOGI(TAG, "Scheduled precondition disabled");
}

void OvmsVehicleFactory::vehicle_climate_schedule_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  bool enabled = MyConfig.GetParamValueBool("vehicle", "climate.precondition", false);
  
  writer->printf("Scheduled precondition: %s\n", enabled ? "ENABLED" : "DISABLED");
  
  if (!enabled)
  {
    writer->puts("\nSchedule is disabled. No automatic precondition will be triggered.");
    writer->puts("Use 'climatecontrol schedule enable' to activate.");
    return;
  }
  
  // Show configured schedules
  vehicle_climate_schedule_list(verbosity, writer, cmd, argc, argv);
}


void OvmsVehicleFactory::vehicle_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandLock(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle locked");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be locked");
      break;
    default:
      writer->puts("Error: Vehicle lock functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandUnlock(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle unlocked");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be unlocked");
      break;
    default:
      writer->puts("Error: Vehicle unlock functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_valet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandActivateValet(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle valet mode activated");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not activate valet mode");
      break;
    default:
      writer->puts("Error: Vehicle valet functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_unvalet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandDeactivateValet(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle valet mode deactivated");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not deactivate valet mode");
      break;
    default:
      writer->puts("Error: Vehicle valet functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  const char* smode = cmd->GetName();
  OvmsVehicle::vehicle_mode_t mode = OvmsVehicle::Standard;
  if (strcmp(smode,"storage")==0)
    mode = OvmsVehicle::Storage;
  else if (strcmp(smode,"range")==0)
    mode = OvmsVehicle::Range;
  else if (strcmp(smode,"performance")==0)
    mode = OvmsVehicle::Performance;
  else if (strcmp(smode,"standard")==0)
    mode = OvmsVehicle::Standard;
  else
    {
    writer->printf("Error: Unrecognised charge mode (%s) not standard/storage/range/performance\n",smode);
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeMode(mode))
    {
    case OvmsVehicle::Success:
      writer->printf("Charge mode '%s' set\n",smode);
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not set charge mode");
      break;
    default:
      writer->puts("Error: Charge mode functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_current(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int limit = atoi(argv[0]);

  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeCurrent((uint16_t) limit))
    {
    case OvmsVehicle::Success:
      writer->printf("Charge current limit set to %dA\n",limit);
      break;
    case OvmsVehicle::Fail:
      writer->printf("Error: Could not set charge current limit to %dA\n",limit);
      break;
    default:
      writer->puts("Error: Charge current limit functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandStartCharge())
    {
    case OvmsVehicle::Success:
      writer->puts("Charge has been started");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not start charge");
      break;
    default:
      writer->puts("Error: Charge start functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandStopCharge())
    {
    case OvmsVehicle::Success:
      writer->puts("Charge has been stopped");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not stop charge");
      break;
    default:
      writer->puts("Error: Charge stop functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_cooldown(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandCooldown(true))
    {
    case OvmsVehicle::Success:
      writer->puts("Cooldown has been started");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not start cooldown");
      break;
    default:
      writer->puts("Error: Cooldown functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_stat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle->CommandStat(verbosity, writer) == OvmsVehicle::NotImplemented)
    {
    writer->puts("Error: Functionality not available");
    }
  }

void OvmsVehicleFactory::vehicle_stat_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle->CommandStatTrip(verbosity, writer) == OvmsVehicle::NotImplemented)
    {
    writer->puts("Error: Functionality not available");
    }
  }

void OvmsVehicleFactory::bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
      OvmsVehicle::vehicle_bms_status_t statusmode = OvmsVehicle::vehicle_bms_status_t::Both;
      const char* smode = cmd->GetName();
      if (strcmp(smode,"volt")==0)
        statusmode = OvmsVehicle::vehicle_bms_status_t::Voltage;
      else if (strcmp(smode,"temp")==0)
        statusmode = OvmsVehicle::vehicle_bms_status_t::Temperature;

      MyVehicleFactory.m_currentvehicle->BmsStatus(verbosity, writer, statusmode);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void OvmsVehicleFactory::bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->BmsResetCellStats();
    writer->puts("BMS cell statistics have been reset.");
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void OvmsVehicleFactory::vehicle_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("AUX BATTERY\n");
  if (StdMetrics.ms_v_bat_12v_voltage->IsDefined())
    {
    const std::string &auxBatt = StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Volts, 2);
    writer->printf("  Voltage: %s\n", auxBatt.c_str());
    }
  if (StdMetrics.ms_v_bat_12v_current->IsDefined())
    {
    const std::string &auxBattI = StdMetrics.ms_v_bat_12v_current->AsUnitString("-", Amps, 2);
    writer->printf("  Current: %s\n", auxBattI.c_str());
    }

  if (StdMetrics.ms_v_bat_12v_voltage_ref->IsDefined())
    {
    const std::string &auxBattVref = StdMetrics.ms_v_bat_12v_voltage_ref->AsUnitString("-", Volts, 2);
    writer->printf("  Voltage Ref: %s\n", auxBattVref.c_str());
    }

  if (StdMetrics.ms_v_bat_12v_voltage_alert->IsDefined())
    {
    const std::string &auxBattI = StdMetrics.ms_v_bat_12v_voltage_alert->AsUnitString("-", Volts, 2);
    writer->printf("  Voltage Alert: %s\n", auxBattI.c_str());
    }
  }

void OvmsVehicleFactory::vehicle_aux_monitor(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle *mycar = MyVehicleFactory.ActiveVehicle();
  if (mycar == NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }
  if (!mycar->m_aux_enabled)
    {
    writer->puts("Aux Monitor Disabled");
    return;
    }

  std::string stat = mycar->BatteryMonStat();
  writer->puts(stat.c_str());
  }

void OvmsVehicleFactory::vehicle_aux_monitor_enable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle *mycar = MyVehicleFactory.ActiveVehicle();
  if (mycar == NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }
  if (argc == 0)
    {
    mycar->EnableAuxMonitor();
    }
  else
    {
    float low_thresh, charge_thresh = 0;
    if (0==sscanf(argv[0], "%f", &low_thresh))
      {
      writer->puts("Error: Expected float for low threshold");
      return;
      }
    if (argc > 1 )
      {
      if (0 == sscanf(argv[1], "%f", &charge_thresh))
        {
        writer->puts("Error: Expected float for charge threshold");
        return;
        }
      }
    mycar->EnableAuxMonitor(low_thresh, charge_thresh);

    }
  writer->printf("Enabled (low=%f charge=%f)\n", mycar->m_aux_battery_mon.low_threshold(), mycar->m_aux_battery_mon.charge_threshold());
  }

void OvmsVehicleFactory::vehicle_aux_monitor_disable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle *mycar = MyVehicleFactory.ActiveVehicle();
  if (mycar == NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }
  mycar->DisableAuxMonitor();
  writer->puts("Disabled");
  }

void OvmsVehicleFactory::bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->FormatBmsAlerts(verbosity, writer, true);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }


void OvmsVehicleFactory::obdii_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyVehicleFactory.m_currentvehicle)
    {
    writer->puts("ERROR: no vehicle module selected\nHint: use module 'NONE' (empty vehicle)");
    return;
    }

  const char* busname = cmd->GetParent()->GetParent()->GetName();
  canbus* bus = (canbus*) MyPcpApp.FindDeviceByName(busname);
  if (!bus)
    {
    writer->printf("ERROR: CAN bus %s not available\n", busname);
    return;
    }
  else if (bus->m_mode != CAN_MODE_ACTIVE)
    {
    writer->printf("ERROR: CAN bus %s not in active mode\n", busname);
    return;
    }
#ifndef CONFIG_OVMS_COMP_POLLER
  writer->puts("Poller not implemented");
#else

  uint32_t txid = 0, rxid = 0;
  uint8_t protocol = ISOTP_STD;
  int timeout_ms = 3000;
  std::string request;
  std::string response;

  // parse args:
  const char* target = cmd->GetName();
  if (strcmp(target, "device") == 0)
    {
    // device: [-e|-E|-v] [-t<timeout_ms>] txid rxid request
    int argpos = 0;
    for (int i = 0; i < argc; i++)
      {
      if (argv[i][0] == '-')
        {
        switch (argv[i][1])
          {
          case 'e':
            protocol = ISOTP_EXTADR;
            break;
          case 'E':
            protocol = ISOTP_EXTFRAME;
            break;
          case 'v':
            protocol = VWTP_20;
            break;
          case 't':
            timeout_ms = atoi(argv[i]+2);
            break;
          default:
            writer->printf("ERROR: unknown option '%s'\n", argv[i]);
            return;
          }
        }
      else
        {
        switch (++argpos)
          {
          case 1:
            txid = strtol(argv[i], NULL, 16);
            break;
          case 2:
            rxid = strtol(argv[i], NULL, 16);
            break;
          case 3:
            request = hexdecode(argv[i]);
            break;
          default:
            writer->puts("ERROR: too many args");
            return;
          }
        }
      }
    if (argpos < 3)
      {
      writer->puts("ERROR: too few args, need: txid rxid request");
      return;
      }
    }
  else
    {
    // broadcast: [-t<timeout_ms>] request
    int argpos = 0;
    for (int i = 0; i < argc; i++)
      {
      if (argv[i][0] == '-')
        {
        switch (argv[i][1])
          {
          case 't':
            timeout_ms = atoi(argv[i]+2);
            break;
          default:
            writer->printf("ERROR: unknown option '%s'\n", argv[i]);
            return;
          }
        }
      else
        {
        switch (++argpos)
          {
          case 1:
            request = hexdecode(argv[i]);
            break;
          default:
            writer->puts("ERROR: too many args");
            return;
          }
        }
      }
    if (argpos < 1)
      {
      writer->puts("ERROR: too few args, need: request");
      return;
      }
    txid = 0x7df;
    rxid = 0;
    }

  // validate request:
  if (timeout_ms <= 0)
    {
    writer->puts("ERROR: invalid timeout (must be > 0)");
    return;
    }
  if (request.size() == 0)
    {
    writer->puts("ERROR: no request");
    return;
    }
  else
    {
    uint8_t type = request.at(0);
    if ((POLL_TYPE_HAS_16BIT_PID(type) && request.size() < 3) ||
        (POLL_TYPE_HAS_8BIT_PID(type) && request.size() < 2))
      {
      writer->printf("ERROR: request too short for type %02X\n", type);
      return;
      }
    }

  // execute request:
  int err = MyVehicleFactory.m_currentvehicle->PollSingleRequest(bus, txid, rxid,
    request, response, timeout_ms, protocol);

  writer->printf("%" PRIx32 "[%" PRIx32 "] %s: ", txid, rxid, hexencode(request).c_str());
  if (err == POLLSINGLE_TXFAILURE)
    {
    writer->puts("ERROR: transmission failure (CAN bus error)");
    return;
    }
  else if (err < 0)
    {
    writer->puts("ERROR: timeout waiting for poller/response");
    return;
    }
  else if (err)
    {
    const char* errname = OvmsPoller::PollResultCodeName(err);
    writer->printf("ERROR: request failed with response error code %02X%s%s\n", err,
      errname ? " " : "", errname ? errname : "");
    return;
    }

  // output response as hex dump:
  writer->puts("Response:");
  char *buf = NULL;
  size_t rlen = response.size(), offset = 0;
  do
    {
    rlen = FormatHexDump(&buf, response.data() + offset, rlen, 16);
    offset += 16;
    writer->puts(buf ? buf : "-");
    }
  while (rlen);

  if (buf)
    free(buf);
#endif
  }
