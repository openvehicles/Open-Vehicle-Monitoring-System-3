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

using namespace std;

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

  protected:
    virtual void IncomingFrameCan1(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan2(CAN_frame_t* p_frame);
    virtual void IncomingFrameCan3(CAN_frame_t* p_frame);

  protected:
    void RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed);

  public:
    virtual void RxTask();
    virtual const std::string VehicleName();
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
