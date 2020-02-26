
=======
VW e-Up 
=======

Vehicle Type: **VWUP**

This vehicle type supports the VW e-UP, Skoda Citigo E IV and the Seat MII electric.

-----------------
Development notes
-----------------

The code is highly experimental. Some MsgIDs used are not confirmed yet.

When our pull request to the OVMS master repository is commited, these notes will be deleted.

To compile this code you will need to check out this repository, check out the components 
mongoose, libzip and zlib from the OVMS master repostory and copy the file

sdkconfig.default.hw31.vweup

from the OVMS.V3/support folder to the OVMS.V3 folder and rename it to

sdkconfig

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: 2020- (should support 2013- VW e-Up as well)
Vehicle Cable               Comfort CAN T26A (OCU connector cable, located under front passenger seat) to DB9 Data Cable for OVMS using pin 6 and 8 for can3
GSM Antenna                 T4AC - R205 with fakra_sma adapter cable or 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 T4AC - R50 with fakra_sma adapter cable or 1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                 Implemented but untested
Range Display               tba
Cabin Pre-heat/cool Control tba
GPS Location                tba
Speed Display               Implemented but untested
Temperature Display         tba
BMS v+t Display             tba
TPMS Display                tba
Charge Status Display       tba
Charge Interruption Alerts  tba
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          tba
Others                      VIN and 12 volt battery status implemented but untested
=========================== ==============

----------------------------------------
Pinout OCU T26A - OVMS DB9 adapter cable
----------------------------------------

Because of the compilicated VAG OBD-gateway communication protocol
we directly tap into the comfort can bus over the OCU cable.

The OCU connector is located under the passenger seat.

Advantage is a faster and easier development.

Disadvantage is that we won't have all vehicle information available
and we won't be able to access all control units.


======= ======= ===========================
OCU	DB9-F	Signal
======= ======= ===========================
26	3	Chassis / Power GND
.	2	can1 L (Can Low, not used)
.	7	can1 H (Can High, not used)
.	4	can2 L (Can Low, not used)
.	5	can2 H (Can High, not used)
2	6	can3 L (Comfort-can Low)
14	8	can3 H (Comfort-can High)
1	9	+12V Vehicle Power
======= ======= ===========================

----------------------
IDs on Comfort CAN Bus
----------------------
message <hhh ll d0 d1 d2...>

hhh: header ID

ll: length

d0 d1...: data

======= ==================== ======= ======================= =======
ID	Conversion	     Unit    Function		     Comment
======= ==================== ======= ======================= =======
61A	d7/2.55 ???	     % 	     State of Charge	     (SoC)
320	d4*1.34	             km/h    Speed		     (KMH)
65F	3 Msg d5-7,d1-7,d1-7 String  VIN number		     (VIN)
571	5+(.05*d0)	     Volt    12 Volt battery voltage (12V)
======= ==================== ======= ======================= =======

All MsgID's are still unconfirmed. Code is experimental.

-------------------------
List of vehicle log files
-------------------------
the .asc files in this directory contain logs of all messages on the Comfort CAN while periodically issuing requests for certain known parameters at the OBD2-port.

**KCAN+obd_Testfahrt3.asc: short trip with motor data from ECU 01 (7E0/7E8)**

==================== ================= ===============
logged OBD2-codes    value             comments 
==================== ================= ===============
22 F4 5B             state of charge
22 14 7D             motor current
22 14 84             motor voltage
22 14 7E             motor power
22 14 7F             motor torque
22 14 9A             motor rpm
22 F4 49             acc. pedal
22 F4 0D             speed
22 14 85             battery power
22 16 17             HV-system current
==================== ================= ===============

**KCAN+obd_ .asc: remote heating test from ECU 75 (767/7D1)**
remote heating started at 200s, stopped at 230s, started again 250-280s
