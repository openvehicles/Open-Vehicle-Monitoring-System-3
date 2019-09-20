==================
DBC Based Vehicles
==================

Vehicle Type: **DBC**

The DBC based vehicle reads a specified DBC file describing CAN bus messages and produces vehicle metrics from that. It is under development, experimental, and not generally available.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    This is vehicle specific. The DBC vehicle module can be configured to use any or all of the 3 CAN buses.
Vehicle Cable               Vehicle specific
GSM Antenna                 Standard GSM antenna
GPS Antenna                 Vehicle specific. Standard OVMS GPS supported.
SOC Display                 Yes, if DBC maps it
Range Display               Yes, if DBC maps it
GPS Location                Yes, if DBC maps it, otherwise OVMS GPS
Speed Display               Yes, if DBC maps it
Temperature Display         Yes, if DBC maps it
BMS v+t Display             Not currently supported
TPMS Display                Yes, if DBC maps it
Charge Status Display       Yes, if DBC maps it
Charge Interruption Alerts  Yes, if DBC maps it
Charge Control              Not supported by DBC format, maybe by extension
Cabin Pre-heat/cool Control Not supported by DBC format, maybe by extension
Lock/Unlock Vehicle         Not supported by DBC format, maybe by extension
Valet Mode Control          Not supported by DBC format, maybe by extension
Others                      None
=========================== ==============
