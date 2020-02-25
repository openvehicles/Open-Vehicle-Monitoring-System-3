==============
Authentication
==============

---------------------------
Cookie Based Authentication
---------------------------

The first thing you must do to use the api is authenticate to establish a
session and get a cookie (so you don't need to authenticate every time). This
is done with a http "GET /api/cookie" passing parameters 'username' and
'password' as your www.openvehicles.com username and password respectively:

::

    $ curl -v -X GET -c cookiejar
    http://api.openvehicles.com:6868/api/cookie?username=USERNAME\&password=PASSWORD
    * About to connect() to tmc.openvehicles.com port 6868 (#0)
    *   Trying 64.111.70.40...
    * connected
    * Connected to tmc.openvehicles.com (64.111.70.40) port 6868 (#0)
    GET /api/cookie?username=USERNAME&password=PASSWORD HTTP/1.1
    User-Agent: curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0
    OpenSSL/0.9.8r zlib/1.2.5
    Host: tmc.openvehicles.com:6868
    Accept: */*
    
    * HTTP 1.0, assume close after body
    < HTTP/1.0 200 Authentication ok
    < Connection: close
    < Content-Length: 9
    < Cache-Control: max-age=0
    < Content-Type: text/plain
    * Added cookie ovmsapisession="9ed66d0e-5768-414e-b06d-476f13be40ff" for domain tmc.openvehicles.com, path /api/, expire 0
    < Set-Cookie: ovmsapisession=9ed66d0e-5768-414e-b06d-476f13be40ff
    < Date: Fri, 22 Feb 2013 12:43:56 GMT
    < Expires: Fri, 22 Feb 2013 12:43:56 GMT
    < 
    Login ok

Once logged in, all subsequent requests should pass the cookie
(ovmsapisession). The session will expire after 3 minutes of no use, or you
can specifically terminate / logout the session by calling "DELETE /api/cookie".

