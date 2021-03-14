.. highlight:: none

===============
WiFi Networking
===============

The OVMS WiFi system is based on the ESP32 integrated WiFi transceiver. It uses and provides WiFi 
protocols 802.11 b/g/n and WPA2 authentication. The current hardware can only use 2.4 GHz frequency 
bands.

The WiFi antenna is built into the ESP32 module (PCB antenna). It's possible to replace that by an 
external antenna, but you'll need electronics knowledge and SMD soldering skills & equipment.


---------------------------
Client & Access Point Modes
---------------------------

The OVMS WiFi network can be configured as a client (connecting to existing WiFi networks) and/or 
access point (providing it's own private WiFi network). Both modes can be run simultaneously, which 
is the recommended default. That way, you can always connect to your module via WiFi.

::

  OVMS# wifi status
  Power: on
  Mode: Access-Point + Client mode

  STA SSID: WLAN-214677 (-78.7 dBm) [scanning]
    MAC: 30:ae:a4:5f:e7:ec
    IP: 192.168.2.102/255.255.255.0
    GW: 192.168.2.1
    AP: 7c:ff:4d:15:2f:86

  AP SSID: DEX106E
    MAC: 30:ae:a4:5f:e7:ed
    IP: 192.168.4.1
    AP Stations: 0


In **client mode**, the module can connect to a fixed network, or automatically scan all channels for
known networks to connect to ("scanning mode"). Scanning mode is configured by enabling client mode
without a specific client SSID.

The module will normally receive an IP address, gateway and DNS from the WiFi access point by DHCP. 
See below on how to connect with a manual static IP setup.

In **access point mode**, the module provides access for other WiFi devices on a private network
with the IP subnet ``192.168.4.0/24``. The module's IP address on this network is ``192.168.4.1``.
The module does not provide a DNS or routing to a public WiFi or GSM network on this subnet.

You can define multiple AP networks, but only one can be active at a time.

Use the ``wifi mode`` command to manually set the mode, use the auto start configuration to 
configure your default mode and networks.

.. note:: **The module cannot act as a mobile hotspot**, i.e. provide access to the internet via it's
  modem connection. That's in part due to security considerations, and in part due to hardware
  limitations. If you need this, consider installing a dedicated mobile hotspot and using that one 
  via WiFi instead of the modem for the module's internet access (you won't need the modem in this 
  setup).


---------------------
Scanning for Networks
---------------------

To manually scan your environment for available WiFi networks, do a ``wifi scan``::

  OVMS# wifi scan
  Scanning for WIFI Access Points...

  AP SSID                          MAC ADDRESS       CHAN RSSI AUTHENTICATION        
  ================================ ================= ==== ==== ==============        
  WLAN-214677                      7c:ff:4d:15:2f:86    1  -83 WPA2_PSK
  Telekom_FON                      78:dd:12:09:dc:be   11  -84 OPEN
  WLAN-184248                      78:dd:12:09:dc:bc   11  -85 WPA2_PSK
  ===========================================================================
  Scan complete: 3 access point(s) found


This can also be done in the web UI's WiFi client network configuration page by clicking on the 
:guilabel:`Select` button.

After the module lost connection to a network, a scan is performed automatically every 10 seconds 
until a new connection can be established. If you'd like to see the background scans and results 
in the log, enable log level ``verbose`` for component ``esp32wifi``.


^^^^^^^^^^^^^^^^^^^^
Scan Troubleshooting
^^^^^^^^^^^^^^^^^^^^

If the module doesn't find your access point, or can only see it occasionally 
in the scan results, you may need to raise your scan times. This has been observed on some older 
Android hotspots.

By default, the module will scan each channel for 120 milliseconds. To raise that, set your scan
time in milliseconds using the config command like this, e.g. to 200 milliseconds::

  OVMS# config set network wifi.scan.tmin 200

200 milliseconds have been reported to solve the Android hotspot issue. In addition to the ``tmin`` 
configuration you may need to allow more time looking for further access points after finding the 
first one. Do so by setting ``tmax``, e.g.::

  OVMS# config set network wifi.scan.tmax 300

Note that every increase will increase the time a full scan takes, so don't set too high values. 
Too high values may result in connection drops on the AP network of the module.


-------------------
WiFi Signal Quality
-------------------

The module monitors the WiFi client signal quality and drops a WiFi connection (switches to modem
if available) if it becomes too bad. The WiFi connection will be kept active and monitored, and as 
the signal recovers, the module will automatically reconnect to the AP.

The default threshold is to stop using the connection if it drops below -89 dBm. A connection is 
assumed to be usable if the signal is above -87 dBm.

Depending on your WiFi environment, the WiFi connection may still be usable at lower signal levels.

To tweak the thresholds, use the web UI WiFi configuration or change the following configuration
variables::

  OVMS# config set network wifi.sq.good -87.0
  OVMS# config set network wifi.sq.bad -89.0


-----------------------
WiFi Mesh Configuration
-----------------------

In normal operation, the module will try to stick to an established connection as long as 
possible. If signal quality drops, it will switch to the modem connection, but monitor the WiFi
signal and reassociate to the current AP if possible.

If using a mesh network, you may want to force scanning for a better mesh AP as soon as the 
signal drops below the "bad" threshold. To do so, set the network configuration ``wifi.bad.reconnect``
to true, either using the web UI or by doing::

  OVMS# config set network wifi.bad.reconnect yes

With this, the module will perform a full WiFi reconnect cycle as soon as the signal becomes bad.


------------------------------
Static IP / SSID Configuration
------------------------------

To connect with a **static client address** instead of using DHCP, use the ``wifi ip static`` command::

  OVMS# wifi ip static [<ip> <subnet> <gateway>]

The gateway will also be used as the DNS.

To configure persistent static details for a known SSID, set these using the following 
configuration syntax::

  OVMS# config set wifi.ssid "<ssid>.ovms.staticip" "<ip>,<subnet>,<gateway>"


You can also **force connection to a specific AP** by it's MAC address, "BSSID" in WiFi terms. To do
so, you need to supply the MAC address as a second argument to the ``wifi mode client`` or as the
third argument to the ``wifi mode apclient`` command::

  OVMS# wifi mode client <ssid> <bssid>

  OVMS# wifi mode apclient <apssid> <ssid> <bssid>

This currently needs manual activation by command. Hint: use a script, for example bound to a 
location.

