=================
Auto Provisioning
=================

A caller can perform auto-provisioning at any time (authenticated or not). However, only one auto-provision can be performed for each connection.

Auto-Provisioning relies on two secrets known both to the server and client. The first is usually the VIN of the vehicle and the second is usually the ICCID of the SIM card in the vehicle module. The reason these two are chosen is that they can be auto-determined by the vehicle module, but also clearly seen by the user (for entry into the server).

The mechanism works by the client module first determining its VIN and ICCID secrets, then connecting to the server and sending a AP-C message to the server proving its VIN. The server will then lookup the auto-provisioning record, and reply with that to the client (via a AP-S or AP-X message).

The auto-provisioning record itself is platform dependent, but will typically be an ordered space separated list of parameter values. For OVMS hardware, and OVMS.X PIC firmware, these are merely parameters #0, #1, #2, etc, to be stored in the car module.

----------------------------------------
Auto Provisioning Protection Scheme 0x30
----------------------------------------

1. For cars

  AP-C <protection scheme> <apkey>

2. For servers

  AP-S <protection scheme> <server token> <server digest> <provisioning>

  AP-X

When the provisioning record is created at the server, a random token is generated (encoded as textual characters). The server then hmac-md5s this token with the shared secret (usually the ICCID known by the server) to create a digest, base64 encoded. Using this hmac digest, the server generates and discards 1024 bytes of cipher text. The server then rc4 encrypts and base64 encodes the provisioning information and stores its token, digest and encoded provision record ready for the client to request.

The car knows its VIN and ICCID. With this information, it makes a connection to the OVMS service on the server, and provides the VIN as the <apkey> in a AP-C message to the server.

The server will ensure that it only responds to one AP-C message for any one connection. Once responded, all subsequent AP-C requests will always be replied with a AP-X message.

Upon receiving the AP-C message, the server looks up any provisioning records it has for the given <apkey>. If it has none, it replies with an AP-X message.

If the server does find a matching provisioning record, it replies with an AP-S message sending the previously saved server token, digest and encoded provisioning record to the car.

If the car receives an AP-X message, it knows that auto-provisioning was not successful.

If the car receives an AP-S message, it can first validate the server authenticity. By producing its own hmac-md5 of the server token and secret ICCID, the car can validate the server-provided digest is as expected. If this validation step does not succeed, the car should abort the auto-provisioning.

If acceptable, the car can decrypt the provisioning record by first generating and discarding 1024 bytes of cipher text, then decoding the provisioning record provided by the server.
