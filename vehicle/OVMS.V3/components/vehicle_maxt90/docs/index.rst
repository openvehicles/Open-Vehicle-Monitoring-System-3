Maxus T90 EV (MT90)
===================

Vehicle type code: ``MT90``

The Maxus T90 EV module provides basic battery, temperature and lock/odometer
integration using the vehicle OBD-II port and a single CAN bus at 500 kbps.

The implementation is still under active development; this page reflects the
current feature set in the initial version of the module.


Hardware & Installation
-----------------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Item
     - Notes
   * - OVMS hardware
     - OVMS v3 module (or later)
   * - Vehicle connection
     - OBD-II port using the standard OVMS OBD-II to DB9 data cable
   * - CAN bus
     - CAN1 at 500 kbps, active mode
   * - GPS / GSM antennas
     - Standard OVMS antennas (or compatible) as per OVMS documentation


Feature Coverage
----------------

.. list-table::
   :widths: 45 15 40
   :header-rows: 1

   * - Function
     - Status
     - Notes
   * - SOC display
     - Yes
     - From OBD-II PID ``0xE002`` → ``ms_v_bat_soc``
   * - SOH display
     - Yes
     - From OBD-II PID ``0xE003`` → ``ms_v_bat_soh`` (with filtering)
   * - Battery capacity
     - Yes
     - Custom metric ``xmt.b.capacity`` (fixed 88.5 kWh)
   * - Odometer
     - Yes
     - From CAN ID ``0x540`` → ``ms_v_pos_odometer`` (0.1 km resolution)
   * - Vehicle READY / ignition state
     - Yes
     - From OBD-II PID ``0xE004`` → ``ms_v_env_on`` and poll state control
   * - Lock status
     - Yes
     - From CAN ID ``0x281`` → ``ms_v_env_locked`` (locked/unlocked)
   * - Charge plug / pilot present
     - Yes
     - From OBD-II PID ``0xE009`` → ``ms_v_charge_pilot``
   * - Cabin / coolant temperature
     - Yes
     - From OBD-II PID ``0xE010`` → custom metric ``xmt.v.hvac.temp``
   * - Ambient temperature
     - Yes
     - From OBD-II PID ``0xE025`` → ``ms_v_env_temp``
   * - GPS location
     - Yes
     - Provided by the OVMS modem GPS (not vehicle-specific)
   * - Speed display
     - No (vehicle-specific)
     - Only GPS-based speed available via OVMS core
   * - Charge state / power / energy
     - No
     - Not yet implemented for this vehicle
   * - Charge control (start/stop, limits)
     - No
     - Not yet implemented
   * - Charging interruption alerts
     - No
     - Not yet implemented
   * - Trip counters / consumption
     - No
     - Not yet implemented for MT90
   * - TPMS
     - No
     - No TPMS integration yet
   * - Door/window state
     - No
     - Only global lock/unlock is currently decoded
   * - Remote lock/unlock control
     - No
     - Read-only lock status only
   * - Pre-heat / HVAC remote control
     - No
     - Not yet implemented
   * - Valet mode
     - No
     - Not implemented for this vehicle
   * - AC / DC charge mode detection
     - No
     - Not yet implemented


Implementation Notes
--------------------

* The module derives from ``OvmsVehicleOBDII`` and registers CAN1 at
  500 kbps in active mode.
* Polling is done on ECU ``0x7E3 / 0x7EB`` using extended OBD-II PIDs.
* The poller currently defines three poll states:
  
  * State 0: vehicle off  
  * State 1: vehicle on / driving  
  * State 2: charging (reserved for future use)

* In the initial implementation only the READY flag (PID ``0xE004``) is polled
  in state 0 to avoid keeping ECUs awake while parked; other PIDs are only
  polled when the vehicle is on.
* The READY bitfield is used to drive ``ms_v_env_on`` and to switch poll
  states between 0 (off) and 1 (on).
* Odometer is taken from CAN ID ``0x540`` using bytes [4..6] as a 24-bit
  little-endian value with 0.1 km resolution.
* Lock status is decoded from CAN ID ``0x281`` (body control module) using
  byte 1 values:

  * ``0xA9`` → locked  
  * ``0xA8`` → unlocked  

  and mapped to the standard metric ``ms_v_env_locked``.

* Multiple sanity filters are applied to SOH and temperature PIDs to discard
  default/bogus values that occur when the vehicle is off or the ECU returns
  fallback frames.


Planned / Potential Extensions
------------------------------

The following features are candidates for future updates once the relevant
PIDs and CAN messages have been fully reverse engineered:

* Charge state / mode / power and energy counters.
* Distinguishing AC vs. DC charging.
* More detailed BMS data (cell voltages, min/max temperatures, etc.).
* Additional body / door / window state.
* Remote climate control and other remote vehicle actions, if feasible.
