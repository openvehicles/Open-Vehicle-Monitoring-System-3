=======
Plugins
=======

-------
ApiHttp
-------

This plugin provides an HTTP (and optional HTTPS) server and is used by subsequent
API plugins to provide HTTP services. The HTTP server is on port tcp/6868. If you
provide a conf/ovms_server.pem file, it will also use that to launch a HTTPS
server on port tcp/6869.

-----------
ApiHttpCore
-----------

This plugin provides support for the HTTP API in OVMS Server v3. It supports
the /api HTTP endpoint.

If you want to use this plugin, you need to also include 'ApiHttp' before loading this.

-----------
ApiHttpFile
-----------

This plugin provides support serving static files from the httpfiles directory. It
supports the /file HTTP endpoint.

If you want to use this plugin, you need to also include 'ApiHttp' before loading this.


------------
ApiHttpMqapi
------------

This plugin provides support for Mosquitto API functions for authentication and
ACL access control. It supports the /mqapi HTTP endpoint.

If you want to use this plugin, you need to also include 'ApiHttp' before loading this.

You will also need to configure your mosquitto.conf to include:

.. code-block:: text

    auth_opt_superusers <your-superuser-username>
    auth_opt_acl_cacheseconds 300
    auth_opt_auth_cacheseconds 300
    auth_opt_backends http
    auth_opt_http_ip 127.0.0.1
    auth_opt_http_port 6868
    auth_opt_http_getuser_uri /mqapi/auth
    auth_opt_http_superuser_uri /mqapi/superuser
    auth_opt_http_aclcheck_uri /mqapi/acl

-----
ApiV2
-----

This plugin provides support for the v2 OVMS protocol, including crypto
protocols 0x30 and 0x31. It provides a raw v2 protocol server on tcp/6867.
If you provide a conf/ovms_server.pem file, it will also launch an SSL enabled
v2 protocol server on tcp/6870.

----------
AuthDrupal
----------

This plugin provides support for authentication via the Drupal database. It
also provides a facility to periodically synchronise drupal users to OVMS
owner table records.

Note that only one Auth* plugin should be enabled.

--------
AuthNone
--------

This is a stub authentication plugin. It will always fail authentication,
and is only provided as an example (or for use in systems where user
authentication is not required).

Note that only one Auth* plugin should be enabled.

-----
DbDBI
-----

This is the core database plugin for perl DBI style SQL databases. It is
used, for example, to provide access to MySQL databases.

----
Push
----

This provides the core support for Push Notifications. The actual notifications
themselves are issued by sub-plugins which register with this.

--------
PushAPNS
--------

This plugin supports the 'apns' notification type, for push notifications
to Apple devices (iPhones, iPads, etc). It requires apple certificates,
in PEM format, to be placed in conf/ovms_apns_sandbox.pem and
conf/ovms_apns_production.pem.

-------
PushGCM
-------

This plugin supports the 'gcm' notification type, for push notifications
to Android devices. It requires a google API key to be placed in the
conf/ovms_server.conf file under [gcm] section, parameter 'apikey'. You
can obtain that key by:

# Log in to your Google account
# Open https://developers.google.com/mobile/add
# Select platform "Android"
# Enter an arbitrary project name, e.g. "MyOvmsServer"
# Package name should be "com.openvehicles.OVMS"
# Activate "Cloud Messaging" and generate the key
# Note the API key and project number (= GCM sender ID)

--------
PushMAIL
--------

This plugin supports the 'mail' notification type, for push notifications
by eMail. It simply requires a sendmail style mailer installed on the server.

You can configure conf/ovms_server.conf [mail] section parameter 'sender' as
the sender address to be used (otherwise defaulting to 'notifications@openvehicles.com'.

----
VECE
----

This plugin extends the notification system to translate error codes into
textual error messgaes. It uses files in the vece directory, named
<vehicletype>.vece (for example, tr.vece, va.vece, etc).

The vece files themselves are ini style files.
