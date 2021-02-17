
=====
MG EV
=====

Vehicle Type: **MGEV**

This vehicle type supports the MG ZS EV (2019-).

MG5 is not yet supported in this build.


----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: 2019-
Vehicle Cable               Right hand OBDII cable (RHD), Left hand OBDII cable (LHD)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes (BMS calculated and WLTP range from SoC)
Cabin Pre-heat/cool Control tba
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          No
Others                      tba
=========================== ==============

-----------------
Development notes
-----------------

To compile the code you will need to check out the repository, check out the components 
mongoose, libzip and zlib  and copy the file

sdkconfig.default.hw31

from the OVMS.V3/support folder to the OVMS.V3 folder and rename it to

sdkconfig

-----------------------
Community documentation
-----------------------

This module is developed from the work provided by the My MG ZS EV community at
**https://discourse.mymgzsev.com/**
Please join the Slack channel for support and the latest builds.


----------
Car status
----------

The car is accessible over the OBD port when it is running (ignition on) and for around
40 seconds after it is turned off or the car is "tweaked" (lock button pushed, etc).

The OBD port may be kept awake by using the "tester present" message to the gateway ECU.
This keeps a lot of systems awake and draws roughly 5A on the 12V bus, so it's not a good
idea to do.

The MGEV module now monitors (and automatically calibrates) the 12V status and will
automaticaly switch on when the 12V exceeds 12.9V. When it does this it will try to poll
the vehicle for data.

There are 4 Poll States
 - 0 ListenOnly: the OVMS module is quiet and stops sending polls, it will enter this state after 50s of being < 12.9V
 - 1 Charging: the OVMS module sends charging specific queries
 - 2 Driving: the OVMS module sends driving specific queries
 - 3 Backup: the OVMS module cannot get data from the car when it is charging so just retries SoC queries
 
The Gateway (GW, GWM) is the keeper of all the data of the car and will enter a locked state 
when it is woken by the car starting charging and the car is locked. 
This we have called "Zombie Mode", and we have developed an override for this. 
 
This override, however causes a few strange things to happen:
 - If Zombie mode override is active, the car will not unlock the charge cable. To fix this dusrupt the charge and wait 50s for OVMS to go back to sleep and the cable should release (or unplug OVMS)
 - Zombie mode override resets the “Accumulated Total Trip” on the Cluster
 - Zombie mode override sets the gearshift LEDs switch on
