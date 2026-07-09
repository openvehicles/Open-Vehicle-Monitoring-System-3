=======================
Smart ED/EQ Gen.4 (453)
=======================

Vehicle Type: **SQ**

.. warning::
  **Potential HV battery contactor cycle counter glitch on Smart 453 (Smart ED/EQ Gen.4)**
  
  Smart/Mercedes have documented that the use of third-party OBD devices
  on the Smart 453 (model variants 453.091/391/491) may impair the
  contactor ageing counter of the high-voltage battery, causing the
  counter to reset to "0" within a short period. When this occurs the
  HV contactors will no longer engage and the vehicle will be
  undriveable. Smart/Mercedes also state that this voids the warranty
  and goodwill entitlement on the HV battery.

  There is `at least one reported case <https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/1405>`_
  of this fault occurring on a Smart 453 with OVMS installed, although the community has not yet
  conclusively established whether OVMS specifically triggers this
  behaviour, or whether it is a more general response to permanently-installed
  OBD devices or just incidental behaviour. The recoverable fix is a BMS counter reset
  (performed by specialist workshops in Europe); full HV battery
  replacement is *not* required for this fault.

  Users of the Smart 453 should weigh this guidance carefully before
  installing OVMS. If you choose to proceed, consider monitoring the
  contactor cycle count via ``xsq hvcycles`` and disconnecting the
  module if any unexpected change is observed.

  References:

  * `Smart EMOTION forum: BMS glitch for contactor switching cycles
    <https://www.smart-emotion.de/forum/thread/4407-bms-glitch-for-contactor-switching-cycles/>`_
  * `GitHub issue #1405
    <https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/issues/1405>`_
  * `Documented case and repair walkthrough (YouTube)
    <https://www.youtube.com/watch?v=9ln-2q_ExEQ>`_


The Smart BMS counts down remaining contactor cycles from 200,000 down to zero. When zero is reached,
the BMS shuts down HV battery access permanently and asks for replacement. Normal usage results
in single contactor counts per HV battery activation (i.e. driving, charging, preconditioning,
12V maintenance charges), amounting to just a couple of counts per day depending on the actual
vehicle use.

The **BMS glitch manifests** as an unbased sudden high counter decrement, usually by a multiple of 1,000.
This happens within seconds, there is no physical relay activation involved, it's a purely
internal counter register value change.

The glitch is triggered by an unknown condition, most probably a combination of multiple factors,
potentially involving some combination of add-on devices and BMS versions, but also potentially
just an internal/hidden fault occurring randomly.

**Beginning with release 3.3.006**, the module offers **monitoring and alerting** for this. The contactor
cycle counter is read (when polling is enabled) and provided via metric ``xsq.bms.contactor.cycles``,
and in readable text report form by command ``xsq hvcycles``. In the Android App, the cycle
counter can be displayed by a long press on "Service".

If a very low remaining cycle count is read, or if a counter change of at least 100 cycles
is detected between two readings, the module will sent an alert notification. In the latter
case, as a safety measure CAN polling will be automatically disabled, so you need to re-enable
it to get further readings.

**Historical data** on the contactor counter decrements is collected on and can be downloaded from
a V2 server in table ``XSQ-BMS-ContactorLog``. If only using V3/MQTT, please consider setting
up a V2 connection as well, to collect the data. Another option is to run an MQTT recorder
saving all contactor log messages received.

**When encountering the issue**: please send your contactor log along with the alerts to the Smart
maintainer(s) for analysis. Please include all info on other devices plugged in or installed, even
dumb devices connected to the 12V system. If enough cases can be collected, there may be a chance
to narrow down potential triggers.


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

-------------------------
Known Issues
-------------------------
- *HV battery contactor cycle counter glitch (Smart 453):* see the
  warning above. Smart/Mercedes' documentation identifies the use of
  third-party OBD devices as a risk factor for premature reset of the
  HV battery contactor ageing counter. At least one OVMS user has
  experienced this fault. Cause-effect is not conclusively established
  in the community, but installing OVMS on a Smart 453 carries the risk
  of HV battery warranty voidance per the manufacturer's stated position.
- Lock/Unlock: The Lock/Unlock function is not really implemented. You can only lock the car when it is open, car is not secured locked.
- Valet Mode: Not implemented.
- Charge Control: Not implemented.

------------------------------
Using Cabin Precond-heat/cool:
------------------------------

Only 5 Minutes preconditioning are implementet by Vehicle

 Shell commands:

 - **climatecontrol on** = 5 Minutes
 - **homelink 1** = 5 Minutes
 - **homelink 2** = 10 Minutes, auto restart after 5 Minutes
 - **homelink 3** = 15 Minutes, auto restart after 5 and 10 Minutes

::

   For Timebased Pre-heat/cool you can use the Android App or Web UI.

-------------------------
Vehicle shell commands:
-------------------------
=========================== ==============
Command                     description
=========================== ==============
xsq mtdata                  maintenance data
xsq ddt4all <number>        Execute DDT4all command by number
xsq ddt4list                List all available DDT4all commands (see below for details)
xsq canwrite <params>       Send custom CAN command (see below for details)
xsq calcadc [voltage]       Recalculate ADC factor (optional: 12V voltage override)
xsq wakeup                  Wake up the car
xsq ed4scan                 Output ED4scan-like BMS diagnostic data
xsq preset                  Apply smart EQ config preset
xsq default                 Set smart EQ config to default values
xsq tpms stat               Show smartEQ TPMS status incl. battery & missing
xsq tpms setdummy           Set TPMS dummy value for testing
xsq show start              Show OBD trip start data
xsq show reset              Show OBD trip total data
xsq show counter            Show vehicle trip counter
xsq show total              Show vehicle trip total data
xsq hvcycles                Show HV contactor cycle counts
=========================== ==============

-------------------------
Vehicle metrics:
-------------------------

=========================== ==============
Metric                      description
=========================== ==============
xsq.v.bat.serial                       Battery serial number (hex string)
xsq.v.energy.used                      Energy used since mission start [kWh]
xsq.v.energy.recd                      Energy recovered since mission start [kWh]
xsq.v.aux.consumption                  Auxiliary consumption since mission start [kWh]
xsq.v.bat.consumption.worst            Worst average consumption [kWh/100km]
xsq.v.bat.consumption.best             Best average consumption [kWh/100km]
xsq.v.charge.bcb.power                 BCB power from mains [W]
xsq.v.reset.time                       Trip time (reset) [hh:mm]
xsq.v.reset.consumption                Average trip consumption (reset) [kWh/100km]
xsq.v.reset.distance                   Trip distance (reset) [km]
xsq.v.reset.energy                     Trip energy consumption (reset) [kWh]
xsq.v.reset.speed                      Average trip speed (reset) [km/h]
xsq.v.start.time                       Time since start [hh:mm]
xsq.v.start.distance                   Trip distance since start [km]
xsq.adc.factor                         Current ADC factor for 12V calculation [float]
xsq.adc.factor.history                 Last calculated ADC factors (ring buffer)
xsq.poll.state                         Current poll state (OFF/ON/RUNNING/CHARGING)
xsq.ed4.values                         ED4scan: number of cells to show
xsq.ddt4all.canbyte                    DDT4all CAN response bytes [hex string]
xsq.odometer.start                     Odometer at trip start [km]
xsq.odometer.start.total               Odometer at total trip start [km]
xsq.odometer.trip                      Current trip distance [km]
xsq.odometer.trip.total                Total trip distance [km]
xsq.obd.charge.duration                OBD charge duration [min]
xsq.obd.mt.day.prewarn                 Maintenance pre-warning days [days]
xsq.obd.mt.day.usual                   Usual maintenance interval [days]
xsq.obd.mt.km.usual                    Usual maintenance interval [km]
xsq.obd.mt.level                       Maintenance level status
xsq.tpms.lowbatt                       TPMS low battery status vector
xsq.tpms.missing                       TPMS missing transmission status vector
xsq.tpms.dummy                         Dummy pressure for TPMS alert testing [kPa]
xsq.bcm.state                          BCM vehicle state
xsq.bcm.gen.mode                       BCM generator mode
xsq.evc.hv.energy                      EVC HV energy [kWh]
xsq.evc.12V.dcdc.act.req               DCDC active request [bool]
xsq.evc.12v.dcdc                       EVC 12V system values vector: [0]=dcdc_volt_req(V), [1]=dcdc_volt(V), [2]=dcdc_power(W), [3]=usm_volt(V), [4]=batt_volt(V), [5]=batt_volt_req(V), [6]=dcdc_amps(A), [7]=dcdc_load(%)
xsq.evc.traceability                   EVC frame traceability information [string] 
xsq.obl.fastchg                        Fast charge active [bool]
xsq.obl.volts                          OBL voltage phases vector [V]
xsq.obl.amps                           OBL current phases vector [A]
xsq.obl.power                          OBL power phases vector [kW]
xsq.obl.misc                           OBL miscellaneous data vector: [0]=freq(Hz), [1]=ground_resistance(Ohm), [2]=max_current(A), [3]=dc_current(mA), [4]=hf10kHz_current(mA), [5]=hf_current(mA), [6]=lf_current(mA)
xsq.obl.leakdiag                       OBL leakage diagnostic status
xsq.bms.prod.data                      BMS production data formatted (serial, MM/YYYY)
xsq.bms.temps                          BMS temperature sensors vector [°C]
xsq.bms.voltages                       BMS voltage values vector: [0]=cell_min(V), [1]=cell_max(V), [2]=cell_mean(V), [3]=cell_sum(V), [4]=pack_volt(V), [5]=traction_link_volt(V), [6]=12v_bms_clamp30(V), [7]=Open Circuit 12V(V)
xsq.bms.contactor.cycles               HV contactor cycles vector: [0]=max, [1]=now, [2]=consumed, [3]=diff, [4]=1h_count - persistent
xsq.bms.soc.values                     SOC values vector [0]=kernel, [1]=real, [2]=min, [3]=max, [4]=display [%]
xsq.bms.soc.recal.state                SOC recalibration state
xsq.bms.soh                            State of Health [%]
xsq.bms.cap                            BMS capacity values vector: [0]=usable_max(Ah), [1]=init(Ah), [2]=estimate(Ah), [3]=loss_pct(%), [4]=usable_capacity(Ah)
xsq.bms.mileage                        Battery mileage [km]
xsq.bms.energy.nominal                 Nominal battery energy [kWh]
xsq.bms.voltage.state                  Voltage state description
xsq.bms.cell.resistance                Cell resistance values vector
xsq.bms.batt.power                     Battery power [kW]
xsq.bms.contact                        HV contactor state text
xsq.bms.ev.mode                        EV mode text
xsq.bms.interlock.hvplug               HV plug interlock status [bool]
xsq.bms.interlock.service              Service interlock status [bool]
xsq.bms.fusi                           FUSI mode text
xsq.bms.safety                         Safety mode text
xsq.bms.id.ident.data                  BMS Identification: PartNo|Supplier|DiagV|HW|SW|basicPart|Ed|Cal
xsq.bms.id.part.no                     BMS Part Number (PartNumber.LowerPart)
xsq.bms.id.hw.version                  BMS Hardware Version (HardwareNumber.LowerPart)
xsq.bms.id.sw.version                  BMS Software Version (SoftwareNumber hex)
xsq.bms.id.basic.parts                 BMS BasicPartList (PN/HW/Approval hex)
xsq.bms.id.mfr                         BMS Manufacturer Identification Code
xsq.12v.trickle.count                  12V trickle charge counted in 24h, reset to 0 after 24h. Alert if count == 3 in 24h [count] - persistent
xsq.12v.undervolt.history              12V undervoltage history vector (<12V>)
=========================== ==============

-------------------------
Using xsq ddt4all:
-------------------------
**Syntax:**
  xsq ddt4all <number>

**Parameters:**
 ``number`` - DDT4all command number from the list

**Examples:**
  xsq ddt4all 42

**Output example:**
  Executing DDT4all command number: 42
    
-------------------------
Using xsq canwrite:
-------------------------
The ``xsq canwrite`` command allows sending custom CAN commands directly to the vehicle.

**Requirements:**

- CAN write access must be enabled: ``config set xsq canwrite yes``
- DDT4all session must be active: ``xsq ddt4all 999``   // activates for 5 minutes
- Command cooldown: 10 seconds between executions

**Syntax:**

  xsq canwrite <txid,rxid,hexbytes[,reset,wakeup]>

**Parameters:**

- ``txid`` - Transmit CAN ID (hex, with or without 0x prefix)
- ``rxid`` - Receive CAN ID (hex, with or without 0x prefix)
- ``hexbytes`` - Hex data bytes to send (multiple bytes separated by /)
- ``reset`` - (optional) Reset CAN session after command (default: false)
- ``wakeup`` - (optional) Wake up vehicle before command (default: true)

**Examples:**

  **Single command**
   xsq canwrite 745,765,3B5880,false,true
    
  **Multiple data bytes (open tailgate 5x)**
    xsq canwrite 745,765,300500/300500/300500/300500/300500,false,true
    
  **Indicator 5x on**
    xsq canwrite 0x745,0x765,2E012100/2E012100

**Output example:**
::

    Sending CAN command:
      txid:    0x745
      rxid:    0x765
      data:    30082002 / 30082002
      reset:   false
      wakeup:  true
    Command executed successfully
