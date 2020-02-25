=======================
Backwards Compatibility
=======================

Typically, comma-separated lists are used to transmit parameters.
Applications, Servers and the Car firmware should in general ignore extra parameters not expected.
In this way, the protocol messages can be extended by adding extra parameters,
without breaking old Apps/Cars that don't expect the new parameters.

Similarly, unrecognized messages should due ignored.
Unrecognised commands in the "C" (command) message should be responded to with a generic
"unrecognized" response (in the "c" (command response) messages).

