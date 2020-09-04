
==============
VW e-Up (T26A)
==============

Vehicle Type: **VWUP.T26**

This vehicle type supports the VW e-UP (2013-, 2020-), Skoda Citigo E IV and the Seat MII electric (2020-).


-----------------
Development notes
-----------------

We prepared the code to use functional units depending on which hardware infrastructure the user uses.

Under this vehicle component we use the original T26A approach, which can write to the Comfort CAN and is able to manage the climate control of the car.

Because T26A does not use the OBD socket and has very limited access to system information a new OBD approach is thought off, but not implemented here.

To compile the code you will need to check out the repository, check out the components 
mongoose, libzip and zlib  and copy the file

sdkconfig.default.hw31

from the OVMS.V3/support folder to the OVMS.V3 folder and rename it to

sdkconfig


----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: 2020- (2013- VW e-Up as well)
Vehicle Cable               Comfort CAN T26A (OCU connector cable, located under front passenger seat) to DB9 Data Cable for OVMS using pin 6 and 8 for can3
GSM Antenna                 T4AC - R205 with fakra_sma adapter cable or 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 T4AC - R50 with fakra_sma adapter cable or 1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
Cabin Pre-heat/cool Control Yes
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes (outdoor, cabin)
BMS v+t Display             tba
TPMS Display                No
Charge Status Display       Yes
Charge Interruption Alerts  Yes (per notification on the charging state)
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          tba
Others                      Odometer, VIN, front doors status
=========================== ==============


----------------------------------------
Pinout OCU T26A - OVMS DB9 adapter cable
----------------------------------------

For the T26A approach we directly tap into the comfort can bus over the OCU cable 

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

.. image:: grinded_ribbon.png
  :width: 100px
  :align: right

For confectioning the T26A adapter cable you can use a standard 26 pin ribbon cable (2x13 pins, 2,54mm grid dimension) and a DB9 female D-Sub connector. You will need to grind down the rim of the socket of the ribbon cable.

To make a GSM/GPS adapter cable to connect to the original VW fakra socket you can use a double fakra male connector with two SMA male connectors attached.

.. image:: fakra_sma.png
  :width: 200px
  :align: right


-----------------
User notes (T26A)
-----------------

* Disconnect the T26A cable and the fakra cable from the OCU (online communication unit) located beneath the passenger seat.
* Connect the confectioned T26A adapter DB9 cable attached to the OVMS to the VW T26A plug.
* Connect your GMS/GPS fakra adapter to the VW fakra cable.
* Configure OVMS as discribed in the OVMS user manual
* Register and connect OVMS to a server (as guided within the OVMS setup)
* Install the OVMS app on your smartphone or tablet and configure to connect to the server
* Enjoy :-)

----------------------
Climate control (T26A)
----------------------

Climate control works, as long as write access to the comfort can has been enabled in the app or in the OVMS webinterface (VW e-Up ... -> Features).

Once the AC is turned on by the app there will be a delay of about 15 seconds till the AC actually starts in the car. Further 10 seconds all communication from the app to the car is blocked.

The communication from the app to the car is also blocked for 10 seconds after the "AC off" command from the app to the car. There is no delay between the "AC turn off" signal of the app and the actually turning off in the car.

In rare cases climate off does not respond. You then need to start it again (when it is already running) to be able to stop it.

The cabin target temperature can be set from the OVMS webinterface (VW e-Up ... -> Climate control).


-----------------------------
IDs on Comfort CAN Bus (T26A)
-----------------------------
message <hhh ll d0 d1 d2...>

hhh: header ID

ll: length

d0 d1...: data
bxx: bit number

======= ==================== ======= =========================================== =======
ID	Conversion	     Unit    Function		     	         	 Comment
======= ==================== ======= =========================================== =======
61A	d7/2   		     % 	     State of Charge (relative)	         	 (SoC)
320	(d4<<8+d3-1)/190     km/h    Speed		     	         	 (KMH)
65F	3 Msg d5-7,d1-7,d1-7 String  VIN number		     	         	 (VIN)
571	5+(.05*d0)	     Volt    12 Volt battery voltage 	         	 (12V)
65D	d3&f<<12|d2<<8|d1    km      Odometer		     	         	 (KM)
3E3	(d2-100)/2           °C      Cabin temperature      	         	 (IN)
527	(d5/2)-50	     °C      Outdoor temperature     	         	 (OUT)
52D	d0		     km	     Calculated range		     
470	d1 00,01,02	     Integer Status doors		     
3E1	d4		     Integer Blower speed (57,66,7D,98,BB,DE,FA)
575	d0 00 to 0F 	     Integer Key position		         	 (KEY)
569	b07			     "AC"-LED
69C	d1/10+10	     °C      temperature setpoint for remote AC
				     (only in message D2 <d1> 00 1E 1E 0A 00 00)
43D	d1 01 or 11		     TX: Working or sleeping in the ring     	 (OCU)
5A7	d1 16			     TX: OCU AC blocking signal
5A9	all 00			     TX: OCU heartbeat
6E9	multiple msg		     TX: AC on / off signals                 	 (AC)
	d0 C1 d6 xx	     °C      TX: set cabin temperature for 69C       	 (CAB)
61C	d2 00,01		     Charging			             	
======= ==================== ======= =========================================== =======


--------------------------
Links to vehicle log files
--------------------------
the .asc files behind these links contain logs of all messages on the Comfort CAN while periodically issuing requests for certain known parameters at the OBD2-port.

**Motor data**

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Testfahrt3.asc:*

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

**Battery data**

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_charge90-100.asc:*

car is charged from about 90% until it stops charging, two different SoC codes are logged:

==================== =========================== ===============
logged OBD2-codes    value                       comments 
==================== =========================== ===============
7E0 03 22 F4 5B      state of charge from ECU 01
7E5 03 22 02 8C      state of charge from ECU 8C
==================== =========================== ===============

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Testfahrt_Akku1.asc:*

short trip with battery data from ECU 8C (7E5/7ED)

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

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_rundown_6-0km.asc:*

complete rundown to vehicle turn off with battery data from ECU 01 & 8C (7E0/7E8 & 7E5/7ED)

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

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_Schnellladen_Fahrt.asc:*

Quick charge followed by a quick trip :)
Useful to distinguish battery temperature from SoC as temperature keeps increasing (slightly) during the trip.

**Remote Air Conditioning**

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN%2Bobd_Klima_remote.asc:*

remote heating test via OBD from ECU 75 (767/7D1)
remote heating started via OBD at 200s, stopped at 230s, started again 250-280s

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_Klima_remote_app_2x.asc:*

remote heating test via online app (no OBD)
car was fully asleep (no messages on KCAN), remote heating turned on via app, then turned off again until car was fully asleep, then repeated the process

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_app_22_20C.asc:*

remote heating activated for two different temperatures (22°C and 20°C, previous logs were all at 21°C)

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_manuell_test3.asc*

unsuccessfull desperate attempt at getting heater to turn on with wild combinations of signals on 43D, 3E1 and 5E8... :(

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_nur_KommSG_remote_Klima_App.asc:*

Communication attempt of ECU for remote services without connection to vehicle

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_wakeup_ID400_errors.asc:*

unsuccessfull attempt sending 400 0C 02 A0 04 04 04 00 00 without communication ECU connected

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_Klima_test_5A7_failed.asc:*

unsuccessfull attempt sending 5A7 60 16 00 00 00 00 00 00 every second without communication ECU connected

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_OCU_connect.asc:*

Initial messages when OCU is connected to car

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_gateway_failed2.asc:*

Unsuccessful attempt of putting CANoe between OCU and car as gateway to determine direction of messages (there seems to be a problem with ACK signals)

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_setpoint_19C.asc*
*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_setpoint_20C.asc*
*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_setpoint_20.5C.asc*
*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_setpoint_22C.asc:*

KCAN messages when temperature setpoint is changed via Car Net (car is asleep in between)

*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_OBD_manual_1%2B4_works.asc*
*https://github.com/sharkcow/VW-e-UP-OBD-CAN-logs/blob/master/KCAN_remote_Klima_OBD_manual_1%2B4once_works_turnsoff.asc:*

starting of remote AC via OBD commands. Except for one instance (at the end of first file), AC always turns off again right away... :(


