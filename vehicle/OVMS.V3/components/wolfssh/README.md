wolfssh
=======

wolfSSL's Embeddable SSH Server

dependencies
------------

wolfSSH is dependent on wolfCrypt. The simplest configuration of wolfSSL
required for wolfSSH is the default build.

    $ cd wolfssl
    $ ./configure [OPTIONS]
    $ make check
    $ sudo make install

To use the key generation function in wolfSSH, wolfSSL will need to be
configured with keygen: `--enable-keygen`.

If the bulk of wolfSSL code isn't desired, wolfSSL can be configured with
the crypto only option: `--enable-cryptonly`.


building
--------

From the source directory run:

    $ ./autogen.sh
    $ ./configure
    $ make
    $ make check

The `autogen.sh` script only has to be run the first time after cloning the
repository. If you have already run it or are using code from a source
archive, you should skip it.


examples
--------

The directory `examples` contains an echoserver that any client should be able
to connect to. From the terminal run:

    $ ./examples/echoserver/echoserver

From another terminal run:

    $ ssh_client localhost -p 22222

The server will send a canned banner to the client:

    CANNED BANNER
    This server is an example test server. It should have its own banner, but
    it is currently using a canned one in the library. Be happy or not.

Characters typed into the client will be echoed to the screen by the server.
If the characters are echoed twice, the client has local echo enabled.


testing notes
-------------

After cloning the repository, be sure to make the testing private keys read-
only for the user, otherwise ssh_client will tell you to do it.

    $ chmod 0600 ./keys/key-gretel.pem ./keys/key-hansel.pem

Authentication against the example echoserver can be done with a password or
public key. To use a password the command line:

    $ ssh_client -p 22222 USER@localhost

Where the `USER` and password pairs are:

    jill:upthehill
    jack:fetchapail

To use public key authentication use the command line:

    $ ssh_client -i ./keys/key-USER.pem -p 22222 USER@localhost

Where the user can be `gretel` or `hansel`.


release notes
-------------

### wolfSSH v1.1.0 (06/16/2017)

- Added DH Group Exchange with SHA-256 hashing to the key exchange.
- Removed the canned banner and provided a function to set a banner string.
  If no sting is provided, no banner is sent.
- Expanded the make checking to include an API test.
- Added a function that returns session statistics.
- When connecting to the echoserver, hitting Ctrl-E will give you some
  session statistics.
- Parse and reply to the Global Request message.
- Fixed a bug with client initiated rekeying.
- Fixed a bug with the GetString function.
- Other small bug fixes and enhancements.

### wolfSSH v1.0.0 (10/24/2016)

Initial release.
