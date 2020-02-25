========
Commands
========

-------------------------------
Commands and Expected Responses
-------------------------------

For message types "C" and "c", the following commands and responses are expected to be supported.

------------------------
1 - Request feature list
------------------------

Command parameters are unused and ignored by the car.

Response is a sequence of individual messages with each message containing the following parameters:

* feature number
* maximum number of features
* feature value

Registered features are:

* 0:	Digital SPEEDO (experimental)
* 8:	Location STREAM mode (consumes more bandwidth)
* 9:	Minimum SOC
* 15:	CAN bus can write-enabled

Note that features 0 through 7 are 'volatile' and will be lost (reset to zero value) if the power is lost to the car module, or module is reprogrammed. These features are considered extremely experimental and potentially dangerous.

Features 8 through 15 are 'permanent' and will be stored as parameters 23 through 31. These features are considered more stable, but optional.

---------------
2 - Set feature
---------------

Command parameters are:

* feature number to set
* value to set

Response parameters are unused, and will merely indicate the success or not of the result.

--------------------------
3 - Request parameter list
--------------------------

Command parameters are unused and ignored by the car.

Response is a sequence of individual messages with each message containing the following parameters:

* parameter number
* maximum number of parameters
* parameter value

Registered parameters are:

* 0:	Registered telephone number
* 1:	Registration Password
* 2:	Miles / Kilometer flag
* 3:	Notification method list
* 4:	Server IP
* 5:	GPRS APN
* 6:	GPRS User
* 7:	GPRS Password
* 8:	Vehicle ID
* 9:	Network Password
* 10:	Paranoid Password

Note that some parameters (24 through 31) are tied directly to the features system (for permanent features) and are thus not directly maintained by the parameter system or shown by this command.

-----------------
4 - Set parameter
-----------------

Command parameters are:

* parameter number to set
* value to set

Response parameters are unused, and will merely indicate the success or not of the result.

----------
5 - Reboot
----------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result. Shortly after sending the response, the module will reboot.

----------------
6 - Charge Alert
----------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result. Shortly after sending the response, the module will issue a charge alert.

-----------------------
7 - Execute SMS command
-----------------------

Command parameter is:

* SMS command with parameters

Response is the output of the SMS command that would otherwise have been sent as the reply SMS, with LF characters converted to CR.

The caller id is set to the registered phone number. Return code 1 is used for all errors, i.e. authorization failure, command failure and unknown/unhandled commands.

Note: SMS commands with multiple replies are not yet supported, only the last reply will be returned.

--------------------
10 - Set Charge Mode
--------------------

Command parameters are:

* mode (0=standard, 1=storage,3=range,4=performance)

Response parameters are unused, and will merely indicate the success or not of the result.

-----------------
11 - Start Charge
-----------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result. 

----------------
12 - Stop Charge
----------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result. 

-----------------------
15 - Set Charge Current
-----------------------

Command parameters are:

* current (specified in Amps)

Response parameters are unused, and will merely indicate the success or not of the result.

--------------------------------
16 - Set Charge Mode and Current
--------------------------------

Command parameters are:

* mode (0=standard, 1=storage,3=range,4=performance)
* current (specified in Amps)

Response parameters are unused, and will merely indicate the success or not of the result.

-----------------------------------------
17 - Set Charge Timer Mode and Start Time
-----------------------------------------

Command parameters are:

* timermode (0=plugin, 1=timer)
* start time (0x059F for midnight GMT, 0x003B for 1am GMT, etc)

Response parameters are unused, and will merely indicate the success or not of the result.

---------------
18 - Wakeup car
---------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result.

---------------------------------
19 - Wakeup temperature subsystem
---------------------------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result.

-------------
20 - Lock Car
-------------

Command parameters are:

* pin (the car pin to use for locking)

Response parameters are unused, and will merely indicate the success or not of the result. 

N.B. unlock/lock may not affect the immobilizer+alarm (when fitted)

------------------------
21 - Activate Valet Mode
------------------------

Command parameters are:

* pin (the car pin to activate valet mode)

Response parameters are unused, and will merely indicate the success or not of the result. 

---------------
22 - Unlock Car
---------------

Command parameters are:

* pin (the car pin to use for unlocking)

Response parameters are unused, and will merely indicate the success or not of the result. 

N.B. unlock/lock does not affect the immobilizer+alarm (when fitted)

--------------------------
23 - Deactivate Value Mode
--------------------------

Command parameters are:

* pin (the car pin to use for deactivating value mode)

Response parameters are unused, and will merely indicate the success or not of the result. 

--------------
24 - Home Link
--------------

Command parameters are:

* button (home link button 0, 1 or 2)

Response parameters are unused, and will merely indicate the success or not of the result. 

-------------
25 - Cooldown
-------------

Command parameters are unused and ignored by the car.

Response parameters are unused, and will merely indicate the success or not of the result. 

----------------------------------
30 - Request GPRS utilization data
----------------------------------

Command parameters are unused and ignored by the car.

Response is a sequence of individual messages with each message containing the following parameters:

* record number
* maximum number of records
* date
* car received bytes
* car transmitted bytes
* apps received bytes
* apps transmitted bytes

Note that this request is handled by the server, not the car, so must not be sent in paranoid mode. The response (from the server) will also not be sent in paranoid mode.

N.B. Dates (and GPRS utilization data) are in UTC.

------------------------------------
31 - Request historical data summary
------------------------------------

Command parameters are:

* since (optional timestamp condition)

Response is a sequence of individual messages with each message containing the following parameters:

* type number
* maximum number of types
* type value
* number of unique records (per type)
* total number of records (per type)
* storage usage (in bytes, per type)
* oldest data timestamp (per type)
* newest data timestamp (per type)

N.B. Timestamps are in UTC.

------------------------------------
32 - Request historical data records
------------------------------------

Command parameters are:

* type (the record type to retrieve)
* since (optional timestamp condition)

Response is a sequence of individual messages with each message containing the following parameters:

* response record number
* maximum number of response records
* data record type
* data record timestamp
* data record number
* data record value

-------------
40 - Send SMS
-------------

Command parameters are:

* number (telephone number to send sms to)
* message (sms message to be sent)

Response parameters are unused, and will merely indicate the success or not of the submission (not delivery) of the SMS. 

------------------------
41 - Send MMI/USSD Codes
------------------------

Command parameters are:

* USSD_CODE (the ussd code to send)

Response parameters are unused, and will merely indicate the success or not of the submission (not delivery) of the request.

------------------------
49 - Send raw AT Command
------------------------

Command parameters are:

* at (the AT command to send - including the AT prefix)

Response parameters are unused, and will merely indicate the success or not of the submission (not delivery) of the request.
