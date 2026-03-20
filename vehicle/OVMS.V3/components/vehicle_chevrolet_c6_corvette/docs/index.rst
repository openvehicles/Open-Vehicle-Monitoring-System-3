==========================
Chevrolet C6 Corvette
==========================

Vehicle Type: **C6CORVETTE**

This vehicle type supports the 2005-2013 Chevrolet Corvette

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3
Vehicle Cable               OBD-II cable
GSM Antenna                 Works with any OVMS v3 supported antenna
GPS Antenna                 Works with any OVMS v3 supported antenna
SOC Display                 Yes
Range Display               No
GPS Location                Yes (from GPS module/antenna)
Speed Display               No
Temperature Display         No
BMS v+t Display             No
TPMS Display                Yes
Charge Status Display       Yes (Fuel level)
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      12V battery
=========================== ==============

The under dash OBD-II connector is located to the left of the
steering wheel.

One of the best places to hardwire connections is at the OnStar
module (aka the vehicle communication interface module or VCIM).
There are several possible module locations. 2010 and later typically
have the module above the IP fuse box and behind the glove box.
2009 and earlier base and Z51 cars have the module in the passenger
side hatch cubby. For 2009 and earlier Z06 and ZR1 cars the module
is under the passenger seat.

Connector C1 has power:

=== ===== ===========================
C1  DB9-F Signal
=== ===== ===========================
8   3	  Chassis / Power GND
15  9	  +12V Vehicle Power
=== ===== ===========================

Connector C4 has the high speed CAN bus:

=== ===== ===========================
C4  DB9-F Signal
=== ===== ===========================
3   2	  can1 L (CAN bus #5 L)
2   7	  can1 H (CAN bus #5 H)
=== ===== ===========================

Fun fact: The high speed CAN bus transits the OnStar module via
connector C4 and disconnecting it bifurcates the bus and prevents
the car from starting.

It's possible to repurpose the OnStar antenna for use with OVMS.
The antenna is mounted to the inside of the front windshield on the
passenger side. Early models have separate coax cables, later GPS
and cellular were combined on a single coax. If you have two coax
cables you will a blue/C female FAKRA to male SMA adapter for the
GPS and a female mini UHF to male SMA adapter.

----------------
Standard Metrics
----------------

============================= ============================================
Metric name                   Notes
============================= ============================================
ms_v_b_soc                    Fuel level (%)
ms_v_bat_12v_voltage          Resolution of 0.1 V
ms_v_e_on
ms_v_vin                      17 characters
============================= ============================================
