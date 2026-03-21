==========================
Cadillac 2nd gen CTS
==========================

Vehicle Type: **C2CTS**

This vehicle type supports the 2008-2014 Cadillac CTS.

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
TPMS Display                No
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
The module is located behind the glove box and connector J1 has
everything needed:

=== ===== ===========================
J1  DB9-F Signal
=== ===== ===========================
14  2	  can1 L (CAN bus #5 L)
4   3	  Chassis / Power GND
6   7	  can1 H (CAN bus #5 H)
16  9	  +12V Vehicle Power
=== ===== ===========================

The roof mounted shark fin contains three antennas: OnStar GPS,
OnStar cellular, and XM satellite radio. GPS and cellular and
combined on a single coax which makes it difficult to repurpose the
OnStar antennas for OVMS. One setup that works is to use the shark
fin antenna for OVMS GPS and place a separate cellular antenna on
the rear window package tray.

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
