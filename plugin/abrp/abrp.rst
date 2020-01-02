*****************************************
Send live data to abetterrouteplanner.com
*****************************************
abrp script example
###################


.. topic:: Overview

    `abetterrouteplanner <http://abetterrouteplanner.com>`_  is probably the best website to plan a route with an EV and optimize the number of stops needed and where to charge your vehicle.
    
    Moreover, it has a wonderful functionality, to update your plan, taking into account your live data, to adjust with the real consumption of the vehicle, by connecting an obd device and soft like *EVNotify* or *Torque Pro*.

     This example should help you to send your live data from OVMS box.

    :Author: **David S Arnold**



.. contents:: 
    :depth: 3


Installation
============

You can use the embedded website *tools/shell* and *tools/editor* of the OVMS box to create the directory and file.

Files to be created in **/store/scripts/**
    * **ovmsmain.js**, if not already exists
    * **sendlivedata2abrp.js**, by copying the file below

This entire document is written with the RST syntax. In the right sidebar, you should find a link **show source**, which shows the RST source code.

Script code
===========
This is a javascript code, to extract in the file *sendlivedata2abrp.js*.


.. warning:: note the minimum firmware version: 3.2.008-147

.. literalinclude:: sendlivedata2abrp.js
    :linenos:
    :language: javascript

Usage
=====
How to make it run:
  - install as described in 'Installation'
  - add to /store/scripts/ovmsmain.js:
                 abrp = require("sendlivedata2arbp");
  - script reload
 
With command lines in the OVMS android app, in the messages part:
  - script eval abrp.info()         => to display vehicle data to be sent to abrp
  - script eval abrp.onetime()      => to launch one time the request to abrp server
  - script eval abrp.send(1)        => toggle send data to abrp
  - script eval abrp.send(0)        => stop sending data

