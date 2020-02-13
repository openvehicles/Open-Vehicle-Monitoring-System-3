===============
Custom Commands
===============

.. note::
  This is currently just a brief overview to feed the search engine.
  See each command's usage info and where applicable corresponding V2 manual entries on details.
  
  * V3 commands need to be in lower case.
  * When accessing via USB terminal, first issue enable (login).
  * V3 commands are similar to V2 commands, just structured slightly different.
  * Try ``help``, ``?`` and ``xrt ?``. Twizy commands are subcommands of ``xrt``.
  * TAB only works via USB / SSH. Shortcuts can be used generally, e.g.  ``mo t`` instead of ``module tasks``.
  * A usage info is shown if a command syntax is wrong or incomplete.

.. code-block:: none

  OVMS# xrt ?
  batt                 Battery monitor
  ca                   Charge attributes
  cfg                  SEVCON tuning
  dtc                  Show DTC report / clear DTC
  mon                  SEVCON monitoring
  obd                  OBD2 tools
  power                Power/energy info


---------------
Battery Monitor
---------------

.. code-block:: none

  OVMS# xrt batt ?
  data-cell            Output cell record
  data-pack            Output pack record
  reset                Reset alerts & watches
  status               Status report
  tdev                 Show temperature deviations
  temp                 Show temperatures
  vdev                 Show voltage deviations
  volt                 Show voltages

.. note:: Also see standard BMS commands (``bms ?``).


-----------------
Charge Attributes
-----------------

.. code-block:: none

  OVMS# xrt ca ?
  Usage: xrt ca [R] | [<range>] [<soc>%] [L<0-7>] [S|N|H]

.. note:: Also see standard charge control commands (``charge ?``).


-------------
SEVCON Tuning
-------------

.. code-block:: none

  OVMS# xrt cfg ?
  brakelight           Tune brakelight trigger levels
  clearlogs            Clear SEVCON diag logs
  drive                Tune drive power level
  get                  Get tuning profile as base64 string
  info                 Show tuning profile
  load                 Load stored tuning profile
  op                   Leave configuration mode (go operational)
  power                Tune torque, power & current levels
  pre                  Enter configuration mode (pre-operational)
  querylogs            Send SEVCON diag logs to server
  ramplimits           Tune max pedal reaction
  ramps                Tune pedal reaction
  read                 Read register
  recup                Tune recuperation power levels
  reset                Reset tuning profile
  save                 Save current tuning profile
  set                  Set tuning profile from base64 string
  showlogs             Display SEVCON diag logs
  smooth               Tune pedal smoothing
  speed                Tune max & warn speed
  tsmap                Tune torque/speed maps
  write                Read & write register
  writeonly            Write register


---------------------------
Show DTC Report / Clear DTC
---------------------------

.. code-block:: none

  OVMS# xrt dtc ?
  clear                Clear stored DTC in car
  reset                Reset OVMS DTC statistics
  show                 Show DTC report


-----------------
SEVCON Monitoring
-----------------

.. code-block:: none

  OVMS# xrt mon ?
  reset                Reset monitoring
  start                Start monitoring
  stop                 Stop monitoring


----------
OBD2 Tools
----------

.. code-block:: none

  OVMS# xrt obd ?
  request              Send OBD2 request, output response
  OVMS# xrt obd request ?
  bms                  Send OBD2 request to BMS
  broadcast            Send OBD2 request as broadcast
  charger              Send OBD2 request to charger (BCB)
  cluster              Send OBD2 request to cluster (display)
  device               Send OBD2 request to a device


-----------------
Power/Energy Info
-----------------

.. code-block:: none

  OVMS# xrt power ?
  report               Trip efficiency report
  stats                Generate RT-PWR-Stats entry
  totals               Power totals


