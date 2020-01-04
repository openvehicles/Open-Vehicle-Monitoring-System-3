=======
Plugins
=======

The OVMS can easily be extended by plugins. Plugins may consist of any combination of module
scripts (Javascript) and web UI extensions (HTML).

Installation currently needs to be done manually, but is simple. See the plugin documentation
on how to do this and on how to use the plugin functions. A plugin store with simplified
download and optional automatic updating will be provided in the future.

This page is intended as an overview of all plugins currently available in the OVMS repository.
If you're a plugin developer and want to add your plugin here, please submit a pull request
containing your plugin directory. Organize your files and include a ``README.rst`` as shown here.


.. toctree::
   :caption: General Plugins
   :maxdepth: 1
   :titlesonly:
   :glob:
   
   [!v][!-]*/README


.. toctree::
   :caption: Vehicle Plugins
   :maxdepth: 2
   :titlesonly:
   :glob:
   
   v-*/README
