============
Introduction
============

The OVMS (Open Vehicle Monitoring System) team is a group of enthusiasts who are developing a means to remotely communicate with our cars, and are having fun while doing it.
 
The OVMS module is a low-cost hardware device that you install in your car simply by installing a SIM card, connecting the module to your car's Diagnostic port connector, and optionally positioning a cellular antenna. Once connected, the OVMS module enables remote control and monitoring of your car.

These  guides are for the version v3 of OVMS module.

There are four ways for you to communicate with the OVMS module:
 
#. If you have the cellular option, and a cellular plan that supports SMS messages, you can send text messages from a cell phone to the OVMS module's phone number. The module will respond back via text messaging. If you want, the OVMS module can also send text messages to you when the car reaches certain states, such as if charging is interrupted.

#. Using either a cellular data connection, or WiFi, you can use a smartphone App. Both the OVMS module and the App communicate with an OVMS server via TCP/IP over the Internet. The smartphone Apps provide a richer experience and more functionality than SMS, but they do require a data plan on the SIM card you purchase and install in the OVMS module.

#. Use a laptop/workstation with a USB port to communicate using a serial terminal.

#. Use WiFi and a web browser / telnet / ssh console client.
  
This Guide will help you setup and configure your OVMS module. Initial configuration of the OVMS v3 module is usually done over WiFi using a Web Browser, telnet or ssh. Once configured, you can use SMS, cell phone Apps, laptops, or WiFi to communicate with the OVMS module.

