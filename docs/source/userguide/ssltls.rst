=======
SSL/TLS
=======

.. highlight:: none

---------------------------------------
SSL/TLS Trusted Certificate Authorities
---------------------------------------

A default minimal list of trusted certificate authorities (CA) is provided with the firmware. you
can see the current loaded list with the ``tls trust list`` command::

  OVMS# tls trust list
  AddTrust External CA Root length 1521
  1521 byte certificate: AddTrust External CA Root
    cert. version     : 3
    serial number     : 01
    issuer name       : C=SE, O=AddTrust AB, OU=AddTrust External TTP Network, CN=AddTrust External CA Root
    subject name      : C=SE, O=AddTrust AB, OU=AddTrust External TTP Network, CN=AddTrust External CA Root
    issued  on        : 2000-05-30 10:48:38
    expires on        : 2020-05-30 10:48:38
    signed using      : RSA with SHA1
    RSA key size      : 2048 bits
    basic constraints : CA=true
    key usage         : Key Cert Sign, CRL Sign

If you want to add to this list, you can place the PEM formatted root CA certificate in the
``/store/trustedca`` directory on your config partition using the text editor. Then, reload the list with::

  OVMS# tls trust reload
  Reloading SSL/TLS trusted CA list
  SSL/TLS has 4 trusted CAs, using 5511 bytes of memory

On boot, the trusted Certificate Authorities provided in firmware, and put in your ``/store/trustedca``
directory, will be automatically loaded and made available.

These trusted certificate authorities are used by the various module in the OVMS system, when
establishing SSL/TLS connections (in order to verify the certificate of the server being
connected to).


----------------------------------
How to get the CA PEM for a Server
----------------------------------

^^^^^^^^^^^^^^^^
Download from CA
^^^^^^^^^^^^^^^^

If you know the CA, check their download section.


^^^^^^^^^^^^^^^
Using a Browser
^^^^^^^^^^^^^^^

This only works for https servers:

#. Open the website you want to access
#. Open the encryption info (e.g. Chrome: lock icon at URL → show certificate)
#. Display the certificate chain (e.g. Chrome: tab "Details", first element)
#. Select the last entry before the actual server certificate
#. Export the certificate in PEM format (usually the default)
#. Install the file contents as shown above


^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Using OpenSSL or GNU TLS CLI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This works for all servers and ports::

  openssl s_client -connect HOSTNAME:PORT -servername HOSTNAME -showcerts </dev/null \
    | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p'

…or…::

  gnutls-cli --print-cert --port PORT HOSTNAME </dev/null \
    | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p'

Substitute HOSTNAME and PORT accordingly, e.g. https = port 443. The ``sed`` just strips
the other info from the output and can be omitted to check for errors or details.

There should be two certificates in the output, look for BEGIN and END markers. The first one is
the server certificate, the second one the CA certificate. Copy that second certificate into
the editor, take care to include the BEGIN and END lines.
