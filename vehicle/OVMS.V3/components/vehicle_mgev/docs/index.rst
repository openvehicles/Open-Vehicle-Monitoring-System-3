
=====
MG EV
=====

Vehicle Type: **MGEV**

This vehicle type supports the MG ZS EV (2019-).


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
SOC Display                 Yes (unless car woken by charger)
Range Display               Yes
Cabin Pre-heat/cool Control tba
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                Pressure only
Charge Status Display       Yes (unless car woken by charger)
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


----------
Car status
----------

The car is accessible over the OBD port when it is running (ignition on) and for around
40 seconds after it is turned off or the car is "tweaked" (lock button pushed, etc).

The OBD port may be kept awake by using the "tester present" message to the gateway ECU.
This keeps a lot of systems awake and draws roughly 5A on the 12V bus, so it's not a good
idea to do.  The gateway can be woken if the car is unlocked by pinging the "tester
present" message continuously every 100ms until it responds.  If the car is locked this
simply causes the gateway to go into some kind of zombie proxy mode that isn't very
useful.

When the car is charging, the DCDC converter is active, so keeping the electronics awake
isn't really an issue.

Given all of this information it is safe to talk to the gateway:

 - When the car is running
 - When the car is charging

If the car is not running or charging we need to stop querying for the status in order to
preserve the 12V battery.  When it is running we do not need to keep the car awake.  When
it is charging we need to keep the car awake.  When transitioning between modes the car
will wake up by itself, we can get a notification of this by pinging for the BMS status.

In addition, if the BCM is queried when the car is in the locked state, then the alarm
will be activated.  This is important, so we will only query this ECU when the car is
running.  This can be mitigated by sending the "tester present" message which stops this
from happening, but has the side effect of stopping the car from going to sleep.

The states that we will poll in are therefore made up of:

 - Car is running
 - Car is charging (may be locked or unlocked)
 - Car is awake but not running or charging
 - Car is asleep

We can easily transition to charging when the car states that it's charging and to running
when it is turned on.  We need to be in the awake mode when the accessory is turned on and
then transition from it to asleep after some time.


----------------
To be researched
----------------

Can we start/stop charging?

Can we pre-heat?

Can we lock/unlock the car?

Can we get the voltage state of the battery cells rather than just the battery?

Can we determine the charger voltage and current?

Can we determine how many kWh have been delivered in the charge session?  We know when
it starts and ends and the SoC, so we could infer it?

Can we read the SoC if the car has been woken by the charger rather than starting
immediately?
