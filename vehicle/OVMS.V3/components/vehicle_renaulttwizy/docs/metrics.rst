--------------
Custom Metrics
--------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
co.can1.nd1.emcy.code                                             SEVCON emergency code
co.can1.nd1.emcy.type                                             SEVCON emergendy type
co.can1.nd1.state                        Operational              SEVCON state (preop/op)
xrt.b.u.soc.max                          70.52%                   Current trip/charge pack SOC max
xrt.b.u.soc.min                          50.8%                    Current trip/charge pack SOC min
xrt.b.u.temp.max                         20°C                     Current trip/charge pack temp max
xrt.b.u.temp.min                         18.1429°C                Current trip/charge pack temp min
xrt.b.u.volt.max                         55V                      Current trip/charge pack volt max
xrt.b.u.volt.min                         50.2V                    Current trip/charge pack volt min
xrt.bms.balancing                        2,4,8,10,13,14           Custom BMS: cell balancing status
xrt.bms.error                            0                        Custom BMS: error status
xrt.bms.state1                           4                        Custom BMS: main state
xrt.bms.state2                           1                        Custom BMS: auxiliary state
xrt.bms.temp                             52°C                     Custom BMS: internal temperature
xrt.bms.type                             1                        Custom BMS: type (0=VirtualBMS, 1=eDriver BMS, 7=Standard)
xrt.cfg.applied                          yes                      Tuning working set has been applied to SEVCON
xrt.cfg.base                             2                        Tuning base profile (preop mode params)
xrt.cfg.profile                          144,110,111,182,…        Tuning profile params
xrt.cfg.type                             Twizy80                  Tuning vehicle type
xrt.cfg.unsaved                          yes                      Tuning working set has unsaved changes
xrt.cfg.user                             2                        Tuning user/live profile (op mode params)
xrt.cfg.ws                               0                        Tuning profile last loaded into working set
xrt.i.cur.act                            0A                       SC monitor: motor current level
xrt.i.frq.output                         0                        SC monitor: motor output frequency
xrt.i.frq.slip                           0                        SC monitor: motor slip frequency
xrt.i.pwr.act                            0kW                      SC monitor: motor power level
xrt.i.trq.act                            0                        SC monitor: motor torque level
xrt.i.trq.demand                         0                        SC monitor: motor torque demand
xrt.i.trq.limit                          0                        SC monitor: motor torque limit
xrt.i.vlt.act                            0V                       SC monitor: output voltage level (motor)
xrt.i.vlt.bat                            0                        SC monitor: input voltage level (battery)          
xrt.i.vlt.cap                            0A                       SC monitor: capacitor voltage level
xrt.i.vlt.mod                            0%                       SC monitor: motor modulation factor
xrt.m.version                            1.2.4 Sep 17 2019        OVMS Twizy component version
xrt.p.stats.acc.dist                     0.6267km                 Power stats: accelerating: distance
xrt.p.stats.acc.recd                     1.30133e-05kWh           Power stats: accelerating: energy recovered
xrt.p.stats.acc.spdavg                   2.7km/h/s                Power stats: accelerating: speed average
xrt.p.stats.acc.used                     0.17692kWh               Power stats: accelerating: energy used
xrt.p.stats.cst.dist                     0.5331km                 Power stats: coasting: distance
xrt.p.stats.cst.recd                     4.23289e-05kWh           Power stats: coasting: energy recovered
xrt.p.stats.cst.spdavg                   26.18km/h                Power stats: coasting: speed average
xrt.p.stats.cst.used                     0.09703kWh               Power stats: coasting: energy used
xrt.p.stats.dec.dist                     0.5436km                 Power stats: decelerating: distance
xrt.p.stats.dec.recd                     0.0140886kWh             Power stats: decelerating: energy recovered
xrt.p.stats.dec.spdavg                   2.6km/h/s                Power stats: decelerating: speed average
xrt.p.stats.dec.used                     0.0379027kWh             Power stats: decelerating: energy used
xrt.p.stats.ldn.dist                     0.2km                    Power stats: downwards: distance
xrt.p.stats.ldn.hsum                     3m                       Power stats: downwards: height sum
xrt.p.stats.ldn.recd                     0.00300565kWh            Power stats: downwards: energy recovered
xrt.p.stats.ldn.used                     0.0356104kWh             Power stats: downwards: energy used
xrt.p.stats.lup.dist                     1.31km                   Power stats: upwards: distance
xrt.p.stats.lup.hsum                     66m                      Power stats: upwards: height sum
xrt.p.stats.lup.recd                     0.010866kWh              Power stats: upwards: energy recovered
xrt.p.stats.lup.used                     0.263831kWh              Power stats: upwards: energy used
xrt.s.b.pwr.drv                                                   SC monitor: virtual dyno drive power levels
xrt.s.b.pwr.rec                                                   SC monitor: virtual dyno recup power levels
xrt.s.m.trq.drv                                                   SC monitor: virtual dyno drive torque levels
xrt.s.m.trq.rec                                                   SC monitor: virtual dyno recup torque levels
xrt.v.b.alert.12v                        no                       Display service indicator: 12V alert
xrt.v.b.alert.batt                       no                       Display service indicator: Battery alert
xrt.v.b.alert.temp                       no                       Display service indicator: Temperature alert
xrt.v.b.status                           0                        Internal BMS status (CAN frame 628)
xrt.v.c.status                           0                        Internal Charger status (CAN frame 627)
xrt.v.i.status                           0                        Internal SEVCON status (CAN frame 629)
======================================== ======================== ============================================

