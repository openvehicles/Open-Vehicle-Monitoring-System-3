=======
Plugins
=======

The OVMS can easily be extended by plugins. Plugins may consist of any combination of module
scripts (Javascript) and web UI extensions (HTML).

Installation currently needs to be done manually, but is simple. See below and the plugin
documentation on specific steps and on how to use the plugin functions. A plugin store with
simplified download and optional automatic updating will be provided in the future.

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


.. _installing-module-plugins:

-------------------------
Installing Module Plugins
-------------------------

A module plugin normally consists of a single Javascript file that needs to be placed
in the ``/store/scripts/lib/`` directory. The plugin is then loaded by a ``require()`` call,
which can be done manually as needed or automatically on boot by adding it to the
``/store/scripts/ovmsmain.js`` file.

1. Menu **Tools → Editor**
2. Cancel the open dialog
3. Paste the plugin source into the editor
4. :guilabel:`Save as…` → ``/store/scripts/lib/…`` using the name as shown in the plugin documentation
5. :guilabel:`Open` → :guilabel:`↰` (one level up) → ``ovmsmain.js``
6. Add the ``require()`` call as shown in the plugin documentation
7. :guilabel:`Save`
8. :guilabel:`Reload JS Engine`

The plugin is now installed and activated.

**Hint**: you can now try out the commands provided by the plugin directly from the editor.
Clear the editor, fill in the command or Javascript snippet, click :guilabel:`E&valuate JS`.
The output will be shown below the input field.

To deactivate a plugin, comment out the ``require()`` call by prefixing the line with ``//``
and do another JS engine reload. To remove a plugin, remove the ``require()`` call and delete
the file using ``vfs rm /store/scripts/lib/…``.


.. _installing-web-plugins:

----------------------
Installing Web Plugins
----------------------

A web plugin normally consists of a single HTML file. Web plugins can be meant to render
new pages or to hook into an existing page. They need an initial registration to work:

1. Menu **Config → Web Plugins**
2. :guilabel:`✚` (add)
3. Set type and name as suggested in the plugin documentation
4. :guilabel:`Save`
5. :guilabel:`Edit`
6. Set attributes as suggested in the plugin documentation
7. Paste the plugin source into the content area
8. :guilabel:`Save`

The plugin is now installed and activated.

**Hint**: you can use the text editor (tools menu) to change or update an already installed
web plugin. Simply edit the plugin file in folder ``/store/plugin/`` directly, the system will
reload the plugin as soon as you save it.

To deactivate a web plugin, set the state to "off". To remove a plugin, click :guilabel:`✖`
→ :guilabel:`Save`.

