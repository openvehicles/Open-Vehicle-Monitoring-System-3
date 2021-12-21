=======
Metrics
=======

Metrics are at the heart of the OVMS v3 system. They are strongly typed named 
parameters, with values in specific units (and able to be automatically 
converted to other units). For example, a metric to record the motor temperature 
may be an integer in Celsius units, and may be convertible to Fahrenheit.

The full list of metrics available can be shown::

  OVMS# metrics list
  m.freeram                                4232852
  m.hardware                               OVMS WIFI BLE BT cores=2 rev=ESP32/1
  m.monotonic                              3568Sec
  ...
  v.p.latitude                             22.2809
  v.p.longitude                            114.161
  v.p.odometer                             100000Km
  v.p.satcount                             12
  v.p.speed                                0Kph
  v.p.trip                                 0Km
  v.t.alert                                0,0,0,1
  v.t.health                               95,93,96,74%
  v.t.pressure                             206.843,216.483,275.79,175.79kPa
  v.t.temp                                 33,33,34,38°C
  v.type                                   DEMO

You can filter the ``metrics list`` output for names matching a given substring,
for example ``metrics list volt`` will show all voltage related metrics.

A base OVMS v3 system has more than 100 metrics available (see below), and
vehicle modules can add more for their own uses (see vehicle sections).

In general, vehicle modules (and some other system components) are responsible 
for updating the metrics, and server connections read those metrics, reformat 
them, and send them on to servers and Apps (for eventual display to the user). 
Status commands (such as STAT) also read these metrics and display them in 
user-friendly forms::

  OVMS# stat
  Not charging
  SOC: 50.0%
  Ideal range: 200Km
  Est. range: 160Km
  ODO: 100000.0Km
  CAC: 160.0Ah
  SOH: 100%

For developer use, there are also some other metric commands used to manually 
modify a metric’s value (for testing and simulation purposes), and trace 
changes::

  OVMS# metrics ?
  list                 Show all metrics
  persist              Show persistent metrics info
  set                  Set the value of a metric
  trace                METRIC trace framework

Some metrics are presistent across warm reboots. This prevents
values such as SOC from being lost when firmware is updated (or in
the event of a crash). You can display these with ``metrics list
-p`` and view general information about presistent metrics with
``metrics persist``.

----------------
Standard Metrics
----------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
m.freeram                                3275588                  Total amount of free RAM in bytes
m.hardware                               OVMS WIFI BLE BT…        Base module hardware info
m.monotonic                              49607Sec                 Uptime in seconds
m.net.mdm.iccid                          89490240001766080167     SIM ICCID
m.net.mdm.model                          35316B09SIM5360E         Modem module hardware info
m.net.mdm.network                        congstar                 Current GSM network provider
m.net.mdm.sq                             -101dBm                  …and signal quality
m.net.provider                           WLAN-214677              Current primary network provider
m.net.sq                                 -79dBm                   …signal quality
m.net.type                               wifi                     …and type (none/modem/wifi)
m.net.wifi.network                       WLAN-214677              Current Wifi network SSID
m.net.wifi.sq                            -79.1dBm                 …and signal quality
m.serial                                                          Reserved for module serial no.
m.tasks                                  20                       Task count (use ``module tasks`` to list)
m.time.utc                               1572590910Sec            UTC time in seconds
m.version                                3.2.005-155-g3133466f/…  Firmware version
m.egpio.input                            0,1,2,3,4,5,6,7,9        EGPIO input port state (ports 0…9, present=high)
m.egpio.monitor                          8,9                      EGPIO input monitoring ports
m.egpio.output                           4,5,6,7,9                EGPIO output port state
s.v2.connected                           yes                      yes = V2 (MP) server connected
s.v2.peers                               1                        V2 clients connected
s.v3.connected                                                    yes = V3 (MQTT) server connected
s.v3.peers                                                        V3 clients connected
v.b.12v.current                          0A                       Auxiliary 12V battery momentary current
v.b.12v.voltage                          12.29V                   Auxiliary 12V battery momentary voltage
v.b.12v.voltage.alert                                             yes = auxiliary battery under voltage alert
v.b.12v.voltage.ref                      12.3V                    Auxiliary 12V battery reference voltage
v.b.c.temp                               13,13,…,13°C             Cell temperatures
v.b.c.temp.alert                         0,0,…,0                  Cell temperature deviation alert level [0=normal, 1=warning, 2=alert]
v.b.c.temp.dev.max                       1.43,0.86,…,-1.29°C      Cell maximum temperature deviation observed
v.b.c.temp.max                           19,18,…,17°C             Cell maximum temperatures
v.b.c.temp.min                           13,12,…,12°C             Cell minimum temperatures
v.b.c.voltage                            4.105,4.095,…,4.105V     Cell voltages
v.b.c.voltage.alert                      0,0,…,0                  Cell voltage deviation alert level [0=normal, 1=warning, 2=alert]
v.b.c.voltage.dev.max                    0.0096,-0.0104,…,0.0125V Cell maximum voltage deviation observed
v.b.c.voltage.max                        4.135,4.125,…,4.14V      Cell maximum voltages
v.b.c.voltage.min                        3.875,3.865,…,3.88V      Cell minimum voltages
v.b.cac                                  90.7796Ah                Calculated battery pack capacity
v.b.consumption                          0Wh/km                   Main battery momentary consumption
v.b.coulomb.recd                         47.5386Ah                Main battery coulomb recovered on trip/charge
v.b.coulomb.recd.total                   947.5386Ah               Main battery coulomb recovered total (life time)
v.b.coulomb.used                         0.406013Ah               Main battery coulomb used on trip
v.b.coulomb.used.total                   835.406013Ah             Main battery coulomb used total (life time)
v.b.current                              0A                       Main battery momentary current (output=positive)
v.b.energy.recd                          2.69691kWh               Main battery energy recovered on trip/charge
v.b.energy.recd.total                    3212.69691kWh            Main battery energy recovered total (life time)
v.b.energy.used                          0.0209496kWh             Main battery energy used on trip
v.b.energy.used.total                    3177.0209496kWh          Main battery energy used total (life time)
v.b.health                                                        General textual description of battery health
v.b.p.level.avg                          95.897%                  Cell level - pack average
v.b.p.level.max                          96.41%                   Cell level - strongest cell in pack
v.b.p.level.min                          94.871%                  Cell level - weakest cell in pack
v.b.p.level.stddev                       0.548%                   Cell level - pack standard deviation
v.b.p.temp.avg                           13°C                     Cell temperature - pack average
v.b.p.temp.max                           13°C                     Cell temperature - warmest cell in pack
v.b.p.temp.min                           13°C                     Cell temperature - coldest cell in pack
v.b.p.temp.stddev                        0°C                      Cell temperature - current standard deviation
v.b.p.temp.stddev.max                    0.73°C                   Cell temperature - maximum standard deviation observed
v.b.p.voltage.avg                        4.1V                     Cell voltage - pack average
v.b.p.voltage.grad                       0.0032V                  Cell voltage - gradient of current series
v.b.p.voltage.max                        4.105V                   Cell voltage - strongest cell in pack
v.b.p.voltage.min                        4.09V                    Cell voltage - weakest cell in pack
v.b.p.voltage.stddev                     0.00535V                 Cell voltage - current standard deviation
v.b.p.voltage.stddev.max                 0.00783V                 Cell voltage - maximum standard deviation observed
v.b.power                                0kW                      Main battery momentary power (output=positive)
v.b.range.est                            99km                     Estimated range
v.b.range.full                           50.8km                   Ideal range at 100% SOC & current conditions
v.b.range.ideal                          48km                     Ideal range
v.b.range.speed                          21.6km/h                 Momentary ideal range gain/loss (charge/discharge) speed
v.b.soc                                  96.3%                    State of charge
v.b.soh                                  85%                      State of health
v.b.temp                                 13°C                     Main battery momentary temperature
v.b.voltage                              57.4V                    Main battery momentary voltage
v.c.12v.current                          7.8A                     Output current of DC/DC-converter
v.c.12v.power                            123W                     Output power of DC/DC-converter
v.c.12v.temp                             34.5°C                   Temperature of DC/DC-converter
v.c.12v.voltage                          12.3V                    Output voltage of DC/DC-converter
v.c.charging                             no                       yes = currently charging
v.c.climit                               0A                       Maximum charger output current
v.c.current                              1.25A                    Momentary charger output current
v.c.duration.full                        25Min                    Estimated time remaing for full charge
v.c.duration.range                       -1Min                    … for sufficient range
v.c.duration.soc                         0Min                     … for sufficient SOC
v.c.efficiency                           87.6%                    Momentary charger efficiency
v.c.kwh                                  2.6969kWh                Energy sum for running charge
v.c.kwh.grid                             3.6969kWh                Energy drawn from grid during running session
v.c.kwh.grid.total                       256.69kWh                Energy drawn from grid total (life time)
v.c.limit.range                          0km                      Sufficient range limit for current charge
v.c.limit.soc                            80%                      Sufficient SOC limit for current charge
v.c.mode                                 standard                 standard, range, performance, storage
v.c.pilot                                no                       Pilot signal present
v.c.power                                125kW                    Momentary charger input power
v.c.state                                done                     charging, topoff, done, prepare, timerwait, heating, stopped
v.c.substate                                                      scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted
v.c.temp                                 16°C                     Charger temperature
v.c.time                                 0Sec                     Duration of running charge
v.c.timermode                                                     yes = timer enabled
v.c.timerstart                                                    Time timer is due to start, seconds since midnight UTC
v.c.type                                                          undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
v.c.voltage                              0V                       Momentary charger supply voltage
v.d.cp                                   yes                      yes = Charge port open
v.d.fl                                                            yes = Front left door open
v.d.fr                                                            yes = Front right door open
v.d.hood                                                          yes = Hood/frunk open
v.d.rl                                                            yes = Rear left door open
v.d.rr                                                            yes = Rear right door open
v.d.trunk                                                         yes = Trunk open
v.e.alarm                                                         yes = Alarm currently sounding
v.e.aux12v                                                        yes = 12V auxiliary system is on (base system awake)
v.e.awake                                no                       yes = Vehicle is fully awake (switched on by the user)
v.e.c.config                                                      yes = ECU/controller in configuration state
v.e.c.login                                                       yes = Module logged in at ECU/controller
v.e.cabintemp                            20°C                     Cabin temperature
v.e.cabinfan                             100%                     Cabin fan
v.e.cabinsetpoint                        24°C                     Cabin set point
v.e.cabinintake                          fresh                    Cabin intake type (fresh, recirc, etc)
v.e.cabinvent                            feet,face                Cabin vent type (comma-separated list of feet, face, screen, etc)
v.e.charging12v                          no                       yes = 12V battery is charging
v.e.cooling                                                       yes = Cooling
v.e.drivemode                            33882626                 Active drive profile code (vehicle specific)
v.e.drivetime                            0Sec                     Seconds driving (turned on)
v.e.footbrake                            0%                       Brake pedal state [%]
v.e.gear                                                          Gear/direction; negative=reverse, 0=neutral
v.e.handbrake                                                     yes = Handbrake engaged
v.e.headlights                                                    yes = Headlights on
v.e.heating                                                       yes = Heating
v.e.hvac                                                          yes = HVAC active
v.e.locked                                                        yes = Vehicle locked
v.e.on                                   no                       yes = Vehicle is in "ignition" state (drivable)
v.e.parktime                             49608Sec                 Seconds parking (turned off)
v.e.regenbrake                                                    yes = Regenerative braking active
v.e.serv.range                           12345km                  Distance to next scheduled maintenance/service [km]
v.e.serv.time                            1572590910Sec            Time of next scheduled maintenance/service [Seconds]
v.e.temp                                                          Ambient temperature
v.e.throttle                             0%                       Drive pedal state [%]
v.e.valet                                                         yes = Valet mode engaged
v.g.generating                           no                       True = currently delivering power
v.g.climit                               0A                       Maximum generator input current (from battery)
v.g.current                              1.25A                    Momentary generator input current (from battery)
v.g.duration.empty                       25Min                    Estimated time remaining for full discharge
v.g.duration.range                       -1Min                    … for range limit
v.g.duration.soc                         0Min                     … for SOC limit
v.g.efficiency                           87.6%                    Momentary generator efficiency
v.g.kwh                                  2.6969kWh                Energy sum generated in the running session
v.g.kwh.grid                             3.6969kWh                Energy sent to grid during running session
v.g.kwh.grid.total                       256.69kWh                Energy sent to grid total
v.g.limit.range                          0km                      Minimum range limit for generator mode
v.g.limit.soc                            80%                      Minimum SOC limit for generator mode
v.g.mode                                 standard                 Generator mode (TBD)
v.g.pilot                                no                       Pilot signal present
v.g.power                                125kW                    Momentary generator output power
v.g.state                                done                     Generator state (TBD)
v.g.substate                                                      Generator substate (TBD)
v.g.temp                                 16°C                     Generator temperature
v.g.time                                 0Sec                     Duration of generator running
v.g.timermode                            false                    True if generator timer enabled 
v.g.timerstart                                                    Time generator is due to start 
v.g.type                                                          Connection type (chademo, ccs, …)
v.g.voltage                              0V                       Momentary generator output voltage
v.i.temp                                                          Inverter temperature
v.i.power                                42.7kW                   Momentary inverter motor power (output=positive)
v.i.efficiency                           98.2%                    Momentary inverter efficiency
v.m.rpm                                                           Motor speed (RPM)
v.m.temp                                 0°C                      Motor temperature
v.p.acceleration                         0m/s²                    Vehicle acceleration
v.p.altitude                             327.8m                   GPS altitude
v.p.direction                            31.2°                    GPS direction
v.p.gpshdop                              1.3                      GPS horizontal dilution of precision (smaller=better)
v.p.gpslock                              no                       yes = has GPS satellite lock
v.p.gpsmode                              AA                       <GPS><GLONASS>; N/A/D/E (None/Autonomous/Differential/Estimated)
v.p.gpsspeed                             0km/h                    GPS speed over ground
v.p.latitude                             51.3023                  GPS latitude
v.p.location                             Home                     Name of current location if defined
v.p.longitude                            7.39006                  GPS longitude
v.p.odometer                             57913.1km                Vehicle odometer
v.p.satcount                             8                        GPS satellite count in view
v.p.speed                                0km/h                    Vehicle speed
v.p.trip                                 0km                      Trip odometer
v.t.alert                                0,0,0,1                  TPMS tyre alert levels [0=normal, 1=warning, 2=alert]
v.t.health                               95,93,96,74%             TPMS tyre health states
v.t.pressure                             206.8,216.4,…kPa         TPMS tyre pressures
v.t.temp                                 33,33,34,38°C            TPMS tyre temperatures
v.type                                   RT                       Vehicle type code
v.vin                                    VF1ACVYB012345678        Vehicle identification number
======================================== ======================== ============================================
