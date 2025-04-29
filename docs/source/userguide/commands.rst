========================
Remote Command Execution
========================

------------
API Overview
------------

Remote command execution can be done via these APIs:

  - **With direct/routed network connection to the module**:

    - Module HTTP Server
    - Module SSH Server

  - **With indirect connection via a server**:

    - V2 Server (OVMS MP TCP/IP protocol)
    - V3 Server (MQTT)
    - Additional custom APIs provided by the server used (e.g. HTTP REST API on dexters-web.de)
    - App Scripting APIs (e.g. Android Broadcast Intents)

Direct/routed connections work best with a Wifi connection to the module, either
to the module's client IP in a Wifi network or to the module's own AP network.
Some cellular network providers also offer options for direct or bridged IP
routing to a mobile device via GSM. When using the recommended Hologram SIM, this option
is called "Spacebridge device tunneling", it can be enabled free of charge in
the Hologram dashboard.

Indirect connection via a server relies on both the module and the client being
connected to the same server. The server acts as a proxy, forwarding the command
request from the client to the module, and the module response back to the client.
This is an asynchronous operation, with potentially long delays and module connection
drops to be taken into account.


-------------
General Rules
-------------

All commands not needing an interactive console can be executed remotely.

Command and option completion via the :kbd:`<TAB>` key needs an interactive console,
but you can use the ``?`` argument to query sub commands and options instead::

  OVMS# can log start ?
  monitor              CAN logging to MONITOR
  tcpclient            CAN logging as TCP client
  tcpserver            CAN logging as TCP server
  udpclient            CAN logging as UDP client
  udpserver            CAN logging as UDP server
  vfs                  CAN logging to VFS

Most commands and command functions don't need an interactive console (only major
exception being the ``vfs edit`` visual text editor). If commands need a confirmation
key press, they normally also provide an option to bypass that step. Example::

  OVMS# module factory reset ?
  Usage: module factory reset [-noconfirm]

All remote APIs require login / API key authentication, so commands are in "enabled"
mode by default, i.e. all commands can be executed directly without a prior "enable"
command step as needed to unlock the USB console.


------------------
Module HTTP Server
------------------

The module's builtin webserver can be reached on port 80 (HTTP) and, if a TLS
certificate has been supplied, on port 443 / HTTPS. It listens on all network
interfaces by default. See :doc:`webui` for details.

The command exection REST API allows both shell and direct Javascript execution.
Authorization can be done by passing the module's local admin password (the same
as used to login to the web UI). Examples:

  -  ``http://myovms.local/api/execute?apikey=mysecret&command=charge%20start``
  -  ``http://myovms.local/api/execute?apikey=mysecret&type=js&command=print(Duktape.version)``

HTTP method may be ``GET`` or ``POST``. Don't forget to URI encode all arguments.
This can be done easily when using `curl <https://curl.se/>`_ to access the API::

  > curl 'http://myovms.local/api/execute' \
    --data-urlencode 'apikey=mysecret' \
    --data-urlencode 'command=charge start'

For more details, see :doc:`../components/ovms_webserver/docs/index`.


-----------------
Module SSH Server
-----------------

The module's builtin SSH server can be reached on port 22. It listens on all
network interfaces by default. For remote command execution via SSH, use public
key authentication. See :doc:`console` for details and necessary setup steps.

Command execution via SSH is straight forward::

  > ssh myovms.local charge start

To execute Javascript, pass the expression to the ``script eval`` command::

  > ssh myovms.local script eval "print(Duktape.version)"


--------------
V2 Server (MP)
--------------

MP (Message Protocol) is the custom protocol used between module, V2 server
and V2 Apps (Android, iOS).

To access a V2 server via MP or create your own client application, use one of these:

  - `Standard OVMS perl client <https://github.com/openvehicles/Open-Vehicle-Server/tree/master/clients>`_
  - `NodeJS OVMS client by Birkir Gudjonsson <https://github.com/openvehicles/ovms-client-nodejs>`_

The OVMS standard **perl client** includes a ``cmd.pl`` script, that allows to send
any MP command. To execute a shell command, use this syntax::

  > ./cmd.pl 7 "start charge"

Note: ``cmd.pl`` outputs the raw MP response, with line breaks encoded as CR (13).
To easily read the response in a shell, pipe the output to ``tr`` like this::

  > ./cmd.pl 7 "stat" | tr '\r' '\n'
  MP-0 c7,0,Not charging
  SOC: 76.5%
  Ideal range: 185km
  Est. range: 149km
  ODO: 10877.0km
  CAC: 111.7Ah
  SOH: 93%

The **NodeJS client** by Birkir Gudjonsson includes a similar, but even easier to use
``cmd.js``, which already does the text post processing and removes the MP header::

  > node cmd.js "stat"
  Not charging
  SOC: 76.5%
  Ideal range: 185km
  Est. range: 149km
  ODO: 10877.0km
  CAC: 111.7Ah
  SOH: 93%

MP command 7 used here processes standard shell commands. To execute Javascript,
pass the expression to the ``script eval`` command as shown above.


----------------
V3 Server (MQTT)
----------------

The ``server v3`` component is an MQTT client, any MQTT server/hub can be used.
MQTT is a standard protocol widely used for IoT applications.

MQTT organizes data transmissions in channels called "topics". You can configure a
common topic prefix in the V3 server configuration. Command exection via MQTT
then follows this scheme:

  - Send command request to topic: ``<prefix>/client/<client_id>/command/<command_id>``
  - Receive response on topic: ``<prefix>/client/<client_id>/response/<command_id>``

This API expects shell commands. To execute Javascript, pass the expression to
the ``script eval`` command as shown above.

Use arbitrary unique client IDs (e.g. some UUID) and command IDs (e.g. command counter)
to identify your client connection and command request, if sending multiple commands
in series over the same connection. The response will use the same IDs as the request.

A standard perl client for command execution via MQTT is included in the OVMS main
repository:

  - https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/tree/master/client

More info on the general OVMS MQTT topic scheme can be found
`on the developer mailing list <http://lists.openvehicles.com/pipermail/ovmsdev/2018-July/005297.html>`_.


------------------
Custom Server APIs
------------------

The :doc:`OVMS server's builtin REST API <../protocol_httpapi/index>` does not
yet implement command execution, but an OVMS server can provide additional APIs
implemented within the server's web framework.


~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Asia-Pacific (openvehicles.com)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Public OVMS server provided and maintained by `Mark Webb-Johnson <mark@webb-johnson.net>`_.

This server does not yet support extended REST APIs.


~~~~~~~~~~~~~~~~~~~~~~~
Europe (dexters-web.de)
~~~~~~~~~~~~~~~~~~~~~~~

Public OVMS server provided and maintained by `Michael Balzer <dexter@dexters-web.de>`_.

This server supports extended REST APIs for command execution and CSV download.

The extended REST API for command execution needs your vehicle ID, the vehicle
password (both as entered in the module's V2 server configuration), and of
course the command to execute:

  - ``https://dexters-web.de/api/ovms/cmd?fn.vehicleid=…&fn.carpass=…&fn.cmd=…``

HTTP method may be ``GET`` or ``POST``. Don't forget to URI encode all arguments.
This can be done easily when using `curl <https://curl.se/>`_ to access the API::

  > curl 'https://dexters-web.de/api/ovms/cmd' \
    --data-urlencode 'fn.vehicleid=MYCAR123' \
    --data-urlencode 'fn.carpass=mysecret' \
    --data-urlencode 'fn.cmd=charge start'

This API is the backend for the server's OVMS web shell, so it supports the extended
command syntax pattern as described there:

  - V2 MP command syntax: ``#<code>[,<parameters>][/<recordcount>]``;
    example: ``#3/32`` will query the 32 V2 parameter slots
  - USSD (cellular network) command syntax: ``*<ussdcode>#``;
    example: ``*100#`` will query the cellular account balance on many networks
  - Modem command syntax: ``@<modemcommand>``;
    example: ``@AT+CPSI?`` = query current cellular network mode

Any other command is expected to be a shell command. To execute Javascript, pass
the expression to the ``script eval`` command as shown above.


------------------
App Scripting APIs
------------------

~~~~~~~
Android
~~~~~~~

If you want to send commands from an Android based device also running the OVMS
App, you can use the App's scripting API. The API is based on standard Android
Broadcast Intents, so can be accessed by scripting Apps like Tasker and KustomWidget.
Calling the API via an ADB shell is also possible.

The API supports interactive and background command execution. Both API modes
support the same extended syntax as the App's integrated command shell. To execute
Javascript, pass the expression to the ``script eval`` command as shown above.

The API works asynchronously:

  - Send a command request via Intent ``com.openvehicles.OVMS.action.COMMAND``
    (interactive mode) or ``com.openvehicles.OVMS.SendCommand`` (background mode)
  - Listen for command response via Intent ``com.openvehicles.OVMS.CommandResult``

To authenticate your API client, you need to use the App's API key, which can be
copied from the App's configuration page.

See
`Android App Wiki <https://github.com/openvehicles/Open-Vehicle-Android/wiki/Command-Execution-via-Broadcast-Intent>`_
for details.


~~~
iOS
~~~

The iOS App does not yet support scripting.

