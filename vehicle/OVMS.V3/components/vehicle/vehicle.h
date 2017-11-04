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

#ifndef __VEHICLE_H__
#define __VEHICLE_H__

#include <map>
#include <string>
#include "can.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

using namespace std;

// Polling types supported:
//  (see https://en.wikipedia.org/wiki/OBD-II_PIDs
//   and https://en.wikipedia.org/wiki/Unified_Diagnostic_Services)

#define VEHICLE_POLL_TYPE_NONE          0x00
#define VEHICLE_POLL_TYPE_OBDIICURRENT  0x01 // Mode 01 "current data"
#define VEHICLE_POLL_TYPE_OBDIIFREEZE   0x02 // Mode 02 "freeze frame data"
#define VEHICLE_POLL_TYPE_OBDIIVEHICLE  0x09 // Mode 09 "vehicle information"
#define VEHICLE_POLL_TYPE_OBDIISESSION  0x10 // UDS: Diagnostic Session Control
#define VEHICLE_POLL_TYPE_OBDIIGROUP    0x21 // enhanced data by 8 bit PID
#define VEHICLE_POLL_TYPE_OBDIIEXTENDED 0x22 // enhanced data by 16 bit PID

#define VEHICLE_POLL_NSTATES            4

class OvmsVehicle
  {
  public:
    OvmsVehicle();
    virtual ~OvmsVehicle();

  protected:
    canbus* m_can1;
    canbus* m_can2;
    canbus* m_can3;
    QueueHandle_t m_rxqueue;
    TaskHandle_t m_rxtask;
    bool m_registeredlistener;

  private:
    void VehicleTicker1(std::string event, void* data);
    void VehicleConfigChanged(std::string event, void* data);
    void PollerSend();
    void PollerReceive(CAN_frame_t* frame);

  protected:
    virtual void IncomingFrameCan1(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan2(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan3(CAN_frame_t* p_frame);
    virtual void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

  protected:
    uint32_t m_ticker;
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    virtual void Ticker300(uint32_t ticker);
    virtual void Ticker600(uint32_t ticker);
    virtual void Ticker3600(uint32_t ticker);

  protected:
    virtual void ConfigChanged(OvmsConfigParam* param);
  
  protected:
    void RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed);

  public:
    virtual void RxTask();
    virtual const std::string VehicleName();

  public:
    typedef enum
      {
      NotImplemented = 0,
      Success,
      Fail
      } vehicle_command_t;
    typedef enum
      {
      Standard = 0,
      Storage,
      Range,
      Performance
      } vehicle_mode_t;

  public:
    virtual vehicle_command_t CommandSetChargeMode(vehicle_mode_t mode);
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStartCharge();
    virtual vehicle_command_t CommandStopCharge();
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart);
    virtual vehicle_command_t CommandCooldown(bool cooldownon);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandHomelink(uint8_t button);
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);

  public:
    typedef struct
      {
      uint32_t txmoduleid;
      uint32_t rxmoduleid;
      uint16_t type;
      uint16_t pid;
      uint16_t polltime[VEHICLE_POLL_NSTATES];
      } poll_pid_t;

  protected:
    uint8_t           m_poll_state;           // Current poll state
    canbus*           m_poll_bus;             // Bus to poll on
    const poll_pid_t* m_poll_plist;           // Head of poll list
    const poll_pid_t* m_poll_plcur;           // Current position in poll list
    uint32_t          m_poll_ticker;          // Polling ticker
    uint32_t          m_poll_moduleid_sent;   // ModuleID last sent
    uint32_t          m_poll_moduleid_low;    // Expected response moduleid low mark
    uint32_t          m_poll_moduleid_high;   // Expected response moduleid high mark
    uint16_t          m_poll_type;            // Expected type
    uint16_t          m_poll_pid;             // Expected PID
    uint16_t          m_poll_ml_remain;       // Bytes remainign for ML poll
    uint16_t          m_poll_ml_offset;       // Offset of ML poll
    uint16_t          m_poll_ml_frame;        // Frame number for ML poll

  protected:
    void PollSetPidList(canbus* bus, const poll_pid_t* plist);
    void PollSetState(uint8_t state);
  };

template<typename Type> OvmsVehicle* CreateVehicle()
  {
  return new Type;
  }

class OvmsVehicleFactory
  {
  public:
    OvmsVehicleFactory();
    ~OvmsVehicleFactory();

  public:
    typedef OvmsVehicle* (*FactoryFuncPtr)();
    typedef map<std::string, FactoryFuncPtr> map_type;

    OvmsVehicle *m_currentvehicle;
    map_type m_map;

  public:
    template<typename Type>
    short RegisterVehicle(std::string VehicleType)
      {
      FactoryFuncPtr function = &CreateVehicle<Type>;
        m_map.insert(std::make_pair(VehicleType, function));
      return 0;
      };
    OvmsVehicle* NewVehicle(std::string VehicleType);
    void ClearVehicle();
    void SetVehicle(std::string type);
  };

extern OvmsVehicleFactory MyVehicleFactory;

#endif //#ifndef __VEHICLE_H__
