==============================
Renault Zoe Phase 2 (OBD port)
==============================

Vehicle Type: **RZ2O**

This vehicle type supports the Renault Zoe(PH2) with 41 or 52kWh battery (2019-2024) through ODB2 connection. 


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
Charge Control              No
Cabin Pre-heat/cool Control Yes*
Lock/Unlock Vehicle         Yes (display only)
Valet Mode Control          No
=========================== ==============

Others:

=================================== ==============
Door open/close                     Yes (exclude hood)
Battery full charge cycles          Yes
Battery max charge, recd pwr        Yes
Trip counter from Car               No
Battery lifetime gauges             Yes
Heat pump power, rpm and hp press.  Yes
Aux power gauges                    No
Charge type                         Yes (DC charge testing needed)
Headlights Status                   Yes (lowbeam only)
Charge efficiency calculation       Yes (but only for AC, pf is measured with power analyzer and included as statics)
=================================== ==============

-------------------
Pre-heat / climate
-------------------

If you want to preheat your car, you need to connect CAN2 from the OVMS module to the V-CAN.
The V-CAN can be grabbed at the BCM or Can-Gateway ECU for example.

------------
TCU removal
------------

If you want to remove your TCU for privacy or other reasons, you need to wait for another integration which uses V-CAN connection to detect vehicle state.
If you remove your TCU the OVMS will not detect car wake up.