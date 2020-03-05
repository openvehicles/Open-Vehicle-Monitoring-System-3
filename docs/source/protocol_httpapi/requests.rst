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

* id Vehicle ID
* v_net_connected Number of vehicles currently connected
* v_apps_connected Number of apps currently connected
* v_btcs_connected Number of batch clients currently connected

-----------------------------
GET /api/protocol/<VEHICLEID>
-----------------------------

Return raw protocol records (no vehicle connection):

* m_msgtime Date/time message received
* m_paranoid Paranoid mode flag
* m_ptoken Paranoid mode token
* m_code Message code
* m_msg Message body

---------------------------
GET /api/status/<VEHICLEID>
---------------------------

Return vehicle status:

* soc
* units
* idealrange
* idealrange_max
* estimatedrange
* mode
* chargestate
* cac100
* soh
* cooldown_active
* fl_dooropen
* fr_dooropen
* cp_dooropen
* pilotpresent
* charging
* caron
* carlocked
* valetmode
* bt_open
* tr_open
* temperature_pem
* temperature_motor
* temperature_battery
* temperature_charger
* tripmeter
* odometer
* speed
* parkingtimer
* temperature_ambient
* carawake
* staletemps
* staleambient
* charging_12v
* vehicle12v
* vehicle12v_ref
* vehicle12v_current
* alarmsounding

-------------------------
GET /api/tpms/<VEHICLEID>
-------------------------

Return tpms status:

* fr_pressure
* fr_temperature
* rr_pressure
* rr_temperature
* fl_pressure
* fl_temperature
* rl_pressure
* rl_temperature
* staletpms

-----------------------------
GET /api/location/<VEHICLEID>
-----------------------------

Return vehicle location:

* latitude
* longitude
* direction
* altitude
* gpslock
* stalegps
* speed
* tripmeter
* drivemode
* power
* energyused
* energyrecd

---------------------------
GET /api/charge/<VEHICLEID>
---------------------------

Return vehicle charge status:

* linevoltage
* battvoltage
* chargecurrent
* chargepower
* chargetype
* chargestate
* soc
* units
* idealrange
* estimatedrange
* mode
* chargelimit
* chargeduration
* chargeb4
* chargekwh
* chargesubstate
* chargetimermode
* chargestarttime
* chargetimerstale
* cac100
* soh
* charge_etr_full
* charge_etr_limit
* charge_limit_range
* charge_limit_soc
* cooldown_active
* cooldown_tbattery
* cooldown_timelimit
* charge_estimate
* charge_etr_range
* charge_etr_soc
* idealrange_max
* cp_dooropen
* pilotpresent
* charging
* caron
* temperature_pem
* temperature_motor
* temperature_battery
* temperature_charger
* temperature_ambient
* carawake
* staletemps
* staleambient
* charging_12v
* vehicle12v
* vehicle12v_ref
* vehicle12v_current

-------------------------------
GET /api/historical/<VEHICLEID>
-------------------------------

Request historical data summary (as array of):

* h_recordtype
* distinctrecs
* totalrecs
* totalsize
* first
* last

------------------------------------------
GET /api/historical/<VEHICLEID>/<DATATYPE>
------------------------------------------

Request historical data records:

* h_timestamp
* h_recordnumber
* h_data

-------------------
Not Yet Implemented
-------------------

* GET /api/vehicle/<VEHICLEID>  Connect to, and return vehicle information
* DELETE /api/vehicle/<VEHICLEID>  Disconnect from vehicle
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

