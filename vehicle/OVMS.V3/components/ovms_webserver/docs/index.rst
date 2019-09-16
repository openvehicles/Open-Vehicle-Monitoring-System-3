Web Framework & Plugins
=======================

TL;DR: Examples
---------------

The following examples include their documentation in the HTML page and source.
Read the source and install them as plugins (see below) to see how they work.

.. toctree::
   :maxdepth: 1
   :caption: Basic usage
   
   metrics
   commands
   notifications
   hooks
   solidgauge

.. toctree::
   :maxdepth: 1
   :caption: API usage
   
   loadcmd

.. toctree::
   :maxdepth: 1
   :caption: UI elements
   
   btn-longtouch
   dialogtest
   filedialog
   filebrowser
   input-slider

.. toctree::
   :maxdepth: 1
   :caption: Plugin examples
   
   regenmon
   dashboard
   plugin-twizy/dashboard-tuneslider
   plugin-twizy/profile-editor
   plugin-twizy/drivemode-config


Installing Plugins
------------------

The framework supports installing user content as pages or extensions to
pages. To install an example:

1. Menu Config → Web Plugins
2. Add plugin: type "Page", name e.g. "dev.metrics" (used as the file
   name in ``/store/plugin``)
3. Save → Edit
4. Set page to e.g. "/usr/dev/metrics", label to e.g. "Dev: Metrics"
5. Set the menu to e.g. "Tools" and auth to e.g. "None"
6. Paste the page source into the content area
7. Save → the page is now accessible in the "Tools" menu.

Hint: use the standard editor (tools menu) in a second session to edit a
plugin repeatedly during test / development.


Basics
------

The OVMS web framework is based on HTML 5, Bootstrap 3 and jQuery 3.
Plenty documentation and guides on these basic web technologies is
available on the web, a very good resource is `w3schools.com`_.

For charts the framework includes Highcharts 6. Info and documentation
on this is available on `Highcharts.com`_.

The framework is "AJAX" based. The index page ``/`` loads the framework
assets and defines a default container structure including a ``#menu``
and a ``#main`` container. Content pages are loaded into the ``#main``
container. The window URL includes the page URL after the hash mark
``#``:

-  ``http://ovms.local/#/status`` – this loads page ``/status`` into
   ``#main``
-  ``http://ovms.local/#/dashboard?nm=1`` – this loads the dashboard and
   activates night mode

Links and forms having id targets ``#…`` are automatically converted to
AJAX by the framework:

-  ``<a href="/edit?path=/sd/index.txt" target="#main">Edit index.txt</a>``
   – load the editor

Pages can be loaded outside the framework as well (e.g.
``http://ovms.local/status``). See index source on framework scripts and
styles to include if you'd like to design standalone pages using the
framework methods.

If file system access is enabled, all URLs not handled by the system or
a user plugin (see below) are mapped onto the file system under the
configured web root. Of course, files can be loaded into the framework
as well. For example, if the web root is ``/sd`` (default):

-  ``http://ovms.local/#/mypage.htm`` – load file ``/sd/mypage.htm``
   into ``#main``
-  ``http://test1.local/#/logs`` – load directory listing ``/sd/logs``
   into ``#main``

**Important Note**: the framework has a global shared context (i.e. the
``window`` object). To avoid polluting the global context with local
variables, good practice is to wrap your local scripts into closures.
Pattern:

::

   <script>
   (function(){
     … insert your code here …
   })();
   </script>


Authorization
-------------

Page access can be restricted to authorized users either session based
or per access. File access can be restricted using digest
authentication.

The module password is used for all authorizations. A user account or
API key administration is not yet included, the main username is
``admin``.

To create a session, call the ``/login`` page and store the resulting
cookie:

1. ``curl -c auth -b auth 'http://192.168.4.1/login' -d username=admin -d password=…``
2. ``curl -c auth -b auth 'http://192.168.4.1/api/execute?command=xrt+cfg+info'``

To issue a single call, e.g. to execute a command from a Wifi button,
supply the password as ``apikey``:

-  ``curl 'http://192.168.4.1/api/execute?apikey=password&command=xrt+cfg+info'``


.. _w3schools.com: https://www.w3schools.com/
.. _Highcharts.com: https://www.highcharts.com/
