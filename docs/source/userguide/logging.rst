=======
Logging
=======

----------------------
Logging to the console
----------------------

Components of the OVMS system output diagnostic logs (information, warnings, etc). You can choose to display these logs on your connected console with the ‘log monitor yes/no’ command::

  OVMS# log monitor ?
  Usage: log monitor [no|yes]
  no                   Don't monitor log
  yes                  Monitor log

By default, the async (USB) console will have log monitoring ‘yes’, but the others ‘no’. Note: the web shell does not support log monitoring yet.

Logs are output at various levels of verbosity, and you can control what is shown both globally and on a per-component basis::

  OVMS# log level ?
  debug                Log at the DEBUG level (4)
  error                Log at the ERROR level (1)
  info                 Log at the INFO level (3)
  none                 No logging (0)
  verbose              Log at the VERBOSE level (5)
  warn                 Log at the WARN level (2)

The syntax of this command is *log level <level> [<component>]*. If the component is not specified, it applies to all components that haven’t had a level set explicitly. The levels increase in verbosity, and setting a particular level will also include all log output at a lower level of verbosity (so, for example, setting level *info* will also include *warn* and *error* output).

A log line typically looks like this::

  I (32244049) ovms-server-v2: One or more peers have connected
  │  │         │               └─ Log message
  │  │         └─ Component name
  │  └─ Timestamp (milliseconds since boot)
  └─ Log level (I=INFO)

------------------
Logging to SD CARD
------------------

You can also choose to store logs on SD CARD. This is very useful to capture debugging information for the developers, as the log will show what happened before a crash or misbehaviour.

We recommend creating a directory to store logs, i.e.::

  OVMS# vfs mkdir /sd/logs

To enable logging to a file, issue for example::

  OVMS# log file /sd/logs/20180420.log

The destination file can be changed any time. To disable logging to the file, issue *log file* without a file name or *log close*. You may choose an arbitrary file name, good practice is using some date and/or bug identification tag. Note: logging will append to the file if it already exists. To remove a file, use “vfs rm …”. File logging does not persist over a reboot or crash, you can use a script bound to the “sd.mounted” event to re-enable file logging automatically or configure automatic logging.

You can use the webserver to view and download the files. The webserver default configuration enables directory listings and access to files located under the document root directory, which is “/sd” by default. Any path not bound to an internal webserver function is served from the document root. So you can get an inventory of your log files now at the URL::

  http://192.168.4.1/logs/

…and access your log files from there or directly by their respective URLs.

---------------------
Logging Configuration
---------------------

Use the web UI or config command to configure your log levels and file setup to be applied automatically on boot::

  OVMS# config list log
  log (readable writeable)
    file.enable: yes
    file.maxsize: 1024
    file.path: /sd/logs/log
    level: info
    level.simcom: info
    level.v-twizy: verbose
    level.webserver: debug

.. image:: logging.png

The “log” command can be used for temporary changes, if you change the configuration, it will be applied as a whole, replacing your temporary setup.

If a maximum file size >0 is configured, the file will be closed and archived when the size is reached. The archive name consists of the log file name with added suffix of the timestamp, i.e. “/sd/logs/log.20180421-140356”. Using a logs directory will keep all your archived logs accessible at one place.

Take care not to remove an SD card while logging to it is active (or any running file access). The log file should still be consistent, as it is synchronized after every write, but the SD file system currently cannot cope with SD removal with open files. You will need to reboot the module. To avoid this, always use the “Close” button or the “log close” command before removing the SD card.

You don’t need to re-enable logging to an SD path after insertion, the module will watch for the mount event and automatically start logging to it.
