/*
;    Project:       Open Vehicle Monitor System
;    Date:          10th May 2020
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
static const char *TAG = "tpms";

const char *TPMS_PARAM = "tpms";

#include <iostream>
#include <iomanip>
#include "ovms_tpms.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "vehicle.h"

OvmsTPMS MyTPMS __attribute__ ((init_priority (4700)));

void tpms_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("\nTyre Sets:");
  OvmsConfigParam *p = MyConfig.CachedParam(TPMS_PARAM);
  if (!p || p->m_map.empty())
    {
    writer->puts("No tyre sets defined.");
    }
  else if (p)
    {
    for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
      {
      writer->printf("  %s: %s\n", it->first.c_str(),it->second.c_str());
      }
    }
  }

void tpms_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::ostringstream* buffer = new std::ostringstream();

  for (int k=1;k<argc;k++)
    {
    if (k>1) { *buffer << ","; }
    for (const char *p=argv[k];*p!=0;p++)
      {
      if (!(((*p>='0')&&(*p<='9'))||
           ((*p>='a')&&(*p<='f'))||
           ((*p>='A')&&(*p<='F'))))
        {
        delete buffer;
        writer->printf("Error: Tyre ID '%s' if not valid\n",argv[k]);
        return;
        }
      }
    uint32_t id = strtoul(argv[k], (char**)0, 16);
    *buffer << std::nouppercase << std::setfill('0') << std::setw(8) << std::hex << id;
    }

  std::string result = buffer->str();
  MyConfig.SetParamValue(TPMS_PARAM,argv[0],result);
  writer->printf("Tyre set '%s' defined as %s\n",argv[0],result.c_str());

  delete buffer;
  }

void tpms_delete(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.IsDefined(TPMS_PARAM,argv[0]))
    {
    MyConfig.DeleteInstance(TPMS_PARAM,argv[0]);
    writer->printf("Deleted tyre set '%s'\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Tyre set '%s' is not defined\n",argv[0]);
    }
  }

void tpms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool data_shown = false;

  writer->printf("TPMS status       ");
  if (MyVehicleFactory.m_currentvehicle)
    {
    std::vector<std::string> tpms_layout = MyVehicleFactory.m_currentvehicle->GetTpmsLayout();
    for (auto wheel : tpms_layout)
      writer->printf(" %8s", wheel.c_str());
    writer->puts("");
    }
  else
    {
    writer->puts("(axles front to back, per axle left to right)");
    }

  if (StandardMetrics.ms_v_tpms_alert->IsDefined())
    {
    const char* alertname[] = { "OK", "WARN", "ALERT" };  // Added UNKNOWN
    writer->printf("Alert level.....: ");
    for (auto val : StandardMetrics.ms_v_tpms_alert->AsVector())
      {
      // Bounds check to prevent array overflow
      if (val >= 0 && val <= 2)
        writer->printf(" %8s", alertname[val]);
      else if (val < 0)
        writer->printf(" %8s", "UNKNOWN");
      else
        writer->printf(" %8s", "INVALID");
      }
    writer->puts(StandardMetrics.ms_v_tpms_alert->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_health->IsDefined())
    {
    writer->printf("Health.......[%%]: ");
    for (auto val : StandardMetrics.ms_v_tpms_health->AsVector())
      {
      // Validate value before printing
      if (val >= 0.0f && val <= 100.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_health->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_pressure->IsDefined())
    {
    metric_unit_t user_pressure = OvmsMetricGetUserUnit(GrpPressure, kPa);
    writer->printf("Pressure...[%s]: ", OvmsMetricUnitLabel(user_pressure));
    for (auto val : StandardMetrics.ms_v_tpms_pressure->AsVector(user_pressure))
      {
      // Validate pressure (typical range 0-500 kPa)
      if (!std::isnan(val) && val >= 0.0f && val <= 500.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_pressure->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (StandardMetrics.ms_v_tpms_temp->IsDefined())
    {
    metric_unit_t user_temp = OvmsMetricGetUserUnit(GrpTemp, Celcius);
    writer->printf("Temperature.[%s]: ", OvmsMetricUnitLabel(user_temp));
    for (auto val : StandardMetrics.ms_v_tpms_temp->AsVector(user_temp))
      {
      // Validate temperature (typical range -40 to +150 °C)
      if (!std::isnan(val) && val >= -50.0f && val <= 200.0f)
        writer->printf(" %8.1f", val);
      else
        writer->printf(" %8s", "-");
      }
    writer->puts(StandardMetrics.ms_v_tpms_temp->IsStale() ? "  [stale]" : "");
    data_shown = true;
    }

  if (!data_shown)
    writer->puts("Sorry, no data available. Try switching the vehicle on.");

  writer->puts("");
  tpms_list(verbosity, writer, cmd, argc, argv);
  }

void tpms_read(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle* ov = MyVehicleFactory.ActiveVehicle();
  if (ov == NULL)
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }

  std::vector<uint32_t> tpms;
  if ((!ov->TPMSRead(&tpms)) || tpms.empty())
    {
    writer->puts("Error: TPMS IDs could not be read from the vehicle (or not implemented)");
    return;
    }

  std::ostringstream* buffer = new std::ostringstream();
  bool first = true;
  for(uint32_t id : tpms)
    {
    if (!first) { *buffer << ","; }
    first = false;
    *buffer << std::nouppercase << std::setfill('0') << std::setw(8) << std::hex << id;
    }

  std::string result = buffer->str();
  delete buffer;

  if (argc>0)
    {
    MyConfig.SetParamValue(TPMS_PARAM,argv[0],result);
    writer->printf("Tyre set '%s' read as %s\n",argv[0],result.c_str());
    }
  else
    {
    writer->printf("TPMS read as %s\n",result.c_str());
    }
  }

void tpms_write(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle* ov = MyVehicleFactory.ActiveVehicle();
  if (ov == NULL)
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }

  if (! MyConfig.IsDefined(TPMS_PARAM,argv[0]))
    {
    writer->printf("Error: Tyre set '%s' not found\n",argv[0]);
    return;
    }

  std::string ids = MyConfig.GetParamValue(TPMS_PARAM,argv[0]);
  std::vector<uint32_t> tpms;
  size_t pos;
  while ((pos = ids.find(",")) != std::string::npos)
    {
    std::string token = ids.substr(0, pos);
    tpms.push_back(strtoul(token.c_str(), (char**)0, 16));
    ids.erase(0, pos + 1);
    }
  tpms.push_back(strtoul(ids.c_str(), (char**)0, 16));

  if (ov->TPMSWrite(tpms))
    {
    writer->printf("Tyre set '%s' written to vehicle TPMS successfully\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Tyre IDs could not be written to the vehicle (or not implemented)\n");
    }
  }

void tpms_mapping(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("\nTPMS Sensor Mapping:");
  
  if (!MyVehicleFactory.m_currentvehicle) 
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }
  
  OvmsVehicle* vehicle = MyVehicleFactory.m_currentvehicle;
  
  // Get wheel position names and layout from vehicle
  std::vector<std::string> wheel_names = vehicle->GetTpmsLayoutNames();
  std::vector<std::string> tpms_layout = vehicle->GetTpmsLayout();
  
  bool sensor_mapping = vehicle->UsesTpmsSensorMapping();
  if (!sensor_mapping || tpms_layout.empty()) 
    {
    writer->puts("Error: No TPMS layout defined for this vehicle");
    return;
    }
  
  if (wheel_names.empty() || wheel_names.size() != tpms_layout.size())
    {
    writer->puts("Error: Invalid TPMS layout configuration");
    return;
    }
  
  // Find maximum wheel name length for alignment
  size_t max_name_length = 0;
  for (const auto& name : wheel_names)
    {
    if (name.length() > max_name_length)
      max_name_length = name.length();
    }
  
  int count = (int)tpms_layout.size();
  std::vector<int> current_map(count);
  // Read mapping
  for (int i = 0; i < count; i++)
    {
    current_map[i] = MyConfig.GetParamValueInt("vehicle", std::string("tpms.")+str_tolower(tpms_layout[i]), i);
    }
  
  // Display current mapping configuration
  for (int i = 0; i < count; i++) 
    {
    int idx = current_map[i];
    std::string sensor_name = (idx >= 0 && idx < (int)tpms_layout.size()) 
                              ? tpms_layout[idx] 
                              : "Invalid";
    writer->printf("  %-*s (%s): Sensor #%d (%s)\n", 
                   (int)max_name_length,
                   wheel_names[i].c_str(), 
                   tpms_layout[i].c_str(), 
                   idx, 
                   sensor_name.c_str());
    }
  
  // Show detailed TPMS data with applied mapping
  if (StandardMetrics.ms_v_tpms_pressure->IsDefined()) 
    {
    // Call tpms_status to display current TPMS values
    tpms_status(verbosity, writer, NULL, 0, NULL);
    }
  
  // Validate mapping configuration
  bool has_issues = false;
  
  // Check for duplicate sensor assignments
  for (int i = 0; i < count; i++) 
    {
    for (int j = i+1; j < count; j++)
      {
      if (current_map[i] == current_map[j]) 
        {
        if (!has_issues) writer->puts("\nWARNINGS:");
        writer->printf("  Duplicate mapping: %s and %s both use Sensor #%d\n",
                       wheel_names[i].c_str(), wheel_names[j].c_str(), current_map[i]);
        has_issues = true;
        }
      }
    }
  
  // Check for out-of-range sensor indices
  for (int i = 0; i < count; i++) 
    {
    if (current_map[i] < 0 || current_map[i] >= (int)tpms_layout.size()) 
      {
      if (!has_issues) writer->puts("\nWARNINGS:");
      writer->printf("  %s: Sensor #%d is out of range (valid: 0-%d)\n",
                     wheel_names[i].c_str(), current_map[i], (int)tpms_layout.size()-1);
      has_issues = true;
      }
    }
  }

/**
 * tpms_map_get: Display TPMS data in V2/MP Y message format for apps
 * 
 * Format (comma-separated):
 *   <wheel_count>,<wheel_names...>,
 *   <pressure_count>,<pressures...>,<pressure_validity>,
 *   <temp_count>,<temps...>,<temp_validity>,
 *   <health_count>,<healths...>,<health_validity>,
 *   <alert_count>,<alerts...>,<alert_validity>
 * 
 * Validity indicators: -1=undefined, 0=stale, 1=valid
 * 
 * Example (4 wheels):
 *   4,FL,FR,RL,RR,4,230.5,245.0,238.2,241.8,1,4,21.0,22.5,20.8,23.1,1,4,100.0,100.0,100.0,100.0,1,4,0,0,0,0,1
 */
void tpms_map_get(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyVehicleFactory.m_currentvehicle) 
    {
    writer->puts("0");  // 0 wheels = no vehicle
    return;
    }
  
  OvmsVehicle* vehicle = MyVehicleFactory.m_currentvehicle;
  std::vector<std::string> tpms_layout = vehicle->GetTpmsLayout();
  
  if (tpms_layout.empty()) 
    {
    writer->puts("0");  // 0 wheels = no layout
    return;
    }
  
  int count = (int)tpms_layout.size();
  
  // Helper lambda for validity indicator: -1=undefined, 0=stale, 1=valid
  auto get_validity = [](OvmsMetric* m) -> int {
    if (!m || !m->IsDefined()) return -1;
    if (m->IsStale()) return 0;
    return 1;
    };
  
  // Wheel names section
  writer->printf("%d", count);
  for (int i = 0; i < count; i++)
    {
    writer->printf(",%s", tpms_layout[i].c_str());
    }
  
  // Pressure section
  auto pressure = StandardMetrics.ms_v_tpms_pressure->AsVector();
  int p_count = (int)pressure.size();
  writer->printf(",%d", p_count);
  for (int i = 0; i < p_count; i++)
    {
    if (!std::isnan(pressure[i]))
      writer->printf(",%.1f", pressure[i]);
    else
      writer->printf(",");
    }
  writer->printf(",%d", get_validity(StandardMetrics.ms_v_tpms_pressure));
  
  // Temperature section
  auto temp = StandardMetrics.ms_v_tpms_temp->AsVector();
  int t_count = (int)temp.size();
  writer->printf(",%d", t_count);
  for (int i = 0; i < t_count; i++)
    {
    if (!std::isnan(temp[i]))
      writer->printf(",%.1f", temp[i]);
    else
      writer->printf(",");
    }
  writer->printf(",%d", get_validity(StandardMetrics.ms_v_tpms_temp));
  
  // Health section
  auto health = StandardMetrics.ms_v_tpms_health->AsVector();
  int h_count = (int)health.size();
  writer->printf(",%d", h_count);
  for (int i = 0; i < h_count; i++)
    {
    if (!std::isnan(health[i]))
      writer->printf(",%.1f", health[i]);
    else
      writer->printf(",");
    }
  writer->printf(",%d", get_validity(StandardMetrics.ms_v_tpms_health));
  
  // Alert section
  auto alert = StandardMetrics.ms_v_tpms_alert->AsVector();
  int a_count = (int)alert.size();
  writer->printf(",%d", a_count);
  for (int i = 0; i < a_count; i++)
    {
    writer->printf(",%d", alert[i]);
    }
  writer->printf(",%d", get_validity(StandardMetrics.ms_v_tpms_alert));
  
  writer->puts("");
  }

/**
 * tpms_map_set: Set TPMS sensor mapping using wheel position names
 * Usage: tpms map set fl=fr fr=rr rl=fl rr=rl
 * This remaps which physical sensor data is shown at which wheel position
 */
void tpms_map_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyVehicleFactory.m_currentvehicle) 
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }
  
  OvmsVehicle* vehicle = MyVehicleFactory.m_currentvehicle;
  // Get wheel position names and layout from vehicle
  std::vector<std::string> tpms_layout = vehicle->GetTpmsLayout();
  
  bool sensor_mapping = vehicle->UsesTpmsSensorMapping();
  if (!sensor_mapping || tpms_layout.empty()) 
    {
    writer->puts("Error: No TPMS layout defined for this vehicle");
    return;
    }
  
  int count = (int)tpms_layout.size();
  std::vector<int> current_map(count);
  // Read current mapping: which sensor index is assigned to each wheel
  for (int i = 0; i < count; i++)
    {
    current_map[i] = MyConfig.GetParamValueInt("vehicle", std::string("tpms.")+str_tolower(tpms_layout[i]), i);
    }
  std::vector<int> new_map = current_map;

  // Process each argument of the form wheel=sensor
  for (int i = 0; i < argc; i++)
    {
    std::string arg(argv[i]);
    size_t pos = arg.find('=');
    
    if (pos == std::string::npos)
      {
      writer->printf("Error: Invalid argument '%s' (expected format: wheel=wheel)\n", argv[i]);
      writer->puts("Example: fl=fr fr=rr rl=fl rr=rl");
      return;
      }
    
    std::string dest_wheel = arg.substr(0, pos);
    std::string src_wheel = arg.substr(pos + 1);
    
    // Convert to lowercase
    std::transform(dest_wheel.begin(), dest_wheel.end(), dest_wheel.begin(), ::tolower);
    std::transform(src_wheel.begin(), src_wheel.end(), src_wheel.begin(), ::tolower);
    
    // Find indices for dest_wheel and src_wheel
    int dest_idx = -1, src_idx = -1;
    for (int j = 0; j < count; ++j)
      {
      std::string layout_lower = tpms_layout[j];
      std::transform(layout_lower.begin(), layout_lower.end(), layout_lower.begin(), ::tolower);
      
      if (layout_lower == dest_wheel) dest_idx = j;
      if (layout_lower == src_wheel) src_idx = j;
      }
    
    if (dest_idx == -1)
      {
      writer->printf("Error: Unknown destination wheel '%s'\n", dest_wheel.c_str());
      return;
      }
    if (src_idx == -1)
      {
      writer->printf("Error: Unknown source wheel '%s'\n", src_wheel.c_str());
      return;
      }
    // Map: dest_wheel gets the sensor that was previously at src_wheel
    new_map[dest_idx] = current_map[src_idx];
    }
  
  
  // Save new mapping to config
  for (int i = 0; i < count; i++)
    {
    MyConfig.SetParamValueInt("vehicle", std::string("tpms.") + str_tolower(tpms_layout[i]), new_map[i]);
    }
  
  writer->puts("Mapping saved successfully");
  
  // Show current status
  writer->puts("");
  tpms_mapping(verbosity, writer, cmd, 0, nullptr);
  }

/**
 * tpms_map_reset: Reset TPMS sensor mapping to default (FL=0, FR=1, RL=2, RR=3)
 */
void tpms_map_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyVehicleFactory.m_currentvehicle) 
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }
  
  OvmsVehicle* vehicle = MyVehicleFactory.m_currentvehicle;
  std::vector<std::string> tpms_layout = vehicle->GetTpmsLayout();

  bool sensor_mapping = vehicle->UsesTpmsSensorMapping();
  if (!sensor_mapping || tpms_layout.empty()) 
    {
    writer->puts("Error: No TPMS layout defined for this vehicle");
    return;
    }
  
  int count = (int)tpms_layout.size();
  
  // Reset mapping to default: wheel[i] = sensor[i]
  for (int i = 0; i < count; i++)
    {
    MyConfig.SetParamValueInt("vehicle", std::string("tpms.") + str_tolower(tpms_layout[i]), i);
    }
  
  writer->puts("TPMS mapping reset to default");
  
  // Show current status
  writer->puts("");
  tpms_mapping(verbosity, writer, cmd, 0, nullptr);
  }

OvmsTPMS::OvmsTPMS()
  {
  ESP_LOGI(TAG, "Initialising TPMS (4700)");

  // Register our commands
  OvmsCommand* cmd_tpms = MyCommandApp.RegisterCommand("tpms","TPMS framework", tpms_status, "", 0, 0, false);
  cmd_tpms->RegisterCommand("status","Show TPMS status",tpms_status);
  cmd_tpms->RegisterCommand("list","Show TPMS tyre sets",tpms_list);
  cmd_tpms->RegisterCommand("read","Read TPMS IDs to sepecified tyre set",tpms_read,"<set>",0,1);
  cmd_tpms->RegisterCommand("write","Write TPMS IDs from sepecified tyre set",tpms_write,"<set>",1,1);
  cmd_tpms->RegisterCommand("set","Manually configure IDs in a tyre set",tpms_set,"<set> <id(s)>",1,9);
  cmd_tpms->RegisterCommand("delete","Delete the specified TPMS tyre set configuration",tpms_delete,"<set>",1,1);
  // TPMS mapping commands
  OvmsCommand* cmd_map = cmd_tpms->RegisterCommand("map","TPMS sensor mapping");
  cmd_map->RegisterCommand("get","Show mapping in machine-readable format",tpms_map_get);
  cmd_map->RegisterCommand("status","Show current mapping with details",tpms_mapping);
  cmd_map->RegisterCommand("set","Set sensor mapping",tpms_map_set,"<wheel=sensor> [...]",1,4);
  cmd_map->RegisterCommand("reset","Reset mapping to default",tpms_map_reset);

  // Register our parameters
  MyConfig.RegisterParam(TPMS_PARAM, "TPMS tyre sets", true, true);
  }

OvmsTPMS::~OvmsTPMS()
  {
  }
