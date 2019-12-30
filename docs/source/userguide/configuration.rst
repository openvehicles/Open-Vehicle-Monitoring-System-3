=============
Configuration
=============

OVMS stores all it’s configuration in a standardised protected configuration area accessed by the ‘config’ command. The configurations are organised as follows::

  <parameter> <instance> = <value>

For example:

========= ======== =====
Parameter Instance Value
========= ======== =====
vehicle   id       OVMSBOX
wifi.ssid MYSSID   MyPassword
auto      init     yes
========= ======== =====

Each parameter can be defined (by the component that owns it) as having readable and/or writeable attributes, and these act as access control for the parameters.

* A ‘writeable’ parameter allows values to be created, deleted and modified.
* A ‘readable’ parameter allows values to be seen. Instance names can be seen on non-readable parameters (it is just the values themselves that are protected).

For example, the ‘vehicle’ parameter is readable and writeable::

  OVMS# config list vehicle
  vehicle (readable writeable)
    id: MYCAR
    timezone: HKT-8

But the ‘wifi.ssid’ parameter is only writeable::

  OVMS# config list wifi.ssid
  wifi.ssid (protected writeable)
    MYSSID
    MyOtherSSID
    MyNeighbour

The ‘config’ command is used to manipulate these configurations, as is fairly self-explanatory::

  OVMS# config ?
  list                 Show configuration parameters/instances
  rm                   Remove parameter:instance
  set                  Set parameter:instance=value


You can add new instances to parameters simply by setting them.

Beginning with firmware release 3.2.009, a dedicated configuration section ``usr`` is provided
for plugins. Take care to prefix all instances introduced by a unique plugin name, so your plugin
can nicely coexist with others.
