/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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

#ifndef __VEHICLE_NISSANLEAF_H__
#define __VEHICLE_NISSANLEAF_H__

#include "freertos/timers.h"
#include "vehicle.h"

#define DEFAULT_MODEL_YEAR 2012
#define GEN_1_NEW_CAR_GIDS 281
#define GEN_1_NEW_CAR_AH 66
#define GEN_1_KM_PER_KWH 7.1
#define GEN_1_WH_PER_GID 80
#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define ACTIVATION_REQUEST_TIME 10 // tenths of a second to hold activation request signal

using namespace std;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  START_CHARGING,
  AUTO_DISABLE_CLIMATE_CONTROL,
  UNLOCK_DOORS,
  LOCK_DOORS
  } RemoteCommand;

typedef enum
  {
  CAN_READ_ONLY,
  INVALID_SYNTAX,
  COMMAND_RESULT_OK
  } CommandResult;

class OvmsVehicleNissanLeaf : public OvmsVehicle
  {
  public:
    OvmsVehicleNissanLeaf();
    ~OvmsVehicleNissanLeaf();

  public:
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    vehicle_command_t CommandHomelink(int button, int durationms=1000);
    vehicle_command_t CommandClimateControl(bool enable);
    vehicle_command_t CommandLock(const char* pin);
    vehicle_command_t CommandUnlock(const char* pin);
    void RemoteCommandTimer();
    void CcDisableTimer();

  private:
    void SendCanMessage(uint16_t id, uint8_t length, uint8_t *data);
    void Ticker1(uint32_t ticker);
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
    OvmsVehicle::vehicle_command_t CommandStartCharge();

    RemoteCommand nl_remote_command; // command to send, see RemoteCommandTimer()
    uint8_t nl_remote_command_ticker; // number remaining remote command frames to send
    void PollReply_Battery(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_VIN(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_Volt(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_Temp(uint8_t reply_data[], uint16_t reply_len);

    TimerHandle_t m_remoteCommandTimer;
    TimerHandle_t m_ccDisableTimer;
    metric_unit_t m_odometer_units = Other;
    OvmsMetricInt *m_gids;
    OvmsMetricFloat *m_hx;
    OvmsMetricFloat *m_soc_new_car;
    OvmsMetricFloat *m_soc_instrument;
    OvmsMetricVector<int> *m_bms_thermistor;
    OvmsMetricVector<int> *m_bms_temp_int;
    OvmsMetricFloat *m_soh_new_car;
    OvmsMetricInt *m_soh_instrument;
  };

#endif //#ifndef __VEHICLE_NISSANLEAF_H__
