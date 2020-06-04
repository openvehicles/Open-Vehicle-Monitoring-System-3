======================
Encryption Scheme 0x31
======================

This scheme is based on server authentication, and is supported in OVMS server v3 and later. Typically, servers provide two methods of authentication:

* Username + Password: The usual username and associated password, as used when registering on the server.
* Username + ApiToken: An API token based authentication scheme, where each user may maintain one or more access tokens (which can be created or revoked at will).

The arguments to the MP-* startup command are:

* <protection scheme>: Must be set to '1' (0x31)
* <token>: Must be the username pre-registered with the server
* <digest>: Must be the password, or api token, for that user
* <car id>: Must be the vehicle ID to connect to, or the special value '*' to request connection to the first vehicle on that user's account

Once the partner has been authenticated, a response 'MP-S 1 <user> <vehicleid> <list-of-all-other-vehicles>' will be sent from the server to indicate a successful authentication.

From then onwards, the messages are in plaintext with no further encryption or encoding. Lines are terminated with CR+LF.

Note that this scheme is intended to be used with external encryption schemes, such as SSL/TLS.
