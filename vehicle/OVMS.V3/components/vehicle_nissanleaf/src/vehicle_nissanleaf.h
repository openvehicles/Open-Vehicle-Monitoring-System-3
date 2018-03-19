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
#define GEN_1_KM_PER_KWH 6.183
#define GEN_1_WH_PER_GID 77.8
#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define ACTIVATION_REQUEST_TIME 10 // tenths of a second to hold activation request signal
#define REMOTE_CC_TIME_GRID 1800 // seconds to run remote climate control on grid power
#define REMOTE_CC_TIME_BATTERY 900 // seconds to run remote climate control on battery power

using namespace std;

typedef enum
  {
  ZERO = 0,
  ONE = 1,
  TWO = 2,
  THREE = 3,
  FOUR = 4,
  FIVE = 5,
  IDLE = 6
  } PollState;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  START_CHARGING
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
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    vehicle_command_t CommandHomelink(int button);
    vehicle_command_t CommandClimateControl(bool enable);
    void RemoteCommandTimer();

  private:
    void PollStart(void);
    void PollContinue(CAN_frame_t* p_frame);
    void SendCanMessage(uint16_t id, uint8_t length, uint8_t *data);
    void Ticker1(uint32_t ticker);
    void Ticker60(uint32_t ticker);
    void SendCommand(RemoteCommand);
    OvmsVehicle::vehicle_command_t RemoteCommandHandler(RemoteCommand command);
    OvmsVehicle::vehicle_command_t CommandStartCharge();

    PollState nl_poll_state = IDLE;
    RemoteCommand nl_remote_command; // command to send, see ticker10th()
    uint8_t nl_remote_command_ticker; // number of tenths remaining to send remote command frames
    uint16_t nl_cc_off_ticker; // seconds before we send the climate control off command
    TimerHandle_t m_remoteCommandTimer;
  };

#endif //#ifndef __VEHICLE_NISSANLEAF_H__
