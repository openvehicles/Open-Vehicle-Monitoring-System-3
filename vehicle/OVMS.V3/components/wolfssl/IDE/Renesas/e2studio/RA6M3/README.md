wolfSSL for Renesas RA Evaluation Kit (EK-RA6M3G)
=================================================

## Description

This directory contains e2studio projects targeted at the Renesas RA 32-bit MCUs.\
The example projects include a wolfSSL TLS client and server.\
They also include benchmark and cryptography tests for the wolfCrypt library.

The wolfssl project contains both the wolfSSL and wolfCrypt libraries.\
It is built as a `Renesas RA C Library Project` and contains the Renesas RA\
configuration.

The other projects (benchmark, client, server and test) are built as a\
`Renesas RA C Project Using RA Library`, where the RA library is the wolfssl project.\
The wolfssl Project Summary is listed below and is relevant for every project.

### Project Summary
|Item|Name/Version|
|:--|:--|
|Board|EK-RA6M3G|
|Device|R7FA6M3AH3CFC|
|Toolchain|GCC ARM Embedded|
|FSP Version|0.8.0|

#### Selected software components

|Components|Version|
|:--|:--|
|Board Support Package Common Files|v0.8.0`|
|Arm CMSIS Version 5 - Core (M)|v5.5.1|
|Amazon FreeRTOS|v0.8.0|
|RA6M3G-EK Board Support Files|v0.8.0|
|Board support package for R7FA6M3AH3CFC|v0.8.0|
|Board support package for RA6M3|v0.8.0|
|Board support package for RA6M3|v0.8.0|
|Amazon FreeRTOS - Memory Management - Heap 4|v0.8.0|
|r_ether to FreeRTOS+TCP Wrapper|v0.8.0|
|Ethernet|v0.8.0|
|Ethernet PHY|v0.8.0|
|FreeRTOS+TCP|v0.8.0|
|Amazon FreeRTOS - Buffer Allocation 2|v0.8.0|


## Setup Steps

The project directories are missing files necessary to build the project.\
These files can be generated when creating a new Renesas RA Project.\
The following steps explain how to generate the missing files and where to place them.

1.) Create a 'dummy' Renesas RA C Library Project.

+ Click File->New->`RA C/C++ Project`
+ Click `Renesas RA C Library Project`. Click Next
+ Enter `dummy_library` as the project name. Click Next.
+ Under `Board: Custom User Board`, select `EK-RA6M3G`.
+ Under `RTOS: No RTOS`, select `Amazon FreeRTOS`.
+ Click Next. Select `Amazon FreeRTOS - Minimal - Static Allocation`
+ Click Finish.

2.) Create a 'dummy' Renesas RA C Project Using RA Library.

+ Click File->New->`RA C/C++ Project`
+ Click `Renesas RA C Project Using RA Library`. Click Next
+ Enter `dummy_app` as the project name. Click Next.
+ Under `RA library project`, select `dummy_library`.
+ Click Finish.

3.) Import all the wolfSSL Projects into e2studio workspace.

+ Click File->`Open Projects from File System`
+ Click `Directory...` to the right of Import source
+ Select the RA6M3G folder location that contains the projects\
   example path: wolfssl/IDE/Renesas/e2studio/RA6M3
+ Deselect the Non-Eclipse project, RA6M3G, by clicking the checkbox\
   Only the folders with 'Eclipse project' under 'Import as' need to be selected.
+ Click Finish.

4.) Copy files from `dummy_library` into `wolfSSL_RA6M3G`

+ Expand the dummy_library and wolfSSL_RA6M3G projects\
  (Click the drop-down arrow to the left of the project name.)
+ Select and Copy the following folders/files inside dummy_library\

  `ra/`\
  `ra_gen/`\
  `ra_cfg/`\
  `script/`\
  `R7FA6M3AH3CFC.pincfg`\
  `RA6M3G-EK.pingcfg`

+ Paste the copied folders/files into wolfSSL_RA6M3G
+ The `dummy_library` project can now be deleted.
+ Generate Project Content.
  + Click `Open RA Configuration` in the top bar (Grey Settings Cog)
  + Click `Generate Project Content` at top right (Green Icon)
+ Build wolfSSL_RA6M3G.

5.) Copy files from `dummy_app` into `./IDE/Renesas/e2studio/RA6M3/common/ra6m3g/`\
    **NOTE:** This may need to be done outside of the e2studio environment (e.g. File Explorer).

+ Select and Copy the followng folder inside dummy_app\
  
  `src/`\
  `script/`

+ Paste the copied folders into `./IDE/Renesas/e2studio/RA6M3/common/ra6m3g/`\
  `(The test, benchmark, client and server projects link to this folder.)`
+ The `dummy_app` project can now be deleted.

6.) Setup Network Environment

        The client and server projects have defines inside their wolfssl_thread_entry.h.
        These defines (ucIPAddress ... ucDNSServerAddress) may need to be changed
        based on your internal network environment.  The g_ether0_mac_address is the default
        mac address found inside the RA configuration inside the wolfssl project.
        The client wolfssl_thread_entry.h has defines (SERVER_IP and DEFAULT_PORT) that
        will need to be changed based on the server you're trying to connect to over
        the ethernet connection.

## Build and Run

### Build Each Project
Right-Click each Project and select Build.

### Run wolfCrypt Test and Benchmark

1.) Right-Click the Project name.\
2.) Select `Debug As` -> `Renesas GDB Hardware Debugging`\
3.) Select J-Link ARM. Click OK.\
4.) Select R7Fa6M3AH. Click OK.

### Run the wolfSSL TLS Server Example.

1.) Right-Click the Project name.\
2.) Select `Debug As` -> `Renesas GDB Hardware Debugging`\
3.) Select J-Link ARM. Click OK.\
4.) Select R7Fa6M3AH. Click OK.\
5.) Run the following wolfSSL example client command inside the base of the wolfssl directory.

```
./examples/client/client -v 4 -h "ucIPAddress" -p 11111 -A ./certs/1024/ca-cert.pem
```

**NOTE:** "ucIPAddress" is "192.168.1.241" by default. (See wolfssl_thread_entry.h)

### Run the wolfSSL TLS Client Example.

 1.) Run the following wolfSSL example server command inside the base of the wolfssl directory.

```
./examples/server/server -v 4 -b -d -p 11111 -c ./certs/1024/server-cert.pem -k ./certs/1024/server-key.pem
```

  **NOTE:** The port 11111 is the DEFAULT_PORT inside wolfssl_thread_entry.h.\

 2.) Right-Click the Project name.\
 3.) Select `Debug As` -> `Renesas GDB Hardware Debugging`\
 4.) Select J-Link ARM. Click OK.\
 5.) Select R7Fa6M3AH. Click OK.

## Troubleshooting

+ The commands for the example client/server assumes it is being run from the
  base directory of wolfssl.

+ Enter "#define DEBUG_WOLFSSL" inside user_settings.h or wolfssl_thread_entry.c\
   to enable wolfssl debug messages to the Renesas Virtual Debug Console.

+ Some linking errors can be caused by the e2studio project files needing to be rebuilt and freshened.
Right-Click a project, select Index, click Rebuild and then click Freshen Files. Repeat for each project.

[Support Forum](https://www.wolfssl.com/forums/)

Support Email: support@wolfssl.com


## Resources

[wolfSSL Website](https://www.wolfssl.com/)

[wolfSSL Wiki](https://github.com/wolfSSL/wolfssl/wiki)

[wolfSSL Manual](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-toc.html)

[wolfSSL API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-17-wolfssl-api-reference.html)

[wolfCrypt API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-18-wolfcrypt-api-reference.html)

[TLS 1.3](https://www.wolfssl.com/docs/tls13/)
