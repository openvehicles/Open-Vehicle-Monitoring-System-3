=======================
Smart ED/EQ Gen.4 (453)
=======================

Vehicle Type: **SQ**

The Smart ED/EQ Gen.4 will be documented here.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3 (or later)
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
SOH Display                 Yes
Range Display               Yes
GPS Location                Yes (from modem module)
Speed Display               Yes
Temperature Display         Yes (External Temp and Battery)
BMS v+t Display             only Cell Volts atm.
TPMS Display                Yes pressure only, for iOS App Open Vehicle activate at Features Web UI 'iOS TPMS fix' for showing pressure value
Charge Status Display       Yes
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control Yes (only 5/10/15 Minutes Pre-heat/cool and timebased Pre-heat/cool App/Web, no Temperature control and SoC > 30% needed)
Lock/Unlock Vehicle         No (not really Implementet, only when the car is open, you can close it. But the lock indicator shows unlocked!)
Valet Mode Control          No
Maintenance Reminders       Yes
12V Battery Monitoring      Yes (if 12V alert raised, the car starts the 12V charging process for 15 Minutes. (homelink 3))
DDT4all simple Support      Yes (a List of all possible commands at www.smart-EMOTION.de)

=========================== ==============
Others
=========================== ==============

-------------------------
Using Cabin Pre-heat/cool
-------------------------

Only 5 Minutes Booster are Implementet white 
Climatecontrol on or 
homelink 1 = 5 Minutes
homelink 2 = 10 Minutes
homelink 3 = 15 Minutes