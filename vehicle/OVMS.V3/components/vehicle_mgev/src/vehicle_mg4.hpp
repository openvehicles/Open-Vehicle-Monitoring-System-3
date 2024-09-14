/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          3rd September 2020
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2011       Michael Stegen / Stegen Electronics
 ;    (C) 2011-2018  Mark Webb-Johnson
 ;    (C) 2011       Sonny Chen @ EPRO/DX
 ;    (C) 2020       Chris Staite
 ;    (C) 2023       Peter Harry
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

#ifndef vehicle_mg4_hpp
#define vehicle_mg4_hpp

#include "vehicle_mgev.h"

/*#define WLTP_RANGE 450.0 //km
 #define BATT_CAPACITY 64 //kWh
 #define MAX_CHARGE_RATE 140 //kW
 #define BMSDoDUpperLimit 930.0
 #define BMSDoDLowerLimit 25.0*/

enum VehicleStates {
  VehicleStateParked = 0,
  VehicleStateCharging,
  VehicleStateDriving,
};

class OvmsVehicleMg4 : public OvmsVehicleMgEv
{
public:
  OvmsVehicleMg4();
  ~OvmsVehicleMg4();
  bool previousPollEnable;
  float batteryVoltage;
  float speed;
  VehicleStates vehState;
  
protected:
  void Ticker1(uint32_t ticker) override;
  vehicle_command_t CommandWakeup() override;
  
private:
  void MainStateMachine(canbus* currentBus, uint32_t ticker);
};


#endif /* vehicle_mg4_hpp */
