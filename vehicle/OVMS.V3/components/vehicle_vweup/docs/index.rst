
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
SOC Display                 Yes
Range Display               Yes
Cabin Pre-heat/cool Control tba
GPS Location                Yes (from modem module GPS)
Speed Display               Yes (untested)
Temperature Display         Yes (outdoor, cabin)
BMS v+t Display             tba
TPMS Display                No
Charge Status Display       tba
Charge Interruption Alerts  tba
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          tba
Others                      Odometer, VIN, front doors status
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

======= ==================== ======= =============================== =======
ID	Conversion	     Unit    Function		     	     Comment
======= ==================== ======= =============================== =======
61A	d7/2   		     % 	     State of Charge (relative)	     (SoC)
320	(d4<<8+d3-1)/190     km/h    Speed		     	     (KMH)
65F	3 Msg d5-7,d1-7,d1-7 String  VIN number		     	     (VIN)
571	5+(.05*d0)	     Volt    12 Volt battery voltage 	     (12V)
65D	d3&f<<12|d2<<8|d1    km      Odometer		     	     (KM)
3E3	(d2-100)/2           °C      Cabin temperature      	     (IN)
527	(d4/2)-50	     °C      Outdoor temperature     	     (OUT)
52D	d0		     km	     Calculated range		     
470	d1 00,01,02	     Integer Status doors		     
======= ==================== ======= =============================== =======

All MsgID's are still unconfirmed. Code is experimental.

-------------------------
Links to vehicle log files
-------------------------
the .asc files behind these links contain logs of all messages on the Comfort CAN while periodically issuing requests for certain known parameters at the OBD2-port.

**Motor data**

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Testfahrt3.asc*

ECU 01 (7E0/7E8)

==================== ================= ===============
logged OBD2-codes    value             comments 
==================== ================= ===============
22 F4 5B             state of charge   net?
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

**KCAN+obd_charge90-100.asc: charging from about 90% to 100%:**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_charge90-100.asc**

car is charged from about 90% until it stops charging, two different SoC codes are logged:

==================== =========================== ===============
logged OBD2-codes    value                       comments 
==================== =========================== ===============
7E0 03 22 F4 5B      state of charge from ECU 01
7E5 03 22 02 8C      state of charge from ECU 8C
==================== =========================== ===============

**KCAN+obd_Klima_remote.asc: remote heating test via OBD from ECU 75 (767/7D1):**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Klima_remote.asc**

remote heating started via OBD at 200s, stopped at 230s, started again 250-280s

**KCAN_Klima_remote_app_2x.asc: remote heating test via online app (no OBD):**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_Klima_remote_app_2x.asc**

car was fully asleep (no messages on KCAN), remote heating turned on via app, then turned off again until car was fully asleep, then repeated the process

**KCAN_remote_Klima_app_22_20C.asc (no OBD):**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_app_22_20C.asc**

remote heating activated for two different temperatures (22°C and 20°C, previous logs were all at 21°C)

**KCAN_remote_Klima_manuell_test3.asc:**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_manuell_test3.asc**

unsuccessfull desperate attempt at getting heater to turn on with wild combinations of signals on 43D, 3E1 and 5E8... :(

**KCAN+obd_Testfahrt_Akku1.asc: short trip with battery data from ECU 8C (7E5/7ED):**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Testfahrt_Akku1.asc**

==================== ============================ ===============
logged OBD2-codes    value                        comments 
==================== ============================ ===============
22 1E 34             minimum cell voltage & index
22 1E 33             maximum cell voltage & index
22 1E 0F             minimum temperature & sensor
22 1E 0E             maximum temperature & sensor
22 1E 3B             battery voltage
22 1E 3D             battery current
22 18 8D             battery power loss
22 02 8C             state of charge              gross?
==================== ============================ ===============

**KCAN+obd_rundown_6-0km.asc: complete rundown to vehicle turn off with battery data from ECU 01 & 8C (7E0/7E8 & 7E5/7ED):**

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_rundown_6-0km.asc**

==================== ============================ ===============
logged OBD2-codes    value                        comments 
==================== ============================ ===============
22 1E 34             minimum cell voltage & index
22 1E 33             maximum cell voltage & index
22 1E 0F             minimum temperature & sensor
22 1E 0E             maximum temperature & sensor
22 1E 3B             battery voltage
22 1E 3D             battery current
22 F4 5B             state of charge   		  net?
22 02 8C             state of charge              gross?
==================== ============================ ===============

**https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_nur_KommSG_remote_Klima_App.asc**:

Communication attempt of ECU for remote services without connection to vehicle
