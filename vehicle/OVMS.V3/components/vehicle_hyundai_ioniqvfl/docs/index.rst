=================
Hyundai Ioniq vFL
=================

**Hyundai Ioniq Electric (28 kWh)**

- Vehicle Type: **HIONVFL**
- Log tag: ``v-hyundaivfl``
- Namespace: ``xhi``
- Maintainers: `Michael Balzer <dexter@dexters-web.de>`_
- Sponsors: Henri Bachmann
- Credits: `EVNotify <https://github.com/EVNotify>`_


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
Range Display               No
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Partial
BMS v+t Display             Yes
SOH Display                 Yes
TPMS Display                No
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others
=========================== ==============


--------------
Custom Metrics
--------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xhi.b.soc.bms                            78.5%                    Internal BMS SOC
xhi.c.state                              128                      Charge state flags
======================================== ======================== ============================================


----------
Debug Logs
----------

To see all PID poll results in your log, set log level ``verbose`` for component ``v-hyundaivfl``.

