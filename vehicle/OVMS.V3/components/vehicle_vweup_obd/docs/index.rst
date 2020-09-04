===========
VW e-Up OBD
===========

Vehicle Type: **VWUP.OBD**

This vehicle type supports the VW e-UP (new model from year 2020 onwards). Untested (so far) but probably working: The older models of the e-Up as well as Skoda Citigo E IV and Seat MII electric.

Connection is via the standard OBD-II port (above the drivers left foot). All communication with the car is read-only. For changing values (i.e. climate control) see the other VW e-Up vehicle type.

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
SOC Display                 Yes (See list of metrics below)
Range Display               No
GPS Location                No
Speed Display               No
Temperature Display         No
BMS v+t Display             Yes (See list of metrics below)
TPMS Display                No
Charge Status Display       No
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      **See list of metrics below**
=========================== ================================================================

--------------
Vehicle States
--------------

Three vehicle states are supported and detected automatically. For proper detection the 12V calibration (OVMS Web UI: Config - Vehicle - 12V Monitor) is recommended.

**Vehicle ON**

The car is on: It is drivable.

**Vehicle CHARGING**

The car is charging: The car's Charger ECU is responsive and reports charging activity.

**Vehicle OFF**

The car is off: It hasn't drawn (or charged) any current into the main battery for a period of time and the 12V battery voltage is smaller than 12.9V.

--------------------------
Supported Standard Metrics
--------------------------

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
v.e.on                                   true                     Is ignition on and drivable (true = "Vehicle ON", false = "Vehicle OFF" state)
v.c.charging                             true                     Is vehicle charging (true = "Vehicle CHARGING" state. v.e.on=false if this is true)
v.b.12v.voltage                          12.9 V                   Current voltage of the 12V battery
v.b.voltage                              320.2 V                  Current voltage of the main battery
v.b.current                              -23.2 A                  Current current into (positive) or out of (negative) the main battery
v.b.power                                -23.234 kW               Current power into (positive) or out of (negative) the main battery.
v.b.soc                                  88.2 %                   Current State of Charge (SoC) of the main battery
v.b.temp                                 22.5 °C                  Current temperature of the main battery
v.p.odometer                             2340 km                  Total distance traveled
======================================== ======================== ============================================

Note: In state "Vehicle OFF" only *v.b.12v.voltage* is updated.

--------------
Custom Metrics
--------------

In addition to the standard metrics above the following custom metrics are read from the car or internally calculated by OVMS using read values.

**Metrics updated in state "Vehicle ON" or "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xuo.b.energy.used                        -540.342 kWh             Total energy taken out of the main battery
xuo.b.energy.charged                     578.323 kWh              Total energy put (by charging or recuperation) into the main battery
xuo.b.cell.delta                         0.012 V                  Delta voltage between lowest and highest cell voltage
======================================== ======================== ============================================

**Metrics updated in state "Vehicle CHARGING"**

======================================== ======================== ============================================
Metric name                              Example value            Description
======================================== ======================== ============================================
xuo.c.eff.ecu                            92.3 %                   Charger efficiency reported by the Charger ECU
xuo.c.loss.ecu                           620 W                    Charger power loss reported by the Charger ECU
xuo.c.ac.p                               7223 W                   Current charging power on AC side (calculated by ECU's AC voltages and AC currents)
xuo.c.dc.p                               6500 W                   Current charging power on DC side (calculated by ECU's DC voltages and DC currents)
xuo.c.eff.calc                           90.0 %                   Charger efficiency calculated by AC and DC power
xuo.c.loss.calc                          733 W                    Charger power loss calculated by AC and DC power
======================================== ======================== ============================================

-----------------------------
Custom Status Page for Web UI
-----------------------------

The easiest way to display custom metrics is using the *Web Plugins* feature of OVMS (see *Installing Plugins* in the *Web Framework & Plugins* section).

This page plugin content shows the metrics in a compact form which can be displayed on a phone in landscape mode on the dashboard of the car.

.. image:: data.png
  :align: center

::

    <div class="panel panel-primary">
     <div class="panel-heading">VW eUp</div>
     <div class="panel-body">
    
      <hr/>
    
      <div class="receiver">  
       <div class="clearfix">
        <div class="metric progress" data-metric="v.b.soc" data-prec="1">
         <div class="progress-bar progress-bar-info value-low text-left" role="progressbar"
          aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="width:0%">
          <div>
           <span class="label">SoC</span>
           <span class="value">?</span>
           <span class="unit">%</span>
          </div>
         </div>
        </div>
       </div>
       <div class="clearfix">
        <div class="metric number" data-metric="vwup.batmgmt.enrg.used" data-prec="3">
         <span class="label">TOTALS:&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbspDischarged</span>
         <span class="value">?</span>
         <span class="unit">kWh</span>
        </div>
        <div class="metric number" data-metric="vwup.batmgmt.enrg.chrgd" data-prec="3">
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
        <div class="metric number" data-metric="vwup.batmgmt.cell.delta" data-prec="3">
         <span class="label">Cell delta</span>
         <span class="value">?</span>
         <span class="unit">V</span>
        </div>
       </div>
    
       <h4>Charger</h4>
    
       <div class="clearfix">
        <div class="metric progress" data-metric="vwup.chrgr.ac.p" data-prec="0">
         <div class="progress-bar progress-bar-warning value-low text-left" role="progressbar"
          aria-valuenow="0" aria-valuemin="0" aria-valuemax="8000" style="width:0%">
          <div>
           <span class="label">AC Power</span>
           <span class="value">?</span>
           <span class="unit">W</span>
          </div>
         </div>
        </div>
        <div class="metric progress" data-metric="vwup.chrgr.dc.p" data-prec="0">
         <div class="progress-bar progress-bar-warning value-low text-left" role="progressbar"
          aria-valuenow="0" aria-valuemin="0" aria-valuemax="8000" style="width:0%">
          <div>
           <span class="label">DC Power</span>
           <span class="value">?</span>
           <span class="unit">W</span>
          </div>
         </div>
        </div>
       </div>   
       <div class="clearfix">
        <div class="metric number" data-metric="vwup.chrgr.eff.calc" data-prec="1">
         <span class="label">Efficiency (calc)</span>
         <span class="value">?</span>
         <span class="unit">%</span>
        </div>
        <div class="metric number" data-metric="vwup.chrgr.eff.ecu" data-prec="1">
         <span class="label">Efficiency (ECU)</span>
         <span class="value">?</span>
         <span class="unit">%</span>
        </div>
        <div class="metric number" data-metric="vwup.chrgr.loss.calc" data-prec="0">
         <span class="label">Loss (calc)</span>
         <span class="value">?</span>
         <span class="unit">W</span>
        </div>
        <div class="metric number" data-metric="vwup.chrgr.loss.ecu" data-prec="0">
         <span class="label">Loss (ECU)</span>
         <span class="value">?</span>
         <span class="unit">W</span>
        </div>
       </div>
      </div>
     </div>
    </div>
