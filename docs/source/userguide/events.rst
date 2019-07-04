======
Events
======

Internally, OVMS raises events whenever significant events occur. An event is simply a name, along with associated data. Individual vehicle types may also issue their own events, and custom user events are also possible. Here is the list of standard system events in the OVMS v3 firmware:

=========================== ======== =======
Event                       Data     Purpose
=========================== ======== =======
sd.mounted                           The SD card is mounted and ready to use
sd.unmounting                        The SD card is currently unmounting
sd.unmounted                         The SD card has completed unmounting
sd.insert                            The SD card has just been inserted
sd.remove                            The SD card has just been removed
location.enter.<name>       <name>   The specified geolocation has been entered
location.leave.<name>       <name>   The specified geolcation has been left
gps.lock.acquired                    GPS lock has been acquired
gps.lock.lost                        GPS lock has been lost
system.wifi.down                     WiFi connection has been lost
app.connected                        One or more remote Apps have connected
app.disconnected                     No remote Apps are currently connected
server.v2.waitnetwork                V2 server connection is waiting for network
server.v2.connectwait                V2 server is pausing before connection
server.v2.connecting                 V2 server connection in progress
server.v2.authenticating             V2 server connection is authenticating
server.v2.connected                  V2 server connection established online
server.v2.disconnected               V2 server connection has been lost
server.v2.waitreconnect              V2 server is pausing before re-connection
server.v2.stopped                    V2 server has been stopped
server.v3.waitnetwork                V3 server connection is waiting for network
server.v3.connectwait                V3 server is pausing before connection
server.v3.connecting                 V3 server connection in progress
server.v3.authenticating             V3 server connection is authenticating
server.v3.connected                  V3 server connection established online
server.v3.disconnected               V3 server connection has been lost
server.v3.waitreconnect              V3 server is pausing before re-connection
server.v3.stopped                    V3 server has been stopped
vehicle.require.gps                  A vehicle has indicated it requires GPS
vehicle.require.gpstime              A vehicle has indicated it requires GPS time
vehicle.type.set            <type>   Vehicle module has been loaded
vehicle.type.cleared                 Vehicle module has been unloaded
vehicle.alert.12v.on                 12V system voltage is below alert threshold
vehicle.alert.12v.off                12V system voltage has recovered
vehicle.alert.bms                    VMS volts/temps exceeded thresholds
vehicle.on                           Vehicle has been switched on
vehicle.off                          Vehicle has been switched off
vehicle.awake                        Vehicle systems are awake
vehicle.asleep                       Vehicle systems are asleep
vehicle.charge.start                 Vehicle has started to charge
vehicle.charge.stop                  Vehicle has stopped charging
vehicle.charge.prepare               Vehicle is preparing to charge
vehicle.charge.finished              Vehicle charge has completed normally
vehicle.charge.pilot.on              Vehicle charge pilot signal is on
vehicle.charge.pilot.off             Vehicle charge pilot signal is off
vehicle.charge.12v.start             Vehicle 12V battery is charging
vehicle.charge.12v.stop              Vehicle 12V battery has stopped charging
vehicle.locked                       Vehicle has been locked
vehicle.unlocked                     Vehicle has been unlocked
vehicle.valet.on                     Vehicle valet mode activated
vehicle.valet.off                    Vehicle valet mode deactivated
vehicle.headlights.on                Vehicle headlights are on
vehicle.headlights.off               Vehicle headlights are off
vehicle.alarm.on                     Vehicle alarm has been armed
vehicle.alarm.off                    Vehicle alarm has been disarmed
vehicle.charge.mode         <mode>   Vehicle charge mode has been set
vehicle.charge.state        <state>  Vehicle charge state has changed
system.modem.gotgps                  Modem GPS has obtained lock
system.modem.lostgps                 Modem GPS has lost lock
system.modem.poweringon              Modem is powering on
system.modem.poweredon               Modem is powered on
system.modem.muxstart                Modem MUX has started
system.modem.netwait                 Modem is pausing before starting DATA
system.modem.netstart                Modem is starting DATA network
system.modem.netloss                 Modem has lost DATA network
system.modem.nethold                 Modem is pausing DATA network
system.modem.netsleep                Modem is sleeping DATA network
system.modem.netdeepsleep            Modem is deep sleeping DATA network
system.modem.stop                    Modem has been shut down
system.modem.received.ussd  <ussd>   A USSD message has been received
network.interface.up                 Network connection is established
system.modem.gotip                   Modem received IP address from DATA
system.modem.down                    Modem has been disconnected
system.shuttingdown                  System is shutting down
system.shutdown                      System has been shut down
network.wifi.up                      WIFI network is up
network.reconfigured                 Networking has been reconfigured
network.up                           One or more networks are up
network.interface.change             Network interface change detected
network.wifi.down                    WIFI network is down
network.down                         All networks are down
network.modem.up                     Modem network is up
network.modem.down                   Modem network is down
network.mgr.init                     Network manager has initialised
network.mgr.stop                     Network managed has been stopped
config.mounted                       Configuration is mounted and available
config.unmounted                     Configuration is unmounted and unavailable
config.changed                       Configuration has changed
housekeeping.init                    Housekeeping has initialised
system.start                         System is starting
ticker.1                             One second has passed since last ticker
ticker.10                            Ten seconds have passed
ticker.60                            One minute has passed
ticker.300                           Five minutes have passed
ticker.600                           Ten minutes have passed
ticker.3600                          One hour has passed
=========================== ======== =======
