
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
SOC Display                 Yes
Range Display               Yes
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


----------
Car status
----------

The car is accessible over the OBD port when it is running (ignition on) and for around
40 seconds after it is turned off or the car is "tweaked" (lock button pushed, etc).

The OBD port may be kept awake by using the "tester present" message to the gateway ECU.
This keeps a lot of systems awake and draws roughly 5A on the 12V bus, so it's not a good
idea to do.

If the car is unlocked, the gateway can be woken if the car is unlocked by pinging the
"tester present" message continuously every 100ms until it responds.  If the car is
locked this simply causes the gateway to go into some kind of zombie proxy mode that isn't
very useful.  If the car is locked, the gateway may be woken by sending a session 2
command every 250ms to the gateway.

When the car is charging, the DCDC converter is active, so keeping the electronics awake
isn't really an issue.

In addition, talking to the BCM when the car is locked will cause the car alarm to be set
off unless it is sent the "tester present" frame while it is unlocked and continued to do
so.

We have four different states:

 - Car unlocked but turned off
 - Car unlocked and turned on
 - Car locked
 - Car charging (might be locked)

In each of these states (except the car being turned on) the CAN may be off.  In the case
of the car being locked or the car charging the gateway will turn on in a zombie state and
it will need waking properly in order to get any useful information.  The safest mode to
assume when we detect a CAN wake up is that the car is locked.  It can then transition to
any of the other states depending on information gained.


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
