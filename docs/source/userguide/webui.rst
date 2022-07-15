===============
The OVMS Web UI
===============

The OVMS has a builtin webserver for HTTP and HTTPS. The web UI framework provides
the local graphical user interface for configuration and using the module. It has
some features the mobile Apps cannot currently provide, and it can be extended
easily by custom plugins.

The webserver also implements a WebSocket channel for live (push) transmission of
metrics updates, events, logs etc. from the module to the browser. This channel
can be used to subscribe to live updates not only by a browser but also by e.g.
a Wifi button or console.

The web framework builtin functions can be used as REST API endpoints as well, for
example for command execution or file download/upload.

Regarding file access, the webserver also provides optional direct access to the
files stored on the SD card. A directory browser is also provided, so you can
easily check and download e.g. log files or data written by a plugin.

The web UI framework is meant to be used as a single page UI, but you can open
multiple instances (separate browser sessions) from any client. All URLs are
bookmarkable deep links, so can be accessed directly. The web framework takes
care of user authentication and will automatically ask for a login as necessary.

The web UI is built as a Web App, so all functions can be added as browser shortcuts
to a mobile phone's or tablet's App launcher. Hint: using the shortcut, most
mobile operating systems will remove the browser navigation bar, freeing up
precious screen space.

The web UI adapts to almost any screen resolution and orientation. It can be
switched into full screen mode and provides an alternative night mode dark
style for night time driving (or cooler looks).

Depending on the vehicle type and the plugins installed, the web UI will provide
different menus and functions. Plugins can provide completely new pages or
add/modify existing ones, for example to extend the dashboard by some
custom buttons or displays.


--------------------
Accessing the Web UI
--------------------

The webserver can be accessed on all active network interfaces. Check
``network status`` for the list of interfaces and their IP addresses.

We recommend keeping the Wifi AP mode enabled, so you can always connect
your phone/tablet to the module's network and access the web UI at
the fixed IP address: ``http://192.168.4.1/``

In the network the Wifi client is connected to, you normally can access
the module by its name in the mDNS ".local" domain, i.e. ``http://<vehicleid>.local/``,
and of course as well directly via the IP address assigned to the module
in that network. Consider configuring your Wifi router to assign a fixed
IP address to the module.

An alternative option to using the module AP is letting the module connect
to your smart phone's AP (tethering). That way it can also use the internet
connection of your smart phone (as long as it's connected). You may need to
connect via the IP address though, mDNS does not work on most tethering
implementations.

Finally, if your mobile data plan includes inbound IP routing or some proxy service
(e.g. for Hologram.io: `Spacebridge <https://support.hologram.io/hc/en-us/articles/360035212654-Spacebridge-for-beginners>`_),
you can connect using the IP address or forwarding port provided. Be aware that's
slow compared to Wifi, but totally usable, as the web framework is optimized for
low data usage and dynamically loads assets as needed.

For "Spacebridge", you only need a proxy rule for destination port 80
to access the web UI, as the WebSocket also runs on port 80. If you
configured your web server for HTTPS, add a rule for port 443.

.. note:: Hint: using a service like "Spacebridge" also enables you to connect to the
  module's SSH port via the cellular network. To use this, add a proxy rule
  for destination port 22 (SSH).

Note: depending on your provider and data plan, inbound IP routing may
be implemented by some proxy service (like "Spacebridge") or by using actual
public IP addresses. If you get a dynamic public IP, you'll currently need 
to take care of determining the IP yourself, the module does not yet provide
a dynamic DNS client. Using the builtin Javascript plugin extendability,
you can attach a HTTP call to network changes to implement a simple DynDNS
client.


-------------------
User Authentication
-------------------

The authentication necessary differs by the page or file accessed. For example,
the "home" menu (page of buttons to all functions) and the dashboard can always
be accessed without any authentication.

When first accessing a protected page, the module will ask for a login
and password:

  - Login: currently always ``admin`` (or leave empty)
  - Password: enter your configured module password (Config → Password)

.. note:: When using stored passwords and having trouble logging in, remember
  your browser password manager may have decided to save your Wifi or server
  password before. Try manual entry.

On a new module or right after a factory reset, no login is necessary,
as the module password is unset. You should secure access as soon as
possible by setting a password via the setup wizard or Config → Password.

User sessions are valid for 60 minutes. Any UI action taken (i.e. opening
some page) renews the session for another 60 minutes. Note: the session
ID is stored in a temporary session cookie, so it will also be lost
when closing the browser.

To actively end a session, click "Logout" in the menu bar.

URLs meant to be used as sessionless (REST) API endpoints provide alternative
authentication based on an ``apikey`` parameter. This parameter currently expects
the configured module password, an actual API key management is still todo.

The authentication method for accessing files on the SD card is by default
standard HTTP digest authentication also requiring the module (admin) password.
This can be disabled in the webserver configuration (Config → Webserver).
There's also an option to restrict authentication to certain directories
only (see config UI for details).


--------------
Enabling HTTPS
--------------

To enable encryption (HTTPS, WSS) you simply need to install a TLS certificate
+ key.

Public certification authorities (CAs) won't issue certificates for private
hosts and IP addresses, so we recommend to create a self-signed TLS certificate
for your module. Use a maximum key size of 2048 bit for acceptable performance.

Example/template using OpenSSL::

  openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \
    -keyout ovms.key -out ovms.crt -subj "/CN=vehicleid.local" \
    -addext "subjectAltName=IP:192.168.4.1,IP:192.168.2.101"

Change the name and add more IPs as needed. The command produces two files in
your current directory, ``ovms.crt`` and ``ovms.key``. Copy their contents
into the respective fields in Config → Webserver.

.. note:: As this is a self-signed certificate, you will need to explicitly
  allow the browser to access the module on the first HTTPS connect.


-----------------
Tools & Utilities
-----------------

.. toctree::
   :maxdepth: 2

   ../components/ovms_webserver/docs/bms-cell-monitor

