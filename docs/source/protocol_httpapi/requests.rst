========
Requests
========

-------------------
GET     /api/cookie
-------------------

Login and return a session cookie.

URL parameters:

* ``username``: (mandatory) your server account login
* ``password``: (mandatory) your server account password

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

Return tpms status: the data available depends on the vehicle and module type.

''''''''''''''''''''''''''''''''''
V2 modules / old firmware versions
''''''''''''''''''''''''''''''''''

On V2 modules, the wheel layout is fixed to two front & two rear wheels, and sensor
data is fixed to pressure and temperature:

* ``fr_pressure``: front right pressure (PSI)
* ``fr_temperature``: front right temperature (Celcius)
* ``rr_pressure``: rear right
* ``rr_temperature``
* ``fl_pressure``: front left
* ``fl_temperature``
* ``rl_pressure``: rear left
* ``rl_temperature``
* ``staletpms``: overall value staleness, -1=undefined, 0=stale, 1=valid
* ``m_msgage_w``: age (seconds) of last TPMS (W) message received if available
* ``m_msgtime_w``: time stamp (UTC) of last TPMS (W) message received if available

''''''''''
V3 modules
''''''''''

V3 modules support any kind of wheel/nonwheel layout as well as two new sensor types
(health & alert level). The actual wheel naming and value availability depends on the
vehicle type. Default wheel layout/naming is the same as with V2, i.e. "fl", "fr",
"rl" & "rr". For the wheel layout of other vehicles, see the respective vehicle manual
pages.

For each wheel, sensor values pressure, temperature, health and alert status **may**
be available, i.e. all wheel fields are optional:

* ``<wheelname>_pressure_kpa``: wheel pressure in kPa
* ``<wheelname>_pressure``: wheel pressure in PSI
* ``<wheelname>_temperature``: wheel temperature in Celcius
* ``<wheelname>_health``: wheel health in percent
* ``<wheelname>_alert``: wheel alert level, 0=none, 1=warning, 2=alert

Staleness is available per sensor type & overall, with -1=undefined, 0=stale, 1=valid:

* ``stale_pressure``
* ``stale_temperature``
* ``stale_health``
* ``stale_alert``
* ``staletpms`` (maximum value of the above)

Metadata:

* ``m_msgage_y``: age (seconds) of last TPMS (Y) message received if available
* ``m_msgtime_y``: time stamp (UTC) of last TPMS (Y) message received if available
* ``m_msgage_w``: compatibility copy of ``m_msgage_y``
* ``m_msgtime_w``: compatibility copy of ``m_msgtime_y``



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
* ``batt_current``
* ``batt_range_speed``
* ``charge_kwh_grid``
* ``charge_kwh_grid_total``
* ``batt_capacity``
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

* ``h_recordtype``: record type, e.g. ``*-LOG-Notification``
* ``distinctrecs``: number of distinct record numbers (``h_recordnumber``) stored
* ``totalrecs``: total number of records stored
* ``totalsize``: total transfer data volume (bytes) of all records
* ``first``: ISO timestamp of earliest record
* ``last``: ISO timestamp of latest record

URL parameters:

* ``since``: (optional) limit output to records after the date/time specified (ISO format)

--------------------------------------------
GET /api/historical/<VEHICLEID>/<RECORDTYPE>
--------------------------------------------

Request historical data records (as array of):

* ``h_timestamp``: ISO date/time of the record
* ``h_recordnumber``: record type/application specific "record number" (arbitrary identifier)
* ``h_data``: record type/application specific payload

The records are ordered by ``h_timestamp`` (primary) and ``h_recordnumber`` (secondary), both in ascending order.

``h_data`` may contain any kind of data. OVMS standard record types normally use a size
optimized CSV (comma separated values) format, with fields as specified/documented
for the record type. Another option can be JSON, with the advantage of field names being
included, at the expense of additional data volume overhead.

URL parameters:

* ``since``: (optional) limit output to records after the date/time specified (ISO format)

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

