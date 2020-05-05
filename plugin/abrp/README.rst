*****************************
ABRP: abetterrouteplanner.com
*****************************

**Send live data to abetterrouteplanner.com**

.. topic:: Overview

    `abetterrouteplanner <http://abetterrouteplanner.com>`_  is probably the best website to plan a route with an EV and optimize the number of stops needed and where to charge your vehicle.

    Moreover, it has a wonderful functionality, to update your plan, taking into account your live data, to adjust with the real consumption of the vehicle, by connecting an obd device and soft like *EVNotify* or *Torque Pro*.

     This example should help you to send your live data from OVMS box.

    :Author: **David S Arnold**



.. contents::
    :depth: 3


Installation
============

You can use the embedded website *tools/shell* and *tools/editor* of the OVMS box to create the directory and file. This editor will allow you to paste the plugin code from the documentation, and also to configure your own token and car model (see Remark).

Files to be created in **/store/scripts/**
    * **ovmsmain.js**, if not already exists
    * **sendlivedata2abrp.js**, by copying the file below and setting your own 'user token'

**Remark**
to get your own 'user token', you must connect with your login to `abetterrouteplanner in classic view <https://abetterrouteplanner.com/classic>`_. In Settings/more Settings, there's an item 'Live Car Connection' with 2 buttons: 'Setup' and 'View live data'.
    * Click on Setup
    * Click on Torque
    * Click 4 times on 'Next', until having two fields to copy: 'Webserver URL' and 'User Email Address'.
    * Copy the field 'User Email Address' and paste it in the source code const MY_TOKEN = ">>> PASTE HERE <<<";

**New in version 1.2:**
Your own parameters (car model, abrp token but also api-url) are now stored as config parameters, you can consult them using the shell with ``config list usr`` or modify with ``config set`` command.


Finally reboot your OVMS module. This was a one-time configuration.
You’re now ready. Test it with the shell page in the embedded web server using the command ``script eval abrp.info()`` and then with the command ``script eval abrp.onetime()``. You can also do it with the mobile app.

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
                 ``abrp = require("sendlivedata2abrp");``
  - script reload

With command lines in the OVMS android app, in the messages part:
  - ``script eval abrp.info()``         => to display vehicle data to be sent to abrp
  - ``script eval abrp.onetime()``      => to launch one time the request to abrp server
  - ``script eval abrp.send(1)``        => toggle send data to abrp
  - ``script eval abrp.send(0)``        => stop sending data
