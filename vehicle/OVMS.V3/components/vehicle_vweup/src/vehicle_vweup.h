/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris van der Meijden
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

#ifndef __VEHICLE_VWEUP_H__
#define __VEHICLE_VWEUP_H__

#include "freertos/timers.h"
#include "vehicle.h"
#include "ovms_webserver.h"

#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define DEFAULT_MODEL_YEAR 2020

using namespace std;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  AUTO_DISABLE_CLIMATE_CONTROL
  } RemoteCommand;

class OvmsVehicleVWeUP : public OvmsVehicle
  {
  public:
    OvmsVehicleVWeUP();
    ~OvmsVehicleVWeUP();

  public:
    void ConfigChanged(OvmsConfigParam* param);
    bool SetFeature(int key, const char* value);
    const std::string GetFeature(int key);

  public:
    void IncomingFrameCan3(CAN_frame_t* p_frame);

  protected:
    virtual void Ticker1(uint32_t ticker);
    char m_vin[18];

  public:
    vehicle_command_t CommandHomelink(int button, int durationms=1000);
    vehicle_command_t CommandClimateControl(bool enable);
    void RemoteCommandTimer();
    void CcDisableTimer();
    bool vin_part1;
    bool vin_part2;
    bool vin_part3;
    bool vwup_enable_write;
    int vwup_modelyear;

  private:
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);

    RemoteCommand vwup_remote_command; // command to send, see RemoteCommandTimer()
    uint8_t vwup_remote_command_ticker; // number remaining remote command frames to send

    TimerHandle_t m_remoteCommandTimer;
    TimerHandle_t m_ccDisableTimer;

    void vehicle_vweup_car_on(bool isOn);

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    static void WebCfgHardware(PageEntry_t& p, PageContext_t& c);

  };

#endif //#ifndef __VEHICLE_VWEUP_H__
