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

class OvmsLocation
  {
  public:
    OvmsLocation(std::string name, float latitude, float longitude, int radius);
    ~OvmsLocation();

  public:
    bool IsInLocation(float latitude, float longitude);
    void Update(float latitude, float longitude, int radius);

  public:
    std::string m_name;
    float m_latitude;
    float m_longitude;
    int m_radius;
    bool m_inlocation;
  };

typedef std::map<std::string, OvmsLocation*> LocationMap;

class OvmsLocations
  {
  public:
    OvmsLocations();
    ~OvmsLocations();

  public:
    bool m_gpslock;
    float m_latitude;
    float m_longitude;
    LocationMap m_locations;

  public:
    void ReloadMap();
    void UpdateLocations();

  public:
    void UpdatedGpsLock(OvmsMetric* metric);
    void UpdatedLatitude(OvmsMetric* metric);
    void UpdatedLongitude(OvmsMetric* metric);
    void UpdatedConfig(std::string event, void* data);
  };

extern OvmsLocations MyLocations;

#endif //#ifndef __LOCATION_H__
