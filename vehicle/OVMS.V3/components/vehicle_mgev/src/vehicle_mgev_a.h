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

#ifndef __VEHICLE_MGEV_A_H__
#define __VEHICLE_MGEV_A_H__

#include "vehicle_mgev.h"

#define WLTP_RANGE 263.0 //km
#define BATT_CAPACITY 42.5 //kWh
#define MAX_CHARGE_RATE 82 //kW
#define BMSDoDUpperLimit 940.0
#define BMSDoDLowerLimit 25.0

class OvmsVehicleMgEvA : public OvmsVehicleMgEv
{
  public:
    OvmsVehicleMgEvA();
    ~OvmsVehicleMgEvA();

  protected:
    void Ticker1(uint32_t ticker) override;
    vehicle_command_t CommandWakeup() override;   

  private:
    void ZombieMode();
    void DeterminePollState(canbus* currentBus, uint32_t ticker);
    void SendAlarmSensitive(canbus* currentBus);

    /// The states that the gateway CAN can be in
    enum GwmState
    {
        AllowToSleep,  // Stop Sending any control to GWM
        SendTester,  // Send Tester Present to GWM
        SendDiagnostic,  // Send Diagnostic Session Override to GWM
        Undefined    // We are not controlling GWM
    };

    /// The last ticker that we saw RX packets on
    uint32_t m_rxPacketTicker;
    /// The last number of packets received on the CAN
    uint32_t m_rxPackets;
    /// The number of ticks that no packets have been recieved
    uint32_t m_noRxCount;
    /// The current state of control commands we are sending to the gateway
    GwmState m_gwmState;
    /// The ticker time for Zombie Mode Sleep Timeout before attempting to wake.
    uint32_t m_preZombieOverrideTicker;
    /// Boolean for Car State
    bool carIsResponsiveToQueries;
    /// A count of the number of times we've woken the car to find it wasn't charging
    uint16_t m_diagCount;    
};

#endif  // __VEHICLE_MGEV_A_H__
