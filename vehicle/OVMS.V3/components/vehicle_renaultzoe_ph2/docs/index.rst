===================
Renault Zoe Phase 2
===================

Vehicle Type: **RZ2**

This module supports the Renault Zoe Phase 2 ZE40/ZE50 with 41/52kWh batteries (2019-2024).

----------------
Connection Types
----------------

The Zoe PH2 can be connected to OVMS in two ways:

**OBD-II Connection** (Standard)
  Connect via the OBD-II port using a standard OBD-II to DB9 cable. This provides basic monitoring capabilities.

**V-CAN Connection** (Advanced)
  Connect directly to the vehicle's main CAN bus by tapping into the BCM or gateway. This unlocks all advanced features including remote climate control, door lock/unlock, and enhanced metrics.

  **Note:** All features marked with * below require a V-CAN connection.

----------------
Support Overview
----------------

.. list-table::
   :header-rows: 1

   * - Function
     - Support Status
   * - **Hardware**
     - OVMS v3.3
   * - **Vehicle Cable**
     - OBD-II to DB9 Data Cable for OVMS (Generic OBD2 Cable - left)
   * - **GSM Antenna**
     - 1000500 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
   * - **GPS Antenna**
     - 1020200 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
   * - **SOC Display**
     - Yes
   * - **Range Display**
     - Yes
   * - **GPS Location**
     - Yes (via modem module GPS)
   * - **Speed Display**
     - Yes
   * - **Temperature Display**
     - Yes
   * - **BMS Voltage/Temperature Display**
     - Yes
   * - **TPMS Display**
     - Yes
   * - **Charge Status Display**
     - Yes
   * - **Charge Interruption Alerts**
     - Yes
   * - **Charge Control**
     - No (impossible to implement, firmware of ECUs doesnt support it)
   * - **Cabin Pre-heat/Cool Control**
     - Yes*
   * - **Lock/Unlock Vehicle**
     - Yes*
   * - **Valet Mode Control**
     - No

(* Requires V-CAN connection)

-----------------
Getting Connected
-----------------

**OBD-II Connection**

1. Use a standard OBD-II to DB9 cable (left pin configuration)
2. Connect to the OBD-II port under the dashboard
3. Configure vehicle type as RZ2 in OVMS
4. Basic monitoring is now active

**V-CAN Connection**

The V-CAN (Vehicle CAN) is the main communication bus connecting the BCM, HVAC, instrument cluster, and other critical ECUs.

Benefits of V-CAN connection:

- Accurate wake-up detection without relying on the TCU
- Remote control functions (climate, locking, etc.)
- Enhanced metrics from vehicle ECUs
- Access to diagnostic (DDT) commands

To connect to V-CAN, you need to tap into the CAN bus at the BCM or gateway module. This requires opening the vehicle and connecting directly to the CAN wires.

**Important:** If you disable or remove the Telematics Control Unit (TCU), a V-CAN connection becomes essential for proper wake-up detection.

----------------------
Battery & Energy Setup
----------------------

**Battery Capacity**

Configure your battery size in the web interface under **Renault Zoe Ph2 Setup**:

- ZE40: 41 kWh
- ZE50: 52 kWh (default)

**Energy Calculation Method**

You can choose between two methods:

**OVMS Calculation** (Recommended for older BMS firmware ≤530)
  OVMS calculates energy values internally. Use this if your BMS firmware is version 530 or lower, as these versions miscalculate energy leading to accelerated SOH decline.

**BMS Calculation** (Recommended for newer firmware >530)
  Uses energy values directly from the Battery Management System. Recommended for updated firmware.

To update BMS firmware, visit a Renault dealer with CLIP and Token access. **Important:** Ensure "COMPUTER DATA" is not rewritten after flashing to prevent issues.

------------------
Remote Control
------------------

All remote control features require a V-CAN connection.

**Climate Control**

Pre-heat or pre-cool your cabin before driving:

**Requirements:**

- Vehicle must be locked
- Battery SOC must be above 15%
- V-CAN connection required

**Methods:**

1. **OVMS App**: Use the climate control button
2. **Shell Command**: ``climatecontrol on``
3. **Scheduled**: Configure weekly schedules (see Scheduled Preconditioning below)
4. **Key Fob**: Press "Remote Lighting" button if configured (see Web Configuration below)

**Extended Climate Control**

For longer preconditioning sessions:

- **homelink 2**: Runs climate control for ~30 minutes (3 cycles)
- **homelink 3**: Cancels extended session

The extended session will also stop if you unlock the vehicle.

**Door Lock/Unlock**

Control your doors remotely:

- **Lock**: ``lock`` command
- **Unlock**: ``unlock`` command
- **Trunk Only**: ``xrz2 unlocktrunk``

These work like the original key fob functions. Standard OVMS lock/unlock commands use the module PIN if PIN protection is enabled.

**Headlights**

Activate headlights for ~30 seconds:

- **Command**: ``xrz2 lighting``
- **Coming Home Feature**: Automatically activates when locking if configured (see Web Configuration)

------------------------
Scheduled Preconditioning
------------------------

Set up automatic climate control using the global OVMS scheduler.

**Configuration:**

- Web UI: **Renault Zoe Ph2 → Preconditioning Schedule**
- Shell: ``climatecontrol schedule`` commands

**Features:**

- Multiple time slots per day
- Individual schedules for each weekday
- Optional duration per slot (5-30 minutes)
- Copy schedules between days
- Global enable/disable switch

**Example Commands:**

::

  # Set Monday schedule with durations
  climatecontrol schedule set mon 07:30/10,17:45/15

  # Copy to rest of work week
  climatecontrol schedule copy mon tue-fri

  # Enable scheduler
  climatecontrol schedule enable

  # Check status
  climatecontrol schedule list

**Requirements:**

- V-CAN connection
- Vehicle must be locked
- Battery SOC must be above 15%
- Scheduler must be enabled

-----------------------
12V Battery Management
-----------------------

Keep your 12V battery healthy with automatic and manual charging features. All features require V-CAN connection.

**Automatic Recharge**

Automatically starts the DC/DC converter when your 12V battery voltage drops:

- Configurable voltage threshold (11.0-14.0V, default: 12.4V)
- Runs for up to 30 minutes
- Only works when vehicle is locked and not charging
- Stops automatically when unlocked, ignition turns on, or charging starts

**After-Ignition Keep-Alive**

Keeps the DC/DC running after turning off the ignition:

- Configurable duration (0-60 minutes, default: 0 = disabled)
- Reduces contactor wear on short stops
- Improves ECU boot time
- Automatically stops if charging starts or external preconditioning is detected
- Smart integration: OVMS-triggered climate control automatically disables this to prevent conflicts

**Manual DC/DC Control**

Manually control the DC/DC converter with safety limits:

- **Enable**: ``xrz2 dcdc enable``
- **Disable**: ``xrz2 dcdc disable``
- **Status**: ``xrz2 debug``

**Safety Limits:**

- SOC limit: Stops at configured percentage (default: 80%)
- Time limit: Stops after configured minutes (default: unlimited)
- Automatic shutdown on ignition, charging, or unlocking

**Configuration**

All 12V battery features are configured via **Renault Zoe Ph2 Setup** in the web UI.

**Troubleshooting**

Use ``xrz2 debug`` to check:

- Current DC/DC state and mode
- Runtime and configuration
- Vehicle state (ignition, charging, SOC, voltage)

If experiencing issues:

- Check if after-ignition keep-alive is active (set to 0 to disable)
- Enable debug logs: ``log level debug v-zoe-ph2``
- Look for "DCDC:" messages in logs
- Check for preconditioning detection messages if DC/DC stops unexpectedly

-------------------
Advanced Diagnostics
-------------------

The Zoe PH2 integration includes DDT (Diagnostic Tool) commands for ECU configuration.

**Requirements:**

- V-CAN connection
- Ignition ON (or ignition feed activated)
- Vehicle in PARK

**Important:** All DDT commands make permanent changes to ECU settings. Use with caution.

**HVAC Control**

**Compressor:**

::

  xrz2 ddt hvac compressor enable   # Allow cooling & heat pump
  xrz2 ddt hvac compressor disable  # PTC heating only

**Use case:** Disable a faulty compressor to use PTC heating only during winter.

**PTC Heaters:**

::

  xrz2 ddt hvac ptc enable    # Force all PTC elements on
  xrz2 ddt hvac ptc disable   # Return to automatic control


**Service Reset**

::

  xrz2 ddt cluster reset service   # Clear service reminder


-----------------
Web Configuration
-----------------

Configure your Zoe via the web interface at **Renault Zoe Ph2 Setup**.

**Battery Settings**

- Battery size (ZE40/ZE50)
- Ideal range (80-500 km)
- Energy calculation method (OVMS/BMS)

**V-CAN Features**

- Enable V-CAN connection
- Coming home function (headlights on lock)
- Remote climate via key fob (repurpose "Remote Lighting" button)
- Alternative unlock command for vehicles that do not respond to the default unlock sequence

**12V Battery Management**

- Auto-recharge enable/disable
- Voltage threshold (11.0-14.0V)
- Manual mode SOC limit (5-100%)
- Manual mode time limit (0-999 min)
- After-ignition duration (0-60 min)

**Charging Notifications**

- PV/Solar charging mode (suppress intermediate notifications)

**HVAC Auto-Enable PTC**

- Auto-enable PTC heaters based on outside temperature
- Minimum temperature threshold (-20.0 to 30.0°C)
- Maximum temperature threshold (-20.0 to 30.0°C)

**Other Pages**

- **BMS View**: Monitor individual cell voltages and temperatures
- **Preconditioning Schedule**: Set up automatic climate control

-----------------------
PV/Solar Charging Mode
-----------------------

When using a solar/PV-controlled wallbox, charging may start and stop multiple times per day based on available solar power. The PV charging mode suppresses individual notifications and provides a single summary when you unplug.

**How It Works**

- Individual charge start/stop notifications are suppressed while cable is plugged in
- Total energy (kWh) is accumulated across all charge sessions
- A summary notification is sent when you unplug the cable

**Summary Notification Includes:**

- Total kWh charged across all sessions
- SOC change (start % → end %)
- Number of charge sessions
- Total duration plugged in

**Configuration:**

Enable via web UI at **Renault Zoe Ph2 Setup** → **Charging Notifications** → **PV/Solar charging mode**

Or via shell:

::

  config set xrz2 pv_charge_notification yes

**Notes:**

- If you unplug without any charging sessions occurring, no summary is sent
- Session tracking resets when you plug in the cable
- System restart while plugged: session starts at boot time, not actual plug time

-------------------------
Automatic PTC Activation
-------------------------

The Zoe PH2 integration can automatically enable the PTC heaters based on outside temperature.

**Conditions**

- V-CAN connection required
- Ignition ON
- Vehicle not charging
- Cabin blower enabled
- Outside temperature within the configured range

**Configuration**

Enable via web UI at **Renault Zoe Ph2 Setup** → **HVAC Auto-Enable PTC**

Config keys:

- ``xrz2 auto_ptc_enabled`` - enable or disable automatic PTC control
- ``xrz2 auto_ptc_temp_min`` - lower temperature threshold
- ``xrz2 auto_ptc_temp_max`` - upper temperature threshold

**Notes**

- Default range is 9.5°C to 20.0°C
- If the maximum threshold is configured below the minimum threshold, the implementation clamps it to the minimum value
- Leaving the configured temperature range does not automatically disable the PTC heaters
- Turning the cabin blower off disables the PTC heaters, regardless of whether they were enabled manually or automatically
- Ignition OFF automatically disables the PTC heaters again

------------------
Additional Metrics
------------------

Beyond standard OVMS metrics, the Zoe PH2 integration provides:

**Battery Metrics**

- ``xrz2.b.cycles`` - SOC cycles from BMS
- ``xrz2.b.max.charge.power`` - Max charge/regen power from BMS
- ``xrz2.b.chg.start`` - Charge session energy tracking
- ``xrz2.b.used.start`` - Trip energy tracking
- ``xrz2.b.recd.start`` - Recuperation tracking

**Charging Metrics**

- ``xrz2.c.main.phases`` - Charging phases (single/three)
- ``xrz2.c.main.phases.num`` - Phase count as number
- ``xrz2.c.main.power.available`` - EVSE available power

**HVAC Metrics**

- ``xrz2.h.ptc.state`` - Persistent PTC target/state flag
- ``xrz2.h.compressor.mode`` - Compressor operating mode
- ``xrz2.h.compressor.power`` - Compressor power draw (W)
- ``xrz2.h.compressor.pressure`` - Refrigerant pressure (bar)
- ``xrz2.h.compressor.speed`` - Compressor speed
- ``xrz2.h.compressor.speed.req`` - Requested compressor speed
- ``xrz2.h.compressor.temp.discharge`` - Compressor discharge temperature (°C)
- ``xrz2.h.compressor.temp.suction`` - Compressor suction temperature (°C)
- ``xrz2.h.cooling.evap.setpoint`` - Evaporator setpoint temperature (°C)
- ``xrz2.h.cooling.evap.temp`` - Evaporator temperature (°C)
- ``xrz2.h.heating.cond.setpoint`` - Condenser setpoint temperature (°C)
- ``xrz2.h.heating.cond.temp`` - Condenser temperature (°C)
- ``xrz2.h.heating.ptc.req`` - PTC power level (0-5)
- ``xrz2.h.heating.temp`` - Output air temperature

**Motor/Inverter Metrics**

- ``xrz2.i.current`` - Inverter HV current (Amps)
- ``xrz2.i.voltage`` - Inverter HV voltage (Volts)
- ``xrz2.m.inverter.status`` - Inverter status string
- ``xrz2.m.temp.stator1`` - Stator temperature sensor 1 (°C)
- ``xrz2.m.temp.stator2`` - Stator temperature sensor 2 (°C)
- ``xrz2.m.temp.rotor.raw`` - Rotor temperature raw estimation (°C)
- ``xrz2.m.temp.rotor`` - Rotor temperature estimated (°C)
- ``xrz2.m.current.stator.u`` - Stator current phase U (Amps)
- ``xrz2.m.current.stator.v`` - Stator current phase V (Amps)
- ``xrz2.m.current.stator.w`` - Stator current phase W (Amps)
- ``xrz2.m.current.rotor1`` - Rotor current sensor 1 (Amps)
- ``xrz2.m.current.rotor2`` - Rotor current sensor 2 (Amps)
- ``xrz2.m.rotor.resistance`` - Rotor reference resistance (Ohms)
- ``xrz2.m.rotor.voltage`` - Rotor excitation voltage, calculated as I×R (Volts)

**System Metrics**

- ``xrz2.v.bus.awake`` - CAN bus active
- ``xrz2.v.c.12v.dcdc.load`` - DC/DC converter load (%)
- ``xrz2.v.b.consumption.aux`` - HVAC trip consumption (kWh)
- ``xrz2.v.poller.inhibit`` - Poller inhibit state
- ``xrz2.v.pos.odometer.start`` - Odometer at trip start (km)

**Notes**

Persistent helper metrics such as ``xrz2.b.chg.start``, ``xrz2.b.used.start``, ``xrz2.b.recd.start`` and ``xrz2.v.pos.odometer.start`` are used internally for session and trip calculations, but can also be inspected for debugging.

-------------
Shell Commands
-------------

**Poller Control**

::

  xrz2 poller start     # Start ISO-TP polling
  xrz2 poller stop      # Stop polling
  xrz2 poller inhibit   # Disable for maintenance
  xrz2 poller resume    # Re-enable after inhibit

**Debug & Status**

::

  xrz2 debug            # Show comprehensive status (DC/DC, config, vehicle state)

**Vehicle-Specific Commands**

::

  xrz2 lighting         # Trigger headlights / coming-home lighting
  xrz2 unlocktrunk      # Unlock trunk / tailgate
  xrz2 dcdc enable      # Enable DC/DC converter manually
  xrz2 dcdc disable     # Disable DC/DC converter

**DDT Commands** (see Advanced Diagnostics section)

-----------------------
Technical: CAN Security
-----------------------

The Renault Zoe Phase 2 uses a CAN security gateway, making the OBD port normally silent. OVMS must actively poll ECUs using UDS/ISO-TP, similar to diagnostic software.

**Implications:**

- OVMS cannot detect wake-up without active polling
- Continuous polling drains the 12V battery
- TCU requests to BMS allow OVMS to detect wake-up events (OBD connection)
- V-CAN connection provides better wake-up detection independent of TCU
- Commands cannot be sent via OBD port (security restriction)

This is why V-CAN connection is required for remote control features.

------------------
Community & Support
------------------

Forum: https://www.goingelectric.de/forum/viewtopic.php?p=2327071
