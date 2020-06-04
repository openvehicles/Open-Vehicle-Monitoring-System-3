==============
Authentication
==============

------------
Alternatives
------------

Each API call must be authenticated, and there are currently three options for this:

# Username+Passsword authentication: Specify parameters 'username' and 'password' in the URL, and they will be validated against registered users on the server.
# Username+ApiToken authentication: Specify parameter 'username' as the registered user, and 'password' as a registered API token, in the URL, and they will be validated against registered API tokens for that user on the server.
# Cookie authentication: A cookie may be obtained (by either of the above two authentication methods), and then used for subsequent API calls.

---------------------------
Cookie Based Authentication
---------------------------

Cookie based authentication avoids the requirement to authenticate
each time. It is also a requirement for session based API calls.
This is done with a http "GET /api/cookie" passing parameters 'username' and
'password' as your www.openvehicles.com username and password
(or API token) respectively:

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

The cookie can be destroyed (and session logged out) using the DELETE method (passing the original cookie):

::

    $ curl -v -X DELETE -b cookiejar
    http://api.openvehicles.com:6868/api/cookie

---------------------
API Token Maintenance
---------------------

An API token can be created with a POST to the /api/token API endpoint:

::

    $ curl -v -X POST -F 'application=<app>' -F 'purpose=<purpose>' -F 'permit=<permit>'
    http://api.openvehicles.com:6868/api/token

Note that the 'purpose' and 'application' fields are comments attached to the token and are
intended to identify the application that created/uses the token and the purpose that
token is used for.

The 'permit' field defines the list of rights granted to the user of this token.

Any of the three authentication mechanisms can be used for this, so long as the permissions include
either 'token.admin' or 'admin' rights.

An API token can be deleted with a DELETE to the /api/token/<TOKEN> API endpoint:

::

    $ curl -v -X DELETE http://api.openvehicles.com:6868/api/token/<TOKEN>

