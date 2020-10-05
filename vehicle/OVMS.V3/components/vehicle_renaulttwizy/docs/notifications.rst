.. highlight:: none

=============
Notifications
=============

The Renault Twizy module sends the custom or customized notifications described here additionally 
to the system notifications.

See :doc:`/userguide/notifications` for general info on notifications.


======= =========================== ================================================================
Type    Subtype                     Purpose / Content
======= =========================== ================================================================
alert   battery.status              Battery pack/cell alert status report (alerts & watches)
alert   bms.status                  Battery management system alert (for custom BMS only)
info    charge.status.sufficient    Sufficient charge reached (SOC/range as configured)
alert   valetmode.odolimit          Odometer limit reached, speed reduction engaged
alert   vehicle.dtc                 OBD2 DTC (diagnostic trouble code) alert
data    xrt.battery.log             Battery pack/cell monitoring log
data    xrt.gps.log                 Extended GPS log
data    xrt.obd.cluster.dtc         OBD2 DTC log for cluster (display/UCH)
data    xrt.power.dyno              SEVCON live monitoring (virtual dyno) data
data    xrt.power.log               Power statistics
info    xrt.power.totals            Current usage cycle power totals
data    xrt.trip.log                Trip history log
info    xrt.trip.report             Trip energy usage report
alert   xrt.sevcon.fault            SEVCON fault condition detected
data    xrt.sevcon.log              SEVCON faults & events logs
info    xrt.sevcon.profile.reset    A tuning RESET has been performed
info    xrt.sevcon.profile.switch   Tuning profile switch result
======= =========================== ================================================================



----------------
Trip history log
----------------

The trip history log can be used as a source for long term statistics on your trips and typical 
trip power usages, as well as your battery performance in different environmental conditions and 
degradation over time.

Entries are created at the end of a trip and on each change in the charge state, so you can also 
see where charges stopped or how long they took and how high the temperatures got.

  - Notification subtype: ``xrt.trip.log``
  - History record type: ``RT-PWR-Log``
  - Format: CSV
  - Maximum archive time: 365 days
  - Fields/columns:

    * odometer_km
    * latitude
    * longitude
    * altitude
    * chargestate
    * parktime_min
    * soc
    * soc_min
    * soc_max
    * power_used_wh
    * power_recd_wh
    * power_distance
    * range_estim_km
    * range_ideal_km
    * batt_volt
    * batt_volt_min
    * batt_volt_max
    * batt_temp
    * batt_temp_min
    * batt_temp_max
    * motor_temp
    * pem_temp
    * trip_length_km
    * trip_soc_usage
    * trip_avg_speed_kph
    * trip_avg_accel_kps
    * trip_avg_decel_kps
    * charge_used_ah
    * charge_recd_ah
    * batt_capacity_prc
    * chg_temp


----------------
Extended GPS log
----------------

The extended GPS log contains additional details about power and current levels, the BMS power 
limits and the automatic power level adjustments done by the OVMS. You can use this to create 
detailed trip power charts and to verify your auto power adjust settings.

The log frequency is once per minute while parking/charging, and controlled by config ``xrt 
gpslogint`` (web UI: Twizy → Features) while driving. Logging only occurs if logged metrics have 
changed.

  - Notification subtype: ``xrt.gps.log``
  - History record type: ``RT-GPS-Log``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

      * odometer_mi_10th
      * latitude
      * longitude
      * altitude
      * direction
      * speed
      * gps_fix
      * gps_stale_cnt
      * gsm_signal
      * current_power_w
      * power_used_wh
      * power_recd_wh
      * power_distance
      * min_power_w
      * max_power_w
      * car_status
      * max_drive_pwr_w
      * max_recup_pwr_w
      * autodrive_level
      * autorecup_level
      * min_current_a
      * max_current_a


--------------------------------
Battery pack/cell monitoring log
--------------------------------

The extended GPS log contains additional details about power and current levels, the BMS power 
limits and the automatic power level adjustments done by the OVMS. You can use this to create 
detailed trip power charts and to verify your auto power adjust settings.

The standard log frequency is once per minute, logging only occurs if logged metrics have changed. 
Additional records are created on battery alert events. Note: an entry consists of a pack level 
record (``RT-BAT-P``) and up to 16 (for LiFePO4 batteries) cell entries (``RT-BAT-C``).

  - Notification subtype: ``xrt.battery.log``
  - History record type: ``RT-BAT-P`` (pack status)
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * packnr
    * volt_alertstatus
    * temp_alertstatus
    * soc
    * soc_min
    * soc_max
    * volt_act
    * volt_min
    * volt_max
    * temp_act
    * temp_min
    * temp_max
    * cell_volt_stddev_max
    * cmod_temp_stddev_max
    * max_drive_pwr
    * max_recup_pwr
    * bms_state1
    * bms_state2
    * bms_error
    * bms_temp


  - Notification subtype: ``xrt.battery.log``
  - History record type: ``RT-BAT-C`` (cell status)
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * cellnr
    * volt_alertstatus
    * temp_alertstatus
    * volt_act
    * volt_min
    * volt_max
    * volt_maxdev
    * temp_act
    * temp_min
    * temp_max
    * temp_maxdev
    * balancing
    * been_balancing
    * balancetime


----------------
Power statistics
----------------

The power statistics are the base for the trip reports and can be used to analyze trip 
sections regarding speed and altitude changes and their respective effects on power usage. The log 
is also written when charging, that data can be used to log changes in the charge current, for 
example triggered externally by some solar charge controller.

Log frequency is once per minute, logging only occurs if metrics have changed.

  - Notification subtype: ``xrt.power.log``
  - History record type: ``RT-PWR-Stats``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * speed_const_dist
    * speed_const_use
    * speed_const_rec
    * speed_const_cnt
    * speed_const_sum
    * speed_accel_dist
    * speed_accel_use
    * speed_accel_rec
    * speed_accel_cnt
    * speed_accel_sum
    * speed_decel_dist
    * speed_decel_use
    * speed_decel_rec
    * speed_decel_cnt
    * speed_decel_sum
    * level_up_dist
    * level_up_hsum
    * level_up_use
    * level_up_rec
    * level_down_dist
    * level_down_hsum
    * level_down_use
    * level_down_rec
    * charge_used
    * charge_recd


--------------------
OBD2 cluster DTC log
--------------------

This server table stores DTC occurrences for one week. This is mostly raw data, and the DTCs are 
internal Renault values that have not yet been decoded.

See: https://www.twizy-forum.de/ovms/86362-liste-df-codes-dtc

  - Notification subtype: ``xrt.obd.cluster.dtc``
  - History record type: ``RT-OBD-ClusterDTC``
  - Format: CSV
  - Maximum archive time: 7 days
  - Fields/columns:

    * EntryNr
    * EcuName
    * NumDTC
    * Revision
    * FailPresentCnt
    * G1
    * G2
    * ServKey
    * Customer
    * Memorize
    * Bt
    * Ef
    * Dc
    * DNS
    * Odometer
    * Speed
    * SOC
    * BattV
    * TimeCounter
    * IgnitionCycle


----------------------
SEVCON faults & events
----------------------

These logs are created on request only, e.g. by the SEVCON logs tool in the Android App, or by 
using the ``xrt cfg querylogs`` command. The command queries the SEVCON (inverter) alerts, faults, 
events and statistics (SEVCON needs to be online). The results are then transmitted to the server 
using the following records.

  - Notification subtype: ``xrt.sevcon.log``
  - History record type: ``RT-ENG-LogKeyTime``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * KeyHour
    * KeyMinSec


  - Notification subtype: ``xrt.sevcon.log``
  - History record type: ``RT-ENG-LogAlerts``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * Code
    * Description
    * TimeHour
    * TimeMinSec
    * Data1
    * Data2
    * Data3


  - Notification subtype: ``xrt.sevcon.log``
  - History record type: ``RT-ENG-LogSystem``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * Code
    * Description
    * TimeHour
    * TimeMinSec
    * Data1
    * Data2
    * Data3


  - Notification subtype: ``xrt.sevcon.log``
  - History record type: ``RT-ENG-LogCounts``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * Code
    * Description
    * LastTimeHour
    * LastTimeMinSec
    * FirstTimeHour
    * FirstTimeMinSec
    * Count


  - Notification subtype: ``xrt.sevcon.log``
  - History record type: ``RT-ENG-LogMinMax``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns:

    * BatteryVoltageMin
    * BatteryVoltageMax
    * CapacitorVoltageMin
    * CapacitorVoltageMax
    * MotorCurrentMin
    * MotorCurrentMax
    * MotorSpeedMin
    * MotorSpeedMax
    * DeviceTempMin
    * DeviceTempMax


----------------------
SEVCON live monitoring
----------------------

These records store the measurement results of the virtual dyno included in the SEVCON live monitor 
(Twizy → SEVCON Monitor). The virtual dyno records a torque/power profile from the actual car 
performance during driving. The profile has four data sets:

  - Maximum battery drive power over speed (metric ``xrt.s.b.pwr.drv``, unit kW)
  - Maximum battery recuperation power over speed (metric ``xrt.s.b.pwr.rec``, unit kW)
  - Maximum motor drive torque over speed (metric ``xrt.s.m.trq.drv``, unit Nm)
  - Maximum motor recuperation torque over speed (metric ``xrt.s.m.trq.rec``, unit Nm)

Power is measured at the battery, so you can derive the efficiency. Speed is truncated to integer, 
the value arrays take up to 120 entries (0 … 119 kph).

These datasets are visualized by the web UI using a chart, and transmitted to the server on any 
monitoring "stop" or "reset" command in the following records:

  - Notification subtype: ``xrt.power.dyno``
  - History record types:
    ``RT-ENG-BatPwrDrv``, ``RT-ENG-BatPwrRec``, ``RT-ENG-MotTrqDrv``, ``RT-ENG-MotTrqRec``
  - Format: CSV
  - Maximum archive time: 24 hours
  - Fields/columns: max 120 values for speed levels beginning at 0 kph


