===========
 BYD Atto 3
===========

- Vehicle Type: **ATTO3**
- Log tag: ``v-atto3``
- Maintainers: `Alan Harper <alan@aussiegeek.net>`_

The testing and development was done on an Australian right hand drive Atto 3 but would expect it to be the same for other countries versions (including the Yuan Plus), and may work on the Dolphin too.

Most of this data has been reverse engineered and is based on a lot of assumptions and guess work, and would be interested in reports for non CCS2 cars as well as left hand drive in terms of which doors are open

-----------------
 Support Overview
-----------------

================================ ==============
Function                         Support Status
================================ ==============
Hardware                         Any OVMS v3 (or later) module
Vehicle Cable                    OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                      1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                      1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                      Yes
Range Display                    Yes (only shows ideal range, not dynamic regardless of car setting)
GPS Location                     Yes (from modem)
Speed Display                    Yes
Temperature Display              No
BMS v+t Display                  Yes
TPMS Display                     No
Charge Status Display            Yes
Charge Interruption              Yes
Alerts                           No
Charge Control                   No
Cabin Pre-heat/cool Control      No
Lock/Unlock Vehicle              No
Valet Mode Control               No
================================ ==============

----------------
Charge port door
----------------

The charge port door is reported as open when charge is in progress or waiting for scheduled charge, however the car doesn't appear to report this, and is only done so the iOS app does show charging status correctly

-----------------
Physical mounting
-----------------

I have had success with attaching the OVMS hardware behind the fuse panel cover and attaching the GPS antenna to a piece of metal higher up behind the fuse panel cover