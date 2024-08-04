ISOTP Poller API
================

The Poller runs a single command thread (replacing the Vehicle RX thread) that
handles transmitting of commands, receiving of responses and various control
commands. This accessed through the global ``MyPollers`` instance.

Within ``MyPollers`` is a ``OvmsPoller`` instance for each active Can Bus
containing the ISOTP/VWTP protocol state as well as a named list of
``PollSeriesEntry`` instances that provide which command to send.  They are
each dispatched to from the single command thread but it means that concurrent
multi-frame queries can happen across different buses.

A separate timer now means that the frequency of the poll can be adjusted from 
the default 1s provided by the timer.1 event.

There is also the ability to set up a number Secondary ticks for each Primary
tick. This means that failed commands can be retried more quickly and also that
particular types of ``PollSeriesEntry`` can repeat commands (like fetching
speed) without holding up the main Polling tick that provides periodic
commands.

Another feature that has been added to reduce latency is that when the response
time between multiple frames is decreased via ``PollSetTimeBetweenSuccess``, a
delay can be added between poller runs that gives more air-time to other
packets on the Bus with ``PollSetResponseSeparationTime``.

``PollSeriesEntry`` that are added with a "!v." prefix will be automatically removed
on shutdown of the vehicle class.

Legacy Support
--------------

A single special ``PollSeriesEntry`` class called ``StandardVehiclePollSeries``
is used to provide the ``PollSetPidList`` functionality.

The blocking poll requests are also implemented using a different internal
``BlockingOnceOffPoll`` class.

Additional Features
-------------------

The idea is that multiple ``PollSeriesEntry`` classes can be assigned to each
BUS with one-shot classes taking priority.

One-shot classes mean that you can easily perform once-off operations without
blocking the main timer thread.  They automatically remove themselves from the
list once they are successful.
The Ioniq 5 implementation has an example of this where the VIN is retrieved.

Another feature is that the overall 'State' is not limited to 4.  Each series
entry can nominate the block of 4 states that they occupy and states outside
that won't apply to it.

