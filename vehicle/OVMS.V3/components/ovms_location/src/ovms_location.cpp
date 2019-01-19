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
static const char *TAG = "location";

#include "ovms_location.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_script.h"
#include "ovms_notify.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include <math.h>

const char *LOCATIONS_PARAM = "locations";
#define LOCATION_DEFRADIUS 100

#define LOCATION_R 6371
#define LOCATION_TO_RAD (3.1415926536 / 180)

double OvmsLocationDistance(double th1, double ph1, double th2, double ph2)
  {
  double dx, dy, dz;

  ph1 -= ph2;
  ph1 *= LOCATION_TO_RAD, th1 *= LOCATION_TO_RAD, th2 *= LOCATION_TO_RAD;

  dz = sin(th1) - sin(th2);
  dx = cos(ph1) * cos(th1) - cos(th2);
  dy = sin(ph1) * cos(th1);
  return (asin(sqrt(dx * dx + dy * dy + dz * dz) / 2) * 2 * LOCATION_R)*1000.0;
  }

OvmsLocationAction::OvmsLocationAction(bool enter, enum LocationAction action, const char* params, int len)
  : m_enter(enter), m_action(action), m_params(params, len) {}

const char* OvmsLocationAction::ActionString()
  {
  switch (m_action)
    {
    case HOMELINK: return "homelink";
    case ACC: return "acc";
    case NOTIFY: return "notify";
    default: return "INVALID";
    }
  }

ActionList::~ActionList()
  {
  clear();
  }

void ActionList::clear()
  {
  for (iterator i = begin(); i != end(); ++i)
    delete *i;
  std::list<OvmsLocationAction*>::clear();
  }

ActionList::iterator ActionList::erase(iterator pos)
  {
  delete *pos;
  return std::list<OvmsLocationAction*>::erase(pos);
  }

OvmsLocation::OvmsLocation(const std::string& name)
  {
  m_name = name;
  m_inlocation = false;
  }

OvmsLocation::~OvmsLocation()
  {
  }

bool OvmsLocation::IsInLocation(float latitude, float longitude)
  {
  // This should check if we are in the location
  double dist = OvmsLocationDistance((double)latitude,(double)longitude,(double)m_latitude,(double)m_longitude);
  std::string event;

  // ESP_LOGI(TAG, "Location %s is %0.1fm distant",m_name.c_str(),dist);

  if (fabs(dist) <= m_radius)
    {
    // We are in the location
    if (!m_inlocation)
      {
      m_inlocation = true;
      if (StandardMetrics.ms_v_env_on->AsBool())
        {
        event = std::string("location.enter.");
        event.append(m_name);
        MyEvents.SignalEvent(event.c_str(), (void*)m_name.c_str(), m_name.size()+1);
        }
      }
    }
  else
    {
    // We are out of the location
    if (m_inlocation)
      {
      m_inlocation = false;
      if (StandardMetrics.ms_v_env_on->AsBool())
        {
        event = std::string("location.leave.");
        event.append(m_name);
        MyEvents.SignalEvent(event.c_str(), (void*)m_name.c_str(), m_name.size()+1);
        }
      }
    }

  // Return current status
  return m_inlocation;
  }

enum LocationAction validate_action(bool enter, const char* act, int alen, const char* par, int plen, const char** msg)
  {
  *msg = NULL;
  if (alen == 8 && strncasecmp(act, "homelink", alen) == 0)
    {
    if (plen != 1 || *par < '1' || *par > '3')
      {
      *msg = "homelink parameter must be 1, 2. or 3";
      return INVALID;
      }
    return HOMELINK;
    }
  if (alen == 3 && strncasecmp(act, "acc", alen) == 0)
    {
    if (!enter)
      {
      *msg = "Location action ACC not valid on leave";
      return INVALID;
      }
    return ACC;
    }
  if (alen == 6 && strncasecmp(act, "notify", alen) == 0)
    return  NOTIFY;
  *msg = "Unrecognized action keyword";
  return INVALID;
  }

bool OvmsLocation::Parse(const std::string& value)
  {
  const char *p = value.c_str();
  char next = 0;
  int len = 0;
  m_actions.clear();
  m_radius = LOCATION_DEFRADIUS;
  int num = sscanf(p, "%f , %f %n%1c", &m_latitude, &m_longitude, &len, &next);
  if (num < 2)
    return false;
  if (num < 3)
    return true;
  p += len;
  if (next == ',')
    {
    num = sscanf(p, ", %u %n%1c", &m_radius, &len, &next);
    if (num < 1)
      return false;
    if (num < 2)
      return true;
    p += len;
    }
  if (next != ';')
    return false;
  while (*p == ';')
    {
    bool enter;
    enum LocationAction action;
    p += strspn(++p, " ");
    if (strncasecmp(p, "enter:", 6) == 0)
      enter = true;
    else if (strncasecmp(p, "leave:", 6) == 0)
      enter = false;
    else
      return false;
    p += 6;
    const char* act = p;
    int alen = strcspn(act, ":");
    p += alen;
    if (*p++ != ':')
      return false;
    len = strcspn(p, ";");
    const char* msg;
    action = validate_action(enter, act, alen, p, len, &msg);
    if (action == INVALID)
      {
      if (msg)
        ESP_LOGE(TAG, "%s", msg);
      return false;
      }
    m_actions.push_back(new OvmsLocationAction(enter, action, p, len));
    p += len;
    }
  return true;
  }

void OvmsLocation::Store(std::string& buf)
  {
  char val[32];
  snprintf(val,sizeof(val),"%0.6f,%0.6f,%d", m_latitude, m_longitude, m_radius);
  buf = val;
  for (ActionList::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
    {
    OvmsLocationAction* ola = *it;
    buf.append(1, ';');
    buf.append(ola->m_enter ? "enter" : "leave");
    buf.append(1, ':');
    buf.append(ola->ActionString());
    buf.append(1, ':');
    buf.append(ola->m_params);
    }
  MyConfig.SetParamValue(LOCATIONS_PARAM, m_name, buf.c_str());
  }

void OvmsLocation::Render(std::string& buf)
  {
  char val[32];
  snprintf(val, sizeof(val), "%0.6f,%0.6f (%dm)", m_latitude, m_longitude, m_radius);
  buf = val;
  bool first = true;
  for (ActionList::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
    {
    OvmsLocationAction* ola = *it;
    if (!ola->m_enter)
      continue;
    if (first)
      {
      buf.append("\n  Enter Actions: ");
      first = false;
      }
    else
      buf.append("; ");
    buf.append(ola->ActionString());
    buf.append(1, ' ');
    buf.append(ola->m_params);
    }
  first = true;
  for (ActionList::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
    {
    OvmsLocationAction* ola = *it;
    if (ola->m_enter)
      continue;
    if (first)
      {
      buf.append("\n  Leave Actions: ");
      first = false;
      }
    else
      buf.append("; ");
    buf.append(ola->ActionString());
    buf.append(1, ' ');
    buf.append(ola->m_params);
    }
  }

void location_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsConfigParam* p = MyConfig.CachedParam(LOCATIONS_PARAM);
  if (p == NULL) return;

  // List all the locations that have been parsed as valid
  for (LocationMap::iterator it=MyLocations.m_locations.begin(); it!=MyLocations.m_locations.end(); ++it)
    {
    const std::string& name = it->first;
    OvmsLocation* loc = it->second;
    std::string value;
    loc->Render(value);
    writer->printf("%s%s: %s\n", loc->m_inlocation ? "*" : "", name.c_str(), value.c_str());
    }

  // List any entries in the location config that are not valid
  for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
    {
    const std::string& name = it->first;
    const std::string& value = it->second;
    auto k = MyLocations.m_locations.find(name);
    if (k == MyLocations.m_locations.end())
      {
      writer->printf("Location %s is invalid: %s\n", name.c_str(), value.c_str());
      }
    }
  }

void location_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char *name = argv[0];
  float latitude, longitude;
  int radius = LOCATION_DEFRADIUS;

  if (argc >= 3)
    {
    latitude = atof(argv[1]);
    longitude = atof(argv[2]);
    }
  else
    {
    latitude = MyLocations.m_latitude;
    longitude = MyLocations.m_longitude;
    }

  if (argc > 3) radius = atoi(argv[3]);

  char val[32];
  snprintf(val,sizeof(val),"%0.6f,%0.6f,%d",latitude,longitude,radius);
  MyConfig.SetParamValue(LOCATIONS_PARAM,name,val);
  writer->puts("Location defined");
  }

void location_radius(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char *name = argv[0];
  OvmsLocation* loc = MyLocations.Find(name);

  if (loc == NULL)
    {
    writer->printf("Error: No location %s defined\n",name);
    return;
    }

  std::string buf;
  loc->m_radius = atoi(argv[1]);
  loc->Store(buf);
  writer->puts("Location radius set");
  }

void location_rm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.DeleteInstance(LOCATIONS_PARAM,argv[0]);
  writer->puts("Location removed");
  }

void location_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Currently at %0.6f,%0.6f ",MyLocations.m_latitude,MyLocations.m_longitude);
  if (MyLocations.m_gpslock)
    writer->puts("(with good GPS lock)");
  else
    writer->puts("(without GPS lock)");

  if ((MyLocations.m_park_latitude != 0)&&(MyLocations.m_park_longitude != 0))
    writer->printf("Vehicle is parked at %0.6f,%0.6f\n",
      MyLocations.m_park_latitude,
      MyLocations.m_park_longitude);

  writer->printf("There are %d location(s) defined\n",MyLocations.m_locations.size());

  bool found = false;
  for (LocationMap::iterator it=MyLocations.m_locations.begin(); it!=MyLocations.m_locations.end(); ++it)
    {
    if (it->second->m_inlocation)
      {
      if (!found) writer->printf("Active locations:");
      writer->printf(" %s",it->second->m_name.c_str());
      found = true;
      }
    }
  if (found)
    writer->puts("");
  else
    writer->puts("No active locations");
  }

void location_action(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool enter = *(cmd->GetName()) == 'e';
  const char *name = argv[0];
  OvmsLocation* loc = MyLocations.Find(name);
  if (loc == NULL)
    {
    writer->printf("Error: No location %s defined\n",name);
    return;
    }
  const char *action = argv[1];
  std::string params;
  for (int i = 2; i < argc; ++i)
    {
    params.append(argv[i]);
    if (i < argc-1)
      params.append(1, ' ');
    }
  const char* p = params.c_str();
  const char* msg;
  enum LocationAction act = validate_action(enter, action, strlen(action), p, strlen(p), &msg);
  if (act == INVALID)
    {
    if (msg)
      writer->printf("Error: %s\n", msg);
    return;
    }
  loc->m_actions.push_back(new OvmsLocationAction(enter, act, p, params.length()));
  loc->Store(params);
  writer->puts("Location action set");
  }

void location_rm_action(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool enter = *(cmd->GetName()) == 'e';
  const char *name = argv[0];
  OvmsLocation* loc = MyLocations.Find(name);
  if (loc == NULL)
    {
    writer->printf("Error: No location %s defined\n",name);
    return;
    }
  const char *action = argc > 1 ? argv[1] : NULL;
  std::string params;
  for (int i = 2; i < argc; ++i)
    {
    params.append(argv[i]);
    if (i < argc-1)
      params.append(1, ' ');
    }
  int removed = 0;
  for (ActionList::iterator it = loc->m_actions.begin(); it != loc->m_actions.end(); ++it)
    {
    OvmsLocationAction* ola = *it;
    if (ola->m_enter != enter)
      continue;
    if (action)
      {
      if (strcmp(ola->ActionString(), action) != 0)
        continue;
      if (!params.empty() && ola->m_params != params)
        continue;
      }
    ++removed;
    it = loc->m_actions.erase(it);
    if (it == loc->m_actions.end())
      break;
    }
  if (!removed)
    {
    writer->printf("Error: No matching action found in location %s\n",name);
    return;
    }
  loc->Store(params);
  writer->printf("%d action%s removed from location %s\n", removed, removed > 1 ? "s" : "", name);
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

static duk_ret_t DukOvmsLocationStatus(duk_context *ctx)
  {
  const char *mn = duk_to_string(ctx,0);
  OvmsLocation *loc = MyLocations.Find(mn);
  if (loc)
    {
    duk_push_boolean(ctx, loc->m_inlocation);
    return 1;  /* one return value */
    }
  else
    return 0;
  }

#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

OvmsLocations MyLocations __attribute__ ((init_priority (1900)));

OvmsLocations::OvmsLocations()
  {
  ESP_LOGI(TAG, "Initialising LOCATIONS (1900)");

  m_gpslock = false;
  m_latitude = 0;
  m_longitude = 0;
  m_park_latitude = 0;
  m_park_longitude = 0;

  // Register our commands
  OvmsCommand* cmd_location = MyCommandApp.RegisterCommand("location","LOCATION framework",NULL, "", 0, 0, true);
  cmd_location->RegisterCommand("list","Show all locations",location_list, "", 0, 0, true);
  cmd_location->RegisterCommand("set","Set the position of a location",location_set, "<name> [<latitude> <longitude> [<radius>]]", 1, 4, true);
  cmd_location->RegisterCommand("radius","Set the radius of a location",location_radius, "<name> <radius>", 2, 2, true);
  cmd_location->RegisterCommand("rm","Remove a defined location",location_rm, "<name>", 1, 1, true);
  cmd_location->RegisterCommand("status","Show location status",location_status, "", 0, 0, true);
  OvmsCommand* cmd_action = cmd_location->RegisterCommand("action","Set an action for a location",NULL, "", 0, 0, true);
  cmd_action->RegisterCommand("enter","Set an action upon entering a location",location_action, "<name> <action> [<parameters>]", 2, INT_MAX, true);
  cmd_action->RegisterCommand("leave","Set an action upon leaving a location",location_action, "<name> <action> [<parameters>]", 2, INT_MAX, true);
  OvmsCommand* cmd_rm_action = cmd_action->RegisterCommand("rm","Remove a location action",NULL, "", 0, 0, true);
  cmd_rm_action->RegisterCommand("enter","Remove an action from entering a location",location_rm_action, "<name> [<action>] [<parameters>]", 1, INT_MAX, true);
  cmd_rm_action->RegisterCommand("leave","Remove an action from leaving a location",location_rm_action, "<name> [<action>] [<parameters>]", 1, INT_MAX, true);

  // Register our parameters
  MyConfig.RegisterParam(LOCATIONS_PARAM, "Geo Locations", true, true);

  // Register our callbacks
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, MS_V_POS_GPSLOCK, std::bind(&OvmsLocations::UpdatedGpsLock, this, _1));
  MyMetrics.RegisterListener(TAG, MS_V_POS_LATITUDE, std::bind(&OvmsLocations::UpdatedLatitude, this, _1));
  MyMetrics.RegisterListener(TAG, MS_V_POS_LONGITUDE, std::bind(&OvmsLocations::UpdatedLongitude, this, _1));
  MyMetrics.RegisterListener(TAG, MS_V_ENV_ON, std::bind(&OvmsLocations::UpdatedVehicleOn, this, _1));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsLocations::UpdatedConfig, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsLocations::UpdatedConfig, this, _1, _2));

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  ESP_LOGI(TAG, "Expanding DUKTAPE javascript engine");
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsLocation");
  dto->RegisterDuktapeFunction(DukOvmsLocationStatus, 1, "Status");
  MyScripts.RegisterDuktapeObject(dto);
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsLocations::~OvmsLocations()
  {
  MyMetrics.DeregisterListener(TAG);
  }

void OvmsLocations::UpdatedGpsLock(OvmsMetric* metric)
  {
  OvmsMetricBool* m = (OvmsMetricBool*)metric;
  m_gpslock = m->AsBool();
  if (m_gpslock)
    {
    MyEvents.SignalEvent("gps.lock.acquired", NULL);
    UpdateLocations();
    }
  else
    {
    MyEvents.SignalEvent("gps.lock.lost", NULL);
    }
  }

void OvmsLocations::UpdatedLatitude(OvmsMetric* metric)
  {
  OvmsMetricFloat* m = (OvmsMetricFloat*)metric;
  m_latitude = m->AsFloat();
  if (m_gpslock)
    {
    UpdateLocations();
    CheckTheft();
    }
  }

void OvmsLocations::UpdatedLongitude(OvmsMetric* metric)
  {
  OvmsMetricFloat* m = (OvmsMetricFloat*)metric;
  m_longitude = m->AsFloat();
  if (m_gpslock)
    {
    UpdateLocations();
    CheckTheft();
    }
  }

void OvmsLocations::UpdatedVehicleOn(OvmsMetric* metric)
  {
  OvmsMetricBool* m = (OvmsMetricBool*)metric;
  bool caron = m->AsBool();
  if (caron)
    {
    m_park_latitude = 0;
    m_park_longitude = 0;
    }
  else
    {
    m_park_latitude = m_latitude;
    m_park_longitude = m_longitude;
    }
  }

void OvmsLocations::ReloadMap()
  {
  OvmsConfigParam* p = MyConfig.CachedParam(LOCATIONS_PARAM);
  if (p == NULL) return;

  // Forward search, updating existing locations
  for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
    {
    const std::string& name = it->first;
    const std::string& value = it->second;
    OvmsLocation* loc;
    auto k = m_locations.find(name);
    if (k == m_locations.end())
      {
      // Create it
      loc = new OvmsLocation(name);
      m_locations[name] = loc;
      }
    else
      loc = k->second;
    // Parse the parameters
    if (!loc->Parse(value))
      {
      ESP_LOGE(TAG, "Location %s is invalid: %s", name.c_str(), value.c_str());
      delete loc;
      m_locations.erase(name);
      }
    else
      {
      // ESP_LOGI(TAG, "Location %s is at %f,%f (%d)", name.c_str(), loc->m_latitude, loc->m_longitude, loc->m_radius);
      }
    }

  // Reverse search, go through existing locations looking for those to delete
  for (LocationMap::iterator it=m_locations.begin(); it!=m_locations.end(); ++it)
    {
    auto k = p->m_map.find(it->first);
    if (k == p->m_map.end())
      {
      // Location no longer exists
      // ESP_LOGI(TAG, "Location %s is removed",it->first.c_str());
      delete it->second;
      m_locations.erase(it);
      }
    }

  if (m_gpslock) UpdateLocations();
  }

void OvmsLocations::UpdateLocations()
  {
  if ((m_latitude == 0)||(m_longitude == 0)) return;

  for (LocationMap::iterator it=m_locations.begin(); it!=m_locations.end(); ++it)
    {
    it->second->IsInLocation(m_latitude,m_longitude);
    }
  }

void OvmsLocations::CheckTheft()
  {
  if (m_park_latitude == 0) return;
  if (m_park_longitude == 0) return;
  if (StandardMetrics.ms_v_env_on->AsBool()) return;
  int alarm = MyConfig.GetParamValueInt("vehicle", "flatbed.alarmdistance", 500);
  if (alarm == 0) return;

  double dist = fabs(OvmsLocationDistance((double)m_latitude,(double)m_longitude,(double)m_park_latitude,(double)m_park_longitude));
  if (dist > alarm)
    {
    m_park_latitude = 0;
    m_park_longitude = 0;
    MyNotify.NotifyString("alert", "flatbed.moved", "Vehicle is being transported while parked - possible theft/flatbed");
    }
  }

OvmsLocation* OvmsLocations::Find(std::string name)
  {
  auto k = m_locations.find(name);
  if (k == m_locations.end())
    return NULL;
  else
    return k->second;
  }

void OvmsLocations::UpdatedConfig(std::string event, void* data)
  {
  if (event.compare("config.changed")==0)
    {
    // Only reload if our parameter has changed
    OvmsConfigParam*p = (OvmsConfigParam*)data;
    if (p->GetName().compare(LOCATIONS_PARAM)!=0) return;
    }

  ReloadMap();
  }
