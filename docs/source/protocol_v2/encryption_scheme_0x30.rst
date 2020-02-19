======================
Encryption Scheme 0x30
======================

This scheme is based on using shared secrets, hmac digest for authentication and encryption key negotiation, with RC4 stream cipher and base64 encoding.

Upon startup, both the server and callers generate random tokens (encoded as textual characters). Each party then hmac-md5s the token with the shared secret to create a digest, base64 encoded.

Note that the server will only issue it's welcome message after receiving and validating the caller's welcome message.

Validation of the welcome message is performed by:

#. Checking the received token to ensure that it is different from its own token, and aborting the connection if the same.
#. Hmac-md5s the received token with the shared secret and comparing the result to the received digest.
#. If the digest match, then the partner had authenticated itself (proven it knows the shared secret, or has listened to a previous conversation).
#. If the digests don't match, then abort the connection as the partner doesn't agree with the shared secret.

Once the partner has been authenticated:

#. Create a new hmac-md5 based on the client and server tokens concatenated, with the shared secret.
#. Use the new hmac digest as the key for symmetric rc4 encryption.
#. Generate, and discard, 1024 bytes of cipher text.

From this point on, messages are rc4 encrypted and base64 encoded before transmission. Lines are terminated with CR+LF.
