===========
Boot Status
===========

OVMS maintains a record of the reason for each boot, in RAM that survives a reboot. It can show you how long the module has been running for, and the reason for the last reboot::

  OVMS# boot status
  Last boot was 2244 second(s) ago
    This is reset #9 since last power cycle
    Detected boot reason: SoftReset
    Crash counters: 0 total, 0 early
    CPU#0 boot reason was 12
    CPU#1 boot reason was 12

If an unexpected (not ‘module reset’) reboot occurs within the first 10 seconds of startup (usually during the boot-time auto-loading of modules, scripts, etc), the crash counters are incremented. If those crash counters reach 5 (without a clean reset in between), then the auto-loading of modules is disabled for the 6th boot.

In case of a crash, the output will also contain additional debug information, i.e.::

  Last crash: abort() was called on core 1
    Backtrace:
    0x40092ccc 0x40092ec7 0x400dbf1b 0x40176ca9 0x40176c6d 0x400eebd9 0x4013aed9
    0x4013b538 0x40139c15 0x40139df9 0x4013a701 0x4013a731

If the module can access the V2 server after the crash reboot, it will also store this information along with the crash counters in the server table “\*-OVM-DebugCrash”, which will be kept on the server for 30 days.

Please include this info when sending a bug report, along with the output of “ota status” and – if available – any log files capturing the crash event (see Logging to SD CARD). If you can repeat the crash, please try to capture a log at “log level verbose”.
