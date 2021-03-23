#How to build with MQX
## Overview
This Makefile is for building wolfSSH library and echoserver sample program running with MQX.
It has following tartes.
 - wolfsshlib: wolfSSH static library
 - echoserver: Simple echo-server example

## Prerequisites
- Installed MQX
- wolfSSH enabled wolfSSL static library

## Setup
- wolfSSH configuration parameters
  You can add or remove configuration options in <wolfSSH-root>/ide/MQX/user_settings.h.

- Setup Makefile
  WOLFSSL_ROOT: wolfSSL install path
  WOLFSSH_ROOT: change this if you move this Makefile location
  MQX_ROOT: MQX source code installed path
  MQXLIB:   MQX library path to like with
  CC:       compiler
  AR:       archiver

