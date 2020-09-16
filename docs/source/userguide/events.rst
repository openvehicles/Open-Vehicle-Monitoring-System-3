======
Events
======

Internally, OVMS raises events whenever significant events occur. An event is a lightweight message
of a name plus optionally associated internal binary data. Event names are top-down structured (so
can be filtered by prefix) and sufficient to identify the source and type of the event. Individual
vehicle types may also issue their own events, and custom user events are also possible.

To **bind a script to an event**, save the script in directory ``/store/events/<eventname>`` (hint:
directories can be created using the web UI editor). Whenever events are triggered, all the scripts
in the corresponding ``/store/events/<eventname>`` directory are executed. Event scripts are
executed in alphanumerical order of their names. Good practice is to prefix script names with 2-3
digit numbers in steps of 10 or 100 (i.e. first script named ``50-…``), so new scripts can easily be
integrated at a specific place. If the event script is written in Javascript, be sure to add the
suffix ``.js`` to the name. Other names will be executed using the standard command interpreter.

If you want to **introduce a custom event** (e.g. for a plugin), prefix the event name by
``usr.<pluginname>.`` followed by the event purpose. :doc:`../components/ovms_script/docs/foglight`

Be aware **events are processed in series** from a queue, so depending on the system load and the
list of registered event listeners, there may be some delay from event generation to e.g. a script
execution.

--------
Commands
--------

- ``event list [<key>]`` -- Show registered listeners for all or events matching a key
  (part of the name)
- ``event trace <on|off>`` -- Enable/disable logging of events at the "info" level.
  Without tracing, events are also logged, but at the "debug" level.
  Ticker events are never logged.
- ``event raise [-d<delay_ms>] <event>`` -- Manually raise an event, optionally with a delay.
  You can raise any event you like, but you shouldn't raise system events without
  good knowledge of their effects.


---------------
Standard Events
---------------

=================================== ========= =======
Event                               Data      Purpose
=================================== ========= =======
app.connected                                 One or more remote Apps have connected
app.disconnected                              No remote Apps are currently connected
canopen.node.emcy                   <event>   CANopen node emergency received
canopen.node.state                  <event>   CANopen node state change received
canopen.worker.start                <worker>  CANopen bus worker task started
canopen.worker.stop                 <worker>  CANopen bus worker task stopping
clock.HHMM                                    Per-minute local time, hour HH, minute MM
clock.dayN                                    Per-day local time, day N (0=Sun, 6=Sat)
config.changed                                Configuration has changed
config.mounted                                Configuration is mounted and available
config.unmounted                              Configuration is unmounted and unavailable
egpio.input.<port>.<state>                    EGPIO input port change (port=0…9, state=high/low)
egpio.output.<port>.<state>                   EGPIO output port change (port=0…9, state=high/low)
gps.lock.acquired                             GPS lock has been acquired
gps.lock.lost                                 GPS lock has been lost
housekeeping.init                             Housekeeping has initialised
location.alert.flatbed.moved                  GPS movement of parked vehicle detected
location.enter.<name>               <name>    The specified geolocation has been entered
location.leave.<name>               <name>    The specified geolcation has been left
network.down                                  All networks are down
network.interface.change                      Network interface change detected
network.interface.up                          Network connection is established
network.mgr.init                              Network manager has initialised
network.mgr.stop                              Network managed has been stopped
network.modem.down                            Modem network is down
network.modem.up                              Modem network is up
network.reconfigured                          Networking has been reconfigured
network.up                                    One or more networks are up
network.wifi.down                             WIFI network is down
network.wifi.sta.bad                          WIFI client has bad signal level
network.wifi.sta.good                         WIFI client has good signal level
network.wifi.up                               WIFI network is up
retools.cleared.all                           RE frame log has been cleared
retools.cleared.changed                       RE frame change flags cleared
retools.cleared.discovered                    RE frame discovery flags cleared
retools.mode.analyse                          RE switched to analysis mode
retools.mode.discover                         RE switched to discovery mode
retools.started                               RE (reverse engineering) toolkit started
retools.stopped                               RE toolkit stopped
sd.insert                                     The SD card has just been inserted
sd.mounted                                    The SD card is mounted and ready to use
sd.remove                                     The SD card has just been removed
sd.unmounted                                  The SD card has completed unmounting
sd.unmounting                                 The SD card is currently unmounting
server.v2.authenticating                      V2 server connection is authenticating
server.v2.connected                           V2 server connection established online
server.v2.connecting                          V2 server connection in progress
server.v2.connectwait                         V2 server is pausing before connection
server.v2.disconnected                        V2 server connection has been lost
server.v2.stopped                             V2 server has been stopped
server.v2.waitnetwork                         V2 server connection is waiting for network
server.v2.waitreconnect                       V2 server is pausing before re-connection
server.v3.authenticating                      V3 server connection is authenticating
server.v3.connected                           V3 server connection established online
server.v3.connecting                          V3 server connection in progress
server.v3.connectwait                         V3 server is pausing before connection
server.v3.disconnected                        V3 server connection has been lost
server.v3.stopped                             V3 server has been stopped
server.v3.waitnetwork                         V3 server connection is waiting for network
server.v3.waitreconnect                       V3 server is pausing before re-connection
server.web.socket.closed            <cnt>     Web server lost a websocket client
server.web.socket.opened            <cnt>     Web server has a new websocket client
system.modem.down                             Modem has been disconnected
system.modem.gotgps                           Modem GPS has obtained lock
system.modem.gotip                            Modem received IP address from DATA
system.modem.lostgps                          Modem GPS has lost lock
system.modem.muxstart                         Modem MUX has started
system.modem.netdeepsleep                     Modem is deep sleeping DATA network
system.modem.nethold                          Modem is pausing DATA network
system.modem.netloss                          Modem has lost DATA network
system.modem.netsleep                         Modem is sleeping DATA network
system.modem.netstart                         Modem is starting DATA network
system.modem.netwait                          Modem is pausing before starting DATA
system.modem.poweredon                        Modem is powered on
system.modem.poweringon                       Modem is powering on
system.modem.received.ussd          <ussd>    A USSD message has been received
system.modem.stop                             Modem has been shut down
system.shutdown                               System has been shut down
system.shuttingdown                           System is shutting down
system.start                                  System is starting
system.vfs.file.changed             <path>    VFS file updated (note: only sent on some file changes)
system.wifi.ap.sta.connected                  WiFi access point got a new client connection
system.wifi.ap.sta.disconnected               WiFi access point lost a client connection
system.wifi.ap.sta.ipassigned                 WiFi access point assigned an IP address to a client
system.wifi.ap.start                          WiFi access point mode starting
system.wifi.ap.stop                           WiFi access point mode stopping
system.wifi.down                              WiFi is shutting down
system.wifi.scan.done                         WiFi scan has been finished
system.wifi.sta.connected                     WiFi client is connected to a station
system.wifi.sta.disconnected                  WiFi client has disconnected from a station
system.wifi.sta.gotip                         WiFi client got an IP address
system.wifi.sta.lostip                        WiFi client lost it's IP address
system.wifi.sta.start                         WiFi client mode starting
ticker.1                                      One second has passed since last ticker
ticker.10                                     Ten seconds have passed
ticker.300                                    Five minutes have passed
ticker.3600                                   One hour has passed
ticker.60                                     One minute has passed
ticker.600                                    Ten minutes have passed
vehicle.alarm.off                             Vehicle alarm has been disarmed
vehicle.alarm.on                              Vehicle alarm has been armed
vehicle.alert.12v.off                         12V system voltage has recovered
vehicle.alert.12v.on                          12V system voltage is below alert threshold
vehicle.alert.bms                             BMS cell/pack volts/temps exceeded thresholds
vehicle.asleep                                Vehicle systems are asleep
vehicle.awake                                 Vehicle systems are awake
vehicle.charge.12v.start                      Vehicle 12V battery is charging
vehicle.charge.12v.stop                       Vehicle 12V battery has stopped charging
vehicle.charge.finished                       Vehicle charge has completed normally
vehicle.charge.mode                 <mode>    Vehicle charge mode has been set
vehicle.charge.pilot.off                      Vehicle charge pilot signal is off
vehicle.charge.pilot.on                       Vehicle charge pilot signal is on
vehicle.charge.prepare                        Vehicle is preparing to charge
vehicle.charge.start                          Vehicle has started to charge
vehicle.charge.state                <state>   Vehicle charge state has changed
vehicle.charge.stop                           Vehicle has stopped charging
vehicle.headlights.off                        Vehicle headlights are off
vehicle.headlights.on                         Vehicle headlights are on
vehicle.locked                                Vehicle has been locked
vehicle.off                                   Vehicle has been switched off
vehicle.on                                    Vehicle has been switched on
vehicle.require.gps                           A vehicle has indicated it requires GPS
vehicle.require.gpstime                       A vehicle has indicated it requires GPS time
vehicle.type.cleared                          Vehicle module has been unloaded
vehicle.type.set                    <type>    Vehicle module has been loaded
vehicle.unlocked                              Vehicle has been unlocked
vehicle.valet.off                             Vehicle valet mode deactivated
vehicle.valet.on                              Vehicle valet mode activated
=================================== ========= =======
