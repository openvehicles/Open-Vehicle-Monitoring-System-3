=========================
Virtual File System (VFS)
=========================

OVMS includes a Virtual File System (VFS) used to unify all storage in the system. The primary configuration and scripting storage is mounted as ‘/store’, and the SD card as ‘/sd’. A ‘vfs’ set of commands is provided for basic manipulation of these stores::

  OVMS# vfs ?
  append               VFS Append a line to a file
  cat                  VFS Display a file
  cp                   VFS Copy a file
  edit                 VFS Edit a file
  ls                   VFS Directory Listing
  mkdir                VFS Create a directory
  mv                   VFS Rename a file
  rm                   VFS Delete a file
  rmdir                VFS Delete a directory
  stat                 VFS Status of a file
  tail                 VFS Output tail of a file

Please take care. This is a very small microcontroller based system with limited storage. The /store area should only be used for storage of configurations and small scripts. The /sd SD CARD area is more flexibly and can be used for storing of configuration logs, firmware images, etc.
