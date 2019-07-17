# Open-Vehicle-Monitoring-System-3 - SWCAN branch

----------------------

This branch of OVMS contains patches to add SWCAN (Single-Wire CAN, GMLAN) support to OVMS. It is possible to use the existing hardware of OVMS v3, but to fully support SWCAN capabilities (especially the High-Voltage Wake-Up function to wake the car and consequentally to enable larger remote controlling capabilities), an [add-on SWCAN board](https://github.com/mjuhanne/OVMS-SWCAN) is needed.

To enable the additional hardware support, launch "make menuconfig" and select Component config -> OVMS -> Component options -> Include support for external SWCAN module

----------------------

Open Vehicle Monitoring System

Why?

We are a group of enthusiasts who want an interface to be able to talk to our cars remotely, perhaps
add on-car displays (such as heads-up speed), and we want to have fun doing it.

What is it?

The Open Vehicle Monitoring System is three things:

1] A low-cost module that fits in the car. It is powered by the car, talks to the car on the CAN bus,
   and talks to its user over Wifi, Bluetooth, or Cellular networks.

2] A server. The car module can be configured to either talk to the server (via UDP/IP or TCP/IP over
   the Internet) or the user directly (via SMS). It will also support the IoT standard MQTT protocol
   for integration to home automation, DIY, and other such systems.

3] A cellphone App. This talks to the server (via TCP/IP protocol) to retrieve messages from the
   car and issue instructions.

Part [1] is all that is required. You can use a cellphone and SMS messages to talk to the App. It
requires a SMS messaging plan on the SIM card in the GSM modem in the car.

Parts [2] and [3] provide a much more seamless and powerful experience, but are optional.
They requires a small data plan on the SIM card in the GSM modem in the car.

Even if you choose [2]+[3], you can still use [1] as well (for initial setup as well as ongoing on-demand).

Open Source

The entire project is open source - from the hardware schematics to the APIs to the car and server firmware.

The purpose of this project is to get the community of vehicle hackers and enthusiasts to be able to expand the
project. We can't do it all, and there is so much to do. What we are doing is providing an affordable and
flexible base that the community can work on and extend.

Everything is open, and APIs are public. Other car modules can talk to the server, and other Apps can show the
status and control the car. This will be a foundation that will hope others will interface to and and build upon.

Thanks

So many people to thank. W.Petefish for sourcing the car connector, Fuzzylogic for the original hardware and
software design and demonstration of it working, Scott451 for figuring out many of the Roadster CAN bus
messages, all the hard workign open source developers that did so much for OVMS v2, and many others for
showing that this kind of thing can work in the real world.

Thanks,
markwj
