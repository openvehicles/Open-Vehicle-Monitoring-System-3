==============================
Renault Zoe Phase 2 (OBD port)
==============================

Vehicle Type: **RZ2O**

This vehicle type supports the Renault Zoe(PH2) e.g ZE50 through OBD connection. 
All values are read-only, no control possible because of CAN Gateway.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3.3
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (Generic OBD2 Cable (left))
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes (Battery, Outside, Cabin, Motor, Inverter, Tyres)
BMS v+t Display             Yes
TPMS Display Zoe            Yes (Pressure and temperature)
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              No (planned, but access to CAN after gw neccessary)
Cabin Pre-heat/cool Control No (planned, but access to CAN after gw neccessary)
Lock/Unlock Vehicle         Yes (display only)
Valet Mode Control          No
=========================== ==============

Others:

=================================== ==============
Door open/close                     Yes (exclude hood)
Battery full charge cycles          Yes
Battery max charge, recd pwr        Yes
Trip counter from Car               No (was included, but datapoint is extremly unreliable)
Battery lifetime gauges             Yes (Charged, Recd and Used kWh)
Heat pump power, rpm and hp press.  Yes
Aux power gauges                    Yes (testing needed)
Charge type                         Yes (DC charge, testing needed)
Headlights Status                   Yes (lowbeam)
Charge efficiency calculation       Yes (but only for AC, pf is measured with power analyzer and included as statics)
=================================== ==============
