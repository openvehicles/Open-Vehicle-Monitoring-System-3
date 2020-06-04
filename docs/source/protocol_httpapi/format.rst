======
Format
======

The API is a RESTfull service running at:

* http://api.openvehicles.com:6868/api/...
* https://api.openvehicles.com:6869/api/...

Other OVMS servers should provide this API on the same ports, but for the
rest of this manual we will simply refer to api.openvehicles.com as the
server being used.

The return data is a json formatted array of hashes. Each record is one for
one vehicle and shows you the vehicle id, as well as counts for the number of
apps currently connected to that vehicle, and whether the vehicle is
connected to the net (server) or not.

From the server point of view, we can treat an api session just like an app
session. From the vehicle point of view there will be no difference - once an
API connects to a vehicle, the server will send a "Z 1" message to tell the
module it has a connection. If the session times out or is logged out, the
server will inform the modules in the vehicles.

-------
Example
-------

For example, once authenticated you can request a list of vehicles on your account:

::

  $ curl -v -X GET -b cookiejar http://api.openvehicles.com:6868/api/vehicles
  * About to connect() to api.openvehicles.com port 6868 (#0)
  *   Trying api.openvehicles.com....
  * connected
  * Connected to tmc.openvehicles.com (64.111.70.40) port 6868 (#0)
  > GET /api/vehicles HTTP/1.1
  User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0
  OpenSSL/0.9.8r zlib/1.2.5
  Host: api.openvehicles.com:6868
   Accept: */*
   Cookie: ovmsapisession=9ed66d0e-5768-414e-b06d-476f13be40ff
  
  * HTTP 1.0, assume close after body
  < HTTP/1.0 200 Logout ok
  < Connection: close
  < Content-Length: 280
  < Cache-Control: max-age=0
  < Content-Type: application/json
  < Date: Fri, 22 Feb 2013 12:44:41 GMT
  < Expires: Fri, 22 Feb 2013 12:44:41 GMT
  < 
  [
    {"id":"DEMO","v_apps_connected":0,"v_net_connected":1},
    {"id":"MARKSCAR","v_apps_connected":1,"v_net_connected":1},
    {"id":"QCCAR","v_apps_connected":0,"v_net_connected":0},
    {"id":"RALLYCAR","v_apps_connected":0,"v_net_connected":0},
    {"id":"TESTCAR","v_apps_connected":0,"v_net_connected":0}
  ]

(note that the above is re-formatted slightly, to make it clearer to read).
