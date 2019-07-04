===========
Nissan Leaf
===========

Vehicle Type: **NL**

This vehicle type supports the Nissan Leaf.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: 2011-2017 (24kWh & 30kWh)
Vehicle Cable               1779000 Nissan Leaf OBD-II to DB9 Data Cable for OVMS
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes (by default based on GIDS)
Range Display               Yes (by default based on GIDS)
GPS Location                Yes (from modem module GPS)
Speed Display               Yes (from vehicle speed PID)
Temperature Display         Yes (from vehicle temperature PIDs)
BMS v+t Display             Yes
TPMS Display                Yes (If hardware available)
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              Start charge only
Cabin Pre-heat/cool Control Yes (requires Nissan TCU unplug or hardware mod on Gen 1)
Lock/Unlock Vehicle         Not currently supported
Valet Mode Control          Not currently supported
Others
=========================== ==============

-------------
Configuration
-------------

^^^^^^^^^^^^^^^^
2011-2015 models
^^^^^^^^^^^^^^^^

To enable remote commands, either unplug any CARWINGS, Nissan Connection or TCU units or on Generation 1 Cars, wire the RC3 to TCU pin 11 (see MyNissanLeaf post)

^^^^^^^^^^^^^^^^
2016-2017 models
^^^^^^^^^^^^^^^^

Set the model year as follows and if necessary configure 30 kwhr model

``config set xnl modelyear 2016``

or

``config set xnl modelyear 2017``

^^^^^^^^^^^^^
30 kwh models
^^^^^^^^^^^^^

For models with a 30 kwhr battery pack, set  the capacity manually as follows

``config set xnl maxGids 356``
``config set xnl newCarAh 79``

Assistance is appreciated as I haven't had time to try to override the TCU using the OVMS or find an alternative solution to prevent the TCU overriding the messages while still allowing the hands free microphone to work.

-----------------
Range Calculation
-----------------

The OVMS uses 2 configuration options to calculate remaining range, whPerGid (default 80Wh/gid) and kmPerKWh (default 7.1km/kWh). The range calculation is based on the remaining gids reported by the LBC and at the moment does not hold 5% in reserve like LeafSpy. Feedback on this calculation is welcomed.

