===============
Metric Displays
===============

OVMS V3 is based on metrics. Metrics can be single numerical or textual values or complex values
like sets and arrays. The web framework keeps all metrics in a global object, which can be read
simply by e.g. ``metrics["v.b.soc"]``.

In addition to the raw metric values, there are 2 main proxy arrays that give access to the
user-configured versions of the raw metric values. The  ``metrics_user`` array
converts the 'metrics' value to user-configured value and the ``metrics_label`` array
provides the corresponding label for that metric.
So for example the user could configure distance values to be in miles, and in this case
``metrics["v.p.odometer"]`` would still contain the value in km (the default) but
``metrics_user["v.p.odometer"]`` would give the value converted to miles and
``metrics_label["v.p.odometer"]`` would return "M".

The user conversion information is contained in another object ``units``.  ``units.metrics``
has the user configuration for each metric and ``units.prefs`` has the user configuration
for each group of metrics (distance, temperature, consumption, pressure etc). There also some methods
for general conversions allowing user preferences:

- The method ``units.unitLabelToUser(unitType,name)`` will return the user
  defined label for that 'unitType', defaulting to ``name``.
- The method ``units.unitValueToUser(unitType,value)`` will convert ``value``
  to the user defined unit (if set) for the group.

Metrics updates (as well as other updates) are sent to all DOM elements having the
``receiver`` class. To hook into these updates, simply add an event listener for
``msg:metrics:``. The event ``msg:units:metrics`` is called when ``units.metrics`` is change
and ``msg:units:prefs`` when ``units.prefs`` are changed.

Listening to the event is not necessary though if all you need is some metrics
display. This is covered by the ``metric`` widget class family as shown here.


----------------------
Single Values & Charts
----------------------

.. image:: metrics.png
   :scale: 50%
   :align: right

The following example covers…

- Text (String) displays
- Number displays
- Progress bars (horizontal light weight bar charts)
- Gauges
- Charts

Where a number element of class 'metric' contains both elements of class
'value' and 'unit', these will be automatically displayed in the units selected
in the user preferences. Having a 'data-user' attribute will also cause the
'value' element to be displayed in user units (unless 'data-scale' attribute is present).

Gauges & charts use the HighCharts library, which is included in the web server. The other widgets
are simple standard Bootstrap widgets extended by an automatic metrics value update mechanism.

Highcharts is a highly versatile charting system. For inspiration, have a look at:

- https://www.highcharts.com/demo
- https://www.highcharts.com/docs

We're using `styled mode <https://www.highcharts.com/docs/chart-design-and-style/style-by-css>`_
so some options don't apply, but everything can be styled by standard CSS.

Install the example as a web page plugin:

:download:`metrics.htm <../dev/metrics.htm>` (hint: right click, save as)

.. literalinclude:: ../dev/metrics.htm
   :language: html
   :linenos:


-------------
Vector Tables
-------------

.. image:: metrics-table.png
   :scale: 50%
   :align: right

Some metrics, for example the battery cell voltages or the TPMS tyre health data, may contain 
vectors of arbitrary size. Besides rendering into charts, these can also be displayed by their 
textual values in form of a table.

The following example shows a live view of the battery cell voltages along with their recorded 
minimum, maximum, maximum deviation and current warning/alert state. Alert states 0-2 are translated 
into icons.

The metric table widget uses the DataTables library, which is included in the web server. The 
DataTables Javascript library offers a wide range of options to create tabular views into 
datasets.

Install the example as a web page plugin:

:download:`metrics-table.htm <../dev/metrics-table.htm>` (hint: right click, save as)

.. literalinclude:: ../dev/metrics-table.htm
   :language: html
   :linenos:
