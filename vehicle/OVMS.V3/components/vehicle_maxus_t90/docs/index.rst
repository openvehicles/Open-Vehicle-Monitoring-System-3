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
     - Primary BMS PID ``0xD117`` (0.1% resolution), VCU PID ``0xE002`` fallback
   * - SOH display
     - Yes
     - From OBD-II PID ``0xE003`` → ``ms_v_bat_soh`` (with filtering)
   * - Battery capacity
     - Yes
     - Custom metric ``xmt.b.capacity`` (fixed 88.5 kWh)
   * - Pack Voltage & Current
     - Yes
     - BMS PIDs ``0xB11E`` (0.1 V) and ``0xB105`` (0.1 A)
   * - Cell Temperatures
     - Yes
     - BMS PID ``0xB110`` (Max / Min / Avg)
   * - Odometer
     - Yes
     - From CAN ID ``0x540`` → ``ms_v_pos_odometer`` (0.1 km resolution)
   * - Vehicle READY / ignition state
     - Yes
     - From OBD-II PID ``0xE015`` → ``ms_v_env_on``
   * - Lock status
     - Yes
     - From CAN ID ``0x281`` → ``ms_v_env_locked`` (locked/unlocked)
   * - Door & Light status
     - Yes
     - From CAN ID ``0x375`` (doors and headlights)
   * - Charge state / power / energy
     - Yes
     - Derived from VCU state (``0xE018``) & pack current; tracks session kWh
   * - Cabin / HVAC status
     - Yes
     - From CAN ID ``0x510`` (fan speed, setpoint) and OBD-II PID ``0xE010``
   * - Ambient temperature
     - Yes
     - From OBD-II PID ``0xE025`` → ``ms_v_env_temp``
   * - GPS location
     - Yes
     - Provided by the OVMS modem GPS (not vehicle-specific)
   * - Speed display
     - No (vehicle-specific)
     - Only GPS-based speed available via OVMS core
   * - Charge control (start/stop, limits)
     - No
     - Not yet implemented
   * - Pre-heat / HVAC remote control
     - No
     - Not yet implemented


Implementation Notes
--------------------

* The module derives from ``OvmsVehicleOBDII`` and registers CAN1 at 500 kbps in active mode.
* Polling is done on ECU ``0x7E3 / 0x7EB`` (VCU) and ``0x748 / 0x7C8`` (BMS) using extended OBD-II PIDs.
* The poller defines three poll states:

  * State 0: vehicle off (polls ignition ``0xE015`` every 5s, BMS SOC ``0xD117`` every 60s)
  * State 1: vehicle on / driving
  * State 2: car off, but charging (HV active ``0xE018``)

* Multi-frame OBD-II response for VIN (PID ``0xF190``) is reassembled across 3 CAN frames into a full 17-character VIN.
* Odometer is taken from CAN ID ``0x540`` using bytes [4..6] as a 24-bit little-endian value with 0.1 km resolution.
* Lock status is decoded from CAN ID ``0x281`` (body control module) using byte 1 values (``0xA9`` → locked, ``0xA8`` → unlocked).
* Door & light status is decoded from CAN ID ``0x375``; cabin fan & temperature setpoint from CAN ID ``0x510``.
* Multiple sanity filters are applied to SOH and temperature PIDs to discard default/bogus values when the vehicle is off.
