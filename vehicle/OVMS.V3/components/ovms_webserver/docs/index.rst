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
-  ``curl 'http://192.168.4.1/api/execute?apikey=password&type=js&command=print(Duktape.version)'``


Web UI Development Framework
----------------------------

To simplify and speed up UI and plugin development, you can simply run a local web server from
the ``dev`` directory of the ``ovms_webserver`` component source.

Preparation: create your local git clone if you don't have one already::

    > cd ~
    > git clone https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3.git

Local web server options: `List of static web servers <https://gist.github.com/willurd/5720255>`_

Example::

    > cd ~/Open-Vehicle-Monitoring-System-3/vehicle/OVMS.V3/components/ovms_webserver/dev
    > python3 -m http.server 8000
    Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...

Now open the development framework in your browser at URI ``http://localhost:8000``. If that
doesn't work, check your firewall settings for port 8000 on localhost.

You should see the examples home. Edit ``index.htm`` to add custom menus, edit ``home.htm`` to
add custom home buttons.

Hint: to test your plugin for mobile devices, use the mobile emulation mode of your browser's
development toolkit. Mobile mode allows to test small screen resolutions, rotation and touch
events.

A static local HTTP server allows to use all frontend features, but cannot emulate the backend API
(command/script execution). If using a CGI capable HTTP server, you can also add a proxy handler
for ``/api/execute`` that forwards the requests to your module by HTTP or SSH.

If you want to add your results to a C++ module, use the tools ``mksrc`` and ``mksrcf`` to convert
your HTML files into C++ compatible strings. ``mksrc`` is meant for static strings, ``mksrcf`` for
strings with printf style placeholders.



.. _w3schools.com: https://www.w3schools.com/
.. _Highcharts.com: https://www.highcharts.com/
