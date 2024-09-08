=====
MG EV
=====

1. **MG ZS EV (2019-2021)**
2. **MG ZS EV (2022-)**
3. **MG5 EV Long Range (2020-2023)**
4. **MG4 (2023-)**


- Vehicle Type: **MG**
- Log tag: ``v-mgev``
- Namespace: ``xmg``
- Maintainers: `Peter Harry`

-----------------------------
MG ZS EV (2019-2021) Variants
-----------------------------

MG ZS EVs sold in different countries seem to possess different behaviours. This is why we have developed 2 different vehicle types to support the different requirements.

^^^^^^^^^^^^^^^
Known Countries
^^^^^^^^^^^^^^^

=========   =========== ===========================   =====================   =
Country     Zombie mode Requires GWM authentication   Poll BCM causes alarm   Suggested vehicle
=========   =========== ===========================   =====================   =
UK/EU       Y           N                             Y                       UK/EU                                                     
Thailand    N           Y                             N                       TH/AU
Australia   N           N                             N                       TH/AU
=========   =========== ===========================   =====================   =

Zombie mode is when the gateway module (GWM) enters the locked state and no PIDs can be accessed. This happens for UK/EU cars when:
   1. Charger is plugged in without power, car is locked and goes to sleep.
   2. Charger is then powered on and car starts charging.

GWM authentication is required for Thai cars as GWM will be locked and no PIDs can be accessed (simiarly to Zombie mode but this happens all the time rather than just during the above scenario). Note: GWM authentication does not work as a mitigation for UK/EU's Zombie mode.

When the BCM is polled, UK/EU cars' alarm (horn) can go off so this is disabled in the UK/EU code.

^^^^^^^^^^^^^^^
Other Countries
^^^^^^^^^^^^^^^

For cars from other countries, to decide which vehicle type you should use, start off by trying the MG EV (TH) type first because Zombie mode mitigation has its side effects (see module notes below) and polling the BCM allows the OVMS to get more data from the car.

You can follow these steps to see which variant may be suitable for your car:
   1. Select MG EV (TH) and use the car as you would normally. Keep an eye on shell for log prints to see if any problems occur.
   2. If the alarm goes off sometimes (like when doors are locked using remote), try switch to MG EV (UK/EU).
   3. If you do not get any alarm, try put the car into Zombie mode. 
   4. If OVMS is unable to get data (like no charging details appear on the app), try switch to MG EV (UK/EU).

-------------------------
MG ZS EV (2022-) Variants
-------------------------

Currently only the Short Range (51.1kWh) variant is supported as testers only seem to have this model.
The Long Range (72.6kWh) variant can easily be supported if BMS scans can be done.

------------------------
MG5 (2020-2023) Variants
------------------------

Both Short Range (49kWh) and Long Range (57kWh) MG5 Variants are supported and can be selected in the web UI. The Version configuration page can be found in the vehicle menu.

The web UI features a configuration page for Manual Polling in the vehicle menu.
The default setting is for Manual Polling to be on. If this is deselected, the polling will be automatic and will start when the vehicle is in READY or CHARGING and stop automatically when the vehicle is turned off to save the 12V battery. This may cause alarms on the MG5 when locked and charging. If this is a problem, please use Manual Polling.

**Manual Polling** - As the alarm sometimes goes off when the MG5 is locked and charging, the MG5 variant starts up with the polls turned off. Polls can be turned on with the `xmg polls on` command in the shell or via a message in the app. You can also turn on polls by pressing the boot of the car in the app and pressing wakeup. Polls will only start when the car is in ready or charging. When the car is turned off again or charging is turned off, the polls will stop again. Polls can be manually stopped with the `xmg polls off` command.

------------------------
MG4 (2023-) Variants
------------------------

Short Range (51kWh), Medium Range (64kWh) and Long Range (77kWh) are all supported and your version can be selected from MG/Version menu.

**Polling** - As the alarm goes off when the MG4 is locked and charging, this variant starts up with the polls turned off. Polls are automatically enabled when the vehicle is being driven and disabled again when the vehicle is locked. To monitor while charging, the vehicle needs to be left unlocked and a door left ajar (so it does not re-lock). To start polling, the 'xmg polls on' command in the shell or via a message in the app can be used once charging has been started. Polls will automatically be turned off when charging is stopped and it will be safe to lock the car again. If you wish to lock the car after charging is commenced and polls are enabled, the 'xmg polls off' command must be issued.

----------------
Support Overview
----------------

=================================== ==============
Function                            Support Status
=================================== ==============
Hardware                            Any OVMS v3 (or later) module. Vehicle support: 2019-
Vehicle Cable                       Right hand OBDII cable (RHD), Left hand OBDII cable (LHD)
GSM Antenna                         1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                         1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                         Yes
Range Display                       Yes (Calculated using battery capacity, average consumption and temperature)
Cabin Pre-heat/cool Control         No
GPS Location                        Yes (from modem module GPS)
Speed Display                       Yes
Temperature Display                 Yes
BMS v+t Display                     Yes
TPMS Display                        Yes
Charge Status Display               Yes
Charge Interruption Alerts          Yes
Charge Control                      No
Lock/Unlock Vehicle                 No
Valet Mode Control                  No
Others                              Daytime Running Light Control (MG ZS EV TH/AU)
=================================== ==============

--------------
Shell Commands
--------------

Precede all commands with ``xmg`` e.g. ``xmg softver``

MG ZS EV (UK/EU) & MG ZS EV MK2

============================  =
``softver``                   Get software version of ECUs
============================  =

MG ZS EV (TH/AU)

==========================  =
``softver``                 Get software version of ECUs
``drl [on | off]``          Turn on/off daytime running lights
``drln [on | off]``         Turn on/off daytime running lights without BCM authentication first (debug)
``auth [all | gwm | bcm]``  Authenticate with specified ECU 'auth all' will authenticate GWM then BCM
==========================  =

MG5 EV

============================  =
``softver``                   Get software version of ECUs
``polls [on | off]``          Start/ Stop all CAN Bus polling
============================  =

MG4 EV

============================  =
``polls [on | off]``          Start/ Stop all CAN Bus polling
============================  =

------------
Module notes
------------

The code for each vehicle type has these behaviours:

=============  ======================  ==================   ========
Vehicle type   Zombie mode mitigation  GWM authentication   Poll BCM
=============  ======================  ==================   ========
UK/EU          Y                       N                    N                                                     
TH/AU          N                       Y                    Y
=============  ======================  ==================   ========

The MG EV module now monitors (and automatically calibrates) the 12V status and will automatically start polling the car for data when the 12V battery voltage is equal to or greater than 12.9V. When it is below 12.9V, it will automatically stop polling (after a 50s delay) to not drain the 12V battery.

**Poll states:**

=  ==========  =
0  ListenOnly  the OVMS module is quiet and stops sending polls.
1  Charging    the OVMS module sends charging specific queries.
2  Driving     the OVMS module sends driving specific queries.
3  Backup      the OVMS module cannot get data from the car when it is charging so just retries SoC queries. This is unused in TH code.
=  ==========  =

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
MG ZS EV (2019-2021) UK/EU spec
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The car is accessible over the OBD port when it is running (ignition on) and for around
40 seconds after it is turned off or the car is "tweaked" (lock button pushed, etc).

The OBD port may be kept awake by using the "tester present" message to the gateway ECU.
This keeps a lot of systems awake and draws roughly 5A on the 12V bus, so it's not a good
idea to do.
 
The Gateway (GW, GWM) is the keeper of all the data of the car and will enter a locked state 
when it is woken by the car starting charging and the car is locked. 
This we have called "Zombie Mode", and we have developed an override for this. 
 
This override, however causes a few strange things to happen:
 - If Zombie mode override is active, the car will not unlock the charge cable. To fix this dusrupt the charge and wait 50s for OVMS to go back to sleep and the cable should release (or unplug OVMS)
 - Zombie mode override resets the “Accumulated Total Trip” on the Cluster
 - Zombie mode override sets the gearshift LEDs switch on

-----------------
Development notes
-----------------

Developers welcome! Follow the developer's guide on https://www.openvehicles.com/developers to get started! Join our slack group (see below) to discuss the nerdy details!

------------------
Community channels
------------------

| Forum: https://www.mgevs.com/
| Slack: https://mgevhackers.slack.com/
