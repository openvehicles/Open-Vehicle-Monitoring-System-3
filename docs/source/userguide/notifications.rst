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

