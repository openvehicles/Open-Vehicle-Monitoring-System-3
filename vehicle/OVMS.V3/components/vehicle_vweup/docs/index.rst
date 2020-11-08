
========
VW e-Up
========

Vehicle Type: **VWUP**

This vehicle type supports the VW e-UP (2013-, 2020-), Skoda Citigo E IV and the Seat MII electric (2020-).
Connection can be made via the Comfort CAN bus, e.g. below the passenger seat (instead of the VW OCU there) and/or the OBD2 port to the top left of the driving pedals.

The code is shamelessly copied from the individual projects for the Comfort CAN by Chris van der Meijden and for the OBD2 port by Soko. Additional metrics and displays were added mainly for OBD2.

For details on the two connection types, please see the corresponding projects:
Comfort CAN: `https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/vehicle_vweup/docs/index.rst <https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/vehicle_vweup/docs/index.rst>`_
OBD2: `https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/vehicle_vweup_obd/docs/index.rst <https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/vehicle_vweup_obd/docs/index.rst>`_


----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module. Vehicle support: 2020- (2013- VW e-Up as well)
Vehicle Cable               Comfort CAN T26A (OCU connector cable, located under front passenger seat) to DB9 Data Cable for OVMS using pin 6 and 8 for can3 _AND_ OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left) for can1
GSM Antenna                 T4AC - R205 with fakra_sma adapter cable or 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 T4AC - R50 with fakra_sma adapter cable or 1020200 Universal GPS Antenna (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
Cabin Pre-heat/cool Control Yes
GPS Location                Yes (from modem module GPS)
Speed Display               Yes
Temperature Display         Yes (see list of metrics below)
BMS v+t Display             Yes
TPMS Display                tba
Charge Status Display       Yes
Charge Interruption Alerts  Yes (per notification on the charging state)
Charge Control              tba
Lock/Unlock Vehicle         tba
Valet Mode Control          tba
Others                      **See list of metrics below**
=========================== ==============


--------------------------
Supported Standard Metrics
--------------------------

The second column specifies the bus from which the metrics are obtained. Metrics via OBD are only updated when the vehicle is on (ignition started) or some in charging mode.
Metrics via T26 (Comfort CAN) can be updated on demand by waking the Comfort CAN from the OVMS module. During charging, the Comfort CAN automatically wakes every 5% of SoC.

============================= ========== ======================== ============================================
Metric name                   bus        Example value            Description
============================= ========== ======================== ============================================
v.b.12v.voltage               direct     12.9 V                   Current voltage of the 12V battery
v.b.consumption               OBD        0Wh/km                   Main battery momentary consumption
v.b.current                   OBD       23.2 A                   Current current into (negative) or out of (positive) the main battery
v.b.energy.recd.total         OBD        578.323 kWh              Energy recovered total (life time) of the main battery (charging and recuperation)
v.b.energy.used.total         OBD        540.342 kWh              Energy used total (life time) of the main battery
v.b.power                     OBD        23.234 kW                Current power into (negative) or out of (positive) the main battery.
v.b.range.est                 T26        99km                     Estimated range
v.b.range.ideal               T26        48km                     Ideal range
v.b.soc                       OBD, T26   88.2 %                   Current usable State of Charge (SoC) of the main battery
v.b.temp                      OBD        22.5 °C                  Current temperature of the main battery
v.b.voltage                   OBD        320.2 V                  Current voltage of the main battery
v.c.charging                  T26        true                     Is vehicle charging (true = "Vehicle CHARGING" state. v.e.on=false if this is true)
v.c.efficiency                OBD        91.3 %                   Charging efficiency calculated by v.b.power and v.c.power
v.c.mode                      T26        standard                 standard, range, performance, storage
v.c.pilot                     T26        no                       Pilot signal present
v.c.power                     OBD        7.345 kW                 Input power of charger
v.c.state                     T26        done                     charging, topoff, done, prepare, timerwait, heating, stopped
v.c.substate                  T26                                 scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted
v.c.temp                      OBD        16°C                     Charger temperature
v.c.time                      T26        0Sec                     Duration of running charge
v.d.cp                        T26        yes                      yes = Charge port open
v.d.fl                        T26                                 yes = Front left door open
v.d.fr                        T26                                 yes = Front right door open
v.d.hood                      T26                                 yes = Hood/frunk open
v.d.rl                        T26                                 yes = Rear left door open
v.d.rr                        T26                                 yes = Rear right door open
v.d.trunk                     T26                                 yes = Trunk open
v.e.awake                     T26        no                       yes = Vehicle/bus awake (switched on)
v.e.cabintemp                 T26        20°C                     Cabin temperature
v.e.drivetime                 T26        0Sec                     Seconds driving (turned on)
v.e.headlights                T26                                 yes = Headlights on
v.e.hvac                      T26                                 yes = HVAC active
v.e.locked                    T26                                 yes = Vehicle locked
v.e.on                        T26        true                     Is ignition on and drivable (true = "Vehicle ON", false = "Vehicle OFF" state)
v.e.parktime                  T26        49608Sec                 Seconds parking (turned off)
v.e.temp                      OBD, T26                            Ambient temperature
v.i.temp                      OBD                                 Inverter temperature
v.m.temp                      OBD        0°C                      Motor temperature
v.p.odometer                  T26        2340 km                  Total distance traveled
v.p.speed                     T26        0km/h                    Vehicle speed
v.vin                         T26        VF1ACVYB012345678        Vehicle identification number
============================= ========== ======================== ============================================

*) Also updated in state "Vehicle OFF"


--------------
Custom Metrics
--------------

In addition to the standard metrics above the following custom metrics are read from the car or internally calculated by OVMS using read values.

============================= ========== ======================== ============================================
Metric name                   bus        Example value            Description
============================= ========== ======================== ============================================
xvu.b.cell.delta              OBD        0.012 V                  Delta voltage between lowest and highest cell voltage
xvu.b.soc.abs                 OBD        85.3 %                   Current absolute State of Charge (SoC) of the main battery
xvu.c.ac.p                    OBD        7.223 kW                 Current charging power on AC side (calculated by ECU's AC voltages and AC currents)
xvu.c.dc.p                    OBD        6.500 kW                 Current charging power on DC side (calculated by ECU's DC voltages and DC currents)
xvu.c.eff.calc                OBD        90.0 %                   Charger efficiency calculated by AC and DC power
xvu.c.eff.ecu*                OBD        92.3 %                   Charger efficiency reported by the Charger ECU
xvu.c.loss.calc               OBD        0.733 kW                 Charger power loss calculated by AC and DC power
xvu.c.loss.ecu*               OBD        0.620 kW                 Charger power loss reported by the Charger ECU
xvu.v.m.d                     OBD        12500 km                 Distance to next scheduled maintenance
xvu.v.m.t                     OBD        123 days                 Time to next scheduled maintenance
============================= ========== ======================== ============================================

*) Only supplied by ECU when the car ignition is on during charging.


