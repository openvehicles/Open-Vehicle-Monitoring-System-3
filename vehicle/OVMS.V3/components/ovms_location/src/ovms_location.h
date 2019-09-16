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

#ifndef __LOCATION_H__
#define __LOCATION_H__

#include "ovms_metrics.h"
#include "ovms_utils.h"
#include "ovms_command.h"

enum LocationAction {
  INVALID = 0,
  HOMELINK,
  ACC,
  NOTIFY
  };

class OvmsLocationAction
  {
  public:
    OvmsLocationAction(bool enter, enum LocationAction action, const char* params, int len);
    const char* ActionString();

  public:
    bool m_enter;
    enum LocationAction m_action;
    string m_params;
  };

class ActionList : public std::list<OvmsLocationAction*>
  {
  public:
    ~ActionList();
    void clear();
    iterator erase(iterator pos);
  };

class OvmsLocation
  {
  public:
    OvmsLocation(const std::string& name);
    ~OvmsLocation();

  public:
    bool IsInLocation(float latitude, float longitude);
    bool Parse(const std::string& value);
    void Store(std::string& buf);
    void Render(std::string& buf);

  public:
    std::string m_name;
    std::string m_value;
    float m_latitude;
    float m_longitude;
    int m_radius;
    bool m_inlocation;
    ActionList m_actions;
  };

typedef NameMap<OvmsLocation*> LocationMap;

class OvmsLocations
  {
  public:
    OvmsLocations();
    ~OvmsLocations();

  public:
    bool m_gpslock;
    float m_latitude;
    float m_longitude;
    float m_park_latitude;
    float m_park_longitude;
    LocationMap m_locations;

  public:
    void ReloadMap();
    void UpdateLocations();
    void CheckTheft();

  public:
    void UpdatedGpsLock(OvmsMetric* metric);
    void UpdatedLatitude(OvmsMetric* metric);
    void UpdatedLongitude(OvmsMetric* metric);
    void UpdatedVehicleOn(OvmsMetric* metric);
    void UpdatedConfig(std::string event, void* data);
  };

extern OvmsLocations MyLocations;

#endif //#ifndef __LOCATION_H__
