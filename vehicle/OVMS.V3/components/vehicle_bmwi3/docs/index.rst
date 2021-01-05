
=============
BMW i3 / i3s
=============

Vehicle Type: **I3**

This vehicle type supports the BMW i3 and i3s models. All model years should be supported.

The OVMS support was developed Jan 2021.

It was developed against the author's 2018 120Ah i3s BEV.  I would welcome engagement from
the owner of a REX type to further develop metrics related to the REX engine. Testing by
drivers of LHD models, as well as those with the smaller batteries will also be helpful.

As of this release this vehicle support is read-only and cannot send commands to the car.

----------------
Support Overview
----------------

"tba" item are still on the to-do list and are not currently supported.

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module.
Vehicle Cable               OBD-II cable: left-handed cable worked best in my RHD car
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
Cabin Pre-heat/cool Control tba
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes
TPMS Display                tba
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          No
Others                      12v battery voltage/current, battery true SOC, etc
=========================== ==============

-------
WARNING
-------

As standard the I3 will sound the alarm if anything is left connected to the OBD-II
port when the car is locked

A tool like Bimmercode will allow you to disable this. Alternatively
you will need to discconnect the OVMS unit before locking the car.

A future version may add a command to allow you to disable this alarm directly
from your OVMS shell.

----------
Car status
----------

The car is accessible over the OBD-II port when it is running (ignition on) and for a short time
(40 seconds or so) after it is turned off or the car is "tweaked" (lock button pushed,
connected-drive command received, etc).

Unfortunately this means that when your car is standing or charging OVMS cannot consistently get data 
from the car.  Metrics "v.e.awake" tells you if the car is awake or not.

You may also refer to metrics xi3.s.pollermode as follows:

==== ================================================
Mode Meaning
==== ================================================
 0   Car is not asleep - no OBD-II data traffic
 1   Car OBD-II is awake - we are seeing data traffic
 2   Car is ready to drive or driving
 3   Car is charging
==== ================================================

----------------
To be researched
----------------

Can we start/stop charging?

Can we pre-heat?

Can we lock/unlock the car?

Can we disable the OBD-II alarm

Still looking for the trip regen kWh

Can we get the voltage state of each individual cells rather than just the battery min / max / average?

