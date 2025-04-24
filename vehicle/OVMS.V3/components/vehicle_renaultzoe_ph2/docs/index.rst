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
   * - **xrz2.v.e.waterpump.lifetime.left**
     - Waterpump lifetime left counter in percent
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
     - Manually stops ISO-TP polling (for debugging). If the Zoe does not enter sleep mode, use this and report the issue.
   * - **xrz2 poller start**
     - Manually starts ISO-TP polling. The correct poller mode is determined by ECU status.
   * - **xrz2 poller inhibit**
     - Disables the poller regardless of wakeup packets received or not, for vehicle maintenanace.
   * - **xrz2 poller resume**
     - Enables poller for normal operation if previously inhibited.
   * - **xrz2 unlock trunk***
     - Unlocks only the trunk.
   * - **xrz2 unlock chargeport***
     - Open the charge port flap and if charging running it will stopped. (EXPERIMENTAL)
   * - **xrz2 debug**
     - Debug output of custom functions like rolling average consumption calculation.
   * - **xrz2 ddt***
     - Various commands to modify configuration of ECUs connected to the V-CAN like DDT did.

(* Requires V-CAN connection)

DDT commands should be executed with ignition on or after ignition feed and in parked state. You can enable and disable compressor, reset service and waterpump counter.

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

(* Requires V-CAN connection)

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