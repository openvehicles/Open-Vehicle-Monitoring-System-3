=================
Hyundai Ioniq vFL
=================

**Hyundai Ioniq Electric (28 kWh)**

- Vehicle Type: **HIONVFL**
- Log tag: ``v-hyundaivfl``
- Namespace: ``xhi``
- Maintainers: `Michael Balzer <dexter@dexters-web.de>`_
- Sponsors: Henri Bachmann, Tóth Lajos
- Credits: `EVNotify <https://github.com/EVNotify>`_, `hokus15 <https://github.com/hokus15/pioniq>`_


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
Range Display               Yes
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Partial
BMS v+t Display             Yes
SOH Display                 Yes
TPMS Display                Yes
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
xhi.b.range.user                         134.56km                 Current maximum user range
xhi.b.soc.bms                            78.5%                    Internal BMS SOC
xhi.c.state                              128                      Charge state flags
xhi.e.state                              13                       General/ignition state flags
======================================== ======================== ============================================


--------------
Custom Configs
--------------

======================================== ============== ========= ============================================
Config name                              Default value  …unit     Description
======================================== ============== ========= ============================================
xhi range.ideal                          200            km        Ideal range of new battery
xhi range.user                           200            km        Typical maximum user range (updated automatically)
xhi range.smoothing                      10                       Number of SOC samples, 10=~5% SOC, 0=disable
xhi tpms.pressure.warn                   230            kPa       Tyre warning at/below this pressure
xhi tpms.pressure.alert                  220            kPa       Tyre alert at/below this pressure
xhi tpms.temp.warn                       90             °C        Tyre warning at/above this temperature
xhi tpms.temp.alert                      100            °C        Tyre alert at/above this temperature
======================================== ============== ========= ============================================

The web UI features a configuration page for this in the vehicle menu.


----------
Debug Logs
----------

To see debug messages in your log, component ``v-hyundaivfl`` to log level ``debug``. You can do so
dynamically (``log level debug v-hyundaivfl``) or persistent by the logging configuration.

To see all PID poll results in your log, set log level ``verbose`` for component ``v-hyundaivfl``.
It's recommended to accompany this by setting log level ``debug`` for ``events``.

