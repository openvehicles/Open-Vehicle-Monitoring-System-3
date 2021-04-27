=================
Chevrolet Bolt / Opel AmperaE
=================

**Chevrolet Bolt / Opel AmperaE (60Kwh)**

- Vehicle Type: **BAE**
- Log tag: ``v-bae``
- Namespace: ``xbae``

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
SOC Display                 No
Range Display               No
GPS Location                Yes (from modem module GPS)
Speed Display               No
Temperature Display         No
BMS v+t Display             No
SOH Display                 No
TPMS Display                No
Charge Status Display       No
Charge Interruption Alerts  No
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
xbae.b.soc.bms                            78.5%                    Internal BMS SOC
xbae.c.state                              128                      Charge state flags
======================================== ======================== ============================================


----------
Debug Logs
----------

To see all PID poll results in your log, set log level ``verbose`` for component ``v-xbae``.

