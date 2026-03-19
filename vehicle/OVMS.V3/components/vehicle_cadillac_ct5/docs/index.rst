==========================
Cadillac CT5
==========================

Vehicle Type: **CT5**

This vehicle type supports the 2020-2026 Cadillac CT5.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    OVMS v3
Vehicle Cable               (Custom)
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

The CT5 uses the GM Global B platform which requires authentication
so polling isn't currently possible. While it's possible support
for OBD-II/SAE J1979 service modes can be added, features such as
vehicle lock/unlock are unlikely. CAN bus #6 is available on the
under dash OBD-II connector but it is designed to be used for
diagnostics and emissions testing and no frames are relayed to it.
Instead we listen to CAN bus #5 which is available on connectors
going to the audio amplifier in the trunk.

To support this vehicle it is necessary to make a custom cable.
Battery power and ground can be found on audio amplifier connector X1:

=== ===== ===========================
X1  DB9-F Signal
=== ===== ===========================
8   3	  Chassis / Power GND
4   9	  +12V Vehicle Power
=== ===== ===========================

CAN bus #5 is available on X3:

=== ===== ===========================
X3  DB9-F Signal
=== ===== ===========================
9   2	  can1 L (CAN bus #5 L)
8   7	  can1 H (CAN bus #5 H)
=== ===== ===========================

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

