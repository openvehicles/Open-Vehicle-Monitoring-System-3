===============
Hyundai Ioniq 5
===============

**Hyundai Ioniq 5**

- Vehicle Type: **HION5**
- Log tag: ``v-ioniq5``
- Namespace: ``xiq``
- Maintainers: `Michael Geddes <frog@bunyip.wheelycreek.net>`_
- Credits: `EVNotify <https://github.com/EVNotify>`_


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
Range Display               No
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
xiq.m.version                            0.0.1 10/09/2022 10:23   Version of Module
xiq.b.bms.soc                            78.5%                    Internal BMS SOC

xiq.v.b.c.voltage.max                    10.0V                    Battery Cell Volt Max
xiq.v.b.c.voltage.min                    10.0V                    Battery Cell Volt Min
xiq.v.b.c.voltage.max.no                 123450                   Battery Cell Volt Max No
xiq.v.b.c.voltage.min.no                 123450                   Battery Cell Volt Min No
xiq.v.b.c.det.min                        12.1%                    Battery Cell Det Min
xiq.v.b.c.det.max.no                     123450                   Battery Cell Det Max No
xiq.v.b.c.det.min.no                     123450                   Battery Cell Det Min No
xiq.c.power                              12345kW                  Power
xiq.c.speed                              109Kph                   Speed
xiq.b.min.temp                           36°C                     Battery Min Temperature
xiq.b.max.temp                           36°C                     Battery Max Temperature
xiq.b.inlet.temp                         36°C                     Battery Inlet Temperature
xiq.b.heat1.temp                         36°C                     Battery Heat 1 Temperature
xiq.b.bms.soc                            12.1%                    Battery Bms Soc
xiq.b.aux.soc                            78%                      Battery Aux Soc
xiq.b.bms.relay                          false                    Battery Bms Relay             
xiq.b.bms.ignition                       false                    Battery Bms Ignition             
xiq.b.bms.power.avail                    7680W                    BMS Available power for charging
xiq.ldc.out.volt                         10.0V                    Low Voltage DC Conv Out Voltage
xiq.ldc.in.volt                          10.0V                    Low Voltage DC Conv In Voltage
xiq.ldc.out.amps                         8.2A                     Low Voltage DC Conv Out Current
xiq.ldc.temp                             8.2°C                    Low Voltage DC Conv Temperature
xiq.obc.pilot.duty                       12.1%                    OBC Pilot Duty
xiq.obc.timer.enabled                    false                    OBC Timer Enabled             
xiq.e.lowbeam                            false                    Env Lowbeam             
xiq.e.highbeam                           false                    Env Highbeam             
xiq.e.preheat.timer1.enabled             false                    Preheat Timer1 Enabled             
xiq.e.preheat.timer2.enabled             false                    Preheat Timer2 Enabled             
xiq.e.preheating                         false                    Preheating             
xiq.e.heated.steering                    false                    Heated Steering Wheel             
xiq.e.rear.defogger                      false                    Rear Defogger             
xiq.v.traction.control                   false                    Traction Control             
xiq.e.trip                               12345Km                  Trip: Distance Travelled since Charged
xiq.e.trip.energy.used                   12345kWh                 Trip: Energy Used since Charged
xiq.e.trip.energy.recuperated            12345kWh                 Trip: Energy Recuperated since Charged
xiq.v.trip.consumption.KWh/100km         9.5                      Trip: Power Consumption (kwH/100km) since Charged
xiq.v.trip.consumption.km/kWh            1234                     Trip: Power Consumption (km/kWh) since Charged
xiq.v.sb.driver                          false                    Seat Belt Driver             
xiq.v.sb.passenger                       false                    Seat Belt Passenger             
xiq.v.sb.back.right                      false                    Seat Belt Back Right             
xiq.v.sb.back.middle                     false                    Seat Belt Back Middle             
xiq.v.sb.back.left                       false                    Seat Belt Back Left             
xiq.v.emergency.lights                   false                    Emergency Lights             
xiq.v.d.l.fl                             false                    Door Locked - Front Left             
xiq.v.d.l.fr                             false                    Door Locked - Front Right             
xiq.v.d.l.rl                             false                    Door Locked - Rear Left             
xiq.v.d.l.rr                             false                    Door Locked - Rear Right

======================================== ======================== ============================================


----------
Debug Logs
----------

To see all PID poll results in your log, set log level ``verbose`` for component ``v-ioniq5``.

