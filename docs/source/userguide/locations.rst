===================
Geofenced Locations
===================

Where GPS location is available, OVMS has the ability to respond to the current GPS location.  This falls into two categories: the system automatic locations
(last parked and last valet locations), and the user-defined locations.

The system locations have system defined responses where the user-defined locations can have user responses associated with them.

System Locations
----------------

Last Parked Location
~~~~~~~~~~~~~~~~~~~~

When the car is turned off (metric *v.e.on* set to *no*), the last-parked position is updated as soon as it is available.
If the car is then subsequently moved without being turned on, this triggers a *flatbed* warning - ie the car is moving
without having been turned on and might be on a flatbed.
The distance threshold for the warning can be configured and defaults to 500m.  The distance in metres be set like this::

  OVMS# config set vehicle flatbed.alarmdistance 500

This alarm will repeat periodically (default 15 mins).  Change how often the alarm repeats like this::

  OVMS# config set vehicle flatbed.alarminterval 10

The alert *flatbed.moved* is sent, along with the event *location.alert.flatbed.moved*.

Valet Start Location
~~~~~~~~~~~~~~~~~~~~

When the car valet is turned on (metric *v.e.valet* set to *yes*), the valet position is updated as soon it is available. Similarly to above, this can trigger a *valet* warning,
ie the car has been moved more than allowed for a valet situation.  The default is to have it disabled (distance is 0). The distance in metres can be similarly set like this:

  OVMS# config set vehicle valet.alarmdistance 1000

This alarm will repeat periodically (default 15 mins).  Change how often the alarm repeats like this::

  OVMS# config set vehicle valet.alarminterval 10

The alert *valet.bounds* is sent along with the event *location.alert.valet.bounds*.

User Defined Locations
----------------------

A user-defined location is defined by a GPS coordinate and a radius (in 'small' distance units).  A location is defined as active if the current GPS location is within the
defined radius.  Multiple locations can be concurrently active.

The locations can be defined from the web interface or from the CLI.  The units used for the radius are defined by the Vehicle unit 'Height' setting - otherwise as
known as the small distance unit.

You can quickly define a vehicle *home* location as follows using the current location (see the help for more detail)::

  OVMS# location set home

You could then update the radius to 80 meters as follows::

  OVMS# location radius home 80 meters

When the car enters the above location, you will get an event "location.enter.home", and when it leaves, you will get the event "location.leave.home".
See :doc:`events` for how to add scripts.
There are also limited actions you can define. For help on how see::

  OVMS# location action ?

