.. _index_obd:

================
VW e-Up via OBD2
================

Vehicle Type: **VWUP.OBD**

This vehicle type supports the VW e-UP (new model from year 2020 onwards). Untested (so far) but probably working: The older models of the e-Up as well as Skoda Citigo E IV and Seat MII electric.

Connection is via the standard OBD-II port (above the drivers left foot):

All communication with the car is read-only. For changing values (i.e. climate control) see the T26A connection to the Comfort CAN bus.

----------------
Support Overview
----------------

=========================== ================================================================
Function                    Support Status
=========================== ================================================================
Hardware                    No specific requirements
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               Yes
GPS Location                Yes
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes (including cell details)
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  Yes
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      **See list of metrics below**
=========================== ================================================================

--------------
Vehicle States
--------------

.. warning::
  For proper state detection, the **12V calibration is crucial**.
  Calibrate using the OVMS Web UI: Config → Vehicle → 12V Monitor

Three vehicle states are supported and detected automatically:

**Vehicle ON**
  The car is on: It is drivable.

**Vehicle CHARGING**
  The car is charging: The car's Charger ECU is responsive and reports charging activity.

**Vehicle OFF**
  The car is off: It hasn't drawn (or charged) any current into the main battery for a 
  period of time and the 12V battery voltage is smaller than 12.9V.

--------------------------
Supported Standard Metrics
--------------------------

**Metrics updated in state "Vehicle ON" or "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
v.e.on                                   yes                      Is ignition on and drivable (true = "Vehicle ON", false = "Vehicle OFF" state)
v.c.charging                             yes                      Is vehicle charging (true = "Vehicle CHARGING" state. v.e.on=false if this is true)
v.c.limit.soc                            100%                     Sufficient SOC: timer mode SOC destination or user CTP configuration (see below)
v.c.mode                                 range                    "range" = charging to 100% SOC, else "standard"
v.c.timermode                            no                       Yes = current/next charge under timer control
v.c.state                                done                     charging, stopped, done
v.c.substate                             scheduledstop            scheduledstop, scheduledstart, onrequest, timerwait, stopped, interrupted
v.b.12v.voltage [1]_                     12.9 V                   Current voltage of the 12V battery
v.b.voltage                              320.2 V                  Current voltage of the main battery
v.b.current                              23.2 A                   Current current into (negative) or out of (positive) the main battery
v.b.power                                23.234 kW                Current power into (negative) or out of (positive) the main battery.
v.b.energy.used.total                    540.342 kWh              Energy used total (life time) of the main battery
v.b.energy.recd.total                    578.323 kWh              Energy recovered total (life time) of the main battery (charging and recuperation)
v.b.temp                                 22.5 °C                  Current temperature of the main battery
v.p.odometer                             2340 km                  Total distance traveled
======================================== ======================== ============================================

.. [1] Also updated in state "Vehicle OFF"

**Metrics updated only in state "Vehicle ON"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
v.b.soc [2]_                             88.2 %                   Current usable State of Charge (SoC) of the main battery
v.e.drivemode                            2                        1=STD, 2=ECO, 3=ECO+
v.e.gear                                 1                        1=forward, 0=neutral, -1=reverse (2020 model only)
======================================== ======================== ============================================

.. [2] Restriction by the ECU. Supplied when the ignition is on during charging. Use xvu.b.soc as an alternative when charging with ignition off.

**Metrics updated only in state "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
v.c.power                                7.345 kW                 Input power of charger
v.c.efficiency                           91.3 %                   Charging efficiency calculated by v.b.power and v.c.power
======================================== ======================== ============================================

--------------
Custom Metrics
--------------

In addition to the standard metrics above the following custom metrics are read from the car or internally calculated by OVMS using read values.

**State metrics**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xvu.e.hv.chgmode                         0                        High voltage charge mode; 0=off, 1=Type2, 4=CCS
xvu.e.lv.autochg                         1                        Auxiliary battery (12V) auto charge mode (0/1)
xvu.e.lv.pwrstate                        0                        Low voltage (12V) power state (0=off, 4=12V, 8=HVAC, 15=on)
======================================== ======================== ============================================


**Timed charge metrics**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xvu.c.limit.soc.max                      80%                      Charge schedule maximum SOC
xvu.c.limit.soc.min                      20%                      Charge schedule minimum SOC
xvu.c.timermode.def                      yes                      Charge timer defined & default
======================================== ======================== ============================================

``xvu.c.timermode.def`` tells if a charge schedule has been configured and enabled. If so, the car uses timed
charging by default (the charge mode button will be lit). ``v.c.timermode`` tells if the charge timer is or will
actually be used for the current or next charge, i.e. reflects the mode selected by pushing the button.

With timed charging, the car first charges to the minimum SOC as soon as possible (when connected). If the
maximum SOC configured for the schedule hasn't been reached by then, it will then wait for the timer to signal
the second phase to charge up to the maximum SOC. The "sufficient SOC" ``v.c.limit.soc`` will be the maximum
SOC if less than 100%. If the timer is disabled or set to charge up to 100%, the sufficient SOC is set to the
user configured charge time prediction SOC limit (config ``xvu ctp.soclimit``).

Charging above ``v.c.limit.soc`` is classified as the "topping off" charge phase. When crossing that SOC
threshold, an intermediate charge status notification is sent.

Note: ``xvu.c.limit.soc.min`` will show the configured minimum SOC also if no schedule is currently enabled.
``xvu.c.limit.soc.max`` shows the maximum for the current/next schedule to apply. If no schedule is enabled,
it will be zero.


**Metrics updated in state "Vehicle ON" or "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xvu.b.cell.delta                         0.012 V                  Delta voltage between lowest and highest cell voltage
xvu.b.soc                                85.3 %                   Current absolute State of Charge (SoC) of the main battery
======================================== ======================== ============================================


**Metrics updated only in state "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xvu.c.eff.ecu [3]_                       92.3 %                   Charger efficiency reported by the Charger ECU
xvu.c.loss.ecu [3]_                      0.620 kW                 Charger power loss reported by the Charger ECU
xvu.c.ac.p                               7.223 kW                 Current charging power on AC side (calculated by ECU's AC voltages and AC currents)
xvu.c.dc.p                               6.500 kW                 Current charging power on DC side (calculated by ECU's DC voltages and DC currents)
xvu.c.eff.calc                           90.0 %                   Charger efficiency calculated by AC and DC power
xvu.c.loss.calc                          0.733 kW                 Charger power loss calculated by AC and DC power
xvu.c.ccs.u [4]_                         331.5V                   CCS charger supplied voltage [V]
xvu.c.ccs.i [4]_                         62.2A                    CCS Charger supplied current [A]
xvu.c.ccs.p [4]_                         20.6193kW                CCS Charger supplied power [kW]
======================================== ======================== ============================================

.. [3] Only supplied by ECU when the car ignition is on during charging.

.. [4] These are not measurements by the car but provided as is by the charger and typically deviate from
  the battery metrics. According to IEC 61851, CCS currents may be off by +/- 3% and voltages by +/- 5%. The
  power figures displayed by some chargers also typically won't match these values, possibly because the charger
  displays the power drawn from the grid (including losses).


----------------------
Battery Capacity & SOH
----------------------

=============== ===================== ================================
e-Up Model      Total capacity        Usable capacity
=============== ===================== ================================
Gen 1 (2016)    18.7 kWh / 50 Ah      16.4 kWh / 43.9 Ah (87.7%)
Gen 2 (2020)    36.8 kWh / 120 Ah     32.3 kWh / 105.3 Ah (87.7%)
=============== ===================== ================================

There are currently two ways to get an estimation of the remaining capacity of the e-Up:

1. By deriving a usable energy capacity from the MFD range estimation.
2. By deriving a total coulomb capacity from the coulombs charged.

You can configure which of the estimations you want to use as the standard SOH from the
"Features" web page or by setting the config parameter ``xvu bat.soh.source`` to either
``charge`` (default) or ``range``.

.. note:: **Consider the capacity estimations as experimental / preliminary.**
  We need field data to optimize the readings. If you'd like to help us, see below.

The **MFD range estimation** seems to include some psychological factors with an SOC below 30%, so we 
only provide this and the derived capacity in two custom metrics. The capacity derivation is only
calculated with SOC >= 30% (initial value needs SOC >= 70%), but if so is available immediately 
after switching the car on. This can serve as a quick first estimation, relate it to the usable 
capacity of your model.

The range based SOH is taken at it's maximum peaks observed and copied if higher than the
previously observed value or added smoothed if lower. So to take the next reading directly,
set metric ``xvu.b.soh.range`` to 0 before switching on the car.

The **charge coulomb based estimation** provides a better estimation but will need a little more 
time to settle. Usable measurements need charges of at least 30% SOC, the more the better. Estimations
are only calculated if a charge has exceeded 30% SOC, and results are smoothed over multiple charges
to provide stable readings.

- To get a rough capacity estimation, charge at least 30% normalized SOC difference.
- To get a good capacity estimation, do at least three charges with each covering 60%
  or more normalized SOC difference.

Charging by CCS (DC) apparently yields higher results, especially on the energy estimations. We
don't know yet the reason or if we need to compensate this.

Note: the **SOH** (state of health) is currently coupled directly and solely to the calculated 
amp-hour capacity **CAC**.


To **log your capacity data** on a connected V2 server, do::

  OVMS# config set xvu log.chargecap.storetime 30

30 is the number of days to keep the data, set to 0 to disable. The counters will be stored in table
``XVU-LOG-ChargeCap``, with one entry every 2.4% absolute SOC difference. Resulting CAC/SOH updates 
will be logged in table ``XVU-LOG-ChargeCapSOH``. You can also extract the data from your module
log file by filtering lines matching ``ChargeCap``.


^^^^^^^^^^^^^^^^^^^^^^^^
Capacity and SOH metrics
^^^^^^^^^^^^^^^^^^^^^^^^

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xvu.b.soh.charge                         99.23%                   SOH based on charge energy sum
xvu.b.soh.range                          98.89%                   SOH based on MFD range estimation
xvu.b.cap.ah.abs                         122.71Ah                 Total coulomb capacity estimation
xvu.b.cap.ah.norm                        113.63Ah                 Usable coulomb capacity estimation
xvu.b.cap.kwh.abs                        39.1kWh                  Total energy capacity estimation
xvu.b.cap.kwh.norm                       36.21kWh                 Usable energy capacity estimation
xvu.b.cap.kwh.range                      32.8947kWh               Usable energy capacity estimation from MFD range
xvu.b.energy.range                       18.5kWh                  Current energy used by MFD range estimation
xvu.b.c.soh                              88.8,91.3,91.3,90.4…     Array: SOH [%] of each HV battery cell
xvu.b.hist.soh.mod.<NN>                  100,97.1,97.5,…          Array: SOH [%] history of each battery module (NN=01…14/17)
======================================== ======================== ============================================

Note on cell module history: the e-Up records an overall SOH level of each module
once every 3 months, up to a maximum time of 10 years.


^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Provide Data to the Developers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To help us with optimizing the capacity estimations, first of all enable file logging if not already 
enabled. Then enable extended polling and logging before a charge by…::

  OVMS# config set xvu dc_interval 30
  OVMS# log level verbose v-vweup

After the charge, disable the extended polling and logging::

  OVMS# config set xvu dc_interval 0
  OVMS# log level info v-vweup

Then download all log files written during the charge (archived and current), zip them and mail
the zip to Michael Balzer <dexter@dexters-web.de>. The log data will only be used for technical 
analysis and deleted afterwards.

Note: if you forgot enabling the local log but still have chargecap logs on the server: these can help
as well.

**Thanks!**


-----------------------
Configuration Variables
-----------------------

The main configuration variables can be set through the web configuration page:

- VW e-Up → Features

Some configuration variables are kept "under the hood", as these will normally not need to be
changed, except for some special use cases or for development / debugging.

Configuration variables can be listed using command ``config list xvu`` and changed using
command ``config set xvu <variable> <value>``.


=================================== =================== ========================================================
Configuration variable              Default value       Description                                           
=================================== =================== ========================================================
bat.soh.source                      charge              SOH source -- see above
bms.autoreset                       no                  Reset BMS statistics on use phase transitions
canwrite                            no                  Allow CAN write access
cell_interval_awk                   60                  BMS cell query interval in awake state [s]
cell_interval_chg                   60                  BMS cell query interval in charge state [s]
cell_interval_drv                   15                  BMS cell query interval in drive state [s]
con_obd                             yes                 Enable OBDII connection
con_t26                             yes                 Enable T26 connection
ctp.maxpower                        0                   Charge time prediction: fallback power limit [kW] (0=none)
ctp.soclimit                        80                  Charge time prediction: fallback SOC limit [%]
dc_interval                         0                   Development: additional DC charge PID query interval [s] (0=off)
log.chargecap.cpstep                24                  Charge capacity: checkpoint interval [1/10%]
log.chargecap.minvalid              272                 Charge capacity: min SOC hub for SOH change [1/10%]
log.chargecap.storetime             0                   Charge capacity: server log archive time [days] (0=off)
modelyear                           2012                Vehicle model year
notify.charge.start.delay           24                  Charge start notification delay [s] [5]_
=================================== =================== ========================================================

.. [5] Charge start needs some time to ramp up the charge current, which implies charge
  speed & time estimations. If you want the start notification as fast as possible,
  reduce the value.


--------------
Shell Commands
--------------

- ``xvu polling <status|pause|continue>`` -- control OBD2 polling
  
  The OBD2 polling is normally continuously active as long as the vehicle module is loaded.
  To free the CAN bus from this load during an extended OBD2 diagnostics or modification
  (coding / adaptation) session, you can temporarily pause the polling using this command.
  
  There is no time limit for a pause, keep in mind you need to explicitly continue the
  polling when you're done. During a polling pause, no vehicle state changes can be detected.



-----------------------------
Custom Status Page for Web UI
-----------------------------

.. note::
  This plugin is obsolete, use the standard page **VW e-Up → Charging Metrics** instead.
  We keep the source here as a base for user customization.

The easiest way to display custom metrics is using the *Web Plugins* feature of OVMS (see :ref:`installing-web-plugins`).

This page plugin content shows the metrics in a compact form which can be displayed on a phone in landscape mode on the dashboard of the car. Best approach is to connect the phone directly to the OVMS AP-WiFi and access the web UI via the static IP (192.168.4.1) of OVMS.

.. image:: data.png
  :align: center

.. code-block:: html

  <div class="panel panel-primary">
    <div class="panel-heading">VW eUp</div>
    <div class="panel-body">
  
    <hr/>
  
    <div class="receiver">  
      <div class="clearfix">
      <div class="metric progress" data-metric="v.b.soc" data-prec="2">
        <div class="progress-bar value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="width:0%">
        <div>
          <span class="label">SoC</span>
          <span class="value">?</span>
          <span class="unit">%</span>
        </div>
        </div>
      </div>
      <div class="metric progress" data-metric="xvu.b.soc" data-prec="2">
        <div class="progress-bar progress-bar-info value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="width:0%">
        <div>
          <span class="label">SoC (absolute)</span>
          <span class="value">?</span>
          <span class="unit">%</span>
        </div>
        </div>
      </div>
      </div>
      <div class="clearfix">
      <div class="metric number" data-metric="v.b.energy.used.total" data-prec="3">
        <span class="label">TOTALS:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Used</span>
        <span class="value">?</span>
        <span class="unit">kWh</span>
      </div>
      <div class="metric number" data-metric="v.b.energy.recd.total" data-prec="3">
        <span class="label">Charged</span>
        <span class="value">?</span>
        <span class="unit">kWh</span>
      </div>
      <div class="metric number" data-metric="v.p.odometer" data-prec="0">
        <span class="label">Distance</span>
        <span class="value">?</span>
        <span class="unit">km</span>
      </div>
      </div>
  
      <h4>Battery</h4>
  
      <div class="clearfix">
      <div class="metric progress" data-metric="v.b.voltage" data-prec="1">
        <div class="progress-bar value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="300" aria-valuemax="350" style="width:0%">
        <div>
          <span class="label">Voltage</span>
          <span class="value">?</span>
          <span class="unit">V</span>
        </div>
        </div>
      </div>
      <div class="metric progress" data-metric="v.b.current" data-prec="1">
        <div class="progress-bar progress-bar-danger value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="-200" aria-valuemax="200" style="width:0%">
        <div>
          <span class="label">Current</span>
          <span class="value">?</span>
          <span class="unit">A</span>
        </div>
        </div>
      </div>
      <div class="metric progress" data-metric="v.b.power" data-prec="3">
        <div class="progress-bar progress-bar-warning value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="-70" aria-valuemax="70" style="width:0%">
        <div>
          <span class="label">Power</span>
          <span class="value">?</span>
          <span class="unit">kW</span>
        </div>
        </div>
      </div>
      </div>
      <div class="clearfix">
      <div class="metric number" data-metric="v.b.temp" data-prec="1">
        <span class="label">Temp</span>
        <span class="value">?</span>
        <span class="unit">°C</span>
      </div>
      <div class="metric number" data-metric="xvu.b.cell.delta" data-prec="3">
        <span class="label">Cell delta</span>
        <span class="value">?</span>
        <span class="unit">V</span>
      </div>
      </div>
  
      <h4>Charger</h4>
  
      <div class="clearfix">
      <div class="metric progress" data-metric="xvu.c.ac.p" data-prec="3">
        <div class="progress-bar progress-bar-warning value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="0" aria-valuemax="8" style="width:0%">
        <div>
          <span class="label">AC Power</span>
          <span class="value">?</span>
          <span class="unit">kW</span>
        </div>
        </div>
      </div>
      <div class="metric progress" data-metric="xvu.c.dc.p" data-prec="3">
        <div class="progress-bar progress-bar-warning value-low text-left" role="progressbar"
        aria-valuenow="0" aria-valuemin="0" aria-valuemax="8" style="width:0%">
        <div>
          <span class="label">DC Power</span>
          <span class="value">?</span>
          <span class="unit">kW</span>
        </div>
        </div>
      </div>
      </div>   
      <div class="clearfix">
      <div class="metric number" data-metric="v.c.efficiency" data-prec="1">
        <span class="label">Efficiency (total)</span>
        <span class="value">?</span>
        <span class="unit">%</span>
      </div>
      <div class="metric number" data-metric="xvu.c.eff.calc" data-prec="1">
        <span class="label">Efficiency (charger)</span>
        <span class="value">?</span>
        <span class="unit">%</span>
      </div>
      <div class="metric number" data-metric="xvu.c.loss.calc" data-prec="3">
        <span class="label">Loss (charger)</span>
        <span class="value">?</span>
        <span class="unit">kW</span>
      </div>
      </div>
    </div>
    </div>
  </div>
