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

#include "esp_log.h"
static const char *TAG = "location";

#include "ovms_location.h"
#include "ovms_config.h"
#include "ovms_events.h"
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

OvmsLocation::OvmsLocation(std::string name, float latitude, float longitude, int radius)
  {
  m_name = name;
  m_latitude = latitude;
  m_longitude = longitude;
  m_radius = radius;
  m_inlocation = false;
  }

OvmsLocation::~OvmsLocation()
  {
  }

bool OvmsLocation::IsInLocation(float latitude, float longitude)
  {
  // This should check if we are in the location
  double dist = OvmsLocationDistance((double)latitude,(double)longitude,(double)m_latitude,(double)m_longitude);
  // ESP_LOGI(TAG, "Location %s is %0.1fm distant",m_name.c_str(),dist);

  if (abs(dist) <= m_radius)
    {
    // We are in the location
    if (!m_inlocation)
      {
      m_inlocation = true;
      MyEvents.SignalEvent("location.enter", (void*)m_name.c_str());
      }
    }
  else
    {
    // We are out of the location
    if (m_inlocation)
      {
      m_inlocation = false;
      MyEvents.SignalEvent("location.leave", (void*)m_name.c_str());
      }
    }

  // Return current status
  return m_inlocation;
  }

void OvmsLocation::Update(float latitude, float longitude, int radius)
  {
  m_latitude = latitude;
  m_longitude = longitude;
  m_radius = radius;
  }

void location_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsConfigParam* p = MyConfig.CachedParam(LOCATIONS_PARAM);
  if (p == NULL) return;

  // Forward search, updating existing locations
  for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
    {
    std::string name = it->first;
    const char *value = it->second.c_str();
    float latitude = atof(value);
    char *x = strchr(value,',');
    if (x != NULL)
      {
      float longitude = atof(x+1);
      x = strchr(x+1,',');
      int radius = LOCATION_DEFRADIUS;
      if (x != NULL) radius = atoi(x+1);
      char inlocation[2]; inlocation[0]=0; inlocation[1]=0;
      auto k = MyLocations.m_locations.find(name);
      if ((k != MyLocations.m_locations.end())&&(k->second->m_inlocation)) inlocation[0]='*';
      writer->printf("%s%s: %0.6f,%0.6f (%dm)\n",inlocation,name.c_str(),latitude,longitude,radius);
      }
    }
  }

void location_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char *name = argv[0];
  float latitude = atof(argv[1]);
  float longitude = atof(argv[2]);
  int radius = LOCATION_DEFRADIUS;
  if (argc > 3) radius = atoi(argv[3]);

  char val[32];
  snprintf(val,sizeof(val),"%0.6f,%0.6f,%d",latitude,longitude,radius);
  MyConfig.SetParamValue(LOCATIONS_PARAM,name,val);
  writer->puts("Location defined");
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

OvmsLocations MyLocations __attribute__ ((init_priority (1900)));

OvmsLocations::OvmsLocations()
  {
  ESP_LOGI(TAG, "Initialising LOCATIONS (1900)");

  m_gpslock = false;
  m_latitude = 0;
  m_longitude = 0;

  // Register our commands
  OvmsCommand* cmd_location = MyCommandApp.RegisterCommand("location","LOCATION framework",NULL, "", 1);
  cmd_location->RegisterCommand("list","Show all locations",location_list, "", 0, 0, true);
  cmd_location->RegisterCommand("set","Set the position of a location",location_set, "<name> <latitude> <longitude> [<radius>]", 3, 4, true);
  cmd_location->RegisterCommand("rm","Remove a defined location",location_rm, "<name>", 1, 1, true);
  cmd_location->RegisterCommand("status","Show location status",location_status, "", 0, 0, true);

  // Register our parameters
  MyConfig.RegisterParam(LOCATIONS_PARAM, "Geo Locations", true, true);

  // Register our callbacks
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, MS_V_POS_GPSLOCK, std::bind(&OvmsLocations::UpdatedGpsLock, this, _1));
  MyMetrics.RegisterListener(TAG, MS_V_POS_LATITUDE, std::bind(&OvmsLocations::UpdatedLatitude, this, _1));
  MyMetrics.RegisterListener(TAG, MS_V_POS_LONGITUDE, std::bind(&OvmsLocations::UpdatedLongitude, this, _1));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsLocations::UpdatedConfig, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsLocations::UpdatedConfig, this, _1, _2));
  }

OvmsLocations::~OvmsLocations()
  {
  MyMetrics.DeregisterListener(TAG);
  }

void OvmsLocations::UpdatedGpsLock(OvmsMetric* metric)
  {
  OvmsMetricBool* m = (OvmsMetricBool*)metric;
  m_gpslock = m->AsBool();
  if (m_gpslock) UpdateLocations();
  }

void OvmsLocations::UpdatedLatitude(OvmsMetric* metric)
  {
  OvmsMetricFloat* m = (OvmsMetricFloat*)metric;
  m_latitude = m->AsFloat();
  if (m_gpslock) UpdateLocations();
  }

void OvmsLocations::UpdatedLongitude(OvmsMetric* metric)
  {
  OvmsMetricFloat* m = (OvmsMetricFloat*)metric;
  m_longitude = m->AsFloat();
  if (m_gpslock) UpdateLocations();
  }

void OvmsLocations::ReloadMap()
  {
  OvmsConfigParam* p = MyConfig.CachedParam(LOCATIONS_PARAM);
  if (p == NULL) return;

  // Forward search, updating existing locations
  for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
    {
    std::string name = it->first;
    const char *value = it->second.c_str();
    float latitude = atof(value);
    char *x = strchr(value,',');
    if (x != NULL)
      {
      float longitude = atof(x+1);
      x = strchr(x+1,',');
      int radius = LOCATION_DEFRADIUS;
      if (x != NULL) radius = atoi(x+1);
      // ESP_LOGI(TAG, "Location %s is at %f,%f (%d)",name.c_str(),latitude,longitude,radius);
      auto k = m_locations.find(name);
      if (k == m_locations.end())
        {
        // Create it
        m_locations[name] = new OvmsLocation(name, latitude, longitude, radius);
        }
      else
        {
        // Update it
        k->second->Update(latitude, longitude, radius);
        }
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

