*****************************
ABRP: abetterrouteplanner.com
*****************************

**Send live data to abetterrouteplanner.com**

.. topic:: Overview

  `abetterrouteplanner <http://abetterrouteplanner.com>`_  is probably the best website to plan a route with an EV and optimize the number of stops needed and where to charge your vehicle.

  Moreover, it has a wonderful functionality, to update your plan, taking into account your live data, to adjust with the real consumption of the vehicle, by connecting an obd device and soft like *EVNotify* or *Torque Pro*.

  This plugin sends your live data from the OVMS box.

  :Author: **David S Arnold**

  **New in version 1.2:** Your own parameters (car model, abrp token but also api-url) are now stored as config parameters, you can consult them using the shell with ``config list usr`` or modify with ``config set`` command.



.. contents::
    :depth: 3


Installation
============

You can use the embedded website *tools/shell* and *tools/editor* of the OVMS box to create the directory and file. This editor will allow you to paste the plugin code from the documentation.

Files to be created in **/store/scripts/**:
  * **ovmsmain.js**, if not already exists
  * **sendlivedata2abrp.js**, by copying the file below

Next, configure your car model, API URL & token:

.. note:: ABRP only supports live data feeds for some car models. You can request a notification when
  support has been added for your car. **Do not feed live data using other car models!** Also, ABRP
  has not yet added this OVMS plugin as a general data source, so you need to disguise as "Torque".

If using the **ABRP 4 UI**:
  * Login to `abetterrouteplanner`_
  * Open "Settings", enable detailed setup
  * Add your car (if ABRP has no live support for your car model, it will display "Live data not available")
  * Click "Link Torque", click "Next" 3 times
  * Set the "Webserver URL" by ``config set usr abrp.url "…"``
    * Note: according to Iternio, the URL ``http://api.iternio.com/1/tlm/send`` should generally be used here instead of the Torque specific one displayed
  * Set the "User Email Address" (API token) by ``config set usr abrp.user_token "…"``

If using **ABRP classic UI**:
  * Login to `abetterrouteplanner in classic view <https://abetterrouteplanner.com/classic>`_
  * In Settings/more Settings, there's an item 'Live Car Connection' with 2 buttons: 'Setup' and 'View live data'
  * Click on Setup (if there is no Setup button, live data support isn't available for your car model)
  * Click on Torque
  * Set the "Webserver URL" by ``config set usr abrp.url "…"``
  * Set the "User Email Address" (API token) by ``config set usr abrp.user_token "…"``

**Determine your car model code**: 
  * Open `API car models list <https://api.iternio.com/1/tlm/get_carmodels_list?api_key=32b2162f-9599-4647-8139-66e9f9528370>`_
  * To improve readability, optionally paste the page into `JSONlint.com <https://jsonlint.com/>`_
  * Search for your car brand and model, the code is the field following the car specification, for example ``renault:zoe:20:52:r110``
  * Set the code by ``config set usr abrp.car_model "…"``

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

With command lines in the OVMS android or iOS app, in the messages part:
  - ``script eval abrp.info()``         => to display vehicle data to be sent to abrp
  - ``script eval abrp.onetime()``      => to launch one time the request to abrp server
  - ``script eval abrp.send(1)``        => toggle send data to abrp
  - ``script eval abrp.send(0)``        => stop sending data
  - ``script eval abrp.resetConfig()``  => reset configuration to defaults

Also in the messages part, configuration can be updated with:
  - ``config set usr abrp.car_model <value>``
  - ``config set usr abrp.url <value>``
  - ``config set usr abrp.user_token <value>``
