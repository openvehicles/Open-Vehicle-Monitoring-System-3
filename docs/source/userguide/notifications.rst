.. highlight:: none

=============
Notifications
=============

Notifications can be simple text messages or transport structured data. To distinguish by their 
purpose and origin, notifications have a **type** and a **subtype**.

Notifications are sent by the module via the available communication **channels** (client/server 
connections). If a channel is temporarily down (e.g. due to a connection loss), the notifications 
for that channel will be kept in memory until the channel is available again. (Note: this message 
queue does not survive a crash or reboot of the module.)

Channels process notifications differently depending on the way they work. For 
example, a v2 server will forward text notifications as push messages to connected smart phones 
and email readers, a v3 server will publish them under an MQTT topic, and the webserver will 
display the message as a modal dialog. See the respective manual sections for details.


**Notification types currently defined:**

  - ``info`` -- informational text messages
  - ``alert`` -- alert text messages
  - ``error`` -- error code messages
  - ``data`` -- historical data records, usually CSV formatted
  - ``stream`` -- live data streaming (high bandwidth), usually JSON formatted

The standard subtypes used are listed below, these can be used to filter messages. Vehicles may 
introduce custom notifications and replace standard notifications, see the respective user guide 
section for details.

Subtypes by convention are given in lower case, with dots ''.'' as structural separators.


**Notification channels currently defined:**

  - ``ovmsv2`` -- server v2 connection
  - ``ovmsv3`` -- server v3 connection
  - ``ovmsweb`` -- websocket connections
  - ``pushover`` -- pushover text messaging service

Channels may have multiple active instances ("readers"), for example you can open multiple websocket
connections. Channels may exclude notification types. Currently ``stream`` records are only 
supported on a websocket connection, all other types are supported on all channels.

Use ``notify status`` to see the currently registered channels ("readers"). Example::

  OVMS# notify status
  Notification system has 3 readers registered
    pushover(1): verbosity=1024
    ovmsv2(2): verbosity=1024
    ovmsweb(3): verbosity=65535
  Notify types:
    alert: 0 entries
    data: 0 entries
    error: 0 entries
    info: 0 entries
    stream: 0 entries

The channel's "verbosity" defines the supported maximum length of a textual notification message 
on that channel. Notification senders *should* honor this, but not all may do so. If 
messages exceed this limit, they may be truncated.


---------------------
Sending notifications
---------------------

You can send custom notifications from the shell or command scripts by using the ``notify raise`` 
command. The command can send the output of another command, an error code (implies type ``error``), 
or any text you enter. For example, to send a custom text message, do::

  OVMS# notify raise text info usr.anton.welcome "Hello, wonderful person!"

In this case, the type would be ``info`` and the subtype ``usr.anton.welcome``. The type must match 
one of the defined types, the subtype can be chosen arbitrarily. Please use a unique ``usr.`` 
prefix for custom notifications to avoid collisions.

To send a battery status command result, do::

  OVMS# notify raise command info battery.status "stat"

To send notifications from Duktape scripts, use the API call ``OvmsNotify.Raise()``.


----------------------
Suppress notifications
----------------------

You can filter the channels to be used for notifications by their subtypes. By default, no 
subtypes are filtered on any channel, so all notifications are sent to all clients.

To disable (suppress) notifications, create a config entry based on the respective subtype, 
that lists the channels to include or exclude::

  OVMS# config set notify <subtype> <channels>

<channels> options are:
  a) explicit inclusion: e.g. ``ovmsv2,pushover`` (only enable these)
  b) explicit exclusion: e.g. ``*,-ovmsv3,-ovmsweb`` (only disable these)
  c) ``-`` (dash) to disable all
  d) empty/``*`` to enable all

**Example**: to disable the OTA update notifications on all channels, do::

  OVMS# config set notify ota.update -


----------------------
Standard notifications
----------------------

======= =========================== ================================================================
Type    Subtype                     Purpose / Content
======= =========================== ================================================================
alert   alarm.sounding              Vehicle alarm is sounding
alert   alarm.stopped               Vehicle alarm has stopped
alert   batt.12v.alert              12V Battery critical
alert   batt.12v.recovered          12V Battery restored
alert   batt.bms.alert              Battery pack/cell alert (critical voltage/temperature deviation)
alert   batt.soc.alert              Battery SOC critical
info    charge.done                 ``stat`` on charge finished
info    charge.started              ``stat`` on start of charge
info    charge.stopped              ``stat`` on planned charge stop
alert   charge.stopped              ``stat`` on unplanned charge stop
data    debug.crash                 Transmit crash backtraces (→ ``*-OVM-DebugCrash``)
data    debug.tasks                 Transmit task statistics (→ ``*-OVM-DebugTasks``)
alert   flatbed.moved               Vehicle is being transported while parked - possible theft/flatbed
info    heating.started             ``stat`` on start of heating (battery)
data    log.grid                    Grid (charge/generator) history log (see below) (→ ``*-LOG-Grid``)
data    log.trip                    Trip history log (see below) (→ ``*-LOG-Trip``)
alert   modem.no_pincode            No PIN code for SIM card configured
alert   modem.wrongpincode          Wrong pin code
info    ota.update                  New firmware available/downloaded/installed
info    pushover                    Connection failure / message delivery response
stream  retools.list.update         RE toolkit CAN frame list update
stream  retools.status              RE toolkit general status update
info    valet.disabled              Valet mode disabled
info    valet.enabled               Valet mode enabled
alert   valet.hood                  Vehicle hood opened while in valet mode
alert   valet.trunk                 Vehicle trunk opened while in valet mode
alert   vehicle.idle                Vehicle is idling / stopped turned on
======= =========================== ================================================================


----------------
Grid history log
----------------

The grid history log can be used as a source for long term statistics on your charges and typical 
energy usages and to calculate your vehicle energy costs.

Log entries are created on each change of the charge or generator state (``v.c.state`` / ``v.g.state``).

You need to enable this log explicitly by configuring a storage time via config param ``notify 
log.grid.storetime`` (in days) or via the web configuration page. Set to 0/empty to disable the log. 
Already stored log entries will be kept on the server until expiry or manual deletion.

Note: the stability of the total energy counters included in this log depends on their source 
and persistence on the vehicle and/or module. If they are kept on the module, they may lose their
values on a power outage.

  - Notification subtype: ``log.grid``
  - History record type: ``*-LOG-Grid``
  - Format: CSV
  - Archive time: config ``notify log.grid.storetime`` (days)
  - Fields/columns:

    * pos_gpslock
    * pos_latitude
    * pos_longitude
    * pos_altitude
    * pos_location
    * charge_type
    * charge_state
    * charge_substate
    * charge_mode
    * charge_climit
    * charge_limit_range
    * charge_limit_soc
    * gen_type
    * gen_state
    * gen_substate
    * gen_mode
    * gen_climit
    * gen_limit_range
    * gen_limit_soc
    * charge_time
    * charge_kwh
    * charge_kwh_grid
    * charge_kwh_grid_total
    * gen_time
    * gen_kwh
    * gen_kwh_grid
    * gen_kwh_grid_total
    * bat_soc
    * bat_range_est
    * bat_range_ideal
    * bat_range_full
    * bat_voltage
    * bat_temp
    * charge_temp
    * charge_12v_temp
    * env_temp
    * env_cabintemp
    * bat_soh
    * bat_health
    * bat_cac
    * bat_energy_used_total
    * bat_energy_recd_total
    * bat_coulomb_used_total
    * bat_coulomb_recd_total


----------------
Trip history log
----------------

The trip history log can be used as a source for long term statistics on your trips and typical 
trip power usages, as well as your battery performance in different environmental conditions and 
degradation over time.

Entries are created at the end of a trip (specifically ``v.e.on`` transition to off). Configure a 
minimum trip length for logging by the config variable ``notify log.trip.minlength`` or via the web 
UI.

You need to enable this log explicitly by configuring a storage time via config param ``notify 
log.trip.storetime`` (in days) or via the web configuration page. Set to 0/empty to disable the log. 
Already stored log entries will be kept on the server until expiry or manual deletion.

  - Notification subtype: ``log.trip``
  - History record type: ``*-LOG-Trip``
  - Format: CSV
  - Archive time: config ``notify log.trip.storetime`` (days)
  - Fields/columns:

    * pos_gpslock
    * pos_latitude
    * pos_longitude
    * pos_altitude
    * pos_location
    * pos_odometer
    * pos_trip
    * env_drivetime
    * env_drivemode
    * bat_soc
    * bat_range_est
    * bat_range_ideal
    * bat_range_full
    * bat_energy_used
    * bat_energy_recd
    * bat_coulomb_used
    * bat_coulomb_recd
    * bat_soh
    * bat_health
    * bat_cac
    * bat_energy_used_total
    * bat_energy_recd_total
    * bat_coulomb_used_total
    * bat_coulomb_recd_total
    * env_temp
    * env_cabintemp
    * bat_temp
    * inv_temp
    * mot_temp
    * charge_12v_temp
    * tpms_temp_min
    * tpms_temp_max
    * tpms_pressure_min
    * tpms_pressure_max
    * tpms_health_min
    * tpms_health_max

