
=============
BMW i3 / i3s
=============

Vehicle Type: **BMWI3**

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

--------
WARNINGS
--------

Alarm behaviour
^^^^^^^^^^^^^^^

As standard, the i3 will sound the alarm if anything is left connected to the OBD-II
port when the car is locked.

A tool like Bimmercode will allow you to disable this. Alternatively
you will need to disconnect the OVMS unit before locking the car.

A future version may add a command to allow you to disable this alarm directly
from your OVMS shell.

12V Battery drain
^^^^^^^^^^^^^^^^^

The i3 has a small 20Ah AGM 12v battery. Whilst care has been taken to minimize OVMS' power usage,
OVMS could eventually drain this battery if the car is left unplugged and locked.
OVMS will also send an alert if 12V drops under 12V alert threshold. (See 12V Calibration section).

HOWEVER: If you are going to leave the car for a fews days, it is recommended to unplug OVMS.

----------
Car status
----------

The car is accessible over the OBD-II port when it is running (ignition on) and for a short time
(40 seconds or so) after it is turned off or the car is "tweaked" (lock button pushed,
connected-drive command received, etc).

Unfortunately this means that when your car is standing or charging OVMS only has
intermittent access to data from the car.  

By observation, whilst the car is charging it wakes up now and then (seems to be every 30 minutes).
So at those times we can update our SOC etc.

Metrics "v.e.awake" tells you if the car is awake or not.  Metric "xi3.s.age" will tell you how
many minutes have passed since we last received data from the car.

You may also refer to metric xi3.s.pollermode as follows:

==== ================================================
Mode Meaning
==== ================================================
 0   Car is asleep - no OBD-II data traffic
 1   Car OBD-II is awake - we are seeing data traffic
 2   Car is ready to drive or driving
 3   Car is charging
==== ================================================

-------------------
Custom metrics
-------------------

======================================== =================== =====================================================================================================
Metric name                              Example value       Description
======================================== =================== =====================================================================================================
xi3.s.age                                5Min                How long since we last got data from the car
xi3.s.pollermode                         0                   OBD-II polling mode as explained above
xi3.v.b.p.ocv.avg                        4.0646V             Main battery pack - average open-circuit voltage
xi3.v.b.p.ocv.max                        4.067V              Main battery pack - highest open-circuit voltage
xi3.v.b.p.ocv.min                        4.063V              Main battery pack - lowest open-circuit voltage
xi3.v.b.range.bc                         245km               Available range per trip computer (based on current driving mode and style)
xi3.v.b.range.comfort                    217km               Available range if you use Comfort mode
xi3.v.b.range.ecopro                     245km               Available range if you use EcoPro mode
xi3.v.b.range.ecoproplus                 247km               Available range if you use EcoPro+ mode
xi3.v.b.soc.actual                       85%                 Actual physical state-of-charge of the main battery pack
xi3.v.b.soc.actual.highlimit             93.7%               Highest physical charge level permitted (shown as 100% SOC)
xi3.v.b.soc.actual.lowlimit              10.5%               Minimum physical charge level permitted (shown as 0% SOC)
xi3.v.c.chargecablecapacity              0A                  Maximum power capacity of connected charge cable per the charging interface
xi3.v.c.chargeledstate                   0                   Colour of the "ring light" on the charging interface.
xi3.v.c.chargeplugstatus                 Not connected       Charging cable connected?
xi3.v.c.current.dc                       0A                  Power flowing on the DC side of the AC charger
xi3.v.c.current.dc.limit                 0.100003A           Limit
xi3.v.c.current.dc.maxlimit              16A                 Maximum limit
xi3.v.c.current.phase1                   0A                  Power being drawn on AC phase 1
xi3.v.c.current.phase2                   0A                  Power being drawn on AC phase 2
xi3.v.c.current.phase3                   0A                  Power being drawn on AC phase 3
xi3.v.c.dc.chargevoltage                 0V                  Voltage seen on the DC charger input
xi3.v.c.dc.contactorstatus               open                DC contactor state (closed implies we are DC charging)
xi3.v.c.dc.controlsignals                0                   DC charger control signals (always see 0?)
xi3.v.c.dc.inprogress                    no                  DC charging in progress?
xi3.v.c.dc.plugconnected                 no                  Is DC charger plug connected (doesn't seem to work)
xi3.v.c.deratingreasons                  0                   Reasons why charging rate is derated
xi3.v.c.error                            0                   Charging error codes
xi3.v.c.failsafetriggers                 0                   Failsafe trigger reasons
xi3.v.c.interruptionreasons              0                   Charging interruption reasons
xi3.v.c.pilotsignal                      0A                  Charge rate pilot signal being received from EVSE
xi3.v.c.readytocharge                    no                  Are we ready to charge
xi3.v.c.temp.gatedriver                  40°C                Charger gatedrive mosfet temperature
xi3.v.c.voltage.dc                       8.4V                Charger output DC voltage being seen (for AC charging, not DC)
xi3.v.c.voltage.dc.limit                 420V                Maximum permitted DC voltge
xi3.v.c.voltage.phase1                   0V                  Voltage seen on AC charger input phase 1
xi3.v.c.voltage.phase2                   0V                  Voltage seen on AC charger input phase 2
xi3.v.c.voltage.phase3                   0V                  Voltage seen on AC charger input phase 3
xi3.v.d.chargeport.dc                    no                  Is the charger port DC cover open (doesn't seem to work)
xi3.v.e.autorecirc                       no                  Ventilation is in "auto-recirculate" mode
xi3.v.e.obdtraffic                       no                  Are we seeing OBD-II frames from the car?
xi3.v.p.tripconsumption                  127Wh/km            Average consumption for the current or most recent trip
xi3.v.p.wheel1_speed                     0km/h               Wheel 1 speed
xi3.v.p.wheel2_speed                     0km/h               Wheel 2 speed
xi3.v.p.wheel3_speed                     0km/h               Wheel 3 speed
xi3.v.p.wheel4_speed                     0km/h               Wheel 4 speed
xi3.v.p.wheel_speed                      0km/h               Average wheel speed
======================================== =================== =====================================================================================================

----------------
To be researched
----------------

Can we start/stop charging?

Can we pre-heat?

Can we lock/unlock the car?

Can we disable the OBD-II alarm

Still looking for the trip regen kWh

Can we get the voltage state of each individual cells rather than just the battery min / max / average?

