--------------
Custom Metrics
--------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xse.mybms.HW.rev                         12,38,0                  BMS hardware-revision year, week, patchlevel
xse.mybms.SW.rev                         12,35,1                  BMS soft-revision year, week, patchlevel
xse.mybms.adc.cvolts.max                 4171                     maximum cell voltages in mV, add offset +1500
xse.mybms.adc.cvolts.mean                4165                     average cell voltage in mV, no offset
xse.mybms.adc.cvolts.min                 4145                     minimum cell voltages in mV, add offset +1500
xse.mybms.adc.volts.offset               103                      calculated offset between RAW cell voltages and ADCref, about 90mV
xse.mybms.amps                           65483A                   battery current in ampere (x/32) reported by by BMS
xse.mybms.amps2                          2046.34A                 battery current in ampere read by live data on CAN or from BMS
xse.mybms.batt.vin                       WME4513901K661441        VIN stored in BMS
xse.mybms.dc.fault                       0                        Flag to show DC-isolation fault
xse.mybms.hv                                                      total voltage of HV system in V
xse.mybms.hv.contact.cycles.left         281991                   counter related to ON/OFF cyles of the car
xse.mybms.hv.contact.cycles.max          300000                   static, seems to be maxiumum of contactor cycles 
xse.mybms.isolation                      10239                    Isolation in DC path, resistance in kOhm
xse.mybms.power                          794.186kW                power as product of voltage and amps in kW
xse.v.b.c.capacity                       18491,18536,18536,...    Cell capacity [As]
xse.v.b.c.capacity.dev.max               45.1,90.1,90.1,...       Cell maximum capacity deviation observed [As]
xse.v.b.c.capacity.max                   18491,18536,18536,...    Cell maximum capacity [As]
xse.v.b.c.capacity.min                   18491,18536,18536,...    Cell minimum capacity [As]
xse.v.b.energy.used.reset                16.34kWh                 Energy used Reset (Dashbord)
xse.v.b.energy.used.start                16.9kWh                  Energy used Start (Dashbord)
xse.v.b.hv.active                        no                       HV Batterie Status
xse.v.b.p.capacity.as.average            18625                    cell capacity statistics from BMS measurement cycle
xse.v.b.p.capacity.as.maximum            19193                    cell capacity statistics from BMS measurement cycle
xse.v.b.p.capacity.as.minimum            18843                    cell capacity statistics from BMS measurement cycle
xse.v.b.p.capacity.avg                   18445.9                  Cell capacity - pack average [As]
xse.v.b.p.capacity.combined.quality      0.515328                 some sort of estimation factor??? constantly updated
xse.v.b.p.capacity.max                   18670                    Cell capacity - strongest cell in pack [As]
xse.v.b.p.capacity.max.cell              9                        Cell capacity - number of strongest cell in pack
xse.v.b.p.capacity.min                   18229                    Cell capacity - weakest cell in pack [As]
xse.v.b.p.capacity.min.cell              59                       Cell capacity - number of weakest cell in pack
xse.v.b.p.capacity.quality               0.742092                 some sort of estimation factor??? after measurement cycle
xse.v.b.p.capacity.stddev                90.8                     Cell capacity - current standard deviation [As]
xse.v.b.p.capacity.stddev.max            90.8                     Cell capacity - maximum standard deviation observed [As]
xse.v.b.p.hv.lowcurrent                  0Sec                     counter time of no current, reset e.g. with PLC heater or driving
xse.v.b.p.hv.off.time                    0Sec                     HighVoltage contactor off time in seconds
xse.v.b.p.last.meas.days                 13                       days elapsed since last successful measurement
xse.v.b.p.ocv.timer                      2676Sec                  counter time in seconds to reach OCV state
xse.v.b.p.voltage.max.cell               1                        Cell volatage - number of strongest cell in pack
xse.v.b.p.voltage.min.cell               5                        Cell volatage - number of weakest cell in pack
xse.v.b.real.soc                         97.8%                    real state of charge
xse.v.bus.awake                          no                       CAN-Bus Status
xse.v.c.active                           no                       Charging Status
xse.v.display.time                       738Min                   Dashbord Time
xse.v.display.trip.reset                 2733.2km                 Dashbord Trip value at Reset
xse.v.display.trip.start                 17.5km                   Dashbord Trip value at Start
xse.v.nlg6.amps.cablecode                0A                       Onboard Charger Ampere Cable
xse.v.nlg6.amps.chargingpoint            0A                       Onboard Charger Ampere ...
xse.v.nlg6.amps.setpoint                 41A                      Onboard Charger Ampere setpoint
xse.v.nlg6.dc.current                    0A                       Onboard Charger LV current Ampere
xse.v.nlg6.dc.hv                         0V                       Onboard Charger HV Voltage
xse.v.nlg6.dc.lv                         13.5V                    Onboard Charger LV Voltage
xse.v.nlg6.main.amps                     0,0,0A                   Onboard Charger main Phasen Ampere
xse.v.nlg6.main.volts                    0,0,0V                   Onboard Charger main Phasen Voltage
xse.v.nlg6.pn.hw                         4519820621               Onboard Charger Hardware Serial No.
xse.v.nlg6.present                       no                       Onboard Charger OBL or NLG6 present
xse.v.nlg6.temp.coolingplate             0°C                      Onboard Charger coolingplate Temperature
xse.v.nlg6.temp.reported                 0°C                      Onboard Charger reported Temperature
xse.v.nlg6.temp.socket                   0°C                      Onboard Charger socket Temperature
xse.v.nlg6.temps                                                  Onboard Charger Temperatures
xse.v.pos.odometer.start                 41940km                  Odo at last Ignition on. For Trip calc Value
xse.v.display.accel                      89%                      Eco score on acceleration over last 6 hours
xse.v.display.const                      18%                      Eco score on constant driving over last 6 hours
xse.v.display.coast                      100%                     Eco score on coasting over last 6 hours
xse.v.display.ecoscore                   69%                      Eco score shown on dashboard over last 6 hours
======================================== ======================== ============================================
