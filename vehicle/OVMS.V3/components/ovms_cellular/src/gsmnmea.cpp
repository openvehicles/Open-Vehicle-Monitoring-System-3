/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th December 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;    (C) 2012-2017  Michael Balzer
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
static const char *TAG = "gsm-nmea";

#include <string>
#include <sstream>

#include "gsmnmea.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_time.h"

#define JDEpoch 2440588 // Julian date of the Unix epoch
#define DIM(a) (sizeof(a)/sizeof(*(a)))


/**
 * gps2latlon: convert NMEA degree/minute form to degrees
 */
static float gps2latlon(const char *gpscoord)
{
  double f;
  long d;

  f = atof(gpscoord);
  d = (long) (f / 100); // extract degrees
  f = d + (f - (d * 100)) / 60; // convert to decimal format

  return (float) f;
}


/**
 * JdFromYMD:
 *  computes the Julian date from year, month, day
 *  http://aa.usno.navy.mil/faq/docs/JD_Formula.php
 *  only valid for the years 1801-2099 (because 1800 and 2100 are not leap years)
 */
static unsigned long JdFromYMD(int year, int month, int day)
{
  return day-32075+1461L*(year+4800+(month-14)/12)/4+367*(month-2-(month-14)/12*12)/12-3*((year+4900+(month-14)/12)/100)/4;
}


/**
 * utc_to_timestamp:
 *  convert GPS date & time (UTC) to timestamp
 *   date: "ddmmyy"
 *   time: "hhmmss"
 */
static int64_t utc_to_timestamp(const char* date, const char* time)
  {
  int day, month, year, hour, minute, second;

  day = (date[0]-'0')*10 + (date[1]-'0');
  month = (date[2]-'0')*10 + (date[3]-'0');
  year = (date[4]-'0')*10 + (date[5]-'0');

  hour = (time[0]-'0')*10 + (time[1]-'0');
  minute = (time[2]-'0')*10 + (time[3]-'0');
  second = (time[4]-'0')*10 + (time[5]-'0');

  int64_t jd = JdFromYMD(2000+year, month, day);
  return (jd - JDEpoch) * (24L * 3600)
      + ((hour * 60L + minute) * 60) + second;
  }


static bool valid_nmea_cksum(const std::string line)
  {
  const char *cp = line.c_str();
  if (*cp != '$')
    return 0;
  ++cp;
  const char *ep = strchr(cp, '*');
  if (ep == NULL)
    return 0;
  unsigned char chk = 0;
  while (cp < ep)
    chk ^= (const unsigned char)*cp++;
  unsigned char chk2 = (unsigned char)strtoul(ep + 1, NULL, 16);
  return (chk == chk2);
  }


void GsmNMEA::IncomingLine(const std::string line)
  {
  std::istringstream sentence(line);
  std::string token;

  if ( line.find("+CGNSSINFO:",0) == 0 )   // CGNSSINFO record
    {
    // check for +CGNSSINFO output record
    // no fix yet: +CGNSSINFO: ,,,,,,,,
    // GPS fix:    +CGNSSINFO: 3,15,,01,01,52.3973694,N,7.1607476,E,040125,091435.00,121.7,2.072,161.56,4.11,2.21,3.46,05
    // Info
    // 3,         : 3|2  3D fix|2D fix
    // 15,,01,01  : number of used sats of 4 GPS sytems (documentation of 76XX states only 3 fields!)
    // 52.3973694,: latitude  degree
    // N,         : N|S
    // 7.1607476, : longitude degree
    // E,         : E|W
    // 040125,    : date DDMMYY
    // 091435.00, : UTC time hhmmss.ss 
    // 121.7,     : altitude in meter
    // 2.072,     : speed in knots
    // 161.56,    : heading in degree
    // 4.11,      : PDOP
    // 2.21,      : HDOP
    // 3.46,      : VDOP 
    // 05 : not mentioned in the documentation. Checksum?
    ESP_LOGV(TAG, "Incoming location data: %s", line.c_str());
    if (!std::getline(sentence, token, ' ')) return;  // skip the CGNSSINFO header
    if (!std::getline(sentence, token, ',')) return;
    float lat=0, lon=0, alt=0, hdop=0, vdop=0, pdop=0;
    char  ns='N', ew='E';
    bool  gpslock = false, speedok=false, directionok=false;
    int   satcnt=0;
    char  date[7] = {0}, timestr[7] = {0};
    float direction=0, speed=0;

    if(line.length() > 25 )
      { 
      gpslock = (token == "2" || token == "3");

      // Parse sentence:

      // no of sats
      if (std::getline(sentence, token, ','))
        satcnt = atoi(token.c_str());
        if (std::getline(sentence, token, ','))  satcnt += atoi(token.c_str());
        if (std::getline(sentence, token, ','))  satcnt += atoi(token.c_str());
        if (std::getline(sentence, token, ','))  satcnt += atoi(token.c_str());

      if (std::getline(sentence, token, ','))
        lat = atof(token.c_str());
      if (std::getline(sentence, token, ','))
        ns = token[0];
      if (std::getline(sentence, token, ','))
        lon = atof(token.c_str());
      if (std::getline(sentence, token, ','))
        ew = token[0];
      if (std::getline(sentence, token, ','))
        strncpy(date, token.c_str(), 6);
      if (std::getline(sentence, token, ','))
        strncpy(timestr, token.c_str(), 6);
      if (std::getline(sentence, token, ','))
        alt = atof(token.c_str());
      if (std::getline(sentence, token, ','))
        { 
        speedok = !token.empty();
        if (speedok) speed = atof(token.c_str()) * 1.852;
        }
      if (std::getline(sentence, token, ',')) 
        {
        directionok = !token.empty();
        if (directionok) direction = atof(token.c_str());
        }
      if (std::getline(sentence, token, ','))
        pdop = atof(token.c_str());
      if (std::getline(sentence, token, ','))
        hdop = atof(token.c_str());
      if (std::getline(sentence, token, ','))
        vdop = atof(token.c_str());
      }
    // Check:

    if ( !ns || !ew || !gpslock || strlen(date) != 6 || strlen(timestr) !=6 )
      return; // malformed/empty sentence

    // Data set complete, store:

    if (ns == 'S')
      lat = -lat;
    if (ew == 'W')
      lon = -lon;

    *StdMetrics.ms_v_pos_gpsmode = std::string(gpslock ? "AA" : "NN");
    *StdMetrics.ms_v_pos_satcount = (int) satcnt;
    *StdMetrics.ms_v_pos_gpshdop = (float) hdop;
    
    // Derive signal quality from lock status, satellite count and HDOP:
    //  quality ~ satcnt / hdop
    *StdMetrics.ms_v_pos_gpssq = (int) LIMIT_MAX(gpslock * LIMIT_MIN(satcnt-1,0) / LIMIT_MIN(hdop,0.1) * 10, 100);
    // Quality raises by satellite count and drops by HDOP. HDOP 1.0 = perfect.
    // The calculation is designed to get 50% as the threshold for a "good" signal.
    // GPS needs at least 4 satellites in view to get a position, but that will need
    // HDOP << 1 to be considered reliable. 6 satellites are on the edge to "good"
    // (with HDOP=1). Samples:
    //   4 satellites / HDOP 1.0 → SQ 30%
    //   4 satellites / HDOP 0.6 → SQ 50%
    //   6 satellites / HDOP 1.0 → SQ 50%
    //   9 satellites / HDOP 1.0 → SQ 80%
    //  10 satellites / HDOP 0.9 → SQ 100%
    //  10 satellites / HDOP 1.8 → SQ 50%

    *StdMetrics.ms_v_pos_latitude = (float) lat;
    *StdMetrics.ms_v_pos_longitude = (float) lon;
    *StdMetrics.ms_v_pos_altitude = (float) alt;
    auto tm = utc_to_timestamp(date, timestr);
    if ( abs(time(NULL)- tm - 1024*7*86400) < 60 ) tm += 1024*7*86400;  // check/correct for 10bit week rollover of older GPS devices
    StdMetrics.ms_v_pos_gpstime->SetValue(tm);

    // upodate gpslock last, so listeners will see updated lat/lon values:
    if (gpslock != StdMetrics.ms_v_pos_gpslock->AsBool())
      {
      *StdMetrics.ms_v_pos_gpslock = (bool) gpslock;
      if (gpslock)
        MyEvents.SignalEvent("system.modem.gotgps", NULL);
      else
        MyEvents.SignalEvent("system.modem.lostgps", NULL);
      }

    if (m_gpstime_enabled)
      {
        MyTime.Set(TAG, 2, true, tm);
      }

    if (directionok)
      *StdMetrics.ms_v_pos_direction = (float) direction;

    if (speedok)
      *StdMetrics.ms_v_pos_gpsspeed = speed;

    ESP_LOGV(TAG, "CGNSSINFO: %s %s nsats %d lat %f lon %f hdop %f vdop %f pdop %f", date, timestr, satcnt, lat, lon, hdop, vdop, pdop);
    
    } // END "+CGNSSINFO" handler
  
else  // $GNS record
 {
  if (!std::getline(sentence, token, ','))
    return;
  if (token.length() < 6 || token[0] != '$')
    return;
  if (!valid_nmea_cksum(line))
    {
    ESP_LOGE(TAG, "IncomingLine: bad checksum: %s", line.c_str());
    return;
    }

  if (token.substr(3) == "GNS")
    {
    ESP_LOGV(TAG, "Incoming GNS: %s", line.c_str());
    // NMEA sentence type "GNS": GNSS Position Fix Data (GPS/GLONASS/… combined position data)
    //  $..GNS,<Time>,<Latitude>,<NS>,<Longitude>,<EW>,<Mode>,<SatCnt>,<HDOP>,<Altitude>,<GeoidalSep>,<DiffAge>,<Chksum>
    // Example:
    //  $GNGNS,085320.0,5118.138139,N,00723.398844,E,AA,12,0.9,321.3,47.0,,*6E
    // Notes:
    //  <Mode>: first char = GPS, second = GLONASS;
    //    N = No fix
    //    A = Autonomous mode (non differential)
    //    D = Differential mode
    //    E = Estimation mode

    float lat=0, lon=0, alt=0, hdop=0;
    char ns=0, ew=0;
    char mode[3] = {0,0,0};
    int satcnt=0;

    // Parse sentence:

    if (std::getline(sentence, token, ','))
      {;} // Time ignored here, see RMC handler
    if (std::getline(sentence, token, ','))
      lat = gps2latlon(token.c_str());
    if (std::getline(sentence, token, ','))
      ns = token[0];
    if (std::getline(sentence, token, ','))
      lon = gps2latlon(token.c_str());
    if (std::getline(sentence, token, ','))
      ew = token[0];
    if (std::getline(sentence, token, ','))
      {
      mode[0] = token[0];
      mode[1] = token[1];
      }
    if (std::getline(sentence, token, ','))
      satcnt = atoi(token.c_str());
    if (std::getline(sentence, token, ','))
      hdop = atof(token.c_str());
    if (std::getline(sentence, token, ','))
      alt = atof(token.c_str());
    
    
    // Check:

    if (!ns || !ew || !mode[0])
      return; // malformed/empty sentence

    // Data set complete, store:

    if (ns == 'S')
      lat = -lat;
    if (ew == 'W')
      lon = -lon;

    bool gpslock = (mode[0] != 'N' || mode[1] != 'N');

    *StdMetrics.ms_v_pos_gpsmode = (std::string) mode;
    *StdMetrics.ms_v_pos_satcount = (int) satcnt;
    *StdMetrics.ms_v_pos_gpshdop = (float) hdop;
    
    // Derive signal quality from lock status, satellite count and HDOP:
    //  quality ~ satcnt / hdop
    *StdMetrics.ms_v_pos_gpssq = (int) LIMIT_MAX(gpslock * LIMIT_MIN(satcnt-1,0) / LIMIT_MIN(hdop,0.1) * 10, 100);
    // Quality raises by satellite count and drops by HDOP. HDOP 1.0 = perfect.
    // The calculation is designed to get 50% as the threshold for a "good" signal.
    // GPS needs at least 4 satellites in view to get a position, but that will need
    // HDOP << 1 to be considered reliable. 6 satellites are on the edge to "good"
    // (with HDOP=1). Samples:
    //   4 satellites / HDOP 1.0 → SQ 30%
    //   4 satellites / HDOP 0.6 → SQ 50%
    //   6 satellites / HDOP 1.0 → SQ 50%
    //   9 satellites / HDOP 1.0 → SQ 80%
    //  10 satellites / HDOP 0.9 → SQ 100%
    //  10 satellites / HDOP 1.8 → SQ 50%

    if (gpslock)
      {
      *StdMetrics.ms_v_pos_latitude = (float) lat;
      *StdMetrics.ms_v_pos_longitude = (float) lon;
      *StdMetrics.ms_v_pos_altitude = (float) alt;
      *StdMetrics.ms_v_pos_gpstime = time(NULL);
      }

    // upodate gpslock last, so listeners will see updated lat/lon values:
    if (gpslock != StdMetrics.ms_v_pos_gpslock->AsBool())
      {
      *StdMetrics.ms_v_pos_gpslock = (bool) gpslock;
      if (gpslock)
        MyEvents.SignalEvent("system.modem.gotgps", NULL);
      else
        MyEvents.SignalEvent("system.modem.lostgps", NULL);
      }

    // END "GNS" handler
    }

  else if (token.substr(3) == "RMC")
    {
    ESP_LOGV(TAG, "Incoming RMC: %s", line.c_str());
    // NMEA sentence type "RMC": Recommended Minimum Specific GNSS Data
    //  $..RMC,<Time>,<Status>,<Latitude>,<NS>,<Longitude>,<EW>,<SpeedKnots>,<Direction>,<Date>,<MagVar>,<MagVarEW>,<Mode>,<Chksum>
    // Example:
    //  $GPRMC,085320.0,A,5118.138139,N,00723.398844,E,0.0,265.5,101217,,,A*62

    char date[6] = {0}, time[6] = {0};
    float direction=0, speed=0;

    // Parse sentence:

    if (std::getline(sentence, token, ','))
      strncpy(time, token.c_str(), 6);
    bool speedok = false, directionok = false;
    if (std::getline(sentence, token, ','))
      {;} // Status
    if (std::getline(sentence, token, ','))
      {;} // Latitude
    if (std::getline(sentence, token, ','))
      {;} // NS
    if (std::getline(sentence, token, ','))
      {;} // Longitude
    if (std::getline(sentence, token, ','))
      {;} // EW
    if (std::getline(sentence, token, ','))
      {
        speedok = !token.empty();
        if (speedok)
          speed = atof(token.c_str()) * 1.852;
      }
    if (std::getline(sentence, token, ','))
      {
      directionok = !token.empty();
      if (directionok)
        direction = atof(token.c_str());
      }
    if (std::getline(sentence, token, ','))
      strncpy(date, token.c_str(), 6);

    // Check:

    if (!date[0] || !time[0])
      return; // malformed/empty sentence

    // Data complete, store:

    if (m_gpstime_enabled)
      {
      auto tm = utc_to_timestamp(date, time);
      if (tm < 1572735600) // 2019-11-03 00:00:00
        tm += (1024*7*86400); // Nasty kludge to workaround SIM5360 week rollover
      MyTime.Set(TAG, 2, true, tm);
      }

    if (directionok)
      *StdMetrics.ms_v_pos_direction = (float) direction;

    if (speedok)
      *StdMetrics.ms_v_pos_gpsspeed = speed;

    // END "RMC" handler
    }
  }

}


void GsmNMEA::Startup()
  {
  ESP_LOGI(TAG, "Startup");

  m_gpstime_enabled = MyConfig.GetParamValueBool("modem", "enable.gpstime", false);
  m_connected = true;
  }


void GsmNMEA::Shutdown(bool hard)
  {
  ESP_LOGI(TAG, "Shutdown (direct)");

  *StdMetrics.ms_v_pos_gpsmode = (std::string) "";
  *StdMetrics.ms_v_pos_gpsspeed = (float) 0;
  *StdMetrics.ms_v_pos_satcount = (int) 0;
  *StdMetrics.ms_v_pos_gpshdop = (float) 500;
  *StdMetrics.ms_v_pos_gpssq = (int) 0;

  if (StdMetrics.ms_v_pos_gpslock->AsBool())
    {
    *StdMetrics.ms_v_pos_gpslock = (bool) false;
    MyEvents.SignalEvent("system.modem.lostgps", NULL);
    }

  m_connected = false;
  }


GsmNMEA::GsmNMEA(GsmMux* mux, int channel)
  {
  m_mux = mux;
  m_channel = channel;
  m_connected = false;
  m_gpstime_enabled = false;
  }

GsmNMEA::~GsmNMEA()
  {
  if (m_connected)
    {
    Shutdown();
    }
  }
