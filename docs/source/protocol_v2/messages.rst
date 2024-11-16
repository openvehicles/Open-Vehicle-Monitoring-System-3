========
Messages
========

----------------------
Car <-> Server <-> App
----------------------

After discarding CR+LF line termination, and base64 decoding, the following protocol messages are defined.

  <message> ::= <magic> <version> <space> <protmsg>

  <magic> ::= MP-

  <version> ::= 1 byte version number - this protocol is 0x30
  
  <space> ::= ' ' (ascii 0x20)

  <protmsg> ::= <servertocar> | <cartoserver> | <servertoapp> | <apptoserver>

  <servertocar> ::- "S" <payload>

  <cartoserver> ::= "C" <payload>

  <servertoapp> ::= "s" <payload>

  <apptoserver> ::= "c" <payload>

  <payload> ::= <code> <data>

  <code> ::= 1 byte instruction code

  <data> ::= N bytes data (dependent on instruction code)

---------------------
Ping message 0x41 "A"
---------------------

This message may be sent by any party, to test the link. The expected response is a 0x61 ping acknowledgement. There is no expected payload to this message, an any given can be discarded.

-------------------------------------
Ping Acknowledgement message 0x61 "a"
-------------------------------------

This message is sent in response to a 0x41 ping message. There is no expected payload to this message, an any given can be discarded.

------------------------
Command message 0x43 "C"
------------------------

This message is sent <apptoserver> then <servertocar> and carries a command to be executed on the car. The message would normally be paranoid-encrypted.

<data> is a comma-separated list of:

* command (a command code 0..65535)
* parameters (dependent on the command code)

For further information on command codes and parameters, see the command section below.

-------------------------
Command response 0x63 "c"
-------------------------

This message is sent <cartoserver> then <servertoapp> and carries the response to a command executed on the car. The message would normally be paranoid-encrypted.

<data> is a comma-separated list of:

* command (a command code 0..65535)
* result (0=ok, 1=failed, 2=unsupported, 3=unimplemented)
* parameters (dependent on the command code and result)

For result=0, the parameters depend on the command being responded to (see the command section below for further information).

For result=1, the parameter is a textual string describing the fault.

For result=2 or 3, the parameter is not used.

--------------------------------
Car Environment message 0x44 "D"
--------------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the environment settings of the vehicle.

<data> is comma-separated list of:

Door state #1

* bit0 = Left Door (open=1/closed=0)
* bit1 = Right Door (open=1/closed=0)
* bit2 = Charge port (open=1/closed=0)
* bit3 = Pilot present (true=1/false=0) (always 1 on my 2.5)
* bit4 = Charging (true=1/false=0)
* bit5 = always 1
* bit6 = Hand brake applied (true=1/false=0)
* bit7 = Car ON ("ignition") (true=1/false=0)

Door state #2

* bit3 = Car Locked (locked=1/unlocked=0)
* bit4 = Valet Mode (active=1/inactive=0)
* bit6 = Bonnet (open=1/closed=0)
* bit7 = Trunk (open=1/closed=0)

Lock/Unlock state

* 4 = car is locked
* 5 = car is unlocked

Temperature of the PEM (celcius)

Temperature of the Motor (celcius)

Temperature of the Battery (celcius)

Car trip meter (in 1/10th of a distance unit)

Car odometer (in 1/10th of a distance unit)

Car speed (in distance units per hour)

Car parking timer (0 for not parked, or number of seconds car parked for)

Ambient Temperature (in Celcius)

Door state #3

* bit0 = Car awake (turned on=1 / off=0)
* bit1 = Cooling pump (on=1/off=0)
* bit6 = 1=Logged into motor controller
* bit7 = 1=Motor controller in configuration mode

Stale PEM,Motor,Battery temps indicator (-1=none, 0=stale, >0 ok)

Stale ambient temp indicator (-1=none, 0=stale, >0 ok)

Vehicle 12V line voltage

Door State #4

* bit2 = alarm sounds (on=1/off=0)

Reference voltage for 12v power

Door State #5

* bit0 = Rear left door (open=1/closed=0)
* bit1 = Rear right door (open=1/closed=0)
* bit2 = Frunk (open=1/closed=0)
* bit4 = 12V battery charging
* bit5 = Auxiliary 12V systems online
* bit7 = HVAC running

Temperature of the Charger (celsius)

Vehicle 12V current (i.e. DC converter output)

Cabin temperature (celsius)

----------------------------------------
Paranoid-mode encrypted message 0x45 "E" 
----------------------------------------
This message is sent for any of the four message <protmsg> types, and represents an encrypted transmission that the server should just relay (or is relaying) without being able to interpret it. The encryption is based on a shared secret, between the car and the apps, to which the server is not privy.

<data> is:

* <paranoidtoken> | <paranoidcode>
* <paranoidtoken> ::= "T" <ptoken>
* <paranoidcode> ::= "M" <code> <data>

In the case of <paranoidtoken>, the <ptoken> is a random token that represent the encryption key. It can only be sent <cartoserver> or <servertoapp>. Upon receiving this token, the server discards all previously stored paranoid messages, sends it on to all connected apps, and then stores the token. Every time an app connects, the server also sends this token to the app.

In the case of <paranoidcode>, the <code> is a sub-message code, and can be any of the codes listed in this document (except for "A", "a" and "E"). The <data> is the corresponding encrypted payload message. The encryption is performed on the <data> by:

* Create a new hmac-md5 based on the <ptoken>, with the shared secret.
* Use the new hmac digest as the key for symmetric rc4 encryption.
* Generate, and discard, 1024 bytes of cipher text.

and the data is base64 encoded. Upon receiving a paranoid message from the car, the server forwards it on the all connected apps, and then stores the message. Every time an app connects, the server sends all such stored messages. Upon receiving a paranoid message from an app, if the car is connected, the server merely forwards it on to the car, otherwise discarding it.

-----------------------------
Car firmware message 0x46 "F"
-----------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the firmware versions of the vehicle.

<data> is comma-separated list of:

* OVMS firmware version
* VIN (vehicle identification number)
* Current network signal quality (Wifi / GSM, in SQ units)
* Write-enabled firmware (0=read-only, 1=write-enabled)
* Vehicle type code (e.g. ``TR`` = Tesla Roadster, see command output ``vehicle list``)
* Current network name (Wifi SSID / GSM provider)
* Distance to next scheduled maintenance/service [km]
* Time to next scheduled maintenance/service [seconds]
* OVMS hardware version
* Cellular connection mode and status [LTE,Online]

--------------------------------
Server firmware message 0x66 "f"
--------------------------------

This message is sent <servertocar> "S", or <servertoapp> "s", and transmits the firmware versions of the server.

<data> is comma-separated list of:

* Server firmware version

---------------------------------------
Car group subscription message 0x47 "G"
---------------------------------------

This message is sent <apptoserver> "A", and requests subscription to the specified group.

<data> is comma-separated list of:

* Group name

---------------------------------
Car group update message 0x67 "g"
---------------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits a group location message for the vehicle.

<data> is comma-separated list of:

* Vehicle ID (only <servertoapp>, not sent <cartoserver>)
* Group name
* Car SOC
* Car Speed
* Car direction
* Car altitude
* Car GPS lock (0=nogps, 1=goodgps)
* Stale GPS indicator (-1=none, 0=stale, >0 ok)
* Car latitude
* Car longitude

---------------------------------------
Historical Data update message 0x48 "H"
---------------------------------------

This message is sent <cartoserver> "C, and transmits a historical data message for storage on the server.

<data> is comma-separated list of:

* type (unique storage class identification type)
* recordnumber (integer record number)
* lifetime (in seconds)
* data (a blob of data to be dealt with as the application requires)

The lifetime is specified in seconds, and indicates to the server the minimum time the vehicle expects the
server to retain the historical data for. Consideration should be made as to server storage and bandwidth requirements.

The type is composed of <vehicletype> - <class> - <property>

<Vehicletype> is the usual vehicle type, or '*' to indicate generic storage suitable for all vehicles.

<Class> is one of:

* PWR (power)
* ENG (engine)
* TRX (transmission)
* CHS (chassis)
* BDY (body)
* ELC (electrics)
* SAF (safety)
* SEC (security)
* CMF (comfort)
* ENT (entertainment)
* COM (communications)
* X** (unclassified and experimental, with ** replaced with 2 digits code)

<Property> is a property code, which the vehicle decides.

The server will timestamp the incoming historical records, and will set an expiry date of timestamp + <lifetime> seconds. The server will endeavor to retain the records for that time period, but may expend data earlier if necessary.

-------------------------------------------
Historical Data update+ack message 0x68 "h"
-------------------------------------------

This message is sent <cartoserver> "C", or <severtocar> "c", and transmits/acknowledges historical data message for storage on the server.

For <cartoserver>, the <data> is comma-separated list of:

* ackcode (an acknowledgement code)
* timediff (in seconds)
* type (unique storage class identification type)
* recordnumber (integer record number)
* lifetime (in seconds)
* data (a blob of data to be dealt with as the application requires)

The ackcode is a numeric acknowledgement code - if the server successfully receives the message, it will reply with "h" and this ackcode to acknowledge reception.

The timediff is the time difference, in seconds, to use when storing the record (e.g.; -3600 would indicate the record data is from one hour ago).

The lifetime is specified in seconds, and indicates to the server the minimum time the vehicle expects the server to retain
the historical data for. Consideration should be made as to server storage and bandwidth requirements.

For <servertocar>, the <data> is:

* ackcode (an acknowledgement code)

The <cartoserver> message sends the data to the server. The <servertocar> message acknowledges the data.

----------------------------------
Push notification message 0x50 "P"
----------------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s". When used by the car, it requests the server to send a textual push notification alert message to all apps registered for this car. The <data> is 1 byte alert type followed by N bytes of textual message. The server will use this message to send the notification to any connected apps, and can also send via external mobile frameworks for unconnected apps.

---------------------------------------
Push notification subscription 0x70 "p"
---------------------------------------

This message is sent <apptoserver> A". It is used by app to register for push notifications, and is normally at the start of a connection. The <data> is made up of:

<appid>,<pushtype>,<pushkeytype>,<vehicleid>,<netpass>,<pushkeyvalue>

The server will verify the credentials for each vehicle, and store the required notification information.

Note: As of June 2020, only one vehicleid can be subscribed at a time. If multiple vehicles are
required, then they should each be subscribed in individual messages.

----------------------------------------
Server -> Server Record message 0x52 "R"
----------------------------------------

This message is sent <servertoserver> "S", and transmits an update to synchronized database table records.

Sub-type RV (Vehicle record): <data> is comma-separated list of:

* Vehicleid
* Owner
* Carpass
* v_server
* deleted
* changed

Sub-type RO (Owner record): <data> is comma-separated list of:

* Ownerid
* OwnerName
* OwnerMail
* PasswordHash
* OwnerStatus
* deleted
* changed

-----------------------------------------------------
Server -> Server Message Replication message 0x72 "r"
-----------------------------------------------------

This message is sent <servertoserver> "S", and replicates a message for a particular car.

<data> is comma-separated list of:

* vehicleid
* message code
* message data

--------------------------
Car state message 0x53 "S"
--------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the last known status of the vehicle.

<data> is comma-separated list of:

* SOC
* Units ("M" for miles, "K" for kilometers)
* Line voltage
* Charge current (amps)
* Charge state (charging, topoff, done, prepare, heating, stopped)
* Charge mode (standard, storage, range, performance)
* Ideal range
* Estimated range
* Charge limit (amps)
* Charge duration (minutes)
* Charger B4 byte (tba)
* Charge energy consumed (1/10 kWh)
* Charge sub-state
* Charge state (as a number)
* Charge mode (as a number)
* Charge Timer mode (0=onplugin, 1=timer)
* Charge Timer start time
* Charge timer stale (-1=none, 0=stale, >0 ok)
* Vehicle CAC100 value (calculated amp hour capacity, in Ah)
* ACC: Mins remaining until car will be full
* ACC: Mins remaining until car reaches charge limit
* ACC: Configured range limit
* ACC: Configured SOC limit
* Cooldown: Car is cooling down (0=no, 1=yes)
* Cooldown: Lower limit for battery temperature
* Cooldown: Time limit (minutes) for cooldown
* ACC: charge time estimation for current charger capabilities (min.)
* Charge ETR for range limit (min.)
* Charge ETR for SOC limit (min.)
* Max ideal range
* Charge/plug type ID according to OpenChargeMaps.org connectiontypes (see http://api.openchargemap.io/v2/referencedata/)
* Charge power output (kW)
* Battery voltage (V)
* Battery SOH (state of health) (%)
* Charge power input (kW)
* Charger efficiency (%)
* Battery current (A)
* Battery ideal range gain/loss speed (mph/kph, gain=positive)
* Energy sum for running charge (kWh)
* Energy drawn from grid during running session (kWh)
* Main battery usable capacity (kWh)
* Date & time of last charge end (seconds)

--------------------------------
Car update time message 0x53 "T"
--------------------------------

This message is sent <servertoapp> "s", and transmits the last known update time of the vehicle.

<data> is the number of seconds since the car last sent an update message

-----------------------------
Car location message 0x4C "L"
-----------------------------

This message is sent <cartoserver> "C" and transmits the last known location of the vehicle.

<data> is comma-separated list of:

* Latitude
* Longitude
* Car direction
* Car altitude
* Car GPS lock (0=nogps, 1=goodgps)
* Stale GPS indicator (-1=none, 0=stale, >0 ok)
* Car speed (in distance units per hour)
* Car trip meter (in 1/10th of a distance unit)
* Drive mode (car specific encoding of current drive mode)
* Battery power level (in kW, negative = charging)
* Energy used (in Wh)
* Energy recovered (in Wh)
* Inverter motor power (kW) (positive = output)
* Inverter efficiency (%)
* GPS mode indicator (see below)
* GPS satellite count
* GPS HDOP (see below)
* GPS speed (in distance units per hour)
* GPS signal quality (%)

**GPS mode indicator**: this shows the NMEA receiver mode. If using the SIM5360 modem for GPS, this 
is a two character string. The first character represents the GPS receiver mode, the second the GLONASS 
receiver mode. Each mode character may be one of:

* `N` = No fix. Satellite system not used in position fix, or fix not valid
* `A` = Autonomous. Satellite system used in non-differential mode in position fix
* `D` = Differential (including all OmniSTAR services). Satellite system used in differential mode in position fix
* `P` = Precise. Satellite system used in precision mode. Precision mode is defined as: no deliberate degradation (such as Selective Availability) and higher resolution code (P-code) is used to compute position fix
* `R` = Real Time Kinematic. Satellite system used in RTK mode with fixed integers
* `F` = Float RTK. Satellite system used in real time kinematic mode with floating integers
* `E` = Estimated (dead reckoning) Mode
* `M` = Manual Input Mode
* `S` = Simulator Mode

**GPS HDOP**: HDOP = horizontal dilution of precision. This is a measure for the currently achievable 
precision of the horizontal coordinates (latitude & longitude), which depends on the momentary relative 
satellite positions and visibility to the device.

The lower the value, the higher the precision. Values up to 2 mean high precision, up to 5 is good. 
If the value is higher than 20, coordinates may be off by 300 meters from the actual position.

See https://en.wikipedia.org/wiki/Dilution_of_precision_(navigation) for further details.

**GPS signal quality**: this is a normalized quality / reliability level (0…100%) derived from lock
status, satellite count and HDOP (if available). Levels below 30% mean the position should not be
depended on, from 50% up mean good, from 80% up excellent.


---------------------------------
Car Capabilities message 0x56 "V"
---------------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the vehicle capabilities. It was introduced with v2 of the protocol.

<data is comma-separated list of vehicle capabilities of the form:

* C<cmd> indicates vehicle support command <cmd>
* C<cmdL>-<cmdH> indicates vehicle will support all commands in the specified range

----------------------------------------
Car TPMS message 0x57 "W" (old/obsolete)
----------------------------------------

.. note:: Message "W" has been replaced by "Y" (see below) for OVMS V3.
  The V3 module will still send "W" messages along with "Y" for old clients for some time.
  Clients shall adapt to using "Y" if available ASAP, "W" messages will be removed from V3
  in the near future.

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the last known TPMS values of the vehicle.

<data> is comma-separated list of:

* front-right wheel pressure (psi)
* front-right wheel temperature (celcius)
* rear-right wheel pressure (psi)
* rear-right wheel temperature (celcius)
* front-left wheel pressure (psi)
* front-left wheel temperature (celcius)
* rear-left wheel pressure (psi)
* rear-left wheel temperature (celcius)
* Stale TPMS indicator (-1=none, 0=stale, >0 ok)


---------------------------------
Car export power message 0x58 "X"
---------------------------------

.. note:: The message code has previously been "G". It's been changed to "X" for firmware release
  3.3.005 to avoid conflict with the <apptoserver> group subscription message code.

This message is sent <cartoserver> "C", or <servertoapp> "s" and transmits "v.g" metrics from the vehicle.

<data> is comma-separated list of:

* v.g.generating (1 = currently delivering power)
* v.g.pilot (1 = pilot present)
* v.g.voltage (in V)
* v.g.current (in A)
* v.g.power (in kW)
* v.g.efficiency (in %)
* v.g.type (eg "chademo")
* v.g.state (eg "exporting")
* v.g.substate (eg "onrequest")
* v.g.mode (eg "standard")
* v.g.climit (in A)
* v.g.limit.range (in km)
* v.g.limit.soc (in %)
* v.g.kwh (in kWh)
* v.g.kwh.grid (in kWh)
* v.g.kwh.grid.total (in kWh)
* v.g.time (in s)
* v.g.timermode (1 = generator timer enabled)
* v.g.timerstart 
* v.g.duration.empty (in min)
* v.g.duration.range (in min)
* v.g.duration.soc (in min)
* v.g.temp (in deg C)
* v.g.timestamp (in seconds)

Refer https://docs.openvehicles.com/en/latest/userguide/metrics.html


-------------------------
Car TPMS message 0x59 "Y"
-------------------------

This message is sent <cartoserver> "C", or <servertoapp> "s", and transmits the last known TPMS values of the vehicle.

<data> is comma-separated list of:

* number of defined wheel names
* list of defined wheel names
* number of defined pressures
* list of defined pressures (kPa)
* pressures validity indicator (-1=undefined, 0=stale, 1=valid)
* number of defined temperatures
* list of defined temperatures (Celcius)
* temperatures validity indicator (-1=undefined, 0=stale, 1=valid)
* number of defined health states
* list of defined health states (Percent)
* health states validity indicator (-1=undefined, 0=stale, 1=valid)
* number of defined alert levels
* list of defined alert levels (0=none, 1=warning, 2=alert)
* alert levels validity indicator (-1=undefined, 0=stale, 1=valid)

.. note:: Pressures are transported in kPa now instead of the former PSI.
  To convert to PSI, multiply by 0.14503773773020923.

--------------------------------
Peer connection message 0x5A "Z"
--------------------------------

This message is sent <servertocar> or <servertoapp> to indicate the connection status of the peer (car for <servertoapp>, interactive apps for <servertocar>). It indicates how many peers are currently connected.

It is suggested that the car should use this to immediately report on, and to increase the report frequency of, status - in the case that one or more interactive Apps are connected and watching the car.

Batch client connections do not trigger any peer count change for the car, but they still receive the car peer status from the server.

<data> is:

* Number of peers connected, expressed as a decimal string

