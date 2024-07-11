=========================
Virtual File System (VFS)
=========================

OVMS includes a Virtual File System (VFS) used to unify all storage in the system. The primary configuration and scripting storage is mounted as ‘/store’, and the SD card as ‘/sd’. A ‘vfs’ set of commands is provided for basic manipulation of these stores::

  OVMS# vfs ?
  append               VFS Append a line to a file
  cat                  VFS Display a file
  cp                   VFS Copy a file
  df                   VFS show disk usage
  edit                 VFS edit a file
  head                 VFS Display first 20 lines of a file
  ls                   VFS Directory Listing
  mkdir                VFS Create a directory
  mv                   VFS Rename a file
  rls                  VFS Recursive Directory Listing
  rm                   VFS Delete a file
  rmdir                VFS Delete a directory
  stat                 VFS Status of a file
  tail                 VFS output tail of a file

Please take care. This is a very small microcontroller based system with limited storage. The /store area should only be used for storage of configurations and small scripts. The /sd SD CARD area is more flexible and can be used for storing of configuration logs, firmware images, etc.

The ls and rls commands take simple (``*`` and ``?``) wildcards. So ``vfs rls /sd/*.js`` wold list all files with ``.js`` extension on the SD CARD.

--------------
Network Access
--------------
Network access to the VFS is available via SCP or through the web server.

SCP can access the whole file system and is read/write::

 #copy a new firmware to the sd card on an OVMS named "leaf" in my ssh config
 scp build/ovms3.bin leaf:/sd/dev_ovms3.bin

 #copy a config backup from OVMS named "leaf" to my local directory
 scp leaf:/sd/backup/cfg-2019-12-05.zip .

In the upload example the firmware can then be loaded with the :code:`OTA flash vfs` command

.. note::
  With OpenSSH version 9.0 (or later), the ``scp`` **protocol** has been disabled by default and
  replaced by the ``sftp`` **protocol**. To be able to use the ``scp`` **command** with OVMS, you need
  to re-enable the ``scp`` **protocol** with option ``-O`` on the command line::

    scp -O ....

The web server offers read access rooted at the sd card.
An example retrieving that same config file from the SCP example would look like::

 http://leaf-abr.local/backup/cfg-2019-12-05.zip
