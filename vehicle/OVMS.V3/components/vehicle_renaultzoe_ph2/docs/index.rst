===================
Renault Zoe Phase 2
===================

Vehicle Type: **RZ2**

This vehicle type supports the Renault Zoe (PH2) ZE50 41/52kWh (2019-2024) via an OBD2 connection. Additional metrics and control functions, such as pre-climate control, are available when connected to the V-CAN instead of OBD port.

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

------------------------------
Additional Zoe Phase 2 Metrics
------------------------------

.. list-table::
   :header-rows: 1

   * - Name
     - Function
   * - **xrz2.b.chg.start**
     - Total charged kWh from BMS, used as a temporary variable to calculate charged energy.
   * - **xrz2.b.cycles**
     - SOC cycles from BMS.
   * - **xrz2.b.max.charge.power**
     - Maximum charging/recuperation power allowed by BMS.
   * - **xrz2.b.recd.start**
     - Total recuperated energy from BMS, used as a temporary variable to calculate trip energy.
   * - **xrz2.b.used.start**
     - Total used energy from BMS, used as a temporary variable to calculate trip energy consumption.
   * - **xrz2.c.main.phases**
     - Charging phase (single or three-phase) as a string.
   * - **xrz2.c.main.phases.num**
     - Charging phase (single or three-phase) as a number.
   * - **xrz2.c.main.power.available**
     - Available power reported by EVSE.
   * - **xrz2.h.compressor.mode**
     - Compressor mode (AC, heat pump, demist, idle).
   * - **xrz2.h.compressor.power**
     - Compressor power consumption.
   * - **xrz2.h.compressor.pressure**
     - Refrigerant discharge pressure.
   * - **xrz2.h.compressor.speed**
     - Compressor RPM.
   * - **xrz2.h.compressor.speed.req**
     - Requested compressor RPM from ClimBox in heat pump mode.
   * - **xrz2.h.compressor.temp.discharge**
     - Discharge refrigerant temperature.
   * - **xrz2.h.compressor.temp.suction**
     - Suction refrigerant temperature.
   * - **xrz2.h.cooling.evap.setpoint**
     - Target evaporator temperature in AC mode.
   * - **xrz2.h.cooling.evap.temp**
     - Current evaporator temperature in AC mode.
   * - **xrz2.h.heating.cond.setpoint**
     - Target condenser temperature in heat pump mode.
   * - **xrz2.h.heating.cond.temp**
     - Current condenser temperature in heat pump mode.
   * - **xrz2.h.heating.ptc.req**
     - Requested PTC power level (0-5). Zoe has three PTC elements with different power ratings.
   * - **xrz2.h.heating.temp**
     - Current heated output air temperature. Heat pump + PTC
   * - **xrz2.i.current**
     - Zoe inverter current.
   * - **xrz2.i.voltage**
     - Zoe inverter voltage (after cabling losses).
   * - **xrz2.m.inverter.status**
     - Inverter status.
   * - **xrz2.m.temp.stator1**
     - Motor stator temperature sensor 1.
   * - **xrz2.m.temp.stator2**
     - Motor stator temperature sensor 2.
   * - **xrz2.v.b.consumption.aux***
     - Compressor trip power consumption
   * - **xrz2.v.bus.awake**
     - Zoe CAN bus running, polling enabled.
   * - **xrz2.dcdc.load***
     - Current load of dc-dc converter in percent.
   * - **xrz2.v.poller.inhibit**
     - If enabled by command, the poller never starts to prevent conflict with tester while maintenanace.
   * - **xrz2.v.pos.odometer.start**
     - Total odometer value as a temporary variable to calculate trip distance.

(* Requires V-CAN connection)

-------------------------------
Additional Zoe Phase 2 Commands
-------------------------------

.. list-table::
   :header-rows: 1

   * - Name
     - Function
   * - **xrz2 poller stop**
     - Manually stops ISO-TP polling. If the Zoe does not enter sleep mode, use this and report the issue.
   * - **xrz2 poller start**
     - Manually starts ISO-TP polling. The correct poller mode is determined by ECU status.
   * - **xrz2 poller inhibit**
     - Disables the poller regardless of wakeup packets received or not, for hiding OVMS while vehicle maintenanace.
   * - **xrz2 poller resume**
     - Enables poller for normal operation if previously inhibited.
   * - **xrz2 unlock trunk***
     - Unlocks only the trunk.
   * - **xrz2 lighting***
     - Activates headlights for approximately 30 seconds (coming home function).
   * - **xrz2 dcdc enable***
     - Manually activates the DC/DC converter for 12V battery charging (30 minute timeout).
   * - **xrz2 dcdc disable***
     - Manually deactivates the DC/DC converter.
   * - **xrz2 debug**
     - Debug output of custom functions like rolling average consumption calculation.
   * - **xrz2 ddt hvac compressor enable***
     - Permanently enable the HVAC compressor (allows both cooling and heating modes).
   * - **xrz2 ddt hvac compressor disable***
     - Disable the HVAC compressor (PTC heating only, no cooling/heat pump).
   * - **xrz2 ddt hvac ptc enable***
     - Permanently enable all three PTC heating elements.
   * - **xrz2 ddt hvac ptc disable***
     - Reset PTC control to automatic mode.
   * - **xrz2 ddt cluster reset service***
     - Reset the service reminder in the instrument cluster.

(* Requires V-CAN connection)

---------------------
OVMS Default Commands
---------------------

.. list-table::
   :header-rows: 1

   * - Name
     - Function
   * - **wakeup***
     - Wakes up some ECUs on the V-CAN and enables polling (active for ~20 seconds, does not enable DC/DC).
   * - **lock***
     - Locks all doors like keyfob
   * - **unlock***
     - Unlocks all doors like keyfob
   * - **climatecontrol on***
     - Pre-climates the cabin to the set target temperature.
   * - **homelink 1***
     - Equivalent to `climatecontrol on`, used as a workaround for older apps without an AC button.
   * - **homelink 2***
     - Long pre conditioning, fires pre-climate three times in a row, about 30 mins runtime.
   * - **homelink 3***
     - Abort Long pre conditioning, it will also aborted if you unlock your Zoe.
   * - **climatecontrol schedule ...***
     - Global scheduler commands (``set``, ``list``, ``copy``, ``clear``, ``enable``, ``disable``) for automatic preconditioning.

(* Requires V-CAN connection)

-------------------------
Scheduled Preconditioning
-------------------------

The Zoe Ph2 integration now uses the global OVMS climate scheduler.

**Requirements:**

- V-CAN connection enabled
- Vehicle must be locked
- Battery SoC must be above 15%
- ``climatecontrol schedule`` must be enabled (either via the page or CLI)


--------------------------------
DDT Commands - ECU Configuration
--------------------------------

The Zoe Ph2 integration includes DDT (Diagnostic Tool) commands that allow you to modify ECU configurations directly, similar to Renault's DDT software. These commands use ISO-TP diagnostic services to change persistent settings in various ECUs.

**IMPORTANT NOTES:**

- DDT commands should be executed with **ignition on** (or after activating ignition feed) and in **parked state**
- Changes are **permanent** and persist across power cycles
- Some changes may require an ECU reset to take effect
- Commands will retry up to 3 times if unsuccessful
- V-CAN connection is required

**HVAC Compressor Control**

Enable compressor (allows cooling and heat pump heating)::

  xrz2 ddt hvac compressor enable

Disable compressor (PTC heating only, no cooling/heat pump)::

  xrz2 ddt hvac compressor disable

**Use case:** If your compressor is faulty, you can disable it to use PTC heating only during winter.

**HVAC PTC Control**

Enable all PTC heaters permanently::

  xrz2 ddt hvac ptc enable

Reset PTC control to automatic mode::

  xrz2 ddt hvac ptc disable

**What it does:**

The Zoe has three PTC (Positive Temperature Coefficient) heating elements with different power ratings. This command sends configuration changes to the BCM (Body Control Module) to enable/disable all three PTC elements.

- **Enable**: Activates all three PTC heaters for maximum heating power
- **Disable**: Resets to automatic PTC control (HVAC decides based on temperature, the PTCs will be disabled if outside temp is above 9,5C regardless if the compressor is enabled or not)

**Instrument Cluster Service Reset**

Reset service reminder::

  xrz2 ddt cluster reset service

**Command Implementation Details:**

**Example Output:**

When running ``xrz2 ddt hvac compressor enable``::

  Try to send HVAC compressor enable command
  Extended session command sent successfully
  Hot loop enable command sent successfully
  Cold loop enable command sent successfully
  ECU reset command sent successfully
  Compressor enabled successfully (hot and cold loops active)

If a command fails::

  ERROR: Hot loop enable command failed
  WARNING: Some compressor enable commands failed - check logs

---------------------------
Web Interface Configuration
---------------------------

The following features can be configured via the OVMS web interface under **Renault Zoe Ph2 Setup**:

**Coming Home Function** 
  When enabled, the headlights automatically turn on for ~30 seconds after locking the car if the headlights were on when the ignition was turned off. This provides lighting when leaving your car at night.

  - Triggers when car is locked (via key fob, app, or OVMS command)
  - Checks if headlights were on before driver door was opened
  - Activates lighting for approximately 30 seconds

**Remote Climate Trigger via Key Fob** 
  When enabled, pressing the "Remote Lighting" button on your key fob triggers climate control (pre-heat/pre-cool) instead of activating the lights. This provides a convenient way to start climate control without using the app.

  - Monitors HFM (Hand Free Module) commands on V-CAN
  - Detects "Remote Lighting" button press
  - Automatically triggers climate control function
  - Follows all standard climate control requirements (car must be locked, SoC > 15%, etc.)

**Automatic 12V Battery Recharging** 
  When enabled, the DC/DC converter automatically activates to recharge the 12V battery when the vehicle is locked and the voltage drops below the configured threshold.

  - Configurable voltage threshold (11.0V - 14.0V, default 12.4V)
  - Activates only when vehicle is locked
  - Automatically stops after 30 minutes
  - Automatically stops when vehicle is unlocked
  - Manual override available via ``xrz2 dcdc enable/disable`` commands

**Scheduled Pre-Climate Configuration**
  A dedicated web page for managing weekly preconditioning schedules is available at **Renault Zoe Ph2 → Preconditioning Schedule**.

-------------------------------------------
CAN security gateway & OBD port limitations
-------------------------------------------

The Renault Zoe Phase 2 has a CAN security gateway, making the OBD port normally silent. All OVMS metrics must be polled through UDS/ISO-TP, similar to OEM diagnostic software. The CAN gateway forwards these polls to the appropriate ECU.

- OVMS cannot determine if the vehicle is awake unless polling is actively triggered.
- Continuous polling drains the 12V battery.
- Due to CAN security restrictions, commands cannot be sent via OBD port. A V-CAN connection is required for remote control functions.
- The vehicle’s Telematics Control Unit (TCU) requests battery statistics from the BMS, allowing OVMS to detect wake-up events.

If you want to disable or remove the TCU a V-CAN connection is absolutely neccessary.

----------------
V-CAN connection
----------------

The primary vehicle CAN (V-CAN) connects critical ECUs such as BCM, HVAC, Instrument Cluster and more. It can be connected to the OVMS module instead of OBD port connection.

A V-CAN connection enables:

- **Accurate wake-up detection** (without relying on the TCU).
- **Additional control functions** (e.g., pre-climate control, unlocking).
- **Metrics like Dashboard-SOC, Charge time remaining and estimated range** (grabbed from Zoes ECUs instead of calculation within OVMS)

---------------------------
BMS firmware recommendation
---------------------------

If your BMS firmware is **530 or lower**, enable "OVMS Energy Calculation" in the setup menu. Older firmware versions miscalculate energy values, leading to an accelerated SoH decline and extended "0 km range" driving.

For an upgrade, visit a Renault dealer with CLIP and Token access. Ensure **"COMPUTER DATA" is not rewritten after flashing** to prevent issues.

------------------
Community channels
------------------

Forum: https://www.goingelectric.de/forum/viewtopic.php?p=2327071