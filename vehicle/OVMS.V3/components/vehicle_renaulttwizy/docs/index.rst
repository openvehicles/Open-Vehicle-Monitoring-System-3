=============
Renault Twizy
=============

Vehicle Type: **RT**


----------------
Support Overview
----------------

=========================== ================================================================
Function                    Support Status
=========================== ================================================================
Hardware                    No specific requirements (except for regen brake light control, see below)
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                No
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              Current limit, stop, SOC/range limit
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         Mapped to dynamic speed lock
Valet Mode Control          Mapped to odometer based speed lock
Others                      - Battery & motor power & energy monitoring
                            - Battery health & capacity
                            - SEVCON monitoring & tuning (Note: tuning only SC firmware <= 0712.0002 / ~07/2016)
                            - Kickdown
                            - Auto power adjust
                            - Regen brake light
                            - Extended trip & GPS logging
=========================== ================================================================

Note: regarding the Twizy, V3 works very similar to V2. All telemetry data tables and tuning profiles are fully compatible, and the commands are very similar. Therefore, the basic descriptions and background info from the V2 manual are valid for V3 as well.

  * `V2 FAQ (english) <https://dexters-web.de/faq/en>`_
  * `V2 FAQ (german) <https://dexters-web.de/faq>`_
  * `V2 documents directory <https://github.com/openvehicles/Open-Vehicle-Monitoring-System/tree/master/docs/Renault-Twizy>`_
  * `V2 user manual (PDF) <https://github.com/openvehicles/Open-Vehicle-Monitoring-System/raw/master/docs/Renault-Twizy/OVMS-UserGuide-RenaultTwizy.pdf>`_

---------------------------
Hints on V3 commands vs. V2
---------------------------

* V3 commands need to be in lower case.
* When accessing via USB terminal, first issue enable (login).
* V3 commands are similar to V2 commands, just structured slightly different.
* Try ``help``, ``?`` and ``xrt ?``. Twizy commands are subcommands of ``xrt``.
* TAB only works via USB / SSH. Shortcuts can be used generally, e.g.  ``mo t`` instead of ``module tasks``.
* A usage info is shown if a command syntax is wrong or incomplete.


--------
Contents
--------

.. toctree::
   :maxdepth: 1

   configuration
   metrics
   events
   commands
   brakehack

