=======================
Smart ED/EQ Gen.4 (453)
=======================

Vehicle Type: **SQ**

The Smart ED/EQ Gen.4 will be documented here.

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
SOH Display                 Yes
Range Display               Yes
GPS Location                Yes (from modem module)
Speed Display               Yes
Temperature Display         Yes (External Temp and Battery)
BMS v+t Display             only Cell Volts atm.
TPMS Display                Yes pressure & enable at TPMS settings -> temperature / battery low & sensor is missing (TPMS alert)
Charge Status Display       Yes
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control Yes (only 5/10/15 Minutes Pre-heat/cool and timebased Pre-heat/cool App/Web, no Temperature control and SoC > 30% needed)
Lock/Unlock Vehicle         No (not really Implementet, only when the car is open, you can close it. But the lock indicator shows unlocked!)
Valet Mode Control          No
Maintenance Reminders       Yes
12V Battery Monitoring      Yes (if 12V alert raised, the car starts the 12V charging process for 15 Minutes. (homelink 3))
DDT4all simple Support      Yes (a List of all possible commands at www.smart-EMOTION.de)

=========================== ==============
Others
=========================== ==============

-------------------------
Using Cabin Pre-heat/cool:
Only 5 Minutes Booster are Implementet white 
Climatecontrol on or 
homelink 1 = 5 Minutes
homelink 2 = 10 Minutes
homelink 3 = 15 Minutes
For Timebased Pre-heat/cool you can use the Android App or Web UI.
-------------------------

-------------------------
Using DDT4all:
You can use some DDT4all commands. A list of all possible commands you can find at www.smart-EMOTION.de (german).
-------------------------

-------------------------
Shell commands:
-------------------------
xsq start                  -- Show OBD trip start data
xsq reset                  -- Show OBD trip total data
xsq counter                -- Show vehicle trip counter
xsq total                  -- Show vehicle trip total
xsq mtdata                 -- Show maintenance data
xsq tpmsset                -- Set TPMS dummy value for testing
xsq ddt4all <number>       -- Execute DDT4all command by number
xsq ddt4list               -- List all available DDT4all commands
xsq calcadc [voltage]      -- Recalculate ADC factor (optional: 12V voltage override)
xsq wakeup                 -- Wake up the car
xsq ed4scan                -- Output ED4scan-like BMS diagnostic data
xsq preset                 -- Apply smart EQ config preset

-------------------------
Known Issues
-------------------------
- Lock/Unlock: The Lock/Unlock function is not really implemented. You can only close the car when it is open. The lock indicator always shows unlocked.
- Charge Control: Not implemented yet.
-------------------------

-------------------------
metrics
-------------------------

    xsq.v.bus.awake                       -- CAN bus awake status [bool]
    xsq.v.bat.serial                      -- Battery serial number (hex string)
    xsq.v.energy.used                     -- Energy used since mission start [kWh]
    xsq.v.energy.recd                     -- Energy recovered since mission start [kWh]
    xsq.v.aux.consumption                 -- Auxiliary consumption since mission start [kWh]
    xsq.v.bat.consumption.worst           -- Worst average consumption [kWh/100km]
    xsq.v.bat.consumption.best            -- Best average consumption [kWh/100km]
    xsq.v.charge.bcb.power                -- BCB power from mains [W]
    xsq.v.charge.timer.value              -- Charging timer value [min]
    xsq.v.charge.timer.status             -- Charging timer status
    xsq.v.charge.prohibited               -- Charge prohibited status
    xsq.v.charge.authorization            -- Charge authorization status
    xsq.v.charge.ext.manager              -- External charging manager status
    xsq.v.reset.time                      -- Trip time (reset) [hh:mm]
    xsq.v.reset.consumption               -- Average trip consumption (reset) [kWh/100km]
    xsq.v.reset.distance                  -- Trip distance (reset) [km]
    xsq.v.reset.energy                    -- Trip energy consumption (reset) [kWh]
    xsq.v.reset.speed                     -- Average trip speed (reset) [km/h]
    xsq.v.start.time                      -- Time since start [hh:mm]
    xsq.v.start.distance                  -- Trip distance since start [km]
    xsq.adc.factor                        -- Current ADC factor for 12V calculation
    xsq.adc.factor.history                -- Last calculated ADC factors (ring buffer)
    xsq.poll.state                        -- Current poll state (OFF/ON/RUNNING/CHARGING)
    xsq.ed4.values                        -- ED4scan: number of cells to show
    xsq.ddt4all.canbyte                   -- DDT4all CAN response bytes [hex string]
    xsq.odometer.start                    -- Odometer at trip start [km]
    xsq.odometer.start.total              -- Odometer at total trip start [km]
    xsq.odometer.trip                     -- Current trip distance [km]
    xsq.odometer.trip.total               -- Total trip distance [km]
    xsq.obd.charge.duration               -- OBD charge duration [min]
    xsq.obd.mt.day.prewarn                -- Maintenance pre-warning days [days]
    xsq.obd.mt.day.usual                  -- Usual maintenance interval [days]
    xsq.obd.mt.km.usual                   -- Usual maintenance interval [km]
    xsq.obd.mt.level                      -- Maintenance level status
    xsq.tpms.temp                         -- TPMS tyre temperatures vector [°C]
    xsq.tpms.pressure                     -- TPMS tyre pressures vector [kPa]
    xsq.tpms.alert                        -- TPMS alert levels vector
    xsq.tpms.lowbatt                      -- TPMS low battery status vector
    xsq.tpms.missing                      -- TPMS missing transmission status vector
    xsq.tpms.dummy                        -- Dummy pressure for TPMS alert testing [kPa]
    xsq.bcm.state                         -- BCM vehicle state
    xsq.bcm.gen.mode                      -- BCM generator mode
    xsq.evc.hv.energy                     -- EVC HV energy [kWh]
    xsq.evc.12V.dcdc.act.req              -- DCDC active request [bool]
    xsq.evc.12V.dcdc.amps                 -- DCDC current [A]
    xsq.evc.12V.dcdc.load                 -- DCDC load [%]
    xsq.evc.12V.dcdc.volt.req             -- DCDC voltage request [V]
    xsq.evc.12V.dcdc.volt                 -- DCDC voltage [V]
    xsq.evc.12V.dcdc.power                -- DCDC power [W]
    xsq.evc.12V.volt.usm                  -- USM 12V voltage [V]
    xsq.evc.12V.volt.can                  -- 12V battery voltage from CAN [V]
    xsq.evc.12V.batt.volt.req.int         -- 12V battery voltage request internal [V]
    xsq.obl.fastchg                       -- Fast charge active [bool]
    xsq.obl.volts                         -- OBL voltage phases vector [V]
    xsq.obl.amps                          -- OBL current phases vector [A]
    xsq.obl.power                         -- OBL power phases vector [kW]
    xsq.obl.misc                          -- OBL miscellaneous data vector
    xsq.obl.leakdiag                      -- OBL leakage diagnostic status
    xsq.obl.current                       -- OBL leakage currents vector [A]
    xsq.bms.prod.data                     -- BMS production data formatted (serial, MM/YYYY)
    xsq.bms.temps                         -- BMS temperature sensors vector [°C]
    xsq.bms.cv.range.min                  -- Minimum cell voltage [V]
    xsq.bms.cv.range.max                  -- Maximum cell voltage [V]
    xsq.bms.cv.range.mean                 -- Mean cell voltage [V]
    xsq.bms.contactor.cycles              -- HV contactor available cycles
    xsq.bms.contactor.cycles.max          -- HV contactor maximum cycles
    xsq.bms.soc.real                      -- Real battery SOC [%]
    xsq.bms.soc.display                   -- Display battery SOC [%]
    xsq.bms.soc.recal.state               -- SOC recalibration state
    xsq.bms.soc.min                       -- Minimum SOC [%]
    xsq.bms.soc.max                       -- Maximum SOC [%]
    xsq.bms.soc                           -- Kernel SOC [%]
    xsq.bms.soh                           -- State of Health [%]
    xsq.bms.cap.usable.max                -- Maximum usable capacity [Ah]
    xsq.bms.cap.init                      -- Initial capacity [Ah]
    xsq.bms.cap.estimate                  -- Estimated capacity [Ah]
    xsq.bms.cap.loss.pct                  -- Capacity loss percentage [%]
    xsq.bms.mileage                       -- Battery mileage [km]
    xsq.bms.energy.nominal                -- Nominal battery energy [kWh]
    xsq.bms.ocv.voltage                   -- Open circuit voltage [V]
    xsq.bms.voltage.state                 -- Voltage state description
    xsq.bms.cell.resistance               -- Cell resistance values vector
    xsq.bms.batt.link.voltage             -- Battery link voltage [V]
    xsq.bms.batt.contactor.voltage        -- Battery contactor voltage [V]
    xsq.bms.batt.cv.sum                   -- Sum of all cell voltages [V]
    xsq.bms.batt.power                    -- Battery power [kW]
    xsq.bms.contact                       -- HV contactor state text
    xsq.bms.hv                            -- HV voltage [V]
    xsq.bms.ev.mode                       -- EV mode text
    xsq.bms.12v                           -- 12V system voltage [V]
    xsq.bms.interlock.hvplug              -- HV plug interlock status [bool]
    xsq.bms.interlock.service             -- Service interlock status [bool]
    xsq.bms.fusi                          -- FUSI mode text
    xsq.bms.mg.rpm                        -- Motor generator RPM
    xsq.bms.safety                        -- Safety mode text