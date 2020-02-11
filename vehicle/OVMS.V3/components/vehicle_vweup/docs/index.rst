=======
VW e-Up 
=======

Vehicle Type: **VWUP**

This vehicle type supports the VW e-UP, Skoda Citigo E IV and the Seat MII electric.

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
SOC Display                 tba
Range Display               tba
Cabin Pre-heat/cool Control tba
GPS Location                tba
Speed Display               tba
Temperature Display         tba
BMS v+t Display             tba
TPMS Display                tba
Charge Status Display       tba
Charge Interruption Alerts  tba
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          tba
Others                      tba
=========================== ==============

----------------------------------------
Pinout OCU T4AC - OVMS DB9 adapter cable
----------------------------------------
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

Because of the compilicated VAG OBD-gateway communication protocol
we directly tap into the comfort can bus over the OCU cable.

Advantage is a faster and easier development.

Disadvantage is that we won't have all vehicle information available
and we won't be able to access all control units.

====================================================================

