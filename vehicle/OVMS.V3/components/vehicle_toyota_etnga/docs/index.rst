======================
Toyota e-TNGA platform
======================

Support for the Toyota e-TNGA platform. This is a shared base module, not a directly selectable
vehicle: it provides the features common to all vehicles built on the platform:

* :doc:`Toyota bZ4X </components/vehicle_toyota_bz4x/docs/index>`
* :doc:`Subaru Solterra </components/vehicle_subaru_solterra/docs/index>`

Those pages list only what is specific to each vehicle; everything below applies to all of them.

- Vehicle Type: not selectable directly — select one of the vehicles above
- Log tag: ``v-etnga`` (nearly all logging; the vehicle wrappers log almost nothing of their own)
- Namespace: ``xte``

.. note::

   e-TNGA support has so far only been confirmed on a 2022-24 Subaru Solterra (96-cell battery
   pack).  The Toyota bZ4X and the 2025/26 refresh models are expected to work the same way, but
   have not yet been tested on a car.  Reports from owners of those vehicles are welcome.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    Any OVMS v3 (or later) module.
Vehicle Cable               OBD-II to DB9 Data Cable for OVMS (1441200 right, or 1139300 left)
GSM Antenna                 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Yes
Range Display               No
GPS Location                Yes
Speed Display               Yes
Temperature Display         Yes
BMS v+t Display             Yes (see note below)
TPMS Display                Yes
Charge Status Display       Yes
Charge Interruption Alerts  Yes (see note below)
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      VIN, charge session report, 12V auxiliary battery telemetry
=========================== ==============

.. note::

   **Charge Interruption Alerts** are sent whenever a charge phase ends, and at **alert** rather
   than info priority.  This includes ordinary pauses that are not a fault, for example a
   scheduled charge that pauses and later resumes.

.. note::

   **BMS v+t Display** works for all battery pack variants: the number of cells and temperature
   sensors is detected automatically, so the cell monitor, cell history and deviation alerts work
   on the 96-cell pack as well as on the 78- and 104-cell packs of the 2025/26 refresh.  Cell
   voltage deviation is flagged at 20 mV (warning) and 30 mV (alert), temperature deviation at
   4 °C (warning) and 8 °C (alert).

----------------------------
Sleep and charging behaviour
----------------------------

The module adapts how often it talks to the car to what the car is doing, so that OVMS itself does
not drain the 12V battery of a parked car:

* **Parked:** a short while after the car goes to sleep, the module stops sending anything and
  only listens.  It wakes up again as soon as the car becomes active or its 12V battery starts
  being charged.
* **Driving:** the full set of driving data is read, from once per second (speed, power) up to
  every few minutes (slowly changing values such as tyre pressures).
* **Plugged in:** the module follows the charge from plug-in to unplug.  For a scheduled or
  delayed charge it checks in periodically while waiting and sleeps in between, so the 12V
  battery can recover, and picks the session up again when charging starts.  A charge that
  starts while the module is checking in is caught within about ten seconds.

The standard charge state ``v.c.state`` follows the session: ``prepared`` while the car and
charger negotiate, ``charging`` while energy flows, ``stopped`` while waiting (e.g. for a scheduled
charge, or between charge phases), and ``done`` after unplugging following a charge (empty if the
session was cancelled before it started).  ``v.e.awake`` stays ``false`` throughout a charge, since
the car is not switched on, so no "Vehicle is idling" notifications are sent while charging.

----
TPMS
----

Tyre pressures and temperatures are read every 60 s while driving.  Three standard vector metrics
are published, one element per wheel in the canonical order ``[FL, FR, RL, RR]``:
``v.t.pressure`` (kPa), ``v.t.temp`` (°C) and ``v.t.alert`` (0 normal, 1 warning, 2 alert).

TPMS sensors are motion-activated, so the values are those last reported while the wheels were
rolling; they are not refreshed for a car that has been parked for a while.  A wheel with no
sensor fitted publishes 0 and is excluded from alerting.  A tyre rotation or a TPMS relearn is
picked up within about a minute of driving, with no restart needed.

The four alert thresholds are ``[xte]`` config parameters (see Configuration below).  Pressure
uses a low-pressure test and temperature an overheat test, so for the three-level behaviour to
work the pressure warn threshold must be **above** its alert threshold and the temperature warn
threshold **below** its alert threshold.

------
Web UI
------

The following pages appear in the **Vehicle** menu of the OVMS web interface:

* **BMS cell monitor** — live per-cell voltages and temperatures, see
  :doc:`Battery Monitor </components/ovms_webserver/docs/bms-cell-monitor>`.
* **Charging monitor** — live charging dashboard with the key charge figures during a session.
* **Charge reports** — the stored charge session reports (see below), each with its HTML report
  and per-sample CSV download.
* **Configuration** — the ``[xte]`` settings (see Configuration below) as a form, plus the
  detected battery pack and the nominal capacity in use.

---------------------
Charge session report
---------------------

At the end of each charging session (plug-in to unplug), the module writes a self-contained HTML
report and a per-sample CSV to ``/sd/charge-reports/`` when an SD card is mounted, falling back to
``/store/charge-reports/`` on internal flash.  The newest 50 sessions are retained; older sessions
and orphan CSV files whose session never produced a report are pruned automatically.

The HTML report contains:

* **Summary** — plug-in and unplug timestamps (UTC), duration, plug-in GPS location with an
  OpenStreetMap link, ambient temperature range, charge type, SOC start to end, delivered and
  grid energy, peak and average power, battery temperature range, and the session outcome
  decoded to a human-readable label.
* **Inline SVG power/SOC chart** — a downsampled power trace with an SOC overlay, rendered
  on-module with no external dependencies.
* **Session event log** — timestamped events (plug-in, charge phases started and ended, unplug).
* **Estimates** — charging efficiency (AC sessions, where grid energy is available) and implied
  pack capacity derived from delivered Ah and the SOC delta.
* **A link to the per-sample CSV** for offline analysis.

The CSV is written one row per second during active charging.  Its columns cover session position
(``phase``, ``elapsed_s``), both SOC readings, the three measured powers (station, battery,
cabin), pack voltage/current/temperature and ambient, the pack temperature spread, the station's
caps, the car's asks, raw station telemetry, and the module's own charge integral.

.. note::

   Parse the CSV by header name, not by column position: columns have been added over time.  For
   charging-curve analysis use ``batt_tmax_c`` (hottest sensor, which the battery derates on)
   rather than ``batt_temp_c`` (the average), and ``battery_kw`` for the actual charging power.

-------------
Configuration
-------------

All e-TNGA vehicles share the ``xte`` config instance.  Every parameter can be set from the shell
with ``config set xte <param> <value>`` or on the **Configuration** page in the web UI's Vehicle
menu.

======================== ============= ==========================================================
Parameter                Default       Meaning
======================== ============= ==========================================================
``tpms.pressure.warn``   240           Tyre pressure (kPa) below which a warning is raised
``tpms.pressure.alert``  220           Tyre pressure (kPa) below which an alert is raised
``tpms.temp.warn``       90            Tyre temperature (°C) above which a warning is raised
``tpms.temp.alert``      100           Tyre temperature (°C) above which an alert is raised
``bat.nominal.ah``       0             Pack nominal full-charge capacity (Ah), the denominator
                                       for ``v.b.soh``.  ``0`` derives it from the detected pack,
                                       which is only established for the 96-cell pack
``bat.nominal.volt``     0             Pack nominal voltage (V), used to convert capacity to kWh
                                       for ``v.b.capacity``.  ``0`` derives it, 96-cell only
======================== ============= ==========================================================

``v.b.soh`` and ``v.b.capacity`` are only published once a pack nominal is known: automatically
for the 96-cell pack, otherwise only after ``bat.nominal.ah`` / ``bat.nominal.volt`` are set, since
a wrong nominal would give a believable but wrong percentage.  ``v.b.soh`` is deliberately not
capped at 100 %: a reading above 100 % means the configured nominal does not match the pack.

-------
Metrics
-------

Besides the standard OVMS metrics, the module provides these custom metrics in the ``xte``
namespace.  Charging:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Metric
     - Meaning
   * - ``xte.v.c.hlcstate`` / ``xte.v.c.piswraw``
     - The car's raw DC charge communication state and cable-seated signal, useful for
       following a session or diagnosing a charge that does not start
   * - ``xte.v.c.ac.opstatus`` / ``xte.v.c.chargerstate``
     - AC charging operation status, and the on-board charger status
   * - ``xte.v.c.ac.tgtpower`` / ``xte.v.c.ac.ilimit``
     - AC target charging power (kW) and AC current limit (A)
   * - ``xte.v.c.output`` / ``xte.v.c.outputtarget``
     - Charger output and target power (kW)
   * - ``xte.v.c.ac.usable``
     - "A/C usable power" (kW) reported during AC charging; its exact meaning is not yet
       confirmed
   * - ``xte.v.c.gridpower``
     - Power drawn from the grid (kW); AC charging only, the grid-side companion to
       ``v.c.power``
   * - ``xte.v.c.permpower`` / ``xte.v.c.tgtcurrent``
     - The minimum charging power the car accepts — the floor of the DC charge curve — and the
       charging current it asks for
   * - ``xte.v.c.dc.maxpower`` / ``xte.v.c.dc.maxcurrent`` /
       ``xte.v.c.dc.maxvoltage``
     - The DC station's advertised maximum power, current and voltage
   * - ``xte.v.c.outcome``
     - Why the last charging session ended.  The car keeps this value between sessions, so it
       may still show the previous session's outcome early in a new one
   * - ``xte.v.c.stoprequest``
     - Reason the car requested a DC charge to stop (partially decoded)
   * - ``xte.v.c.myroom``
     - My Room active — the cabin running on grid power while plugged in
   * - ``xte.s.controlstate``
     - The car's own operating mode (0 none, 1 driving, 3 charging)

Climate and cabin energy:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Metric
     - Meaning
   * - ``xte.v.e.hvac.power``
     - HVAC / cabin power draw (kW), while driving and while charging
   * - ``xte.v.e.hvac.kwh``
     - My Room cabin energy (kWh) used this charging session, AC or DC
   * - ``xte.v.e.hvac.kwh.drive``
     - Cabin/HVAC energy (kWh) used this trip, reset at trip start
   * - ``xte.v.e.awd``
     - AWD / X-MODE status (not yet confirmed on a vehicle)

HV battery:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Metric
     - Meaning
   * - ``xte.v.b.soc.bms``
     - SOC as the battery management system reports it.  This differs from the displayed
       ``v.b.soc``: the displayed figure spans only the usable part of the pack and reaches
       100 % at roughly 95 % BMS SOC

12V auxiliary battery, as reported by the car:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Metric
     - Meaning
   * - ``xte.v.b.12v.voltage``
     - 12V battery voltage as measured by the car, alongside the module's own reading in
       ``v.b.12v.voltage``
   * - ``xte.v.b.12v.temp`` / ``xte.v.b.12v.cac``
     - 12V battery temperature (°C) and full-charge capacity (Ah)
   * - ``xte.v.b.12v.charge.ah`` / ``xte.v.b.12v.discharge.ah`` /
       ``xte.v.b.12v.readyon.h``
     - Lifetime charge and discharge totals (Ah) and total Ready-ON time (h)

----------
Debug Logs
----------

To see state changes, metric updates and data read from the car in the log, run
``log level verbose v-etnga``.  Use this tag on both the Solterra and the bZ4X; their own log tags
(``v-subsol`` / ``v-toybz4x``) only log startup and shutdown.
