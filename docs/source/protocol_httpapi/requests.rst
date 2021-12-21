========
Requests
========

-------------------
GET     /api/cookie
-------------------

Login and return a session cookie.

-------------------
DELETE  /api/cookie
-------------------

Delete the session cookie and logout.

All vehicles still connected (via ``/api/vehicle/…``) will be disconnected automatically.

------------------
GET     /api/token
------------------

Return a list of registered API tokens.

------------------
POST    /api/token
------------------

Create an API token.

--------------------------
DELETE  /api/token/<TOKEN>
--------------------------

Delete the specified API token.

---------------------
GET     /api/vehicles
---------------------

Return a list of registered vehicles:

* ``id``: Vehicle ID
* ``v_net_connected``: Number of vehicles currently connected
* ``v_apps_connected``: Number of apps currently connected
* ``v_btcs_connected``: Number of batch clients currently connected

----------------------------
GET /api/vehicle/<VEHICLEID>
----------------------------

Connect to the vehicle and return vehicle information:

* ``m_msgage_s``: age (seconds) of last status (S) message received if available
* ``m_msgtime_s``: time stamp (UTC) of last status (S) message received if available
* ``v_apps_connected``: number of apps currently connected (including the API connection)
* ``v_btcs_connected``: number of batch clients currently connected
* ``v_first_peer``: 0/1, 1 = the API connection is the first peer connecting to the car
* ``v_net_connected``: 0/1, 1 = the car is currently connected to the server

While no application is connected to the vehicle, the module will send status updates using
the configured "idle" interval to reduce data volume and energy consumption.

Connecting to the car tells the module (if connected) to send status updates in the normally
shorter "connected" intervals (as configured). If you're the first peer and the car is online,
the module will send a full status update within the next second after receiving the new peer
count. Depending on the connection speed you should give the car module some seconds to transmit
the first update before proceeding to other status queries.

You can also check ``m_msgage_s`` and ``m_msgtime_s`` to see if the status record is up to date
(yet). These fields are also included in the other status responses e.g. ``/api/charge/<VEHICLEID>``.

-------------------------------
DELETE /api/vehicle/<VEHICLEID>
-------------------------------

Disconnect from the vehicle.

This is optional, on session logout or expiry, all vehicles will be disconnected automatically.

-----------------------------
GET /api/protocol/<VEHICLEID>
-----------------------------

Return raw protocol records (no vehicle connection):

* ``m_msgtime``: Date/time message received
* ``m_paranoid``: Paranoid mode flag
* ``m_ptoken``: Paranoid mode token
* ``m_code``: Message code
* ``m_msg``: Message body

---------------------------
GET /api/status/<VEHICLEID>
---------------------------

Return vehicle status:

* ``soc``
* ``units``
* ``idealrange``
* ``idealrange_max``
* ``estimatedrange``
* ``mode``
* ``chargestate``
* ``cac100``
* ``soh``
* ``cooldown_active``
* ``fl_dooropen``
* ``fr_dooropen``
* ``cp_dooropen``
* ``pilotpresent``
* ``charging``
* ``caron``
* ``carlocked``
* ``valetmode``
* ``bt_open``
* ``tr_open``
* ``temperature_pem``
* ``temperature_motor``
* ``temperature_battery``
* ``temperature_charger``
* ``temperature_cabin``
* ``tripmeter``
* ``odometer``
* ``speed``
* ``parkingtimer``
* ``temperature_ambient``
* ``carawake``
* ``staletemps``
* ``staleambient``
* ``charging_12v``
* ``vehicle12v``
* ``vehicle12v_ref``
* ``vehicle12v_current``
* ``alarmsounding``
* ``m_msgage_s``: age (seconds) of last status (S) message received if available
* ``m_msgtime_s``: time stamp (UTC) of last status (S) message received if available
* ``m_msgage_d``: age (seconds) of last doors/env (D) message received if available
* ``m_msgtime_d``: time stamp (UTC) of last doors/env (D) message received if available

-------------------------
GET /api/tpms/<VEHICLEID>
-------------------------

Return tpms status:

* ``fr_pressure``
* ``fr_temperature``
* ``rr_pressure``
* ``rr_temperature``
* ``fl_pressure``
* ``fl_temperature``
* ``rl_pressure``
* ``rl_temperature``
* ``staletpms``
* ``m_msgage_w``: age (seconds) of last TPMS (W) message received if available
* ``m_msgtime_w``: time stamp (UTC) of last TPMS (W) message received if available

-----------------------------
GET /api/location/<VEHICLEID>
-----------------------------

Return vehicle location:

* ``latitude``
* ``longitude``
* ``direction``
* ``altitude``
* ``gpslock``
* ``stalegps``
* ``speed``
* ``tripmeter``
* ``drivemode``
* ``power``
* ``energyused``
* ``energyrecd``
* ``m_msgage_l``: age (seconds) of last location (L) message received if available
* ``m_msgtime_l``: time stamp (UTC) of last location (L) message received if available

---------------------------
GET /api/charge/<VEHICLEID>
---------------------------

Return vehicle charge status:

* ``linevoltage``
* ``battvoltage``
* ``chargecurrent``
* ``chargepower``
* ``chargetype``
* ``chargestate``
* ``soc``
* ``units``
* ``idealrange``
* ``estimatedrange``
* ``mode``
* ``chargelimit``
* ``chargeduration``
* ``chargeb4``
* ``chargekwh``
* ``chargesubstate``
* ``chargetimermode``
* ``chargestarttime``
* ``chargetimerstale``
* ``cac100``
* ``soh``
* ``charge_etr_full``
* ``charge_etr_limit``
* ``charge_limit_range``
* ``charge_limit_soc``
* ``cooldown_active``
* ``cooldown_tbattery``
* ``cooldown_timelimit``
* ``charge_estimate``
* ``charge_etr_range``
* ``charge_etr_soc``
* ``idealrange_max``
* ``cp_dooropen``
* ``pilotpresent``
* ``charging``
* ``caron``
* ``temperature_pem``
* ``temperature_motor``
* ``temperature_battery``
* ``temperature_charger``
* ``temperature_ambient``
* ``temperature_cabin``
* ``carawake``
* ``staletemps``
* ``staleambient``
* ``charging_12v``
* ``vehicle12v``
* ``vehicle12v_ref``
* ``vehicle12v_current``
* ``m_msgage_s``: age (seconds) of last status (S) message received if available
* ``m_msgtime_s``: time stamp (UTC) of last status (S) message received if available
* ``m_msgage_d``: age (seconds) of last doors/env (D) message received if available
* ``m_msgtime_d``: time stamp (UTC) of last doors/env (D) message received if available

-------------------------------
GET /api/historical/<VEHICLEID>
-------------------------------

Request historical data summary (as array of):

* ``h_recordtype``
* ``distinctrecs``
* ``totalrecs``
* ``totalsize``
* ``first``
* ``last``

------------------------------------------
GET /api/historical/<VEHICLEID>/<DATATYPE>
------------------------------------------

Request historical data records:

* ``h_timestamp``
* ``h_recordnumber``
* ``h_data``

-------------------
Not Yet Implemented
-------------------

* PUT /api/charge/<VEHICLEID>   Set vehicle charge status
* DELETE /api/charge/<VEHICLEID>   Abort a vehicle charge
* GET /api/lock/<VEHICLEID>   Return vehicle lock status
* PUT /api/lock/<VEHICLEID>   Lock a vehicle
* DELETE /api/lock/<VEHICLEID>   Unlock a vehicle
* GET /api/valet/<VEHICLEID>   Return valet status
* PUT /api/valet/<VEHICLEID>   Enable valet mode
* DELETE /api/valet/<VEHICLEID>   Disable valet mode
* GET /api/features/<VEHICLEID>  Return vehicle features
* PUT /api/feature/<VEHICLEID>  Set a vehicle feature
* GET /api/parameters/<VEHICLEID>  Return vehicle parameters
* PUT /api/parameter/<VEHICLEID>  Set a vehicle parameter
* PUT /api/reset/<VEHICLEID>   Reset the module in a particular vehicle
* PUT /api/homelink/<VEHICLEID>  Activate home link

