=======
SSL/TLS
=======

.. highlight:: none

.. warning:: **Let's Encrypt root certificate expiration on September 30, 2021:**
  
  To prevent losing TLS connectivity with an LE secured server (like ``dexters-web.de``)
  with **module firmware versions 3.2.016 or earlier**,
  you need to install the new root certificate manually. Sorry for the inconvenience!
  
  See below: `Let's Encrypt Root Certificate Update`_


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


-------------------------------------
Let's Encrypt Root Certificate Update
-------------------------------------

**If running firmware version 3.2.016 or earlier**, you need to install the new Let's Encrypt 
root certificate ("ISRG Root X1") manually to enable encrypted connections to servers
secured by a Let's Encrypt certificate.

1. Save the certificate text shown below to file ``/store/trustedca/isrgx1``
2. Issue command ``tls trust reload`` or reboot the module

Certificate text::

  -----BEGIN CERTIFICATE-----
  MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
  TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
  cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
  WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
  ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
  MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
  h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
  0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
  A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
  T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
  B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
  B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
  KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
  OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
  jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
  qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
  rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
  HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
  hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
  ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
  3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
  NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
  ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
  TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
  jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
  oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
  4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
  mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
  emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
  -----END CERTIFICATE-----


