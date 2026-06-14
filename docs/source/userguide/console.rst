================
The OVMS Console
================

-------------------
Console Connections
-------------------

OVMS v3 includes a powerful command line console that can be accessed in various ways:

1. Using a micro USB cable to a host computer.

  If the OVMS is not recognised via USB download the driver from `SILABS website <https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers>`_. You will also need a suitable terminal emulator. The baud rate is 115200, and you should not enable hardware flow control.

2. TELNET (over wifi).

  Note that for security reasons, the telnet server component is not enabled in the default production firmware (but may be available in custom builds). If Telnet is available telnet to the IP address of the module (or <vehicleid>.local MDNS address).

3. SSH (over wifi).

  SSH to the IP address of the module (or <vehicleid>.local MDNS address). Note that when first booted with a network connection, the module takes a minute or so to generate server side keys (which are stored in the config store).

4. Web Console SHELL tab.

  Use a web browser to connect to the IP address of the module (or  <vehicleid>.local MDNS address). A SHELL tab is available for direct command line console access.

5. Remote Apps.

  The OVMS Android App currently includes a shell screen that can be used to issue command line console commands via the OVMS server v2. This functionality is not currently available in the iPhone/iPad App.

6. SMS.

  Console commands can be issued via SMS in the future (not yet implemented).

^^^^^^^^^^^
USB Console
^^^^^^^^^^^

Our recommendations for the USB console are as follows:

1. You can use a Windows, Linux, Mac OSX workstation or an Android device with a USB OTG adapter cable.

2. If your operating system does not have the SILABS USB driver, you can download the driver from `SILABS website <https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers>`_. If you use Linux and your distribution includes the braille display driver “brltty”, you may need to uninstall that, as it claims any CP2102 device to be a braille device. This applies e.g. to openSuSE 15.0.

3. Plug in the module to your PC/laptop, using a micro USB cable. Check to ensure a serial port appears (using the SILABS driver). For OSX and Linux this will normally appear as /dev/tty.SLAB_USBtoUART or /dev/ttyUSB0 (or 1/2/… if other serial devices are connected). List your devices using “ls /dev/\*USB\*”.

4. Once the serial port is available you will need a terminal emulator.

  * For OSX, the simplest is the built-in SCREEN utility. You run this as ``screen -L /dev/tty.SLAB_USBtoUART 115200`` But note that the device path may be different for you - check with ‘ls /dev/\*USB\*’. You can use ‘control-a control-\ y’ or ‘control-a k y’ (three key sequences) to exit the screen. The “-L” option tells screen to capture a log of your session into the file “screenlog.<n>”.
  * For Linux, the SCREEN utility is also simple to get. If it is not included with your distribution, you can simply *yum install screen*, or *apt-get install screen* (depending on your distribution). From there, the command is the same as for OSX. Alternatively, you can use minicom (which is included with many linux distributions).
  * For Windows, a simple approach is to download the free PUTTY terminal emulator. This supports both direct ASYNC (over USB) connections, as well as SSH (network). You can download putty `using this link <https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html>`_.
  * For Android, there are `multiple USB serial Apps in the Play store <https://play.google.com/store/search?q=usb+serial+terminal&c=apps>`_. A good recommendation is `Serial USB Terminal by Kai Morich <https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal>`_.

5. Once you have established the connection, press ENTER to see the “OVMS>” prompt.

^^^^^^^^^^^
SSH Console
^^^^^^^^^^^

A workstation (Mac, Linux, Window), on the same wifi network as the OVMS module, can use the ssh protocol to connect. In Windows you can use the free PUTTY ssh client. In Linux and OSX ssh is built-in.

The syntax is simply:

  ``ssh user@ip``

Where ‘user’ is the username (normally ‘ovms’) and ‘ip’ is the IP address of the OVMS v3 module. In environments supporting mDNS networking, you should also be able to connect using the mDNS name *<vehicleid>.local*. The password you enter is the module password.

If you use ssh public/private key pairs, you can store your public key on the OVMS v3 module, to take advantage of passwordless login.

  ``OVMS# config set ssh.keys <user> <public-key>``

In this case, ‘user’ is the username you use to ssh, and the public key is your RSA public key (the long base64 blob of text you find in id-rsa.pub between ‘ssh-rsa’ and your username/comment).

You can also use SCP to copy files to and from the OVMS v3 VFS.

.. note::
  With OpenSSH version 8.8 (or later), the ``ssh-rsa`` algorithm has been disabled by default and
  needs to be enabled manually, either on the command line::

    ssh -o HostkeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa user@ip

  …or by adding a host entry to your ``~/.ssh/config`` file::

    Host ovmsname.local
	HostkeyAlgorithms +ssh-rsa
	PubkeyAcceptedAlgorithms +ssh-rsa

.. note::
  With OpenSSH version 9.0 (or later), the ``scp`` **protocol** has been disabled by default and
  replaced by the ``sftp`` **protocol**. To be able to use the ``scp`` **command** with OVMS, you need
  to re-enable the ``scp`` **protocol** with option ``-O`` on the command line::

    scp -O ....

--------------
Console Basics
--------------

.. highlight:: none

Let’s use SSH to demonstrate this::

  $ ssh ovms@ovms.local

  Welcome to the Open Vehicle Monitoring System (OVMS) - SSH Console
  Firmware: 3.1.003-2-g7ea18b4-dirty/factory/main
  Hardware: OVMS WIFI BLE BT cores=2 rev=ESP32/1

  OVMS#

When first connecting using USB, the console will be in non-secure mode (as indicated by the “OVMS>” prompt). Here, only a limited number of commands are available (such as viewing network status, modem status, or system time). To get to secure mode, enter the command ‘enable’, and provide the module password. The prompt will then change to “OVMS#”::

  OVMS> enable
  Password:
  Secure mode
  OVMS#

You can enter the ‘disable’ command to get out of secure mode, and ‘exit’ to exit the console completely.

When connecting via a pre-authenticated protocol such as SSH, you will be in secure mode automatically.

At any time, you can use “?” to show the available commands. For example::

  OVMS# ?
  .                    Run a script
  boot                 BOOT framework
  can                  CAN framework
  charge               Charging framework
  co                   CANopen framework
  config               CONFIG framework
  disable              Leave secure mode (disable access to most commands)
  egpio                EGPIO framework
  enable               Enter secure mode (enable access to all commands)
  event                EVENT framework
  exit                 End console session
  help                 Ask for help
  homelink             Activate specified homelink button
  location             LOCATION framework
  lock                 Lock vehicle
  log                  LOG framework
  metrics              METRICS framework
  module               MODULE framework
  network              NETWORK framework
  notify               NOTIFICATION framework
  obdii                OBDII framework
  ota                  OTA framework
  power                Power control
  re                   RE framework
  script               Run a script
  sd                   SD CARD framework
  server               OVMS Server Connection framework
  modem                MODEM framework
  stat                 Show vehicle status
  store                STORE framework
  test                 Test framework
  time                 TIME framework
  unlock               Unlock vehicle
  unvalet              Deactivate valet mode
  valet                Activate valet mode
  vehicle              Vehicle framework
  vfs                  Virtual File System framework
  wakeup               Wake up vehicle
  wifi                 WIFI framework

You can also use “?” as part of a command to expand on the available options within that command root::

  OVMS# wifi ?
  mode                 WIFI mode framework
  scan                 Perform a wifi scan
  status               Show wifi status

  OVMS# wifi mode ?
  ap                   Acts as a WIFI Access Point
  apclient             Acts as a WIFI Access Point and Client
  client               Connect to a WIFI network as a client
  off                  Turn off wifi networking

  OVMS# wifi mode client ?
  Usage: wifi mode client <ssid> <bssid>

Command tokens can be abbreviated so long as enough characters are
entered to uniquely identify the command.  Optionally pressing
TAB at that point will auto-complete the token.  If the abbreviated form is not
sufficient to be unique (in particular if no characters have been
entered yet) then TAB will show a concise list of the possible
subcommands and retype the portion of the command line already
entered so it can be completed::

  OVMS# wifi <TAB>
  mode scan status
  OVMS# wifi █

Pressing TAB is legal at any point in the command; if there is nothing
more that can be completed automatically then there will just be no
response to the TAB.

